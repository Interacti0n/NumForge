# Supported monitor PyJWT qualification on 6 November 2026

## Candidate and import boundary

The candidate uses unmodified upstream **PyJWT 2.15.1**, the current release in
the [official PyPI metadata](https://pypi.org/pypi/PyJWT/2.15.1/json). Its universal
wheel is pinned to 33,860 bytes and SHA-256
`42d59d631f7768a1028a64c7ff581a9bf7519804daf91fc5b6c56e30eec5e193`.
The [manifest](../../deploy/monitor-jwt.json) and
[stager](../../deploy/prepare_monitor_jwt.py) verify integrity before extraction,
bound download/expanded size and member count, reject traversal/symlinks and keep
the upstream module, metadata and license unchanged.

The supported module is installed in Debian Python 3.11's existing administrator
package directory `/usr/local/lib/python3.11/dist-packages`, ahead of distribution
packages in the normal import path. No global PYTHONPATH or vendor code patch is
introduced. Default imports and `importlib.metadata.version('PyJWT')` must both
resolve to 2.15.1. The receipt `/opt/numforge/system-jwt-upgrade.json` records the
exact upstream archive and every installed file's hash.

Debian's older package and its previous no-redirect backport remain unchanged
under `/usr/lib/python3/dist-packages`. They are retained as package-managed
dependencies, not mislabeled as upgraded Debian packages. Directly importing that
legacy path or bypassing normal site initialization is outside this replacement;
its version-based OS scanner records remain visible. This is a qualified default
import replacement, not removal of all vulnerable code from the filesystem.

## Actual consumers and compatibility changes

Inspection of the installed Debian package tree found OAuthlib JWT signing,
verification and RSA helpers; its verification path explicitly allows RS256.
There was no PyJWKClient caller outside PyJWT in that bounded tree. This does not
prove the absence of every possible optional/custom consumer.

The upgrade intentionally adopts upstream JWKS behavior:

- callers supply a URL plus `headers`, rather than an urllib Request object;
- failed/malformed/redirected fetches retain only the previously trusted cached
  key set until its existing expiration; they cannot replace it with failed input;
- unknown key IDs are subject to a 30-second refresh cooldown, reducing fetch
  storms; explicit refresh still works;
- non-HTTP(S) JWKS schemes are rejected before I/O.

These changes are tested explicitly. The actual installed OAuthlib and Node
Apprise notification provider pass without compatibility shims. The library
itself is not altered to imitate the older cache or constructor API.

## Regression and build evidence

- Four host tests cover archive integrity, source/metadata preservation, expanded
  size limits, traversal and links. The Windows ZIP parser normalizes backslashes;
  the stager checks the original member name so normalization cannot hide them.
- Six disposable-container security tests cover loader-accepted mutated RSA/EC
  PEM keys, DER public-key/HMAC confusion, non-HTTP JWKS schemes, ordinary RSA/EC/
  HMAC round trips and a real OAuthlib service-application assertion.
- The same suite on the old deployed image fails seven subcases, demonstrating
  that the guard tests detect real missing defenses rather than only version strings.
- Seven helper checks cover all five redirect codes, credentials/destination/cache
  behavior, JWKS fetch/cache/explicit refresh, errors, unrelated urllib redirects,
  OAuthlib and a real Node-to-Apprise HTTP notification. They also verify every
  receipt file and the existing system SQLite regression.
- The monitor fixture passed original → candidate → original → candidate, with
  238 Node checks on all four selections. User/config/history hashes, authenticated
  provisioning, up/down/push signals and persistence remained intact.
- A fresh ordinary Dockerfile build passed 238 Node checks, four system SQLite
  checks, seven helper checks and the six new JWT security checks.

Both maintenance and ordinary images contain the identical complete `/app` tree,
OS package versions, legacy JWT source/backport, SQLite native engine and image
configuration. The ordinary SQLite receipt differs only in ASLR load addresses;
dependency paths and all other receipt fields compare equal. No monitor schema
or account feature migration is involved.

Evidence is under `build/followup-20261106/jwt/`: `inventory.json`, `upgrade.log`,
`fresh.log`, `helpers-final.log`, `security-candidate.log`,
`security-negative-control.log`, build logs and Trivy reports.

## Security interpretation and promotion gate

The newer upstream implementation rejects the public-key confusion cases described
in the [PEM advisory](https://github.com/jpadilla/pyjwt/security/advisories/GHSA-ffc3-869f-jxw9)
and preserves the no-redirect boundary of the earlier private repair. This does
not establish an exploitable path in the pilot's configured notifications.

Before promotion, scan the exact final image with the current database, require
no new unexplained package/advisory findings or listed fixes, freeze source and
image IDs, and retain verified encrypted pre/post restore points. Replace only
the monitor, preserving its volume, UID/GID, runtime protections, existing native
engine and six monitors. On a failed live acceptance check, select the recorded
old image/profile and keep the data volume. Refresh the portable kit afterwards.

**Status:** qualification complete; live promotion is recorded separately below
only after the exact-image checks and encrypted backup gates have passed.
