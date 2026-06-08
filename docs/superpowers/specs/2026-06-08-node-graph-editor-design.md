# Node Graph Editor — Design

**Date:** 2026-06-08
**Status:** Approved design, pending implementation plan
**Component:** DROID Forge (Qt 6 desktop app)

## Summary

Add a visual **node graph editor** to DROID Forge as an *alternative view* onto the
existing patch, living permanently alongside the current list-based editor. The Droid
system wires fixed-function blocks together with virtual cables, fixed values, and
physical I/O — a natural fit for node-graph representation. The product remains the same
`.ini` text file; the node graph is a new way to view and edit the *same* `Patch` model.

This document is the design contract. It captures decisions made during brainstorming and
the reasoning behind them. It is intentionally explicit about how Droid concepts map onto
node-graph concepts, because that mapping is load-bearing for the whole feature.

## Goals

- A node-graph view that edits the same `Patch` model the list editor uses, with lossless
  round-trip (because there is only one model).
- Whole-patch canvas showing end-to-end signal flow, left to right.
- Faithful to Droid semantics: compound inputs, named-net cables, typed text values,
  per-controller hardware.
- Maximize reuse of existing infrastructure (model, edit engine/undo, action registry,
  `QGraphicsView` substrate, `PatchProblem` validation).

## Non-Goals (deferred past V1)

- Keyboard shortcuts for graph actions.
- Click-through navigation from a controller node to the rackview.
- "Promote inline element to node" gestures.
- Combining a hardware unit's inputs and outputs into a single node (we split by direction).
- Replacing the list editor. The list editor stays, unchanged.

## Background: the existing model

The text file is the source of truth. The in-memory model is a strict hierarchy:

```
Patch → PatchSection (a page/tab) → Circuit (a block instance) → JackAssignment (a port) → Atom (the value)
```

- **Circuit** — an instance of a fixed-function block (`lfo`, `vco`, `sequencer`, …). Holds a
  name, optional comment, fold state, disabled flag, and an ordered list of `JackAssignment`s.
  It stores **only the jacks the user has actually added**, not the full catalog.
- **JackAssignment** — one per port. Subtypes `JackAssignmentInput`, `JackAssignmentOutput`,
  `JackAssignmentUnknown`. An **input holds three atoms** (`atoms[3]`) forming the Droid
  expression `primary × scale + offset`. An output holds a single atom.
- **Atom** — the value in a slot. Subtypes: `AtomCable` (a named net), `AtomRegister` (physical
  I/O: I1–8, O1–8, pots, LEDs, gates…), `AtomNumber` (constant), `AtomText` (display string),
  `AtomInvalid`.
- **Cables are global named nets**, resolved by name across the whole patch (all pages), not
  point-to-point references. One producer, many readers.
- The firmware catalog (`droidfirmware.json`, currently `blue-7`) defines every circuit's full
  jack list, each jack typed (`cv`, `gate`, `trigger`, `fraction`, `integer`, `stepped`,
  `bipolar`, `voltperoctave`, `text`) with an `essential` rating (0/1/2).
- Register types (`registertypes.h`): `I` input, `N` normalize, `O` output, `G` gate,
  `B` button, `L` LED, `P` pot, `E` encoder, `S` switch, `R` RGB-LED, `X` extra. Each
  `AtomRegister` also carries a controller index (0 = master, 1…N = a physical controller).
- The Forge already saves a single `.ini` and embeds its own metadata as `#`-comment lines that
  the firmware ignores but the parser reads back (`# title`, `# LABELS:`, `# description`).
- Existing views (`patchsectionview`) are built on `QGraphicsView`/`QGraphicsScene`.
- The right-click context menu is assembled from a central action registry
  (`ADD_ACTION(ACTION_NEW_JACK, …)`, `ACTION_ADD_REMAINING_JACKS`, `ACTION_RENAME_CABLE`,
  `ACTION_DUPLICATE_CIRCUIT`, `ACTION_DISABLE/ENABLE`, `ACTION_EDIT_VALUE`, `ACTION_FOLD_UNFOLD`…).

## Concept mapping

| Node-graph concept | Droid model |
|---|---|
| Node | Circuit (a block instance) |
| Port | JackAssignment (inputs left, outputs right) |
| Wire | A cable (named net) for circuit→circuit; a register atom for hardware connections |
| Source/sink node | A hardware register, grouped per unit and split by direction |
| Constant on a port | Number atom (or text atom for text jacks) |
| Group/frame | PatchSection |

## Design

### 1. Architecture

A new **GraphView**, toggleable with the existing list view (list ⇄ graph), rendering and
editing the *same* `Patch`. Built on `QGraphicsView`/`QGraphicsScene`, matching the existing
section editor. All mutations go through the existing edit engine and `commit()`/undo. The
scene rebuilds (or reconciles) on the existing `patchModified` signal. Because there is exactly
one model, round-trip, undo/redo, and selection stay consistent across both views with no
synchronization layer.

Rejected alternatives: a third-party Qt node library (impedance mismatch with compound pins,
section frames, per-controller hardware, and direct model binding; new dependency; licensing/Qt
6.11 risk), and a standalone window (loses the integration that makes the feature valuable).

### 2. Canvas, sections, and frames

- One canvas shows the **whole patch**.
- Each `PatchSection` is a **draggable rectangular frame** containing nodes — an optional
  organizing principle, not mandatory structure.
- A node's **containing frame determines its section**. Dragging a node from one frame into
  another moves the corresponding circuit between sections in the model.
- A node dropped **outside any frame** falls into a default section (the existing
  "Untitled Section"), which is created if absent.

### 3. Circuit nodes

- One node per circuit. Title bar shows the circuit type and optional comment.
- The body lists **only the jacks present in the model** (same rule as the list editor). New
  circuits are pre-populated at the same default jack-selection level the list editor uses.
- **Inputs** render as a framed **compound group of three pins — primary · scale · offset**.
  Each pin is independently either wired or shows a **constant text field**; the field is hidden
  when a wire is plugged in. Scale/offset render as secondary (smaller, hollow) pins under the
  primary. All three accept wires (scale and offset are full atoms, not just numbers).
- **Outputs** are single pins on the right (no scale/offset).
- **Collapse/expand** (chevron on the node's bottom edge, Unreal material-node style) is a
  **view-only** mechanism that hides pins which are both unconnected and default-valued
  (scale = 1, offset = 0, primary empty), shown as a `+N` chip. It does not change the model.
  This is distinct from add/remove, which changes the model.

### 4. Pins: add, remove, edit

- **No add-pin button.** Pin management lives in the **node's right-click menu**, which reuses
  the existing circuit/jack action registry (`ACTION_NEW_JACK`, `ACTION_ADD_REMAINING_JACKS`,
  `ACTION_REMOVE_UNDEFINED_JACKS`, `ACTION_RENAME_CABLE`, `ACTION_EDIT_VALUE`, etc.). The node
  view sets the model cursor/selection, then invokes the same `QAction`s as the list view.
- Add → choose from the firmware catalog of missing jacks; remove → delete that
  `JackAssignment`.
- (Keyboard shortcuts for these actions are deferred past V1.)

### 5. Value types and ports

Two port kinds:

- **Signal** (round pin): all numeric jack types (`cv`, `gate`, `trigger`, `fraction`,
  `integer`, `stepped`, `bipolar`, `voltperoctave`). These are interchangeable voltages —
  freely interconnectable, matching Droid's permissive model.
- **Text** (square pin): the `text` jack type (DB8E display strings — `display.text`,
  `display.header`, and the `header` jack on many controller circuits). A single pin, **no
  scale/offset**, rendering a string field when unset.

Type safety: a text pin and a signal pin cannot be cross-wired (blocked at connect time). No
circuit currently outputs text and there are no text registers, so in current firmware nothing
can wire into a text pin — but the distinct square pin is kept for legibility and future-proofing
(chosen over a plain field).

### 6. Hardware nodes

Hardware (registers) becomes nodes — a **view construct**, since registers are atoms in the
model, not circuits.

- **Split by direction:** sources (`I`, `N`, `P`, `B`, `E`, `S`) on the **left**; sinks
  (`O`, `G`, `L`, `R`) on the **right**. Preserves left-to-right signal flow.
- **Grouped per unit:**
  - Master inputs (`I`, `N`) — one node, left.
  - Master outputs (`O`, `G`) — one node, right.
  - Each controller's controls (`P`, `B`, `E`, `S`) — one node per controller, left.
  - Each controller's LEDs (`L`, `R`) — one node per controller, right.
- **All available pins are always shown** for the configured master, expanders, and
  controllers (derived from the rack configuration), with used pins visually distinguished from
  free ones — for at-a-glance "what's in use / what's available." (This differs from circuit
  nodes, which show only added jacks.)
- A hardware pin **fans out**: e.g. `P1.1` read by many circuit inputs = several wires from the
  one `P1.1` pin.

### 7. Connections and cables

A wire connects a **producer** to a **consumer**. Storage depends on the endpoints:

| Wire | Stored as |
|---|---|
| circuit out → circuit in | a cable: output jack atom = `_N`; each input's primary atom = `_N` |
| hardware source → circuit in | input's primary atom = the register (no cable) |
| circuit out → hardware sink | output jack atom = the register (no cable) |
| hardware → hardware | not expressible — disallowed |

- A named cable is needed **only** for circuit→circuit wires. Hardware connections store the
  register directly on the jack.
- An **output pin holds one atom** (one cable or one register). Fan-out happens on the cable
  (many readers) or on a source register (referenced by many inputs).
- Drawing a circuit→circuit wire **auto-mints** a fresh hidden cable name
  (`Patch::freshCableName()`). Wires show **no label** by default; reveal/rename via the existing
  rename-cable action.
- The whole-patch canvas means **no off-page stubs** — every wire is a real line.

### 8. Layout persistence

Node positions and frame rectangles are not part of the Droid format.

- **Auto-layout is the default**, and is always the fallback for any patch with no stored
  layout (including hand-written `.ini` files).
- An **opt-in setting** persists positions as `#`-comment metadata embedded in the `.ini`, using
  the same mechanism the Forge already uses for title/labels/description. Firmware ignores it.

### 9. Validation and errors

Reuse the existing `PatchProblem` machinery. Problems already detected — duplicate cable
producers, undefined jacks, register conflicts, LED mismatches — are rendered as badges on the
offending node or pin. Type mismatches (text↔signal) are prevented at connect time.

## V1 scope

**In:** whole-patch canvas; section frames with drag-in/out and default-section fallback;
circuit nodes with compound (3-pin) inputs, text pins, and view-only collapse; right-click pin
management reusing existing actions; hardware nodes (split by direction, per-controller/master,
all pins shown, fan-out); cables/wires with auto-naming and the producer/consumer storage rules;
`PatchProblem` badges and connect-time type safety; auto-layout with opt-in `.ini`-comment
persistence; list ⇄ graph toggle bound to the shared model and undo.

**Out (deferred):** keyboard shortcuts; rackview link-through; promote-to-node; combined
hardware nodes.

## Open implementation questions (for the plan, not blocking design)

- Exact auto-layout algorithm (layered left-to-right by signal flow is the natural fit).
- Scene rebuild vs incremental reconciliation on `patchModified` (start simple; optimize if
  needed).
- The precise `#`-comment grammar for persisted layout.
