# Equation release readiness — 19 October 2026

This is a readiness checklist for the next release, **not a release record**.
The published release remains 2.3.0. Equation work is local/unreleased until
explicitly pushed, validated in CI and packaged. Current mathematical scope is
documented in the [guide](../guides/EQUATIONS.md), [exact API](../reference/EQUATION_HTTP_API.md)
and [numerical API](../reference/NUMERIC_ROOT_HTTP_API.md).

## Supported release scope

| Capability | State and boundary |
| --- | --- |
| Exact linear/quadratic equations | Whole equations or real rational coefficient expressions; exact real/complex roots, degeneracies and decimal previews. No irrational coefficients or intermediate degree above two. |
| One general real numerical root | Explicit sign-changing bracket, BigRational coordinates, BigDecimal residuals, conservative local domain checks and higher-precision final reevaluation. No completeness or certified root-error claim. |
| Multiple real root discovery | 128 rational cells plus bounded midpoint/local-quadratic candidates, at most 32 roots/4096 coordinate pairs, one shared operation budget. Simple rational quadratic contacts and some close pairs are covered. |
| Difficult numerical cases | Undefined/non-real samples, unsupported continuity checks, insufficient precision and work/result ceilings remain explicit. A small positive minimum is not accepted as a root. |
| Original domain preservation | Both original sides are evaluated, including candidate points; algebraic cancellation does not remove holes. General numerical tangencies such as (cos(x)-x)^2 remain potentially undiscovered. |
| Coordinate integrity | Scan coordinates retain canonical rationals, including 1/3. Copy and residual refer to the same point; decimal previews only position the graph camera. Single-root decimal x contract is unchanged. |
| Browser/API | Stateless private C application operations, strict HTTP options, SK/EN UI, errors, abort/stale-response protection, same-tab state, per-root copy/graph actions and focused guide navigation. |
| Public installed C SDK | Equation application interfaces remain private; no stable public numeric ABI is promised for these types. This is documented scope, not an accidentally missing export. |

## Gates before publication

1. **Freeze the initial feature scope.** Keep incomplete discovery explicit.
   General symbolic solving, arbitrary-degree exact polynomials, systems,
   quantities, arbitrary unknown names and general non-polynomial tangent
   discovery are subsequent capabilities, not implied by this release.
2. **Browser usability implementation is complete for the initial scope.**
   The equation editor now uses the calculator/graph catalogue metadata and C
   availability registry, with mode-appropriate functions/constants, balanced
   insertion, argument selection, a/b/c targeting, search, pagination and retry.
   Examples fill each mode without auto-solving. Full checked numerical points
   and residuals are expandable; copy failures expose the raw coordinate.
   Escape/close, validation focus, clean SPA disposal and keyboard-friendly
   mobile layouts are covered. Keep this scope frozen for the final regression.
3. **Local regression passed; run remote CI on the frozen commit.** Local GCC/MSVC
   numerical/failure checks and the complete Windows browser run are recorded
   below. Remote sanitizer/Linux32/ARM64/macOS/Windows jobs
   must pass for the exact release commit. Local Windows results do
   not stand in for these platforms. The scan smoke benchmark is now included
   in the existing three-OS benchmark job; remote results are pending a push.
4. **Check a current deployed preview on physical phones.** Browser emulation
   is covered; Android Chrome/iOS Safari keyboard, focus, copy permissions,
   navigation restore and graph handoff still need a short physical-device pass.
   The previously running development server does not automatically contain
   local commits. Deploy/update deliberately before requesting this check.
5. **Create and validate release artifacts.** After successful CI: choose the
   version, close both changelogs, create the tag/release, build application/SDK
   archives, verify checksums, smoke-test extracted applications and installed
   C/C++ consumers. No version/tag/release/push is performed by this audit.
   The application packaging helper now also checks both embedded equation
   languages/assets, function availability, exact radicals and a rational scan
   contact from the extracted application. Running this smoke against a local
   build does not replace checking the final versioned archives.

## Evidence and release blockers

The [scan baseline](../benchmarks/NUMERIC_ROOT_SCAN_2026_10_19.md) and
[candidate follow-up](../benchmarks/NUMERIC_ROOT_CANDIDATES_2026_10_19.md) record
time, allocation and independent coordinate checks. Native tests include
precision extremes, exact/near minima, holes, tiny/huge coordinates, allocation
and deadline failures, shared budgets and no partial successful publication.
Browser checks include full-coordinate copy, recurring fractions, graph handoff,
cache invalidation, both languages, mobile width and stale requests.

Earlier numerical-stage checks: MSVC Release/WX **61/61 native CTest checks**, then
the final candidate unit group after the last cap/domain assertions; GCC
Release/WX numerical/failure tests and scan smoke; **56 targeted browser cases**,
then **16 final scan/guide cases** after documentation updates; **135 benchmark
rows / 450 independently checked coordinates**. The apps-disabled benchmark
configuration and all three CI-selected smoke checks also passed locally.
These earlier results establish the mathematical baseline. Final editor-stage
regression is recorded separately below; remote platforms, physical devices
and extracted release artifacts remain separate gates above.

Block publication on a wrong root, residual referring to a different coordinate,
domain hole accepted as a root, budget reset per candidate, partial success after
resource failure, regression in other tools or a failed required CI/package job.
A documented missing general tangent or incomplete finite discovery is a scope
limit, provided messages do not claim no roots or a complete enumeration.

## Final editor-stage local regression

- MSVC v143 RelWithDebInfo with warnings as errors: complete build and
  **61/61 CTest groups passed in 45.22 s**, including equation, numerical scan,
  allocation/deadline, HTTP and embedded-server checks.
- **446/446 browser cases passed in 18.7 min** with one Chromium worker,
  covering calculator, complex values, equations, graphs, guide navigation,
  converter, sessions and mobile layouts. No retries were used.
- **19/19 final editor/scan cases passed in 28.6 s**. This includes one added
  clipboard-denial case beyond the full run's discovery snapshot, plus catalogue
  failure after resize/retry, Escape lifecycle, validation focus, balanced
  insertion, rational contact details and 320/390/812/1280 px layouts.
  Narrow-phone and landscape screenshots were visually reviewed.
- The extended application-package smoke passed against the local built
  applications on its own temporary port: CLI, HTTP calculator, both equation
  languages/assets, function registry, exact radicals and rational scan contact.
  This validates the helper; final versioned extracted archives are still pending.
- JavaScript syntax, documentation links (**117 Markdown files / 549 local
  links/anchors**) and whitespace checks passed.

No local regression blocker remains for the supported initial scope. Remote CI,
physical-device checks and versioned release/package verification remain open;
these results do not publish a release or certify all numerical roots.

## Later improvements

- Bounded adaptive/derivative-assisted candidates for more general contacts and
  clusters, retaining original-expression verification and explicit incompleteness.
- Higher-degree polynomial support and stronger domain/interval enclosures.
- Systems, user functions/parameters, quantity-aware equations and complex
  numerical solving with their own contracts and references.
- A stable installed equation C API or dedicated CLI commands once ownership,
  result schemas and numerical scope are ready to be maintained publicly.
