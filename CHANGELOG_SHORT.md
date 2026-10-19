# Changelog highlights

The main changes in a few words. See the [full changelog](CHANGELOG.md) for
details and the [2.4.0 release record](docs/project/RELEASE_2_4.md) for validation.

## Unreleased

No changes yet.

## 2.4.0 — 19 October 2026

- Solve exact linear/quadratic equations with rational, radical and complex roots.
- Find one or multiple real roots on a chosen interval, with checked coordinates,
  explicit work limits and incomplete-discovery diagnostics.
- Edit equations using shared searchable functions/constants and mode-specific examples.
- Copy full checked points, inspect residuals and open real solutions in Graphs.
- Preserve inputs/results across tools and languages with responsive mobile editing.
- Expand independent correctness, failure, benchmark and extracted-package checks.

Numerical discovery is bounded and does not guarantee all roots or certified error bounds.

## 2.3.0 — 17 October 2026

- Plot up to four colored functions with mouse/touch pan, zoom and expanded views.
- Automatically choose continuous curves or supported integer-domain points.
- Inspect cached coordinates and C-verified intercept/intersection markers.
- Improve extrema, valleys, narrow peaks, domain gaps and dense oscillations.
- Reuse bounded samples and faster intersection refinement to reduce repeated work.
- Add direct function editing, searchable shared functions/constants and a color palette.
- Preserve completed graphs across tools/languages; keep mobile controls compact.
- Split the guide by tool and show calculation timing above 0.5 seconds.
- Expand independent correctness, failure, efficiency and browser regression checks.

Graphs remain approximate; finite sampling cannot guarantee every feature or root.

## 2.2.1 — 12 October 2026

- Accelerate large balanced multiplication with budget-aware Karatsuba.

- Benchmark private Karatsuba multiplication and grouped product-tree factorials.
- Wait for two seconds of idle editing before calculating web previews.

- Faster high-precision logarithms with bounded AGM and adaptive precision;
  existing low-precision behavior and resource budgets remain.

- Allow larger factorial inputs under existing time and memory budgets.
- Reduce ln/exp allocations; 5000-place exp now fits existing resource limits.
- Cut GCD allocation churn so large real/complex fractions fit existing limits.
- Expand benchmark coverage and refresh performance/limit diagnostics.
- Further reduce complex function allocations by sharing sinh/cosh work.
- Reduce complex trigonometry allocations by sharing scalar sine/cosine work.
- Avoid full integer text conversion when choosing decimal precision.
- Reduce temporary decimal allocations and improve high-precision limit headroom.
- Speed up integer division and dependent GCD, logarithm and trigonometry work.
- Give large factorials more time-limit headroom by grouping small factors.
- Speed up integer-to-decimal conversion and large permutations/combinations.
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
