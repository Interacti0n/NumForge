# Release 2.2.1 record

NumForge 2.2.1 — 12 October 2026. This patch release primarily integrates the
measured arithmetic and allocation improvements following 2.2.0. Public numeric
C signatures and ownership contracts remain unchanged.

## Scope

- Budget-aware Karatsuba for large balanced products, normalized limb division,
  grouped/product-tree factorials, checked admission and reusable split scratch.
- Exact large-coefficient zero normalization, direct coefficient/digit handling
  and reduced permutation, combination and GCD allocation traffic.
- Reusable decimal ln/exp workspaces, bounded theta/AGM logarithms and shared
  scalar trigonometric/hyperbolic work inside complex functions.
- Factorials no longer have the practical 100000-input cap. Counter-range and
  application time/memory/output budgets still apply; arbitrary inputs are not
  guaranteed to complete within five seconds.
- Calculator and Units previews wait for two seconds of idle editing;
  confirmation remains immediate and cancels pending previews.
- Expanded independent benchmarks, references and failure checks, exhaustive
  allocation test shards, sanitizer cleanup and stable browser synchronization.

See the [full changelog](../../CHANGELOG.md) and
[dated readiness audit](RELEASE_READINESS_2026_10_12.md). Further factorial
research remains deferred. Transcendental functions retain their documented
approximation contracts; universal correct rounding is not claimed.

## Validation and publication

The preceding 16da8ce CI run passed all ten non-browser jobs, including Linux
GCC/Clang sanitizers, fuzz campaigns, Windows, Linux 32-bit, ARM64 platforms,
coverage and benchmark checks. Chromium reported 166/166 passed before its
overall job deadline cancelled the run. The job deadline is now twenty minutes,
including setup, without changing individual test timeouts.

Local fresh Release checks and relocated Windows application/SDK archive checks
are recorded in the readiness audit. Complete release delivery requires a successful
CI run for the final 2.2.1 source and verified Windows/Linux archives from the
exact release tag. Publication and asset results will be reported after those
jobs complete.
