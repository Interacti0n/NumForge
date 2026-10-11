# Release readiness audit, 12 October 2026

This is a local Windows audit of the development branch after the bounded
factorial experiment. It does not record a published release or a completed
GitHub Actions run. The source version remains 2.2.0 and both changelogs retain
their Unreleased sections.

## Source and environment

Numeric/application code and archive builds use source commit
`9ab859876f197d18c439113ebbf4be7ea02fe2a3`. The audit also includes the test-only
MSVC build fix in `433ce35` and the accompanying browser-test/documentation
changes. None of these audit changes alter public numeric signatures, algorithms
or application resource limits.

Fresh builds are under ignored `build/release-audit-2026-10-12/`. Environment:
Windows x64, MSVC 19.51.36247.0, Visual Studio 2026, Release configuration,
warnings as errors, Node 24.19 and Playwright 1.63.0 Chromium.

## Findings and fixes

- The normalization allocation-failure regression used `strcat`, rejected by
  strict MSVC builds. Two bounded `memcpy` calls now construct the same input,
  including its terminating null byte. The overflow/failure assertion remains.
- Long complex-function browser scenarios waited for the two-second preview
  after every expression and exceeded their overall timeout. Their function
  catalogues now confirm with Enter and retain every expected-value assertion.
  Dedicated `preview.spec.js` scenarios still verify request counts, idle delay,
  immediate confirmation and cancellation in both languages and both tools.
- The converter guide now specifies the preview delay and immediate conversion.
  The roadmap lists implemented BigComplex support and keeps further factorial
  research deferred until after the overall release work.

## Validation

- Fresh Release test build: all 40 ordinary CTest checks and all 16 exhaustive
  allocation shards passed via `scripts/run_ci_tests.cmake`. Ordinary checks
  include independent numeric/division/complex references, C API, CLI, HTTP,
  session/unit tests, allocation-failure tests and fuzz smoke checks. Server
  checks run serially; exhaustive allocation shards use two workers.
- Complete Chromium browser run: 166/166 passed in 9.5 minutes, one worker,
  no retries, against the freshly built audit server. Coverage includes SK/EN,
  mobile/desktop layouts, navbar/guide navigation, complex forms and copying,
  three/five-line inputs, previews, variables, sessions and unit conversion.
- Fresh uninstrumented portable application build uses the static MSVC runtime
  (`/MT`). `scripts/package_apps.py` verifies the archive contents, dependencies,
  extracted CLI and extracted HTTP server with embedded web assets. Both binaries
  depend only on Windows system DLLs.
- Fresh uninstrumented library-only SDK build uses `/MD`.
  `scripts/package_sdk.py` verifies a relocated archive after removing its install
  staging directories. The bundled example and external C and C++ consumers all
  build and pass against the extracted SDK.
- Documentation: 69 Markdown files and 322 local links/anchors passed
  `node scripts/check_docs.cjs`; `git diff --check` passed.

These concurrent audit runs are regression checks, not new performance
measurements. The dated [factorial report](../benchmarks/SPLIT_WORKSPACE_2026_10_12.md)
retains the earlier isolated measurements and their limits.

## Local packaging evidence

`2.2.0-audit.1` is a local verification label, not a release version to publish.
The ZIP files and their checksum files remain in ignored audit directories.

| Archive | SHA256 |
| --- | --- |
| `NumForge-2.2.0-audit.1-win-x64.zip` | `12e83a3bb58d5be2e509fa7665fdd4777a552ec183a78c36ecace8a3f20cacaf` |
| `NumForge-2.2.0-audit.1-sdk-win-x64.zip` | `34fca2649358b7ccddba1c2eedc938ae362402c6760ba5db0511381784d11fb6` |

## Remaining release steps

1. Choose the next version, update version references and close both changelogs.
2. Push when requested and verify the final commit in GitHub Actions, including
   Linux, ARM/macOS, 32-bit, sanitizer, fuzz, coverage and Windows jobs. Local
   MSVC 2026 checks do not replace the CI MSVC 2022 checks.
3. Create the exact release tag only after the final checks pass. Build and
   verify release-version application/SDK archives on Windows and Linux using
   the [packaging workflow](RELEASE_PACKAGING.md), then publish the release.

The existing 2.2.0 tag/assets should be preserved. These local audit archives
must not be attached as replacements for that release.

## CI follow-up

The [CI run for 42886e5](https://github.com/Interacti0n/NumForge/actions/runs/38181949184)
completed with three failed jobs. Linux GCC and Clang sanitizers both reported
7,568 leaked bytes in nine allocations owned by the test-only numeric oracle
driver's reusable decimal workspace. Normal process exit now clears that
workspace. Allocation-instrumented oracle builds also assert a complete ledger
and zero live allocations after cleanup. This was test-driver ownership, not a
missing cleanup in the public numeric operation.

Chromium passed 165/166 checks. The Units geometry test compared a textarea's
pre-resize height (46 px) with its post-resize height (49 px) after switching
from the compact desktop breakpoint to 1100 x 700. It now waits for two render
frames before capturing the baseline; exact card-geometry equality is retained.

Focused local verification: numeric/division reference checks passed with MSVC
and GCC, including the GCC allocation ledger check; the MSVC allocation-failure
suite also passed. The exact Units geometry test passed all ten consecutive
Chromium repetitions, each covering both languages and all three desktop sizes.
Linux LeakSanitizer verification still requires the next CI
run. The unchanged Windows, Linux 32-bit, Linux/macOS ARM64, coverage and all
three numeric benchmark jobs succeeded in the failed CI run.

The [next CI run for 16da8ce](https://github.com/Interacti0n/NumForge/actions/runs/38183650012)
passed all ten other jobs, including both Linux sanitizer configurations and
Clang fuzz campaigns. Chromium also reported 166/166 passed in 9.3 minutes,
including the corrected Units geometry check. Its job was then cancelled at the
overall ten-minute deadline, which also includes compilation and Chromium
installation. The browser job now allows twenty minutes for setup, the serial
suite and report handling; individual test timeouts and assertions are unchanged.
