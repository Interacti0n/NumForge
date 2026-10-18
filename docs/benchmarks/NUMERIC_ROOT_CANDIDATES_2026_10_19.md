# Local root candidates — 19 October 2026

Follow-up to the [128-cell scan](NUMERIC_ROOT_SCAN_2026_10_19.md), baseline
`d61ffde`. The original scan missed off-grid tangencies and two crossings inside
one cell. This stage adds bounded midpoint/local-quadratic candidate discovery
without changing production single-bracket bisection or its acceptance checks.

## Proposal and acceptance

Same-sign real grid endpoints receive an additional midpoint sample. If it
reveals opposite signs, both halves use the existing checked bracket solver.
Otherwise a local interpolating parabola may propose an interior vertex:

```text
vertex = midpoint + span * (left_value-right_value)
                    / (4 * (left_value-2*middle_value+right_value))
```

All coordinate/proposal arithmetic uses BigRational. Exact sampled differences
are preserved as rationals; decimal differences are only proposal data. A
distinct interior vertex is sampled through **both original expression trees**.
Only a typed exact zero or the original accepted sign brackets may publish a
root. A minimum, small residual, fitted parabola or rounded zero is insufficient.
No derivative or continuity property is inferred from the fitted curve.

This recovers `(x-0.1234)^2=0`, `(x-1/3)^2=0`, close quadratic pairs and small
coordinates. Tests also preserve an exact contact at `1E100+1/3`. A positive
`(x-0.1234)^2+1E-100` remains rootless. A hole on either original side at a
candidate point remains excluded, e.g. `(x-1/3)^2=0/(x-1/3)`.

General contacts such as `(cos(x)-x)^2=0` may remain undiscovered. Finite sampling
and local quadratic candidates still miss some clusters, multiple roots within
subsections, approximate endpoint zeros and unsupported domains. Results remain
complete:false; finishing the grid does not enumerate all mathematical roots.

## Coordinate transport

The previous scan had dyadic decimal grid/bracket coordinates. A new candidate
can be exactly `1/3`; rounding it to a finite decimal would detach the residual
from the emitted coordinate. Scan HTTP roots now use canonical rational x plus
coordinate_kind:rational, with a separate decimal approximation. Copy preserves
that rational point and the graph camera uses the preview. Graph markers still
require their own C verification. Single-root numerical mode keeps its existing
full decimal x contract. This is an unreleased scan-contract refinement; older
grid-only tab results are discarded while inputs remain available.

## Measurement

GCC 16.2 UCRT Release/WX, allocation counters enabled and live tracking disabled.
Serial runs on the development PC, excluding HTTP/browser overhead. The same
`numeric_root_scan_benchmark` now has 15 cases, 3 precisions (16/34/100) and
3 repeats: **135 rows**. Final ignored data is
`build/numeric-root-candidates-2026-10-19/final.csv`.

```text
numeric_root_scan_benchmark > candidates.csv
python scripts/check_numeric_root_scan.py candidates.csv
```

Median milliseconds at precision 34:

| Case | Original scan | Local candidates | Result |
| --- | ---: | ---: | --- |
| Three polynomial roots | 5.83 | 16.85 | Same three roots |
| Seven sine roots | 54.21 | 87.03 | Same seven roots |
| cos(x)=x | 25.40 | 60.97 | Same root |
| Partial logarithm domain | 36.94 | 56.01 | Same root, domain diagnostics |
| Off-grid quadratic contact | 6.17 | 17.38 | Previously missed; now exact 0.1234 |
| Two roots in one grid cell | 5.46 | 19.68 | Previously missed; now 0.01 and 0.02 |
| Rational contact at 1/3 | — | 22.20 | Exact recurring fraction |
| Pair separated by 2E-10 | — | 35.71 | Both checked coordinates |
| Positive minimum 1E-100 | — | 26.41 | No false root |
| General cos-square contact | — | 92.53 | Still unconfirmed; limitation retained |
| Evaluation ceiling | 221.93 | 229.12 | Bounded stop, no root |

Additional discovery costs about one extra sample per same-sign cell; this is
a deliberate coverage tradeoff, not a speedup claim. Sine at precision 34 uses
630 coordinate pairs and 11,918,356 cumulative allocation bytes; the previous
scan used 504 pairs/8,233,344 bytes. The worst final row was **298.24 ms**, repeated
steps at precision 100, with 3962 pairs and 40,493,624 cumulative bytes. Tested
cases remain within one five-second/64-MiB budget. Arbitrary expressions can
still exhaust cooperative time/memory before the coordinate ceiling.

The independent Decimal-180/Fraction checker passed all 135 rows and **450 full
coordinates**: 162 finite-root coordinates and 288 intentionally capped identity
grid points. It includes known polynomial/π/fixed-point references, independently
evaluated cosine residuals, empty diagnostic cases and new candidate counters.

## Validation and follow-up

Tests cover precision 16/34/100, exact recurring fractions, tiny and large-offset
contacts, positive near-minima, close pairs, original-side domain holes, sorted
distinct results, shared work/memory, allocation/deadline failures and atomic
HTTP publication. Browser tests cover canonical rational copy, graph handoff,
retained inputs, invalidation of old grid-only results and both languages.

The scan smoke benchmark is now part of the existing three-OS CI benchmark job.
The equivalent apps-disabled Release/WX configuration and all three selected
smokes passed locally; remote platform results require a push. Release gates and
remaining usability work are tracked in the [equation release audit](../project/EQUATION_RELEASE_AUDIT_2026_10_19.md).
