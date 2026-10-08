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

`bigrational_tests` covers canonical fractions, aliasing, conversions and a
small independent signed-integer reference grid. Allocation-failure tests check
that rational arithmetic leaves aliased destinations unchanged. Installed C
and C++ package consumers compile and call the public rational API.

## Choose a focused check

| Guide | Checks |
| --- | --- |
| [Numerical references and parser limits](testing/NUMERIC_TESTING.md) | Independent oracle, transcendental references, fuzzing and precision contracts. |
| [Session and unit regressions](testing/REGRESSIONS.md) | Application ownership, variables, Quantity, catalogue, converter and failure atomicity. |
| [Browser tests and CI](testing/WEB_AND_CI.md) | Playwright setup, browsers, sanitizers, coverage and installed C/C++ consumers. |
| [Performance checks](testing/PERFORMANCE.md) | Opt-in benchmark builds, profiling and narrow timing probes. |

See [release record](project/RELEASE_2_1.md) for the 2.1 checks and
[packaging](project/RELEASE_PACKAGING.md) for extracted binary verification.
