# Alt-click "disconnect all" on an unconnected pin commits an empty undo step

- **Area:** node graph (wiring gestures)
- **Severity:** low
- **Status:** recorded 2026-06-10 (final milestone review)
- **Origin:** pre-existing wiring-milestone behavior, amplified by the hub
  broadcast

## Problem

`GraphEdits::disconnectPin` returns true even when the pin had no connections,
so Alt-clicking an unconnected pin commits a no-op "disconnect all" undo step —
and, since graph commits now broadcast through the `UpdateHub`, also triggers a
full rack/list refresh for nothing.

## Fix sketch

Make `disconnectPin` (and possibly `disconnectWire`) return false when nothing
was cleared, so `GraphView::commitEdit(false, …)` skips the commit. Needs a
small audit of the call sites' expectations plus harness tests.
