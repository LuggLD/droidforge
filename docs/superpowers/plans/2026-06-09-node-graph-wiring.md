# Node Graph Editor — Wiring — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the node graph editable by drawing and removing wires with the mouse — drag a pin to a compatible pin to connect; modifiers and right-click to move/copy/re-home/disconnect; click a wire and press Delete to remove it — with every change flowing through the edit engine (undo) and reflected in the list view.

**Architecture:** A pure, GUI-free `GraphEdits` layer turns pin-id pairs + a `Patch*` into atom mutations (mint/reuse cables, set registers, clear endpoints), TDD'd in the gitignored harness exactly like M1's `GraphModel`. A thin `GraphView` gesture layer hit-tests pins, draws a rubber-band, calls `GraphEdits`, then `commit()`s — verified by running the app. `NodeItem` gains pin hit-testing (testable); `WireItem` gains selectability.

**Tech Stack:** C++17, Qt 6.11 (Widgets), CMake + Ninja, Qt Test (throwaway harness only).

**Spec:** `docs/superpowers/specs/2026-06-09-node-graph-wiring-design.md`

**Deliberate refinement of the spec:** the spec says each `GraphEdits` function "ends in `commit()`." For purity and testability, `GraphEdits` performs the model mutation only and returns `bool`; **`GraphView` calls `patch->commit(msg)` once** after a successful edit. One gesture still equals one undo step. This keeps `GraphEdits` dependency-free of `PatchEditEngine`, so its tests mirror the existing `test_graphmodel.cpp` (plain `Patch`).

---

## COMMAND HYGIENE (read before running anything)

These rules are enforced in this repo. Pass them verbatim to any subagent.

- **One command per Bash call.** No `&&`, no `;`-chains, no `{...}` blocks, no `cd foo && ...`.
- **No inline env-var prefixes.** Pass Qt platform as a CLI arg: `forgetests -platform offscreen` (never `QT_QPA_PLATFORM=offscreen forgetests`).
- **No `find -exec`. No inline Python.** Use the Read tool or `rg`.
- **Absolute paths everywhere.**
- The test harness under `droidforge/tests/` is **gitignored — never `git add` it.** Every commit contains production files only (under `droidforge/graphview/` + `droidforge/CMakeLists.txt`).
- **No PR.** Commit to the current branch (`feature/node-graph-editor`) and stop there.
- Commit messages end with: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

**Build / test / run commands (copy exactly):**

```bash
# Configure the throwaway test harness (only needed once, or after CMakeLists changes)
cmake -S /Users/jan.kaluza/Projects/droidforge/droidforge/tests -B /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
# Build the tests
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
# Run the tests (offscreen so no window is needed)
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen
```
```bash
# Build the app (for GUI tasks)
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build
```
```bash
# Run the app
open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"
```

---

## Data model (defined once, used by all tasks)

`GraphEdits` is a namespace in new files `droidforge/graphview/graphedits.{h,cpp}`. Its full public surface (referenced by every task — do not rename):

```cpp
// graphview/graphedits.h
#ifndef GRAPHEDITS_H
#define GRAPHEDITS_H

#include "graphmodeltypes.h"   // GraphPortKind
#include <QString>
#include <QStringList>

class Patch;

namespace GraphEdits {

// Which gesture is being performed; set by GraphView from keyboard modifiers.
enum class DragMode { Connect, Copy, Move, Rehome };

// Parsed + classified pin id. Sources fan out; sinks hold one wire.
struct PinRef {
    enum Kind { CircuitInput, CircuitOutput, HwWrite, HwRead, HwSource, Invalid };
    Kind kind = Invalid;
    int section = -1;     // circuit pins only
    int circuit = -1;     // circuit pins only
    QString jack;         // circuit pins only
    int column = 1;       // circuit inputs: 1=primary/2=scale/3=offset; else 1
    QString reg;          // hardware pins only (register string, e.g. "O1", "P1.1")

    bool isSource() const { return kind == CircuitOutput || kind == HwRead || kind == HwSource; }
    bool isSink()   const { return kind == CircuitInput  || kind == HwWrite; }
    bool valid()    const { return kind != Invalid; }
};

// --- classification / validity (const, pure) ---
PinRef        parsePin(const Patch *patch, const QString &pinId);
GraphPortKind portKindOfPin(const Patch *patch, const PinRef &ref);
bool          isValidDrop(const Patch *patch, const QString &fromPin,
                          const QString &toPin, DragMode mode);

// --- queries (const, pure) ---
QStringList getConnectedSinks(const Patch *patch, const QString &sourcePin);
QString     getConnectedSource(const Patch *patch, const QString &sinkPin);

// --- mutations (modify the patch model; return true on success; DO NOT commit) ---
bool connectPins(Patch *patch, const QString &fromPin, const QString &toPin);
bool copyWire(Patch *patch, const QString &fromSink, const QString &toSink);
bool moveWire(Patch *patch, const QString &fromSink, const QString &toSink);
bool rehomeWires(Patch *patch, const QString &fromSource, const QString &toSource);
bool disconnectWire(Patch *patch, const QString &fromPin, const QString &toPin);
bool disconnectPin(Patch *patch, const QString &pin);

} // namespace GraphEdits

#endif
```

**Pin-id formats (from `graphmodel.cpp`, do not change):** circuit input `c<S>.<C>.<jack>.[p|s|o]`; circuit output `c<S>.<C>.<jack>.out`; hardware write/source `hw.<reg>`; hardware read `hw.<reg>.read`. The register string itself may contain a dot (`P1.1`).

---

## File Structure

**Production (committed):**
- `droidforge/graphview/graphedits.h` / `.cpp` — pure connection logic (new).
- `droidforge/graphview/nodeitem.h` / `.cpp` — add `pinAt()` hit-testing (modify).
- `droidforge/graphview/wireitem.h` / `.cpp` — add selectability + `shape()` + selected paint (modify).
- `droidforge/graphview/graphview.h` / `.cpp` — drag state machine, rubber-band, wire selection, context menus (modify).
- `droidforge/CMakeLists.txt` — add `graphedits.{h,cpp}` (modify).

**Throwaway (gitignored, never committed):**
- `droidforge/tests/test_graphedits.cpp` (new).
- `droidforge/tests/CMakeLists.txt` — add `test_graphedits.cpp` + `graphedits.cpp` to `forgetests` (modify).

---

## Task 1: `GraphEdits` scaffold — pin parsing, port kind, validity

**Files:**
- Create: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphedits.h`
- Create: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphedits.cpp`
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/CMakeLists.txt` (add the two files in the `graphview/` block)
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/CMakeLists.txt`
- Test: `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/test_graphedits.cpp` (new)

- [ ] **Step 1: Create the header**

Write `graphedits.h` exactly as in the "Data model" section above.

- [ ] **Step 2: Wire the new files into both CMake targets**

In `/Users/jan.kaluza/Projects/droidforge/droidforge/CMakeLists.txt`, in the `graphview/` source block (currently ending at `graphview/sectionframeitem.cpp`), add after the `wireitem.cpp` line:

```cmake
    graphview/graphedits.h
    graphview/graphedits.cpp
```

In `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/CMakeLists.txt`, add `test_graphedits.cpp` and `${SRC_ROOT}/graphview/graphedits.cpp` to the `forgetests` target's source list (alongside `test_graphmodel.cpp` and `${SRC_ROOT}/graphview/nodeitem.cpp`):

```cmake
add_executable(forgetests
    test_stubs.cpp
    test_graphmodel.cpp
    test_graphedits.cpp
    ${SRC_ROOT}/graphview/graphmodeltypes.cpp
    ${SRC_ROOT}/graphview/nodeitem.cpp
    ${SRC_ROOT}/graphview/graphedits.cpp
    ${GRAPH_SRC} ${MODEL_SRC} ${PATCHVIEW_SRC} ${SRC_ROOT}/resources.qrc)
```

(Note: `${GRAPH_SRC}` globs `graphmodel*.cpp`/`graphlayout*.cpp`, so `graphedits.cpp` must be listed explicitly as shown.)

- [ ] **Step 3: Write the failing test**

Create `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/test_graphedits.cpp`:

```cpp
// Tests for GraphEdits (wiring connection logic).
// This file is gitignored — never committed.

#include <QtTest>
#include "patch.h"
#include "patchparser.h"
#include "graphedits.h"
#include "droidfirmware.h"
#include "atomcable.h"
#include "atomregister.h"
#include "circuit.h"
#include "patchsection.h"
#include "jackassignment.h"

extern DroidFirmware *the_firmware;

using namespace GraphEdits;

static Patch *parse(const QString &src) {
    Patch *p = new Patch();
    PatchParser parser;
    parser.parseString(src, p);
    return p;
}

// Read the atom at a circuit input column (1=primary/2=scale/3=offset).
static const Atom *inAtom(Patch *p, int s, int c, const QString &jack, int col) {
    JackAssignment *ja = p->section(s)->circuit(c)->findJack(jack);
    return ja ? ja->atomAt(col) : nullptr;
}
// Read the atom at a circuit output.
static const Atom *outAtom(Patch *p, int s, int c, const QString &jack) {
    JackAssignment *ja = p->section(s)->circuit(c)->findJack(jack);
    return ja ? ja->atomAt(1) : nullptr;
}

class TestGraphEdits : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { the_firmware = new DroidFirmware(); }

    void parsesCircuitInputPin() {
        Patch *p = parse("[lfo]\n  hz = 0\n  output = _X\n");
        PinRef r = parsePin(p, "c0.0.hz.p");
        QCOMPARE(r.kind, PinRef::CircuitInput);
        QCOMPARE(r.section, 0);
        QCOMPARE(r.circuit, 0);
        QCOMPARE(r.jack, QString("hz"));
        QCOMPARE(r.column, 1);
        QVERIFY(r.isSink());
        delete p;
    }

    void parsesCircuitOutputPin() {
        Patch *p = parse("[lfo]\n  output = _X\n");
        PinRef r = parsePin(p, "c0.0.output.out");
        QCOMPARE(r.kind, PinRef::CircuitOutput);
        QCOMPARE(r.column, 1);
        QVERIFY(r.isSource());
        delete p;
    }

    void parsesHardwarePins() {
        Patch *p = parse("[lfo]\n  hz = P1.1\n  output = O1\n");
        // Output register O1: bare id is the WRITE pin (sink), .read is the read pin (source).
        QCOMPARE(parsePin(p, "hw.O1").kind, PinRef::HwWrite);
        QVERIFY(parsePin(p, "hw.O1").isSink());
        QCOMPARE(parsePin(p, "hw.O1.read").kind, PinRef::HwRead);
        QVERIFY(parsePin(p, "hw.O1.read").isSource());
        // Read-only input register P1.1: bare id is a source.
        QCOMPARE(parsePin(p, "hw.P1.1").kind, PinRef::HwSource);
        QVERIFY(parsePin(p, "hw.P1.1").isSource());
        QCOMPARE(parsePin(p, "hw.P1.1").reg, QString("P1.1"));
        delete p;
    }

    void rejectsBadDirections() {
        Patch *p = parse("[lfo]\n  hz = 0\n  output = _X\n[lfo]\n  hz = 0\n  output = _Y\n");
        // sink -> sink (two inputs) is invalid for a plain Connect
        QVERIFY(!isValidDrop(p, "c0.0.hz.p", "c0.1.hz.p", DragMode::Connect));
        // source -> source (two outputs) is invalid for a plain Connect
        QVERIFY(!isValidDrop(p, "c0.0.output.out", "c0.1.output.out", DragMode::Connect));
        // source -> sink is valid
        QVERIFY(isValidDrop(p, "c0.0.output.out", "c0.1.hz.p", DragMode::Connect));
        // sink -> source is valid (drag started at the sink)
        QVERIFY(isValidDrop(p, "c0.1.hz.p", "c0.0.output.out", DragMode::Connect));
        delete p;
    }

    void rejectsTypeMismatch() {
        // display has a text input ("text"); lfo.output is a signal. Cross-wiring is invalid.
        Patch *p = parse("[lfo]\n  output = _X\n[display]\n  text = \"hi\"\n");
        // locate the display section/circuit dynamically is unnecessary: it is section 1, circuit 0.
        QVERIFY(!isValidDrop(p, "c0.0.output.out", "c1.0.text.p", DragMode::Connect));
        delete p;
    }
};

QTEST_MAIN(TestGraphEdits)
#include "test_graphedits.moc"
```

- [ ] **Step 4: Run to verify it fails (link error / missing symbols)**

Run:
```bash
cmake -S /Users/jan.kaluza/Projects/droidforge/droidforge/tests -B /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
Expected: FAIL — link/compile errors (`GraphEdits::parsePin` etc. undefined). This confirms the test is wired in.

- [ ] **Step 5: Implement `graphedits.cpp` (parsing, port kind, validity)**

Create `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphedits.cpp`:

```cpp
#include "graphedits.h"
#include "patch.h"
#include "patchsection.h"
#include "circuit.h"
#include "jackassignment.h"
#include "atom.h"
#include "atomcable.h"
#include "atomregister.h"
#include "droidfirmware.h"

extern DroidFirmware *the_firmware;

namespace GraphEdits {

PinRef parsePin(const Patch *patch, const QString &pinId)
{
    PinRef ref;
    if (pinId.startsWith("hw.")) {
        QString rest = pinId.mid(3);
        bool isRead = rest.endsWith(".read");
        if (isRead)
            rest.chop(5); // strip ".read"
        ref.reg = rest;
        ref.column = 1;
        AtomRegister areg(rest);
        if (isRead)
            ref.kind = PinRef::HwRead;
        else if (patch->registerIsOutputOnly(areg))
            ref.kind = PinRef::HwWrite;
        else
            ref.kind = PinRef::HwSource;
        return ref;
    }
    if (pinId.startsWith("c")) {
        // c<S>.<C>.<jack>.<suffix>   (jack may itself contain '.')
        const QStringList parts = pinId.mid(1).split('.');
        if (parts.size() < 3)
            return ref; // Invalid
        bool ok1 = false, ok2 = false;
        const int s = parts.first().toInt(&ok1);
        const int c = parts.at(1).toInt(&ok2);
        if (!ok1 || !ok2)
            return ref;
        const QString suffix = parts.last();
        ref.section = s;
        ref.circuit = c;
        ref.jack = parts.mid(2, parts.size() - 3).join('.');
        if (suffix == "p")        { ref.kind = PinRef::CircuitInput;  ref.column = 1; }
        else if (suffix == "s")   { ref.kind = PinRef::CircuitInput;  ref.column = 2; }
        else if (suffix == "o")   { ref.kind = PinRef::CircuitInput;  ref.column = 3; }
        else if (suffix == "out") { ref.kind = PinRef::CircuitOutput; ref.column = 1; }
        return ref;
    }
    return ref;
}

GraphPortKind portKindOfPin(const Patch *patch, const PinRef &ref)
{
    if (ref.kind == PinRef::CircuitInput || ref.kind == PinRef::CircuitOutput) {
        const Circuit *circ = patch->section(ref.section)->circuit(ref.circuit);
        const QString sym = the_firmware->jackTypeSymbol(
            circ->getName(),
            ref.kind == PinRef::CircuitInput ? "inputs" : "outputs",
            ref.jack);
        return sym == "text" ? GraphPortKind::Text : GraphPortKind::Signal;
    }
    return GraphPortKind::Signal; // all hardware registers are signal
}

// Does a source pin currently hold a net (a producer atom)?
static bool sourceHasNet(const Patch *patch, const PinRef &ref)
{
    if (ref.kind == PinRef::CircuitOutput) {
        const Circuit *circ = patch->section(ref.section)->circuit(ref.circuit);
        const JackAssignment *ja = circ->findJack(ref.jack);
        return ja && ja->atomAt(1) != nullptr;
    }
    // Hardware sources always "exist" but can't be re-homed onto (see isValidDrop).
    return true;
}

bool isValidDrop(const Patch *patch, const QString &fromPin,
                 const QString &toPin, DragMode mode)
{
    if (fromPin == toPin || fromPin.isEmpty() || toPin.isEmpty())
        return false;
    const PinRef a = parsePin(patch, fromPin);
    const PinRef b = parsePin(patch, toPin);
    if (!a.valid() || !b.valid())
        return false;

    switch (mode) {
    case DragMode::Connect:
        if (!((a.isSource() && b.isSink()) || (a.isSink() && b.isSource())))
            return false;
        break;
    case DragMode::Copy:
    case DragMode::Move:
        if (!(a.isSink() && b.isSink()))
            return false;
        break;
    case DragMode::Rehome:
        // Re-home moves a producer onto another *circuit output* that is empty.
        if (!(a.isSource() && b.kind == PinRef::CircuitOutput))
            return false;
        if (sourceHasNet(patch, b))
            return false; // edge #2: never clobber an occupied source
        break;
    }

    if (portKindOfPin(patch, a) != portKindOfPin(patch, b))
        return false; // signal vs text
    return true;
}

} // namespace GraphEdits
```

(The mutation/query functions are added in Tasks 2–4. The file compiles now because the header declares them but the harness test only calls the four implemented above. If the linker complains about the not-yet-defined functions, temporarily comment their declarations is **not** needed — they are only referenced from tests added later.)

- [ ] **Step 6: Run to verify the four tests pass**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen
```
Expected: PASS for `parsesCircuitInputPin`, `parsesCircuitOutputPin`, `parsesHardwarePins`, `rejectsBadDirections`, `rejectsTypeMismatch` (plus the pre-existing `test_graphmodel` cases). If `[display]`/`text` jack handling differs, adjust the type-mismatch test's circuit to any known text jack (`display.text`) — the firmware is the source of truth.

- [ ] **Step 7: Build the app to confirm production still links**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build
```
Expected: links clean (the new `graphedits.cpp` defines only the four functions used so far; the others are declared but unreferenced by production until Task 5+, which is fine for a header declaration).

- [ ] **Step 8: Commit (production only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphedits.h droidforge/graphview/graphedits.cpp droidforge/CMakeLists.txt
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Add GraphEdits scaffold: pin parsing, port kind, drop validity\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Task 2: `connectPins` — the core connect primitive

**Files:**
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphedits.cpp`
- Test: `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/test_graphedits.cpp`

`connectPins` resolves which pin is the source and which is the sink, then stores the connecting atom on the jack that physically holds it:

- **Sink is a circuit input** → set that input's column atom to the source's net: a circuit-output source mints a fresh `AtomCable` (and stores it on the output) if empty, reuses its existing cable, or — if the output already holds a register (it drives a hardware sink) — the input reads that register; a hardware source/read pin yields `AtomRegister(reg)`.
- **Sink is a hardware write pin** (valid only from a circuit-output source) → store `AtomRegister(sink.reg)` on the **circuit output** (replacing whatever it held).

- [ ] **Step 1: Write the failing tests**

Add these methods to `TestGraphEdits` in `test_graphedits.cpp` (before `QTEST_MAIN`):

```cpp
    void connectCircuitOutToInputMintsCable() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  hz = 0\n");
        // clear the parsed values so we start empty
        QVERIFY(connectPins(p, "c0.0.output.out", "c0.1.hz.p"));
        const Atom *out = outAtom(p, 0, 0, "output");
        const Atom *in  = inAtom(p, 0, 1, "hz", 1);
        QVERIFY(out && out->isCable());
        QVERIFY(in && in->isCable());
        QCOMPARE(out->toString(), in->toString()); // same cable name
        delete p;
    }

    void secondReaderReusesCable() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  hz = 0\n[lfo]\n  hz = 0\n");
        QVERIFY(connectPins(p, "c0.0.output.out", "c0.1.hz.p"));
        const QString cable = outAtom(p, 0, 0, "output")->toString();
        QVERIFY(connectPins(p, "c0.0.output.out", "c0.2.hz.p"));
        // output cable unchanged; second input reads the same cable
        QCOMPARE(outAtom(p, 0, 0, "output")->toString(), cable);
        QCOMPARE(inAtom(p, 0, 2, "hz", 1)->toString(), cable);
        delete p;
    }

    void connectHwSourceToInputStoresRegister() {
        Patch *p = parse("[lfo]\n  hz = 0\n");
        QVERIFY(connectPins(p, "hw.P1.1", "c0.0.hz.p"));
        const Atom *in = inAtom(p, 0, 0, "hz", 1);
        QVERIFY(in && in->isRegister());
        QCOMPARE(in->toString(), QString("P1.1"));
        delete p;
    }

    void connectCircuitOutToHwSinkStoresRegisterOnOutput() {
        Patch *p = parse("[lfo]\n  output = _X\n");
        QVERIFY(connectPins(p, "c0.0.output.out", "hw.O1"));
        const Atom *out = outAtom(p, 0, 0, "output");
        QVERIFY(out && out->isRegister());
        QCOMPARE(out->toString(), QString("O1"));
        delete p;
    }

    void connectReadPinToInputStoresOutputRegister() {
        Patch *p = parse("[lfo]\n  output = O1\n[lfo]\n  hz = 0\n");
        // O1 is an output register; reading it back from its read pin
        QVERIFY(connectPins(p, "hw.O1.read", "c0.1.hz.p"));
        const Atom *in = inAtom(p, 0, 1, "hz", 1);
        QVERIFY(in && in->isRegister());
        QCOMPARE(in->toString(), QString("O1"));
        delete p;
    }

    void connectReplacesExistingSinkAtom() {
        Patch *p = parse("[lfo]\n  hz = P1.1\n");
        QVERIFY(connectPins(p, "hw.P1.2", "c0.0.hz.p"));
        QCOMPARE(inAtom(p, 0, 0, "hz", 1)->toString(), QString("P1.2"));
        delete p;
    }

    void outputAlreadyRegisterReadsBackOnDragToInput() {
        // Edge #1: output drives O1; dragging it to a circuit input makes the input read O1.
        Patch *p = parse("[lfo]\n  output = O1\n[lfo]\n  hz = 0\n");
        QVERIFY(connectPins(p, "c0.0.output.out", "c0.1.hz.p"));
        QCOMPARE(inAtom(p, 0, 1, "hz", 1)->toString(), QString("O1"));
        // output still drives O1 (unchanged)
        QCOMPARE(outAtom(p, 0, 0, "output")->toString(), QString("O1"));
        delete p;
    }
```

- [ ] **Step 2: Run to verify failure**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
Expected: FAIL — `connectPins` undefined symbol.

- [ ] **Step 3: Implement `connectPins` (+ private helpers) in `graphedits.cpp`**

Add inside `namespace GraphEdits` (before the closing brace), after `isValidDrop`:

```cpp
// Resolve a parsed pin to its JackAssignment (or nullptr for hardware pins).
static JackAssignment *jackFor(Patch *patch, const PinRef &ref)
{
    if (ref.kind == PinRef::CircuitInput || ref.kind == PinRef::CircuitOutput)
        return patch->section(ref.section)->circuit(ref.circuit)->findJack(ref.jack);
    return nullptr;
}

// The atom a source pin offers to a circuit-input sink (cloned for the caller).
// For a circuit output this may MINT a cable and store it on the output.
static Atom *netAtomForInput(Patch *patch, const PinRef &src)
{
    if (src.kind == PinRef::HwSource || src.kind == PinRef::HwRead)
        return new AtomRegister(src.reg);

    if (src.kind == PinRef::CircuitOutput) {
        JackAssignment *out = jackFor(patch, src);
        if (!out)
            return nullptr;
        const Atom *cur = out->atomAt(1);
        if (cur)
            return cur->clone();          // reuse existing cable, or read-back a register (edge #1)
        const QString name = patch->freshCableName();
        out->replaceAtom(1, new AtomCable(name)); // mint + store on producer
        return new AtomCable(name);
    }
    return nullptr;
}

bool connectPins(Patch *patch, const QString &fromPin, const QString &toPin)
{
    PinRef a = parsePin(patch, fromPin);
    PinRef b = parsePin(patch, toPin);
    if (!a.valid() || !b.valid())
        return false;
    // identify source + sink regardless of drag order
    PinRef src = a.isSource() ? a : b;
    PinRef snk = a.isSink()   ? a : b;
    if (!src.isSource() || !snk.isSink())
        return false;

    if (snk.kind == PinRef::CircuitInput) {
        Atom *atom = netAtomForInput(patch, src);
        if (!atom)
            return false;
        JackAssignment *in = jackFor(patch, snk);
        if (!in) { delete atom; return false; }
        in->replaceAtom(snk.column, atom);
        return true;
    }
    if (snk.kind == PinRef::HwWrite) {
        // out -> hw sink: the atom lives on the producing circuit output.
        if (src.kind != PinRef::CircuitOutput)
            return false;
        JackAssignment *out = jackFor(patch, src);
        if (!out)
            return false;
        out->replaceAtom(1, new AtomRegister(snk.reg));
        return true;
    }
    return false;
}
```

- [ ] **Step 4: Run to verify the tests pass**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen
```
Expected: all Task 2 tests PASS.

- [ ] **Step 5: Commit (production only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphedits.cpp
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Implement GraphEdits::connectPins (cable mint/reuse, register, read-back)\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Task 3: queries + `disconnectWire` + `disconnectPin`

**Files:**
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphedits.cpp`
- Test: `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/test_graphedits.cpp`

Semantics:
- `getConnectedSinks(source)` — circuit output holding cable `C` → every circuit input whose column atom equals `C`; holding register `R` → the hardware write pin `hw.R`; a hardware source/read pin → every circuit input whose atom equals the register string.
- `getConnectedSource(sink)` — circuit input holding cable `C` → the circuit output that holds `C`; holding register `R` → `hw.R.read` if `R` is output-only, else `hw.R`; a hardware write pin → the circuit output whose atom equals `R`.
- `disconnectWire(from,to)` — clear the atom on the endpoint that stores it: the circuit **input** when one endpoint is a circuit input; otherwise the circuit **output** (for output→hw-sink).
- `disconnectPin(pin)` — sink: clear it. Circuit-output source: clear the output **and** every reading circuit input (dissolve). Hardware source: clear every reading circuit input. Hardware write sink: clear the producing circuit output.

- [ ] **Step 1: Write the failing tests**

Add to `TestGraphEdits`:

```cpp
    void getConnectedSinksForCable() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  hz = 0\n[lfo]\n  hz = 0\n");
        connectPins(p, "c0.0.output.out", "c0.1.hz.p");
        connectPins(p, "c0.0.output.out", "c0.2.hz.p");
        QStringList sinks = getConnectedSinks(p, "c0.0.output.out");
        QCOMPARE(sinks.size(), 2);
        QVERIFY(sinks.contains("c0.1.hz.p"));
        QVERIFY(sinks.contains("c0.2.hz.p"));
        delete p;
    }

    void getConnectedSourceForCable() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  hz = 0\n");
        connectPins(p, "c0.0.output.out", "c0.1.hz.p");
        QCOMPARE(getConnectedSource(p, "c0.1.hz.p"), QString("c0.0.output.out"));
        delete p;
    }

    void getConnectedSourceForOutputRegister() {
        Patch *p = parse("[lfo]\n  hz = O1\n");
        // reading O1 (an output register) resolves to its read pin
        QCOMPARE(getConnectedSource(p, "c0.0.hz.p"), QString("hw.O1.read"));
        delete p;
    }

    void disconnectWireClearsConsumerInput() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  hz = 0\n");
        connectPins(p, "c0.0.output.out", "c0.1.hz.p");
        QVERIFY(disconnectWire(p, "c0.0.output.out", "c0.1.hz.p"));
        QVERIFY(inAtom(p, 0, 1, "hz", 1) == nullptr);   // reader cleared
        QVERIFY(outAtom(p, 0, 0, "output") != nullptr); // producer cable left (orphan ok)
        delete p;
    }

    void disconnectWireOutToHwSinkClearsOutput() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        QVERIFY(disconnectWire(p, "c0.0.output.out", "hw.O1"));
        QVERIFY(outAtom(p, 0, 0, "output") == nullptr); // atom lived on the output
        delete p;
    }

    void disconnectPinDissolvesSource() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  hz = 0\n[lfo]\n  hz = 0\n");
        connectPins(p, "c0.0.output.out", "c0.1.hz.p");
        connectPins(p, "c0.0.output.out", "c0.2.hz.p");
        QVERIFY(disconnectPin(p, "c0.0.output.out"));
        QVERIFY(outAtom(p, 0, 0, "output") == nullptr);
        QVERIFY(inAtom(p, 0, 1, "hz", 1) == nullptr);
        QVERIFY(inAtom(p, 0, 2, "hz", 1) == nullptr);
        delete p;
    }

    void disconnectPinClearsSink() {
        Patch *p = parse("[lfo]\n  hz = P1.1\n");
        QVERIFY(disconnectPin(p, "c0.0.hz.p"));
        QVERIFY(inAtom(p, 0, 0, "hz", 1) == nullptr);
        delete p;
    }
```

- [ ] **Step 2: Run to verify failure**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
Expected: FAIL — `getConnectedSinks`/`getConnectedSource`/`disconnectWire`/`disconnectPin` undefined.

- [ ] **Step 3: Implement queries + disconnects in `graphedits.cpp`**

Add inside `namespace GraphEdits`:

```cpp
// Build the canonical pin id for a circuit input column.
static QString inputPinId(int s, int c, const QString &jack, int column)
{
    const char *suffix = column == 1 ? "p" : column == 2 ? "s" : "o";
    return QString("c%1.%2.%3.%4").arg(s).arg(c).arg(jack).arg(suffix);
}

// The net key a source produces: the cable name, or the register string, or empty.
static QString sourceNetKey(const Patch *patch, const PinRef &src)
{
    if (src.kind == PinRef::HwSource || src.kind == PinRef::HwRead)
        return src.reg;
    if (src.kind == PinRef::CircuitOutput) {
        const JackAssignment *out =
            patch->section(src.section)->circuit(src.circuit)->findJack(src.jack);
        const Atom *a = out ? out->atomAt(1) : nullptr;
        return a ? a->toString() : QString();
    }
    return QString();
}

// Visit every circuit input column atom in the patch.
template <class Fn>
static void forEachInputAtom(const Patch *patch, Fn fn)
{
    for (qsizetype s = 0; s < patch->numSections(); s++) {
        const PatchSection *sec = patch->section(s);
        for (unsigned c = 0; c < sec->numCircuits(); c++) {
            const Circuit *circ = sec->circuit(c);
            for (qsizetype j = 0; j < circ->numJackAssignments(); j++) {
                const JackAssignment *ja = circ->jackAssignment(static_cast<unsigned>(j));
                if (!ja->isInput())
                    continue;
                for (int col = 1; col <= 3; col++) {
                    const Atom *a = ja->atomAt(col);
                    if (a)
                        fn(static_cast<int>(s), static_cast<int>(c), ja->jackName(), col, a);
                }
            }
        }
    }
}

QStringList getConnectedSinks(const Patch *patch, const QString &sourcePin)
{
    QStringList out;
    const PinRef src = parsePin(patch, sourcePin);
    if (!src.isSource())
        return out;

    // A circuit output that drives a hardware sink (atom is a register): the sink is hw.<reg>.
    if (src.kind == PinRef::CircuitOutput) {
        const JackAssignment *o =
            patch->section(src.section)->circuit(src.circuit)->findJack(src.jack);
        const Atom *a = o ? o->atomAt(1) : nullptr;
        if (a && a->isRegister()) {
            out << (QString("hw.") + a->toString());
            return out;
        }
    }

    const QString key = sourceNetKey(patch, src);
    if (key.isEmpty())
        return out;
    forEachInputAtom(patch, [&](int s, int c, const QString &jack, int col, const Atom *a) {
        if (a->toString() == key)
            out << inputPinId(s, c, jack, col);
    });
    return out;
}

QString getConnectedSource(const Patch *patch, const QString &sinkPin)
{
    const PinRef snk = parsePin(patch, sinkPin);
    if (!snk.isSink())
        return QString();

    if (snk.kind == PinRef::CircuitInput) {
        const JackAssignment *in =
            patch->section(snk.section)->circuit(snk.circuit)->findJack(snk.jack);
        const Atom *a = in ? in->atomAt(snk.column) : nullptr;
        if (!a)
            return QString();
        if (a->isRegister()) {
            AtomRegister areg(a->toString());
            return patch->registerIsOutputOnly(areg)
                       ? (QString("hw.") + a->toString() + ".read")
                       : (QString("hw.") + a->toString());
        }
        if (a->isCable()) {
            // find the circuit output holding this cable
            const QString cable = a->toString();
            for (qsizetype s = 0; s < patch->numSections(); s++) {
                const PatchSection *sec = patch->section(s);
                for (unsigned c = 0; c < sec->numCircuits(); c++) {
                    const Circuit *circ = sec->circuit(c);
                    for (qsizetype j = 0; j < circ->numJackAssignments(); j++) {
                        const JackAssignment *ja = circ->jackAssignment(static_cast<unsigned>(j));
                        if (ja->isOutput() && ja->atomAt(1) && ja->atomAt(1)->toString() == cable)
                            return QString("c%1.%2.%3.out").arg(s).arg(c).arg(ja->jackName());
                    }
                }
            }
        }
        return QString();
    }

    // Hardware write pin: the producer is the circuit output whose atom == this register.
    if (snk.kind == PinRef::HwWrite) {
        for (qsizetype s = 0; s < patch->numSections(); s++) {
            const PatchSection *sec = patch->section(s);
            for (unsigned c = 0; c < sec->numCircuits(); c++) {
                const Circuit *circ = sec->circuit(c);
                for (qsizetype j = 0; j < circ->numJackAssignments(); j++) {
                    const JackAssignment *ja = circ->jackAssignment(static_cast<unsigned>(j));
                    if (ja->isOutput() && ja->atomAt(1) && ja->atomAt(1)->toString() == snk.reg)
                        return QString("c%1.%2.%3.out").arg(s).arg(c).arg(ja->jackName());
                }
            }
        }
    }
    return QString();
}

// Clear the atom physically stored at a sink (circuit input) or producer (circuit output).
static void clearJackAtom(Patch *patch, const PinRef &ref)
{
    JackAssignment *ja = jackFor(patch, ref);
    if (!ja)
        return;
    ja->replaceAtom(ref.kind == PinRef::CircuitInput ? ref.column : 1, nullptr);
}

bool disconnectWire(Patch *patch, const QString &fromPin, const QString &toPin)
{
    const PinRef a = parsePin(patch, fromPin);
    const PinRef b = parsePin(patch, toPin);
    if (!a.valid() || !b.valid())
        return false;
    // The connecting atom is stored at the circuit input, if one is involved.
    if (a.kind == PinRef::CircuitInput) { clearJackAtom(patch, a); return true; }
    if (b.kind == PinRef::CircuitInput) { clearJackAtom(patch, b); return true; }
    // Otherwise it is a circuit-output -> hw-sink wire: clear the output.
    if (a.kind == PinRef::CircuitOutput) { clearJackAtom(patch, a); return true; }
    if (b.kind == PinRef::CircuitOutput) { clearJackAtom(patch, b); return true; }
    return false;
}

bool disconnectPin(Patch *patch, const QString &pin)
{
    const PinRef ref = parsePin(patch, pin);
    if (!ref.valid())
        return false;

    if (ref.kind == PinRef::CircuitInput) {
        clearJackAtom(patch, ref);
        return true;
    }
    if (ref.kind == PinRef::HwWrite) {
        const QString srcPin = getConnectedSource(patch, pin);
        if (!srcPin.isEmpty())
            clearJackAtom(patch, parsePin(patch, srcPin));
        return true;
    }
    // Source: clear every reader, then (for a circuit output) clear the producer.
    const QStringList sinks = getConnectedSinks(patch, pin);
    for (const QString &sinkPin : sinks) {
        const PinRef snk = parsePin(patch, sinkPin);
        if (snk.kind == PinRef::CircuitInput)
            clearJackAtom(patch, snk);
        // hw write sinks: the atom is on the producing output, cleared next.
    }
    if (ref.kind == PinRef::CircuitOutput)
        clearJackAtom(patch, ref);
    return true;
}
```

- [ ] **Step 4: Run to verify the tests pass**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen
```
Expected: all Task 3 tests PASS.

- [ ] **Step 5: Commit (production only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphedits.cpp
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Implement GraphEdits queries + disconnectWire/disconnectPin\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Task 4: `copyWire`, `moveWire`, `rehomeWires`

**Files:**
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphedits.cpp`
- Test: `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/test_graphedits.cpp`

These compose the primitives:
- `copyWire(fromSink,toSink)` = connect `toSink` to whatever feeds `fromSink` (`getConnectedSource`); `fromSink` unchanged.
- `moveWire(fromSink,toSink)` = `copyWire` then clear `fromSink`.
- `rehomeWires(fromSource,toSource)` = re-point every reader of `fromSource` onto `toSource`, then clear `fromSource`'s producer atom. Capturing the reader list **before** mutating is essential.

- [ ] **Step 1: Write the failing tests**

Add to `TestGraphEdits`:

```cpp
    void copyWireDuplicatesToAnotherSink() {
        Patch *p = parse("[lfo]\n  hz = P1.1\n  output = 0\n");
        // sink c0.0.hz.p reads P1.1; copy onto another input
        QVERIFY(connectPins(p, "hw.P1.1", "c0.0.output.out") == false); // sanity: output is a source, not valid
        Patch *q = parse("[lfo]\n  hz = P1.1\n[lfo]\n  hz = 0\n");
        QVERIFY(copyWire(q, "c0.0.hz.p", "c0.1.hz.p"));
        QCOMPARE(inAtom(q, 0, 0, "hz", 1)->toString(), QString("P1.1")); // original kept
        QCOMPARE(inAtom(q, 0, 1, "hz", 1)->toString(), QString("P1.1")); // target reads same
        delete p; delete q;
    }

    void moveWireRelocatesConnection() {
        Patch *p = parse("[lfo]\n  hz = P1.1\n[lfo]\n  hz = 0\n");
        QVERIFY(moveWire(p, "c0.0.hz.p", "c0.1.hz.p"));
        QVERIFY(inAtom(p, 0, 0, "hz", 1) == nullptr);                   // origin cleared
        QCOMPARE(inAtom(p, 0, 1, "hz", 1)->toString(), QString("P1.1")); // target set
        delete p;
    }

    void rehomeMovesCableProducerPreservingReaders() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  hz = 0\n[lfo]\n  output = 0\n");
        connectPins(p, "c0.0.output.out", "c0.1.hz.p");
        const QString origCable = inAtom(p, 0, 1, "hz", 1)->toString();
        QVERIFY(rehomeWires(p, "c0.0.output.out", "c0.2.output.out"));
        QVERIFY(outAtom(p, 0, 0, "output") == nullptr);          // old producer cleared
        QVERIFY(outAtom(p, 0, 2, "output") != nullptr);          // new producer holds a cable
        // reader still wired to the (re-homed) producer
        QCOMPARE(inAtom(p, 0, 1, "hz", 1)->toString(),
                 outAtom(p, 0, 2, "output")->toString());
        Q_UNUSED(origCable);
        delete p;
    }

    void rehomeRejectedOntoOccupiedSource() {
        Patch *p = parse("[lfo]\n  output = _X\n[lfo]\n  output = _Y\n");
        QVERIFY(!isValidDrop(p, "c0.0.output.out", "c0.1.output.out", DragMode::Rehome));
        delete p;
    }
```

- [ ] **Step 2: Run to verify failure**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
Expected: FAIL — `copyWire`/`moveWire`/`rehomeWires` undefined.

- [ ] **Step 3: Implement in `graphedits.cpp`**

Add inside `namespace GraphEdits`:

```cpp
bool copyWire(Patch *patch, const QString &fromSink, const QString &toSink)
{
    const QString src = getConnectedSource(patch, fromSink);
    if (src.isEmpty())
        return false;
    return connectPins(patch, src, toSink);
}

bool moveWire(Patch *patch, const QString &fromSink, const QString &toSink)
{
    if (!copyWire(patch, fromSink, toSink))
        return false;
    clearJackAtom(patch, parsePin(patch, fromSink));
    return true;
}

bool rehomeWires(Patch *patch, const QString &fromSource, const QString &toSource)
{
    const PinRef from = parsePin(patch, fromSource);
    if (!from.isSource())
        return false;
    const QStringList sinks = getConnectedSinks(patch, fromSource); // capture before mutating
    for (const QString &sinkPin : sinks)
        connectPins(patch, toSource, sinkPin);
    if (from.kind == PinRef::CircuitOutput)
        clearJackAtom(patch, from);
    return true;
}
```

- [ ] **Step 4: Run to verify the tests pass**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen
```
Expected: all Task 4 tests PASS.

- [ ] **Step 5: Commit (production only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphedits.cpp
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Implement GraphEdits copyWire/moveWire/rehomeWires\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Task 5: `NodeItem::pinAt` — pin hit-testing

**Files:**
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/nodeitem.h`
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/nodeitem.cpp`
- Test: `/Users/jan.kaluza/Projects/droidforge/droidforge/tests/test_graphedits.cpp`

`pinAt(localPos)` returns the id of the pin whose anchor is within a comfortable radius of `localPos`, or an empty string. It uses the already-computed `pinAnchors` (item-local coordinates), so it is testable without a window.

- [ ] **Step 1: Declare `pinAt` in `nodeitem.h`**

Add this public method declaration after `pinAnchorLocal`:

```cpp
    // Returns the id of the pin whose connector contains localPos (within a
    // generous hit radius), or an empty string. localPos is in item coords.
    QString pinAt(const QPointF &localPos) const;
```

- [ ] **Step 2: Write the failing test**

Add to `TestGraphEdits` in `test_graphedits.cpp` (and add `#include "nodeitem.h"` and `#include "graphmodel.h"` to its includes):

```cpp
    void nodeItemHitTestsPins() {
        Patch *p = parse("[lfo]\n  hz = P1.1\n  output = _X\n");
        GraphDescription g = GraphModel::describe(p);
        const GraphNode *lfo = nullptr;
        for (const auto &n : g.nodes)
            if (n.kind == GraphNodeKind::Circuit) lfo = &n;
        QVERIFY(lfo);
        NodeItem item(*lfo);
        // The "hz" primary input pin: hitting its exact anchor returns its id.
        QString hzId;
        for (const auto &pin : lfo->pins)
            if (pin.label == "hz" && pin.role == GraphPinRole::Primary) hzId = pin.id;
        QVERIFY(!hzId.isEmpty());
        QPointF anchor = item.pinAnchorLocal(hzId);
        QCOMPARE(item.pinAt(anchor), hzId);
        // A point far from any pin returns empty.
        QCOMPARE(item.pinAt(QPointF(-500, -500)), QString());
        delete p;
    }
```

- [ ] **Step 3: Run to verify failure**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
Expected: FAIL — `NodeItem::pinAt` undefined.

- [ ] **Step 4: Implement `pinAt` in `nodeitem.cpp`**

Add after `pinAnchorLocal()`:

```cpp
QString NodeItem::pinAt(const QPointF &localPos) const
{
    if (layoutDirty) doLayout();
    // Hit radius: a bit larger than the drawn connector for comfortable grabbing.
    constexpr qreal HIT_RADIUS = CONNECTOR_RADIUS + 5.0;
    QString best;
    qreal bestDist = HIT_RADIUS;
    for (auto it = pinAnchors.cbegin(); it != pinAnchors.cend(); ++it) {
        const QPointF d = it.value() - localPos;
        const qreal dist = std::hypot(d.x(), d.y());
        if (dist <= bestDist) {
            bestDist = dist;
            best = it.key();
        }
    }
    return best;
}
```

Add `#include <cmath>` to `nodeitem.cpp` if not already present.

- [ ] **Step 5: Run to verify the test passes**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
```bash
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen
```
Expected: `nodeItemHitTestsPins` PASS.

- [ ] **Step 6: Build the app + commit (production only)**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/nodeitem.h droidforge/graphview/nodeitem.cpp
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Add NodeItem::pinAt for pin hit-testing\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Task 6: `WireItem` — selectable + hit-testable

**Files:**
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/wireitem.h`
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/wireitem.cpp`

**Verification:** by running the app (Qt selection state is not worth harness-testing). No new harness test.

A wire must be clickable for selection and expose its endpoints so `GraphView` can call `disconnectWire`. We widen the hittable area with a `shape()` around the cubic path and paint a highlight when selected.

- [ ] **Step 1: Update `wireitem.h`**

Replace the class body with (adds a public `graphWire()` accessor, `shape()`, and constructor sets selectable):

```cpp
class WireItem : public QGraphicsItem {
    GraphWire wire;
    QPointF fromPt; // scene coords, endpoint at producer
    QPointF toPt;   // scene coords, endpoint at consumer
    bool valid;     // false → skip drawing

public:
    explicit WireItem(const GraphWire &w, const QPointF &from, const QPointF &to);
    const GraphWire &graphWire() const { return wire; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};
```

Add `#include <QPainterPath>` to the header includes.

- [ ] **Step 2: Update `wireitem.cpp`**

In the constructor, after `setPos(0, 0);`, add:

```cpp
    setFlag(QGraphicsItem::ItemIsSelectable, valid);
    setZValue(-1); // wires sit behind nodes
```

Add a helper to build the cubic path (extract the path construction so `shape()` and `paint()` share it). Add near the top:

```cpp
static QPainterPath cubicPath(const QPointF &from, const QPointF &to)
{
    qreal tx = tangentX(from, to);
    QPainterPath path;
    path.moveTo(from);
    path.cubicTo(QPointF(from.x() + tx, from.y()),
                 QPointF(to.x() - tx, to.y()), to);
    return path;
}
```

Add `shape()`:

```cpp
QPainterPath WireItem::shape() const
{
    if (!valid) return QPainterPath();
    QPainterPathStroker stroker;
    stroker.setWidth(10.0); // generous click target
    return stroker.createStroke(cubicPath(fromPt, toPt));
}
```

In `paint()`, replace the local path construction with `QPainterPath path = cubicPath(fromPt, toPt);` and, before drawing, widen/recolor when selected:

```cpp
    if (option->state & QStyle::State_Selected) {
        pen.setColor(QColor(255, 255, 255));
        pen.setWidthF(pen.widthF() + 1.5);
    }
```

Add `#include <QPainterPathStroker>` and `#include <QStyleOptionGraphicsItem>` and `#include <QStyle>` to `wireitem.cpp`.

- [ ] **Step 3: Build the app**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build
```
Expected: links clean.

- [ ] **Step 4: Build the harness (graphshot links wireitem.cpp — confirm no break)**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
```
Expected: builds clean.

- [ ] **Step 5: Commit (production only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/wireitem.h droidforge/graphview/wireitem.cpp
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Make WireItem selectable and hit-testable (shape + highlight)\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Task 7: `GraphView` — drag-to-connect state machine + preview

**Files:**
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphview.h`
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphview.cpp`

**Verification:** by running the app. This is interaction code; correctness of the *edits* is already covered by Tasks 1–4. Confirm gestures by wiring on a real patch.

The drag begins on a pin (found via `NodeItem::pinAt`), draws a rubber-band to the cursor, highlights valid targets, sets the forbidden cursor over invalid ones, and on release calls the matching `GraphEdits` op then `commit()`. Disable `ScrollHandDrag` while a pin drag is active so the two don't fight.

- [ ] **Step 1: Extend `graphview.h`**

Add includes `#include "graphedits.h"` and forward-declare `class QGraphicsPathItem;`. Add protected overrides and private drag state:

```cpp
protected:
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;   // used in Task 8

private:
    // Returns the pin id under a scene position, or empty. Scans NodeItems.
    QString pinAtScene(const QPointF &scenePos) const;
    void commitEdit(bool ok, const QString &message);      // commit + rebuild if ok

    // Active drag state
    bool dragging = false;
    QString dragFromPin;
    GraphEdits::DragMode dragMode = GraphEdits::DragMode::Connect;
    QGraphicsPathItem *rubber = nullptr;
    QList<QGraphicsItem *> highlighted; // pins highlighted as valid targets (Task 7 keeps simple)
```

- [ ] **Step 2: Implement `pinAtScene` and the press handler**

In `graphview.cpp` add includes: `#include "nodeitem.h"`, `#include "graphedits.h"`, `#include <QMouseEvent>`, `#include <QKeyEvent>`, `#include <QGraphicsPathItem>`, `#include <QApplication>`, `#include <QPen>`.

```cpp
QString GraphView::pinAtScene(const QPointF &scenePos) const
{
    const QList<QGraphicsItem *> items = scene->items(scenePos);
    for (QGraphicsItem *it : items) {
        if (auto *ni = dynamic_cast<NodeItem *>(it)) {
            const QString pin = ni->pinAt(ni->mapFromScene(scenePos));
            if (!pin.isEmpty())
                return pin;
        }
    }
    // Fall back to a small search around the point (connectors sit on node edges).
    const QList<QGraphicsItem *> near =
        scene->items(QRectF(scenePos.x() - 8, scenePos.y() - 8, 16, 16));
    for (QGraphicsItem *it : near) {
        if (auto *ni = dynamic_cast<NodeItem *>(it)) {
            const QString pin = ni->pinAt(ni->mapFromScene(scenePos));
            if (!pin.isEmpty())
                return pin;
        }
    }
    return QString();
}

static GraphEdits::DragMode modeFor(const GraphEdits::PinRef &ref, Qt::KeyboardModifiers mods)
{
    using M = GraphEdits::DragMode;
    if (ref.isSource())
        return (mods & Qt::ControlModifier) ? M::Rehome : M::Connect;
    // sink
    if (mods & Qt::ShiftModifier)   return M::Copy;
    if (mods & Qt::ControlModifier) return M::Move;
    return M::Connect;
}

void GraphView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        const QString pin = pinAtScene(scenePos);
        if (!pin.isEmpty()) {
            GraphEdits::PinRef ref = GraphEdits::parsePin(patch, pin);
            dragMode = modeFor(ref, event->modifiers());
            dragFromPin = pin;
            dragging = true;
            setDragMode(QGraphicsView::NoDrag); // suspend pan while wiring
            rubber = new QGraphicsPathItem();
            QPen pen(QColor(255, 255, 255, 200));
            pen.setWidthF(1.5);
            pen.setStyle(Qt::DashLine);
            rubber->setPen(pen);
            rubber->setZValue(10);
            scene->addItem(rubber);
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}
```

- [ ] **Step 3: Implement move + release**

```cpp
void GraphView::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging) {
        const QPointF scenePos = mapToScene(event->pos());
        // rubber band from the anchored pin to the cursor
        QPointF from = scenePos;
        const QString target = pinAtScene(scenePos);
        // anchor point: look up the from-pin's scene pos via its NodeItem
        for (QGraphicsItem *it : scene->items()) {
            if (auto *ni = dynamic_cast<NodeItem *>(it)) {
                const QPointF a = ni->pinAnchorLocal(dragFromPin);
                if (!a.isNull()) { from = ni->mapToScene(a); break; }
            }
        }
        QPainterPath path;
        path.moveTo(from);
        path.lineTo(scenePos);
        rubber->setPath(path);

        const bool ok = !target.isEmpty()
                        && GraphEdits::isValidDrop(patch, dragFromPin, target, dragMode);
        viewport()->setCursor(target.isEmpty() ? Qt::ArrowCursor
                              : ok ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent *event)
{
    if (dragging && event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        const QString target = pinAtScene(scenePos);
        const QString from = dragFromPin;
        const GraphEdits::DragMode mode = dragMode;

        // tear down preview first
        if (rubber) { scene->removeItem(rubber); delete rubber; rubber = nullptr; }
        dragging = false;
        dragFromPin.clear();
        viewport()->setCursor(Qt::ArrowCursor);
        setDragMode(QGraphicsView::ScrollHandDrag);

        if (!target.isEmpty() && GraphEdits::isValidDrop(patch, from, target, mode)) {
            bool ok = false;
            QString msg;
            switch (mode) {
            case GraphEdits::DragMode::Connect:
                ok = GraphEdits::connectPins(patch, from, target); msg = tr("connect wire"); break;
            case GraphEdits::DragMode::Copy:
                ok = GraphEdits::copyWire(patch, from, target);    msg = tr("copy wire"); break;
            case GraphEdits::DragMode::Move:
                ok = GraphEdits::moveWire(patch, from, target);    msg = tr("move wire"); break;
            case GraphEdits::DragMode::Rehome:
                ok = GraphEdits::rehomeWires(patch, from, target); msg = tr("re-home wires"); break;
            }
            commitEdit(ok, msg);
        }
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void GraphView::commitEdit(bool ok, const QString &message)
{
    if (!ok)
        return;
    patch->commit(message);
    rebuildGraphics();
}
```

- [ ] **Step 4: Build the app**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build
```
Expected: links clean.

- [ ] **Step 5: Manual verification (run the app)**

Run:
```bash
open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"
```
Open a patch, switch to the graph, and verify by eye:
- Drag a circuit output to a circuit input → a wire appears (and the list view shows a new cable on both jacks).
- Drag a hardware source pin (e.g. `P1.1`) to a circuit input → a register wire appears.
- Drag a circuit output to a hardware write pin (e.g. `O1`) → the output now drives `O1`.
- Drag from a sink to another sink with **Shift** (copy) / **Ctrl** (move) → behaves per the matrix; the forbidden cursor shows over invalid targets and empty space.
- Esc-equivalent (release on empty space) cancels with no change.
- Undo (Cmd-Z) reverts each wiring action as one step.

- [ ] **Step 6: Commit (production only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphview.h droidforge/graphview/graphview.cpp
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Add GraphView drag-to-connect with rubber-band preview and cursor feedback\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Task 8: `GraphView` — wire selection/Delete + context menus

**Files:**
- Modify: `/Users/jan.kaluza/Projects/droidforge/droidforge/graphview/graphview.cpp`

**Verification:** by running the app.

Selecting a wire and pressing Delete removes it; right-clicking a pin or wire opens a menu of disconnect actions. All actions call `GraphEdits` then `commitEdit`.

- [ ] **Step 1: Implement `keyPressEvent` (Delete selected wires)**

```cpp
void GraphView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        const QList<QGraphicsItem *> sel = scene->selectedItems();
        bool any = false;
        for (QGraphicsItem *it : sel) {
            if (auto *wi = dynamic_cast<WireItem *>(it)) {
                const GraphWire &w = wi->graphWire();
                if (GraphEdits::disconnectWire(patch, w.fromPinId, w.toPinId))
                    any = true;
            }
        }
        if (any) {
            patch->commit(tr("delete wire"));
            rebuildGraphics();
            event->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}
```

Add `#include "wireitem.h"` and `#include <QMenu>`, `#include <QContextMenuEvent>` to `graphview.cpp`.

- [ ] **Step 2: Implement `contextMenuEvent` (pin and wire menus)**

```cpp
void GraphView::contextMenuEvent(QContextMenuEvent *event)
{
    const QPointF scenePos = mapToScene(event->pos());

    // Wire under the cursor? Offer Delete.
    for (QGraphicsItem *it : scene->items(scenePos)) {
        if (auto *wi = dynamic_cast<WireItem *>(it)) {
            const GraphWire w = wi->graphWire();
            QMenu menu(this);
            QAction *del = menu.addAction(tr("Delete connection"));
            if (menu.exec(event->globalPos()) == del) {
                if (GraphEdits::disconnectWire(patch, w.fromPinId, w.toPinId)) {
                    patch->commit(tr("delete wire"));
                    rebuildGraphics();
                }
            }
            event->accept();
            return;
        }
    }

    // Pin under the cursor? Offer per-wire + "Disconnect all".
    const QString pin = pinAtScene(scenePos);
    if (!pin.isEmpty()) {
        GraphEdits::PinRef ref = GraphEdits::parsePin(patch, pin);
        QMenu menu(this);
        QList<QPair<QString, QString>> targets; // (label, otherPin) for disconnectWire
        if (ref.isSource()) {
            const QStringList sinks = GraphEdits::getConnectedSinks(patch, pin);
            for (const QString &s : sinks)
                targets.append({tr("Disconnect from %1").arg(s), s});
        } else {
            const QString src = GraphEdits::getConnectedSource(patch, pin);
            if (!src.isEmpty())
                targets.append({tr("Disconnect from %1").arg(src), src});
        }
        QList<QAction *> acts;
        for (const auto &t : targets)
            acts.append(menu.addAction(t.first));
        QAction *all = nullptr;
        if (!targets.isEmpty()) {
            menu.addSeparator();
            all = menu.addAction(tr("Disconnect all"));
        }
        if (menu.isEmpty()) { event->accept(); return; }
        QAction *chosen = menu.exec(event->globalPos());
        if (chosen) {
            bool ok = false;
            if (chosen == all) {
                ok = GraphEdits::disconnectPin(patch, pin);
            } else {
                const int idx = acts.indexOf(chosen);
                if (idx >= 0)
                    ok = GraphEdits::disconnectWire(patch, pin, targets[idx].second);
            }
            if (ok) { patch->commit(tr("disconnect")); rebuildGraphics(); }
        }
        event->accept();
        return;
    }

    QGraphicsView::contextMenuEvent(event);
}
```

Note: `disconnectWire(pin, otherPin)` is order-independent (it clears the storing endpoint regardless of argument order), so passing `(pin, targets[idx].second)` is correct whether `pin` is the source or the sink.

- [ ] **Step 3: Add alt+click "disconnect all" to the press handler**

In `mousePressEvent`, at the very top of the `Qt::LeftButton` branch (before starting a drag), handle Alt+click:

```cpp
        if (event->modifiers() & Qt::AltModifier) {
            const QString pin = pinAtScene(mapToScene(event->pos()));
            if (!pin.isEmpty()) {
                if (GraphEdits::disconnectPin(patch, pin)) {
                    patch->commit(tr("disconnect all"));
                    rebuildGraphics();
                }
                event->accept();
                return;
            }
        }
```

- [ ] **Step 4: Build the app**

Run:
```bash
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build
```
Expected: links clean.

- [ ] **Step 5: Manual verification (run the app)**

Run:
```bash
open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"
```
Verify:
- Click a wire (it highlights white) → Delete/Backspace removes it; undo restores it.
- Right-click a fan-out source pin → menu lists each reader + "Disconnect all"; choosing one removes just that wire.
- Right-click a wire → "Delete connection".
- Alt+click a pin → all its wires vanish in one undo step.

- [ ] **Step 6: Commit (production only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphview.cpp
```
```bash
git -C /Users/jan.kaluza/Projects/droidforge commit -m "$(printf 'Add wire selection/Delete, context-menu disconnect, alt-click disconnect-all\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

## Self-Review

**1. Spec coverage:**
- Source/sink model + gesture matrix → Tasks 1 (`isValidDrop` + `DragMode`), 7 (`modeFor` + handlers), 8 (alt/right-click). ✔
- Four connection kinds (cable mint/reuse, hw-source→in, out→hw-sink, read-pin→in) → Task 2. ✔
- copy/move/re-home → Task 4. ✔
- disconnect (wire-delete, disconnect-pin, dissolve) → Task 3 + Task 8. ✔
- Direction + type safety → Task 1 (`isValidDrop`, `portKindOfPin`). ✔
- Rubber-band preview, valid-target cursor feedback, forbidden cursor → Task 7. ✔
- Wire selection + Delete, context menus → Tasks 6 + 8. ✔
- Pin hit-testing → Task 5. ✔
- Edge cases: output-drives-register read-back → Task 2 (`outputAlreadyRegisterReadsBackOnDragToInput`); re-home-onto-occupied rejection → Tasks 1+4 (`rehomeRejectedOntoOccupiedSource`); orphan/producerless cables → Task 3 (`disconnectWireClearsConsumerInput` asserts the producer cable is left). ✔
- Pure-logic in harness, GUI by running → reflected in every task's verification. ✔
- **Deferred (correctly absent):** inline values, pin add/remove, node drag/collapse, section drag-in/out, problem badges, drop-on-empty add-circuit, drag-onto-node create-pin, layout persistence. ✔

**2. Placeholder scan:** No TBD/TODO. Every code step shows complete code. GUI tasks (7–8) carry full handler implementations, verified by running the app (consistent with M1's plan convention for GUI paint/interaction code). The one non-code judgment ("adjust the type-mismatch test if `[display]` differs") names the concrete fallback (`display.text`).

**3. Type consistency:** `GraphEdits::PinRef`, `DragMode`, and every function signature are defined once in the Task-1 header and used unchanged in Tasks 2–8. `GraphPortKind` comes from `graphmodeltypes.h` (existing). `NodeItem::pinAt`/`pinAnchorLocal`/`graphNode` and `WireItem::graphWire`/`shape` match their declarations. Pin-id construction in `getConnectedSource`/`getConnectedSinks` (`c%1.%2.%3.out`, `hw.<reg>`, `hw.<reg>.read`) matches `graphmodel.cpp`'s scheme and `parsePin`'s inverse.

**Note on commit boundary:** per the refinement at the top, `GraphEdits` never calls `commit()`; `GraphView::commitEdit` (and the Task-8 handlers) commit exactly once per gesture. This keeps the harness tests free of `PatchEditEngine`.
