# M2/M3 design-spec deltas to reconcile

- **Area:** node graph (reference, not a bug)
- **Severity:** n/a
- **Status:** reconcile when planning M2/M3
- **Origin:** 2026-06-09

The overall design spec `docs/superpowers/specs/2026-06-08-node-graph-editor-design.md`
predates the 2026-06-09 HW-register & layout work. Its M2 (editing) and M3
(persistence) sections describe a model that has since changed. **Trust the
current code over that spec** when writing the M2/M3 plans, and reconcile these
points:

- **§6 Hardware nodes** says sources are `I,N,P,B,E,S` (left) and sinks
  `O,G,L,R` (right), with "Master inputs (`I,N`)" on the left. Now superseded:
  classification comes from `Patch::registerIsOutputOnly()`; `N` is an **output**
  (on Master-out, right), so sources are `I,P,B,E,S` and outputs are
  `N,O,G,L,R,X`. Output nodes carry a write pin (`hw.<reg>`, left) **and** a read
  pin (`hw.<reg>.read`, right) — except `N`, which is write-only.
- **§7 Connections** — the producer/consumer storage table is still correct, but
  it predates output read-back. **M2 connection editing** (now largely shipped in
  the wiring milestone) handles dragging *from* an output node's read pin
  (`hw.<reg>.read`) to a circuit input, in addition to source→input and
  output→sink gestures. Read vs. write pins on the same hardware node are a real
  distinction the edit logic respects.
- **§8 + "Open implementation questions"** guessed auto-layout would be "layered
  left-to-right by signal flow." That was **rejected** (cross-section flow is
  bidirectional/unreliable). Shipped instead: per-section grid packing with
  source hardware pinned left and output hardware right, section bands stacked
  vertically. **M3 is unaffected** — node-position persistence keys on node ids
  (`c<S>.<C>`, `hw.<key>`), which are unchanged — but the spec's auto-layout
  description is stale.
- **V1 scope** in the spec bundled "auto-layout with opt-in `.ini`-comment
  persistence." In practice this split: auto-layout shipped; persistence is M3.
