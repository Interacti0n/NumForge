# Release 2.1 preparation

Version 2.1.0 is prepared in CMake but is **not published**. The changelogs
remain marked unreleased until publication. See [highlights](../../CHANGELOG_SHORT.md)
for the short feature list and [full changelog](../../CHANGELOG.md) for details.

## Scope

This release adds exact unit conversions, typed Quantity arithmetic, session
variables with deletion, numeral bases 2–36, short-output optimization and
tangent accuracy improvements. It also separates calculation/application/HTTP
ownership and improves the browser layout and documentation.

Existing public numeric C signatures remain source compatible with 2.0. The
installed target stays `NumForge::numforge`; application targets and their
headers remain private. No database, accounts, graphs or equation solver ship
in this release. The latter navigation entries are informational placeholders.

## Local verification

- MSVC Release: 32/32 CTest checks passed for the application-layer split.
- GCC 2.1 instrumented build: 37/37 CTest checks passed, including benchmark smoke checks.
- Production MSVC 2.1 browser suite: 94/94 passed, including whole-sidebar
  keyboard scrolling, 420-pixel-high windows and the version footer.
- Standalone numeric library build/install and external C/C++ consumers passed.
- Documentation audit: 39 Markdown files and 185 local links/anchors passed.
- Production MSVC x64 build passed with warnings as errors and static runtime.
- Current desktop/mobile screenshots were captured and inspected at version 2.1.

The Windows archive is checked after committing preparation, so its provenance
can name an actual source commit. It remains a local build artifact until the
tagged release workflow is run. Linux archives still require that workflow.

Local checks do not substitute for GitHub CI on the final pushed commit or Linux
archive verification. Benchmark reports retain their measured source snapshots;
no new timing claims are made for this release.

## Before publication

1. Push the reviewed commit and wait for all CI jobs, including Linux/macOS,
   installed consumers, sanitizers and browser tests. Fix failures first.
2. Review notes and screenshots. Replace the unreleased headings with the actual
   publication date when releasing.
3. Create `v2.1.0` on the checked commit. A tagged prerelease such as
   `v2.1.0-rc.1` can be used first; its numeric version still matches CMake.
4. Run **Release application binaries** on that tag with **publish unchecked**
   to verify Windows and Linux archives before publishing the release.
5. Publish the GitHub release with the highlights. Automatic packaging attaches
   archives and SHA-256 files once both platform jobs succeed.
6. Download the attached archives, check checksums, extract and launch both
   apps. Confirm filenames and `BUILDINFO.json` match the tag/commit.

See [packaging](RELEASE_PACKAGING.md) for commands, runtimes and triggers.
Tagging, pushing and publication are separate actions from local preparation.

## Known boundaries

- HTTP session list/history/reset coverage is incomplete; see
  [Web/API coverage](../reference/WEB_API_PARITY.md). Evaluation and deletion
  work, but the project does not promise complete web/API parity yet.
- Sessions are process-local; converter history belongs to a browser tab.
  There is no persistence or account synchronization.
- General compound unit strings and further Quantity extensions remain roadmap
  work; use the currently documented supported syntax and catalogue.
- Windows downloads are unsigned. Linux archives target x64 glibc, not musl.
  ARM/macOS downloads and installers are outside the portable release assets.
- The HTTP listener is loopback-only and unsuitable for direct public hosting.

These boundaries do not require a database or accounts to release 2.1.
