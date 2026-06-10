# Node Graph Editor — Rack-Accurate Hardware Nodes + Rack Reconciliation — Design

**Date:** 2026-06-10
**Status:** Approved design, pending implementation plan
**Component:** DROID Forge (Qt 6 desktop app)
**Milestone:** Covers two backlog items together: `docs/backlog/node-graph-rack-accurate-hardware-nodes.md` and `docs/backlog/node-graph-rack-auto-show-hide.md`. Both fold into this spec and close when it ships.

## Summary

The graph's hardware nodes today are built from fixed firmware counts (always 8 I / 8 N / 8 O / 12 G on a single Master-in/Master-out pair) and ignore the configured rack: wrong for MASTER18, blind to extra G8 expanders, and G8 jacks — physically bidirectional — can only be used as inputs. Separately, wiring edits made in the graph never trigger the rack view's auto-show/hide of the X7 and G8 modules, because graph commits bypass the `UpdateHub` broadcast the list editor uses.

This milestone makes the graph's hardware nodes **per-module and rack-accurate** (Master in/out, one in/out node pair per G8 expander, X7, matching exactly what the rack view shows) and routes graph commits through the **same `UpdateHub` path** as the list editor, so rack reconciliation and all other views update on graph edits.

**Hard constraint (operator rule, 2026-06-10):** the graph view must be **purely additive**. No modifications to preexisting droidforge code (`patch/`, `rackview/`, `main/`, `modules/`, …) — not even clean refactors. Where the design needs upstream logic, it either calls existing public APIs or mirrors small formulas into `graphview/` code with a source-of-truth comment. This milestone modifies **zero preexisting files**.

## Background: verified model facts

- **Current hardware-node construction:** `addHardwareNodes()` (`graphview/graphmodel.cpp:142–215`) builds `hw.master.in` / `hw.master.out` from `the_firmware->numGlobalRegisters(t)` for I/N/O/G, plus one `controls` + `leds` node per controller via `the_firmware->numControllerRegisters`. Pin construction is `appendRegisterPins()` (`graphmodel.cpp:102`): input-only register → one read pin `hw.<reg>` (Out); output-only register → write pin `hw.<reg>` (In) + read pin `hw.<reg>.read` (Out), except `N` which is write-only.
- **Register classification:** `Patch::registerIsOutputOnly()` (`patch.cpp:688`): N/O/L/R/X output-only; gates: `G9+` output-only, `g8==1` output-only **iff** `typeOfMaster()==18`, otherwise gates are "input" — which is the limitation this milestone replaces with a graph-side bidirectional classification.
- **Module register inventories** (`modules/module*.cpp::numRegisters`):
  - `master`: I×8, O×8, N×8, R×16 (the 4×4 LED matrix), X×1
  - `master18`: I×2 (gate/trigger inputs), O×8, G×4 — no N, no R, no X
  - `g8`: G×8 (`G<n>.1`–`G<n>.8`), R×8 (numbers offset per rack position)
  - `x7`: G×4 at offset 8 (`G9`–`G12`), R×8 at offset 48 (`R49`–`R56`)
- **Rack visibility formula** (`rackview/rackview.cpp:408–440`, `RackView::refreshScene`):
  - `g8_offset = (typeOfMaster() != 16) ? 1 : 0` — on MASTER18 the built-in gates occupy the `g8=1` bank, so external G8 expanders are numbered from `g8Number = 2`.
  - `show_g8s = max(QSettings "show_g8s", patch->highestGatePrefix() − g8_offset)`.
  - G8 #g (g = 1…show_g8s) is added with `g8Number = g + g8_offset` and `rgbOffset = 8 + g*8` (so the first shown G8 has `R17`–`R24`).
  - X7 is shown iff `!ACTION(ACTION_SHOW_X7_ON_DEMAND)->isChecked() || patch->needsX7()` (`patch.cpp:1171`).
- **Gate canonicalization gotcha (round two):** `ModuleMaster18` does not override `registerAtom`, so `allRegistersOf("master18")` yields gates as bare `G1`–`G4` (`g8=0`) — the same non-canonical form that caused the 2026-06-10 vanishing-wire bug. The `AtomRegister(QString)` parser normalizes bare `G1`–`G8` to `g8=1`. **Every gate register taken from `ModuleBuilder` must be round-tripped through its string form** before deriving pin ids.
- **Simultaneous gate in+out use is legal.** The DROID manual: a G8 jack is "an input or output, depending on how you use it in your patch"; writing makes it an output and concurrent reads are read-back of the written value (same as reading an `O` register). The Forge's `updateRegisterProblems()` (`patch.cpp:846`) deliberately raises no problem for it. The graph therefore allows it — no guard.
- **Update plumbing:** list-editor edits flow `patchModified` → `UpdateHub::modifyPatch` → `UpdateHub::patchModified` → `RackView::modifyPatch()` → `refreshScene()` (reconciles modules). `MainWindow::theHub()` is public (`mainwindow.h:94`); `GraphView` is constructed with the `MainWindow` pointer (`mainwindow.cpp:38`); the hub→graph rebuild connection already exists from M1 (`mainwindow.cpp:121`). The missing direction is graph→hub: `GraphView::commitEdit` (`graphview.cpp:248`) calls `patch->commit()` + `rebuildGraphics()` privately and broadcasts nothing.

## Part 1 — Module visibility (graph mirrors the rack)

New pure function in `graphview/` (new file, e.g. `rackmodules.{h,cpp}`):

```cpp
struct RackModuleSpec {
    QString  name;       // "master" | "master18" | "g8" | "x7"
    unsigned g8Number;   // for "g8": 1-based bank number (already includes g8_offset)
    unsigned rgbOffset;  // for "g8": R-register offset, rack formula 8 + g*8
};

struct RackVisibilitySettings {
    int  showG8s;        // QSettings "show_g8s"
    bool x7OnDemand;     // ACTION_SHOW_X7_ON_DEMAND checked?
};

QList<RackModuleSpec> visibleRackModules(const Patch *patch,
                                         const RackVisibilitySettings &s);
```

- Mirrors the `refreshScene` formula above exactly (master kind from `typeOfMaster()`, G8 count/numbering/RGB offsets, X7 condition). A comment marks `rackview.cpp:408–440` as the source of truth and flags the drift risk — accepted trade-off of the purely-additive rule.
- Deterministic (settings passed in by the caller) → TDD'd in the gitignored harness against a plain `Patch`.
- Controllers are not part of this list; the existing per-controller node code is untouched.
- `GraphModel` consumes the spec list when describing hardware nodes; the call site (in `graphview/`) reads `QSettings`/`ACTION` state. The harness, where `ACTION`/`QSettings` are stubbed or absent, constructs `RackVisibilitySettings` directly.

## Part 2 — Per-module hardware nodes

`addHardwareNodes()` is rewritten to build nodes per visible module, registers enumerated via `ModuleBuilder::allRegistersOf(name, 0, g8Number, rl)` (gates string-round-tripped to canonical form):

| Module | Source node (left) | Sink node (right) |
|---|---|---|
| MASTER | `hw.master.in` "Master in": I1–I8 | `hw.master.out` "Master out": N1–N8 (write-only), O1–O8, R1–R16, X1 (write+read) |
| MASTER18 | `hw.master.in` "Master in": I1, I2 | `hw.master.out` "Master out": O1–O8, G1.1–G1.4 (write+read) |
| G8 #n | `hw.g8.<n>.in` "G8 #n in": read pin per jack | `hw.g8.<n>.out` "G8 #n out": write pin per jack, R pins (write+read) |
| X7 | — | `hw.x7.out` "X7": G9–G12 (write+read), R49–R56 (write+read) |
| Controllers | unchanged (`hw.ctrl<N>.controls`) | unchanged (`hw.ctrl<N>.leds`) |

- Node kinds stay `HardwareSource` / `HardwareSink`, so `GraphLayout`'s left/right column logic applies unchanged; the new nodes simply stack in their columns.
- Nodes with no pins are dropped (existing behavior). On MASTER18, `hw.master.in` still exists (I1, I2).
- `hw.master.in`/`hw.master.out` ids are kept; `hw.g8.<n>.*` and `hw.x7.out` are new. M3 layout persistence has not landed, so id changes are free; M3 will key on these ids.
- Per-pin `used` flags keep coming from `Patch::registerUsed()` (unchanged).
- New register types appearing on master/G8/X7 nodes (R, X) are handled by the existing `appendRegisterPins` output-only path (write + read pin), same as controller LEDs today.

## Part 3 — Bidirectional G8 jacks

New graph-side classification (lives in `graphview/`, e.g. next to `visibleRackModules`; **not** on `Patch`):

```cpp
// A register the hardware can use as input or output depending on patch usage.
// True exactly for gate registers on a G8 *expander*:
//   master16: g8Number 1..4;  master18: g8Number >= 2
// (master18's built-in g8==1 bank stays output-only; X7's G9+ stay output-only)
bool registerIsBidirectional(const Patch *patch, const AtomRegister &reg);
```

Pin scheme for a bidirectional register (one read pin + one write pin, existing grammar, no collisions):

- **Read pin `hw.<reg>.read`** (direction Out) on the module's **"in" node** — using the jack as a gate/trigger/clock input, or reading back a written value.
- **Write pin `hw.<reg>`** (direction In) on the module's **"out" node** — driving the jack as an output.

Consequences:

- `hwReadPinId()` / `appendRegisterPins()` (`graphmodel.cpp:88–140`) learn the third classification: bidirectional → read id is `base + ".read"` and the two pins land on different nodes.
- `addWires()` resolves a circuit-input atom holding a bidirectional register to `hw.<reg>.read`, and a circuit-output atom holding it to `hw.<reg>` — automatically, once `hwReadPinId` is taught the rule.
- `GraphEdits` (`portKindOfPin`, `isValidDrop`, `connectPins`) must classify `hw.<reg>.read` of a bidirectional gate as a **source** and `hw.<reg>` as a **sink**. Existing GraphEdits/GraphModel tests that assume master16 gates are plain source pins (`hw.G1.3`) are updated to the new scheme — a deliberate behavior change, TDD'd.
- Simultaneous use of both pins of one jack is allowed (read-back semantics, see Background). No new validity rule.

## Part 4 — Graph commits broadcast through the UpdateHub

All inside `graphview/graphview.cpp`:

1. `GraphView` gets a `patchModified()` signal. In its constructor it self-connects:
   `connect(this, &GraphView::patchModified, mainWindow->theHub(), &UpdateHub::modifyPatch);`
2. `commitEdit()` replaces its direct `rebuildGraphics()` call with `emit patchModified()`. The hub then fans out to **every** subscriber: `RackView::modifyPatch()` → `refreshScene()` (the auto-show/hide fix), the list editor, and back to our own `rebuildGraphics` via the existing `mainwindow.cpp:121` connection. One rebuild, no double work, and graph edits now update all views — not just the rack.
3. The graph also rebuilds when module visibility changes without a patch edit: self-connect the seven view actions — `ACTION_SHOW_USED_G8s`, `ACTION_SHOW_ONE_G8`, `ACTION_SHOW_TWO_G8`, `ACTION_SHOW_THREE_G8`, `ACTION_SHOW_FOUR_G8` (these write the `show_g8s` setting), `ACTION_SHOW_X7_ON_DEMAND`, `ACTION_SHOW_X7_ALWAYS` (`mainwindow.cpp:331–339`, `editoractions.cpp:369+`) — `::triggered` → `rebuildGraphics`. Mirrors what the rack does on the same toggles. Connection order note: the rack/settings handlers update `QSettings("show_g8s")` on these actions; the graph must read settings at rebuild time (it does — rebuild re-queries `visibleRackModules`'s inputs), so ordering doesn't matter.

Preexisting files touched: **none** (the hub→graph connection already exists from M1).

## Testing

- **Harness (TDD):** `visibleRackModules` (master16/18, show_g8s vs highestGatePrefix, g8_offset, RGB offsets, X7 condition), `registerIsBidirectional`, the `hwReadPinId`/pin-construction changes, and the `GraphEdits` gate reclassification. All on a plain `Patch`, as today (49 existing tests must stay green, updated where the gate scheme intentionally changed).
- **App-verified:** node construction through the real `ModuleBuilder` (stubbed in the harness — extend the stub only as far as compilation requires), plus the hub wiring.
- **Interactive checklist for the operator:** master16 vs master18 patch shows correct pins; wiring a `G2.x` jack makes G8 #2 appear in the rack (and in the graph); deleting the last X7 wire hides the X7 (with show-on-demand enabled); toggling the X7/G8 view actions updates the graph; G8 jack drives as output and reads as input, including both at once; undo of a graph edit updates rack and graph.

## Out of scope

- Output-register read-back UX redesign (`backlog/node-graph-output-register-readback-ux.md`) — bidirectional gates reuse today's read-pin pattern as-is.
- Compound input pin role indicators, pin-id grammar dedup, Model N internal link (their backlog items stand).
- Remaining M2 sub-milestones (inline constants, pin add/remove, node drag/collapse, section drag-in/out, problem badges) and M3 persistence.
