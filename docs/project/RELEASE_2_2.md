# Release 2.2 record

NumForge 2.2.0 — 10 October 2026. Publication and final CI are tracked at
[the GitHub release](https://github.com/Interacti0n/NumForge/releases/tag/v2.2.0).
The complete feature scope is in [the changelog](../../CHANGELOG.md).

## Complex stability stage

- Exact common-exponent scaling for division, magnitude, square-root exactness
  checks and logarithms whose squared input would overflow the decimal scale.
- Bounded magnitude rounding when an irrationality witness and a tail bound
  permit it; exact finite roots and inconclusive cases retain the exact path.
- Scaled tangent for large imaginary components, preserving tiny nonzero tails.
- Input-sensitive logarithm precision for general complex powers.
- 1284 independent complex references, including 384 stress cases, and 584
  isolated memory probes. Explicit tests cover directed rounding, ties, exact
  finite roots and allocation failures in new magnitude/logarithm paths.

Public C signatures and ownership remain unchanged in this stage. The shared
numeric C implementation serves the calculator and HTTP API. Approximate
transcendental results still have no universal correctly-rounded guarantee;
arbitrary cancellation and exact intermediate growth remain future work.

## Local validation, 9 October 2026

- MSVC Release with warnings as errors: all 36 CTest checks passed across the
  full run and focused reruns. The initial run exposed three stale tests that
  still expected real-domain errors; these now assert the supported complex
  results or updated help text. Exhaustive numeric/calculator allocation-fault
  checks passed; new magnitude/logarithm fault paths also passed separately.
  One final parallel CLI run timed out starting its child process; the isolated
  rerun passed. Keep final release CI as an independent check.
- Independent complex references: 1284/1284; isolated memory probes: 584/584,
  with zero tracked live allocations after cleanup.
- Chromium desktop/mobile and HTTP suite: 161/161.
- Installed public SDK: both external C and C++ consumers passed.
- Documentation: 45 Markdown files and 232 local links/anchors verified.

These are local Windows results, not a completed release CI or archive audit.

## Publication checks

The version and changelog are closed for 2.2.0. Run CI against the final
pushed commit and exact release tag. Verify Windows
and Linux release archives and the installed SDK using the
[packaging guide](RELEASE_PACKAGING.md). Local Windows checks alone do not
establish Linux or sanitizer results.
