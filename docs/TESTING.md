# Testing and performance baselines

## Run the complete local suite

```sh
cmake -P cmake/LocalBuild.cmake
```

The helper configures Release with warnings as errors, builds and runs CTest.
Its ignored `build/local-tests-*` directory depends on the absolute checkout
path. Moving the checkout creates a new build tree; it never deletes old ones.
Use the printed path for direct CTest commands. A bare `ctest --test-dir build`
can run stale generated tests from another checkout location.

The first test configuration fetches pinned Unity using Git/network access.
Node.js enables additional architecture, CLI, numeric-oracle and web-script
checks. CI sets `NUMFORGE_REQUIRE_NODE_TESTS=ON` so those checks cannot silently
skip. Testing tools are not application runtime dependencies.

## Build without tests and open the apps

```sh
cmake -DNUMFORGE_LOCAL_TESTS=OFF -P cmake/LocalBuild.cmake
```

This uses a separate reusable `build/local-apps-*` tree. On Windows, add
`-DNUMFORGE_LOCAL_RUN=both` before `-P` to launch CLI and web after a successful
build; `cli`/`web` select one. The helper opens a visible app console. Change the
default port with `-DNUMFORGE_LOCAL_PORT=8767`; occupied ports are rejected.
Build-only mode works on other hosts. No tunnel is started.

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
| Summaries and covariance | `statistics_tests`, Python statistics/covariance tests | [Statistics contract](reference/STATISTICS_API.md) |
| Grids, refinement, curve/phase policies | `numerical_grid_tests`, `numerical_refinement_tests`, `numerical_curve_tests`, `numerical_phase_tests` | [Numerical contracts](reference/NUMERICAL_API.md) |
| Public root kernels | `numerical_tests`, `numerical_scan_tests` | [Root/scan meaning](reference/ROOT_API.md#use-and-result-meaning) |
| Equation application/HTTP | `equation_tests`, `numeric_root_tests`, `numeric_root_scan_tests`; affected browser equations | [Equation API](reference/EQUATION_HTTP_API.md), [numerical HTTP](reference/NUMERIC_ROOT_HTTP_API.md) |
| Sessions, units, client ownership | Application, session, unit, cache and HTTP suites | [Regression guide](testing/REGRESSIONS.md) |
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
partial-limit diagnostics. See the [structural audit](project/AUDIT.md)
for a recorded example of bounded cross-layer verification.

## Installed packages and Python

Standalone consumers must compile against installed headers/targets outside the
source tree. Validate numeric-only and engine-enabled installations separately;
`NUMFORGE_TEST_ENGINE=OFF` is for an intentionally numeric-only installation.
The [browser/CI guide](testing/WEB_AND_CI.md) owns the C/C++ package commands.
SDK examples in `examples/sdk/` exercise supported operations through those targets.

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
