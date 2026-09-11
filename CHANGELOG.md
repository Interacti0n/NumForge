# Changelog

All notable changes to NumForge are documented in this file. The project uses
[Semantic Versioning](https://semver.org/).

## Unreleased

### Changed

- Separate bounded HTTP byte framing from socket I/O and add direct regression
  tests plus formatter/HTTP fuzz harnesses and portable replay coverage.
- Add pinned Playwright Chromium tests for SK/EN calculation, precision,
  keypad, clipboard, navigation, stale responses and transport-error recovery,
  with a dedicated CI job and failure artifacts.
- Document extreme scientific output as display text, not guaranteed
  round-trip serialization outside the input parser's exponent range.

- Preserve exact terminating calculator quotients, including after fraction
  reduction, regardless of working precision. Non-terminating division keeps
  significant-digit rounding; public fixed-scale division is unchanged.

- Reject repeated decimal separators and adjacent numeric tokens instead of
  silently treating malformed numbers as implicit products. Existing constant,
  parenthesized and postfix implicit products retain their precedence.

- Non-terminating calculator division uses working significant digits,
  preserving tiny values such as `1E-40 / 3`.
- CLI and HTTP share a bounded complete pipeline: five-second monotonic budget,
  64 MiB cumulative allocations, 128 KiB per allocation, 65536 output bytes,
  and selectable output precision of 0 through 10000. Full output remains
  available but is not infinite intermediate precision.
- Expensive BigInt loops support cooperative cancellation within application
  scopes; ordinary public numeric calls remain unrestricted.
- HTTP uses absolute two-second receive/send deadlines and JSON 408/413 errors.
- Web requests are invalidated on input changes; stale responses and copy
  timers cannot overwrite a newer result. Non-JSON/network errors are handled.

### Fixed

- Normalize multiplication coefficients before checking the combined scale,
  allowing representable extreme-scale results without weakening overflow checks.
- Strip trailing decimal zeros in blocks rather than repeated general division,
  keeping high-precision terminating quotients practical.

- Reject decimal formatting sizes that cannot include the terminating NUL
  byte, including compact inputs that overflow `size_t` on 32-bit systems.
- Allow division scales to cancel before reporting intermediate overflow.
- Validate factorial bounds and non-negative integer operands before expanding
  compact decimal values into strings.
- Reserve exclusive Windows server ports, reject unsupported HTTP transfer
  encodings, and accept canonical browser origins on port 80.
- Preserve working relative links in installed documentation.
- Clarify the soft evaluation budget and intermediate division rounding.

### Tests

- Add real CLI process regression tests and optional installed-package C++
  consumers, including a Windows Release installation job.
- Add GCC line/branch coverage artifacts and a Clang sanitizer/libFuzzer CI
  profile, with bounded parser/numeric harnesses and portable replay tests.
- Extend opt-in benchmarks with operand-size sweeps and phase allocation
  counts/volume. Instrumentation is absent from ordinary production builds.

- Add calculator grammar and rounding-contract regressions, a deterministic
  random-byte parser smoke test, and 5792 independent numerical oracle cases.
- Require Node.js test suites in CI; add opt-in parse/evaluate/format benchmarks.

- Add deterministic cancellation checkpoints, significant-division regressions,
  extreme multiplication/aliasing cases, slow HTTP input and oversized-body tests.
- Add optional Node.js tests of the actual SK/EN embedded UI scripts, without
  introducing an application runtime dependency or npm packages.

- Add guarded 32-bit allocation regressions, extreme-scale division and
  calculator cases, and wider real HTTP smoke coverage.
- Test large-factorial correctness independently of the default five-second
  application budget, retaining CTest's process timeout.

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
