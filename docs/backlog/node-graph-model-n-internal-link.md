# Model N's internal link to its input

- **Area:** node graph (model/correctness)
- **Severity:** low
- **Status:** deferred (from the 2026-06-09 HW-register work)
- **Origin:** 2026-06-09

## Problem

`N` (normalize) registers are currently plain write targets (write pin only;
read pin dropped). Semantically, writing `N1` sets the normalized value of input
`I1`, so the true signal path is:

```
producer → N1 → (internally) → I1 → readers of I1
```

## Why it matters

Modeling that internal link would let the graph show the flow as N→I (the
intended direction) and would give a read of `N1` a real endpoint (today such a
read would resolve to a non-existent pin and be skipped).
