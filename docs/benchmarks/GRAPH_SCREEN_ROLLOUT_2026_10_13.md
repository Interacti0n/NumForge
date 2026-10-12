# Screen-space graph rollout, 2026-10-13

The [315-row sweep](graph_screen_rollout_2026_10_12.csv) compares 21 expressions,
three repetitions, uniform129, precision adaptive/HTTP and screen-space C/HTTP.
Measurements started on October 12 and finished with validation on October 13.
GCC Release allocation statistics measure cumulative bytes, not peak heap.

| Case | Precision HTTP median ms | Screen HTTP median ms | Screen points | C cumulative MiB |
| --- | ---: | ---: | ---: | ---: |
| Wide sine, -100..611 | 111.56 | 49.95 | 6535 | 0.41 |
| sin(33x), -10..10 | 119.87 | 41.50 | 6143 | 0.41 |
| Mixed sin/cos | 140.57 | 3.45 | 455 | 0.80 |
| tan(x), -10..10 | 431.03 | 37.72 | 6673 | 0.41 |
| sin(x*x), -100..100 | 346.04 | 119.56 | 8192 | 0.47 |

All rows completed with status OK and accepted C points passed independent libm
references (maximum observed relative point error below 1.02e-13). HTTP rows
measure full formatting but do not independently parse points; separate API
tests do. Screen C rows can fall back to precision sampling, notably the
100-digit case; the mode's cap is an upper bound, not a forced point count.

Wide sine and sine33 have full accepted coverage and actual periodic peak samples.
The wide sine regression probes quarter/mid/three-quarter positions of every
chord and bounds observed vertical error by 1.2 pixels at requested height 440.
Tangent retains about 99.9% accepted coverage with poles disconnected. These are
sampled heuristics, not proofs of global continuity or interpolation bounds.

Extremely dense oscillation still exhausts the cap: sin(x*x) on +/-100 retains
about 4.8% connected coverage and sin(3x) on +/-1000 about 3%. More speed and
8192 samples do not resolve arbitrary frequencies on a finite screen. Unresolved
samples are labelled as such rather than invented zeros or connected poles.

The browser keeps cached camera motion immediate, paints coarse results before
final refinement and rejects obsolete responses. Full precision fallback is not
requested twice. SVG paths now reuse connected line commands. Cache eviction
protects useful visible data; dense partial jobs replace a full viewport to
respect total point bounds. Initial server sampling, payload decoding and
rendering remain real work; no universal frame-rate guarantee is claimed.
In a local Chromium cached-motion probe over 20 alternating wheel frames, the
median frame interval was 16.7 ms and maximum 33.4 ms; the dense curve remained
visible throughout. This is one machine/run, not a CI timing threshold. A dense
background-cache regression separately verifies that optional neighbor work
does not evict the current fine viewport.

Validation covers the existing graph/guide browser suite, independent fast API
values/domain/pole/chord tests, precision fallback, progressive cancellation and
MSVC/GCC C suites. Compact sampler allocation failures and inherited-budget
failure leave outputs unchanged; tracked allocations return to zero.

See the [screen API contract](../reference/SCREEN_GRAPH_HTTP_API.md) for schema,
precision differences and resource limits. The previous
[double experiment](GRAPH_FAST_EXPERIMENT_2026_10_12.md) records only raw point
evaluation and must not be read as an HTTP/render timing claim.
