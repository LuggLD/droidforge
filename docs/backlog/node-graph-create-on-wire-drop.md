# Create nodes/pins by dropping wires on empty canvas or onto a node

- **Area:** node graph (editing)
- **Severity:** medium (workflow)
- **Status:** research first ("we should research that later" — operator), then design
- **Origin:** operator feedback after the rack-accurate hardware milestone, 2026-06-10

## Problem / wish

Operator: "I'd love a way to create nodes and pins from dropping wires on the
empty graph or onto somewhere on the node. I like how TiXL solves this, we
should research that later and tackle it at some point."

The wiring design spec explicitly deferred both halves of this
(`docs/superpowers/specs/2026-06-09-node-graph-wiring-design.md`:
"drop-on-empty-space → 'add circuit' menu; drag-onto-a-node → create-a-new-pin
— both explicitly out of this milestone"). This item picks them back up with a
concrete reference to study.

## To do

1. **Research how TiXL (Tooll 3, https://github.com/tooll3/t3) handles it:**
   dropping a connection on empty canvas opens its operator-picker filtered to
   compatible inputs; dropping onto a node body offers matching input slots.
   Capture what translates to DROID circuits (typed jacks, the circuit chooser
   dialog, compound inputs).
2. Design pass: what appears for circuit-out drops vs register drops; how the
   existing circuit-selection dialog is reused; where the new circuit lands
   (section, position) — connects to M3 layout persistence.

## Related

- [[node-graph-assign-registers-by-typing-or-dialog]]
- M2 sub-milestones: pin add/remove, inline constants.
