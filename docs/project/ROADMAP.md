# Current scope and roadmap

This page distinguishes implemented behavior from future work. It is a direction
of development, not a schedule or promise of a particular release.

## Available today

- C17 BigInt, BigDecimal and BigRational APIs with opaque types, explicit
  ownership, documented aliasing and strong failure guarantees.
- Exact integer/decimal/rational arithmetic; configurable precision/rounding for
  approximations; roots, logarithms, trigonometric/hyperbolic functions,
  constants, statistics and selected integer operations.
- Shared CLI/local browser calculator with English/Slovak UI, confirmed `ans`,
  [session variables](../guides/VARIABLES.md),
  session history, stable random previews and a bounded loopback HTTP API.
- [Unit-conversion C foundation](../reference/UNITS.md): a sourced 233-unit catalogue,
  compatibility checks, exact rational factors and guarded angle projection.
  [Local HTTP catalogue and conversion](../reference/UNIT_HTTP_API.md) with read-only session
  access and a [SK/EN browser converter](../guides/UNIT_CONVERTER.md) are available.
  [Quantity expressions](../reference/QUANTITIES.md) support dimension-checked arithmetic,
  powers/roots, temperature points/intervals and selected functions.
- CMake installation and C/C++ consumer support; numerical, property,
  allocation-failure, integration, browser and fuzz-smoke checks.
- Formatting/cache/HTTP/memory, BigInt and BigDecimal/math benchmarks with independent
  result checks and repeated local Release baselines.

For exact supported functions and limits, use [API.md](../API.md). Screenshots in
the README show development code and may be newer than a published release.

## Next accuracy and optimization work

Direct BigDecimal arithmetic and higher-function/constant measurements are
implemented with independent references through 2,000 significant digits;
see [Decimal/math measurements](../benchmarks/DECIMAL_MATH_BENCHMARKS.md). The historical stress matrix
exposed tangent-pole cancellation, now addressed by adaptive reduction and
strict independent regressions through 2,000 digits. General correctly rounded
approximate functions remain a separate goal. Some high-precision cases exceed
the collection deadline and need dedicated longer measurements.

Short scientific formatting now uses bounded prefix conversion with exact
fallback. Buffer reuse, product-tree factorial, normalized division and
Karatsuba remain candidates for measured follow-ups.
Existing experiments are described with their measured tradeoffs in
[BIGINT_BENCHMARKS.md](../benchmarks/BIGINT_BENCHMARKS.md).

## Planned tool development

First close the [web/API coverage gaps](../reference/WEB_API_PARITY.md): session variable and
history lists, explicit lifecycle commands, discoverable functions and converter
history operations. New mathematical/session features should expose their HTTP
contract alongside the web implementation.

1. Share a parameterized expression evaluator between graphs and solvers: parse
   once, bind a local variable, snapshot session values and bound the total work.
   Sampling and solving must leave calculator state unchanged.
2. Add basic 2D `y = f(x)` graphs with ranges, axes, discontinuity handling,
   accessible point data and responsive controls.
3. Add real linear/quadratic equation solving, then bounded numerical root
   finding on a specified interval, with residuals and clear convergence limits.
4. Add session user functions, local parameters and definition management,
   followed by their integration with graphs and solvers.
5. Extend Quantity with general compound unit strings, named derived units,
   further functions, structured HTTP snapshots and a possible public C API.
   Dimensional equations and graph axes need explicit unit contracts first.

These stages are a proposed order, not implemented features or release dates.
Matrices/vectors and dataset statistics follow once their initial scope is set.
Measured arithmetic optimizations can proceed independently of these tools.

## Later, according to use cases

- Stable serialization and language bindings; BigInt numeral bases 2–36 are implemented.
- Further numerical analysis and graphing beyond the basic tools above.
- Improvements to directed rounding of approximate functions, supported by
  rigorous error bounds or adaptive precision and independent references.
- Additional constants and mathematical types/functions where there is a
  concrete application need.

Graph, equation, sign-in and sign-up navigation entries currently
open informational pages. Accounts, synchronization and those tools are not
implemented. The app has no Premium feature or subscription system.

## Distribution and project presentation

The published release currently provides source archives. Portable Windows/Linux
archive packaging and automatic uploads on release publication are prepared in
[RELEASE_PACKAGING.md](RELEASE_PACKAGING.md); archives still need validation on
both platforms and publication. A hosted interactive demo needs a hosting plan.
Publishing the current loopback server directly is insufficient: public service
work requires worker isolation, queues, rate limiting and hard resource limits.

README screenshots, architecture and benchmark highlights are maintained in
[PROJECT_PRESENTATION.md](PROJECT_PRESENTATION.md). A portfolio connection is
deferred until the portfolio is ready; no connection is configured now.
