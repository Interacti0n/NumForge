# NumForge

<img align="right" src="web/logo.png" alt="NumForge logo" width="96" height="96">

**Exact arithmetic. High-precision math. One C17 core.**

[![CI](https://github.com/Interacti0n/NumForge/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Interacti0n/NumForge/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/Interacti0n/NumForge)](https://github.com/Interacti0n/NumForge/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C17](https://img.shields.io/badge/C-17-00599C.svg)](docs/LIBRARY_GUIDE.md)
[![CMake 3.20+](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg)](CMakeLists.txt)

NumForge 2.0 is a C17 mathematics library for arbitrary-precision integers
(`BigInt`), exact decimals (`BigDecimal`), and reduced fractions
(`BigRational`). It also includes optional command-line and local browser
calculators. Both calculators use the same C expression parser and numeric
library.

[Try it locally](#get-started) · [See the calculator](#in-the-browser) ·
[Explore the architecture](#architecture) · [Browse the API](docs/API.md) ·
[Latest release](https://github.com/Interacti0n/NumForge/releases/latest)

## NumForge in 15 seconds

Try these in either calculator. Select **Full** precision to keep every digit
of exact results, with **Plain** notation for decimals/integers or **Fraction**
notation for fractions:

| Expression | Result |
| --- | --- |
| `0.1 + 0.2` | `0.3` — exact decimal arithmetic |
| `2^128` | `340282366920938463463374607431768211456` |
| `1/3 + 1/6` | `1/2` — an exact, reduced fraction |
| `sqrt(4/9)` | `2/3` — an exact rational root |

For an irrational result, choose **Plain** notation and **40** output places:

```text
sqrt(2) ≈ 1.4142135623730950488016887242096980785697
```

You can also calculate `100!` as an exact 158-digit integer, or request more
than 500 digits of π, e and φ, computed dynamically. The local browser calculator
runs entirely on your machine using the same C library as the command line.

## In the browser

Exact fractions, a separate decimal hint, confirmed history and a searchable
function library in the current English interface:

![Desktop calculator showing an exact fraction and four confirmed calculations](docs/images/calculator-desktop.png)

<details>
<summary>See the responsive mobile calculator</summary>

<p><img src="docs/images/calculator-mobile.png" alt="Mobile calculator with exact fraction, keypad, function library and session history" width="320"></p>

</details>

Screenshots show the current development interface; release archives may have an
earlier layout. The calculator is available in English and Slovak.

## What it does

- Integer, decimal, and rational arithmetic, including configurable decimal
  rounding and conversion between numeric types.
- Powers, roots, logarithms, trigonometric and hyperbolic functions, constants,
  and statistics in the calculators.
- Exact rational results where possible; precision-aware approximations for
  results that cannot be represented exactly.
- A Slovak/English browser calculator served by a self-contained local
  executable. Running it requires no Node.js, database, or external service.
- [Unit conversion](docs/UNIT_CONVERTER.md) in the SK/EN browser, C and [local HTTP APIs](docs/UNIT_HTTP_API.md),
  with a sourced 233-unit catalogue and compatibility checks.
- Unit, property, integration, browser, fuzz, and package-consumer checks.

The numeric library is available through the public headers in
[`include/numforge/`](include/numforge/). Calculator syntax, the local HTTP
endpoint, and application limits are documented in the [API overview](docs/API.md).

## Get started

Requires CMake 3.20 or later and a C17 compiler.

Start from a [release source archive](https://github.com/Interacti0n/NumForge/releases/latest)
or clone the current development version:

```sh
git clone https://github.com/Interacti0n/NumForge.git
cd NumForge
```

Build the library and both calculators without test dependencies:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
```

Run `numforge_web` from the build directory (or `build/Release/` with Visual
Studio) to open the local calculator at `http://127.0.0.1:8765`. It listens
only on the local machine. Use `--port N` to choose a port or `--no-browser`
to suppress automatic browser opening. Run `calculator` for the interactive
command-line calculator.

For example, with a single-configuration build on Linux/macOS:

```sh
./build/numforge_web --port 8765
./build/calculator
```

On Windows with Visual Studio, run either application from PowerShell:

```powershell
.\build\Release\numforge_web.exe --port 8765
.\build\Release\calculator.exe
```

In the CLI, use `precision full` and `notation plain` for the exact decimal and
integer examples above, or `notation fraction` for exact fractions. Use
`precision 40` for the illustrated square root, and `quit` to exit. The latest
published release currently contains source archives. Portable Windows/Linux
binary packaging is prepared to run automatically when a release is published;
see [release packaging](docs/RELEASE_PACKAGING.md)
for the build, verification and publication process. A hosted demo remains future
distribution work.

For a library-only build and installation, see the [library guide](docs/LIBRARY_GUIDE.md).
An installed CMake consumer links `NumForge::numforge`:

```cmake
find_package(NumForge 2.0 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE NumForge::numforge)
```

Existing 1.x numeric C signatures remain source compatible in 2.0. CMake
consumers that explicitly requested package major version 1 must update their
`find_package` requirement. The calculator and local HTTP server are optional
clients, outside the public numeric C API.

## Architecture

The CLI and browser share the C expression parser, evaluator and session logic.
The browser sends expressions to a loopback HTTP server; JavaScript handles the
interface while the numerical work stays in C.

```mermaid
flowchart TD
    CLI[Command-line calculator] --> Calculator
    Web[Browser interface] --> HTTP[Local HTTP server and embedded assets]
    HTTP --> Calculator[Shared parser, typed evaluator and sessions]
    Calculator --> BI[BigInt: arbitrary-precision integers]
    Calculator --> BD[BigDecimal: exact base-10 values and approximate functions]
    Calculator --> BR[BigRational: exact reduced fractions]
    BD --> BI
    BR --> BI
    BR -->|decimal conversion| BD
```

Applications can also link the numeric library directly, without either
calculator. Public types are opaque; mutating operations preserve the destination
on failure and support the documented aliasing cases. Read the
[architecture overview](docs/ARCHITECTURE.md) for boundaries and ownership.

## Measured performance

Selected opt-in Release benchmark results on **Windows x64, Ryzen 7 7435HS,
MSVC 19.51**. Ranges span the medians of three recorded runs; parsing and
formatting are excluded from direct arithmetic timings.

| Operation | Input | Median range |
| --- | --- | ---: |
| BigInt multiplication | Two 4096-bit integers | 26.8–28.3 µs |
| BigInt multiplication | Two 65536-bit integers | 6.67–7.14 ms |
| Public factorial | `10000!` | 31.82–32.50 ms |
| Auto scientific formatting | 16384 decimal digits, 10 output places | 2.951–2.975 ms |
| BigDecimal addition, equal scales | Two 2000-digit coefficients | 1.357–1.456 µs |
| Square root | `sqrt(2)`, 1000 significant digits | 11.928–12.698 ms |

These instrumented measurements describe one machine, not universal speed
guarantees. The arithmetic suite checks 1084 scenarios against independent exact
references. Private optimization experiments are reported separately and are
not production features. See [BigInt measurements](docs/BIGINT_BENCHMARKS.md) and
[formatting/cache/memory methodology](docs/BENCHMARKS.md) to reproduce the runs,
inspect allocation costs and understand their limits.
The [decimal/math measurements](docs/DECIMAL_MATH_BENCHMARKS.md) document the
new suite, including high-precision timeouts and the extreme tangent-pole
accuracy limitation; those cases are excluded from successful timing claims.

## Current scope and roadmap

Development updates: tangent pole regressions now pass strict references
through 2,000 digits. Short scientific formatting uses outward-rounded bounds
with exact fallback. Benchmark reports preserve the historical failures and
record the subsequent fixes separately.

**Available:** the numeric C library, CLI and local bilingual browser calculator,
exact rational arithmetic, configurable precision/rounding, scientific functions,
session variables/history, BigInt bases 2–36, unit-conversion C/HTTP APIs,
and opt-in benchmarks.

**Next:** Accuracy follow-ups and optimizations supported by the collected data.
Direct [BigDecimal and higher-function measurements](docs/DECIMAL_MATH_BENCHMARKS.md)
include strict references and explicit stress-case limitations.
Session variables and BigInt conversion in bases 2–36 are implemented.
User functions and language bindings are future work. Graphs, equations and
account links currently lead to informational pages. See the
[roadmap](docs/ROADMAP.md) for scope and prerequisites; no delivery dates are promised.

## Test

```sh
cmake -P cmake/LocalBuild.cmake
```

This configures a local Release build, builds it, and runs CTest. The first
test-enabled configuration needs Git and internet access to fetch Unity.
Node.js enables the optional CLI, numeric-oracle, and web-script tests; the
browser suite additionally uses Playwright. See the [testing guide](docs/TESTING.md)
for individual checks, CI coverage, and benchmarks.

## Contributing

Bug reports, small fixes, documentation and independently checked numerical
cases are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md) for local setup,
validation and pull-request guidance. For a numerical issue, include the exact
expression or API call, precision, rounding mode, expected value and reference.

## Documentation

Browse the [documentation guide](docs/README.md) by task or the
[benchmark tools](benchmarks/README.md) by purpose.

| Document | Contents |
| --- | --- |
| [Variables](docs/VARIABLES.md) | Session assignments, exact snapshots and lifetime. |
| [Library guide](docs/LIBRARY_GUIDE.md) | Build, install, and consume the C library. |
| [API overview](docs/API.md) | Public types, ownership, calculator syntax, and local HTTP API. |
| [Architecture overview](docs/ARCHITECTURE.md) | Numeric layers, clients, ownership and project boundaries. |
| [BigInt design](docs/BIGINT_DESIGN.md) | Representation, semantics, and implementation. |
| [BigDecimal design](docs/BIGDECIMAL_DESIGN.md) | Decimal representation, rounding, and future work. |
| [Calculator design](docs/CALCULATOR_DESIGN.md) | Parser, evaluation, CLI/web behavior, and limits. |
| [Web source](web/README.md) | Editable HTML, CSS, JavaScript, and embedded asset build. |
| [Testing guide](docs/TESTING.md) | Test suites, CI, fuzzing, and performance checks. |
| [BigInt measurements](docs/BIGINT_BENCHMARKS.md) | Arithmetic baseline, exact references and private experiments. |
| [Formatting/cache measurements](docs/BENCHMARKS.md) | Timing phases, memory measurements and reproduction. |
| [Roadmap](docs/ROADMAP.md) | Available capabilities, planned work and prerequisites. |
| [Project presentation](docs/PROJECT_PRESENTATION.md) | README assets, screenshot capture and presentation maintenance. |
| [Release packaging](docs/RELEASE_PACKAGING.md) | Portable Windows/Linux archives, verification and publishing. |
| [Decimal/math measurements](docs/DECIMAL_MATH_BENCHMARKS.md) | Direct BigDecimal and high-precision math benchmarks, independent references and limitations. |

## Repository layout

```text
include/numforge/     Public C library headers
src/
  bigint/            Integer arithmetic
  bigdecimal/        Decimal arithmetic and scientific functions
  bigrational/       Exact rational arithmetic
  calculator/        Parsing, evaluation, formatting and sessions
  web/               Local HTTP server and API
  internal/          Private allocation and diagnostic support
  main.c             CLI entry point
web/                 Editable browser assets
tests/               Regression suites, browser checks and package consumers
benchmarks/          Opt-in measurement tools
  references/        Frozen math references and their generator
docs/                Guides, design notes and measurement reports
  images/            README screenshots
cmake/               Build, embedding and package helpers
scripts/             Release packaging tools
.github/workflows/   CI and release automation
```

Generated builds, installed staging files and raw benchmark recordings are
ignored local outputs; they are not part of the source tree.

## License

NumForge is available under the [MIT License](LICENSE). Copyright (c) 2026
Interacti0n.
