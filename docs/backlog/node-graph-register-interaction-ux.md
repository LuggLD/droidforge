# Make register interactions feel more wire-like

- **Area:** node graph (interaction design)
- **Severity:** medium (UX direction)
- **Status:** needs design pass — "I don't know what that means yet" (operator)
- **Origin:** operator feedback after the rack-accurate hardware milestone, 2026-06-10

## Problem

Operator: "I feel like we need a better way to deal with registers. We already
have differently colored wires on them, but we need to make interacting with
them more intuitive, so they behave more like wires. I don't know what that
means yet."

Register connections are stored differently from cables (atom on the consuming
or producing jack, no net object), and some of that model leaks into the
interaction: read-back wires flow right-to-left, registers appear both as pins
and as wire colors, fan-out and disconnect behave subtly differently from
cable nets.

## To do

Brainstorm/design session: what would "registers behave like wires" mean
concretely? Candidate threads to pull on:

- Treat a register's full reader set as a *net* in every gesture (re-home,
  disconnect-all already do; copy/move semantics around `hw.<reg>` pins are
  rougher — see [[node-graph-copy-move-onto-hw-write-pin]]).
- The read-back presentation question is a subset of this:
  [[node-graph-output-register-readback-ux]].

## Related

- `node-graph-output-register-readback-ux.md`
- `node-graph-copy-move-onto-hw-write-pin.md`
