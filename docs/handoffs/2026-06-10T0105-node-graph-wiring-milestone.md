# 2026-06-10 — node-graph wiring milestone (interactive connect/disconnect)

> Continues `2026-06-09T0442-node-graph-hw-registers-and-section-layout.md`. Covers work after that handoff's last commit `8cfbc69`: the first slice of M2 (wiring), commits `47c1759`…`57c4987`.

## TL;DR

- Built the **first M2 slice — interactive wiring**: drag a pin to connect, with a full source/sink gesture matrix (plain / shift / ctrl / alt+click / right-click), wire selection + Delete, context-menu disconnect, rubber-band preview, type+direction safety. Spec `47c1759`, plan `0278226`, then 8 subagent-driven tasks (`e361da1`…`d69d0e0`).
- Architecture: a **pure, GUI-free `GraphEdits` layer** (parse pin-ids → classify → connect/disconnect/copy/move/rehome) TDD'd in the gitignored harness; `GraphView` is a thin gesture translator (app-verified). **29 GraphEdits + 20 GraphModel tests green.**
- **Refined the spec mid-flight:** `GraphEdits` does *not* commit — `GraphView::commitEdit` commits once per gesture — so the logic layer stays edit-engine-free and unit-testable on a plain `Patch`.
- Interactive testing surfaced 5 issues. Fixed **#1** gate wires silently dropped (canonical-id bug, `2c301ec`), **#3** pick-up preview anchored at wrong end (`4f6e6fd`) + curved like real wires (`64a9548`), **#4** rehome-onto-occupied now merges nets (`1915b39`). **#2** (Ctrl→menu) is macOS behavior, not a bug. **#5** (output-register read-back) deferred to a design pass.
- Replaced the single `node-graph-followups.md` with a **`docs/backlog/` folder, one file per issue** + README index (`57c4987`).
- Tightened the local permission allowlist: removed broad `Bash(git *)`, added nondestructive repo-scoped git entries only; switched commit messages to two `-m` flags (no `$(...)` substitution) so they auto-allow.

## Changes landed

| Ref | Title | Notes |
|---|---|---|
| `47c1759` | Add node-graph wiring design spec | Source/sink gesture matrix; cable mint/reuse; explicit edge cases. Approved before planning. |
| `0278226` | Add wiring implementation plan | 8 tasks, full TDD code; COMMAND HYGIENE block; the "GraphEdits doesn't commit" refinement noted. |
| `e361da1` | GraphEdits scaffold | `parsePin`, `portKindOfPin`, `isValidDrop` + the `PinRef`/`DragMode` types. |
| `73e16fe` | `connectPins` | cable mint/reuse, register, read-back, out→hw-sink. |
| `e99b846` | queries + `disconnectWire`/`disconnectPin` | `getConnectedSinks`/`Source`; `findOutputHolding` dedup. |
| `785fc5b` | `copyWire`/`moveWire`/`rehomeWires` | compose the primitives. |
| `2e20173` | `NodeItem::pinAt` | pin hit-testing (harness-tested). |
| `c236c8f` | `WireItem` selectable + `shape()` | + selected highlight; `boundingRect` widened to cover the 10px hit stroke. |
| `3d2912b` | GraphView drag-to-connect | rubber-band, cursor feedback, `endDrag()` teardown, press re-entry guard, cached anchor. |
| `d69d0e0` | wire select/Delete + context menus + alt-click | mid-drag context-menu guard; routes through `commitEdit`. |
| `2cd50d3` | Sync spec to commit-boundary; track pin-id dedup | from the final whole-feature review. |
| `2c301ec` | **Fix gate pins to use canonical register ids** | the #1 bug — see Decision 4. |
| `1915b39` | **Allow re-home onto occupied source (merge)** | #4 — was forbidden; now merges nets. |
| `4f6e6fd` | **Anchor drag preview at the wire's fixed end(s)** | #3 — copy/move previews from the source; rehome previews one line per connected sink. |
| `64a9548` | **Curve the drag preview like committed wires** | exposed `WireItem::curve` as the shared cubic. |
| `cad527b`, `7bd1e52` | Record deferred wiring follow-ups | rack-accurate nodes; read-back edge; auto-copy idea; rack auto-show/hide. |
| `57c4987` | **Move follow-ups into `docs/backlog/`** | one md per issue + README; deletes `node-graph-followups.md`. |

## Decisions made (and why)

### 1. `GraphEdits` performs model mutations but never commits; `GraphView` commits once per gesture
The spec originally said each mutating function "ends in `commit()`". Moving the commit to the view keeps `GraphEdits` free of any `PatchEditEngine` dependency, so its tests construct a plain `Patch` exactly like `test_graphmodel.cpp` (no undo-engine setup). One gesture still equals one undo step. Spec updated to match (`2cd50d3`).

### 2. Sink/source unifying model for gestures
Every pin is a **source** (circuit output, hw read pin, hw source register — fan out) or a **sink** (circuit input, hw write pin — single wire). The dragged wire carries an *atom* (cable name or register). This makes the gesture matrix fall out cleanly and guarantees wires always read source→sink (a sink↔sink drag can never "snap to a shared source"). Operator drove this: plain-drag = new wire even from an already-connected source; shift/ctrl on a sink carry the *source net* to another sink.

### 3. macOS modifier mapping is platform behavior, not a bug (#2)
Ctrl+drag opened the delete menu because **macOS turns Control+click into a right-click**, and Qt maps the **⌘ key to `Qt::ControlModifier`** (Cmd/Ctrl swapped by default). Our code checks `Qt::ControlModifier` → so **⌘+drag** is the move/re-home gesture on Mac, Shift for copy. Left as-is per operator.

### 4. Gate hardware pins must use the canonical register form (#1, the headline bug)
Gate pins were built `AtomRegister(REGISTER_GATE, 0, 0, n)` → `g8=0` → `toString()` = `"G5"`, but the `AtomRegister(QString)` parser **normalizes a bare `G1`–`G8` to `g8=1`** (`"G1.5"`). So a gate wire's stored register stringified to `hw.G1.5` while the pin was `hw.G5`; the ids never matched and `rebuildGraphics` **silently drops any wire whose endpoint id isn't found**. Fix: build gate pins via the same string normalization, so `1..8 → G1.n` (first G8 expander) and `9.. → Gn` (X7) — which also happens to match the Forge's documented "show one G8 by default" behavior. Verified with a throwaway repro (`built="G5"` vs `parsed="G1.5"`) before fixing.

### 5. Re-home onto an occupied source merges; copy/move onto an occupied sink replaces (#4)
Operator wanted these to be allowed, not forbidden. `rehomeWires` already re-points every reader of the origin onto the target via `connectPins`, which *reuses* the target's existing cable — so simply removing the occupied-source rejection in `isValidDrop` yields a clean merge with no logic change. Copy/move onto an occupied sink already replaced (no occupancy check); locked in with a test.

### 6. Backlog folder over a single follow-ups doc
Operator asked for "a proper backlog folder, one md per issue." Migrated all items to `docs/backlog/*.md` + a README index table (severity/status), deleted `docs/superpowers/node-graph-followups.md`. Chose a local markdown backlog over GitHub issues for now — and flagged that I won't open issues on the upstream (Zarkuun) repo, only the operator's fork on explicit request.

## Key gotchas surfaced this session

- **`rebuildGraphics` silently drops wires whose endpoint pin-id isn't in the scene.** This is by design (M1) but means any pin-id/register-string inconsistency manifests as "wires just don't appear," not an error. The gate bug (#4 above) was exactly this. When adding hardware pins, the pin id (from `AtomRegister::toString()`) MUST match what `parsePin`/the stored atom produce.
- **`AtomRegister(QString)` is not a clean round-trip for gates:** bare `G1`–`G8` → `g8=1` (`"G1.n"`), `G9+` → `g8=0` (`"Gn"`) (`atomregister.cpp:46`). Build gate registers via the string form to stay canonical.
- **`registerIsOutputOnly` (`patch.cpp:688`) classifies master gates `G1`–`G8` as *inputs* (sources)** unless `typeOfMaster()==18 && g8==1`; `G9+` are outputs. So on a standard master you currently can't drive `G1`–`G8` as outputs in the graph — that's tied to the deferred rack-accurate-hardware work, not a wiring bug.
- **`Circuit::findJack(name)` non-const overload is `private`** (const is public). `GraphEdits::jackFor` iterates `jackAssignment(i)` matching `jackName()` to get a mutable `JackAssignment*`.
- **`QGraphicsView` is in `ScrollHandDrag`, which eats left-clicks** — so wires don't select by default; `mousePressEvent` explicitly selects a `WireItem` under the cursor when no pin was hit.
- **DROID master facts (from `droidforge/droid-manual.pdf`, verified):** standard MASTER = 8 CV in / 8 CV out, **no gate sockets**; MASTER18 = 8 out, **no CV in**, 2 gate inputs (I1/I2) + 4 gate outputs (G1–G4); G8 expander = 8 bidirectional gate jacks each (up to 4); X7 = 4 gate outputs + MIDI. The graph's master node is still hardcoded and doesn't disambiguate — deferred.

## Process / discipline notes for the next agent

- **Command hygiene is enforced to keep commands auto-allowable.** No `$(...)`/heredoc command substitution (it can NEVER be auto-allowed regardless of allowlist entries — the harness can't statically analyze it), no compound `&&`/`;`, no inline env-vars, one command per call, absolute paths. Operator pushed back twice on permissions: (1) they wanted `git commit` allowed — the real fix was dropping `$(...)` from commit messages (use two `-m` flags); (2) they did **not** want `Bash(git *)` (auto-allows destructive `push`/`reset`/`clean`) — consolidated to nondestructive repo-scoped entries (`status`/`log`/`diff`/`show`/`rev-parse`/`add`/`commit`). These live in `.claude/settings.local.json` (gitignored, not committed).
- **Verify bug reports before acting.** For #1 I traced `AtomRegister`/`graphmodel` and wrote a throwaway repro test confirming `"G5"` vs `"G1.5"` *before* changing code — the operator's "name collision" hunch was right but the mechanism was specific. (This repeats M1/earlier lessons.)
- **Subagent-driven execution worked well**: 8 tasks, each implementer + spec-review + code-quality-review. Reviewers caught real issues (drag-state leak on press re-entry; `boundingRect` not covering `shape()`; unguarded mid-drag context menu) that were fixed and re-reviewed before moving on. Minor reviewer nits (a `static_cast`, a DRY dedup) I applied directly via amend rather than re-dispatching — fine for one-liners, but the heavier fixes went back through review.
- **GUI tasks (7, 8) are "verified by building + running," not unit-tested** — Qt mouse/menu interaction isn't worth harness-testing. The operator does the interactive pass; give them a focused checklist.
- **No PR** (global rule + standing instruction): branch `feature/node-graph-editor` is committed locally; pushing to the operator's fork is fine, upstream PR is operator-driven.

## Open follow-ups

All now tracked as one file per issue under `docs/backlog/` (README index there):

| Topic | Ref | Status |
|---|---|---|
| Rack-accurate hardware nodes (MASTER/MASTER18/G8/X7) | `backlog/node-graph-rack-accurate-hardware-nodes.md` | next session (biggest) |
| Graph wiring doesn't trigger rack auto-show/hide of X7 & G8s | `backlog/node-graph-rack-auto-show-hide.md` | next session (adjacent) |
| Output-register read-back UX (#5) incl. auto-"copy"-circuit idea | `backlog/node-graph-output-register-readback-ux.md` | needs design pass |
| Compound input pin role indicators (`name`/`*`/`+`) | `backlog/node-graph-compound-input-indicators.md` | appearance/polish |
| Pin-id grammar duplicated (construct vs parse) | `backlog/node-graph-pin-id-grammar-duplication.md` | tech debt |
| Model N's internal link to its input | `backlog/node-graph-model-n-internal-link.md` | deferred |
| M2/M3 design-spec deltas | `backlog/node-graph-m2-m3-spec-deltas.md` | reconcile when planning M2/M3 |
| Remaining M2 sub-milestones: inline constant values, pin add/remove, node drag/collapse, section drag-in/out, problem badges | — | not started |
| M3: layout persistence | — | not started |

## Memories that would help next session

| Proposed file | What |
|---|---|
| (existing) `bash-command-style.md` | Command hygiene; reinforced. Could add: "no `$(...)` in commit messages — use two `-m` flags" so commits auto-allow. |
| (existing) `plan-execution-preference.md` | Subagent-driven execution; reinforced. No change. |

## What works now

- **Wiring works end-to-end in the app**: drag circuit-out→circuit-in (mints a cable), hw-source→input, circuit-out→hw-sink, read-pin→input. Gate pins (`G1.1`–`G1.8`, `G9`–`G12`) now connect — previously their wires vanished.
- **Gesture matrix**: plain-drag connect/fan-out; ⌘-drag move (sink) / re-home+merge (source); Shift-drag copy (sink); Alt+click disconnect-all; right-click pin/wire menus; click wire + Delete. Direction + signal/text type safety enforced; forbidden cursor over invalid targets and empty space.
- **Pick-up preview** anchors at the wire's far end and is **curved** like real wires; re-home shows one preview line per connected reader.
- Every gesture is **one undo step** (Cmd-Z) via `GraphView::commitEdit`.
- **Tests:** 20 GraphModel + 29 GraphEdits pass (gitignored harness). App + harness build clean (one pre-existing unrelated `colorscheme.cpp` warning).
- Branch `feature/node-graph-editor` committed through `57c4987`; tree clean (only untracked `.DS_Store`s). No PR.

## Verification commands handy for next session

```bash
# Build + run the app
cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build"
open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"

# Tests (gitignored harness; recreate on a fresh clone)
cmake -S /Users/jan.kaluza/Projects/droidforge/droidforge/tests -B /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests"
"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen

# Branch state since the prior handoff
git -C /Users/jan.kaluza/Projects/droidforge log --oneline 8cfbc69..HEAD
```

## Specs + plan artifacts

| Doc | Path |
|---|---|
| Wiring design spec | `docs/superpowers/specs/2026-06-09-node-graph-wiring-design.md` |
| Wiring implementation plan | `docs/superpowers/plans/2026-06-09-node-graph-wiring.md` |
| Backlog (one file per deferred issue) | `docs/backlog/` (+ `README.md` index) |
