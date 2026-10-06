# Contributing to NumForge

NumForge is a C17 numeric library with optional command-line and local web
calculators. Small, focused contributions are easier to review. For a new public
API or a substantial algorithm change, open an issue describing the use case and
proposed contract before building a large implementation.

## Build and test

Install a C17 compiler and CMake 3.20 or newer. Git and network access are needed
for the first test build to fetch Unity; Node.js enables the CLI, independent
numeric oracle and web-script checks.

```sh
git clone https://github.com/Interacti0n/NumForge.git
cd NumForge
cmake -P cmake/LocalBuild.cmake
```

The helper creates a separate build for your checkout, builds Release and runs
CTest with warnings-as-errors. On Linux/macOS, install a C/C++ toolchain; on
Windows, use Visual Studio's C++ build tools or a supported GCC setup. Consult
[TESTING.md](docs/TESTING.md) for explicit builds, sanitizers, browser checks,
32-bit/ARM64 coverage and installed C/C++ package consumers.

## Where changes belong

- Public numeric APIs: `include/numforge/` and `src/bigint/`, `src/bigdecimal/`,
  `src/bigrational/`. Keep representation details private.
- Calculator parsing, typed evaluation and sessions: `src/calculator/`.
- Local HTTP transport: `src/web/`.
- Browser UI: `web/`. Keep English and Slovak behavior aligned; rebuild the
  server after editing assets. Never edit the generated embedded-asset header.
- Tests: `tests/`; opt-in measurement harnesses: `benchmarks/`.

The [architecture overview](docs/ARCHITECTURE.md) explains the boundaries.
Calculators compose the numeric API rather than duplicating its algorithms.

## Numerical changes

Preserve documented aliasing and unchanged-output-on-failure guarantees. A new
public function needs a complete input/precision/rounding/ownership/error
contract, tests, allocation-failure coverage, consumer validation and docs.

Use independent exact references for integer/rational/decimal arithmetic and
documented high-precision references for approximate functions. Floating-point
`double` values or identities alone are insufficient for high-precision accuracy.
Cover cancellation, small arguments, large magnitudes, domain boundaries and
rounding edges relevant to the change. Existing directed-rounding exceptions are
documented in the test methodology; do not silently broaden their tolerance.

For an optimization, compare matching inputs, precision and compiler settings
against repeated Release baselines. Keep live-memory probes outside timing and
validate the result before reporting a speedup. See the benchmark docs.

## Submit a focused pull request

Describe the problem, resulting behavior and relevant validation. Include a
before/after example when it helps. Update docs and the Unreleased changelog for
user-visible behavior or public API changes. Run checks appropriate to the scope
and `git diff --check`; avoid unrelated formatting, generated build output and
local baseline artifacts. Documentation-only changes normally need link/example
verification rather than a full numerical regression run.

Report bugs through [GitHub issues](https://github.com/Interacti0n/NumForge/issues).
Include the commit/version, OS/compiler, minimal reproduction, actual result and
expected result with its reference. Redact credentials and private information
from logs.
