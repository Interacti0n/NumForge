# Current scope and roadmap

This page distinguishes implemented behavior from future work. It is a direction
of development, not a schedule or promise of a particular release.

## Available today

- C17 BigInt, BigDecimal, BigRational and BigComplex APIs with opaque types, explicit
  ownership, documented aliasing and strong failure guarantees.
- Exact integer/decimal/rational arithmetic; configurable precision/rounding for
  approximations; roots, logarithms, trigonometric/hyperbolic functions,
  constants, statistics and selected integer operations.
- [Complex arithmetic](../reference/BIGCOMPLEX.md), principal-branch functions,
  automatic domain promotion and Cartesian/trigonometric/exponential display.
- Shared CLI/local browser calculator with English/Slovak UI, confirmed `ans`,
  [session variables](../guides/VARIABLES.md),
  session history, stable random previews and a bounded loopback HTTP API.
- [Unit-conversion C foundation](../reference/UNITS.md): a sourced 233-unit catalogue,
  compatibility checks, exact rational factors and guarded angle projection.
  [Local HTTP catalogue and conversion](../reference/UNIT_HTTP_API.md) with read-only session
  access and a [SK/EN browser converter](../guides/UNIT_CONVERTER.md) are available.
  [Quantity expressions](../reference/QUANTITIES.md) support dimension-checked arithmetic,
  powers/roots, temperature points/intervals and selected functions.
- CMake installation and C/C++ consumer support; numerical, property,
  allocation-failure, integration, browser and fuzz-smoke checks.
- Formatting/cache/HTTP/memory, BigInt and BigDecimal/math benchmarks with independent
  result checks and repeated local Release baselines.

For exact supported functions and limits, use [API.md](../API.md). Screenshots in
the README show development code and may be newer than a published release.

## Next accuracy and optimization work

Direct BigDecimal arithmetic and higher-function/constant measurements are
implemented with independent references through 2,000 significant digits;
see [Decimal/math measurements](../benchmarks/DECIMAL_MATH_BENCHMARKS.md). The historical stress matrix
exposed tangent-pole cancellation, now addressed by adaptive reduction and
strict independent regressions through 2,000 digits. General correctly rounded
approximate functions remain a separate goal. Some high-precision cases exceed
the collection deadline and need dedicated longer measurements.

Short scientific formatting now uses bounded prefix conversion with exact
fallback. Normalized limb division, grouped factorial multiplication, private
GCD remainder buffers and per-call ln/exp arithmetic workspaces are implemented.
The practical factorial cap is removed; the uint64 counter range and shared
application time/memory/output budgets remain. See
[factorial budgets and final text conversion](../benchmarks/FACTORIAL_BUDGETS_2026_10_11.md).
Budget-aware Karatsuba now serves sufficiently large balanced products;
see the [integration report](../benchmarks/KARATSUBA_INTEGRATION_2026_10_11.md).
Budget-admitted product-tree factorial now reuses per-depth Karatsuba frames;
see [factorial tree integration](../benchmarks/FACTORIAL_TREE_INTEGRATION_2026_10_11.md).
The [larger factorial stress matrix](../benchmarks/FACTORIAL_STRESS_2026_10_11.md)
identifies decimal zero stripping as the dominant successful-path cost and
separates conservative admission from real scratch allocation constraints.
Large decimal coefficients now use exact 27-zero inverse division with optional
scratch; see [normalization integration](../benchmarks/ZERO_NORMALIZATION_2026_10_11.md).
Checked factor-range bounds and shared workspace planning now tighten admission;
see [the boundary verification](../benchmarks/FACTORIAL_ADMISSION_2026_10_12.md).
Split sum/cross scratch is now implemented and measured in the
[bounded final factorial experiment](../benchmarks/SPLIT_WORKSPACE_2026_10_12.md).
Further factorial/bound/multiplication research is deferred; next work is the
overall regression, UX and CI review followed by release preparation.
The [private Karatsuba experiment](../benchmarks/KARATSUBA_EXPERIMENT_2026_10_11.md)
adds bounded scratch, exact references and cancellation/failure checks, with
threshold and grouped product-tree comparisons preceding the integration.
Existing experiments are described with their measured tradeoffs in
[BIGINT_BENCHMARKS.md](../benchmarks/BIGINT_BENCHMARKS.md).

The [bounded theta/AGM logarithm](../benchmarks/LOGARITHM_AGM_INTEGRATION_2026_10_11.md)
is integrated with outward-rounded theta tails, AGM/pi enclosures, adaptive
precision and a private precision-dependent dispatcher. The original series
remains for low precision and near-one inputs. Next numerical candidates are
lower interval allocation traffic and enclosure-based rounding for further
functions after release validation. Composed
complex functions retain their existing approximation contracts.

## Planned tool development

The [web/API contract](../reference/WEB_API_PARITY.md) now covers session lists,
lifecycle commands, function discovery and saved conversion history. New
mathematical/session features must expose their application operation and
documented HTTP contract alongside the web implementation, using the versioned
[session value schema](../reference/SESSION_HTTP_API.md) where appropriate.

1. The [parametric C application foundation](../design/PARAMETRIC_EXPRESSIONS.md)
   now parses once, binds a local parameter and snapshots referenced session
   values. Typed batches share one budget and preserve calculator state.
   The [HTTP graph sampler](../reference/GRAPH_HTTP_API.md) now adds bounded exact
   uniform coordinates, plotting projections and point classifications.
2. [Browser graphs](../guides/GRAPHS.md) now display up to four colored `y = f(x)`
   curves with inferred parameters, automatic sampling, pan/zoom, visibility,
   a shared function editor/palette, an expanded canvas and cached coordinate inspection.
   Automatic rendering includes bounded integer-domain sampling without a mode selector.
   Axis/pair markers are numerically verified against original C expressions and
   selected across curve groups and screen regions. Arbitrary discrete lattices
   and complete/symbolic root enumeration remain outside this feature.
   Visible-first sampling, reusable resolution levels and accepted cached previews
   reduce repeated work; missing/gapped views keep coarse/refined sampling.
   Supported affine/quadratic sin/cos carriers, selected sums and absolute-value
   forms have approximate envelope displays; broader domains remain future work.
   [Sustained interaction/memory diagnostics](../benchmarks/GRAPH_STABILITY_2026_10_14.md)
   are available. [Domain-sensitive view refinement and per-function diagnostics](../benchmarks/GRAPH_DOMAIN_VIEWS_2026_10_15.md)
   are implemented; informational notes are hidden in the UI. Initial phone feedback
   is positive; broader physical-device checks and graph export/import remain next. Adaptive refinement
   and connection flags remain heuristics, not continuity proofs.
3. The [private C linear/quadratic foundation](../design/POLYNOMIAL_EQUATIONS.md)
   now solves real rational coefficients exactly, including irrational and
   complex roots and degenerate cases. Stateless coefficient expressions and
   the [HTTP adapter](../reference/EQUATION_HTTP_API.md), exact root text,
   stable decimal previews and the [browser editor](../guides/EQUATIONS.md)
   are available. Whole equations in lowercase x now normalize exact polynomial
   operations on both sides, with intermediate degree <=2 and conservative
   rejection of variable denominators/functions. Coefficient mode remains available.
   Then add bounded numerical root finding on a specified interval, with
   residuals and clear convergence limits.
4. Add session user functions, local parameters and definition management,
   followed by their integration with graphs and solvers.
5. Extend Quantity with general compound unit strings, named derived units,
   further functions and a possible public C API.
   Dimensional equations and graph axes need explicit unit contracts first.

The remaining stages are a proposed order, not implemented features or release dates.
Matrices/vectors and dataset statistics follow once their initial scope is set.
Measured arithmetic optimizations can proceed independently of these tools.

## Later, according to use cases

- [Accounts and authentication](../design/ACCOUNT_DESIGN.md): proposed private
  C application operations and HTTP contracts for verified registration, login,
  session revocation and password recovery. Storage, crypto and mail adapters
  precede implementation; account-bound saved values remain a separate stage.
  The [post-login product proposal](../design/ACCOUNT_DESIGN.md#proposed-benefits-after-login--product-review)
  prioritizes settings, named workspaces, favorite calculations/conversions and
  cross-device continuation with explicit revision conflicts. Export/deletion
  precedes public rollout; saved graphs/functions and explicit sharing follow later.
- Stable serialization and language bindings; BigInt numeral bases 2–36 are implemented.
- Further numerical analysis and graphing beyond the basic tools above.
- Improvements to directed rounding of approximate functions, supported by
  rigorous error bounds or adaptive precision and independent references.
- Additional constants and mathematical types/functions where there is a
  concrete application need.

Sign-in and account-creation navigation entries currently
open informational pages. Accounts and synchronization are not implemented;
coefficient-based equation solving is available in C, HTTP and the browser.
The app has no Premium feature or subscription system.

## Distribution and project presentation

Releases provide source archives for library developers and portable Windows/Linux
x64 application archives through the verified binary workflow in
[RELEASE_PACKAGING.md](RELEASE_PACKAGING.md). The [2.1 release record](RELEASE_2_1.md)
documents scope and verification. A hosted interactive demo needs a hosting plan.
Publishing the current loopback server directly is insufficient: public service
work requires worker isolation, queues, rate limiting and hard resource limits.

README screenshots, architecture and benchmark highlights are maintained in
[PROJECT_PRESENTATION.md](PROJECT_PRESENTATION.md). A portfolio connection is
deferred until the portfolio is ready; no connection is configured now.
