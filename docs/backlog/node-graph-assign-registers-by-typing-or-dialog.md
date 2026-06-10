# Assign registers by typing or via the standard parameter dialog

- **Area:** node graph (editing)
- **Severity:** medium (capability gap)
- **Status:** proposed by operator, 2026-06-10 — fold into the next editing milestone
- **Origin:** operator feedback after the rack-accurate hardware milestone

## Problem

There is no way to *create* a G8/X7 (or any register) reference from the graph
other than dragging from an existing hardware pin — and hidden modules have no
pins to drag from (chicken-and-egg: the G8 node only appears once a gate is
used; see also `node-graph-g8-visibility-rgb-only.md`).

## Operator proposal

Mirror the list view's affordances:

- **Type a register name** while a pin is focused/selected — like typing
  `G2.3` onto a jack in the list editor.
- **Double-click a pin (and/or its label, maybe)** to open the standard
  "Edit input/output parameter" dialog and pick the atom there.

Either path writes the atom through `GraphEdits`/`commitEdit`, so the existing
hub broadcast auto-shows the G8/X7 module in rack and graph.

## Notes

- The list editor's dialog/typing machinery lives in the patchview/dialog code
  (e.g. the jack/atom selector dialogs) — reuse, don't reimplement; respect the
  purely-additive rule (invoke the existing dialogs from graph code).
- Related: inline constant values are their own planned M2 sub-milestone;
  double-click on a *constant* pin may want to route to the same dialog.
- Related: [[node-graph-create-on-wire-drop]] (creating things from the graph
  canvas more generally).
