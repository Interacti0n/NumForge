# Changelog highlights

The main changes in a few words. See the [full changelog](CHANGELOG.md) for
details and the [2.2 release record](docs/project/RELEASE_2_2.md) for validation.

## Unreleased

- Stabilize CI with exhaustive complex test shards, deterministic resource-limit
  checks and browser synchronization; prevent small desktop layout shifts.

## 2.2.0 — 10 October 2026

- Public BigComplex and BigRationalComplex C APIs, integrated into CLI and web.
- Principal complex roots, powers, logarithms, trigonometry and inverse functions.
- Complex display forms, automatic domain promotion and factorials up to 100000.

- Improve complex stability and intermediate growth; add 384 stress references.

- Add complex memory probes and reduce repeated work in tan/tanh quotients.

- Preserve tiny complex logarithm components for nearly equal inputs;
  extend the independent complex oracle to 900 cases.

- Complex sum, product and mean with exact rational components.
- Independent complex accuracy references and timing diagnostics.

- Authoritative session/history HTTP API.
- Typed values, function catalogue and explicit lifecycle.
- Server-owned conversion history with safe retries.

## 2.1.0 — 8 October 2026

- Unit converter in Slovak and English.
- Sourced catalogue of 233 units.
- Exact `convert(...)` expressions.
- `qty(...)` and dimensional arithmetic.
- Session variables and easy deletion.
- History/Variables tabs and accessible scrolling.
- BigInt input/output in bases 2–36.
- Faster short output of huge numbers.
- More accurate tangent near poles.
- Independent arithmetic and math benchmarks.
- Separate calculation, application and HTTP layers.
- Portable Windows/Linux release automation.
- Separate C/C++ SDK downloads (added after publication).
- Refreshed README, UI and documentation.

## 2.0.0 — 28 September 2026

- Exact rational calculator values.
- Scientific functions and high-precision constants.
- Confirmed sessions, previews and history.
- Bilingual browser calculator and CLI.
- Independent numerical reference tests.
- Wider platform CI and package checks.

## 1.0.0 — 1 September 2026

- Initial C17 BigInt and BigDecimal library.
- CMake builds, tests and installation.
