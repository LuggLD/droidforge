# List view + node graph: side-by-side use / deeper integration

- **Area:** node graph ↔ list editor
- **Severity:** medium (workflow)
- **Status:** needs design — "how exactly the two will integrate remains to be worked out" (operator)
- **Origin:** operator feedback after the rack-accurate hardware milestone, 2026-06-10

## Problem / wish

Operator: "I'd love a keyboard shortcut to switch between list and node view,
or a way to have both active at the same time. Using the menu to switch
between them is tedious."

**Interim shipped 2026-06-11:** `Ctrl+G` (⌘G) toggles the View → Node Graph
action. The deeper questions remain:

- Both views visible at once (splitter? second window? graph as a dock?).
- What "current selection" means across views — clicking a node/wire in the
  graph could move the list cursor and vice versa (the hub already broadcasts
  `cursorMoved`/`sectionSwitched` events that the graph currently ignores).
- Whether the graph stays a whole-patch view or can focus the current section
  while the list shows detail.

## Notes

- Everything flows through the `UpdateHub` already, so cross-view
  highlighting/cursor sync has plumbing to build on.
- Purely-additive rule applies: a second window or dock hosting `GraphView`
  can be created from our own integration code without touching the existing
  `editorStack` arrangement.
