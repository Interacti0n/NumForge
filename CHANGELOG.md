# Changelog

All notable changes to NumForge are documented in this file. The project uses
[Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-09-01

### Added

- Stable opaque C APIs for signed arbitrary-precision `BigInt` and exact
  base-10 `BigDecimal` values.
- BigInt conversion, comparison, arithmetic, powers, number theory, bitwise
  operations, shifts, probable-prime testing, and perfect-square testing.
- BigDecimal conversion, comparison, exact arithmetic, rescaling, division,
  and six explicit rounding modes.
- Exact-decimal expression calculator with constants, implicit multiplication,
  powers, square, cube, factorial, configurable output precision, scientific
  notation, and bounded evaluation.
- Bilingual loopback-only browser calculator and documented local HTTP API.
- Installable CMake package exposing `NumForge::numforge`, public headers,
  command-line tools, documentation, and the MIT license.

### Reliability

- Unit, deterministic property, allocation-failure, parser/evaluator,
  web-adapter, real HTTP server, and installed-package consumer tests.
- Warnings-as-errors builds on GCC and MSVC, Linux ASan/UBSan/LeakSanitizer,
  Windows CI, and a dedicated 32-bit Linux build.
- Strong destination-unchanged guarantees for fallible numeric operations and
  explicit out-of-memory status propagation.

### License

- Released under the MIT License, copyright 2026 Interacti0n.

[1.0.0]: https://github.com/Interacti0n/NumForge/releases/tag/v1.0.0
