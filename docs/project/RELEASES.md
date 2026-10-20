# Release verification history

Published artifacts are versioned snapshots. Current engine/module/Python source
can be newer than these archives. Feature chronology belongs in the
[changelog](../../CHANGELOG.md); publishing steps belong in
[release packaging](RELEASE_PACKAGING.md).

| Version | Recorded date | Scope / immutable source where recorded |
| --- | --- | --- |
| [2.1.0](https://github.com/Interacti0n/NumForge/releases/tag/v2.1.0) | 8 October 2026 | Units, quantities, variables, bases; numeric SDK. Graph/equation routes were placeholders. |
| [2.2.0](https://github.com/Interacti0n/NumForge/releases/tag/v2.2.0) | 10 October 2026 | Complex stability and independent reference expansion. |
| [2.2.1](https://github.com/Interacti0n/NumForge/releases/tag/v2.2.1) | 12 October 2026 | Arithmetic, allocation and bounded factorial/logarithm improvements. |
| [2.3.0](https://github.com/Interacti0n/NumForge/releases/tag/v2.3.0) | 17 October 2026 | Graph application; `e91796007f5abf8b59c858d509dfd618c5e9709e`. |
| [2.4.0](https://github.com/Interacti0n/NumForge/releases/tag/v2.4.0) | 19 October 2026 | Equations; `d2b897a083225b416127bbf2e1deb129f207f00b`. |

Earlier step-by-step release audits and candidate test-race investigations remain
in Git history. They are superseded by the tagged release outcome, not current
release instructions. Do not move old tags or overwrite existing release assets.

## Published 2.4.0 evidence



- [Exact-source CI](https://github.com/Interacti0n/NumForge/actions/runs/39234580890)
  passed all 12 jobs for the tagged source, including 447 browser cases,
  platform/sanitizer/fuzz checks, coverage, installed C/C++ consumers and
  benchmark checks on Windows, Linux and macOS.
- [Release packaging](https://github.com/Interacti0n/NumForge/actions/runs/39235889260)
  passed Windows x64 and Linux x64 native tests, extracted application checks
  and relocated SDK/example/external C/C++ consumer checks. The upload job
  verified all checksums before attaching the eight assets.
- Independently downloaded all four public archives and four SHA256 files.
  Archive sizes, GitHub asset digests, SHA256, ZIP integrity and embedded
  version/source provenance matched the immutable release source above.
- Extracted the public Windows application archive into a fresh directory and
  reran CLI, HTTP calculator and equation checks, including both editor languages,
  required assets, the function registry, exact radicals and the rational 1/3
  contact with residual zero. All passed. Linux execution was checked in its
  packaging job; no local Linux or physical-phone run is claimed.

| Published archive | SHA256 |
| --- | --- |
| `NumForge-2.4.0-win-x64.zip` | `19980612ea2aab030d61302c084f94dd4c875b2ecaf23c4e08c86cb8bb0e8e3e` |
| `NumForge-2.4.0-linux-x64.tar.gz` | `f1321f558da31073919428b4dd0238036d784869714defcc123b3d0f20120a36` |
| `NumForge-2.4.0-sdk-win-x64.zip` | `043873d16db888e7dc25bfd5d3742bfad582a41268d907b87a243bcf4b6420a7` |
| `NumForge-2.4.0-sdk-linux-x64.tar.gz` | `75e9deb0ddef7113d3e0321665bbc954bfb8e60482bcfaf574a7a9f3aafd43b7` |

The immutable tag stays on the CI-verified source. This later documentation
update records publication without moving that tag or replacing its assets.

## Limits retained at publication

2.4.0 exact equations support rational coefficients and degree at most two;
irrational/complex roots are supported. Bounded numerical searches can miss
contacts/clusters and do not certify existence or completeness. Production uses
bisection. Physical-phone keyboard/focus/clipboard review is separate from
browser emulation. Public multi-user hosting, accounts and persistence are not
introduced by these releases. Current contracts live in the [API index](../API.md).
