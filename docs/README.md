# Documentation guide

## Build and use

- [Library guide](LIBRARY_GUIDE.md): build, install and consume the C library.
- [Units](UNITS.md): public conversion foundation, compatibility and precision.
- [Quantity expressions](QUANTITIES.md): typed arithmetic, dimensions, temperatures and session values.
- [Unit catalogue](UNIT_CATALOG.md): SK/EN names, factors and primary-source provenance.
- [Unit HTTP API](UNIT_HTTP_API.md): catalogue, expression conversion and read-only session access.
- [Browser unit converter](UNIT_CONVERTER.md): categories, precision, preview/confirmation and session behavior.
- [Variables](VARIABLES.md): session assignments, exact values and lifetime.
- [API overview](API.md): numeric contracts, calculator syntax and local HTTP API.
- [Testing](TESTING.md): regression checks, sanitizers, fuzzing and browser tests.
- [Release packaging](RELEASE_PACKAGING.md): portable archives and release automation.

## Understand the implementation

- [Architecture](ARCHITECTURE.md): dependency boundaries and source map.
- [BigInt design](BIGINT_DESIGN.md): integer representation and algorithms.
- [BigDecimal design](BIGDECIMAL_DESIGN.md): scales, precision and rounding.
- [Calculator design](CALCULATOR_DESIGN.md): parsing, evaluation and sessions.
- [Browser assets](../web/README.md): editable UI and embedded-asset generation.

## Measure and plan

- [Benchmark tools](../benchmarks/README.md): harnesses, reference data and recording.
- [Formatting/cache measurements](BENCHMARKS.md): display costs, cache and memory.
- [BigInt measurements](BIGINT_BENCHMARKS.md): arithmetic and algorithm experiments.
- [Decimal/math measurements](DECIMAL_MATH_BENCHMARKS.md): decimal operations,
  scientific functions, independent references and known limitations.
- [Roadmap](ROADMAP.md): planned work and prerequisites.
- [Project presentation](PROJECT_PRESENTATION.md): screenshots and README maintenance.

Screenshots used by the README live in `images/`. Recorded benchmark output and
generated build files belong in ignored build directories, outside these docs.
