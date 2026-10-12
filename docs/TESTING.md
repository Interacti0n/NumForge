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

| Guide | Checks |
| --- | --- |
| [Numerical references and parser limits](testing/NUMERIC_TESTING.md) | Independent oracle, transcendental references, fuzzing and precision contracts. |
| [Session and unit regressions](testing/REGRESSIONS.md) | Application ownership, variables, Quantity, catalogue, converter and failure atomicity. |
| [Browser tests and CI](testing/WEB_AND_CI.md) | Playwright setup, browsers, sanitizers, coverage and installed C/C++ consumers. |
| [Performance checks](testing/PERFORMANCE.md) | Opt-in benchmark builds, profiling and narrow timing probes. |

See [release record](project/RELEASE_2_1.md) for the 2.1 checks and
[packaging](project/RELEASE_PACKAGING.md) for extracted binary verification.
