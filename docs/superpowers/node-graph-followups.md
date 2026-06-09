# Node Graph — deferred follow-ups

Running backlog of items surfaced during development but deliberately deferred.
Each should be folded into the spec of the milestone that picks it up.

## Appearance & polish milestone

### Compound input pin indicators
A circuit's compound input (`primary × scale + offset`) currently renders as
three vertically stacked rows all showing the same jack label — e.g.
`input / input / input` — with no visual cue for which row is which.

Add a per-row indicator distinguishing the three roles:
- **Primary**: the jack name (e.g. `input`, `hz`).
- **Scale**: a multiply indicator, e.g. `*`.
- **Offset**: a bias/add indicator, e.g. `+`.

The scale and offset rows should be visually subordinate to the primary —
e.g. slightly indented — so the grouping reads as "this input, times this,
plus this." (Operator note, 2026-06-09.)

## Tech debt

### Pin-id grammar is duplicated (construction vs parsing)
The pin-id string grammar — `c<S>.<C>.<jack>.[p|s|o|out]`, `hw.<reg>`,
`hw.<reg>.read` — is encoded independently in two places: `graphmodel.cpp`
*constructs* ids (`pinId`, `inputAtomSuffix`, `hwReadPinId`, `kHwPrefix`), and
`graphedits.cpp` *parses* them back (`parsePin`) and re-constructs some
(`inputPinId`, `findOutputHolding`). They agree today, but nothing enforces it —
a change to one side would silently break wiring. Hoist the grammar (construct +
parse) into one shared helper (e.g. a small `pinid.{h,cpp}` or additions to
`graphmodeltypes.h`) used by both. Surfaced by the final wiring review,
2026-06-09. Low severity (no live bug), worth doing before the grammar grows.

## Model / correctness (deferred from the 2026-06-09 HW-register spec)

### Model N's internal link to its input
`N` (normalize) registers are currently plain write targets (write pin only;
read pin dropped). Semantically, writing `N1` sets the normalized value of
input `I1`, so the true signal path is `producer → N1 → (internally) → I1 →
readers of I1`. Modeling that internal link would let the graph show the flow
as N→I (the intended direction) and would give a read of `N1` a real endpoint
(today such a read would resolve to a non-existent pin and be skipped).

## Design-spec deltas for M2/M3 to reconcile

The overall design spec `specs/2026-06-08-node-graph-editor-design.md` predates
the 2026-06-09 HW-register & layout work. Its M2 (editing) and M3 (persistence)
sections describe a model that has since changed. **Trust the current code over
that spec** when writing the M2/M3 plans, and reconcile these points:

- **§6 Hardware nodes** says sources are `I,N,P,B,E,S` (left) and sinks
  `O,G,L,R` (right), with "Master inputs (`I,N`)" on the left. Now superseded:
  classification comes from `Patch::registerIsOutputOnly()`; `N` is an **output**
  (on Master-out, right), so sources are `I,P,B,E,S` and outputs are
  `N,O,G,L,R,X`. Output nodes carry a write pin (`hw.<reg>`, left) **and** a read
  pin (`hw.<reg>.read`, right) — except `N`, which is write-only.
- **§7 Connections** — the producer/consumer storage table is still correct, but
  it predates output read-back. **M2 connection editing** must handle dragging
  *from* an output node's read pin (`hw.<reg>.read`) to a circuit input (stores
  `input primary atom = that register`), in addition to the source→input and
  output→sink gestures. Read vs. write pins on the same hardware node are now a
  real distinction the edit logic must respect.
- **§8 + "Open implementation questions"** guessed auto-layout would be "layered
  left-to-right by signal flow." That was **rejected** (cross-section flow is
  bidirectional/unreliable). Shipped instead: per-section grid packing with
  source hardware pinned left and output hardware right, section bands stacked
  vertically. **M3 is unaffected** — node-position persistence keys on node ids
  (`c<S>.<C>`, `hw.<key>`), which are unchanged — but the spec's auto-layout
  description is stale.
- **V1 scope** in the spec bundled "auto-layout with opt-in `.ini`-comment
  persistence." In practice this split: auto-layout shipped; persistence is M3.
