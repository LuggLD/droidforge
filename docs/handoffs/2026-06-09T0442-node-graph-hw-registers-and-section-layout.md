# 2026-06-09 — node-graph hw-registers & section-layout (afternoon)

> Continues `2026-06-09T0208-node-graph-editor-m1-foundation.md`. Covers only work after that handoff's commit `d88feab`.

## TL;DR

- Ran the M1 graph on a real patch (`EnOscBuddy v2.ini`) and fixed three bugs found by eye: unbounded zoom, a layout coordinate explosion, and node overlap (`4750f8d`).
- Then designed + executed a focused milestone: **hardware-register correctness + section-aware auto-layout**. Spec `6dd3d95`, plan `0b03d42`, three subagent-driven tasks (`f8bfbb1`, `a4fcbfe`, `b199701`).
- Reclassified registers off the authoritative `Patch::registerIsOutputOnly()`: `N` is now an **output**, output nodes gained right-side **read pins**, and reading an output register back (`fold.input = O1`) now wires from that read pin.
- Replaced global data-flow layout with **per-section grid packing**, hardware pinned to the left/right edges, section bands stacked vertically so frames no longer overlap.
- Two fixes from live testing: dropped the hollow read pin from `N` registers (`8ac21c9`), and gave hardware read pins their own labels so they stay identifiable when they don't align row-for-row with write pins (`069ccfc`).
- All verified in the gitignored harness (**20/20 tests**) + offscreen `graphshot` renders. Branch `feature/node-graph-editor` pushed to the LuggLD fork, **no PR**. A polish follow-up was recorded (`27a5943`).

## Changes landed

| Ref | Title | Notes |
|---|---|---|
| `4750f8d` | Fix node-graph view: zoom clamp, runaway layout, node overlap | Three bugs from running EnOscBuddy. Zoom clamped to `[0.05, 4.0]`. Layout sentinel-explosion fixed (see gotchas). `NodeItem::heightFor()` added; layout stacks by real height. |
| `6dd3d95` | Add design spec for HW-register correctness & section-aware layout | `docs/superpowers/specs/2026-06-09-...-design.md`. Approved before planning. |
| `0b03d42` | Add implementation plan | `docs/superpowers/plans/2026-06-09-...md`. Full code in every TDD step. |
| `f8bfbb1` | Classify graph hw registers via `registerIsOutputOnly` | Task 1. Removed graph's own `isSourceRegisterType` (it wrongly listed `N` as a source). Output nodes get write pin + read pin (`hw.<reg>.read`). |
| `a4fcbfe` | Source output register read-backs from the read pin | Task 2. `addWires` input branch uses `hwReadPinId()` → reads of output registers source from `hw.<reg>.read`. |
| `b199701` | Section-aware grid auto-layout with hardware on the edges | Task 3. Rewrote `GraphLayout::layout()`. Retired `downstreamCircuitIsRightOfUpstream` test. |
| `8ac21c9` | Drop the read pin from normalize (N) registers | Live-testing fix. N is written, never read back directly → no read pin. |
| `27a5943` | Record node-graph polish follow-ups | `docs/superpowers/node-graph-followups.md`. |
| `069ccfc` | Label hardware read pins instead of relying on alignment | Live-testing fix. Read/write pins are independent top-aligned columns; once N lost its read pin they no longer matched, so read pins float up. Self-labeling fixes identifiability. |

## Decisions made (and why)

### 1. Register source/output split comes from `Patch::registerIsOutputOnly()`, not the graph's own table
The DROID manual: input registers (`I,P,B,E,S`) can never be outputs; output registers (`O,G,L,R,X`) are written *and* readable; `N` is an output that internally feeds its matching input. `patch.cpp:688` already encodes exactly this (incl. the `G` gate-number/master nuances). The graph's `isSourceRegisterType()` disagreed (listed `N` as a source) — that was the bug. Driving off the authoritative classifier is correct and avoids a duplicate, drifting table.

### 2. Output nodes carry both a write pin and a read pin; classification by *usage* not just type
Outputs are written (left/`In` pin) and can be read back (right/`Out` pin, id `hw.<reg>.read`). Reading an output register sources from the read pin; writing targets the write pin. This makes `fold.input = O1` resolve correctly instead of landing on the wrong pin.

### 3. `N` gets a write pin only (no read pin)
You write `N` to set a normalized value but read the corresponding *input* (`I`), never `N` directly. A read pin on `N` was a hollow connector that — sharing a row with the write pin and overlapping a horizontal wire — misread as an `I↔N` link. Operator chose to drop it (the simple "functionality first" fix) over modeling N's internal link (deferred).

### 4. Read pins are self-labeled
The node renderer lays out left (write) and right (read) pins as two independent top-aligned columns. They only line up row-for-row when both sides hold the same registers; once `N` lost its read pin, every `O`/`G` read pin floated up next to an `N` write row. Rather than re-engineer the renderer to align by register, label each read pin (operator's call) — cheaper and robust.

### 5. Auto-layout: per-section grid, hardware on the edges, no data-flow layering
Cross-section cabling is common and bidirectional, so flow direction isn't a reliable layout axis, and auto-layout is only the fallback (manual layout persistence is the real prize, deferred to M3). So: source hardware far-left column, output hardware far-right, each section packed into a compact `ceil(sqrt(n))`-column grid in patch order, section bands stacked vertically with a gap `> 2×FRAME_PAD` to guarantee disjoint frames.

### 6. Sectionless patches need no special handling
The parser already normalizes a header-less `.ini` into one empty-titled section ("Untitled section"), so `describe()` always sees ≥1 section. Confirmed by test, reused as-is.

## Key gotchas surfaced this session

- **Layout sentinel explosion (the headline bug):** the old layout seeded sink nodes with `SINK_COL = 1000000` and did arithmetic on it, filtering by *exact* equality. Reading an output register back made a circuit inherit `1000001`, which slipped the filter → `maxCol ≈ 1e6` → sinks at `x ≈ 2.8e8`px, unreachable. Fix: no sentinel arithmetic — longest-path ignores wires *from* sinks and only relaxes *into* circuits; sinks pinned after. (Later superseded by the §5 grid rewrite, but the diagnosis is the lesson.)
- **`registerIsOutputOnly` is misleadingly named** — it's the source/sink classifier, returning `false` for read-only inputs and `true` for outputs (incl. `N`). Don't read it as "write-only".
- **`GraphView` shadows `QGraphicsView::scene()`** with a *private member* named `scene`. To call the inherited `scene()` (e.g. in `graphshot` to render the full scene), go through a `QGraphicsView*` base pointer.
- **Node renderer assumes In-left / Out-right are parallel columns.** Any node with mismatched In/Out pin sets (the output hardware nodes) will visually misalign — that's why read pins are self-labeled now. A future renderer change could align by register row.
- **Pre-existing warning, not ours:** `main/colorscheme.cpp:415` ignores `QFile::open()`'s `[[nodiscard]]`. Upstream code (`ebb00ba`); left as-is per operator.
- **Harness vs app controller registration:** in the gitignored harness the `[b32]`/`[e4]` controller declarations parse as *circuits* (ModuleBuilder is stubbed); the real app should register them as controllers. Unverified in-app — flagged to operator.

## Process / discipline notes for the next agent

- **Command hygiene is enforced and was baked into every subagent prompt:** one command per Bash call; no `cd &&`/`;`/`{...}`; no inline env-vars (pass `-platform offscreen` as an arg); no `find -exec`; no inline Python (use Read/`rg`). The plan carries a "COMMAND HYGIENE" block; reuse it verbatim when dispatching.
- **Reviewers must verify against ground truth, not the plan.** M1's lesson (a plan-introduced `atomAt` bug slipped review) was applied: every reviewer subagent was told to re-run tests and check real semantics, not just "matches the plan."
- **Don't blindly act on a bug report — verify first.** Operator reported an "I1→N1 implicit wire"; dumping the actual wires proved no such wire existed (it was two horizontal wires overlapping + a hollow read pin). The real fixes (drop N read pin, label read pins) differed from the literal request.
- **The test harness (`droidforge/tests/`) is gitignored** — never `git add` it. Every commit added only production files under `droidforge/graphview/` (plus docs). Tests live locally; recreate on a fresh clone.
- **No PR.** Per the global rule + operator's standing instruction, the branch is pushed to the LuggLD fork only; the eventual upstream PR (Zarkuun) is operator-driven.
- **`graphshot.cpp` is currently hacked for this debugging** — points at `/Volumes/home/DROID Patches/EnOscBuddy v2.ini`, dumps N/I wires, and renders the full scene + a zoomed Master-out strip. It's gitignored/throwaway; reset it if you want the generic sample back.

## Open follow-ups

| Topic | Ref | Status |
|---|---|---|
| Compound input role indicators (`name` / `*` / `+`, indented) | `docs/superpowers/node-graph-followups.md` | Deferred to appearance/polish milestone |
| Model N's internal link to its input (so flow reads N→I; gives a read-of-N a real endpoint) | same doc | Deferred |
| Align output-node read pins by register row (renderer change) vs. current self-labeling | this handoff | Idea, not filed |
| Confirm `[b32]`/`[e4]` register as controllers in the real app (not circuits) | this handoff | Unverified |
| Milestone 2 (editing) and Milestone 3 (layout persistence) | original design spec | Not started — each needs spec→plan→execute |

## Memories that would help next session

| Proposed file | What |
|---|---|
| (existing) `bash-command-style.md` | Already covers command hygiene; reinforced this session. No change needed. |
| (existing) `plan-execution-preference.md` | Subagent-driven execution preference; reinforced. No change needed. |

## What works now

- The graph renders `EnOscBuddy v2.ini` faithfully: source hardware (Master in / controllers) far-left, output hardware (Master out / LEDs) far-right, each section a compact non-overlapping rectangle of grid-packed circuits.
- `encoder.output = N1` flows left→right into the Master-out node's `N1` write pin (N is correctly an output); no hollow `N` read stub.
- `fold.input = O1` (reading an output back) wires from `O1`'s right-side read pin; that pin is labeled `O1`.
- Output read pins are individually labeled (`O1`–`O8`, `G9`–`G12`), so the write/read column mismatch no longer misattributes them.
- Zoom is clamped — you can't lose the view by zooming out, and the far-right output column is reachable.
- 20/20 harness tests pass. App builds and links clean (one unrelated upstream warning). Branch pushed; no PR.

## Verification commands handy for next session

```bash
# Build + run the app
cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build"
open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"
# Force a full rebuild if needed
cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build" --clean-first

# Tests (gitignored harness; may need re-create on a fresh clone)
cmake --build "/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests"
"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests" -platform offscreen

# Render the graph (writes /tmp/graphshot.png + /tmp/graphshot_masterout.png; dumps N/I wires)
"/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/graphshot" -platform offscreen

# Branch state
git -C /Users/jan.kaluza/Projects/droidforge log --oneline d88feab..HEAD
git -C /Users/jan.kaluza/Projects/droidforge remote -v   # origin=LuggLD (push OK), no upstream PR
```

## Specs + plan artifacts

| Doc | Path |
|---|---|
| Design spec | `docs/superpowers/specs/2026-06-09-node-graph-hw-registers-and-section-layout-design.md` |
| Implementation plan | `docs/superpowers/plans/2026-06-09-node-graph-hw-registers-and-section-layout.md` |
| Polish/deferred follow-ups | `docs/superpowers/node-graph-followups.md` |
