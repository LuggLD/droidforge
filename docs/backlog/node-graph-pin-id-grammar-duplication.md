# Pin-id grammar duplicated (construction vs parsing)

- **Area:** node graph
- **Severity:** low (tech debt — no live bug)
- **Status:** worth doing before the grammar grows
- **Origin:** final wiring review, 2026-06-09

## Problem

The pin-id string grammar — `c<S>.<C>.<jack>.[p|s|o|out]`, `hw.<reg>`,
`hw.<reg>.read` — is encoded independently in two places:

- `graphmodel.cpp` *constructs* ids (`pinId`, `inputAtomSuffix`, `hwReadPinId`,
  `kHwPrefix`).
- `graphedits.cpp` *parses* them back (`parsePin`) and re-constructs some
  (`inputPinId`, `findOutputHolding`).

They agree today, but nothing enforces it — a change to one side would silently
break wiring (a wire id that no longer matches a pin id is dropped without error).

## Proposal

Hoist the grammar (both construct and parse) into one shared helper — e.g. a
small `pinid.{h,cpp}` or additions to `graphmodeltypes.h` — used by both sides.
