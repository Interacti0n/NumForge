# Testing

## Run the complete local suite

```sh
cmake -P cmake/LocalBuild.cmake
```

Requires a C17 compiler and CMake 3.21+ for shared presets (direct library
configuration still supports CMake 3.20). The helper configures, builds and runs
`dev` in `build/dev`. CMake rejects caches belonging to another checkout; keep
build directories in their original location.

## Shared build profiles

Run presets from the repository root:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

| Profile | Output and scope |
| --- | --- |
| `dev` | `build/dev`: applications and regression tests |
| `apps` | `build/apps`: applications without test dependencies |
| `sdk` | `build/sdk`: numeric/engine libraries; default install prefix `build/sdk-installed` |
| `benchmarks` | `build/benchmarks`: instrumented benchmarks and their correctness checks |
| `analysis` | `build/analysis`: production libraries/applications with clang-tidy |

All profiles select Release and project warnings as errors; `dev` and
`benchmarks` also have matching CTest presets. CTest remains serial by default
for server checks. Visual Studio uses `Release/` below the build directory;
single-configuration generators place executables directly in it.
Command-line `-D` options override configure settings. Keep machine-specific
compiler/tool paths in ignored `CMakeUserPresets.json` or the environment.

The first test configuration fetches pinned Unity using Git/network access.
Node.js enables additional architecture, CLI, numeric-oracle and web-script
checks. CI sets `NUMFORGE_REQUIRE_NODE_TESTS=ON` so those checks cannot silently
skip. Testing tools are not application runtime dependencies.

## Build without tests and open the apps

```sh
cmake -DNUMFORGE_LOCAL_TESTS=OFF -P cmake/LocalBuild.cmake
```

This uses the separate `apps` profile. On Windows, add
`-DNUMFORGE_LOCAL_RUN=both` before `-P` to launch CLI and web after a successful
build; `cli`/`web` select one. The helper opens a visible app console. Change the
default port with `-DNUMFORGE_LOCAL_PORT=8767`; occupied ports are rejected.
Build-only mode works on other hosts. No tunnel is started.

Test sources are grouped by domain in the [test source map](../tests/README.md).

## Static analysis

The optional `analysis` profile needs Ninja, a C17 compiler and clang-tidy on PATH.
On Windows use a compiler developer shell. For example:

```sh
cmake --preset analysis
cmake --build --preset analysis --clean-first
```

`.clang-tidy` checks division by zero, misplaced widening casts and suspicious
`sizeof`/`memset` usage; findings fail this profile. It covers production
sources in the ordinary platform configuration, including application adapters,
without fetching test dependencies. Add checks after reviewing their findings.
Normal builds and installed consumers do not require the analysis tools.
Static analysis complements runtime/failure tests; it does not prove numerical
accuracy or exercise disabled platform/optional code paths.

## Choose a focused check

Use the suite matching the changed behavior, then broader checks when the change
crosses boundaries. Detailed edge cases, numerical oracles and measured evidence
belong to the linked contracts/reports rather than this test index.

| Change | Main checks | Contract or method |
| --- | --- | --- |
| Numeric types, rounding, conversions | `bigint_tests`, `bigrational_tests`, `bigdecimal_tests`, property/oracle and allocation suites | [Numeric methodology](testing/NUMERIC_TESTING.md), [C API](reference/C_API.md) |
| Complex functions and branches | `bigcomplex_tests`, complex oracle/allocation groups | [Complex reference](reference/BIGCOMPLEX.md) |
| Public expressions/parameters | `engine_tests`, `engine_parameters_tests` | [Engine contract](reference/ENGINE_API.md) |
| Exact polynomial solving | `polynomial_tests`, equation adapter checks | [Polynomial contract](reference/POLYNOMIAL_API.md) |
| Matrices and rectangular systems | `matrix_tests`, `linear_system_tests` | [Matrices](reference/MATRIX_API.md), [general systems](reference/LINEAR_SYSTEM_API.md) |
| Summaries, covariance and regression | `statistics_tests`, `regression_tests`, Python statistics/regression tests | [Statistics contract](reference/STATISTICS_API.md) |
| Grids, refinement, curve/phase policies | `numerical_grid_tests`, `numerical_refinement_tests`, `numerical_curve_tests`, `numerical_phase_tests` | [Numerical contracts](reference/NUMERICAL_API.md) |
| Public root kernels | `numerical_tests`, `numerical_scan_tests` | [Root/scan meaning](reference/ROOT_API.md#use-and-result-meaning) |
| Equation application/HTTP | `equation_tests`, `numeric_root_tests`, `numeric_root_scan_tests`; affected browser equations | [Equation API](reference/EQUATION_HTTP_API.md), [numerical HTTP](reference/NUMERIC_ROOT_HTTP_API.md) |
| Sessions, units, client ownership | Application, session, unit, cache and HTTP suites | [Regression guide](TESTING.md#choose-a-focused-check) |
| Prepared application expressions/graphs | `application_expression_tests`, `graph_tests`; affected graph browser cases | [Application expressions](design/PARAMETRIC_EXPRESSIONS.md), [graph contracts](API.md) |
| HTTP decoding/framing | `http_request_tests`, affected unit/session routes and browser API tests | [HTTP contract](reference/HTTP_API.md) |
| Source boundaries/manifests | `architecture_check`, `architecture_guard_tests` | [Architecture](design/ARCHITECTURE.md) |

For example, use `ctest --test-dir YOUR_BUILD -C Release -R statistics_tests
--output-on-failure` after building that target. Relevant allocation/deadline
tests must preserve destinations, release owned values and recover after failure.
Instrumented numerical/application runs additionally check zero tracked live
allocations. High-precision tests require independent references, not formatted
agreement or identities alone.

For result-boundary changes, compare complete HTTP results with the preceding
build, including exact/radical/complex values, domain gaps, contact roots and
partial-limit diagnostics. The [architecture](design/ARCHITECTURE.md#dependencies)
defines the boundaries those regressions must preserve.

## Installed packages and Python

Standalone consumers must compile against installed headers/targets outside the
source tree. Validate numeric-only and engine-enabled installations separately;
`NUMFORGE_TEST_ENGINE=OFF` is for an intentionally numeric-only installation.
Install the chosen build with `cmake --install BUILD_DIR --config Release --prefix /absolute/sdk`, then:

```sh
cmake -S tests/package_consumer -B build/consumer -DCMAKE_PREFIX_PATH=/absolute/sdk -DNUMFORGE_TEST_CPP=ON
cmake --build build/consumer --config Release
ctest --test-dir build/consumer -C Release --output-on-failure
```

SDK examples in `examples/sdk/` exercise the same installed targets.

```sh
python scripts/check_python_package.py --output build/python-package-check
```

Use a fresh output directory and installed Python build tools. This constructs
an sdist, builds its wheel with network lookup disabled, installs into an isolated
target and runs adapter tests plus the standalone example. It never installs
globally. Native adapter changes must remain registered in the binding manifest
and survive this source-archive build. Ordinary C builds do not need Python.

## Other validation guides

| Task | Guide |
| --- | --- |
| Independent references, parser limits and fuzzing | [Numeric testing](testing/NUMERIC_TESTING.md) |
| Browser setup, CI, sanitizers and C/C++ consumers | [Browser tests and CI](testing/WEB_AND_CI.md) |
| Benchmark setup and measurement rules | [Performance](testing/PERFORMANCE.md), [tool index](../benchmarks/README.md) |
| Documentation links, examples and generated output | [Documentation maintenance](project/DOCUMENTATION.md) |

Direct source checks are `node scripts/check_architecture.cjs`,
`node tests/test_architecture.cjs` and `node scripts/check_docs.cjs`.
The guard uses isolated fixtures without modifying working sources. Historical
test counts are preserved with their recorded revisions, not advertised as the
current total.

## Local output retention

`build/` is ignored by Git: it can be recreated, but its old binaries and raw
measurements are not stored in repository history. Keep one current ordinary
test build and one benchmark build while they are useful; remove superseded
build trees, reports, downloads and scratch scripts after validation. Preserve
any running executable and uncommitted source before cleaning. Reusable scripts,
independent fixtures and selected benchmark evidence belong in the repository.
