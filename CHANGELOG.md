# Changelog

User-visible changes only. API details live in the [reference](docs/API.md);
published artifact checks live in [release verification](docs/project/RELEASES.md).
Earlier detailed development notes remain in Git history.

## Unreleased

- Exact simple linear regression with intercept, independently callable in C and Python.
- Bounded finite-interval Simpson integration in C/Python, with approximate error
  estimates, evaluation/refinement limits and domain diagnostics.

- Shared CMake build/test presets and opt-in clang-tidy correctness checks.

- Standalone C expression engine with prepared expressions, parameters, typed
  results and operation budgets; numeric-only use stays independent of the engine.
- Public exact polynomial, rational matrix, complete rectangular linear-system,
  descriptive statistics and paired covariance APIs.
- Public exact grids, adaptive sampling/curve policies, phase anchors and bounded
  single/multiple-root searches, shared with graph/equation applications.
- Experimental source-built Python adapter over the C kernels, including exact
  inputs, high-precision roots, prepared expressions, modules, grids and root scans.
- Modular source ownership, shared private helpers and leaner documentation,
  benchmarks and tests. Public interfaces and numerical guarantees are retained.
- Optional MkDocs/Doxygen documentation with verified C/Python examples.

## 2.4.0 — 19 October 2026

- Equation application: exact linear/quadratic solving, bounded numerical root
  discovery, rational coordinates and guarded decimal previews.
- Shared expression editing, localized diagnostics, copy/graph actions and
  retained same-tab work. Numerical discovery remains potentially incomplete.

## 2.3.0 — 17 October 2026

- Four-function graphs with pan/zoom, bounded adaptive screen sampling, discrete
  points and original-expression marker verification.
- Bounded caches, background preparation and same-tab graph restoration.

## 2.2.1 — 12 October 2026

- Faster limb division, budget-aware Karatsuba and bounded factorial trees.
- Less allocation traffic in decimal/GCD/complex work; bounded AGM logarithms.
- Resource limits still govern completion; no fixed practical factorial cap.

## 2.2.0 — 10 October 2026

- Public complex and rational-complex types, principal roots, powers, logarithms,
  trigonometric/inverse functions and stable extreme-magnitude operations.
- Complex display forms, independent accuracy references and memory probes.

## 2.1.0 — 8 October 2026

- Unit converter, sourced unit catalogue, dimensional quantities and variables.
- Bases 2–36, faster short output and improved tangent accuracy near poles.
- Separate application/transport ownership and portable application packaging.
  Developer SDK downloads were added after publication.

## 2.0.0 — 28 September 2026

- Exact rational calculator values, scientific functions and precision-aware constants.
- Confirmed sessions, history, bilingual CLI/browser applications and wider CI.

## 1.0.0 — 1 September 2026

- C17 BigInt/BigDecimal library, CMake builds, tests and installation.
