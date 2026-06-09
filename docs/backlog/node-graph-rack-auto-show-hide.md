# Graph wiring doesn't trigger rack auto-show/hide of X7 and G8s

- **Area:** node graph
- **Severity:** medium
- **Status:** next session (adjacent to rack-accurate hardware nodes)
- **Origin:** interactive testing, 2026-06-10

## Problem

With "only show X7 if needed by the current patch" enabled, connecting/
disconnecting X7 wires in the **node graph** does not add/remove the X7 from the
rack the way the list editor does. The same applies to the automatic adding/
removing of G8 expanders as their gates come into / go out of use.

The graph mutates the model (via `GraphEdits` + `commit()`) but doesn't run
whatever rack-reconciliation the list-editor path triggers on the same edits.

## To do

Find the rack-reconciliation the list editor runs after a register
connect/disconnect and make graph commits run the same path (or hook it into the
shared post-commit flow). Ties into the rack-accurate hardware-node work.
