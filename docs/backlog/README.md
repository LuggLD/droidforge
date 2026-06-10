# Backlog

One Markdown file per deferred issue. These are surfaced during development but
intentionally not done yet — fold each into the spec/plan of whatever milestone
picks it up. Supersedes the old `docs/superpowers/node-graph-followups.md`.

Each file carries a short header (Area / Status / Severity / Origin) followed by
the problem, proposed approaches, and notes.

## Node graph

| Issue | Severity | Status |
|---|---|---|
| [Make register interactions feel more wire-like](node-graph-register-interaction-ux.md) | medium | needs design pass |
| [Assign registers by typing or via the standard parameter dialog](node-graph-assign-registers-by-typing-or-dialog.md) | medium | proposed; next editing milestone |
| [Create nodes/pins by dropping wires on empty canvas or a node (TiXL-style)](node-graph-create-on-wire-drop.md) | medium | research TiXL first |
| [List view + node graph: side-by-side use / deeper integration](node-graph-list-view-integration.md) | medium | needs design (Ctrl+G interim shipped) |
| [Output-register read-back feels wrong in the graph](node-graph-output-register-readback-ux.md) | low–medium | needs design pass |
| [Copy/Move drop onto a hardware write pin: cursor valid, semantics off](node-graph-copy-move-onto-hw-write-pin.md) | low–medium | recorded 2026-06-10 |
| [G8 used only via RGB LEDs stays hidden; wire drops](node-graph-g8-visibility-rgb-only.md) | low | recorded 2026-06-10 |
| [Alt-click disconnect on an unconnected pin commits an empty undo step](node-graph-disconnect-empty-commit.md) | low | recorded 2026-06-10 |
| [Compound input pin role indicators](node-graph-compound-input-indicators.md) | low | appearance/polish |
| [Pin-id grammar duplicated (construction vs parsing)](node-graph-pin-id-grammar-duplication.md) | low (tech debt) | when convenient |
| [Model N's internal link to its input](node-graph-model-n-internal-link.md) | low | deferred |
| [M2/M3 design-spec deltas to reconcile](node-graph-m2-m3-spec-deltas.md) | n/a (reference) | reconcile when planning M2/M3 |
