# Output-register read-back feels wrong in the graph

- **Area:** node graph
- **Severity:** low–medium
- **Status:** needs a design pass
- **Origin:** interactive testing, 2026-06-10 (spec edge #1)

## Problem

When a circuit output drives an output register (e.g. `out = O1`) and you drag
that output to another circuit's input, the new wire connects from the hardware
node's **read pin** (`hw.O1.read`), not from the circuit output you grabbed —
because a DROID output jack holds exactly one atom (the register `O1`), so the
value can only be re-read via the register, not also produced as a cable. This
is correct in list/text form but reads oddly in a node graph (the wire appears
to jump to the hardware node).

## Options to weigh

- **(a)** Render the wire visually from the grabbed output to the input while the
  model still stores `O1` (treat `hw.O1.read` and the producing output as visual
  aliases).
- **(b)** Disallow dragging from a register-driving output to an input, requiring
  the read to start from the read pin.
- **(c)** Accept current behavior with a visual hint linking the producing output
  to its `*.read` pin.
- **(d)** *(operator's idea)* When a circuit output is wired to a register *and*
  to other circuits, auto-insert a "copy" circuit "docked" to the output
  register, so the producing output feeds a real cable consumed by both the
  copy→register and the other circuits — making the fan-out explicit instead of
  routing reads through the hardware read pin.
