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

## Extend the platform

Use the [architecture source map and dependency rules](docs/design/ARCHITECTURE.md)
to choose an owner. Core representations live in `src/core/`, mathematical
capabilities in `src/modules/`, parsing/evaluation in `src/engine/`, and sessions
and workflow state in `src/application/`. CLI and web transport/assets live in
`apps/`; language adapters live in `bindings/`. Create directories when they gain
an implementation, not for speculative modules or bindings.
For native Python adapters, register sources in `bindings/python/CMakeLists.txt`
and reuse `native_internal.h` support. Include public C APIs; the architecture
check rejects private `src/` headers and unregistered adapter sources.

For a new mathematical capability:

1. Define the public contract in `include/numforge/`, using opaque owned handles
   where appropriate. Specify exactness, precision units, errors and lifetimes.
2. Implement the algorithm once in its owning module. Add sources explicitly to
   that module's `CMakeLists.txt`; a new module is registered by one
   `add_subdirectory` in `src/modules/CMakeLists.txt`. Do not use source globs.
3. Add independent numerical, boundary and failure checks, an installed consumer
   example and a benchmark for expensive operations. Follow the
   [feature checklist](docs/project/FEATURE_CHECKLIST.md).
4. Add thin engine/binding/application adapters where supported. Keep exact input
   lossless and typed results separate from their display text. UI code presents
   results; it does not reimplement the mathematics.
5. Document workflows in the relevant guide and link the API index to the contract.
   Avoid duplicating public declarations and historical implementation narratives.

Documentation ownership, local MkDocs/Doxygen builds and executable-example
checks are described in the [documentation workflow](docs/project/DOCUMENTATION.md).

Use repository-qualified paths for private headers and `<numforge/...>` for public
headers. Keep module-private headers inside their owning module and read opaque
results through public getters. The existing equation research hook is limited
to strategy callbacks, without result layouts. Keep only the public include
directory in installed target interfaces.
Run `node scripts/check_architecture.cjs` and `node scripts/check_docs.cjs` in
addition to relevant tests. CTest runs the architecture check when Node is present.
For broad structural changes, also validate a numeric-only installation, external
C/C++ consumers, Python source/wheel packaging and affected browser flows as
described in [TESTING.md](docs/TESTING.md).

Comments should explain invariants, numerical choices, ownership or surprising
constraints; avoid restating each line. Keep disposable builds and measurements
under `build/`; reusable tools belong in `scripts/`. Edit web assets under
`apps/web/assets/`, keep English/Slovak behavior aligned and rebuild the embedded
server. Never edit generated asset headers.

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
