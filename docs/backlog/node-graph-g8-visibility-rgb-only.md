# G8 used only via its RGB LEDs stays hidden — its wire silently drops

- **Area:** node graph (+ rack view, same formula)
- **Severity:** low
- **Status:** recorded 2026-06-10 (rack-accurate hardware nodes review)
- **Origin:** code-quality review of the per-module hardware nodes work

## Problem

Module visibility keys G8 expanders on `highestGatePrefix()` — **gate** usage
only. A patch whose only G8 reference is an RGB write (`output = R17`, no
`G1.x`) therefore shows no `G8 #1` node, and the wire to `hw.R17` is silently
dropped by `rebuildGraphics` (the known drop-unresolvable-endpoints behavior).

The rack view has the *same* blindspot (`rackview.cpp:410` uses
`highestGatePrefix()` too) — but there the module is merely hidden, while in
the graph the user's wire disappears, which reads as data loss.

Pinned by the harness test `rgbOnlyG8ReferenceDropsWire` (documents "no crash,
wire dropped").

## Options

- Graph-side: also show G8 #n when any of its R registers
  (`R(8+8n+1)..R(8+8n+8)`) is used. Diverges from the rack's formula —
  contradicts the mirror-the-rack decision, so do it deliberately if at all.
- Render a dangling-wire affordance instead of dropping (ties into the broader
  "wires whose endpoint pin is missing" UX).
- Upstream fix in the rack formula (out of scope under the purely-additive
  rule; could be proposed to upstream separately).
