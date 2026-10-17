# Release 2.3.0 preparation record

Prepared on 17 October 2026. Version 2.3.0 introduces the graph tool and its
supporting application/HTTP operations. This record is a release candidate audit,
not evidence of a published tag, successful remote CI or downloadable 2.3.0 assets.
The latest published download links remain on 2.2.1 until publication.

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
local configuration evidence; the new hosted jobs still require a push.

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
No tag, push or published release was made during this preparation.

## Publication checklist

1. Commit the tested candidate. Push only when requested by the maintainer.
2. Verify all remote CI jobs for that exact candidate, including both browser
   shards, Linux sanitizers/fuzzing, 32-bit, ARM64 and installed C/C++ consumers.
3. After successful CI, create the exact version tag and publish the release.
   Build applications and SDKs from that tag using the
   [packaging workflow](RELEASE_PACKAGING.md); verify both Windows/Linux assets.
4. Update published-download links to 2.3.0 after the matching assets exist.
   Keep older release tags/assets intact. Audit archives must not be uploaded as
   release-tag binaries.
