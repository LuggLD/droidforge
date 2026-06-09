# Node Graph Editor — Wiring (interactive connect/disconnect) — Design

**Date:** 2026-06-09
**Status:** Approved design, pending implementation plan
**Component:** DROID Forge (Qt 6 desktop app)
**Milestone:** First slice of "M2 — editing." Editing was decomposed into independent sub-milestones; this one is **wiring only**. Inline constant values, pin add/remove, node drag/collapse, section drag-in/out, and problem badges are each their own later milestone. Layout persistence remains M3.

## Summary

Make the node graph editable by **drawing and removing wires with the mouse**, while every mutation flows through the existing edit engine (`commit()`/undo) so the list view and the graph stay in lockstep. The graph already renders faithfully (M1) and classifies hardware registers correctly (the HW-register milestone). This milestone adds the interaction layer: drag a pin to a compatible pin to connect; modifiers and right-click to move, copy, re-home, and disconnect; click a wire and press Delete to remove it.

The design keeps M1's discipline: **all connection semantics live in a pure, GUI-free layer (`GraphEdits`)** that is TDD'd in the gitignored test harness; `GraphView` is a thin gesture translator verified by running the app.

## Background: how connections are stored

DROID has no point-to-point wire object. A drawn wire is backed by one of two model facts:

- **Cable (named net):** circuit-output → circuit-input. The producing output jack holds an `AtomCable("_CABLE1")`; each reading input holds the *same* cable name in one of its atom columns. One producer, many readers (fan-out). Cables are global across all sections.
- **Register reference:** hardware connections store the register atom directly on the consuming/producing jack — `hardware-source → circuit-input` (input atom = the register), `circuit-output → hardware-sink` (output atom = the register), and reading an output register back (input atom = the output register, drawn from the output node's read pin).

Relevant model facts (verified against the code):

- Mint a fresh cable: `Patch::freshCableName()` → `CABLE1`, `CABLE2`, … (`patch.cpp:661`).
- Enumerate/rewrite: `Patch::allCables()` (`patch.cpp:560`), `Patch::cableExists()` (`patch.h:76`), `Patch::renameCable()` (`patch.cpp:569`), `Patch::findCableConnections()` (counts, `patch.cpp:649`).
- Atom type/equality: `Atom::isCable()/isRegister()/isText()/isNumber()` (`atom.h:25-30`), `Atom::sameAs()` (`atom.cpp:4`), `AtomCable::getCable()` (`atomcable.h:18`), `AtomRegister::toString()` (`atomregister.cpp:85`).
- Mutate jacks: `JackAssignmentInput::replaceAtom(column, Atom*)` (1-based: 1=primary, 2=scale, 3=offset), `JackAssignmentOutput::replaceAtom(_, Atom*)`; read via `atomAt(column)` (`jackassignmentinput.cpp:38`) and output `atomAt(1)`/`getAtom()` (`jackassignmentoutput.h:14-18`).
- Walk the patch: `Patch::numSections()/section(i)` → `PatchSection::numCircuits()/circuit(n)` → `Circuit::numJackAssignments()/jackAssignment(i)`; `JackAssignment::isInput()/isOutput()` (`jackassignment.h:56`).
- Commit to undo: `PatchEditEngine::commit(QString)` (`patcheditengine.h:35`); fires `patchModified`, which `GraphView::rebuildGraphics()` already listens to.

## Core concept: source pins and sink pins

Every pin is either a **source** or a **sink**. Wires always flow source → sink.

- **Sources** (fan out to many sinks): circuit outputs; hardware *read* pins (`hw.O1.read`…); hardware *source registers* (`I`, `P`, `B`, `E`, `S`).
- **Sinks** (hold exactly one wire): circuit inputs (primary/scale/offset); hardware *write* pins (`O`, `G`, `L`, `R`, `N`).

This source/sink distinction — not the model storage — is what governs the gestures, guaranteeing wires read source → sink and that two sinks can never connect with the wire silently snapping to a shared source.

Pin-id scheme (unchanged from earlier milestones): circuit `c<S>.<C>.<jack>.[p|s|o|out]`; hardware write `hw.<reg>`; hardware read `hw.<reg>.read`; hardware source register `hw.<reg>`.

## Interaction model

A dragged wire carries an **atom** (a cable name or a register) plus one anchored end; a valid drop commits that atom to the appropriate jack.

| Pin kind | plain drag | shift+drag | ctrl+drag | alt+click | right-click |
|---|---|---|---|---|---|
| **Source** | add a **new** wire → drop on a **sink** (reuse the cable, or mint one if empty) | — (plain-drag already fans out) | **re-home** the whole net → drop on a **source** (producer moves, readers preserved) | disconnect all (dissolve the net) | selective "Disconnect from …" list + "Disconnect all" |
| **Sink** | **re-source** me → drop on a **source** (replaces my one wire) | **copy** my wire → drop on a **sink** (I stay; target also reads my source) | **move** my wire → drop on a **sink** (I clear; target reads my source) | disconnect me | "Disconnect" |

- **Wires** are selectable: click to select, **Delete/Backspace** (or right-click → "Delete") removes the selected wire.
- Shift/Ctrl on a sink and Ctrl on a source are **armed only when a wire is present** to carry.
- **Universal rules:**
  - Plain sink↔sink and source↔source drops are forbidden → `Qt::ForbiddenCursor`.
  - Type safety: a signal pin and a text pin can never connect, on any gesture.
  - The model is **not mutated until a valid drop commits.** Esc, or a drop on empty graph space, cancels cleanly and restores the prior visual state (no change). During a sink re-source drag, the existing wire is hidden and restored on cancel.

**Deferred (treated as plain cancellation for now):** drop-on-empty-space → "add circuit" menu; drag-onto-a-node → create-a-new-pin. Both are explicitly out of this milestone.

## Architecture

Two units, mirroring M1's pure-logic / GUI split.

### `GraphEdits` — pure connection logic (GUI-free, TDD'd)

New `droidforge/graphview/graphedits.{h,cpp}`. Operates on `Patch*` + pin-id strings, performs the atom mutations, and commits. No Qt GUI dependency. Functions are verbs:

| Function | Behavior |
|---|---|
| `connectPins(patch, fromPin, toPin)` | Create a wire (orientation-agnostic; resolves which is source/sink). Circuit output empty → mint `AtomCable(freshCableName())`; output already a cable → reuse `getCable()`; hardware source/read pin → use `AtomRegister`. Sets the sink's atom (and the producing output's atom when minting). Replaces the sink's prior atom. |
| `copyWire(patch, fromSink, toSink)` | Clone `fromSink`'s incoming atom onto `toSink`; `fromSink` unchanged. |
| `moveWire(patch, fromSink, toSink)` | Clone `fromSink`'s incoming atom onto `toSink`, then clear `fromSink`. |
| `rehomeWires(patch, fromSource, toSource)` | Move all of `fromSource`'s wires to `toSource`. Cable→cable: move the cable atom (clear old output, set new). Cross-type (readers reference a register, or the new source is a circuit output): rewrite readers to reference the new source's net, minting a cable on the new output if needed. |
| `disconnectWire(patch, fromPin, toPin)` | Remove one wire by clearing the atom on the endpoint that *physically stores* it: the consumer **input** for circuit→circuit / hw-source→in / read-pin→in; the producer **output** for circuit-out→hw-sink. |
| `disconnectPin(patch, pin)` | Remove every wire at a pin. Source → clear producer + all readers (dissolve). Sink → clear its one atom. |
| `getConnectedSinks(patch, sourcePin)` | The sink pin-ids wired to a source (scans the patch for atoms referencing the source's cable/register). For menus and dissolve. |
| `getConnectedSource(patch, sinkPin)` | The source pin-id feeding a sink, or empty. |
| `isValidDrop(patch, fromPin, toPin, mode)` | Direction check per the matrix (plain: sink↔source; copy/move: sink↔sink; re-home: source↔source) **and** type safety (Signal vs Text via the firmware jack type). |

`mode` is a small enum (`Connect`, `Copy`, `Move`, `Rehome`) set from the gesture's modifiers.

**Explicit edge cases** (specified, not improvised):

1. **Output already drives a register** (`output.atom == O1`) and the user plain-drags it to a circuit input: the input reads register `O1` (its atom becomes `AtomRegister("O1")`); the wire renders from the `hw.O1.read` pin. The read-pin machinery from the prior milestone exists for exactly this. One output cannot simultaneously hold a cable and a register.
2. **Re-home onto an occupied source:** if `toSource` already holds its own net, the drop is **rejected** (`Qt::ForbiddenCursor`) rather than silently clobbering it.
3. **Orphan cables:** removing a cable's last reader leaves the producing output's cable atom in place (a producer with no readers). This is valid and matches list-editor behavior; no garbage collection.
4. **Producerless cable:** dissolving via a sink, or `disconnectWire` on a circuit→circuit wire, can leave readers referencing a cable with no producer. Also valid (reads as 0/undefined); the existing `PatchProblem` machinery may flag it — rendering badges is a *later* milestone.

Every mutating function ends in `patch->commit(<message>)`, producing one undo step.

### `GraphView` — gesture translation (GUI, app-verified)

Adds, to `droidforge/graphview/graphview.{h,cpp}`:

- **Pin hit-testing:** `NodeItem::pinAt(scenePos) → pinId` (new), checking each stored anchor within `CONNECTOR_RADIUS` + slop. `GraphView` resolves the pin under the cursor on press.
- **Drag state machine:** `mousePressEvent` starts a drag if a pin is hit; the `DragMode` comes from `(source|sink) × event->modifiers()`. `mouseMoveEvent` updates a transient rubber-band; `mouseReleaseEvent` commits or cancels; `keyPressEvent` handles Esc (cancel) and Delete/Backspace (delete selected wire).
- **Preview:** a transient `QGraphicsPathItem` rubber-band from the anchored pin to the cursor. At drag start, compute the valid-target set once via `isValidDrop` over all pins and **highlight** them; hovering an invalid target / empty space sets `Qt::ForbiddenCursor`.
- **Wire selection:** `WireItem` gains `ItemIsSelectable`, a `shape()` (a stroked cubic path, widened for easy clicking), and a selected-state paint. (`WireItem` already stores its `GraphWire` with `fromPinId`/`toPinId`.)
- **Context menus:** right-click a pin or wire builds a local `QMenu` of dynamically-created `QAction`s ("Disconnect from `lfo.hz`", "Disconnect all", "Delete") wired to `GraphEdits` calls. (Pin add/remove via the global action registry belongs to a later milestone.)

On any successful commit, `patchModified` fires and `rebuildGraphics()` redraws the scene — **no incremental wire patching**, same as M1. The rubber-band and any pre-drag hidden wire are torn down before the rebuild.

## Testing

Same split as M1.

- **TDD'd in the gitignored harness** (`droidforge/tests/`, never committed): every `GraphEdits` function and `isValidDrop`, asserting resulting atoms on a parsed `Patch`. Coverage: cable mint vs reuse; fan-out (one source, many sinks); re-source replace; `copyWire`; `moveWire`; `rehomeWires` cable→cable and cross-type; `disconnectPin` dissolve (source) and clear (sink); `disconnectWire` endpoint resolution for each wire kind (incl. circuit-out→hw-sink clearing the **output**); direction rejection (sink↔sink, source↔source); type-safety rejection (signal↔text); the output-drives-register edge; re-home-onto-occupied rejection.
- **Verified by running the app:** the gestures, rubber-band, valid-target highlighting, the forbidden cursor, wire selection, and context menus. Qt mouse interaction is not worth unit-testing in the throwaway harness.

## File structure

**Production (committed):**
- `droidforge/graphview/graphedits.h` / `.cpp` — the pure connection-logic layer (new).
- `droidforge/graphview/graphview.h` / `.cpp` — add mouse/key event handling, drag state, rubber-band, context menus (modify).
- `droidforge/graphview/nodeitem.h` / `.cpp` — add `pinAt()` hit-testing (modify).
- `droidforge/graphview/wireitem.h` / `.cpp` — add selectability, `shape()`, selected paint (modify).
- `droidforge/CMakeLists.txt` — add `graphedits.{h,cpp}` (modify).

**Throwaway (gitignored, never committed):**
- `droidforge/tests/test_graphedits.cpp` (new) + `droidforge/tests/CMakeLists.txt` (add `graphedits.cpp` to `forgetests`).

## V1 scope (this milestone)

**In:** drag-to-connect with all four backing connection kinds (cable mint/reuse, hw-source→in, out→hw-sink, read-pin→in); the full gesture matrix (plain / shift / ctrl / alt+click / right-click) for sources and sinks; wire selection + Delete; rubber-band preview, valid-target highlighting, forbidden cursor; connect-time direction + type safety; commit/undo via the edit engine with scene rebuild on `patchModified`; all connection semantics in a pure, tested `GraphEdits` layer.

**Out (each its own later milestone):** inline constant-value editing; pin add/remove via the action registry; node drag-to-reposition and collapse/expand; section drag-in/out; `PatchProblem` badges; drop-on-empty → add-circuit menu; drag-onto-node → create-pin; layout persistence (M3).

## Open implementation questions (for the plan, not blocking design)

- The exact rubber-band item and highlight styling (reuse `WireItem`'s pen palette, or a dedicated preview pen).
- Whether `pinAt` slop and the wire `shape()` width need tuning for comfortable hit-testing (settle by running the app).
- Message strings passed to `commit()` per gesture (for the undo-history labels).
