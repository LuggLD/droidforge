# Node Graph — Hardware Register Correctness & Section-Aware Auto-Layout

**Date:** 2026-06-09
**Status:** Approved (design)
**Branch:** `feature/node-graph-editor`
**Follows:** `2026-06-08-node-graph-editor-design.md` (overall design), Milestone 1 (read-only graph)

## Motivation

Running the node graph on a real patch (`EnOscBuddy v2.ini`) surfaced correctness
gaps that Milestone 1's model and auto-layout did not handle. Three pure bugs
(unbounded zoom, a sink-sentinel layout explosion, and fixed-pitch node overlap)
were already fixed in commit `4750f8d`. This spec covers the two remaining,
deeper issues that require model and layout changes:

1. **Hardware registers used in their non-default direction.** DROID allows
   reading back output registers and writing the normalize register. The graph
   mis-modelled both.
2. **Section frames overlap.** Auto-layout positioned nodes by global data-flow
   with no awareness of sections, so section bounding boxes overlapped — in
   EnOscBuddy one section was drawn entirely inside the other.

## Ground truth (from the DROID manual + `patch.cpp`)

- **Input registers** (`I`, `P`, `B`, `E`, `S`) are read-only. They can *never*
  be written.
- **Output registers** (`O`, `G`, `L`, `R`, `X`) are written and can *also* be
  read back.
- **`N` (normalize)** is an *output* register that internally connects to the
  matching input. It is therefore on the output side, not the input side.
- The codebase already encodes exactly this split in
  `Patch::registerIsOutputOnly()` (`patch.cpp:688`): `false` for the read-only
  inputs, `true` for the outputs (including the gate-number/master nuances for
  `G`). The graph's current `isSourceRegisterType()` disagrees (it wrongly lists
  `N` as a source) and is the source of the bug.

## Decisions

### 1. Register classification via `registerIsOutputOnly()`

Replace the graph's `isSourceRegisterType()` with the authoritative
`Patch::registerIsOutputOnly()`. A register is a **source** (read-only input)
iff `registerIsOutputOnly()` is false; otherwise it is an **output**. This is a
per-register query (not per-type), so the `G` gate nuances are handled
correctly. `N` consequently moves from the source side to the output side.

### 2. Node pins by direction

- **Source nodes** (far-left; Master in, controller controls): one **read pin**
  per register (right-side / `Out` connector), as today. Sources can never be
  written, so they never gain a left connector.
- **Output nodes** (far-right; Master out, controller LEDs): every register row
  carries **both** a **write pin** (left-side / `In` connector) and a **read
  pin** (right-side / `Out` connector). This is shown for all available output
  registers, consistent with the existing "show all available pins" decision —
  reading is a capability of every output, so the read connector is always
  present (its `used` flag still reflects whether it is actually read). `N` rows
  appear here.

  *Rationale for "always show the read connector" over "only when read": it is
  predictable, needs no usage scan to decide pin existence, and matches the
  show-all-available principle. Per-row visual de-emphasis of an unused read
  connector is handled by the existing `used` styling.*

### 3. Wiring rules (`addWires`)

- **Read an input register** (`circuit.input = I1`): wire from the source node's
  read pin → circuit input. Unchanged.
- **Write an output register** (`circuit.output = O1` / `= N1`): wire from
  circuit output → the output node's **write pin**. Flows left→right.
- **Read an output register back** (`fold.input = O1`): wire from the output
  node's **read pin** → circuit input. This is a right→left back-edge (the
  output node is on the far right); accepted as-is for now.

The pin-id scheme gains a distinct id for the output read pin so producers and
consumers resolve unambiguously (e.g. `hw.<reg>` for the write pin and a
`hw.<reg>.read` form for the read pin, or equivalent — exact spelling is an
implementation detail for the plan, but the two must be distinct ids).

### 4. Section-aware auto-layout

Global longest-path layering is abandoned: cross-section cabling is common and
bidirectional, so flow direction is not a reliable layout axis, and auto-layout
is a fallback (manual layout persistence is the eventual prize — Milestone 3).
The new layout:

- **Source hardware** → a far-left column (nodes stacked vertically).
- **Output hardware** → a far-right column.
- **Each section** → a compact **grid** of its circuits: roughly `ceil(sqrt(n))`
  columns, filled row-major in patch order (mirrors the list view's ordering),
  producing a tidy rectangle. Cell pitch uses the existing per-node
  `NodeItem::heightFor()` and `NODE_WIDTH`.
- **Section rectangles are tiled vertically** (stacked top-to-bottom) with gaps,
  guaranteeing frames never overlap.
- Frame rect = bounding box of the section's grid. Hardware nodes are not
  framed.

The zoom clamp and per-node height stacking from commit `4750f8d` are retained.

### 5. Sectionless patches

A `.ini` with no section headers is legal. The parser already normalizes this:
the first circuit with no current section auto-creates a single empty-titled
section (`patchparser.cpp:305`), and an empty title renders as "Untitled
section" (`getNonemptyTitle()` → `SECTION_DEFAULT_NAME`). `GraphModel::describe()`
iterates `patch->numSections()`, which is therefore always ≥ 1. No special-casing
is needed: a sectionless patch renders as one "Untitled section" rectangle around
all grid-packed circuits — the same normalization the list view relies on.

## Non-goals (deferred)

- Modeling `N`'s internal link to its corresponding input register.
- Smart routing / minimizing crossings of read-back back-edges.
- Manual layout persistence (Milestone 3) and editing (Milestone 2).
- Any change to the list view or the patch file format.

## Testing (throwaway harness)

- Reading an output register back (`fold.input = O1`) produces a wire from the
  register's **read pin**, not a source pin.
- `N` is classified as an output (appears on an output node with a write pin).
- Writing `N1` connects to its write pin.
- Section frame rects are pairwise **disjoint** (no two intersect).
- A sectionless patch yields exactly one frame titled "Untitled section".
- Source-hardware column is leftmost; output-hardware column is rightmost.
- Retire `downstreamCircuitIsRightOfUpstream` (grid layout no longer guarantees
  data-flow column ordering).
- `graphshot` re-render of `EnOscBuddy v2.ini` as the visual confirmation.
