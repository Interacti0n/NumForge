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
  [session variables](VARIABLES.md),
  session history, stable random previews and a bounded loopback HTTP API.
- [Unit-conversion C foundation](UNITS.md): a sourced 233-unit catalogue,
  compatibility checks, exact rational factors and guarded angle projection.
  [Local HTTP catalogue and conversion](UNIT_HTTP_API.md) with read-only session
  access and a [SK/EN browser converter](UNIT_CONVERTER.md) are available.
  Quantity arithmetic remains future work.
- CMake installation and C/C++ consumer support; numerical, property,
  allocation-failure, integration, browser and fuzz-smoke checks.
- Formatting/cache/HTTP/memory, BigInt and BigDecimal/math benchmarks with independent
  result checks and repeated local Release baselines.

For exact supported functions and limits, use [API.md](API.md). Screenshots in
the README show development code and may be newer than a published release.

## Next accuracy and optimization work

Direct BigDecimal arithmetic and higher-function/constant measurements are
implemented with independent references through 2,000 significant digits;
see [Decimal/math measurements](DECIMAL_MATH_BENCHMARKS.md). The historical stress matrix
exposed tangent-pole cancellation, now addressed by adaptive reduction and
strict independent regressions through 2,000 digits. General correctly rounded
approximate functions remain a separate goal. Some high-precision cases exceed
the collection deadline and need dedicated longer measurements.

Short scientific formatting now uses bounded prefix conversion with exact
fallback. Buffer reuse, product-tree factorial, normalized division and
Karatsuba remain candidates for measured follow-ups.
Existing experiments are described with their measured tradeoffs in
[BIGINT_BENCHMARKS.md](BIGINT_BENCHMARKS.md).

## Later, according to use cases

- Stable serialization and language bindings; BigInt numeral bases 2–36 are implemented.
- User functions (session variables are implemented), followed by equation solving, numerical
  analysis and graphing once their semantics and resource limits are defined.
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
