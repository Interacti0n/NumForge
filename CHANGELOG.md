# Changelog

All notable changes to NumForge are documented in this file. The project uses
[Semantic Versioning](https://semver.org/).

For a quick overview, read the [short changelog](CHANGELOG_SHORT.md).

## Unreleased

- Support mixed real/complex `sum`, `product` and `mean` in the shared C
  calculator and HTTP API. Keep exact rational components, including mean;
  preserve approximation metadata and reject mixed unit quantities.

- Add 744 independently verified complex references up to 250 significant
  digits and per-case timing diagnostics.

- Automatically extend supported dimensionless function domains to principal
  complex results: negative logarithms, out-of-domain inverse trigonometric and
  hyperbolic inputs, even roots and fractional powers. Add public complex
  asinh/acosh/atanh; preserve real-domain results, quantities and singularities.

- Automatically promote negative dimensionless `sqrt` / `√` arguments to
  principal complex roots in the typed calculator and HTTP API; preserve exact
  rational roots and existing real/quantity behavior.

- Raise calculator factorial input to 100000, matching BigInt, and the
  single-allocation limit to 512 KiB while retaining time/output/total budgets.
  Extend exact-reference factorial/product-tree benchmarks through 100000.

- Add principal complex `asin`, `acos` and `atan` to C, calculator and HTTP,
  with radian results, documented cut conventions and atomic errors at atan(±i).

- Add public complex `sinh`, `cosh` and `tanh` across C, calculator and HTTP,
  with a scaled tanh formula for large real components and retained imaginary tails.

- Add public complex `sin`, `cos` and `tan` in the C library, calculator and
  HTTP API, using radians for complex inputs and preserving real RAD/DEG behavior.

- Add public `bigcomplex_log` and complex base-10/custom-base logarithms
  through calculator and HTTP API, with principal branches and atomic errors.

- Add public `bigcomplex_pow` and principal complex powers via `^` / `pow`
  in calculator and HTTP API, preserving exact integer powers and real domains.

- Add public `bigcomplex_ln` and principal complex natural logarithms in the
  calculator and HTTP API, preserving real domains and documenting the branch cut.

- Add public `bigcomplex_sqrt` and principal complex roots through calculator
  `sqrt` / `√`, preserving proven rational roots and existing real domains.

- Integrate typed exact/decimal complex values into calculator arithmetic,
  variables, ans, previews, history and HTTP snapshots. Add complex(re;im),
  explicit mixed promotion, versioned component snapshots and web display-form
  selection; reserve lowercase i as the imaginary unit and reject complex unit conversions.
- Add natural a+bi input, re/im/conj/abs/arg and complex exp/e^z in the C
  calculator, with exact component extraction and rational magnitudes.
  Add public bigcomplex_exp, rational conjugation and squared-modulus APIs.

- Add standalone exact BigRationalComplex arithmetic, signed powers and explicit
  decimal projection. Add Cartesian, trigonometric and exponential complex
  display forms, radian argument, symbolic axis angles and polar construction.

- Add bounded BigComplex display formatting using shared decimal rounding and
  notation modes, preserving signed directed rounding and UTF-8 byte limits.

- Extend BigComplex with exact integer powers (including negative exponents
  and INT64_MIN) and exact-or-significant modulus, with explicit precision,
  rounding, atomic failure behavior and allocation-failure tests.

- Add an opaque standalone BigComplex C API with owned decimal components,
  exact basic arithmetic, componentwise exact-or-significant division,
  conjugation, squared modulus and canonical text. Include aliasing/allocation
  tests and installed C/C++ consumer coverage; calculator integration is deferred.

- Reduce the desktop navbar height; grow conversion input to three lines and
  calculator input to five, then scroll longer expressions within the field.

- Show a secondary decimal hint with its unit beneath short fractional conversion
  results, using the same C formatter as calculator fraction hints.

- Show desktop conversion precision/display settings permanently with a static
  heading; retain the collapsible controls on smaller screens.

- Separate result annotations with a subtle rule, place highlighted conversion
  units next to the value and blend the navbar into the violet workbench palette.

- Keep desktop converter/result panel heights stable when opening precision
  settings and remove the redundant conversion examples card.

- Hide the mobile header when scrolling down and reveal it when scrolling up,
  including the guide reading pane; keep navigation available while its menu is open.

- Refine mobile guide navigation with compact section links, a reading-section
  indicator and scroll-aware highlighting that keeps the active link in view;
  include the introduction and final session/conversion HTTP API section.

- Reveal the result after explicit calculator/converter confirmation on phones,
  without moving live previews; simplify mobile converter copy and place history
  below its result while retaining errors and unit-source information.

- Hide the lengthy Session introduction on phones in portrait and landscape;
  keep its controls and the full guide available.

- Keep mobile input actions in one row with compact labels and request a text
  virtual keyboard with capitalization/correction disabled and a Go enter key,
  allowing variables and punctuation in expressions.

- Place Session and always-expanded Settings in equal-width columns on phones
  in landscape; keep portrait settings open and preserve desktop layout.

- Extend library search across all 46 functions with English/Slovak names,
  familiar abbreviations and server-registered aliases; keep canonical insertion.

- Search library constants by English aliases and localized Slovak names,
  ignoring case, accents and straight/curly apostrophe differences.

- Give mobile input a full-width row with calculate, clear and library actions;
  hide its keypad, move settings below the session, and support landscape.
  Add searchable pi, Euler and golden-ratio constants to the library in both languages.

- Remove the calculator's introductory copy; keep mobile settings visible below
  the session with a summary of current precision and angle mode.

- Refine shared navigation with compact tool links, grouped account/language
  controls and a mobile menu with localized open/close labels and Escape focus return.

- Add an explicit `numforge_web --origin` option for browser requests through
  an HTTPS reverse proxy or tunnel; preserve loopback binding and default origin checks.

- Complete HTTP coverage for existing web state: authoritative paginated
  variables/history, versioned exact/approximate/Quantity snapshots, ordered
  reset/release/history clearing and a function/alias/arity registry. Clearing
  history preserves owned ans; reads preserve preview, revisions and RNG.
- Store confirmed unit conversions in bounded application sessions with
  separate revisions and identical-request replay. The web reads/restores
  snapshots through the same API; it retains tab IDs through reload/navigation
  instead of treating browser storage as authoritative mathematical state.

- Add automated Windows/Linux developer SDK packaging and SDK-only backfills
  for existing release tags. Ship public headers, a Release static library,
  relocatable CMake exports, compiler/runtime metadata and a verified C example;
  check extracted archives with external C/C++ consumers. The 2.1 SDK backfill
  uses the unchanged `v2.1.0` numeric source.

## [2.1.0] - 2026-10-08

- Keep overflow checks portable on 32-bit GCC and centralize conversion error
  diagnostics in the HTTP unit adapter for Linux warnings-as-errors builds.
- Fit the mobile header at 320 pixels with Linux font metrics and keep guide
  control geometry stable across Slovak and English.
- Keep portable archives minimal: two applications, MIT license and short
  launch instructions with version, source commit and runtime requirements.

- Organize documentation into user guides, API references, design, testing,
  benchmarks and project maintenance. Split long mixed-topic documents, add
  concise release highlights and prepare version 2.1.0 packaging.

- Separate private expression evaluation, application sessions/client storage
  and HTTP adapters into build targets. Move session/cache ownership into
  `src/application`; CLI no longer depends on HTTP adapters. Document boundaries
  for future persistence without introducing a database.
- Scroll the full right calculator sidebar so functions, session controls,
  history and all variables remain reachable in short desktop windows.

- Document web/HTTP API coverage and remaining session/history gaps. Correct
  SK/EN guide variable support and explain the existing delete-variable call.

- Show all session variables in a scrollable SK/EN list, with
  confirmed value previews, a capacity counter and click-to-insert names.
  Switch between History and Variables tabs; delete individual variables from
  the list or the revision-checked HTTP session command without changing ans.

- Separate the SK/EN Guide/API link from mathematical tool tabs. Keep it visible
  in the header when the mobile navigation menu is closed.

- Add `qty(value; "unit")` expressions, dimension-checked arithmetic, derived
  units, powers/roots, affine temperature rules and selected quantity-aware
  functions. Preserve quantities in variables, ans, history and previews across
  CLI/web/API; keep `convert` numeric and reject conflicting converter inputs.

- Add `convert(value; "from"; "to")` to CLI and web calculator expressions.
  Return numbers, preserve exact fractions, validate case-sensitive catalogue
  IDs and compatibility, and report errors at the offending unit argument.

- Add independent browser conversion history with value/unit snapshots,
  original expressions/settings and approximation metadata. Preserve exact
  values independently of display rounding; restore without reevaluation.
  Bound tab history to 16 entries/1 MiB, support clear/copy and confirmation retry,
  and leave calculator ans, variables and history unchanged.

- Add the SK/EN browser unit converter with ten quantity categories, compatible
  selectors, live preview/confirmation, swap/copy, precision controls, sourced
  definitions and responsive keyboard-accessible controls. Preserve calculator
  sessions and converter fields during tool/language navigation. Fit expanded
  conversion settings into the desktop viewport and allow natural page scrolling
  in narrower or shorter windows.

- Add local HTTP unit catalogue and expression conversion routes, with separate
  working/display precision, exact rational input, validated compatibility and
  read-only access to session variables/ans; reject assignments and random input.

- Add a public unit-conversion foundation with a sourced 233-unit SK/EN catalogue, exact
  rational and decimal conversion, SI/imperial/information/angle units,
  guarded pi projection, separate temperature points/intervals,
  compatibility errors and unchanged outputs on failure.
  Calculator conversion syntax is described in the later `convert` entry above.

- Refine tangent argument reduction when cancellation near pi/2 loses digits;
  add strict independent pole references for all six rounding modes and keep
  the benchmark smoke check strict. Other trigonometric behavior is unchanged.
- Accelerate short scientific/Auto formatting of large coefficients using
  outward-rounded bounds from leading limbs; preserve exact fallback at
  rounding/exponent boundaries, full output and all rounding modes.
- Add BigInt input/output APIs for explicit bases 2–36, ASCII case selection,
  canonical signed output and unchanged destinations/output pointers on failure.
- Add confirmed session variables to CLI/web: `x=2/3`, exact typed snapshots,
  read-only previews, atomic reassignment, retry protection and isolated/reset
  lifetime. Add bilingual instructions and located undefined-variable errors.

- Add documentation and benchmark navigation guides, a repository source map,
  and group high-precision benchmark references with their generator.

- Add direct BigDecimal scale/normalization/division and higher-function/constant
  benchmarks with independently checked high-precision references, isolated
  three-run recording, separate memory/phase probes and platform smoke checks.
  Report extreme tangent-pole accuracy mismatches and timeouts explicitly;
  preserve existing production algorithms and numerical tolerance policy.

- Prepare portable Windows x64/Linux x64 application archives with launch
  instructions, build provenance, SHA-256 checksums and extracted-package smoke
  checks. Automatically build exact release tags and attach verified binaries
  when a GitHub release is published; retain manual dry runs and backfills
  without overwriting existing assets.

- Expand the README with status badges, verified examples, real desktop/mobile
  screenshots, architecture and benchmark highlights. Add contributor,
  architecture, roadmap and presentation-maintenance documentation.

- Add independently checked BigInt arithmetic measurements for dense operand
  sizes, signs/aliasing, fresh outputs, copies, division/modulo/GCD, factorial
  and powers. Compare private product-tree and symmetric-square experiments
  without changing production algorithms; record three isolated baseline runs.

- Extend opt-in formatting benchmarks with explicit Auto/scientific notation,
  full precision, extreme scales and independently checked rounding edges.
- Add cache/session, destination-reuse and real loopback HTTP benchmarks,
  with exclusive phase timing and separate live/peak payload and process-peak
  memory measurements. Keep production numeric behavior and ownership intact.
- Add a three-run baseline recorder with compiler/build/machine metadata,
  source snapshots and CSV output, without CI timing thresholds.

- Add a subtle purple gradient with mathematical sketches and forge details
  around the page edges, softened on mobile.

- Keep the web header and footer mounted while internal navigation replaces the
  center content; remove the enclosing menu frame and distinguish each link.
  Keep header control positions and widths stable across Slovak and English.
- Add a shared single-row top menu for the calculator, guide and future graphs,
  equations and unit conversion. Sign-in and registration remain separate
  actions with localized informational pages until those features exist.
- Keep function category labels on one line and number confirmed web history
  entries in order, including after the 16-entry window rolls over.
- Show an approximate decimal hint beneath exact fractions in the local web
  calculator while preserving the exact result for copying and history.
- Remove redundant section labels from both language versions of the web guide.

## [2.0.0] - 2026-09-28

This release greatly expands the numeric library and refreshes its optional CLI
and web calculator. Existing BigInt and BigDecimal C signatures remain source
compatible with 1.0. The CMake package now uses major version 2, so consumers
that request NumForge 1.x through `find_package` must update that requirement.

- Add Linux ARM64 and macOS ARM64 CI jobs for Release builds, the full CTest
  suite and installed-package checks with external C and C++ consumers.

- Show the supplied NumForge wordmark in the Slovak and English guide titles.

- Use the supplied NumForge PNG logo in the calculator and guide headers and
  as the favicon on both language versions.

- Extend the independent numerical oracle with 319 frozen high-precision
  exp/log, trigonometric and hyperbolic references. Check domain errors and
  nearest rounding exactly, and bound directed approximation error to 1 ULP.

- Align calculator and guide header/footer dimensions, keep a result or error
  line visible under expanded input, preserve the same-tab web session across
  language and guide navigation, show the local MIT text in a dialog, and open
  GitHub in a new tab. Start results at the top of their panel with a
  subtle accent while keeping long values and errors readable.

- Accept `arc` and `arcus` aliases for all six inverse trigonometric and
  hyperbolic calculator functions. Include those names in bilingual function
  search, help and documentation.

- Redesign the bilingual local calculator and API guide with responsive layouts,
  larger touch controls, searchable function groups and a
  guide table of contents. Let narrow screens scroll at normal scale instead
  of shrinking the entire calculator to fit the viewport. Keep the desktop
  calculator within the viewport at ordinary window heights, open long results
  in a scrollable modal, make output settings quieter, and keep the guide
  navigation visible while reading. Move settings to a compact strip above the
  expression, enlarge secondary keypad labels, list function categories
  vertically in a panel separate from larger function buttons, allow
  accent-free Slovak search, and fit recent tools into one readable row according
  to the available width. Keep function help visible below
  the scrolling list, use a full-width compact settings bar, and align both
  pages with Melanie Brown's Deep Purple palette. Give each calculator card a
  title and icon, let the session fill the sidebar below the function library,
  and allow the expanded result to close on outside click or copy its value.
  Keep the entire guide header and section menu in place while its text scrolls.
  Show the CMake project version, MIT license, author and GitHub link in a
  shared footer on both pages. Start expressions at one line, grow to five
  lines, then enlarge the same editable field over the
  result for long input without blocking keypad controls, and collapse it on
  Enter or Esc. Let the result fill the available height above the keypad
  before marking overflow with an ellipsis and `Show all...`.

- Split calculator value formatting, named and trigonometric function calls,
  exact function implementations, and web server startup into focused private
  C modules. This reorganizes the client code without changing its API.

- Move the SK/EN web pages, CSS, and calculator JavaScript into editable source
  files under `web/`. Generate their embedded C representation at build time so
  the local server remains a single executable; serve styles and script through
  local asset routes.

- Add a fraction notation to CLI and browser output. It shows exact reduced
  fractions without /1 up to 16 characters; longer fractions and approximate
  values use Auto decimal notation. Auto chooses a fraction with denominator at
  most 10000 and at most 16 output characters when it is at least two characters
  shorter than the decimal display. Restore the rand button in
  the English function panel.

- Preserve exact BigInt and BigRational results through calculator previews,
  confirmed `ans`, history and cache for literal arithmetic, integer powers,
  proven rational roots and selected named arithmetic, aggregation and integer
  functions. Convert exact subexpressions at the current working precision when
  an irrational result or approximate constant needs BigDecimal. Output
  notation is selected by the formatter without changing the stored value.

- Add a standalone public `BigRational` numeric API with reduced exact
  fractions, arithmetic, comparison, conversions to and from BigInt/BigDecimal,
  and integer-style text for denominator one. Decimal conversion preserves
  terminating values and rounds recurring values at explicit precision.

- Add selectable Auto, plain, scientific and mathematical result notation to
  the public BigDecimal formatter, CLI and browser. Auto uses scientific output
  for exponent magnitude at least 10 or plain output over 80 characters.
  Mathematical output copies as parser-compatible `E` notation; explicit plain
  output respects the application size limit without changing the stored value.

- Add calculator `rand()`, `rand(x)` and `rand(x;y)` with independent 34-place
  decimal draws, bounded ranges, stable session previews and replay-safe
  confirmation. Document the non-cryptographic distribution and add a web key.

- Fix 32-bit GCC warnings-as-errors in the decimal formatter by checking
  whether a `size_t` can exceed `INT64_MAX` only on platforms where it can.

- Add private calculator sessions shared by CLI and web: `ans`, explicit
  confirmation and a bounded 16-entry in-memory history retaining internal
  values. Previews never update ans; failed confirmations preserve state.
  Add CLI `history`/`reset`, bilingual history controls and HTTP session
  start/preview/commit actions with replay protection and explicit expiration.
  Keep numeric public APIs and the legacy stateless/cache HTTP paths unchanged.

- Add precision-aware negative integer powers through `bigdecimal_pow_signed`
  and the calculator while retaining the exact non-negative `bigdecimal_pow`
  contract. Add public `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, and `atanh`
  operations with stable small/extreme argument paths, bilingual calculator
  controls, domains, documentation, consumer coverage and failure-safety tests.

- Add public BigDecimal sequence aggregation with exact `sum` and `product`,
  plus exact-first `mean` with explicit significant-digit rounding. Enable the
  matching variadic calculator calls, bilingual web controls, documentation,
  consumer coverage and failure-safety tests.

- Add population `variance`/`stdevp` and sample `stdev` backed by exact
  sufficient statistics, guarded roots and explicit public precision. Include
  a dedicated Statistics web category, bilingual help and edge-case tests.

- Add exact `median`, non-negative `geomean`, and positive-domain `harmean`
  through the public BigDecimal API, calculator, bilingual web UI and tests.

- Add public BigDecimal `floor`, `ceil`, `trunc`, and half-even `round`
  operations. Enable matching calculator calls, including optional signed
  decimal places for `round(x;n)`, with bilingual controls and documentation.

- Add exact public BigInt permutation and combination operations without
  factorial intermediates. Enable calculator `npr(n;r)` and `ncr(n;r)` calls
  for non-negative integer arguments satisfying `r <= n`.

- Accelerate decimal conversion with native 128-by-64 division on GCC/Clang and
  MSVC x64 while retaining the portable fallback, and avoid a redundant initial
  coefficient conversion for provably fixed-notation BigDecimal values. Add a
  dedicated decimal conversion/formatting benchmark.

- Add an opt-in direct BigInt multiplication benchmark with reproducible operand
  patterns, size ratios, squaring, warm-up and calibrated median timings.
  Use a shared high-resolution monotonic timer for both benchmark executables.

- Offer Auto (10 places), Full and Custom output precision modes; show the
  numeric field only for Custom and preserve the mode across language changes.

- Place the contrasting RAD/DEG selector beside precision settings on the right, remove the introductory
  paragraph and preserve the expression/output settings when switching language.

- Retain unformatted calculator values for a bounded, isolated per-page web cache.
  Keep working precision automatic; reformat compatible cached values and
  recompute context-dependent expressions when working precision changes.
  Proven precision-independent expressions such as factorials can be reformatted
  across output precisions without evaluation. Add cache, allocation-failure,
  HTTP eviction/revision and browser isolation regressions.

- Add a shared segmented RAD/DEG selector,
  bilingual function signatures/domain hints for mouse, keyboard and touch, and
  clearer calculation, connection and unexpected-response errors.

- Fit the collapsed web calculator to the viewport height; allow vertical page
  scrolling for expanded results and test the layout across desktop/mobile sizes.
- Show web expression errors with a marked source excerpt and character position,
  including an explicit end-of-expression message in Slovak and English.

- Insert complete function calls with the caret inside the parentheses, move
  the caret to the expression end on Enter, and organize web functions into
  horizontal tabs with one active panel.

- Add arbitrary-precision radian sine, cosine, tangent, asin, acos, and atan,
  magnitude-aware π argument reduction, calculator RAD/DEG mode, explicit angle
  conversions, HTTP selection, and active bilingual web controls.
  Preserve large degree rotations and inverse values near domain endpoints;
  keep constant precision independent of earlier calls in the same expression.

- Add precision-aware `π`, `e`, and `φ`: retain the stored 500-place values for
  ordinary requests, calculate larger requests dynamically, and reuse each
  constant within a calculator evaluation.

- Add public arbitrary-precision `exp`, `ln`, base-10 `log10`, and
  arbitrary-base `log` BigDecimal operations without binary floating point.
  Enable `exp(x)`, `ln(x)`, `log(x)` and `log(x;b)` in the calculator and
  SK/EN web UI, including domain, aliasing and regression coverage.

- Implement real `sqrt`/`√`, `cbrt` and `root(x;n)` without floating-point
  conversion. Preserve exact finite roots; round irrational roots at working
  significant precision. Allow integer degrees 1..10000, with negative values
  only for odd degrees, subject to existing resource budgets.
- Activate root controls and update SK/EN help. Add a dedicated root test suite,
  520 independent root oracle cases, allocation/cancellation tests and browser
  checks for precision, aliases and domains.

- Enable calculator `gcd`, `lcm`, signed remainder `mod`, and non-negative
  integer floor root `isqrt`, including SK/EN controls and domain documentation.
- Add integer-function boundary cases, exhaustive small roots, 700 independent
  oracle comparisons, allocation-failure coverage and browser checks.
- Fix the unverified `sign` predicate calls and result-panel line height/reset;
  split embedded JavaScript to stay within portable C string limits.

### Changed

- Implement calculator `abs`, `sign`, variadic `min` and `max` using decimal
  operations, bounded live argument storage and error propagation; activate
  their SK/EN web controls.

- Raise the calculator factorial input limit to 10000 for both `n!` and
  `factorial(n)`. The five-second and memory budgets remain unchanged;
  accepted inputs are not guaranteed to finish within those budgets.

- Recognize 24 letter-only function names, nested calls, semicolon-separated
  arguments and the parenthesized square-root symbol. Validate arity and cap
  calls at 256 arguments; preserve standalone e, uppercase E and implicit products.
- Enable pow/factorial aliases through existing operators. Other registered
  calls report not implemented; group their disabled web controls into four
  collapsible sections with matching SK/EN help and regression coverage.

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

[2.0.0]: https://github.com/Interacti0n/NumForge/releases/tag/v2.0.0
[2.1.0]: https://github.com/Interacti0n/NumForge/releases/tag/v2.1.0
[1.0.0]: https://github.com/Interacti0n/NumForge/releases/tag/v1.0.0
