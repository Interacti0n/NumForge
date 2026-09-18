# NumForge

NumForge is a C17 mathematics library with CLI and web clients. It provides
two public numeric types: signed arbitrary-precision `BigInt` and base-10
`BigDecimal`. The command-line and local browser calculators share the same C
tokenizer, parser, evaluator, and BigDecimal implementation.
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
- Arbitrary-precision real roots, exponential, natural logarithm, common
  logarithm, and logarithms with a caller-selected base.
- Arbitrary-precision radian sine, cosine, tangent, and inverse trigonometric
  functions with guarded large-argument reduction.
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
- Unit tests, deterministic property tests, warnings-as-errors, and Linux and
  Windows CI, including deterministic allocation-failure injection.

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
| [API overview](docs/API.md) | Public `BigInt` and `BigDecimal` API, ownership rules, calculator syntax, and local HTTP API. |
| [BigInt design](docs/BIGINT_DESIGN.md) | Limb representation, semantics, and optimization boundaries. |
| [BigDecimal design](docs/BIGDECIMAL_DESIGN.md) | Exact-decimal representation, rounding, and future work. |
| [Calculator design](docs/CALCULATOR_DESIGN.md) | Expression grammar, evaluation policy, and CLI/web integration. |
| [Testing guide](docs/TESTING.md) | Unit, integration, browser, fuzz, and performance checks. |

## Project layout

```text
NumForge/
├── include/numforge/   Public BigInt, BigDecimal, and runtime headers
├── src/
│   ├── bigint/        Integer implementation and private helpers
│   ├── bigdecimal/    Decimal implementation and private helpers
│   ├── internal/      Allocators, resource budgets, and test instrumentation
│   ├── calculator/    Expression syntax and evaluation using the public library
│   ├── web/           Local HTTP server, adapter, and embedded SK/EN pages
│   └── main.c         CLI entry point
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

Local build trees, editor state, browser reports, and `PROJECT_REVIEW.md`
are ignored by Git. Browser npm dependencies are test tooling; running the
calculator requires no Node.js installation.

## Build

### Build the library and demo

```sh
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build --parallel
```

### Build and run tests

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

With a multi-configuration generator such as Visual Studio, specify a
configuration:

```sh
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

For stricter local verification with GCC or Clang:

```sh
cmake -S . -B build -DBUILD_TESTING=ON \
  -DNUMFORGE_WARNINGS_AS_ERRORS=ON \
  -DNUMFORGE_ENABLE_SANITIZERS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`NUMFORGE_ENABLE_SANITIZERS` enables AddressSanitizer and
UndefinedBehaviorSanitizer on GCC and Clang.

### Run the local web calculator

No Node.js, package manager, database, or external service is needed. Build
the project and start the `numforge_web` executable. On Windows with the
default Visual Studio generator:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Debug --parallel 2
.\build\Debug\numforge_web.exe
```

With a single-configuration generator, as normally used on Linux and macOS:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
./build/numforge_web
```

On Windows, the executable automatically opens `http://127.0.0.1:8765` in the
default browser. It listens only on the local machine; press `Ctrl+C` in the
terminal to stop it. Use `--port 9000` to select another loopback port and
`--no-browser` when the page should not open automatically. The existing
`NUMFORGE_WEB_NO_BROWSER=1` environment setting is also supported for headless
runs.

The page includes a clickable keypad for the current expression grammar,
including `π`, `e`, `φ`, `xʸ`, `x²`, `x³`, and `n!`. Powers, squaring, and
cubing accept any exact decimal base with a non-negative whole-number exponent;
factorial requires an input from 0 to 10000. Its precision control defaults to
10 decimal places (configurable from 0 to 10000); full output is also available.
Non-terminating division uses working significant digits, so `1E-40 / 1` remains `1E-40`.
The complete calculation has a five-second monotonic budget, checked during
parsing, arithmetic and formatting, including expensive BigInt loops. The
calculator also bounds allocations and output size. Exceeding these limits
returns `TLE` or `value too large`; public numeric library calls remain uncapped
by these application policies. Cancellation is cooperative, not a hard real-time
process-kill guarantee.
Named calls use parentheses and semicolons: `pow(2;3)` and `factorial(5)`
already calculate through the existing operators. `abs`, `sign`, `min` and `max`
also calculate using decimal values without additional rounding. Integer-valued
arguments support `gcd`, `lcm`, `mod` and floor square root `isqrt`. Real roots
`sqrt`/`√`, `cbrt` and `root(x;n)` preserve exact finite roots and otherwise use
working precision (34 significant digits by default). Trigonometric and inverse
trigonometric calls use the shared RAD/DEG selector; explicit `radians(x)` and
`degrees(x)` conversions remain available. Four horizontal tabs contain the
function controls, with signatures and domain hints on hover, focus or touch.
An indicator beside the expression mirrors the shared angle mode. Connection
and unexpected-response errors offer retry with Enter.
The page is available in Slovak and English, and the displayed
result can be copied with one click. See the
[API overview](docs/API.md) for exact syntax and the local HTTP API.
Long results stay in a compact five-line panel and can be expanded with
`Show all` when needed.
Working precision remains automatic; only output precision is user-configurable.
The web server keeps one successful numeric result per page (up to eight pages).
Changing the display reuses it when safe; a changed working precision recomputes
context-dependent expressions. Exact factorials can be reformatted without
recalculating. Reloading the page or cache eviction may require a fresh calculation.
The collapsed calculator fits the viewport height, scaling down in short
windows. Vertical page scrolling is needed only while a long result is expanded.

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
```

The public API, ownership rules, arithmetic semantics, and concise function
reference for both types are in [the API overview](docs/API.md).

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
`include/numforge/bigdecimal.h` and optional `include/numforge/runtime.h`.
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
  port and verifies HTML, calculation, same-origin policy, and HTTP error
  responses over sockets.
- `allocation_failure_tests`: fails each internal allocation in turn and checks
  out-of-memory propagation, cleanup, and the strong destination-unchanged
  guarantee across BigInt, BigDecimal, and the calculator pipeline.
- `web_ui_tests`: when Node.js is available, executes the actual embedded
  SK/EN scripts with a controlled DOM/network to check stale responses,
  input changes, copying, UTF-8 limits and transport errors. No npm install
  or Node.js runtime dependency is added to the application.
- `tests/package_consumer`: a separate project built by CI against the
  installed package through `find_package(NumForge)`, including a C++ linkage
  test when `NUMFORGE_TEST_CPP=ON`.

CI also runs twenty SK/EN Chromium scenarios from `tests/browser`, using a
pinned Playwright dependency and the real C server, separately from CTest.
Only this browser suite requires npm packages; the application does not.
Reproduction commands, scope and opt-in phase benchmarks are in
[TESTING.md](docs/TESTING.md).

The native and dependency-free Node.js suites run through CTest when
`BUILD_TESTING=ON`; CI requires Node.js, while local builds can omit it.
GitHub Actions builds and runs
them on 64-bit Linux with warnings-as-errors and sanitizers, on 32-bit Linux,
and on Windows with Visual Studio warnings-as-errors. CI also builds a clean
Release package, installs it, and tests an external `find_package(NumForge)`
consumer.

Fault injection is internal test instrumentation, not public API. It is enabled
only in test-enabled builds and remains inactive unless the dedicated test
explicitly selects an allocation call to fail. With `BUILD_TESTING=OFF`, the
fault injector is absent. The internal allocation boundary still enforces a
thread-local budget when the calculator explicitly opens one; ordinary public
numeric calls use the standard allocator without an application budget.

## Project status and roadmap

NumForge 1.0 provides stable `BigInt` and `BigDecimal` library APIs plus the
initial exact-decimal CLI and local browser calculator. Future work is mostly
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
