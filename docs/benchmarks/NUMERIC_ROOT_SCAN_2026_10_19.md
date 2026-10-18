# Bounded multiple-root discovery — 19 October 2026

Baseline before this stage: `b2d3a42`. This adds discovery around the existing
production bisection solver; it does not promote either private interpolation
experiment. See the [C/HTTP contract](../reference/NUMERIC_ROOT_HTTP_API.md#multiple-root-scan).

Historical baseline: the [local-candidate follow-up](NUMERIC_ROOT_CANDIDATES_2026_10_19.md)
now recovers the two deliberately missed quadratic cases below and preserves
recurring rational coordinates in HTTP. These measurements describe the earlier
grid-only implementation.

## Method and scope

Sample up to 129 exact rational vertices of 128 equal interval cells. Refine
opposite-sign candidates using the unchanged single-root acceptance/domain and
higher-precision stability checks. An approximately zero grid vertex needs
opposite-sign real neighbours; exact typed zeros have their pointwise path.
Retain at most 32 ordered points and 4096 total coordinate-pair evaluations,
under one inherited operation budget. Coordinate equality removes duplicates;
nearby distinct coordinates are not tolerance-merged.

Discovery is always incomplete. The corpus deliberately includes a missed
off-grid tangent and two roots in one cell. It also checks a sampled pole,
vanishing jump, tiny scaled pole, partial logarithm domain, result ceiling and
repeated steps that exhaust the evaluation allowance. Finite sampling is not a
proof that all roots or all domain gaps were located.

## Reproduction

With benchmarks enabled, build `numeric_root_scan_benchmark`, then:

```text
numeric_root_scan_benchmark --quick
numeric_root_scan_benchmark > scan.csv
python scripts/check_numeric_root_scan.py scan.csv
```

The full run records 11 cases × 3 precisions (16/34/100) × 3 repetitions =
99 rows. `--quick` runs all 11 at precision 34 once and is a CTest smoke check.
Rows include time, termination reason, work, domain/rejection diagnostics,
cumulative allocation bytes and complete rational coordinates.

Measurements below used GCC 16.2 UCRT, Release with allocation counters enabled,
serially on the development PC. Live allocation tracking was disabled for timing.
Raw local output: `build/numeric-root-scan-2026-10-19/gcc.csv` (ignored build data).
Timings exclude HTTP/browser work and are observations, not portable bounds.

## Results

Median milliseconds of three runs:

| Case | Precision 16 | Precision 34 | Precision 100 | Result |
| --- | ---: | ---: | ---: | --- |
| Three polynomial roots | 5.65 | 5.83 | 7.46 | −3, 0, 2 |
| sin(x)=0 on −10..10 | 47.17 | 54.21 | 97.73 | Seven roots |
| cos(x)=x on −2..2 | 19.94 | 25.40 | 50.03 | One root |
| ln(x)=0 on −2..2 | 31.79 | 36.94 | 73.19 | One root, invalid grid region reported |
| 1/x=0 on −1..1 | 3.69 | 3.60 | 4.03 | No root confirmed; two sampled gap cells |
| Vanishing jump | 10.92 | 11.73 | 11.20 | Candidate rejected |
| Tiny scaled pole | 13.58 | 14.30 | 14.65 | Candidate rejected |
| Off-grid tangent | 5.90 | 6.17 | 6.16 | Missed, incomplete discovery |
| Two roots in one cell | 4.97 | 5.46 | 4.94 | Missed, incomplete discovery |
| x=x | 0.85 | 1.06 | 1.06 | Stops at 32 sampled points |
| Repeated floor/mod steps | 219.35 | 221.93 | 222.01 | Stops at evaluation allowance |

Worst observed row: **229.61 ms**, repeated steps at precision 34. It used 3962
coordinate pairs, 3840 bisections and 40,487,613 cumulative allocation bytes.
The next full 192-pair refinement would not fit, so the scan stops before 4096.
Seven sine roots at precision 34 used 504 coordinate pairs and 8,233,344 bytes;
at precision 100 they used 11,698,250 bytes. These cases fit the shared five-second
and 64-MiB budgets; arbitrary allowed expressions can still exceed them.

The independent standard-library checker passed all 99 rows and 396 full
coordinates: 108 finite-root coordinates against rational/π/fixed-point
references and 288 deliberately truncated identity grid points. It uses
Decimal precision 180 and exact Fraction parsing; the cosine residual is also
recomputed using a separate Taylor implementation. Empty diagnostic/missed-root
rows are explicitly checked, rather than treated as proof of root absence.

## Validation and conclusions

New unit tests cover ascending order, both overall endpoints, shared-grid
uniqueness, exact grid tangencies, approximate grid zeros, precision extremes,
domain/rejection diagnostics and both work ceilings. Allocation and deadline
injection preserve the caller's output; a small inherited cumulative budget
also rejects the whole pending scan after work has begun. HTTP allocation tests
never publish a partial successful root list. GCC allocation-ledger checks
require zero remaining tracked allocations.

MSVC testing initially caught an oversized **test fixture** session pool on the
Windows stack. Moving that fixture to static storage, as existing HTTP tests do,
fixes the setup without changing production stack sizes or solver behavior.
Browser regressions cover both languages, copy of full coordinates, session
restore, graph handoff, incomplete results, limits and narrow-screen layout.

This is fast enough to expose as an explicit discovery mode without further
interpolation changes. Next, study off-grid tangencies and clustered roots with
bounded adaptive subdivision or derivative-assisted candidates. Preserve the
same final original-expression checks and incomplete-result contract; do not
replace them with a small sampled residual or claim full enumeration.
