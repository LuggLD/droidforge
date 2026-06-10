# 2026-06-11 — node-graph rack-accurate hardware milestone (+ UX polish)

> Continues `2026-06-10T0105-node-graph-wiring-milestone.md`. Covers commits `c831fab`…`2f96636` (21 commits).

## TL;DR

- Shipped the **rack-accurate hardware milestone** (both "next session" backlog items at once): hardware nodes are now **per visible rack module** — Master/Master18, one in/out node pair per G8 expander, X7 — with **bidirectional G8 jacks** (read pin on the in-node, write pin on the out-node) and the master's R1–R16/X1 pins. Spec `c831fab`, plan `61a2190`, then 8 subagent-driven tasks with two-stage reviews.
- **Graph commits now broadcast through the `UpdateHub`** (`e9f442e`): graph edits auto-show/hide X7/G8s in the rack, the list editor refreshes, and the View-menu module toggles rebuild the graph. The headline bug from the prior session is fixed.
- New **operator hard rule, now in memory: the graph view must be purely additive** — no modifications to preexisting droidforge code. The rack's visibility formula is *mirrored* into `graphview/rackmodules.{h,cpp}` (source-of-truth comment, drift risk accepted) instead of refactored out of `rackview.cpp`. **Zero preexisting files changed this milestone** (the `mainwindow.cpp`/`CMakeLists.txt` hunks extend blocks this branch added in M1).
- Reviews caught three real bugs pre-merge: the rack-expression divergence (`==18` vs `!=16`, `31ef176`), an `isValidDrop`/`connectPins` disagreement (valid cursor, silent no-op — `eb3ff27`), and a mid-drag-rebuild use-after-free window (`8d10fba`).
- **"White background" bug after the milestone was NOT a regression**: the operator's Mac auto-switches appearance by time of day and all prior graph sessions ran at night. The graph never owned its background (inherited `QPalette::Base`); pinned it dark (`b173472`).
- Post-milestone operator feedback: shipped pin-hover cursor, delta-scaled wheel zoom + pinch + anchor-under-cursor, ⌘G list/graph toggle (`ca11432`, `178f9f2`); four bigger wishes recorded as backlog (`2f96636`).
- **Command-hygiene correction mid-session** (operator interrupt): `git -C <path>` prefixes and `sed` can't be auto-allowed — plain `git <subcommand>` from the repo-root cwd, file edits only via Read/Edit/Write. `bash-command-style.md` memory updated; every subagent prompt now carries these rules verbatim.
- **84 harness tests green** (31 GraphModel + 37 GraphEdits + 16 RackModules; was 49). App + graphshot build clean. Operator ran the 10-item interactive checklist: all pass.

## Changes landed

| Ref | Title | Notes |
|---|---|---|
| `c831fab` | Add rack-accurate hardware nodes + rack reconciliation design spec | Approved after AskUserQuestion rounds: per-module nodes, mirror-rack visibility, R/X pins included, in+out node pair per G8. |
| `61a2190` | Add implementation plan | 8 TDD tasks, full code, verified facts section; command-hygiene block per task. |
| `4391851` | Scaffold graphview/rackmodules unit | + upgraded the gitignored harness `ModuleBuilder` stub from no-op to faithful per-module inventories — this is what made the whole milestone harness-testable. |
| `0644fc7` | visibleRackModules mirrors the rack view's module visibility | Pure function; settings passed in by callers. |
| `86e0f57` | Spec: hub connection moves to the M1 mainwindow block | Mid-flight spec fix — `graphview.cpp` can't include `mainwindow.h` (graphshot harness). Almost went uncommitted; a spec reviewer flagged the dirty tree. |
| `31ef176` | Mirror the rack's master-type expression exactly | Review catch: g8-offset must key on `typeOfMaster() != 16` like `rackview.cpp:412`, not `== 18` (master36 hint exists in patch.cpp). |
| `9c51f9a` + `35f9cec` | Classify G8 expander jacks as bidirectional (+ doc fix) | Graph-side third classification; review fixed a phantom "g8 1..4" upper bound in the doc comment. |
| `e1d3b11` + `73b1d29` | registersOfModule (+ comment clarifications) | Gate string round-trip (master18 emits bare G1–G4!); g8 RGB offsets applied from the spec, not ModuleBuilder. |
| `a4dceaa` | Build per-module rack-accurate hardware nodes | The central change: `addHardwareNodes` rewritten around `visibleRackModules`/`registersOfModule`; two-node `appendRegisterPins`. |
| `33c85e7` | GraphEdits: bidirectional gate jacks wire both ways | 3-line change: `parsePin` + `getConnectedSource` learn the new classification. |
| `eb3ff27` | isValidDrop: hardware sinks accept only circuit outputs | Review catch: hw-read→hw-write drag showed a valid cursor but `connectPins` refused (pre-existed for O registers; gates made it prominent). |
| `e9f442e` | Graph commits broadcast through the UpdateHub | `commitEdit` emits `patchModified`; connects live in our M1 `mainwindow.cpp` block; rebuilds read `show_g8s`/`show_x7_on_demand` from QSettings. |
| `8d10fba` | Tear down an active drag before rebuilding the scene | Review catch: hub-driven rebuilds can now arrive mid-drag; `scene->clear()` would delete the live rubber item. |
| `8cf425e` + `43a213e` | Backlog closure + 3 new review-finding items | Closed the two shipped items; recorded G8-RGB-only visibility blindspot, copy/move-onto-hw-write-pin, empty disconnect commits. |
| `b173472` | Give the node graph its own dark background | Root-caused via `defaults read` (light mode + auto-switch); systematic-debugging, not a code regression. |
| `ca11432` | Gentler delta-scaled zoom, pinch support, pin hover cursor | Wheel zoom now scales by `angleDelta` (~7%/notch; old code did fixed 15% **per event**, hence wild trackpad zoom); native pinch gesture; `AnchorUnderMouse`. |
| `178f9f2` | Ctrl+G toggles between list editor and node graph | Interim for the integration design question. |
| `2f96636` | Backlog: four items from operator feedback | Register-UX design pass; assign-registers-by-typing/dialog; TiXL-style create-on-wire-drop; list/graph integration. |

## Issues filed

(Local backlog, not GitHub — see `docs/backlog/README.md`.)

| Ref | Title | Why filed |
|---|---|---|
| `node-graph-g8-visibility-rgb-only.md` | G8 used only via RGB LEDs stays hidden; wire drops | Review finding: `highestGatePrefix()` is gates-only; `output = R17` alone yields a dangling wire. Pinned by test `rgbOnlyG8ReferenceDropsWire`. |
| `node-graph-copy-move-onto-hw-write-pin.md` | Copy/Move onto a hw write pin: cursor valid, semantics off | Final-review finding; can strand cable readers. Pre-existing, surface doubled by gate write pins. |
| `node-graph-disconnect-empty-commit.md` | Alt-click disconnect on unconnected pin commits empty undo step | Pre-existing; now also triggers a full hub refresh per no-op. |
| `node-graph-register-interaction-ux.md` | Make register interactions feel more wire-like | Operator: "we need to make interacting with them more intuitive… I don't know what that means yet." Design pass. |
| `node-graph-assign-registers-by-typing-or-dialog.md` | Assign registers by typing / standard parameter dialog | Operator proposal; also the answer to "create G8/X7 references from the graph" (chicken-and-egg with hidden modules). |
| `node-graph-create-on-wire-drop.md` | Create nodes/pins by dropping wires (TiXL-style) | Operator wish; research Tooll3 first. The wiring spec had explicitly deferred both halves. |
| `node-graph-list-view-integration.md` | List + graph side-by-side / cursor sync | Operator wish; ⌘G shipped as interim. |

## Decisions made (and why)

### 1. Purely additive — mirror upstream formulas instead of refactoring them out
Operator (verbatim intent): "our graph view should — as much as possible at least — be purely an addition building on top of existing code… minimize the chance of us breaking or changing anything in the existing droidforge app." Consequences: `visibleRackModules` duplicates `RackView::refreshScene`'s formula with a MIRRORS comment (drift risk accepted and documented); `registerIsBidirectional` lives in `graphview/`, not on `Patch`; hub/action connects go in the M1 `mainwindow.cpp` block we already own. Saved to memory (`graph-view-purely-additive.md`).

### 2. Per-module nodes, mirror-rack visibility, R/X pins now
All operator choices via AskUserQuestion: one source/sink node pair per installed module (over keeping aggregate master nodes or mixed in/out nodes); the graph shows exactly what the rack shows (same settings — graph and rack can never disagree); RGB/X registers included now rather than deferred. Side effect of mirroring: a gateless patch shows **no G8 node** by default (View menu forces one) — intentional, matches the rack.

### 3. Bidirectional G8 jacks = read pin on the in-node, write pin on the out-node
Each expander jack: `hw.<reg>.read` (Out) on "G8 #n in", `hw.<reg>` (In) on "G8 #n out" — existing pin-id grammar, no collisions, no GraphEdits grammar changes. The operator asked whether simultaneous in+out use needs prevention: **no** — the manual says direction follows patch usage (write → jack becomes output, reads become read-back, same as O registers), and the Forge's `updateRegisterProblems()` deliberately doesn't flag it. X7 gates and MASTER18's built-in `g8==1` bank stay output-only.

### 4. Hub broadcast instead of direct rebuild; settings read at rebuild time
`commitEdit` emits `patchModified` → `UpdateHub::modifyPatch` — the exact path list-editor edits take, so the rack reconciles and our own rebuild returns via the pre-existing hub→graph connection (one rebuild, no recursion — verified). `rebuildGraphics` reads `show_g8s`/`show_x7_on_demand` from QSettings instead of `ACTION` state: keeps `graphview.cpp` free of `mainwindow.h`/`editoractions.h` (the graphshot harness compiles it), and the values are written by `RackView`'s earlier-connected handlers before our slots run.

### 5. Review findings on *pre-existing* semantics get recorded, not fixed
Copy/Move onto a hw write pin (can strand cable readers / silent no-op) and the empty "disconnect all" commit are wiring-milestone semantics, out of this spec's scope — changing gestures unilaterally would bypass the operator. Both backlogged with concrete fix sketches. The line: review fixes within the milestone's own invariants ship; semantics changes get filed.

### 6. The dark canvas is pinned, hardcoded, not colorscheme-driven
Root cause of "white background": graph items use hand-picked dark-first QColors but the canvas inherited `QPalette::Base` — white in macOS light mode, and `AppleInterfaceStyleSwitchesAutomatically=1` on the operator's machine meant night sessions saw dark, the first daytime session saw white. Fix: `scene->setBackgroundBrush(QColor(30,30,30))` in the constructor. Hardcoded because the graphshot harness has no colorscheme (`the_colorscheme` is nullptr there) — same reason the node colors are hardcoded.

### 7. Zoom scales with the wheel delta; anchor under cursor was added unrequested but flagged
Old wheel code applied a fixed 1.15× per event regardless of `angleDelta` — trackpad scrolling (many small events) zoomed wildly. New: `2^(dy/1200)` (~7% per 120-notch, proportionally less per trackpad tick) + native pinch via `QNativeGestureEvent`. `AnchorUnderMouse` was added beyond the literal ask as "the other half of zoom feel" and explicitly flagged to the operator for veto; no objection.

## Key gotchas surfaced this session

- **macOS auto-appearance is a hidden test variable.** `defaults read -g AppleInterfaceStyleSwitchesAutomatically` → 1 on this machine. Anything palette-dependent looks different between night and day sessions. The graph now owns its background, but watch for this class of "regression" elsewhere.
- **`ModuleBuilder::allRegistersOf` has two traps:** `ModuleMaster18` emits gates as **bare `G1`–`G4` (`g8=0`)** — the same non-canonical form behind the original vanishing-wire bug (round-trip all gates through `AtomRegister(reg.toString())`); and **g8 RGB offsets are rack-position data** (`DATA_INDEX_G8_RGB_OFFSET`, set only by `RackView::addModule`) — `allRegistersOf` yields R1–R8, the spec's `rgbOffset` must be applied by the caller.
- **`graphview.cpp` compiles in the gitignored graphshot harness** — it must never include `mainwindow.h`/`editoractions.h` (drags `patchoperator.h → macmidihost.h`, absent there). App-side wiring goes in the M1 `mainwindow.cpp` block; settings come from `QSettings` directly.
- **Settings/connection ordering holds by construction:** `RackView` connects the show-G8/X7 actions in its constructor (MainWindow member-init), before the MainWindow-body graph connects — so `show_g8s`/`show_x7_on_demand` are written before the graph rebuild reads them. Don't reorder members or move the connects.
- **`isValidDrop` and the mutation functions must agree** — a "valid" cursor over a drop `connectPins` refuses reads as a broken app. New guard covers Connect; Copy/Move still has the gap (backlogged).
- **Hub-driven rebuilds can arrive mid-gesture** (other views, colorscheme, View menu). `rebuildGraphics` now tears down an active drag first; anything else holding `QGraphicsItem` pointers across event-loop boundaries must copy values out (the context-menu path copies `GraphWire` by value for exactly this reason).
- **`registerIsBidirectional` assumes canonicalized registers** (bare `G1`–`G8` never reach it) — documented in the header; the canonicalization lives in `registersOfModule`.

## Process / discipline notes for the next agent

- **Command hygiene v2 (operator interrupt mid-session):** "your agents have been using commands that can't be auto-approved. The whole git -C … prefix is hard to auto-allow for example, as is using sed." Corrected rules — plain `git <subcommand>` from the repo-root cwd (matches the `Bash(git *)` allowlist entry the operator added), NEVER `git -C`; never sed/awk/cat/head for file work — Read/Edit/Write tools only; `grep`/`rg` for search. `bash-command-style.md` updated. **Subagent prompts must carry these rules verbatim** — they don't inherit memory.
- **Final text must carry the deliverable.** The operator said "you haven't shown me a design" — the design had been written as text *between* tool calls, which isn't reliably displayed. Anything the user must read goes in the final message of the turn (or a committed file).
- **Don't forget mid-flight doc amendments:** the spec edit for the hub-connection change sat uncommitted until a *spec reviewer* noticed the dirty tree. After editing a committed doc, commit it in the same breath.
- **Reviewer-prescribed one-liners can be applied directly** (comment fixes, the `endDrag` guard) per the prior session's precedent; anything touching behavior went back through a fix subagent with TDD (`31ef176`, `eb3ff27`).
- **Verify before fixing held up well:** the white-background report pattern-matched to "today's styling commits" but the root cause was environmental (auto-appearance). Phase-1 evidence (`grep setBackgroundBrush`, `defaults read`) beat the obvious-suspect diff.

## Workflow lessons

- Subagent-driven execution (8 tasks × implementer + spec review + quality review): the two-stage reviews caught three genuine bugs and several doc bugs; none of the implementers' self-reviews caught them. Model mix worked: sonnet for fully-specified mechanical tasks, default (most capable) for the central rewrite, integration, and all whole-feature reviews.
- The plan's "Verified facts you must not re-derive" section and per-task COMMAND HYGIENE block measurably reduced subagent flailing — keep both in future plans.
- Continuing a prior agent via `SendMessage` wasn't possible (a `ToolSearch` for it returned no matching tool, despite agent-result footers advertising it) — review fixes went to fresh fix-subagents with tight prompts instead; worked fine.

## Open follow-ups

| Topic | Ref | Status |
|---|---|---|
| "Pins get values without dragging" milestone (typing/dialog + inline constants) | `backlog/node-graph-assign-registers-by-typing-or-dialog.md` + M2 inline constants | recommended next; operator undecided |
| Register interaction UX design pass | `backlog/node-graph-register-interaction-ux.md` | needs brainstorm session |
| TiXL create-on-wire-drop research | `backlog/node-graph-create-on-wire-drop.md` | research first |
| List/graph integration | `backlog/node-graph-list-view-integration.md` | design; ⌘G interim shipped |
| Copy/Move onto hw write pin guard | `backlog/node-graph-copy-move-onto-hw-write-pin.md` | small fix + semantics decision |
| Remaining M2 sub-milestones: pin add/remove, node drag/collapse, section drag-in/out, problem badges | — | not started |
| M3: layout persistence | — | not started; new `hw.g8.<n>.*`/`hw.x7.out` ids are now the ones to persist |

## Memories that would help next session

| Proposed file | What |
|---|---|
| (new, written) `graph-view-purely-additive.md` | The operator's hard rule + how to apply it. |
| (updated) `bash-command-style.md` | Plain `git` from repo root (no `-C`), no sed/awk/cat/head — with the why. |
| (existing) `plan-execution-preference.md` | Subagent-driven execution; reinforced again. No change. |

## What works now

- **Rack-accurate hardware nodes in the app:** master16 shows I/N/O/R/X across Master in/out; switching to MASTER18 live-updates to I1–I2 + G1.1–G1.4 (no N, no R); G8 expanders appear as "G8 #n in/out" node pairs exactly when the rack shows them; X7 has its own node (G9–G12 + R49–R56).
- **Bidirectional gates:** a G8 jack can be wired as a clock/gate *input* (read pin) and driven as an *output* (write pin) — both at once renders both wires (read-back).
- **Rack reconciliation from graph edits:** wiring/unwiring X7 or G8 registers in the graph adds/removes those modules in the rack (with show-on-demand), and View-menu module toggles rebuild the graph. One undo step per gesture, all views in sync. Operator verified the full 10-item checklist.
- **UX:** dark canvas regardless of OS appearance; pin hover shows a pointing hand; wheel zoom ~7%/notch and trackpad-proportional; pinch zooms; zoom anchors under the cursor; ⌘G toggles list/graph.
- **Tests:** 84 green — 31 GraphModel + 37 GraphEdits + 16 RackModules (`forgetests -platform offscreen`). App and graphshot build clean. Branch `feature/node-graph-editor` at `2f96636`, tree clean, not pushed this session. No PR (standing rule).

## Verification commands handy for next session

```bash
# Build + run the app
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build
open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"

# Tests (gitignored harness)
cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen

# Visual smoke render (needs /Volumes/home mounted for the sample patch)
/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/graphshot -platform offscreen

# Branch state since the prior handoff
git log --oneline afe927f..HEAD
```

## Specs + plan artifacts

| Doc | Path |
|---|---|
| Rack-accurate hardware design spec | `docs/superpowers/specs/2026-06-10-node-graph-rack-accurate-hardware-design.md` |
| Implementation plan | `docs/superpowers/plans/2026-06-10-node-graph-rack-accurate-hardware.md` |
| Backlog index | `docs/backlog/README.md` |
