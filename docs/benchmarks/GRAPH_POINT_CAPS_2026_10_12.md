# Adaptive graph point-cap experiment, 2026-10-12

This section records the pre-rollout experiment. The follow-up below describes
the subsequent production change to 2048 adaptive points.

The [raw CSV](graph_point_caps_2026_10_12.csv) compares 1024, 2048, 4096 and
8192-point caps across 21 cases with three repetitions (252 rows). GCC Release
uses the same independent libm references, 34/100-digit calculation precision,
depth-12 refinement, phase checks and coarse-display tolerance as the existing
graph benchmark. Allocation statistics report cumulative requested bytes, not
peak live memory. Live-allocation bookkeeping is excluded from these timings.

Run `graph_benchmark 3 2048` to repeat one cap. The optional second argument
selects adaptive C mode only: there are no enlarged HTTP responses or browser
limits in this experiment. `application_graph_adaptive_limit` is an internal C
application helper, not an installed SDK interface. The normal graph adapter
still uses 1024 points and its documented 128 KiB response cap.

## Successful comparisons

| Function and range | Cap | Median C ms | Connected x coverage | Cumulative MiB |
| --- | ---: | ---: | ---: | ---: |
| `sin(33*x)`, -10..10 | 1024 | 74.03 | 100% | 16.55 |
| Same | 2048 | 70.72 | 100% | 16.92 |
| `tan(x)`, -10..10 | 1024 | 126.86 | 89.79% | 26.03 |
| Same | 2048 | 312.21 | 96.77% | 52.88 |
| `sin(x*x)`, -100..100 | 1024 | 98.49 | 3.91% | 21.62 |
| Same | 2048 | 198.93 | 7.62% | 43.59 |
| `sin(3*x)`, -1000..1000 | 1024 | 96.50 | 0% | 22.09 |
| Same | 2048 | 197.10 | 0% | 44.72 |
| `sin(x)`, +/-1e6 | 1024 | 80.49 | 0% | 18.50 |
| Same | 2048 | 161.74 | 0% | 37.28 |

For already resolved functions the sampler stops early; sine33 still returns
723 points. The cap is a maximum, not a forced point count. A larger cap improves
some difficult curves but does not alone resolve extreme oscillations. Coverage
means accepted heuristic edges, not a mathematical proof of continuity or a
guarantee of interpolation accuracy. Existing pole checks remain unchanged.

## Limits exposed by the experiment

On this 64-bit build a coordinate occupies 168 bytes and a sample 208 bytes.
At 4096 points their contiguous arrays require 688128 and 851968 bytes;
both exceed the preserved 512 KiB single-allocation guard. All 4096/8192 rows
therefore return `CALCULATOR_VALUE_TOO_LARGE` (status 7) atomically before
sampling. They are rejected experiments, not completed large-graph timings.
No memory/time safety guard was disabled or enlarged.

At 2048 points the arrays fit, but typical dense results need 216-222 KiB for
point objects alone, before root metadata. The `projected_point_bytes` column
formats the same 16-place scientific strings and point fields as the adapter,
outside the timed C operation; it is not a complete HTTP benchmark. These
results would exceed the current 128 KiB JSON capacity. The tangent run also
consumes about 52.88 MiB of the cumulative 64 MiB allocation budget, leaving
little room to simply double work again.

## Decision and next implementation

Keep the production cap unchanged for this experiment. A safe first rollout
would allow up to 2048 points, enlarge graph-only JSON capacity to 256 KiB,
bound the browser's aggregate cache by points/bytes, and test full adapter
projection, rendering and cancellation. The single-operation 5 s and memory
guards must remain shared with response formatting.

Before 4096/8192, reduce the large per-point representation or use bounded
storage blocks; also reduce repeated temporary-allocation work. A larger graph
payload needs bounded/chunked formatting, not a blanket increase to unrelated
application limits. After those changes, rerun this matrix and measure browser
frame time/cache memory before choosing points per pixel. Dense oscillations
may still need an explicitly labelled overview/envelope mode.

GCC tracked-allocation and MSVC graph tests cover invalid caps, successful 2048
work, oversized-array rejection and unchanged previously owned output on
allocation failure. Normal adapter/browser regression checks retain the
existing 1024-point contract.

## Follow-up: bounded 2048-point rollout

The adaptive wrapper, HTTP adapter and browser now allow 2048 points. Adaptive
JSON uses a separate 256 KiB capacity; uniform graph and other session responses
still use 128 KiB. The inherited 5 s / 64 MiB / 512 KiB guards are unchanged.
Cache FIFO eviction checks 12 ranges, 12288 total points and an 8 MiB conservative
weight (twice response characters plus 256 bytes per point). This weight is not
an exact heap measurement; visible chunks may retain evicted ranges until the
next redraw. Detail tables still render at most 1024 samples plus a truncation row.

The [rollout CSV](graph_2048_rollout_2026_10_12.csv) repeats all 21 cases in
uniform/adaptive/full-adapter modes three times (189 rows). All statuses are OK;
the slowest measured full adapter repetition is 453.19 ms. Median adapter times
are 441.63 ms for tangent, 348.66 ms for wide sine, 333.92 ms for sin(x*x), and
303.02 ms for million-range sine. These runs include formatting and allocation
statistics, and are machine-specific. Dense unresolved sine views remain gaps;
increasing the cap is not sufficient to render every oscillation.

Browser tests verify real 2048-point responses exceeding 128 KiB, pole gaps,
bounded table rendering and separate eviction by total points and estimated
bytes. Existing cache/refinement/session/precision and guide regressions pass.
MSVC and GCC graph suites, including allocation failure cleanup, also pass.
