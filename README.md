# NumForge

NumForge is a C17 mathematics library and exact-decimal calculator. It provides
two public numeric types: signed arbitrary-precision `BigInt` and base-10
`BigDecimal`. The command-line and local browser calculators share the same C
tokenizer, parser, evaluator, and BigDecimal implementation.

## Features

- Signed arbitrary-precision integers stored internally in base 2<sup>64</sup>.
- Decimal parsing and formatting.
- Addition, subtraction, multiplication, division, modulo, powers, GCD, LCM,
  and factorial.
- Bitwise operations and shifts.
- Perfect-square and Miller-Rabin probable-prime checks.
- Exact decimal arithmetic with configurable rounding for division and
  rescaling.
- Interactive expression calculator with source-positioned diagnostics.
- Built-in 200-decimal-place approximations of `π`, `e`, and `φ` in the
  calculator syntax.
- Configurable result precision, full output mode, and readable scientific
  notation for very large or very small non-zero results.
- Local browser calculator served directly by the C executable; its requests
  are evaluated by the same parser and `BigDecimal` core.
- Output/input aliasing for arithmetic operations, including
  `bigint_add(x, x, y)` and `bigint_div_mod(q, r, q, r)`.
- Unit tests, deterministic property tests, warnings-as-errors, and Linux CI.

## Requirements

- CMake 3.20 or later
- A compiler with C17 support
- Internet access on the first test-enabled CMake configure, so CMake can
  download the Unity test framework

## Documentation

| Document | Purpose |
| --- | --- |
| [API overview](docs/API.md) | Public `BigInt` and `BigDecimal` API, ownership rules, calculator syntax, and local HTTP API. |
| [BigInt design](docs/BIGINT_DESIGN.md) | Limb representation, semantics, and optimization boundaries. |
| [BigDecimal design](docs/BIGDECIMAL_DESIGN.md) | Exact-decimal representation, rounding, and future work. |
| [Calculator design](docs/CALCULATOR_DESIGN.md) | Expression grammar, evaluation policy, and CLI/web integration. |

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
the project and start the `numforge_web` executable:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Debug --parallel
./build/Debug/numforge_web
```

On Windows, the executable automatically opens `http://127.0.0.1:8765` in the
default browser. It listens only on the local machine; press `Ctrl+C` in the
terminal to stop it. With a single-configuration generator, the executable is
normally at `build/numforge_web` instead.

The page includes a clickable keypad for the current expression grammar,
including `π`, `e`, `φ`, `xʸ`, `x²`, `x³`, and `n!`. Powers, squaring, and
cubing accept any exact decimal base with a non-negative whole-number exponent;
factorial requires a non-negative whole number. Its precision control defaults
to 10 decimal places; full output is also available.
The dimmed function buttons are intentionally inactive and show planned
features. See the
[API overview](docs/API.md) for exact syntax and the local HTTP API.

## Library API

Include the public headers and link the `numforge` CMake target:

```c
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
```

The public API, ownership rules, arithmetic semantics, and concise function
reference for both types are in [the API overview](docs/API.md). The design
documents explain the internal representation and decisions that should remain
stable while the library is still pre-1.0.

## Testing

The repository contains focused unit, property, and integration test
executables:

- `bigint_tests`: focused unit and regression tests.
- `bigint_property_tests`: deterministic generated tests of algebraic
  identities, multi-limb boundaries, and aliasing behavior.
- `bigdecimal_tests`: covers parsing, canonical form, exact arithmetic,
  aliasing, division, and rounding modes.
- `calculator_tests`: covers tokenization, parsing, evaluation, source
  positions, and division policy.
- `web_api_tests`: confirms that the local web adapter evaluates expressions
  through the same exact C `BigDecimal` pipeline.

All run through CTest when `BUILD_TESTING=ON`. GitHub Actions builds and runs
them on Linux with warnings treated as errors and sanitizers enabled.

## Project status and roadmap

`BigInt` and the initial exact-decimal calculator are complete foundations.
Remaining work is mostly broader test coverage, performance optimization for
very large operands, and calculator features. Planned work includes:

1. Extend `BigDecimal` with property tests, larger generated decimal vectors,
   and performance optimizations. Its representation and implementation notes are in
   [the BigDecimal design](docs/BIGDECIMAL_DESIGN.md).
2. Calculator variables, functions, exponentiation, and configurable runtime
   precision. Its module boundaries, grammar, and evaluation policy are in
   [the calculator design](docs/CALCULATOR_DESIGN.md).
3. Additional BigDecimal property/reference tests and performance profiling.

The public API is still pre-1.0 and may evolve.
