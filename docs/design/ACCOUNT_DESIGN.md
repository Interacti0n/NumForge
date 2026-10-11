# Account and authentication proposal

Status: proposed on 11 October 2026. None of the functions, routes, tables or
account behavior below is implemented. The existing `/login` and `/register`
pages remain informational. This document is a starting contract for review,
not an installed C API or a deployment guide.

## Scope and application boundary

Keep the calculator and unit converter usable as a guest. The first account
version provides registration, verified email, login, logout, session management
and password recovery. Persisted history, variables and settings follow as a
separate feature; creating an account does not imply synchronization exists.

## Proposed benefits after login — product review

Status: proposal only. Authentication and saved-work features are separate
implementation stages; the first useful account release should include the
initial benefits below once their application and HTTP contracts are verified.
Guest users retain the calculator, converter and mathematical functions.
Accounts add persistence and continuity rather than restricting arithmetic.

| Priority | Feature | Proposed behavior |
| --- | --- | --- |
| Initial | Personal settings | Restore language, precision, rounding, RAD/DEG, notation and complex display form on another device. |
| Initial | Named work sessions | Save a workspace such as School or Project with confirmed calculator/conversion history, variables and `ans`; list, rename, reopen and delete it. |
| Initial | Favorite calculations and conversions | Save a named expression or conversion with an optional note; opening it restores the inputs/settings without silently evaluating or committing it. |
| Initial | Cross-device continuation | Open the latest saved workspace on PC or phone, with explicit revision conflict handling. |
| Before public rollout | Account and data management | Change password, revoke device sessions, export saved data and delete the account with its owned data; define retention and backup handling. |
| Later | Own functions, graphs and equations | Save definitions, graph configurations and solving inputs when those tools have their own implemented contracts. |
| Later | Explicit sharing | Private by default; sharing requires a deliberate action and a separate access/revocation contract. |

Start synchronization with confirmed results and explicit workspace changes.
Do not send every keystroke or transient preview into account history. Keep the
unfinished input local in version one, and make save status visible. Failed saves
must not claim success or silently discard work. Reading a saved result uses its
stored typed snapshot, not reevaluation under a different precision or new RNG.

Each saved workspace has an owner, schema version and revision. A write supplies
its expected revision and commits atomically. Concurrent PC/phone changes must
produce an explicit conflict rather than silently replacing newer work; offer
reload or save as a separate workspace. Automatic merging is deferred until its
semantics are defined. Set bounded, paginated history and storage quotas before
implementation; accounts do not imply unlimited retention.

Existing anonymous calculator IDs cannot prove account ownership. Importing a
guest workspace is an explicit action using a validated snapshot from the
current guest context, not a claim on arbitrary sessions by ID. Login/logout
switches account-specific UI state without exposing another account's data.
All saved-work operations must have matching C application and documented HTTP
operations; the SK/EN web interface consumes those same operations.

Next design step: define settings, workspace and favorite schemas, their CRUD
operations, revision conflicts and quotas. This section does not add database
tables, routes or working UI. Export/deletion details, conflict UX and the precise
initial release scope remain review decisions.

Place the service behind `src/application/`, with injected storage, password
hashing, secure randomness, clock and email delivery adapters. HTTP translates
requests into these same application operations. The numerical library and its
installed headers acquire no account or database dependency.

Start with a SQLite storage adapter and migrations for a single-server setup.
Use a test-only memory adapter for contract tests, not as production account
storage. A later PostgreSQL adapter can implement the same transactional
operations. Database portability requires tests, not only interchangeable names.

## Proposed C application operations

Names and request/result types are provisional private application contracts.
Each operation returns a dedicated `AccountStatus`, distinct from math errors.
Output objects own their data and have a matching destroy function. Failures
leave supplied outputs unchanged; storage mutations commit atomically.

| Operation | Proposed function | Behavior |
| --- | --- | --- |
| Create service | `application_accounts_create(config, adapters, out)` | Validate dependencies and policy; open no mathematical session. |
| Destroy service | `application_accounts_destroy(service)` | Release owned resources; does not delete accounts. |
| Register | `application_account_register(service, request, out)` | Request email verification; never overwrite an existing account or its password. |
| Verify email | `application_account_verify_email(service, token, out)` | Consume a valid verification token and activate the account atomically. |
| Resend verification | `application_account_request_verification(service, email, out)` | Return a generic accepted response; delivery is throttled. |
| Log in | `application_account_login(service, credentials, out)` | Verify credentials and issue a fresh authenticated session. |
| Identify caller | `application_account_authenticate(service, session_token, out)` | Validate expiry/revocation and return an internal principal. |
| Current account | `application_account_get_profile(service, principal, out)` | Return safe profile fields for the authenticated account. |
| Log out | `application_account_logout(service, principal)` | Revoke the current session; repeated logout is harmless. |
| Log out everywhere | `application_account_logout_all(service, principal)` | Revoke all account sessions, including the current one. |
| Request recovery | `application_account_request_password_reset(service, email, out)` | Generic accepted response; queue recovery only for eligible accounts. |
| Finish recovery | `application_account_reset_password(service, token, password)` | Consume token, replace hash and revoke all sessions in one transaction. |
| Change password | `application_account_change_password(service, principal, old_password, new_password)` | Require current password; replace hash and revoke all sessions, then require login. |
| List sessions | `application_account_list_sessions(service, principal, page, out)` | Bounded list with opaque revocation IDs, creation/expiry and current-session flag. |
| Revoke session | `application_account_revoke_session(service, principal, session_id)` | Owner-only revocation; never expose an authentication token. |

The HTTP adapter cannot construct an authenticated principal from a submitted
user ID. Authenticate through the service first. Every protected operation
checks ownership again; a principal is internal request context, not JSON input.

## Proposed HTTP contract

JSON request bodies with bounded parsing; passwords and tokens never enter query
strings or logs. All account responses use `Cache-Control: no-store`. Use HTTPS
for deployed accounts. Existing numerical endpoints retain their current contract.

| Method and route | Body / purpose | Success |
| --- | --- | --- |
| `GET /api/auth/csrf` | Issue an anonymous or authenticated CSRF context. | `200`, CSRF token; no account data. |
| `POST /api/auth/register` | `{email, password}` | `202`, generic verification instructions. |
| `POST /api/auth/verify-email` | `{token}` | `200`; user proceeds to login. |
| `POST /api/auth/verification/request` | `{email}` | `202`, generic acceptance. |
| `POST /api/auth/login` | `{email, password}` | `200`, safe user summary and session cookie. |
| `GET /api/auth/me` | Current caller. | `200`, safe user summary; `401` if unauthenticated. |
| `POST /api/auth/logout` | No credentials in body. | `204`, revoke current session and clear cookie. |
| `POST /api/auth/logout-all` | Authenticated caller. | `204`, revoke all sessions and clear cookie. |
| `POST /api/auth/password/reset/request` | `{email}` | `202`, generic acceptance. |
| `POST /api/auth/password/reset/confirm` | `{token, new_password}` | `204`, require a fresh login. |
| `POST /api/auth/password/change` | `{current_password, new_password}` | `204`, require a fresh login. |
| `GET /api/auth/sessions` | Bounded cursor pagination. | `200`, owned sessions without secrets. |
| `POST /api/auth/sessions/revoke` | `{session_id}` | `204`, owner-only and idempotent. |

Safe user summary: `{id, email, email_verified}`. Never include password hashes,
session tokens, recovery tokens or internal security counters. Login failure
uses `401 invalid_credentials` for unknown accounts, incorrect passwords and
unverified accounts alike; verification can be requested separately. Registration,
verification-request and recovery-request responses must not disclose whether
the address exists. Do not replace an existing password during registration.
Unknown or expired verification/recovery tokens receive the same invalid-token
response. Use `429` plus `Retry-After` for throttling and `503` for unavailable
dependencies. Define a bounded `{ok:false, code, message}` error envelope.

## Storage and lifecycle proposal

- `users`: opaque ID, unique normalized email lookup key, delivery address,
  password hash with algorithm/parameters, verification state, creation time.
- `auth_sessions`: opaque public revocation ID, hash of the secret token, user
  ID, creation, absolute expiry, idle expiry and revocation state.
- `account_tokens`: hash of a verification/recovery token, purpose, account ID,
  expiry and consumption state. Tokens cannot be used for another purpose.
- `mail_outbox`: bounded queued delivery, retries and expiry. Treat stored mail
  payloads containing links as secrets and remove them after delivery/expiry.
- `schema_migrations`: versioned, transactional schema changes.

Provisional session policy: 30-minute idle and 24-hour absolute expiry, without
"remember me" in version one. Verification links expire after 24 hours and
recovery links after 30 minutes. These are proposed product settings, not claimed
security standards. Token consumption must be atomic under concurrent requests.
Reserve a unique email key transactionally so duplicate registration races cannot
create multiple accounts. Define and test the email normalization policy before
writing the first migration; preserve the delivery address separately.

Password recovery invalidates other outstanding recovery tokens. Avoid automatic
login after recovery. Email delivery failure must be retryable without partially
activating an account. Do not roll back an already committed password change merely
because a notification email fails.

## Authentication and existing calculator sessions

The current `client` ID is not an authentication token. Login does not claim,
merge or persist any guest session solely because its ID appears in a request.
Version one keeps guest mathematical sessions independent of account login.

Before adding account-linked saved values, introduce explicit owner binding and
authorization for every calculator/session/conversion read and mutation. Rotate
identifiers on identity transitions, clear account-specific browser state on
logout, and design explicit import of a guest snapshot. Keep the existing typed
value schema and revision checks. Public hosting also needs bounded workers and
queues; adding accounts does not make the synchronous server multiuser-ready.

## Security and resource requirements

Use a maintained Argon2id implementation with per-password random salts and
encoded parameters; benchmark the chosen settings before deployment. Never
implement password hashing or cryptographic randomness with NumForge math/RNG.
Proposed password policy: 15–128 Unicode code points, bounded UTF-8 input,
spaces allowed, no trimming or forced composition rules. Validate consistently
before hashing and check known compromised passwords through a selected provider.

Issue opaque unpredictable session secrets from an OS cryptographic RNG, store
only their hashes, and rotate on login. Browser authentication uses an HttpOnly,
Secure, SameSite cookie with narrow scope; keep secrets out of browser storage
and JSON responses. Protect every cookie-authenticated state-changing request,
including login, with CSRF tokens and origin validation. A separate native/API
token contract can be designed later; do not reuse calculator IDs as bearer tokens.

Throttle credential and email attempts, bound concurrent hash jobs and queued
mail, and make unknown-account credential checks comparable in work to real ones.
Authentication has separate resource controls from the five-second numerical
budget. Test the generic responses and timing behavior; identical wording alone
does not prevent account enumeration. Never record passwords, cookies or raw
tokens in diagnostic output. Select dependencies and trusted proxy handling
before implementation.

These requirements follow [OWASP password storage guidance](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html),
[authentication guidance](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html),
and [session management guidance](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html).
They require implementation and security review; this proposal is not a security
certification.

## Implementation order and acceptance

1. Agree on identifiers, email/password policy, adapters, dependencies and exact
   request/result ownership. Add private application contracts and deterministic
   tests with fake clock, RNG and mail transport.
2. Implement transactional storage and migrations, audited password hashing,
   verification, login and session revocation. Test restart persistence,
   duplicates, every partial failure, expiry and concurrent token consumption.
3. Add HTTP adapters and real SK/EN forms consuming the same operations. Test
   cookie attributes, CSRF, generic errors, throttling and secret-free logging.
4. Add recovery and password changes before a public account rollout. Verify
   one-use tokens, session invalidation, mail retry and recovery races.
5. Design account-bound settings/history/variable persistence independently,
   including ownership isolation, schema migration and guest import.

Until these stages pass, account routes remain proposed and the UI must not claim
that registration, login or synchronization works. Social login, passkeys, MFA,
email changes and account export/deletion need their own follow-up contracts;
resolve account deletion and recovery policy before a public rollout.
