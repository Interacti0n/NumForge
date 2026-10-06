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
  session history, stable random previews and a bounded loopback HTTP API.
- CMake installation and C/C++ consumer support; numerical, property,
  allocation-failure, integration, browser and fuzz-smoke checks.
- Formatting/cache/HTTP/memory and BigInt arithmetic benchmarks with independent
  result checks and repeated local Release baselines.

For exact supported functions and limits, use [API.md](API.md). Screenshots in
the README show development code and may be newer than a published release.

## Next measurement work

Measure BigDecimal scale alignment, normalization and finite/recurring division
separately from the BigInt core. Expand direct higher-function and constant
measurements to hundreds/thousands of digits, including large angles and
difficult domains. These scenarios need new independently generated, sufficiently
precise references. This block is prepared but not implemented yet.

Use those measurements with the completed suites to select optimizations.
Prefix formatting, buffer reuse, product-tree factorial, specialized squaring,
normalized division and Karatsuba are candidates, not implemented promises.
Existing experiments are described with their measured tradeoffs in
[BIGINT_BENCHMARKS.md](BIGINT_BENCHMARKS.md).

## Later, according to use cases

- Input/output in numeral bases 2–36, stable serialization and language bindings.
- Session variables and user functions, followed by equation solving, numerical
  analysis and graphing once their semantics and resource limits are defined.
- Improvements to directed rounding of approximate functions, supported by
  rigorous error bounds or adaptive precision and independent references.
- Additional constants and mathematical types/functions where there is a
  concrete application need.

Graph, equation, unit-conversion, sign-in and sign-up navigation entries currently
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
