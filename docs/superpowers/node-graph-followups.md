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

## Model / correctness (deferred from the 2026-06-09 HW-register spec)

### Model N's internal link to its input
`N` (normalize) registers are currently plain write targets (write pin only;
read pin dropped). Semantically, writing `N1` sets the normalized value of
input `I1`, so the true signal path is `producer → N1 → (internally) → I1 →
readers of I1`. Modeling that internal link would let the graph show the flow
as N→I (the intended direction) and would give a read of `N1` a real endpoint
(today such a read would resolve to a non-existent pin and be skipped).
