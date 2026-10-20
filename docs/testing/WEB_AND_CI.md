# Browser, CI and coverage checks

## Browser tests

Install dependencies in `tests/browser`, select your built server, then run:

```sh
cd tests/browser
npm ci
npx playwright install chromium
```

```powershell
$env:NUMFORGE_WEB_EXECUTABLE = 'C:/path/to/NumForge/build/Release/numforge_web.exe'
npx playwright test
```

On POSIX set the same environment variable before the command. Use a concrete
server path for your build. Playwright owns its loopback server and rejects reuse
of an occupied port; `NUMFORGE_TEST_PORT` overrides the test port.
Run a filename for a focused suite or `--shard=1/2` / `--shard=2/2` for CI partitions.

Controlled request interception exercises failures and delayed responses;
ordinary calculations reach C. Failure traces/reports are ignored. Custom
screenshots/JSON use `tests/browser/artifacts.js` under `build/browser-artifacts/`.

For optional Firefox/WebKit graph checks:

```sh
npx playwright install firefox webkit
npx playwright test --config playwright.cross-browser.config.js
```

Chromium-specific multi-touch checks skip on other engines. Desktop mobile
emulation and Windows WebKit do not establish physical Android/iPhone behavior.

## CI responsibilities

The [CI workflow](https://github.com/Interacti0n/NumForge/blob/main/.github/workflows/ci.yml) owns exact platforms, shard sizes,
timeouts and artifact retention; do not duplicate its changing job inventory here.
It covers native tests, independent references, benchmark smoke checks, sanitizers,
fuzzing, coverage, browser shards, installed C/C++ consumers, extracted development
archives, Python packages and
documentation in one run per push. Independent jobs run in parallel; a newer
revision cancels an unfinished run for the same branch or pull request.
The separate [release workflow](https://github.com/Interacti0n/NumForge/blob/main/.github/workflows/release-binaries.yml)
builds published/manual release packages, without running on ordinary pushes.
Configured jobs are not evidence that an unpublished local change passed remote CI.

## Coverage

Use a fresh GCC build without sanitizers:

```sh
cmake -S . -B build/coverage -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug -DNUMFORGE_ENABLE_COVERAGE=ON
cmake --build build/coverage --parallel 2
ctest --test-dir build/coverage --output-on-failure
cmake --build build/coverage --target coverage_report
```

`build/coverage/coverage/summary.txt` reports production line/branch counts;
adjacent `.gcov` files show details. Match gcov to the compiler. Coverage is not
correctness and has no percentage gate. Terminating a server subprocess can
prevent gcov flushes, so absent server counts do not prove non-execution.
See [Testing](../TESTING.md#installed-packages-and-python) for package checks.
