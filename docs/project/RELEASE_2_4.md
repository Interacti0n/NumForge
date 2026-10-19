# Release 2.4.0 record

[NumForge 2.4.0](https://github.com/Interacti0n/NumForge/releases/tag/v2.4.0) was
published on 19 October 2026 from immutable tag source
`d2b897a083225b416127bbf2e1deb129f207f00b`. It introduces Equations alongside
Calculator, Units and Graphs. Exact-source CI, both platform packaging jobs and
independent published-download checks passed as recorded below.

## Scope

- Exact linear/quadratic equations in lowercase x, or real rational coefficient
  expressions a, b and c; exact rational, radical and complex roots, multiplicity,
  degree reduction and separate guarded decimal previews.
- One general real root in an explicit sign-changing bracket, with rational
  coordinates, high-precision residuals, original-domain checks and final
  higher-precision reevaluation.
- Bounded multiple-root discovery, including simple rational contacts and some
  close pairs. Checked rational coordinates, residuals, per-root copy and graph
  actions remain consistent.
- Shared searchable function/constant editing, balanced insertion, examples,
  coefficient targeting, compact mobile layouts and same-tab continuation.
- Stateless C application/HTTP operations and documented shared calculation,
  allocation, output and work limits. The installed public C SDK remains stable;
  equation application interfaces remain private.

See the [changelog](../../CHANGELOG.md), [equation guide](../guides/EQUATIONS.md),
[exact HTTP contract](../reference/EQUATION_HTTP_API.md) and
[numerical HTTP contract](../reference/NUMERIC_ROOT_HTTP_API.md).

## Supported-scope limits

Exact input is limited to real rational coefficients and intermediate polynomial
degree at most two. Irrational/complex coefficient expressions, higher-degree
symbolic solving, systems and arbitrary unknown names are outside this release.
Exact irrational and complex **roots** are supported.

Numerical results are finite-precision approximations with verification checks,
not certified root-error or existence enclosures. Multiple-root discovery is
always potentially incomplete, with at most 32 roots/4096 coordinate pairs.
General tangencies such as `(cos(x)-x)^2=0` may be missed. Unsupported domain
checks, insufficient precision and work limits have explicit outcomes and must
not be interpreted as proofs that no solution exists. Original domain holes are
preserved. Production refinement uses bisection; experimental hybrids are private.

Browser emulation covers narrow phones, landscape and desktop. Broader physical
Android/iOS keyboard, focus and clipboard checks remain separate from automation;
no new physical-device certification is claimed. Accounts, persistent storage,
native mobile applications and public multi-user hosting are not introduced.

## Validation before the version change

- MSVC v143 warnings-as-errors build: 61/61 native CTest groups passed.
- Full local Chromium run: 446/446 cases, plus 19/19 final editor/scan checks
  containing one subsequently added clipboard-denial case.
- Independent scan benchmark checks cover 135 rows and 450 checked coordinates;
  allocation/deadline, original-domain and atomic-failure regressions are included.
- [CI on 6ddd475](https://github.com/Interacti0n/NumForge/actions/runs/39180598916):
  all 12 jobs passed, including both browser shards (242 + 205 = 447 cases),
  Linux sanitizers/fuzzing, Windows, Linux32, ARM64 platforms, coverage, installed
  C/C++ consumers and the three-OS benchmark checks.
- A graph expansion test race was reproduced exactly and fixed by waiting for
  the requested zoom to reach the SVG before saving its camera. All three
  layouts passed ten repetitions each, without retries or timeout increases.

Detailed performance and scope evidence is in the
[equation readiness audit](EQUATION_RELEASE_AUDIT_2026_10_19.md). Historical
benchmark reports retain the source snapshots and limits they measured.

After changing the version to 2.4.0, the MSVC v143 warnings-as-errors build and
all 61 native CTest groups passed again (23.43 s). The extended application smoke
also passed against this build, including exact and numerical equation routes.
Documentation links and whitespace checks passed before the release commit.

## Versioned candidate follow-up

[CI on 34bf0ec](https://github.com/Interacti0n/NumForge/actions/runs/39181793899)
passed 11/12 jobs and 446/447 browser cases. One font-family test read the history
element immediately after a confirmed result, before the independent history
refresh had created that element. A controlled delayed-history response reproduced
the missing target while the result already displayed `1/3`; releasing the response
created the expected mathematical-font element. The test now waits for that target
before comparing all original font families. No application change, retry or timeout
increase is involved. The subsequent final commit passed CI before the tag
was created, as recorded below.
Both language variants passed ten repetitions each: 20/20 cases in 26.7 s.

## Published release verification

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
