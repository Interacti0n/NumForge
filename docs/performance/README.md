# Performance evidence

Start with the [tool index](../../benchmarks/README.md) and
[measurement method](../testing/PERFORMANCE.md). Protocols describe how to run
diagnostics; baselines contain selected historical results. None is a timing
promise for the current checkout, another machine or concurrent server users.

## Protocols

| Protocol | Coverage |
| --- | --- |
| [Application and formatting](protocols/APPLICATION.md) | Output conversion, caches, HTTP and memory instrumentation |
| [BigInt](protocols/BIGINT.md) | Exact fixtures, operand shapes, independent recording |
| [Decimal](protocols/DECIMAL.md) | Arithmetic/functions, rounding, timing and allocation probes |
| [Complex](protocols/COMPLEX.md) | Independent unary/binary/stability references and memory probes |

## Baselines

| Record | Recorded dates | Purpose |
| --- | --- | --- |
| [Arithmetic](baselines/ARITHMETIC.md) | October 2026 | Selected division, multiplication, reuse, complex composition and formatting comparisons |
| [Factorial](baselines/FACTORIAL.md) | 12 October 2026 | Final split workspace, admission boundaries and complete checked outputs |
| [Logarithm](baselines/LOGARITHM.md) | 11 October 2026 | Production interval algorithm, enclosure rationale and paired timings |
| [Exact modules](baselines/MODULES.md) | 19–20 October 2026 | Polynomial, matrix/system, statistics and covariance fixtures/results |
| [Sampling](baselines/SAMPLING.md) | 19 October 2026 | Extracted grids, scheduling, classification, phase anchors and HTTP parity |
| [Public roots](baselines/ROOTS.md) | 19 October 2026 | Callback bisection and bounded scan, including expensive stress limits |
| [Equation experiments](baselines/ROOT_EXPERIMENTS.md) | 18–19 October 2026 | Domain safeguards, discovery costs and why interpolation stays private |
| [Graph corpus](baselines/GRAPHS.md) | 16 October 2026 | Browser/HTTP coverage, measured errors and unresolved sampling limits |
| [Graph refinement](baselines/GRAPH_REFINEMENT.md) | 16–17 October 2026 | Cache and Brent comparisons with original-expression verification |

## Retention policy

Keep one useful baseline per topic: date/revision, environment, command, fixture,
verification, timing scope, result and limits. Keep a comparison only while it
explains a retained algorithm decision or a useful regression baseline. Nine CSVs
under `data/` directly support the selected sampling/root/graph tables; benchmark
input fixtures stay with the executable sources under `benchmarks/`.

Superseded tuning steps, duplicate test counts and intermediate CSVs were removed
from the working tree. Original reports remain in Git history (the pre-cleanup
snapshot is `c63e5c9`). Use that snapshot to investigate an old experiment; do not
restore every diary as an active document. New routine recordings belong under
ignored `build/`, not another date-named Markdown/CSV pair in the repository.

For a new capability, extend its topic baseline when results are useful and fully
verified. Replace a baseline only with a matching reproducible comparison, or
label a changed protocol explicitly. Preserve important failures and research
decisions; an attractive fast row alone is insufficient evidence.
