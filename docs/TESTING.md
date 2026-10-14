# Testing and performance baselines

## Run the complete local suite

```sh
cmake -P cmake/LocalBuild.cmake
```

The command configures a Release build, builds it and runs CTest. Its ignored
directory under `build/local-tests-*` depends on the absolute project path.
After moving or renaming the project it creates a new build tree instead of
using CMake files that point at the old location. It also checks both source
and build paths saved in an existing cache before reusing it. It does not
delete old build trees. Use the printed build path for any direct `ctest`
command. A standalone `ctest --test-dir build` can run stale generated tests
from a previous project location and is not a reliable validation step.

Node.js enables the UI and numerical
oracle suites with no npm install. Add `-DNUMFORGE_REQUIRE_NODE_TESTS=ON` to
your own CMake configure to require them instead of allowing a local skip;
CI CTest jobs do this.
Unity is a pinned, test-only dependency. Testing tools are not installed with
the production library.

## Build without tests and open the apps

```sh
cmake -DNUMFORGE_LOCAL_TESTS=OFF -P cmake/LocalBuild.cmake
```

This uses a separate `build/local-apps-*` Release tree, without Unity or test
executables. Subsequent runs reuse it and rebuild only changed dependencies.
On Windows, build and open the CLI and web calculator together:

```sh
cmake -DNUMFORGE_LOCAL_TESTS=OFF -DNUMFORGE_LOCAL_RUN=both -P cmake/LocalBuild.cmake
```

Use `cli` or `web` instead of `both` to open only one app. Web mode starts a
local server in a visible console and opens the default browser after it is
ready. Close that console or press Ctrl+C to stop the server. An occupied port
is rejected without stopping the existing server; add
`-DNUMFORGE_LOCAL_PORT=8767` to choose another port (default 8765).
No tunnel is started. Automatic launching is currently Windows-only; the
build-only command also supports other hosts. Apps open only after a successful
build and, when enabled, successful tests.

`bigrational_tests` covers canonical fractions, aliasing, conversions and a
small independent signed-integer reference grid. Allocation-failure tests check
that rational arithmetic leaves aliased destinations unchanged. Installed C
and C++ package consumers compile and call the public rational API.

## Choose a focused check

`application_expression_tests` covers the
[parametric C application foundation](design/PARAMETRIC_EXPRESSIONS.md), including
snapshot lifetime, typed samples, per-point errors and shared resource budgets.
Opt-in allocation-instrumented builds also check zero live allocations and
cumulative allocation limits across points.

`graph_tests` checks exact inclusive grids, point classifications, session
isolation, inherited budgets and allocation failures through HTTP serialization.
Playwright `graph.spec.js` verifies the [graph HTTP contract](reference/GRAPH_HTTP_API.md)
against the real server, including query schemas, Unicode columns and Origin
protection; the guide suite checks its localized navigation entry.
`graph-ui.spec.js` verifies responsive axes, adaptive lines and inferred parameters, typed sample
data, finite browser projection/underflow, read-only session snapshots, navigation
state and cancellation of stale requests.

| Guide | Checks |
| --- | --- |
| [Numerical references and parser limits](testing/NUMERIC_TESTING.md) | Independent oracle, transcendental references, fuzzing and precision contracts. |
| [Session and unit regressions](testing/REGRESSIONS.md) | Application ownership, variables, Quantity, catalogue, converter and failure atomicity. |
| [Browser tests and CI](testing/WEB_AND_CI.md) | Playwright setup, browsers, sanitizers, coverage and installed C/C++ consumers. |
| [Performance checks](testing/PERFORMANCE.md) | Opt-in benchmark builds, profiling and narrow timing probes. |

See [release record](project/RELEASE_2_1.md) for the 2.1 checks and
[packaging](project/RELEASE_PACKAGING.md) for extracted binary verification.

Adaptive graph tests compare linear/curved density, offset poles, automatic AST
parameters, read-only sessions, stratified allocator failures and deterministic
deadlines. Allocation-stat builds require zero live allocations after cleanup.

Graph regressions also cover `sin(3x)`, mixed/nested functions, 100-digit requests,
trillion-wide smooth ranges, high-frequency phase safeguards, inherited budget
admission, phase ownership/faults and empty axes without HTTP requests. Browser
checks compare real points and quarter/mid/three-quarter chord probes against
independent JavaScript references; numeric C suites retain high-precision oracles.
The opt-in `graph_benchmark [1..10]` reports uniform/adaptive/HTTP timings,
cumulative allocations, resolution limits, independent libm point/chord errors
and accepted interval coverage. See the [graph audit](benchmarks/GRAPH_AUDIT_2026_10_12.md).
The 21-case reliability sweep includes reciprocal/shifted poles and square-root
and logarithm domain boundaries. HTTP regressions verify independent real values
and disconnected poles on centered and shifted intervals; cache tests compare
curve geometry and axis labels after returning to the same viewport.
An optional second benchmark argument, `graph_benchmark 3 2048`, selects an
adaptive C-only point-cap experiment (1024/2048/4096/8192). See the
[point-cap audit](benchmarks/GRAPH_POINT_CAPS_2026_10_12.md) for allocation and
projected-payload constraints. The production adaptive HTTP/browser cap is now
2048 with a graph-only 256 KiB response; uniform sampling stays at 1024.
The opt-in `graph_fast_benchmark [1..10]` compares an experimental C double
bytecode evaluator with exact C points and libm compositions. See the
[fast graph audit](benchmarks/GRAPH_FAST_EXPERIMENT_2026_10_12.md). It is not
routed directly through the precision API. The web now uses the
[screen-space adapter](reference/SCREEN_GRAPH_HTTP_API.md), with pixel/chord,
periodic/stationary-extremum, derivative-domain, pole/domain, coarse/refined
cancellation and cache tests. Browser geometry is compared with independent
analytic curves on desktop/mobile; next-zoom cache reuse and malformed optional
slopes are covered. The reproducible [smooth-zoom comparison](benchmarks/GRAPH_SMOOTH_ZOOM_2026_10_13.md)
records HTTP work, bytes, reference errors and local frame observations.
Atomic resource/fault checks include both evaluators and the compact sampler.
Multi-curve regressions check four-row limits, color-only repaint, error isolation,
stale deleted results, selected-row library insertion/aliases, SPA persistence
and expanded-canvas identity/return on desktop and both mobile orientations.
Library insertion checks both languages/mobile and desktop: empty calls, caret
placement, selected/nested expressions, required argument separators, recent
calculator tools and real calculation/plotting after typing the arguments.

Graph interaction regressions exercise wheel debounce, pointer panning, touch
pinch, keyboard/reset controls, empty-view navigation and y-axis rescaling.
C/HTTP checks verify newly discovered extrema and bounded unresolved pole views.

Periodic-extremum regressions check every sin/cos peak on −100..611, reversed
phases, sin(33x), degree mode and rendered y bounds. Peak-work allocator failures
verify atomic cleanup; the graph benchmark includes the reported wide sine case.

Range-cache browser tests verify immediate prepared pan, requests for missing
pieces only, retained curves during refinement, current session-revision checks
and cache invalidation on expression/precision/angle edits. Interaction rendering
is coalesced to animation frames; delayed foreground/background work is canceled
by edits and page disposal. The server mathematics and SDK remain unchanged.

Deep-zoom regressions compare sine near −3π/2 with independent references,
require distinct projected values and axis labels, check that negative tick
labels fit inside the SVG, and keep near-flat quantization below a visible pixel.
Adaptive output uses 16 scientific places; uniform output remains at 12.

## Multi-function graph visibility and performance

`graph-visibility.spec.js` checks cache-only visibility toggles in both languages
and desktop/mobile layouts, unchanged paths/camera/colors, hidden-request
cancellation, edited hidden expressions, keyboard focus and persistence.

Run the real-browser diagnostic against two built server executables:

```powershell
node scripts/benchmark_graph_multi.cjs --before build/server-8ee7e5a/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-multi-benchmark.json
```

Install browser dependencies as described above. The script starts and stops only
its own servers, compares four mixed and four dense/domain-sensitive functions on
desktop/mobile-sized Chromium viewports, and records initial/expanded rendering, drag,
zoom, warm toggles, HTTP requests/data/points, frame timing, long tasks and actual
Chromium JavaScript heap usage. Times are diagnostics, not CI thresholds. Settled
times include a fixed 350 ms network quiet interval; mobile-sized Chromium on a PC
is not a physical phone measurement. Warm toggles must make zero graph requests.

Recorded comparison and limitations: [four-function graph benchmark](project/GRAPH_MULTI_BENCHMARK_2026_10_13.md).

Coordinate inspection regressions in `graph-inspector.spec.js` compare the readout
against independent sin/sqrt/reciprocal references, verify closest-curve ownership
and color, accepted-edge gaps, touch tracing, second-finger pinch, empty-area pan,
expanded view, zero labels and both locales/layouts. Mouse and touch inspection
must issue zero graph HTTP requests. A burst of 200 pointer events must produce
one coalesced readout update while preserving the original plot DOM/geometry;
its measured frame latency is recorded diagnostically, without a flaky CI timer.

`graph-priority.spec.js` verifies that each foreground request uses the current
camera and all visible functions finish before off-screen preparation. A held
buffer must not delay/change the displayed curve. Dense sqrt/abs/sine checks
complete accepted-edge coverage against independent references; extreme sine
remains bounded and is not counted without an accepted stroke. Cached pan and
reciprocal pole breaks remain covered by the existing regressions.

Visible-first comparison: [sampling priority benchmark](benchmarks/GRAPH_VISIBLE_PRIORITY_2026_10_14.md).
