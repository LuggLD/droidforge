# Summary: 2026-06-09T0442-node-graph-hw-registers-and-section-layout.md

## Executive summary

Second milestone on branch `feature/node-graph-editor` (continues the M1 handoff; covers only work after commit `d88feab`). Running the M1 graph on a real patch (`EnOscBuddy v2.ini`) exposed bugs and modeling gaps, fixed across 9 commits and pushed to the LuggLD fork (**no PR**). First, three eyeball bugs (`4750f8d`): unbounded zoom (clamped `[0.05,4.0]`), a layout coordinate explosion (sink-column sentinel `1000000` underwent arithmetic and slipped an exact-equality filter → outputs ~2.8e8px off-screen), and fixed-pitch node overlap (added `NodeItem::heightFor()`). Then a designed milestone (spec `6dd3d95`, plan `0b03d42`, subagent-driven): register source/output classification now derives from the authoritative `Patch::registerIsOutputOnly()` (`patch.cpp:688`) — `N` is correctly an **output**, not a source — and output hardware nodes gained right-side **read pins** (`hw.<reg>.read`) so reading an output back (`fold.input = O1`) wires from the read pin (`f8bfbb1`, `a4fcbfe`). Auto-layout was rewritten (`b199701`) to abandon unreliable data-flow layering: per-section `ceil(sqrt(n))` grid packing, source hardware pinned left and output hardware right, section bands stacked vertically with a gap `>2×FRAME_PAD` so frames are provably disjoint; sectionless patches reuse the parser's "Untitled section" normalization. Two fixes from live testing: `N` read pin dropped (it's written, never read directly — a hollow pin that misread as an `I↔N` link) (`8ac21c9`), and read pins self-labeled because the renderer lays out In/Out pins as independent top-aligned columns that misalign once counts differ (`069ccfc`). Verified: 20/20 gitignored-harness tests + offscreen `graphshot` renders. Deferred (recorded in `node-graph-followups.md`): compound-input role indicators, modeling N's internal link; plus M2 (editing) and M3 (layout persistence).

## Contents

- **L1 — Title** — node-graph hw-registers & section-layout, 2026-06-09 afternoon; continues `...T0208`, covers post-`d88feab` only.
- **L5 — TL;DR** — three bug fixes; designed HW-register + layout milestone; N→output, read pins, read-back wiring; per-section grid layout; two live-testing fixes; 20/20 tests; pushed no-PR.
- **L14 — Changes landed** — table of the 9 commits (`4750f8d`…`069ccfc`) with the *why* in Notes.
- **L28 — Decisions made (and why)**
  - **L30 — 1.** Classification from `registerIsOutputOnly()` (graph's own table wrongly listed `N` as source — the bug).
  - **L33 — 2.** Output nodes get write + read pins; classification by usage; read-backs resolve correctly.
  - **L36 — 3.** `N` gets a write pin only — you read the input `I`, not `N`; hollow read pin misread as a link.
  - **L39 — 4.** Read pins self-labeled (renderer's In/Out columns misalign once counts differ).
  - **L42 — 5.** Per-section grid layout, hardware on edges, no data-flow layering; disjoint frames via gap > 2×FRAME_PAD.
  - **L45 — 6.** Sectionless patches reuse the parser's single-"Untitled section" normalization.
- **L48 — Key gotchas** — the sentinel-explosion diagnosis; `registerIsOutputOnly` is misleadingly named (it's the source/sink classifier); `GraphView` shadows `scene()` with a private member; renderer assumes parallel In/Out columns; pre-existing `colorscheme.cpp:415` warning (not ours); harness parses `[b32]`/`[e4]` as circuits (stubbed builder).
- **L57 — Process / discipline notes** — command hygiene baked into subagent prompts; reviewers verify ground truth not the plan; verify bug reports before acting (the phantom I1→N1 wire); tests gitignored (never `git add`); no PR; `graphshot` is hacked for this debugging.
- **L66 — Open follow-ups** — input role indicators; N internal-link; read-pin row alignment; confirm `[b32]`/`[e4]` as controllers in-app; M2/M3.
- **L76 — Memories** — existing `bash-command-style` + `plan-execution-preference` reinforced; no new memory needed.
- **L83 — What works now** — faithful EnOscBuddy render; N flows into Master-out write pin; O1 read-back from labeled read pin; labeled read pins; clamped zoom; 20/20 tests; pushed no-PR.
- **L92 — Verification commands** — build/run app (+ `--clean-first`); build/run `forgetests`; `graphshot` renders; branch state.
- **L113 — Specs + plan artifacts** — design spec, implementation plan, follow-ups doc paths.
