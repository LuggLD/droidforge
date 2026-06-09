# 2026-06-09 — node-graph-editor M1 foundation

## TL;DR

- Brand-new feature on branch `feature/node-graph-editor` (pushed to `origin` = LuggLD/droidforge fork, **no PR** by request): a visual **node-graph editor** for DROID Forge, as an *alternative read-only view* of the patch that toggles with the existing list editor.
- This session went full cycle: **brainstorm → design spec → implementation plan → subagent-driven execution → reviews → branch pushed**. Milestone 1 (foundation + read-only graph) is **done and on the branch**; Milestones 2 (editing) and 3 (layout persistence) are designed-but-not-built.
- The app **compiles and runs** (Qt 6.11 via Homebrew). The graph renders correctly: circuits as nodes with compound primary/scale/offset pins, hardware as source/sink nodes split by direction and per-controller, cable + register wires, section frames, laid out left-to-right. Verified by **13 unit tests** + a rendered screenshot.
- Tests live in a **gitignored throwaway harness** (`droidforge/tests/`, `droidforge/build-tests/`) — by deliberate choice the shipped repo stays test-free. The harness files exist on local disk but are **not committed**; a fresh clone won't have them.
- Most visible rough edge deferred to next time: auto-layout uses a **fixed row pitch**, so tall nodes in the same column **overlap and hide pins**.
- New global rule added to `~/.claude/CLAUDE.md`: never autonomously open a PR on a repo not owned by LuggLD (upstream `Zarkuun/droidforge` is off-limits for self-service PRs).

## Changes landed

All on `feature/node-graph-editor`. The graph code lives in `droidforge/graphview/`.

| Ref | Title | Notes |
|---|---|---|
| 5c0e027 | Add node graph editor design spec | `docs/superpowers/specs/2026-06-08-node-graph-editor-design.md` — the approved design contract. |
| 1df8951 | Add Milestone 1 implementation plan | `docs/superpowers/plans/2026-06-08-node-graph-editor-m1-foundation.md`. **Its Task 1 reference code had a latent bug** (see Gotchas / Decision 3). |
| efb3173 | Add graph model types and gitignore test harness | `graphmodeltypes.h/.cpp` (committed) + gitignored `tests/`. |
| 0d628c6 | Derive circuit nodes and compound pins from a Patch | `GraphModel::describe` — circuit nodes, compound 3-pin inputs, text pins, output pins. |
| 884f920 | Derive hardware nodes split by direction and controller | master in/out + per-controller controls/LEDs; all pins shown with `used` flag. |
| acac0d6 | Derive cable and register wires between pins | producer→consumer; cable named-nets + register direct connections. |
| 0301a45 | Fix off-by-one in input atom indexing (atomAt is 1-based) | **Correctness fix** spanning Task 1 + Task 3 logic; added `inputAtomSuffix` shared helper + a mapping-lock test. |
| 14cbeb0 | Derive section frames grouping circuit nodes | frames = sections; group circuit node ids by section. |
| 289d5a4 | Add layered left-to-right auto-layout | `GraphLayout::layout` — longest-path columns; sources left, sinks right. |
| 2f9048d | Add GraphView scaffolding and item stubs, wire into build | `GraphView` + item stubs; added the 14 graphview sources to `droidforge/CMakeLists.txt`. |
| b97b80a | Render node, pin, wire and frame graphics | real painting for NodeItem/WireItem/SectionFrameItem. |
| 8aae717 | Add list/graph view toggle bound to the live patch | `mainwindow.{h,cpp}`: GraphView member + QStackedWidget + **View ▸ Node Graph** toggle + `UpdateHub::patchModified`→`rebuildGraphics`. |
| 77522f5 | Fix node boundingRect coverage and skip unresolved wires | final-review fixes: pad boundingRect for edge connectors (stale-pixel artifacts); skip wires whose endpoints don't resolve. |

## Decisions made (and why)

### 1. Alternative *view*, not a replacement; same `Patch` model, lossless round-trip
The node editor edits the same in-memory `Patch` the list editor uses (one model → round-trip is automatic, no sync layer). Chosen over a separate representation or a replacement editor. Rejected a third-party Qt node library (impedance mismatch with compound pins / section frames / per-controller hardware / direct model binding + a dependency + Qt 6.11 risk) and a standalone window (loses shared undo/selection/toolbar). Built on `QGraphicsView`/`QGraphicsScene`, matching `patchsectionmanager`/`patchsectionview`.

### 2. Whole-patch canvas; sections are spatial frames; hardware are first-class nodes
One canvas shows the whole patch (not page-per-canvas). `PatchSection`s become draggable rectangular frames; a node's containing frame will determine its section (drag-in/out is M2). Hardware (registers) render as **nodes split by direction** — sources (`I N P B E S`) on the left, sinks (`O G L R`) on the right — and **grouped per physical unit** (master in / master out / per-controller controls / per-controller LEDs), to preserve left-to-right signal flow. Hardware nodes **always show all available pins** (used ones highlighted) for at-a-glance "what's free", unlike circuit nodes which show only added jacks.

### 3. Compound inputs = 3 independent pins (primary/scale/offset); text is its own port type
A DROID input is `primary × scale + offset`, and **all three slots accept wires** (not just numbers). Each renders as an independently-wireable pin with a constant text field when unwired; a node-level collapse (M2) hides default/unconnected pins. `text` jacks (DB8E display strings, blue-7) are a distinct **port kind** (square pin, no scale/offset); text↔signal cross-wiring is disallowed. All numeric jack types collapse to one interchangeable "signal" kind (matches DROID's permissive voltage model).

### 4. Cables only for circuit→circuit; hardware connections store the register directly
A wire is producer→consumer. Circuit-out → circuit-in stores an `AtomCable` named net (auto-minted hidden name; rename via the existing action — M2). Hardware connections store the `AtomRegister` directly on the jack (no cable). hardware→hardware is not expressible, so disallowed.

### 5. Auto-layout default; opt-in `.ini`-comment persistence later
Node positions aren't in the DROID format. Auto-layout (layered left→right) is the default and the always-available fallback; an opt-in setting to persist positions as `#`-comment metadata (same mechanism as title/LABELS/description) is **Milestone 3**.

### 6. Throwaway, gitignored test harness (not committed)
Per operator choice: TDD the pure logic (derivation, layout) in a scratch Qt Test build that stays out of the shipped repo; GUI is run/screenshot-verified. Keeps the codebase's existing test-free convention while still pinning the logic during development.

### 7. Subagent-driven execution with per-task spec + quality review
Operator preferred subagent-driven development. Each task: fresh implementer subagent → independent spec-compliance review → code-quality review → fix loop. Pure-logic tasks were code-complete & TDD'd; GUI tasks were build/screenshot-verified.

## Key gotchas surfaced this session

- **`JackAssignmentInput::atomAt(column)` is 1-based**: it returns `atoms[column-1]`, valid columns 1/2/3, `atomAt(0)` is null. The atoms are `A*B+C` → `atoms[0]`=primary, `atoms[1]`=scale, `atoms[2]`=offset. So **primary=atomAt(1), scale=atomAt(2), offset=atomAt(3)**. (Output `JackAssignmentOutput::atomAt(1)` is the single atom.) The plan's reference code wrongly used `atomAt(0/1/2)`; fixed in 0301a45. `graphmodel.cpp` now centralizes the column→suffix map in `inputAtomSuffix(col)` (1→`p`,2→`s`,3→`o`) used by **both** pin-building and wire-building so they can't drift.
- **`AtomCable::getCable()` strips the leading underscore**: `_LFO` in the `.ini` is stored internally as `LFO` (the `_` is a display convention from `toString()`). Wire matching uses `getCable()` on both ends so it's consistent, but any display/rename code must account for it.
- **The bundled `droidfirmware.json` IS `blue-7`** and includes the `text` jack type. `numGlobalRegisters(type)` returns fixed counts (I:8, N:8, O:8, G:12) independent of master type; `numControllerRegisters(controllerName, type)` is keyed by the firmware controller name (e.g. `p2b8`).
- **Parser API**: `Patch *p = new Patch(); PatchParser parser; parser.parseString(src, p);` — void, fills a pre-allocated patch (NOT `return parseString(src)`).
- **Section-header syntax** in the `.ini`: a `#`-comment separator line matching `^----*$` (≥2 dashes), with the title as the first comment line between two separator lines (`PatchSection::toString` uses 49 dashes).
- **Throwaway harness excludes `modules/*.cpp`** (they drag in `mainwindow.h`→GUI) and stubs `the_colorscheme` + `ModuleBuilder::allRegistersOf/controllerExists/allControllers` in `tests/test_stubs.cpp`. Consequence: in tests, set up controllers via `patch->addController("p2b8")` **directly** (the parser's controller path routes through stubbed ModuleBuilder). `registerUsed`/`numControllers` work fine (pure model). The `graphshot` visual-render helper provides its own inline `PatchView` defs (patchview.cpp also pulls GUI).
- **`QStackedWidget` ownership of value-member widgets is safe**: `patchSectionView` and `graphView` are MainWindow value members added to a heap `QStackedWidget`. No double-delete — members destruct first and QWidget's dtor unparents. This mirrors the pre-existing pattern (patchSectionView was already added to a splitter).
- **Graph items don't use `the_colorscheme`** — all colors are hand-picked literals, so painting is null-safe even where the global colorscheme isn't set.
- **`QGraphicsItem::boundingRect` must cover painted connectors**: pins are drawn centered on the node edge (x=0 / x=NODE_WIDTH) and overhang the body rect; boundingRect must be padded or you get stale-pixel trails on scroll/zoom (fixed in 77522f5; `SectionFrameItem` already padded for its pen).

## Process / discipline notes for the next agent

- **Command hygiene is strictly enforced by the operator** (repeated corrections this session). One single command per Bash call. NO `cd foo && …`, NO `;`/`{…}` compound blocks, NO inline env-var prefixes (`QT_QPA_PLATFORM=offscreen cmd` — use the CLI arg `-platform offscreen` instead), NO inline Python (`python3 -c`), NO `find … -exec`. Reason: anything that can't be matched to an allowlist prompts every time. Use absolute paths (`git -C <abs>`, `cmake -S/-B <abs>`), the Read tool for data files, `rg` for search. **Subagents reflexively reach for inline python to inspect JSON** — when dispatching, forbid it loudly AND give the alternative (Read/`rg`). This is captured in the `bash-command-style` memory.
- **Reviewers that check code against the plan will miss bugs the plan itself contains.** The `atomAt` off-by-one lived in the plan's reference code; the Task 1 spec + quality reviewers both passed it because it matched the plan. It was only caught when the Task 3 implementer ran against *real parsed data*. Lesson: for derivation logic, add a test that asserts against ground-truth semantics (the `inputAtomMappingLock` test now does this), don't just verify "matches the plan".
- **Never autonomously PR a repo you don't own** (now a hard rule in `~/.claude/CLAUDE.md`). `origin` = LuggLD/droidforge (fork, OK to push/PR). `upstream` = Zarkuun/droidforge (the eventual destination — operator opens that PR, not the agent). For this session: pushed the branch, **no PR**.
- **Subagent-driven loop worked well** but is token-heavy with full spec+quality review per task. For trivial tasks (e.g. section frames, ~11 lines) a single combined spec+quality review is proportional. Pure-logic tasks benefit most from the full TDD + double review; GUI tasks need a rendered screenshot to verify (the `graphshot` helper renders a GraphView to PNG offscreen — invaluable since a subagent can't click the View menu).

## Open follow-ups

| Topic | Ref | Status |
|---|---|---|
| Auto-layout fixed-row-pitch → column node overlap hides pins | layout polish | **Top next item.** Pure-logic fix: estimate per-node height (from pin/row count) and stack within a column by cumulative height + gap; add a test that two tall same-column nodes don't overlap. |
| `rebuildGraphics` fires on every `patchModified` even when graph hidden | perf | Deferred to M2. Gate on visibility (dirty flag + rebuild on show / on toggle-to-graph). |
| Milestone 2 — editing | needs own spec→plan | wiring gestures (mint cables / set registers), add/remove pins via the existing action registry (no add-button), value/text field editing, drag nodes between section frames → section reassignment, collapse/expand, `PatchProblem` badges, connect-time type safety, shared undo. |
| Milestone 3 — layout persistence | needs own plan | opt-in setting to persist node positions/frame rects as `#`-comment metadata in the `.ini`. |
| Deferred-past-V1 niceties | spec §Non-Goals | keyboard shortcuts for graph actions; controller-node → rackview click-through; "promote chip to node". |
| Eventual upstream PR | — | When the whole feature is polished, **the operator** opens the PR against `Zarkuun/droidforge`. The agent must not. |

## Memories that would help next session

| Proposed file | What |
|---|---|
| `bash-command-style.md` | Already written. Command-hygiene rules + the "give subagents the alternative to inline python" note. |
| `plan-execution-preference.md` | Already written. Operator prefers subagent-driven execution of plans. |
| (global) `~/.claude/CLAUDE.md` | Already written. Hard rule: never autonomously PR a repo not owned by LuggLD. |

## What works now

- `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build` produces `build/DROID Forge.app`, which launches (Qt 6.11) and opens in the **list editor** by default — no crash from the new stacked-widget hosting.
- A new **View ▸ Node Graph** menu toggle switches the right editor pane between the list view and the node graph; the graph rebuilds live on `UpdateHub::patchModified`.
- The node graph renders faithfully: blue source nodes (Master in, per-controller controls) on the left, green circuit nodes inside translucent section frames in the middle (compound `hz` shows primary wired + `hz = 0.5` scale + `hz = 0.1` offset rows), orange sink nodes (Master out, LEDs) on the right; solid orange cable wires, dashed green register wires; used hardware pins highlighted, free ones dimmed. (Caveat: tall nodes in a column overlap — see follow-ups.)
- The pure derivation + layout pass **13/13 tests** in the throwaway harness.
- Branch `feature/node-graph-editor` is pushed to `origin` and tracking; nothing committed to `main`.

## Verification commands handy for next session

```bash
# Build + run the app
cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build"
open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"

# Pure-logic tests (throwaway harness; gitignored — may need re-create on a fresh clone)
cmake -S "/Users/jan.kaluza/Projects/droidforge/droidforge/tests" -B "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests" -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests"
"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen

# Render the graph to a PNG offscreen (graphshot helper, also in the gitignored harness)
"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/graphshot" -platform offscreen   # writes /tmp/graphshot.png

# Branch state
git -C /Users/jan.kaluza/Projects/droidforge log --oneline -13
git -C /Users/jan.kaluza/Projects/droidforge remote -v   # origin=LuggLD (push OK), upstream=Zarkuun (no self-PR)
```

## Specs + plan artifacts

| Doc | Path |
|---|---|
| Design spec | `docs/superpowers/specs/2026-06-08-node-graph-editor-design.md` |
| M1 implementation plan | `docs/superpowers/plans/2026-06-08-node-graph-editor-m1-foundation.md` (note: Task 1 reference code's `atomAt(0/1/2)` is the bug fixed in 0301a45 — trust the committed `graphmodel.cpp`, not the plan snippet) |
