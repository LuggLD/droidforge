# Rack-accurate hardware nodes (MASTER vs MASTER18, G8 expanders, X7)

- **Area:** node graph
- **Severity:** medium
- **Status:** deferred to a dedicated session (too big to fold into the wiring fixes)
- **Origin:** interactive testing, 2026-06-10

## Problem

The hardware nodes are built from hardcoded firmware counts
(`numGlobalRegisters` = fixed 8 I / 8 N / 8 O / 12 G) and do **not** reflect the
configured rack. Per the DROID manual (verified):

- **Standard MASTER:** 8 CV inputs (I1–I8), 8 CV outputs (O1–O8), a 4×4 LED
  matrix. **No dedicated gate sockets** — gates come only from G8 expanders / X7.
- **MASTER18:** 8 CV outputs; **no CV inputs** — instead 2 gate/trigger inputs
  (I1, I2, logic-level) and 4 gate outputs (G1–G4); built-in VCO tuner; MIDI.
- **G8 expander:** up to 4, each 8 gate jacks (`G1.1…`, `G2.1…`), every jack
  input *or* output depending on use. The Forge shows one G8 by default.
- **X7:** 4 gate outputs (`G9`–`G12`) plus USB/MIDI.

Today the master node always shows I1–8 / N1–8 / O1–8 (correct for a standard
MASTER, **wrong for MASTER18**) and 12 gate pins that — after the 2026-06-10 id
fix — represent "one G8 (`G1.1`–`G1.8`) + X7 (`G9`–`G12`)", which matches the
Forge default but isn't disambiguated by what's actually installed.

## To do

- Drive node construction from `Patch::typeOfMaster()` and the rack config
  (`ModuleBuilder::allRegistersOf` / installed modules) instead of fixed counts.
- Render only installed G8 expanders (and additional ones, `g8≥2`).
- Model MASTER18's gate I/O (I1/I2 as gate inputs, G1–G4 outputs, no CV inputs).

## Notes

**Caveat:** `ModuleBuilder` is stubbed in the gitignored test harness, so this is
largely app-verified, not unit-testable there.
