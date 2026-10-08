# Release 2.1 record

NumForge 2.1.0 — 8 October 2026. See the
[GitHub release](https://github.com/Interacti0n/NumForge/releases/tag/v2.1.0)
for publication status and downloads, [highlights](../../CHANGELOG_SHORT.md)
for the short feature list and [full changelog](../../CHANGELOG.md) for details.

This file preserves release scope, validation and limitations. Future work
belongs in the roadmap; packaging instructions live in their separate guide.

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

The local Windows archive from preparation commit `25f6954` passed extracted
CLI/HTTP/embedded-asset checks, static runtime dependency checks and SHA-256
verification. Official assets are rebuilt from the final release tag; they
record that source commit in `START_HERE.txt`. Each archive contains only the
two applications, MIT license and short launch instructions.

Local checks do not substitute for GitHub CI on the final pushed commit or Linux
archive verification. Benchmark reports retain their measured source snapshots;
no new timing claims are made for this release.

## Distribution and hosted verification

The release gate is successful CI on the final source commit plus a manual
**Release application binaries** dry run of `v2.1.0` on Windows and Linux.
The [Actions history](https://github.com/Interacti0n/NumForge/actions) records
platform tests, installed consumers, sanitizers, browsers and archive checks.

Publishing triggers packaging and attachment of:

- `NumForge-2.1.0-win-x64.zip`.
- `NumForge-2.1.0-linux-x64.tar.gz`.
- `NumForge-2.1.0-sdk-win-x64.zip` (MSVC Release `/MD`).
- `NumForge-2.1.0-sdk-linux-x64.tar.gz` (GCC/glibc).
- A SHA-256 file for each archive.

Both platforms must pass before upload. Extracted-package smoke checks cover
the CLI, HTTP evaluation and embedded assets. Download checksums and build
instructions identify the actual published artifacts.

SDK assets were added after initial publication without moving `v2.1.0`.
Their numeric library and headers come from the same tag; updated packaging
automation adds a minimal example and verifies relocated C/C++ consumers.
The [SDK guide](../guides/SDK.md) explains compatibility and integration.

See [packaging](RELEASE_PACKAGING.md) for commands, runtimes and triggers.

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
