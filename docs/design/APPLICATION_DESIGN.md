# Application and calculation boundaries

The application uses memory only; there is no database or account backend.
The public numeric library can still be built and installed alone.

The [architecture](ARCHITECTURE.md) owns the source map and target dependency
rules. Application and HTTP support targets remain private. The public expression
contract is [engine.h](../reference/ENGINE_API.md); calculator implementation
headers are not installed.

## Ownership

The expression engine receives explicit context and borrowed variable/ans
values, and returns an owned typed result. It does not own client IDs, session
pools, confirmation history, UI fields, accounts or persistence. For random
expressions the caller supplies state; the application decides whether preview
draws become confirmed state.

`CalculatorSession`, in `src/application/sessions/session.h`, owns variable/history
values, confirmed conversions and one preview. Clearing calculator history moves
its latest typed value into separately owned ans storage. Reset destroys values;
release retains a bounded tombstone until FIFO eviction. Confirmation, deletion and revision handling remain
shared by CLI and HTTP. UI summaries do not replace full stored values.
Formatting projects values without overwriting their exact/approximate/Quantity
representation.

`ApplicationClientStore` owns two fixed pools: 64 sessions and 64 legacy caches,
with [idle and retention limits](../guides/HOSTING.md#capacity-and-state).
Read-only lookup never creates a session
or revives an expired one. Returned pointers are borrowed until eviction or
destruction. Invalid IDs do not allocate or evict; destruction releases all
owned values. Stores are independent instances, not mathematical globals.
The server owns one instance and uses synchronized request leases. These private
application contracts do not create a public session ABI or persistent accounts.

## Presentation and future storage

The engine has no database dependency and receives no DOM/layout state.
Confirmed conversion history belongs to the application session, independently
of calculator ans/history and RNG. The browser retains presentation fields and
the session ID, then reads authoritative saved values from the server.

The [HTTP coverage](../reference/WEB_API_PARITY.md) and
[session API](../reference/SESSION_HTTP_API.md) document list/read, lifecycle,
function registry and converter-history routes. Snapshot schema version 1
represents exact values, approximations, Quantity dimensions/units, temperature
semantics and original context. Responses are bounded and paginated; full values
are requested explicitly without silently rounding exact values.

The next selected product direction is PostgreSQL-backed accounts and saved
work. The proposal below is not implemented: login/register remain informational,
and current server sessions still disappear on restart.

## Proposed accounts and saved work

Keep PostgreSQL and account management outside `src/core/`, `src/modules/` and
the installed C API. Recommend an optional, small Django account service using
its maintained authentication, database sessions, CSRF protection, ORM and
migrations, with PostgreSQL as storage. Choose a supported LTS/patch combination
at implementation time. The C server continues to own mathematical operations;
no calculation is reimplemented in Python. The standalone library, CLI and guest
application remain usable without this service or database.

This adds Python/PostgreSQL to account-enabled deployments; the existing portable
guest executable keeps its current requirements. Start with a local development
database and an isolated preview. Use one public HTTPS origin with explicit
gateway routing; PostgreSQL and internal service ports stay private. Redis,
queues, paid hosting and a managed identity provider are not initial requirements.

### Useful first account screen

| Area | User controls and persistence |
| --- | --- |
| Profile | Display name, email, password change, account export/deletion. |
| Calculation defaults | Language, working precision, decimal places, notation, rounding and RAD/DEG; validate against existing C limits. |
| Tools | Preferred unit/currency pair, graph view and colors, matrix/editor inputs; use the existing versioned graph/matrix formats. |
| Workspaces | Named calculator/graph/matrix/equation tasks, duplicate/rename/delete, explicit save and optional autosave. |
| Privacy | History off/on, retention choice within server limits, save inputs only or include supported results; guest work imports only on explicit request. |
| Sign-in | Optional remembered login, active devices, revoke one/all sessions; reauthentication for sensitive changes. |

On login, load account preferences, then the selected workspace. Restore inputs
without executing expensive calculations. Autosave after edits settle, with
visible Saving/Saved/Offline states; on failure retain dirty local work. Each
write includes the expected revision. Conflicts between tabs/devices preserve
both versions or ask for a choice, never silently overwrite newer work.
Signing out clears account-scoped browser state. Signing into another account
must not import the previous account's fields, cache or pending saves.

### Storage and ownership

Use framework-owned users/auth sessions, plus preferences, workspaces and
optional history entries linked to their owner. Index owner/update time and
enforce ownership in every read/write/export/delete using the authenticated
server identity, never a submitted user ID. Use validated, versioned JSONB for
tool payloads; exact numbers/fractions remain strings, never binary floats.
Store original context and approximation provenance with saved results.

Existing typed snapshots can be read but do not yet provide a complete restore
contract. Full variable/ans/history restoration requires a bounded validated C
decoder and transactional restore; never replay stored expressions, assignments
or random operations to reconstruct old values. First delivery can save inputs
and settings while that separate capability is developed.

Authentication cookies and C calculation client IDs have different roles.
The existing anonymous session routes are not an account authorization boundary.
Before restoring private runtime state, implement/test an authenticated bridge
that binds execution/session access to the owner and prevents direct anonymous
access. An unguessable client ID or a valid Origin alone is not sufficient.

Provisional pilot limits: 100 named workspaces and 25 MiB saved payload per user,
1 MiB per workspace and 200 KiB per history entry, with existing stricter tool
limits retained. Reject oversized saves explicitly; never truncate an exact
result and label it complete. History retention/autosave are configurable within
server caps. These are proposed defaults to validate, not existing guarantees.

### Implementation order and completion gates

1. Isolated PostgreSQL/account-service setup, versioned migrations, connection
   configuration outside Git, least-privilege DB user and tested backup/restore.
2. Private pilot accounts, login/logout, remembered sign-in, account settings,
   expiration/revocation, CSRF and bounded login attempts. Use framework password
   handling with Argon2id configured and measured; no custom password/token crypto.
3. Preferences and named input workspaces, explicit guest import, autosave,
   quotas and revision-conflict handling. Verify restore after backend/DB restart
   and on a second browser, plus isolation between two different users.
4. Account UI for device management, export/deletion and recovery. Configure mail
   and test verification/reset flows before enabling public self-registration;
   a private pilot can start with manually provisioned users instead.
5. Typed calculator snapshot restore and authenticated execution binding, then
   optional confirmed history/variables/ans with original context and provenance.
6. Cross-account access, sign-out/cache isolation, revoked cookies, lost writes,
   outages, migration rollback, backup recovery and representative mixed load.
   Move the public preview only after these gates pass.

PostgreSQL restart must preserve saved work. If a Quick Tunnel hostname changes,
users sign in again at the new origin; their data remains in the database.
TLS cookies use Secure/HttpOnly/SameSite and server-enforced expiry. Credentials
do not belong in browser storage. Account deletion covers active data and
documents the backup expiry period; exact recovery/retention policy is part of
the implementation, not an implied unlimited archive.

Implementation references: [Django authentication](https://docs.djangoproject.com/en/5.2/topics/auth/default/),
[database sessions](https://docs.djangoproject.com/en/5.2/topics/http/sessions/),
[password storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
and [session management](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html).

## Verification

`application_tests` links the application without HTTP and checks independent
stores, preservation, separate FIFO eviction, invalid IDs and destruction with
typed values. Existing calculator/session, CLI/HTTP, allocation-failure, fuzz
and benchmark checks keep their contracts. Package consumers reject accidental
export of private targets.
