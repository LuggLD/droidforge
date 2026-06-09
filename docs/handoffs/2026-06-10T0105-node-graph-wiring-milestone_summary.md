# Summary: 2026-06-10T0105-node-graph-wiring-milestone.md

## Executive summary

Shipped the first slice of M2 — **interactive wiring** of the node graph — on branch `feature/node-graph-editor` (continues the `…T0442` handoff; covers commits `47c1759`…`57c4987`). A spec (`47c1759`) and plan (`0278226`) drove 8 subagent-driven tasks building a **pure, GUI-free `GraphEdits` layer** (parse pin-ids → classify source/sink → `connectPins`/`disconnect*`/`copy`/`move`/`rehomeWires`), TDD'd in the gitignored harness, plus a thin `GraphView` gesture translator (drag-to-connect, rubber-band preview, wire select+Delete, context menus, alt-click) verified by running the app. Key design refinement: `GraphEdits` never commits — `GraphView::commitEdit` does, once per gesture — keeping the logic layer edit-engine-free and unit-testable on a plain `Patch`. Interactive testing surfaced 5 issues: **#1** gate wires silently dropped because gate pins were built `g8=0` ("G5") while the parser normalizes bare gates to `g8=1` ("G1.5"), so wire ids never matched pin ids and `rebuildGraphics` dropped them — fixed by building gate pins canonically (`2c301ec`); **#3** pick-up preview anchored at the grabbed pin instead of the wire's far end, and was straight not curved — fixed (`4f6e6fd`, `64a9548`, exposing `WireItem::curve`); **#4** rehome-onto-occupied-source now merges nets (`1915b39`); **#2** Ctrl→menu is macOS Control-click behavior (⌘ is the real modifier), not a bug; **#5** output-register read-back UX deferred to a design pass. Follow-ups moved from a single doc into `docs/backlog/` (one file per issue + README, `57c4987`). Local permission allowlist tightened to nondestructive repo-scoped git only. **29 GraphEdits + 20 GraphModel tests green; app builds clean; no PR.**

## Contents

- **L5 — TL;DR** — wiring milestone arc: spec→plan→8 tasks; pure GraphEdits + thin GraphView; 5 interactive issues (3 fixed, 1 platform, 1 deferred); backlog reorg; permission tightening.
- **L14 — Changes landed** — table of all session commits `47c1759`…`57c4987` with the *why* in Notes.
- **L36 — Decisions made (and why)**
  - **L38 — 1.** GraphEdits doesn't commit; GraphView commits once/gesture (keeps logic layer pure + testable).
  - **L41 — 2.** Sink/source unifying model; dragged wire carries an atom; guarantees source→sink.
  - **L44 — 3.** macOS Ctrl=right-click; ⌘ maps to Qt::ControlModifier — ⌘ is the move/re-home modifier.
  - **L47 — 4.** Gate pins must use canonical register ids (the headline #1 bug; "G5" vs "G1.5").
  - **L50 — 5.** Rehome-onto-occupied merges; copy/move onto occupied sink replaces.
  - **L53 — 6.** Backlog folder (one file/issue) over a single follow-ups doc; local md not GitHub issues.
- **L56 — Key gotchas** — `rebuildGraphics` silently drops unresolved wires; `AtomRegister(QString)` gate non-round-trip; `registerIsOutputOnly` makes master G1–G8 inputs; private non-const `findJack`; ScrollHandDrag eats clicks (explicit wire-select); verified MASTER vs MASTER18 hardware facts.
- **L65 — Process / discipline notes** — command hygiene (no `$(...)` in commits → two `-m`; operator's two permission pushbacks); verify-before-fixing (#1 repro test); subagent-driven review caught real bugs; GUI verified by running; no PR.
- **L73 — Open follow-ups** — table mapping each deferred item to its `docs/backlog/*.md`; remaining M2 sub-milestones + M3 persistence not started.
- **L89 — Memories** — reinforce `bash-command-style` (add "no `$(...)` in commit messages") + `plan-execution-preference`.
- **L96 — What works now** — wiring end-to-end incl. gates; full gesture matrix; curved far-end preview; one-undo-per-gesture; 20+29 tests; clean tree; no PR.
- **L105 — Verification commands** — build/run app; configure/build/run `forgetests`; branch log since `8cfbc69`.
- **L121 — Specs + plan artifacts** — wiring spec, plan, and the `docs/backlog/` folder.
