# Copy/Move drop onto a hardware write pin: cursor valid, semantics off

- **Area:** node graph (wiring gestures)
- **Severity:** low–medium
- **Status:** recorded 2026-06-10 (final milestone review)
- **Origin:** pre-existing wiring-milestone semantics, surface doubled by
  bidirectional G8 write pins

## Problem

`isValidDrop`'s Connect case now guarantees "valid cursor ⇒ the edit will
commit" for hardware write pins (`graphedits.cpp` HwWrite guard). The
**Copy/Move** cases don't have the same guard:

- Shift/⌘-dragging a wire onto a hw write pin (`hw.O1`, `hw.G1.3`) shows a
  valid cursor. On release, `copyWire`/`moveWire` resolve the dragged wire's
  source and call `connectPins(source, hwWritePin)`:
  - source is a hw read/source pin → `connectPins` refuses → silent no-op
    behind a valid cursor (the exact mismatch the Connect guard fixed).
  - source is a circuit output currently producing a **cable** →
    `connectPins` replaces the output's cable atom with the register,
    **stranding every other reader of that cable**. Copy semantics ("origin
    stays") are violated for the net.

## Options

- Mirror the Connect-case guard: in `isValidDrop` Copy/Move, when the target
  is `HwWrite`, require the *resolved* source to be a circuit output — and
  decide explicitly what copying a cable-producing output onto a hw sink
  should mean (probably: keep the cable AND add the register? Not expressible
  today — an output jack holds one atom).
- Or restrict Copy/Move targets to circuit inputs entirely (hw sinks only
  reachable via plain Connect from the producing output).

## Related

- `node-graph-disconnect-empty-commit.md` — same review pass.
