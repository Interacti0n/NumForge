# Documentation guide

Start with your task. Folders follow the same categories as this page; API and
testing entry points remain directly under `docs/`.

## Use NumForge — `guides/`

- [Browser calculator](guides/CALCULATOR_WEB.md): controls, results and precision.
- [Remote hosting](guides/HOSTING.md): HTTPS tunnels, reverse proxies and capacity.
- [Variables](guides/VARIABLES.md): assignments, deletion, snapshots and lifetime.
- [Function graphs](guides/GRAPHS.md): function/color controls, typed gaps, smooth curves and axes.
- [Unit converter](guides/UNIT_CONVERTER.md): categories, preview, swap and history.
- [C library guide](guides/LIBRARY_GUIDE.md): build, install and consume the library.
- [Developer SDK](guides/SDK.md): prebuilt C/C++ library, toolchains and quick start.

## Look up a contract — `reference/`

Start with the [API guide](API.md), or open a specific reference:

- [Public C API](reference/C_API.md): numeric types, functions and ownership.
- [BigComplex](reference/BIGCOMPLEX.md): complex arithmetic, principal branches, precision and display forms.
- [Calculator expressions](reference/CALCULATOR_EXPRESSIONS.md): syntax and calls.
- [Local HTTP API](reference/HTTP_API.md): evaluation, sessions, revisions and errors.
- [Session HTTP API](reference/SESSION_HTTP_API.md): saved values, pagination, lifecycle and conversion history.
- [Units](reference/UNITS.md) and [Quantity](reference/QUANTITIES.md): conversion and dimensions.
- [Unit catalogue](reference/UNIT_CATALOG.md): 233 units, localized names and sources.
- [Unit HTTP API](reference/UNIT_HTTP_API.md): catalogue and conversion routes.
- [Graph sampling HTTP API](reference/GRAPH_HTTP_API.md): bounded grids, point errors and session isolation.
- [Web/API coverage](reference/WEB_API_PARITY.md): implemented calls and remaining gaps.

## Understand the code — `design/`

- [Architecture](design/ARCHITECTURE.md): dependency flow and source map.
- [Application design](design/APPLICATION_DESIGN.md): sessions, adapters and future storage.
- [Parametric expressions](design/PARAMETRIC_EXPRESSIONS.md): implemented C application foundation for future graphs/solvers, snapshots and shared budgets.
- [Account proposal](design/ACCOUNT_DESIGN.md): planned C/HTTP login, registration, recovery and storage contracts; not implemented.
- [BigInt](design/BIGINT_DESIGN.md) and [BigDecimal](design/BIGDECIMAL_DESIGN.md): representation and arithmetic.
- [Calculator](design/CALCULATOR_DESIGN.md): modules, evaluation and budgets.
- [Expression parser](design/EXPRESSION_PARSER.md): grammar and name resolution.
- [Browser/result design](design/WEB_DESIGN.md): layout, precision, caches and history.
- [Web assets](../web/README.md): editable assets and embedding.

## Verify a change — `testing/`

The [testing guide](TESTING.md) gives the complete local command and links to
numerical references, session/unit regressions, browser/CI checks and profiling.

## Investigate performance — `benchmarks/`

- [Harnesses and reference data](../benchmarks/README.md).
- [Formatting/cache measurements](benchmarks/BENCHMARKS.md).
- [Graph release audit, 15 October 2026](benchmarks/GRAPH_RELEASE_AUDIT_2026_10_15.md): independent geometry checks, before/after timings, sustained cache tests and remaining release gates.
- [Smooth graph zoom](benchmarks/GRAPH_SMOOTH_ZOOM_2026_10_13.md): derivative anchors, interpolation accuracy and prepared detail.
- [BigInt measurements](benchmarks/BIGINT_BENCHMARKS.md).
- [Karatsuba experiment](benchmarks/KARATSUBA_EXPERIMENT_2026_10_11.md): private threshold measurements, whole-product references and bounded scratch.
- [Karatsuba integration](benchmarks/KARATSUBA_INTEGRATION_2026_10_11.md): compiler/shape crossover, budget-aware dispatch and composition regressions.
- [Factorial tree integration](benchmarks/FACTORIAL_TREE_INTEGRATION_2026_10_11.md): reusable bounded workspace, complete references and calculator/HTTP verification through 200000.
- [Factorial stress diagnostics](benchmarks/FACTORIAL_STRESS_2026_10_11.md): admission boundary, real scratch limits and normalization cost through 300000.
- [Exact zero normalization](benchmarks/ZERO_NORMALIZATION_2026_10_11.md): inverse division, bounded scratch and whole-request regressions.
- [Factorial admission bounds](benchmarks/FACTORIAL_ADMISSION_2026_10_12.md): checked range/workspace planning and complete requests at the new boundary.
- [Split factorial scratch](benchmarks/SPLIT_WORKSPACE_2026_10_12.md): bounded final experiment, wider admission and full-request validation.
- [Decimal/math measurements](benchmarks/DECIMAL_MATH_BENCHMARKS.md).
- [Performance audit, 10 October 2026](benchmarks/PERFORMANCE_AUDIT_2026_10_10.md): current measurements, five-second risks and optimization priorities.
- [Logarithm algorithm review](benchmarks/LOGARITHM_ALGORITHM_REVIEW_2026_10_11.md) and
  [theta/AGM prototype](benchmarks/LOGARITHM_AGM_PROTOTYPE_2026_10_11.md): experimental path, independent checks and production prerequisites.
- [Factorial budgets and text conversion](benchmarks/FACTORIAL_BUDGETS_2026_10_11.md): removal of the practical cap, cancellation and calculation versus formatting timings.
- [Bounded AGM integration](benchmarks/LOGARITHM_AGM_INTEGRATION_2026_10_11.md): production logarithm selection, interval error bounds and adaptive precision.

Measurements retain their dates, environments and limitations. They are evidence
for specific source snapshots, not promises about every machine.

## Plan and maintain — `project/`

- [Roadmap](project/ROADMAP.md): available features and future work.
- [Presentation](project/PROJECT_PRESENTATION.md): README assets and screenshots.
- [Release packaging](project/RELEASE_PACKAGING.md): archives and automation.
- [Release readiness, 12 October 2026](project/RELEASE_READINESS_2026_10_12.md): local regression and archive checks before the next release.
- [Release 2.1 record](project/RELEASE_2_1.md): scope, validation and limitations.
- [Release 2.2 record](project/RELEASE_2_2.md): stability scope and publication checks.
- [Release 2.2.1 record](project/RELEASE_2_2_1.md): performance scope and validation.
- [Short changelog](../CHANGELOG_SHORT.md) or [full changelog](../CHANGELOG.md).

Screenshots live in `images/`. Generated output and benchmark runs stay in
ignored build directories. Run documented shell commands from the repository
root unless a guide explicitly says otherwise.

See the [adaptive graph API](reference/ADAPTIVE_GRAPH_HTTP_API.md) for the automatic web plotter.
