# Node Graph — HW Register Correctness & Section-Aware Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the node graph model output registers as both writable and readable (and reclassify `N` as an output), and replace the data-flow auto-layout with section-aware grid packing so section frames never overlap.

**Architecture:** All changes are in the read-only graph model (`graphmodel.cpp`) and the auto-layout (`graphlayout.cpp`). Register source/output classification moves to the authoritative `Patch::registerIsOutputOnly()`. Output hardware nodes gain a right-side read pin per register. Layout pins source hardware to a far-left column, output hardware to a far-right column, and packs each section's circuits into a compact grid, with section bands stacked vertically. No rendering (`nodeitem.cpp`) or view (`graphview.cpp`) changes are needed — read pins are ordinary `Out` pins with empty labels, and frames/positions flow through existing code.

**Tech Stack:** C++17, Qt 6 Widgets, QGraphicsScene. Tests run in the gitignored throwaway harness (`droidforge/tests/`) via Qt Test.

**Spec:** `docs/superpowers/specs/2026-06-09-node-graph-hw-registers-and-section-layout-design.md`

---

## COMMAND HYGIENE — every executor/subagent MUST obey

These rules are mandatory for every Bash call in this plan:

- **One command per Bash call.** No compound commands: no `cd foo && ...`, no `;`-chains, no `{ ...; ... }` blocks.
- **No inline env-var prefixes** (e.g. `QT_QPA_PLATFORM=offscreen cmd`). Pass `-platform offscreen` as a CLI argument instead.
- **No `find ... -exec ...`.**
- **No inline Python** (`python3 -c "..."`). To inspect a data/JSON file, use the **Read** tool; to search, use `rg`. Never script ad-hoc.
- Use **absolute paths** so no `cd` is needed (`git -C <abs>`, `cmake --build <abs>`, `rg <pat> <abs>`).
- The test harness (`droidforge/tests/`) is **gitignored** — never `git add` anything under it. Commits add only production files under `droidforge/graphview/`.
- End every commit message with the trailer: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

## File Structure

- **Modify** `droidforge/graphview/graphmodel.cpp` — register classification, output read pins, read-back wiring (Tasks 1–2).
- **Modify** `droidforge/graphview/graphlayout.cpp` — section-aware grid layout (Task 3).
- **Modify** `droidforge/tests/test_graphmodel.cpp` — new tests, retire one obsolete test (Tasks 1–3, gitignored).

Reference facts (already verified against the codebase):
- `Patch::registerIsOutputOnly(AtomRegister) const` (`patch.cpp:688`) returns `false` for read-only inputs (`I,P,B,E,S`) and `true` for outputs (`N,O,G,L,R,X`, with gate-number/master nuances for `G`). It is a `const` method.
- Current pin-id scheme: hardware write/read pin base is `hw.<reg>` (e.g. `hw.O1`). The new output **read** pin id is `hw.<reg>.read`.
- `NodeItem::heightFor(const GraphNode&)` and `NodeItem::NODE_WIDTH` are public and already linked into the test harness.
- `GraphModel::describe()` iterates `patch->numSections()` (always ≥ 1; the parser auto-creates one "Untitled section" for sectionless patches), so sectionless patches need no special handling.

---

## Task 1: Reclassify registers via `registerIsOutputOnly`; output nodes get read pins

**Files:**
- Modify: `droidforge/graphview/graphmodel.cpp` (replace `isSourceRegisterType` + `addHwPin` + the node-building loops in `addHardwareNodes`)
- Test: `droidforge/tests/test_graphmodel.cpp`

- [ ] **Step 1: Write the failing tests**

Add these two slots immediately before `void cleanupTestCase()` in `test_graphmodel.cpp`:

```cpp
    void normalizeRegisterIsOutput() {
        // N is an output register (writable), not a source. Writing N1 must put
        // its write pin on the Master OUT (sink) node, never on Master in.
        Patch *p = new Patch(); PatchParser parser;
        parser.parseString("[lfo]\n  output = N1\n", p);
        GraphDescription g = GraphModel::describe(p);
        const GraphPin *writePin = nullptr;
        bool onMasterOut = false, onMasterIn = false;
        for (const auto &n : g.nodes) {
            for (const auto &pin : n.pins) {
                if (pin.id == "hw.N1") {
                    writePin = &pin;
                    if (n.id == "hw.master.out") onMasterOut = true;
                    if (n.id == "hw.master.in")  onMasterIn  = true;
                }
            }
        }
        QVERIFY(writePin);
        QCOMPARE(writePin->direction, GraphPinDirection::In); // write pin
        QVERIFY(onMasterOut);
        QVERIFY(!onMasterIn);
        delete p;
    }

    void outputNodeHasReadPin() {
        // Every output register also exposes a right-side read pin (Out) so it
        // can be read back.
        Patch *p = new Patch(); PatchParser parser;
        parser.parseString("[lfo]\n  output = O1\n", p);
        GraphDescription g = GraphModel::describe(p);
        const GraphPin *readPin = nullptr;
        for (const auto &n : g.nodes)
            for (const auto &pin : n.pins)
                if (pin.id == "hw.O1.read") readPin = &pin;
        QVERIFY(readPin);
        QCOMPARE(readPin->direction, GraphPinDirection::Out);
        delete p;
    }
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen normalizeRegisterIsOutput outputNodeHasReadPin`

(If the binary is stale, first run `cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests"` as a separate Bash call.)

Expected: `normalizeRegisterIsOutput` FAILS (`onMasterIn` is true / `onMasterOut` false — N is currently a source) and `outputNodeHasReadPin` FAILS (`hw.O1.read` does not exist yet).

- [ ] **Step 3: Replace the classification + pin-building code**

In `graphmodel.cpp`, **delete** the `isSourceRegisterType` function (currently lines ~85–90) and the `addHwPin` function (currently lines ~92–102). Replace both with the two helpers below (keep them in the same anonymous namespace, after `portKindOf`/`addOutputPin`):

```cpp
// Read-pin id for a register. Output registers expose a distinct read pin
// (hw.<reg>.read) on their output node; read-only inputs are read straight
// from their single source pin (hw.<reg>).
QString hwReadPinId(const AtomRegister &reg, const Patch *patch)
{
    const QString base = QString(kHwPrefix) + reg.toString();
    return patch->registerIsOutputOnly(reg) ? base + ".read" : base;
}

// Append the pin(s) for one hardware register to a node:
//  - read-only input  -> one read pin (Out)          id: hw.<reg>
//  - output register  -> write pin (In)  + read pin (Out)
//                        ids: hw.<reg>  and  hw.<reg>.read
void appendRegisterPins(GraphNode &node, const AtomRegister &reg, Patch *patch)
{
    const QString base = QString(kHwPrefix) + reg.toString();
    const bool used = patch->registerUsed(reg);

    if (!patch->registerIsOutputOnly(reg)) {
        GraphPin p;
        p.id        = base;
        p.label     = reg.toString();
        p.direction = GraphPinDirection::Out;
        p.portKind  = GraphPortKind::Signal;
        p.role      = GraphPinRole::Simple;
        p.used      = used;
        node.pins.append(p);
        return;
    }

    GraphPin w;
    w.id        = base;
    w.label     = reg.toString();
    w.direction = GraphPinDirection::In;
    w.portKind  = GraphPortKind::Signal;
    w.role      = GraphPinRole::Simple;
    w.used      = used;
    node.pins.append(w);

    GraphPin r;
    r.id        = hwReadPinId(reg, patch);   // base + ".read"
    r.label     = QString();                 // labelled by the write pin's row
    r.direction = GraphPinDirection::Out;
    r.portKind  = GraphPortKind::Signal;
    r.role      = GraphPinRole::Simple;
    r.used      = used;
    node.pins.append(r);
}
```

- [ ] **Step 4: Route registers to nodes by `registerIsOutputOnly`**

In `addHardwareNodes`, replace the **master** register loop. Find this block:

```cpp
    for (register_type_t t : globalTypes) {
        unsigned count = the_firmware->numGlobalRegisters(t);
        for (unsigned n = 1; n <= count; n++) {
            AtomRegister reg(t, 0, 0, n);
            bool source = isSourceRegisterType(t);
            addHwPin(source ? masterIn : masterOut, reg, patch, source);
        }
    }
```

Replace it with:

```cpp
    for (register_type_t t : globalTypes) {
        unsigned count = the_firmware->numGlobalRegisters(t);
        for (unsigned n = 1; n <= count; n++) {
            AtomRegister reg(t, 0, 0, n);
            appendRegisterPins(patch->registerIsOutputOnly(reg) ? masterOut : masterIn,
                               reg, patch);
        }
    }
```

Then replace the **controller** register loops. Find:

```cpp
        for (register_type_t t : ctrlSource) {
            unsigned count = the_firmware->numControllerRegisters(ctrlName, t);
            for (unsigned n = 1; n <= count; n++)
                addHwPin(controls, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch, true);
        }
        for (register_type_t t : ctrlSink) {
            unsigned count = the_firmware->numControllerRegisters(ctrlName, t);
            for (unsigned n = 1; n <= count; n++)
                addHwPin(leds, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch, false);
        }
```

Replace with:

```cpp
        for (register_type_t t : ctrlSource) {
            unsigned count = the_firmware->numControllerRegisters(ctrlName, t);
            for (unsigned n = 1; n <= count; n++)
                appendRegisterPins(controls, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch);
        }
        for (register_type_t t : ctrlSink) {
            unsigned count = the_firmware->numControllerRegisters(ctrlName, t);
            for (unsigned n = 1; n <= count; n++)
                appendRegisterPins(leds, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch);
        }
```

(The `ctrlSource`/`ctrlSink` arrays already match `registerIsOutputOnly`, so `appendRegisterPins` produces a read-only pin for `controls` registers and write+read pins for `leds` registers automatically.)

- [ ] **Step 5: Build and run the two tests**

Run (build first, as its own call): `cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests"`
Then: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen normalizeRegisterIsOutput outputNodeHasReadPin`
Expected: both PASS.

- [ ] **Step 6: Run the full suite to check for regressions**

Run: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen`
Expected: all PASS. (`hardwareNodesSplitByDirection` still passes: `hw.O1` is still an `In` write pin and still `used`. `registerWireToHardwareSink`/`registerWireFromHardwareSource` unaffected.)

- [ ] **Step 7: Commit (production file only)**

Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Classify graph hw registers via registerIsOutputOnly\n\nN becomes an output register; output nodes gain a right-side read\npin (hw.<reg>.read) so outputs can be read back.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')" -- droidforge/graphview/graphmodel.cpp`

---

## Task 2: Wire output read-backs from the read pin

**Files:**
- Modify: `droidforge/graphview/graphmodel.cpp` (`addWires`, the input-register branch)
- Test: `droidforge/tests/test_graphmodel.cpp`

- [ ] **Step 1: Write the failing test**

Add this slot before `void cleanupTestCase()`:

```cpp
    void readBackOutputConnectsFromReadPin() {
        // Reading an output register back (fold.input = O1) must source the wire
        // from the output node's read pin (hw.O1.read), not from hw.O1.
        Patch *p = new Patch(); PatchParser parser;
        parser.parseString("[lfo]\n  output = O1\n[lfo]\n  hz = O1\n", p);
        GraphDescription g = GraphModel::describe(p);
        bool found = false;
        for (const auto &w : g.wires)
            if (!w.isCable && w.fromPinId == "hw.O1.read" && w.toPinId == "c0.1.hz.p")
                found = true;
        QVERIFY(found);
        delete p;
    }
```

- [ ] **Step 2: Run it to verify it fails**

Run (build first if needed): `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen readBackOutputConnectsFromReadPin`
Expected: FAIL — the wire currently sources from `hw.O1` (the write pin), so no wire matches `hw.O1.read`.

- [ ] **Step 3: Source read-backs from the read pin**

In `graphmodel.cpp`'s `addWires`, the input-register branch currently reads:

```cpp
                        } else if (a->isRegister()) {
                            GraphWire w;
                            w.fromPinId = QString(kHwPrefix) + a->toString();
                            w.toPinId   = inPin;
                            g.wires.append(w);
                        }
```

Replace it with:

```cpp
                        } else if (a->isRegister()) {
                            const AtomRegister &areg =
                                *static_cast<const AtomRegister *>(a);
                            GraphWire w;
                            w.fromPinId = hwReadPinId(areg, patch); // hw.<reg>.read for outputs
                            w.toPinId   = inPin;
                            g.wires.append(w);
                        }
```

(`hwReadPinId` returns `hw.<reg>` for read-only inputs, so reading `I1`/`P1.1` is unchanged; for output registers it returns `hw.<reg>.read`.)

- [ ] **Step 4: Build and run the test**

Run (build as its own call): `cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests"`
Then: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen readBackOutputConnectsFromReadPin`
Expected: PASS.

- [ ] **Step 5: Full suite**

Run: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen`
Expected: all PASS. (`registerWireFromHardwareSource` still passes: reading `P1.1` still sources from `hw.P1.1`.)

- [ ] **Step 6: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Source output register read-backs from the read pin\n\nReading an output register (e.g. fold.input = O1) now wires from\nhw.<reg>.read instead of the write pin, so flow resolves to the\noutput node read connector.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')" -- droidforge/graphview/graphmodel.cpp`

---

## Task 3: Section-aware grid auto-layout with hardware on the edges

**Files:**
- Modify: `droidforge/graphview/graphlayout.cpp` (rewrite `layout()`, adjust `columnOf` seeding)
- Test: `droidforge/tests/test_graphmodel.cpp` (add three tests, retire one)

- [ ] **Step 1: Write the failing tests; retire the obsolete one**

In `test_graphmodel.cpp`, **delete** the entire `downstreamCircuitIsRightOfUpstream()` slot (grid layout no longer orders circuits by data flow).

Add these three slots before `void cleanupTestCase()`:

```cpp
    void sourceHardwareLeftmostSinkRightmost() {
        Patch *p = new Patch(); PatchParser parser;
        parser.parseString("[lfo]\n  hz = I1\n  output = O1\n", p);
        GraphDescription g = GraphModel::describe(p);
        GraphLayout::layout(g);
        const GraphNode *in = g.findNode("hw.master.in");
        const GraphNode *out = g.findNode("hw.master.out");
        const GraphNode *c = g.findNode("c0.0");
        QVERIFY(in && out && c);
        QVERIFY(in->pos.x()  < c->pos.x());   // source hardware on the left
        QVERIFY(out->pos.x() > c->pos.x());   // output hardware on the right
        delete p;
    }

    void sectionFramesAreDisjoint() {
        Patch *p = new Patch(); PatchParser parser;
        parser.parseString(
            "# -------------------------------------------------\n"
            "# Section A\n"
            "# -------------------------------------------------\n"
            "\n[lfo]\n  output = _A\n\n"
            "# -------------------------------------------------\n"
            "# Section B\n"
            "# -------------------------------------------------\n"
            "\n[lfo]\n  hz = _A\n",
            p);
        GraphDescription g = GraphModel::describe(p);
        GraphLayout::layout(g);
        QCOMPARE(g.frames.size(), 2);
        QVERIFY(!g.frames[0].rect.intersects(g.frames[1].rect));
        delete p;
    }

    void sectionlessPatchHasSingleUntitledFrame() {
        // No section headers: the parser normalizes to one default section.
        Patch *p = new Patch(); PatchParser parser;
        parser.parseString("[lfo]\n[lfo]\n", p);
        GraphDescription g = GraphModel::describe(p);
        GraphLayout::layout(g);
        QCOMPARE(g.frames.size(), 1);
        QVERIFY(!g.frames[0].title.isEmpty());   // shows "Untitled section"
        delete p;
    }
```

- [ ] **Step 2: Run to verify failure / pre-existing state**

Run (build first if needed): `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen sectionFramesAreDisjoint sourceHardwareLeftmostSinkRightmost sectionlessPatchHasSingleUntitledFrame`
Expected: `sectionFramesAreDisjoint` FAILS (current data-flow layout overlaps the two section frames). The other two may already pass under the old layout — that is fine; they lock in behavior the new layout must preserve.

- [ ] **Step 3: Rewrite the layout**

Replace the **entire contents** of `droidforge/graphview/graphlayout.cpp` with:

```cpp
#include "graphlayout.h"
#include "nodeitem.h"
#include <QHash>
#include <cmath>

namespace {
const double COL_GAP     = 80.0;  // between a hardware column and the section band
const double H_GAP       = 40.0;  // between grid columns within a section
const double V_GAP       = 30.0;  // between stacked nodes / grid rows
const double SECTION_GAP = 60.0;  // between section bands (must exceed 2*FRAME_PAD)
const double FRAME_PAD   = 24.0;
}

namespace GraphLayout {

static QHash<QString,int> g_columns;

int columnOf(const GraphDescription &, const QString &nodeId)
{
    return g_columns.value(nodeId, 0);
}

void layout(GraphDescription &g)
{
    g_columns.clear();

    // Coarse role columns, only for columnOf()/tests: source=0, circuit=1, sink=2.
    for (const auto &n : g.nodes) {
        int col = 1;
        if (n.kind == GraphNodeKind::HardwareSource)    col = 0;
        else if (n.kind == GraphNodeKind::HardwareSink) col = 2;
        g_columns[n.id] = col;
    }

    // 1. Source hardware -> far-left column, stacked by actual height.
    double y = 0.0;
    for (auto &n : g.nodes) {
        if (n.kind != GraphNodeKind::HardwareSource) continue;
        n.pos = QPointF(0.0, y);
        y += NodeItem::heightFor(n) + V_GAP;
    }

    const double sectionsX = NodeItem::NODE_WIDTH + COL_GAP;

    // 2. Each section -> a compact grid; section bands stacked vertically.
    double sectionY = 0.0;
    double maxSectionRight = sectionsX;
    for (int s = 0; s < g.frames.size(); s++) {
        QList<GraphNode *> circuits;
        for (auto &n : g.nodes)
            if (n.kind == GraphNodeKind::Circuit && n.sectionIndex == s)
                circuits.append(&n);
        if (circuits.isEmpty()) continue;

        const int cnt  = circuits.size();
        const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(cnt))));

        double rowY = sectionY;
        int idx = 0;
        while (idx < cnt) {
            double rowH = 0.0;
            for (int j = 0; j < cols && idx + j < cnt; j++)
                rowH = qMax(rowH, NodeItem::heightFor(*circuits[idx + j]));
            for (int j = 0; j < cols && idx + j < cnt; j++) {
                const double x = sectionsX + j * (NodeItem::NODE_WIDTH + H_GAP);
                circuits[idx + j]->pos = QPointF(x, rowY);
                maxSectionRight = qMax(maxSectionRight, x + NodeItem::NODE_WIDTH);
            }
            rowY += rowH + V_GAP;
            idx  += cols;
        }
        sectionY = rowY + SECTION_GAP;
    }

    // 3. Output hardware -> far-right column, stacked.
    const double sinkX = maxSectionRight + COL_GAP;
    y = 0.0;
    for (auto &n : g.nodes) {
        if (n.kind != GraphNodeKind::HardwareSink) continue;
        n.pos = QPointF(sinkX, y);
        y += NodeItem::heightFor(n) + V_GAP;
    }

    // 4. Frame rect = padded bounding box of each section's circuits. Because
    // section bands are stacked with SECTION_GAP > 2*FRAME_PAD, frames are
    // pairwise disjoint.
    for (auto &f : g.frames) {
        if (f.nodeIds.isEmpty()) continue;
        double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
        for (const auto &id : f.nodeIds) {
            const GraphNode *n = g.findNode(id);
            if (!n) continue;
            x0 = qMin(x0, n->pos.x());
            y0 = qMin(y0, n->pos.y());
            x1 = qMax(x1, n->pos.x() + NodeItem::NODE_WIDTH);
            y1 = qMax(y1, n->pos.y() + NodeItem::heightFor(*n));
        }
        f.rect = QRectF(x0 - FRAME_PAD, y0 - FRAME_PAD,
                        (x1 - x0) + 2 * FRAME_PAD,
                        (y1 - y0) + 2 * FRAME_PAD);
    }
}

} // namespace GraphLayout
```

- [ ] **Step 4: Build and run the new tests**

Run (build as its own call): `cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests"`
Then: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen sectionFramesAreDisjoint sourceHardwareLeftmostSinkRightmost sectionlessPatchHasSingleUntitledFrame`
Expected: all PASS.

- [ ] **Step 5: Full suite**

Run: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen`
Expected: all PASS. (`nodesGetPositionsAndSourcesLeftmost` still passes via the coarse role columns; `framesGroupCircuitsBySection` still passes.)

- [ ] **Step 6: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Section-aware grid auto-layout with hardware on the edges\n\nReplace global data-flow layering with per-section grid packing,\nsource hardware pinned left and output hardware pinned right.\nSection bands stack vertically so frames no longer overlap.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')" -- droidforge/graphview/graphlayout.cpp`

---

## Task 4: Visual + app verification

**Files:** none (verification only).

- [ ] **Step 1: Build the app target**

Run: `cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build"`
Expected: links `DROID Forge.app` with no errors.

- [ ] **Step 2: Render the real patch offscreen**

Run (build the helper as its own call): `cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests" --target graphshot`
Then: `"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/graphshot" -platform offscreen`
Expected: exit 0, writes `/tmp/graphshot.png`.

- [ ] **Step 3: Inspect the render**

Read `/tmp/graphshot.png` and confirm: source hardware (Master in / Ctrl controls) on the far left, output hardware (Master out / LEDs) on the far right with both left (write) and right (read) connectors, each section drawn as a compact non-overlapping rectangle, and `N1` appearing on the output side. Note any issue for follow-up; do not silently pass over a visibly wrong render.

---

## Self-Review (completed by plan author)

**Spec coverage:**
- §1 register classification → Task 1 (Steps 3–4).
- §2 source read pins / output write+read pins, "always show read connector" → Task 1.
- §3 wiring rules (write to write pin, read-back from read pin, input reads unchanged) → Task 1 (write pin id unchanged) + Task 2 (read-back).
- §4 section-aware layout, hardware on edges, grid packing, vertical bands → Task 3.
- §5 sectionless patches → Task 3 (`sectionlessPatchHasSingleUntitledFrame`).
- Testing list → covered across Tasks 1–3; `downstreamCircuitIsRightOfUpstream` retired in Task 3; `graphshot` re-render in Task 4.

**Placeholder scan:** none — all steps contain concrete code/commands.

**Type consistency:** `hwReadPinId(reg, patch)` and `appendRegisterPins(node, reg, patch)` are defined in Task 1 and reused (Task 2 calls `hwReadPinId`); read pin id is `hw.<reg>.read` consistently in model, wiring, and tests. `NodeItem::heightFor` / `NodeItem::NODE_WIDTH` signatures match the existing header. Layout constants are defined in the rewritten file.
