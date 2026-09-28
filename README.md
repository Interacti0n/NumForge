# NumForge

NumForge is a C17 mathematics library with CLI and web clients. It provides
three public numeric types: signed arbitrary-precision `BigInt`, base-10
`BigDecimal`, and exact `BigRational`. The command-line and local browser
calculators share the same C tokenizer, parser, evaluator, and BigDecimal
implementation.
All implemented numeric operations are exposed by the public library; clients
add expression syntax, presentation and application resource limits.

## Features

- Signed arbitrary-precision integers stored internally in base 2<sup>64</sup>.
- Decimal parsing and formatting.
- Addition, subtraction, multiplication, division, modulo, powers, GCD, LCM,
  and factorial.
- Bitwise operations and shifts.
- Perfect-square and Miller-Rabin probable-prime checks.
- Exact decimal arithmetic with configurable rounding for division and
  rescaling.
- Exact reduced rational arithmetic and explicit precision conversion to
  BigDecimal. The calculator retains exact rational results where possible.
- Arbitrary-precision real roots, exponential, natural logarithm, common
  logarithm, and logarithms with a caller-selected base.
- Arbitrary-precision radian sine, cosine, tangent, and inverse trigonometric
  functions with guarded large-argument reduction.
- Arbitrary-precision hyperbolic and inverse hyperbolic functions with stable
  small-argument and extreme-value paths.
- Interactive expression calculator with source-positioned diagnostics.
- Precision-aware approximations of `π`, `e`, and `φ`: stored values make
  ordinary requests cheap, while requests beyond 500 digits are calculated
  dynamically without binary floating point.
- Configurable result precision, full output mode, and readable scientific
  notation for very large or very small non-zero results.
- Local browser calculator served directly by the C executable; its requests
  are evaluated by the same parser and `BigDecimal` core.
- Output/input aliasing for arithmetic operations, including
  `bigint_add(x, x, y)` and `bigint_div_mod(q, r, q, r)`.
- Unit tests, deterministic property tests, warnings-as-errors, and Linux,
  Windows and ARM64 macOS CI, including deterministic allocation-failure injection.

## Requirements

- CMake 3.20 or later
- A compiler with C17 support
- Git when configuring tests for the first time, because CMake uses it to
  fetch the Unity test framework
- Internet access on the first test-enabled CMake configure, so CMake can
  download the Unity test framework

## Documentation

| Document | Purpose |
| --- | --- |
| [Library guide](docs/LIBRARY_GUIDE.md) | Standalone core builds and public C/C++ usage. |
| [API overview](docs/API.md) | Public numeric APIs, ownership rules, calculator syntax, and local HTTP API. |
| [BigInt design](docs/BIGINT_DESIGN.md) | Limb representation, semantics, and optimization boundaries. |
| [BigDecimal design](docs/BIGDECIMAL_DESIGN.md) | Exact-decimal representation, rounding, and future work. |
| [Calculator design](docs/CALCULATOR_DESIGN.md) | Expression grammar, evaluation policy, and CLI/web integration. |
| [Testing guide](docs/TESTING.md) | Unit, integration, browser, fuzz, and performance checks. |

## Project layout

```text
NumForge/
├── include/numforge/   Public BigInt, BigDecimal, BigRational, and runtime headers
├── src/
│   ├── bigint/        Integer implementation and private helpers
│   ├── bigdecimal/    Decimal implementation and private helpers
│   ├── bigrational/   Exact rational implementation
│   ├── internal/      Allocators, resource budgets, and test instrumentation
│   ├── calculator/    Expression syntax and evaluation using the public library
│   ├── web/           Local HTTP server and adapter
│   └── main.c         CLI entry point
├── web/               Editable SK/EN HTML pages, shared CSS, and calculator JavaScript
├── tests/             Unit, property, and integration tests
│   ├── browser/       Playwright browser scenarios and their npm dependencies
│   ├── fuzz/          Fuzz harnesses, replay driver, and dictionaries
│   └── package_consumer/  Independent installed-library C/C++ consumer
├── benchmarks/        Optional phase and allocation benchmarks
├── cmake/             Package configuration and coverage support
├── docs/              API, design, usage, and testing documentation
└── .github/workflows/ Continuous integration
```

`NumForge::numforge` contains the numeric library and runtime support.
The private `numforge_client` target adds the calculator and HTTP adapters;
the CLI and web executables use this client layer. Only `include/numforge/`
is installed as public headers. Module-level source maps are in the design
documents above and in the corresponding header comments.

The web source lives in `web/`. CMake generates an ignored header from these
assets when building `numforge_web`, so the executable remains self-contained.
Edit the HTML, CSS, or JavaScript source files directly; the next build embeds
the changes. No separate asset directory is needed at runtime.

Local build trees, editor state, browser reports, and `PROJECT_REVIEW.md`
are ignored by Git. Browser npm dependencies are test tooling; running the
calculator requires no Node.js installation.

## Build

### Build the library and demo

```sh
cmake -DNUMFORGE_LOCAL_TESTS=OFF -P cmake/LocalBuild.cmake
```

This uses an ignored build directory tied to the current project path. When
the project is moved, CMake generates a new build directory; an old `build/`
tree can contain test and executable paths from the previous location. For
the complete local test suite, run `cmake -P cmake/LocalBuild.cmake`.

### Build and run tests

```sh
cmake -P cmake/LocalBuild.cmake
```

The command configures Release, builds and runs CTest. It prints the current
build path. After moving the project, the path changes and the next run uses a
new build tree. See [Testing guide](docs/TESTING.md) for targeted and CI checks.

For stricter local verification with GCC or Clang:

```sh
cmake -S . -B build-sanitize -DBUILD_TESTING=ON \
  -DNUMFORGE_WARNINGS_AS_ERRORS=ON \
  -DNUMFORGE_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

`NUMFORGE_ENABLE_SANITIZERS` enables AddressSanitizer and
UndefinedBehaviorSanitizer on GCC and Clang. For a manual build like this,
choose a fresh directory after moving the project.

### Run the local web calculator

No Node.js, package manager, database, or external service is needed. Run
`cmake -DNUMFORGE_LOCAL_TESTS=OFF -P cmake/LocalBuild.cmake`, then start the
`numforge_web` executable in the printed build directory. Visual Studio puts
it in that directory's `Release/` subdirectory.

On Windows, the executable automatically opens `http://127.0.0.1:8765` in the
default browser. It listens only on the local machine; press `Ctrl+C` in the
terminal to stop it. Use `--port 9000` to select another loopback port and
`--no-browser` when the page should not open automatically. The existing
`NUMFORGE_WEB_NO_BROWSER=1` environment setting is also supported for headless
runs.

The page includes a clickable keypad for the current expression grammar,
including `π`, `e`, `φ`, `xʸ`, `x²`, `x³`, and `n!`. Powers, squaring, and
cubing accept any exact decimal base with a whole-number exponent; negative
exponents use a precision-aware reciprocal, preserving finite decimals exactly;
factorial requires an input from 0 to 10000. Its precision control defaults to
10 decimal places (configurable from 0 to 10000); full output is also available.
Non-terminating division uses working significant digits, so `1E-40 / 1` remains `1E-40`.
The complete calculation has a five-second monotonic budget, checked during
parsing, arithmetic and formatting, including expensive BigInt loops. The
calculator also bounds allocations and output size. Exceeding these limits
returns `TLE` or `value too large`; public numeric library calls remain uncapped
by these application policies. Cancellation is cooperative, not a hard real-time
process-kill guarantee.
The `rand()` key inserts a complete call. `rand()`, `rand(x)` and `rand(x;y)`
draw independently in `[0,1)`, `[0,x)` and `[x,y)` (`x > 0`, `x < y`). Draws use
a 34-place decimal grid. Repeated previews keep their draws; a new confirmation
draws again. The generator is not cryptographically secure.

Named calls use parentheses and semicolons: `pow(2;3)` and `factorial(5)`
already calculate through the existing operators. `abs`, `sign`, `min` and `max`
also calculate using decimal values without additional rounding. `floor`, `ceil`
and `trunc` round to an integer; `round(x)` and `round(x;n)` use half-even at
zero or `n` decimal places, including negative `n` for tens and larger powers.
Integer-valued arguments support `gcd`, `lcm`, `mod` and floor square root
`isqrt`. `npr(n;r)` and `ncr(n;r)` provide exact permutations and combinations
for integers satisfying `0 ≤ r ≤ n`. `sum`, `product`, and `mean` aggregate
one to 256 decimal values; sum and product are exact, while mean rounds only
a non-terminating quotient to working precision. `median` is exact, `geomean`
accepts non-negative values, and `harmean` accepts positive values. `variance`
and `stdevp` use population denominator `n`; `stdev` uses sample denominator
`n−1`. Real roots `sqrt`/`√`, `cbrt` and `root(x;n)` preserve exact finite roots
and otherwise use working precision (34 significant digits by default). Trigonometric and inverse
trigonometric calls use the shared RAD/DEG selector; explicit `radians(x)` and
`degrees(x)` conversions remain available. Inverse trigonometric and hyperbolic
calls also accept `arc` and `arcus` names (`arcsin`, `arcussin`, `arctanh`, etc.).
Hyperbolic calls are independent of RAD/DEG. Six vertically listed categories and a search field expose the function
controls. Search ignores Slovak diacritics; controls show signatures and domain
hints on hover, focus or touch. The compact output settings sit above the
expression. Recently used constants, operations and functions appear below
the keypad, capped at five shortcuts. The page uses a two-column layout on wide screens and
natural scrolling with touch-sized controls on narrow screens. Connection
and unexpected-response errors offer retry with Enter.
The page is available in Slovak and English, and the displayed
result can be copied with one click. See the
[API overview](docs/API.md) for exact syntax and the local HTTP API.
Long results stay in a compact five-line panel. `Show all` opens a scrollable
dialog on desktop and expands the panel on narrow screens. The expression wraps
across two visible lines; longer expressions can be opened in an editable
dialog. A clipped result shows `...` and a `Show all...` control.
Working precision remains automatic; output precision and notation are user-configurable.
Choose Auto (10 decimal places), Full (no final output rounding), or Custom
(0–10000 places). The number field appears only in Custom mode.
The separate notation selector offers Auto, plain, scientific (`1.23E+45`),
mathematical (`1.23 × 10^45`) and fraction output. Auto uses a reduced exact
fraction when its denominator is at most 10000, its text has at most 16
characters, and it is at least two characters shorter than the decimal display;
otherwise its decimal rules select plain or scientific notation. Fraction mode
writes an exact non-integer as `a/b` when it fits in 16 characters; longer
fractions and approximate values use Auto decimal notation. Exact integers
remain integers. Plain and fraction output
are limited to 65536 UTF-8 bytes; an oversized result
reports an error. Mathematical output copies as parser-compatible `E` notation
when it fits the input range and size limit. The CLI uses `notation auto`,
`notation plain`, `notation scientific`, `notation math` or `notation fraction`.
Each page has an isolated in-memory session (up to eight pages on the server).
Automatic previews do not change `ans`. Enter, Calculate or `=` confirms a
successful result and appends it to the last 16 history entries. `ans` stores
the internal value, so confirming `1/8` at two output places retains `0.125`.
Literal arithmetic with `+`, `-`, `*` and `/` keeps an exact integer or reduced
fraction internally: confirming `1/3` can therefore show more digits when the
precision later increases, and `(1/3)*3` evaluates to `1`. Integer powers,
perfect rational roots and named arithmetic and statistics functions keep exact values too:
`sqrt(4/9)` is internally `2/3`. Irrational roots and constants are approximate;
their results do not become exact merely because the display looks like an integer.
Increasing precision cannot recover digits already lost in an approximation.
History buttons restore only the expression; evaluation uses the current `ans`.
Language changes and guide navigation in the same tab retain `ans`, history,
input and settings while the local server keeps the session. New session and
reload start with undefined `ans` and empty history. FIFO session eviction or
server restart requires reloading the page.
The CLI confirms each successful expression and supports `history` and `reset`.

The web server also retains a preview value per session.
Changing the display reuses it when safe; a changed working precision recomputes
context-dependent expressions. Exact factorials can be reformatted without
recalculating. Confirmations invalidate previews that depend on the previous `ans`.
The collapsed desktop calculator keeps its controls at normal scale within
ordinary viewport heights; very short windows can scroll the calculator column.

Both the interactive CLI and local HTTP adapter accept expressions up to 4096
UTF-8 bytes. This is an application input limit rather than a limit of the
numeric types themselves.

## Library API

For an in-tree build or a project that includes NumForge with
`add_subdirectory()`, include the public headers and link the
`NumForge::numforge` CMake target (`numforge` remains available as its local
target name):

```c
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include <numforge/bigrational.h>
```

The public API, ownership rules, arithmetic semantics, and concise function
reference for all three types are in [the API overview](docs/API.md).

To install the library, headers, applications, and CMake package into a chosen
prefix:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release --config Release
cmake --install build-release --config Release --prefix path/to/prefix
```

An installed consumer can then use:

```cmake
find_package(NumForge CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE NumForge::numforge)
```

### Stable 1.x API scope

The public API consists of `include/numforge/bigint.h`,
`include/numforge/bigdecimal.h`, `include/numforge/bigrational.h` and optional
`include/numforge/runtime.h`.
Existing 1.x numeric signatures remain compatible. Calculator modules and `src/web/` are
application code, not public C library headers. The loopback HTTP endpoint is
documented for local use, but is not an Internet-facing service or a separately
versioned remote API.

## Testing

The repository contains focused unit, property, and integration test
executables:

- `bigint_tests`: focused unit and regression tests.
- `bigint_property_tests`: deterministic generated tests of algebraic
  identities, multi-limb boundaries, and aliasing behavior.
- `bigdecimal_tests`: covers parsing, canonical form, exact arithmetic,
  aliasing, division, and rounding modes.
- `bigdecimal_property_tests`: deterministic generated reference checks for
  conversion, exact arithmetic, comparison, rescaling, division, rounding,
  and aliasing.
- `constants_tests`: stored and dynamic π/e/φ precision, rounding, validation,
  and destination-preservation checks.
- `trigonometric_tests`: forward/inverse radian values, large-argument
  reduction, domains, rounding, aliasing, and destination preservation.
- `hyperbolic_tests`: forward/inverse values, tiny and extreme arguments,
  domains, rounding, aliasing, and destination preservation.
- `calculator_tests`: covers tokenization, parsing, evaluation, source
  positions, and division policy.
- `calculator_contract_tests`: covers numeric-token boundaries, implicit
  products, and intermediate-rounding/cancellation examples.
- `parser_fuzz_tests`: deterministic bounded random-byte parser smoke test.
- `function_calls_tests`: registered names, arity, nesting, aliases, argument
  limits and preservation of constants/implicit multiplication.
- `fuzz_parser_smoke`, `fuzz_numbers_smoke`, `fuzz_formatter_smoke` and
  `fuzz_http_smoke`: portable replay of the optional Clang fuzz harnesses.
- `http_request_tests`: socket-independent framing, partial requests, origin
  checks and malformed/oversized headers and bodies.
- `cli_tests`: when Node.js is available, drives the actual CLI process through
  calculation, precision changes, errors, oversized input, EOF and exit commands.
- `numeric_oracle_tests`: optional Node.js exact-integer/rational reference
  checking 7012 numeric cases through a test-only C driver.
- `web_api_tests`: confirms that the local web adapter evaluates expressions
  through the same exact C `BigDecimal` pipeline.
- `web_server_smoke_tests`: starts the real server on a temporary loopback
  port and verifies HTML, embedded CSS/JS assets, calculation, same-origin policy, and HTTP error
  responses over sockets.
- `allocation_failure_tests`: fails each internal allocation in turn and checks
  out-of-memory propagation, cleanup, and the strong destination-unchanged
  guarantee across BigInt, BigDecimal, and the calculator pipeline.
- `web_ui_tests`: when Node.js is available, executes the shared web source
  script with a controlled SK/EN DOM/network to check stale responses,
  input changes, copying, UTF-8 limits and transport errors. No npm install
  or Node.js runtime dependency is added to the application.
- `tests/package_consumer`: a separate project built by CI against the
  installed package through `find_package(NumForge)`, including a C++ linkage
  test when `NUMFORGE_TEST_CPP=ON`.

CI also runs SK/EN Chromium scenarios from `tests/browser`, using a
pinned Playwright dependency and the real C server, separately from CTest.
Only this browser suite requires npm packages; the application does not.
Reproduction commands, scope, direct arithmetic/conversion and calculator-phase benchmarks are in
[TESTING.md](docs/TESTING.md).

The native and dependency-free Node.js suites run through CTest when
`BUILD_TESTING=ON`; CI requires Node.js, while local builds can omit it.
GitHub Actions builds and runs
them on x64 Linux with warnings-as-errors and sanitizers, on 32-bit Linux,
on x64 Windows with Visual Studio warnings-as-errors, and on ARM64 Linux and
macOS in Release mode with warnings-as-errors. The ARM64 jobs also run the CLI
and local-server smoke tests. CI builds clean Release packages, installs them,
and tests external C and C++ `find_package(NumForge)` consumers on x64 Linux,
x64 Windows and both ARM64 platforms.

Fault injection is internal test instrumentation, not public API. It is enabled
only in test-enabled builds and remains inactive unless the dedicated test
explicitly selects an allocation call to fail. With `BUILD_TESTING=OFF`, the
fault injector is absent. The internal allocation boundary still enforces a
thread-local budget when the calculator explicitly opens one; ordinary public
numeric calls use the standard allocator without an application budget.

## Project status and roadmap

NumForge 1.0 provides stable `BigInt` and `BigDecimal` library APIs plus the
initial exact-decimal CLI and local browser calculator. The additive BigRational
API is available in the current unreleased version. Future work is mostly
additive: broader test coverage, performance optimization for very large
operands, and calculator features. Planned work includes:

1. Broaden `BigDecimal` with larger generated decimal vectors, optional
   external-oracle checks, and performance optimizations. Its representation
   and implementation notes are in [the BigDecimal design](docs/BIGDECIMAL_DESIGN.md).
2. Calculator variables and additional scientific functions. Exponentiation,
   trigonometry and configurable output precision are already implemented. Its module boundaries, grammar,
   and evaluation policy are in [the calculator design](docs/CALCULATOR_DESIGN.md).
3. Performance profiling and targeted optimization of very large operands.

The public C headers follow semantic versioning. Incompatible public API
changes are reserved for a future major release.

## License

NumForge is available under the [MIT License](LICENSE). Copyright (c) 2026
Interacti0n.
