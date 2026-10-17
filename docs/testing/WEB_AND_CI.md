# Browser tests and CI

## Real-browser tests (optional locally)

`tests/browser` pins Playwright and its Chromium revision through the committed
npm lockfile. It is separate from CTest and adds no application runtime
dependency. Build `numforge_web` first, then run from `tests/browser`:

```sh
npm ci --ignore-scripts --no-audit --no-fund
npx playwright install chromium
NUMFORGE_WEB_EXECUTABLE=/absolute/path/to/build/numforge_web npm test
```

In PowerShell, use the same install commands, then:

```powershell
$env:NUMFORGE_WEB_EXECUTABLE = 'C:/path/to/NumForge/build/Debug/numforge_web.exe'
npm test
```

Adjust the executable path for your generator/configuration. Playwright starts
and stops its own loopback server on port 18765; set `NUMFORGE_TEST_PORT` to
another free port if necessary. It refuses to reuse an existing server.
Chromium scenarios cover both languages: real C calculations, precision and
notation changes, keypad entry, clipboard, help/navigation, arithmetic errors,
transport failures and stale-response protection. Network-failure and delayed
response cases use controlled interception; ordinary calculations reach C.
Session API scenarios cover lossless typed snapshots, pinned pagination,
read-only RNG/preview preservation, reset/release and FIFO expiration,
independent conversion revisions and retry without reevaluation. Application
tests verify owned ans after clearing history; allocation fault injection checks
conversion preparation and serialization preflight before state changes.
Function-group tests also cover keyboard expansion, arity errors, the RAD/DEG
selector, active integer/root/exponential/logarithmic/trigonometric/hyperbolic functions, five-line result
expansion and mobile layout.
`function_calls_tests` covers all
registered names, syntax/depth/argument limits and e/E boundaries; allocation
failure tests exercise partial nested calls and argument-array growth.

The browser CI matrix runs two shards, each with one worker and unchanged
individual test assertions/timeouts. Together they run the full suite; locally,
`npm test -- --shard=1/2` and `npm test -- --shard=2/2` reproduce those partitions.
Each shard allows twenty minutes including build/install time and has a distinct
failure artifact name. The dedicated browser CI job installs Chromium with OS dependencies using
`npx playwright install --with-deps chromium`. Failures upload the HTML report,
screenshots and traces for 14 days. Local reports, dependencies and test results
are ignored by Git. That CI job remains Chromium-only.

### Optional Firefox and WebKit graph checks

The separate configuration reuses the same C server and runs graph/guide tests
in both engines, with desktop and narrow viewport scenarios from those tests:

```powershell
npx playwright install firefox webkit
$env:NUMFORGE_WEB_EXECUTABLE = 'C:/path/to/NumForge/build/Release/numforge_web.exe'
npx playwright test --config playwright.cross-browser.config.js
```

Run these commands from `tests/browser`. Optionally set `PLAYWRIGHT_BROWSERS_PATH`
to an absolute directory under `build/` for project-local browser downloads.
Reports are written to `playwright-report-cross-browser/`, and artifacts to
`test-results-cross-browser/`; both are ignored by Git. Clipboard permissions
are omitted because these scenarios do not need them and engines differ in
permission support. Three native multi-touch tests use Chromium's DevTools
protocol and explicitly skip in other engines; ordinary touch taps still run.
WebKit on Windows is not Safari on an iPhone. Physical-device pinch/pan,
viewport resizing and expanded-view checks remain necessary.

## Coverage and package CI

Use a fresh GCC build without sanitizers for coverage:

```sh
cmake -S . -B build-coverage -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug -DNUMFORGE_ENABLE_COVERAGE=ON
cmake --build build-coverage --parallel 2
ctest --test-dir build-coverage --output-on-failure
cmake --build build-coverage --target coverage_report
```

`build-coverage/coverage/summary.txt` contains per-file line/branch statistics;
the adjacent `.gcov` files annotate source lines and individual branch counts.
The report selects production `src/` objects, excluding Unity and test sources.
Use a gcov matching the compiler (`NUMFORGE_GCOV` can override discovery).
It reports measured coverage, not correctness, and has no percentage gate.
Fresh build directories avoid accumulated counts from earlier runs.
For compiler instrumentation details, see [GCC coverage options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html).

The HTTP smoke test terminates its server subprocess. Such termination can
prevent gcov counters from being flushed: server coverage may be missing/zero
even though socket tests passed. Do not interpret this as measured non-execution.
Coverage CI uploads the text report and annotated files as the `gcc-coverage`
artifact, retained for 14 days, using [upload-artifact](https://github.com/actions/upload-artifact).

The Clang job runs the standard suite with sanitizers before fuzzing. The
Linux ARM64 and macOS ARM64 jobs build the complete project in Release mode
with warnings-as-errors and run CTest, including the CLI and HTTP server smoke
tests. The x64 Linux, x64 Windows and both ARM64 installation jobs build
separate C and C++ consumers against the installed package. To enable the
latter locally, configure `tests/package_consumer` with `-DNUMFORGE_TEST_CPP=ON`
and a matching `CMAKE_PREFIX_PATH`; on Windows also match architecture and
configuration.
The ordinary library build still requires only a C compiler.
