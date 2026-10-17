# Release 2.3.0 record

[NumForge 2.3.0](https://github.com/Interacti0n/NumForge/releases/tag/v2.3.0) was published on 17 October 2026 from
exact tag source `e91796007f5abf8b59c858d509dfd618c5e9709e`. It introduces the graph tool
and supporting application/HTTP operations. All twelve CI jobs and both platform
packaging jobs passed before the published archives were verified.

## Scope

- Up to four independent colored expressions with inferred variables, automatic
  radians/sampling, pan/zoom, visibility and per-function failures.
- Calculator-style function editing, shared function/constant search and insertion,
  a color palette, coordinate inspection and mobile expanded views.
- Bounded C uniform/adaptive/screen sampling, guarded derivatives/interpolation,
  integer-domain points and original-expression intersection verification.
- Visible-first work, bounded resolution caches/background preparation and
  same-tab settings/completed-graph restoration.
- Focused guide sections, slow-calculation timing and expanded independent
  references, allocation/cancellation checks and recorded performance studies.

See the [changelog](../../CHANGELOG.md), [graph guide](../guides/GRAPHS.md),
[screen HTTP contract](../reference/SCREEN_GRAPH_HTTP_API.md) and
[graph HTTP contract](../reference/GRAPH_HTTP_API.md).

## Supported-scope limits

Graphs are approximate display samples, not certified interval enclosures or a
complete root enumeration. Narrow features or discontinuities can be missed;
unsupported/dense compositions retain finite sampling and may leave gaps.
Supported dense ranges can overestimate attained values. Integer-domain detection
recognizes a conservative set of direct parameter uses, not arbitrary lattices.

Graph calculations retain cooperative time/memory/output limits. Multiple
serialized requests/functions can take longer overall than one request's
five-second budget. Marker verification has its own bounded work/one-second HTTP
budget and is numerical, with residual tolerances. A missing marker does not
prove no intersection exists. Informational diagnostics are hidden in the UI;
actual input/calculation errors remain visible.

The local server/tunnel is useful for testing. This release does not introduce
public multi-user hosting, account storage, a database or native mobile apps.
Broader physical-device checks remain distinct from browser emulation.

## Local validation

Strict Release builds completed with MSVC 19.51 and GCC 16.2 on Windows x64,
with warnings treated as errors and benchmark instrumentation disabled. GCC
used a fresh build tree; MSVC regenerated and rebuilt the existing audit tree.
Both compilers passed all 42 ordinary CTest checks and all 16 exhaustive
allocation shards (58/58 each). Checks include numeric/complex references,
public C API, CLI, HTTP, session/unit operations, graph regressions, allocation
faults and fuzz smoke checks. Server tests ran serially per compiler.

The full Chromium run executed all 408 cases: 396 passed and 12 stale test
expectations failed. The failures concerned guide language/section links,
required semicolon placeholders and trying to edit/submit the background form
while the modal function editor was open. The tests now exercise the visible
editor and current navigation, retaining value, caret, error and cancellation
assertions. No production calculation change was needed.

The six affected calculator cases passed, followed by all 12 editor/preview
cases. Combining the complete run with the affected-case reruns accounts for
all 408 distinct cases with no remaining failure; this is not a claim that a
second complete 408-case run was made. The local full run took 20.2 minutes,
which also justifies splitting the expanded CI suite.

Representative Firefox/WebKit checks passed 108 cases, with four Chromium-only
protocol scenarios skipped. They cover independent marker/curve references,
camera cycles, coordinate inspection, editor layout, palette and mobile controls.
Native and browser checks ran partly concurrently; these functional results do
not replace the dated isolated performance comparisons. Production app/SDK
builds and extracted-archive checks completed as recorded below.

Documentation review corrected stale uniform/adaptive graph descriptions,
removed sample-table and future pan/zoom claims, updated API coverage and quiet
UI diagnostics, and consolidated both changelogs around final shipped behavior.
Historical benchmark reports retain their original snapshots and limitations.

The browser CI matrix now has two one-worker shards with unchanged assertions
and individual timeouts. A listing audit confirms 213 + 195 = 408 cases, no
overlap and no omission. YAML parsing/matrix/artifact checks passed. This is
local configuration evidence; the subsequent exact-source hosted jobs also passed.

## Local archive evidence

The audited source commit is `9268ae54212dc8d907f56ba55b83bbbaa8d49d40`.
The archive label `2.3.0-audit.1` is a local validation label, not a published tag
or the final 2.3.0 release asset name. Outputs stay in ignored
`build/release-prep-2026-10-17/`; rebuild the exact release tag for publication.

| Windows x64 archive | SHA256 |
| --- | --- |
| `NumForge-2.3.0-audit.1-win-x64.zip` | `7df61244528de75becdc208fce053949aa8a128d2c5036f9515026db0b9b9ec0` |
| `NumForge-2.3.0-audit.1-sdk-win-x64.zip` | `bfe61ba325c7a39e0ea1a5b50e3528abe5f2ef542e75308b9f09e28aa135a402` |

- Applications use MSVC Release with static runtime; imports are limited to
  Windows system DLLs. Extracted CLI/HTTP/embedded-asset checks passed.
- Additional checks against the extracted application confirmed the 2.3.0 graph
  UI, 135 sine samples, 21 disconnected factorial samples and the actual root
  `(5,0)` of `x-5` through C candidate verification.
- SDK uses MSVC Release `/MD`. Its install/staging copies were removed before
  building/running the extracted example and external C and C++ API consumers.
- Documentation check: 106 Markdown files and 486 local links/anchors passed;
  whitespace/diff checks passed. Both archived SHA256 values were checked again.

Local Windows GCC checks do not replace Linux sanitizers, hosted Windows MSVC
2022, 32-bit, Linux/macOS ARM64, fuzz campaigns or Linux archive verification.
These local audit checks preceded tagging and publication; final release evidence is below.

## CI follow-up: MSVC 2022 and 32-bit floating-point tests

The [first candidate CI run](https://github.com/Interacti0n/NumForge/actions/runs/38990966161)
for `fbdf6cd` passed ten jobs, including both browser shards, both Linux
sanitizer configurations, coverage, ARM64 platforms and numeric benchmark jobs.
Windows failed to compile one test; Linux 32-bit failed one graph assertion.
All 16 allocation shards passed on Linux 32-bit.

- MSVC 19.44 reported C4127 for a compile-time allocation-size condition with
  warnings-as-errors. The test now uses its requested sample-capacity variable
  for both the condition and API call; the allocation/error assertion is retained.
- The coordinate-cache test compared a stored `double` to the literal `0.1`.
  x87 can evaluate the literal with excess precision, making that equality fail
  even when input and output doubles match exactly. The test now asserts that
  the adjacent inputs are distinct and compares each output to its stored input
  bit for bit, retaining the expected cache hit.

Both failures were reproduced locally before the changes: MSVC 19.44.35228
with v143 (CI uses 19.44.35229) reproduced C4127, and GCC 16.2 with
`-mfpmath=387` reproduced the same graph-test failure as Linux 32-bit.
After the test-only fixes, all 35 graph cases passed in each profile. No numeric
algorithm, resource budget, tolerance, warning policy or browser code changed.
The complete older-MSVC RelWithDebInfo build also passed with warnings-as-errors,
followed by all 42 ordinary checks and 16 allocation shards (58/58). Actual Linux
32-bit hosted verification then passed in the final CI run; the local x87 run is a reproduction
of its arithmetic behavior, not a claim of a local Linux run.

## Published release verification

- [Exact-source CI](https://github.com/Interacti0n/NumForge/actions/runs/38993282758): all 12 jobs passed, including
  Windows/MSVC 2022, Linux 32-bit, both browser shards, Linux sanitizers/fuzzing,
  ARM64 platforms, coverage, benchmark checks and installed C/C++ consumers.
- [Release packaging](https://github.com/Interacti0n/NumForge/actions/runs/38994071707): Windows x64 and Linux x64
  jobs passed full native tests and verified extracted application/SDK archives;
  the upload job verified checksums before attaching all eight assets.
- Downloaded the four published archives and four checksum files independently.
  SHA256, asset sizes/digests, Windows ZIP integrity and embedded source/version
  provenance all matched the exact `v2.3.0` source above.
- README and SDK online links now target the published 2.3.0 release. Local audit
  archives remain separate and were not uploaded as release assets.

| Published archive | SHA256 |
| --- | --- |
| `NumForge-2.3.0-win-x64.zip` | `c624bfd92b2eeedbabe090e1f0632b4f6d328049b2d653b95de09771d7fe3668` |
| `NumForge-2.3.0-linux-x64.tar.gz` | `55e930839634d71ed503678b598e58ee229b838dd1d5fab4b5544e5d4859e0af` |
| `NumForge-2.3.0-sdk-win-x64.zip` | `002cc99c6d62f3b57198536bf441042c72742a59c35a5c4cab2ffa95c9a24e85` |
| `NumForge-2.3.0-sdk-linux-x64.tar.gz` | `61a35b5b7e8ee54c32fddac9c75a4c9caefedb16203917afa9d51a3f3895c570` |

The immutable release tag remains on the CI-verified source. Later documentation
updates record publication without moving that tag or replacing its archives.
