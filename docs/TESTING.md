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

`numerical_tests` checks the [public root API](reference/NUMERICAL_API.md),
including exact zeros, continuity, callback/resource failures, preserved outputs
and an independent 1000-place reference. `numeric_root_tests` and
`numeric_root_scan_tests` retain equation/domain/scan regressions. Installed C/C++
consumers and SDK examples check the new API with the engine disabled.

The optional [Python adapter](guides/PYTHON.md) has its own reproducible source-archive,
wheel, isolated adapter-test and standalone-example check:
`python scripts/check_python_package.py --output build/python-package-check`.
It uses installed build tools with network lookup disabled and never installs
globally. The separate Python workflow covers Linux 3.10/3.12, Windows 3.12 and
macOS 3.12. Ordinary C tests/builds do not depend on Python.

`engine_tests` verifies the [public standalone engine](reference/ENGINE_API.md):
typed exact/approximate results, reusable expression ownership, a frozen independent
1000-digit sqrt(2) reference, six-rounding-mode parity with the existing evaluator,
atomic failures, allocation/deadline injection and inherited budgets. Installed
C/C++ consumers also link `NumForge::engine` without source-tree includes; use
`-DNUMFORGE_TEST_ENGINE=OFF` only for an intentionally numeric-only installation.

`engine_parameters_tests` checks explicit declarations, borrowed/aliased typed
arguments, input copies, high precision, ownership and failure recovery.
`polynomial_tests` links only the numeric library and verifies the public exact
coefficient solver, degeneracies, complex/radical descriptors, 1000-digit inputs
and allocation/deadline failures. `polynomial_benchmark` is opt-in and validates
exact sum/product invariants outside its timing interval.

`matrix_tests` links only the numeric library and checks exact rectangular
products, row pivoting, multiple RHS columns, singularity, 1000-digit cancellation,
overflow, ownership and atomic allocation/deadline failures. Python adds independent
Fraction permutation-determinant/product/known-solution references. Installed
C/C++ consumers and SDK examples verify numeric-only use. The opt-in
`matrix_benchmark` checks closed-form exact references outside timing;
see [the baseline](benchmarks/MATRIX_2026_10_19.md).

`linear_system_tests` verifies exact rectangular rank, skipped pivots, unique/
infinite/inconsistent outcomes, canonical particular/nullspace matrices, 1000-digit
inputs, queries/lifetime and atomic allocation/deadline failure. Python checks
minor-based ranks, augmented-matrix consistency and kernel/particular identities
independently with Fraction; installed C/C++ and SDK examples cover the new API.
The [follow-up benchmark](benchmarks/MATRIX_SYSTEMS_2026_10_19.md) extends the
existing matrix harness with rank and general solves, including rank deficiency.

`statistics_tests` checks [exact dataset summaries](reference/STATISTICS_API.md):
recurring fractions, population/sample divisors, singleton undefined sample
variance, duplicates/aliases/ownership, 1000-digit offset/tiny-spread cases,
validation and atomic allocation/deadline failure. Python adds an independent
pairwise-difference Fraction variance oracle and transformation identities.
Installed C/C++ consumers and SDK examples use the numeric-only target. The opt-in
`statistics_benchmark` validates closed-form summaries outside timing; see
[the baseline](benchmarks/STATISTICS_2026_10_19.md).

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

For extracted binary verification, use [release packaging](project/RELEASE_PACKAGING.md).
Graph correctness spans the C grid/sampler/budget checks, HTTP schema tests and
browser geometry/lifecycle tests. The suites check independent analytic values,
domain gaps and poles, high-frequency and discrete cases, cache reuse, stale
request cancellation, navigation, desktop/mobile layouts and inherited resource
failures. Screen-space refinement remains heuristic rather than a continuity proof.

Graph timing and memory diagnostics are documented once in the
[performance guide](testing/PERFORMANCE.md#graph-interaction-diagnostics).
Dated reports retain their measurements and limitations; they are not current
performance guarantees. Browser setup and diagnostic output locations belong in
[the browser guide](testing/WEB_AND_CI.md).
