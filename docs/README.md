# Documentation guide

Start with your task. Folders follow the same categories as this page; API and
testing entry points remain directly under `docs/`.

## Use NumForge — `guides/`

- [Browser calculator](guides/CALCULATOR_WEB.md): controls, results and precision.
- [Variables](guides/VARIABLES.md): assignments, deletion, snapshots and lifetime.
- [Unit converter](guides/UNIT_CONVERTER.md): categories, preview, swap and history.
- [C library guide](guides/LIBRARY_GUIDE.md): build, install and consume the library.
- [Developer SDK](guides/SDK.md): prebuilt C/C++ library, toolchains and quick start.

## Look up a contract — `reference/`

Start with the [API guide](API.md), or open a specific reference:

- [Public C API](reference/C_API.md): numeric types, functions and ownership.
- [Calculator expressions](reference/CALCULATOR_EXPRESSIONS.md): syntax and calls.
- [Local HTTP API](reference/HTTP_API.md): evaluation, sessions, revisions and errors.
- [Session HTTP API](reference/SESSION_HTTP_API.md): saved values, pagination, lifecycle and conversion history.
- [Units](reference/UNITS.md) and [Quantity](reference/QUANTITIES.md): conversion and dimensions.
- [Unit catalogue](reference/UNIT_CATALOG.md): 233 units, localized names and sources.
- [Unit HTTP API](reference/UNIT_HTTP_API.md): catalogue and conversion routes.
- [Web/API coverage](reference/WEB_API_PARITY.md): implemented calls and remaining gaps.

## Understand the code — `design/`

- [Architecture](design/ARCHITECTURE.md): dependency flow and source map.
- [Application design](design/APPLICATION_DESIGN.md): sessions, adapters and future storage.
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
- [BigInt measurements](benchmarks/BIGINT_BENCHMARKS.md).
- [Decimal/math measurements](benchmarks/DECIMAL_MATH_BENCHMARKS.md).

Measurements retain their dates, environments and limitations. They are evidence
for specific source snapshots, not promises about every machine.

## Plan and maintain — `project/`

- [Roadmap](project/ROADMAP.md): available features and future work.
- [Presentation](project/PROJECT_PRESENTATION.md): README assets and screenshots.
- [Release packaging](project/RELEASE_PACKAGING.md): archives and automation.
- [Release 2.1 record](project/RELEASE_2_1.md): scope, validation and limitations.
- [Short changelog](../CHANGELOG_SHORT.md) or [full changelog](../CHANGELOG.md).

Screenshots live in `images/`. Generated output and benchmark runs stay in
ignored build directories. Run documented shell commands from the repository
root unless a guide explicitly says otherwise.
