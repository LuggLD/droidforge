# Compound input pin role indicators

- **Area:** node graph
- **Severity:** low
- **Status:** appearance/polish milestone
- **Origin:** operator note, 2026-06-09

## Problem

A circuit's compound input (`primary × scale + offset`) currently renders as
three vertically stacked rows all showing the same jack label — e.g.
`input / input / input` — with no visual cue for which row is which.

## Proposal

Add a per-row indicator distinguishing the three roles:

- **Primary:** the jack name (e.g. `input`, `hz`).
- **Scale:** a multiply indicator, e.g. `*`.
- **Offset:** a bias/add indicator, e.g. `+`.

The scale and offset rows should be visually subordinate to the primary — e.g.
slightly indented — so the grouping reads as "this input, times this, plus this."
