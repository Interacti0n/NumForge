# NumForge

NumForge 2.0 is a C17 mathematics library for arbitrary-precision integers
(`BigInt`), exact decimals (`BigDecimal`), and reduced fractions
(`BigRational`). It also includes optional command-line and local browser
calculators. Both calculators use the same C expression parser and numeric
library.

## What it does

- Integer, decimal, and rational arithmetic, including configurable decimal
  rounding and conversion between numeric types.
- Powers, roots, logarithms, trigonometric and hyperbolic functions, constants,
  and statistics in the calculators.
- Exact rational results where possible; precision-aware approximations for
  results that cannot be represented exactly.
- A Slovak/English browser calculator served by a self-contained local
  executable. Running it requires no Node.js, database, or external service.
- Unit, property, integration, browser, fuzz, and package-consumer checks.

The numeric library is available through the public headers in
[`include/numforge/`](include/numforge/). Calculator syntax, the local HTTP
endpoint, and application limits are documented in the [API overview](docs/API.md).

## Get started

Requires CMake 3.20 or later and a C17 compiler.

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

## Test

```sh
cmake -P cmake/LocalBuild.cmake
```

This configures a local Release build, builds it, and runs CTest. The first
test-enabled configuration needs Git and internet access to fetch Unity.
Node.js enables the optional CLI, numeric-oracle, and web-script tests; the
browser suite additionally uses Playwright. See the [testing guide](docs/TESTING.md)
for individual checks, CI coverage, and benchmarks.

## Documentation

| Document | Contents |
| --- | --- |
| [Library guide](docs/LIBRARY_GUIDE.md) | Build, install, and consume the C library. |
| [API overview](docs/API.md) | Public types, ownership, calculator syntax, and local HTTP API. |
| [BigInt design](docs/BIGINT_DESIGN.md) | Representation, semantics, and implementation. |
| [BigDecimal design](docs/BIGDECIMAL_DESIGN.md) | Decimal representation, rounding, and future work. |
| [Calculator design](docs/CALCULATOR_DESIGN.md) | Parser, evaluation, CLI/web behavior, and limits. |
| [Web source](web/README.md) | Editable HTML, CSS, JavaScript, and embedded asset build. |
| [Testing guide](docs/TESTING.md) | Test suites, CI, fuzzing, and performance checks. |

The library lives in `src/bigint/`, `src/bigdecimal/`, and
`src/bigrational/`. The calculator and HTTP adapters live in
`src/calculator/` and `src/web/`; editable browser assets live in `web/`.

## License

NumForge is available under the [MIT License](LICENSE). Copyright (c) 2026
Interacti0n.
