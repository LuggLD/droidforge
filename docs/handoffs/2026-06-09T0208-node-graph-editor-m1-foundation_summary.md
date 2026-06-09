# Summary: 2026-06-09T0208-node-graph-editor-m1-foundation.md

## Executive summary

This session took a **visual node-graph editor for DROID Forge** from nothing to a working Milestone-1 foundation on branch `feature/node-graph-editor` (pushed to `origin` = LuggLD fork; **no PR**, by request). Full cycle: brainstorm → approved design spec (5c0e027) → M1 plan (1df8951) → subagent-driven execution (commits efb3173…77522f5). The feature is an *alternative read-only view* of the same `Patch` model, toggled with the list editor via a new **View ▸ Node Graph** menu item: circuits become nodes with compound primary/scale/offset pins (all three wireable) + text pins; hardware (registers) become source/sink nodes split by direction and grouped per controller/master, showing all available pins; cables render as circuit→circuit named-net wires, registers as direct connections; sections are translucent frames; everything laid out left-to-right by a longest-path auto-layout. Verified by **13 unit tests** in a deliberately **gitignored throwaway Qt Test harness** (`droidforge/tests/`, not committed — the shipped repo stays test-free) plus an offscreen `graphshot` PNG render. Key correctness catch: the plan's reference code used 0-based `atomAt`, but `JackAssignmentInput::atomAt` is **1-based** (primary=atomAt(1)/scale=2/offset=3) — fixed in 0301a45 with a shared `inputAtomSuffix` helper and a mapping-lock test; reviewers had missed it because they checked code *against the plan*, and the plan was wrong. Operator enforced strict **command hygiene** (no `cd`-compound / inline env-vars / inline python / `find -exec`) and added a global `~/.claude/CLAUDE.md` rule: never autonomously PR a repo not owned by LuggLD (upstream `Zarkuun/droidforge` is the eventual destination but operator opens that PR). Top deferred item: auto-layout's fixed row pitch makes tall nodes in a column **overlap and hide pins**. Milestones 2 (editing) and 3 (layout persistence) are designed but unbuilt.

## Contents

- **L1 — Title** — node-graph-editor M1 foundation, 2026-06-09.
- **L3 — TL;DR** — full-cycle session; M1 done on branch, pushed no-PR; app builds/runs; tests gitignored; layout-overlap caveat; global no-PR rule.
- **L12 — Changes landed** — table of 13 commits (spec, plan, Tasks 0–8 + the atomAt fix + final-review fix), all on `feature/node-graph-editor`; graph code in `droidforge/graphview/`.
- **L32 — Decisions made (and why)**
  - **L34 — 1.** Alternative view on the same `Patch` model (lossless round-trip); custom `QGraphicsScene`, not a node library.
  - **L37 — 2.** Whole-patch canvas; sections as frames; hardware as nodes split by direction + per controller; hardware shows all pins.
  - **L40 — 3.** Compound inputs = 3 wireable pins (primary/scale/offset); `text` is a distinct port kind; numeric types unify to "signal".
  - **L43 — 4.** Cables only for circuit→circuit; hardware connections store the register on the jack.
  - **L46 — 5.** Auto-layout default; opt-in `.ini`-comment position persistence deferred to M3.
  - **L49 — 6.** Throwaway gitignored test harness; shipped repo stays test-free.
  - **L52 — 7.** Subagent-driven execution with per-task spec + quality review.
- **L55 — Key gotchas** — `atomAt` is 1-based; `AtomCable::getCable()` drops the leading `_`; firmware json is blue-7 with fixed register counts; parser API `parseString(src, p)`; section-header `# ----` syntax; harness excludes modules + stubs ModuleBuilder (use `addController` directly); QStackedWidget value-member ownership safe; items don't use `the_colorscheme`; boundingRect must pad for edge connectors.
- **L67 — Process / discipline notes** — strict command hygiene (+ tell subagents the no-python alternative); reviewers-vs-plan miss plan-introduced bugs (assert ground-truth semantics); never PR non-owned repos; subagent loop is token-heavy (combine reviews for trivial tasks; screenshot GUI via `graphshot`).
- **L74 — Open follow-ups** — node-overlap layout fix (top item); rebuild-on-hidden perf; M2 editing; M3 persistence; deferred niceties; operator-only upstream PR.
- **L85 — Memories that would help** — `bash-command-style.md`, `plan-execution-preference.md`, global no-PR rule (all already written).
- **L93 — What works now** — app builds/launches in list view; toggle switches to a faithful live-rebuilding graph; 13/13 tests; branch pushed, `main` untouched.
- **L101 — Verification commands** — build/run app; configure/build/run `forgetests`; `graphshot` PNG; branch/remote state.
- **L121 — Specs + plan artifacts** — design spec + M1 plan paths (with the plan's `atomAt` snippet caveat).
