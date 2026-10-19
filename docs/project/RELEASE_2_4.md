# Release 2.4.0 record

Version 2.4.0 introduces the Equations tool alongside Calculator, Units and
Graphs. This record tracks preparation and publication separately; final tagged
CI, packaging and published-download verification are recorded after completion.

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

## Final publication verification

Pending for the versioned source: final CI, immutable `v2.4.0` tag, GitHub release,
Windows/Linux extracted application and relocated SDK checks, and independent
published archive/checksum verification. Preparation does not by itself claim
these steps have completed. Existing release assets and tags are never replaced.
