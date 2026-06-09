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

## Wiring milestone follow-ups (surfaced during interactive testing, 2026-06-10)

### Rack-accurate hardware nodes (MASTER vs MASTER18, G8 expanders, X7)
The hardware nodes are built from hardcoded firmware counts
(`numGlobalRegisters` = fixed 8 I / 8 N / 8 O / 12 G) and do **not** reflect the
configured rack. Per the DROID manual (verified):
- **Standard MASTER:** 8 CV inputs (I1–I8), 8 CV outputs (O1–O8), a 4×4 LED
  matrix. **No dedicated gate sockets** — gates come only from G8 expanders / X7.
- **MASTER18:** 8 CV outputs; **no CV inputs** — instead 2 gate/trigger inputs
  (I1, I2, logic-level) and 4 gate outputs (G1–G4); built-in VCO tuner; MIDI.
- **G8 expander:** up to 4, each 8 gate jacks (`G1.1…`, `G2.1…`), every jack
  input *or* output depending on use. The Forge shows one G8 by default.
- **X7:** 4 gate outputs (`G9`–`G12`) plus USB/MIDI.

Today the master node always shows I1–8 / N1–8 / O1–8 (correct for a standard
MASTER, **wrong for MASTER18**) and 12 gate pins that — after the 2026-06-10 id
fix — represent "one G8 (`G1.1`–`G1.8`) + X7 (`G9`–`G12`)", which matches the
Forge default but isn't disambiguated by what's actually installed. To do:
drive node construction from `Patch::typeOfMaster()` and the rack config
(`ModuleBuilder::allRegistersOf` / installed modules) instead of fixed counts;
render only installed G8 expanders (and additional ones, `g8≥2`); model
MASTER18's gate I/O. **Caveat:** `ModuleBuilder` is stubbed in the gitignored
test harness, so this is largely app-verified, not unit-testable there. Deferred
to a dedicated session (too big to fold into the wiring fixes). Operator note,
2026-06-10.

### Output-driving-a-register read-back feels wrong in the graph (edge #1)
When a circuit output drives an output register (e.g. `out = O1`) and you drag
that output to another circuit's input, the new wire connects from the hardware
node's **read pin** (`hw.O1.read`), not from the circuit output you grabbed —
because a DROID output jack holds exactly one atom (the register `O1`), so the
value can only be re-read via the register, not also produced as a cable. This
is correct in list/text form but reads oddly in a node graph (the wire appears
to jump to the hardware node). Needs a design pass. Options to weigh: (a) render
the wire visually from the grabbed output to the input while the model still
stores `O1` (treat `hw.O1.read` and the producing output as visual aliases);
(b) disallow dragging from a register-driving output to an input, requiring the
read to start from the read pin; (c) accept current behavior with a visual hint
linking the producing output to its `*.read` pin; (d) operator's idea — when a
circuit output is wired to a register *and* to other circuits, auto-insert a
"copy" circuit "docked" to the output register, so the producing output feeds a
real cable consumed by both the copy→register and the other circuits, making the
fan-out explicit instead of routing reads through the hardware read pin.
Operator note, 2026-06-10.

### Graph wiring doesn't trigger rack auto-show/hide of X7 and G8s
With "only show X7 if needed by the current patch" enabled, connecting/
disconnecting X7 wires in the **node graph** does not add/remove the X7 from the
rack the way the list editor does; same for automatic adding/removing of G8
expanders as their gates come into / go out of use. The graph mutates the model
but doesn't run whatever rack-reconciliation the list-editor path triggers. Ties
into the rack-accurate hardware-node work above. Operator note, 2026-06-10.

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
