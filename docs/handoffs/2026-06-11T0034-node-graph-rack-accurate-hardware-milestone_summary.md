# Summary: 2026-06-11T0034-node-graph-rack-accurate-hardware-milestone.md

## Executive summary

This session shipped the rack-accurate hardware milestone — both "next session" backlog items — across 21 commits (`c831fab`…`2f96636`): the graph's hardware nodes are now built per visible rack module (Master/Master18, one in/out node pair per G8 expander, X7 with G9–G12 + R49–R56, master R/X pins) via a new pure unit `graphview/rackmodules.{h,cpp}` that mirrors `RackView::refreshScene`'s visibility formula; G8 expander jacks are bidirectional (read pin on the in-node, write pin on the out-node, simultaneous use = read-back per the manual); and graph commits broadcast through the `UpdateHub`, fixing rack auto-show/hide of X7/G8s from graph edits. A new operator hard rule shaped everything: the graph must be purely additive — zero preexisting droidforge files changed (formulas mirrored, not refactored; classification graph-side, not on `Patch`). Subagent-driven execution with two-stage reviews caught three real bugs pre-merge (`!=16` vs `==18` rack-expression drift, an `isValidDrop`/`connectPins` mismatch, a mid-drag-rebuild use-after-free). A post-milestone "white background" report turned out environmental — macOS auto-appearance switching, graph never owned its background — fixed by pinning the canvas dark. Operator feedback then yielded pin-hover cursors, delta-scaled wheel zoom + pinch + anchor-under-cursor, ⌘G view toggle, and four new backlog items (register-UX design pass, typing/dialog register assignment, TiXL-style create-on-drop, list/graph integration). Mid-session the operator corrected command hygiene: no `git -C` prefixes, no sed — memory updated. 84 harness tests green; operator verified the 10-item interactive checklist.

## Contents

- **L5 — TL;DR** — milestone shipped, hub fix, purely-additive rule, review catches, white-bg root cause, UX polish, hygiene correction, 84 tests.
- **L16 — Changes landed** — all 21 commits: spec/plan, rackmodules unit (4 TDD steps + review fixes), GraphModel rewrite, GraphEdits reclassification, hub broadcast, drag-teardown guard, backlog closure, dark canvas, zoom/cursor/⌘G, feedback backlog.
- **L39 — Issues filed** — 7 local backlog items (3 review findings, 4 operator wishes).
- **L53 — Decisions made (and why)**
  - **L55 — 1. Purely additive** — operator hard rule; mirror formulas with drift-risk comments; saved to memory.
  - **L58 — 2. Per-module nodes, mirror-rack visibility, R/X pins** — operator choices; gateless patches show no G8 node (intentional).
  - **L61 — 3. Bidirectional G8 jacks** — read pin in-node / write pin out-node; simultaneous use legal (manual + Forge problem checker agree).
  - **L64 — 4. Hub broadcast** — one rebuild, no recursion; QSettings (not ACTION) keeps graphshot harness clean; connection-order guarantees settings freshness.
  - **L67 — 5. Pre-existing-semantics findings recorded, not fixed** — copy/move-onto-hw-write and empty disconnect commits → backlog, not unilateral gesture changes.
  - **L70 — 6. Dark canvas pinned, hardcoded** — auto-appearance root cause; harness has no colorscheme.
  - **L73 — 7. Delta-scaled zoom** — ~7%/notch, pinch, AnchorUnderMouse flagged as the one unrequested extra.
- **L76 — Key gotchas** — auto-appearance variable; `allRegistersOf`'s bare master18 gates + missing g8 RGB offsets; graphview.cpp's no-mainwindow-include constraint; settings/connect ordering; isValidDrop↔mutation agreement; mid-gesture rebuilds; canonicalized-input assumption.
- **L86 — Process notes** — command hygiene v2 (no `git -C`, no sed — subagents need it verbatim); deliverables go in final messages; commit doc amendments immediately; one-liner-fix precedent; verify-before-fixing won again.
- **L94 — Workflow lessons** — two-stage reviews caught what self-reviews missed; model mix; no SendMessage → fresh fix-subagents.
- **L100 — Open follow-ups** — recommended next: "pins get values without dragging" milestone; register-UX brainstorm; TiXL research; M2 remainder; M3.
- **L112 — Memories** — new `graph-view-purely-additive.md`; updated `bash-command-style.md`.
- **L120 — What works now** — rack-accurate nodes incl. live MASTER18 switch; bidirectional gates with read-back; rack reconciliation from graph edits; UX items; 84 tests; branch at `2f96636`, clean, unpushed, no PR.
- **L128 — Verification commands** — app/tests/graphshot builds, branch-state log.
- **L146 — Specs + plan artifacts** — spec, plan, backlog index paths.
