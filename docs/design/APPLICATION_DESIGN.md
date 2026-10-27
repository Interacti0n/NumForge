# Application and calculation boundaries

Current sessions use memory only; accounts/database are not implemented.
Numeric APIs and the public engine remain independently installable.
[Architecture](ARCHITECTURE.md) owns source/target boundaries.

## Ownership

- Engine: explicit context, borrowed values/RNG, owned typed results; no IDs,
  history, UI or persistence.
- `CalculatorSession`: confirmed ans, variables, calculator/conversion history
  and preview. Confirmation stages values before atomic commit. Clearing
  calculator history preserves separately owned ans; reset destroys values;
  release keeps a bounded tombstone. Display strings never replace typed values.
- `ApplicationClientStore`: independent session/cache pools, synchronized leases,
  [expiry/retention quotas](../guides/HOSTING.md#capacity-and-state). Reads never
  create/revive sessions; invalid IDs cannot evict. Destruction releases owners.

## Presentation and future storage

Browsers retain presentation fields/session IDs and fetch authoritative
[versioned snapshots](../reference/SESSION_HTTP_API.md). Exact/approximate values,
quantity metadata and original contexts remain distinct. Lists are bounded and
paginated; full reads never silently round exact values.

## Proposed accounts and saved work

**Proposal, not implementation.** Use optional Django authentication/sessions/
CSRF/migrations with PostgreSQL, choosing supported versions when implemented.
Math stays in C. The guest executable/CLI/library work without Python or a DB.
Start isolated, with one HTTPS origin/gateway; DB/internal ports stay private.
Redis, queues and paid hosting are not initial requirements.

### Useful first account screen

| Area | Saved controls |
| --- | --- |
| Profile | Name/email, password, export/deletion |
| Defaults | Language, precision, notation, rounding, RAD/DEG |
| Tools | Unit/currency pairs, graph view/colors, editor inputs |
| Workspaces | Named tasks, duplicate/rename/delete, explicit save/optional autosave |
| Privacy | History/retention choice and explicit guest import |
| Sign-in | Remembered login, device revocation, sensitive-action reauthentication |

Restore inputs without computing. Autosave shows Saving/Saved/Offline and retains
dirty work after failure. Expected revisions detect tab/device conflicts;
preserve both versions or ask, never silently overwrite. Sign-out/account changes
clear account-specific caches and pending saves.

### Storage and ownership

Framework users/sessions own preferences, versioned JSONB workspaces and optional
history. Enforce authenticated ownership for every read/write/export/delete,
never trust submitted owner IDs. Exact numbers stay strings, with context and
approximation provenance. Index owner/update time.

Full ans/variable/history restore needs a bounded transactional C decoder;
existing snapshots only support reads. Never replay saved expressions/random
calls to reconstruct values. Inputs/settings can ship first. Bind private C
execution/session access to authenticated owners before restoring runtime state;
anonymous client IDs and Origin checks are not authorization.

Proposed pilot caps: 100 workspaces/25 MiB per user, 1 MiB/workspace and
200 KiB/history entry, retaining stricter tool limits. Reject oversized values
without truncation. These are proposed defaults, not current guarantees.

### Implementation order and completion gates

1. Local PostgreSQL/service, migrations, least-privilege credentials outside Git,
   tested backup/restore.
2. Private pilot login/logout, remembered sessions, expiry/revocation, CSRF and
   login-attempt limits; framework password handling with measured Argon2id.
3. Defaults/named inputs, guest import, autosave, quotas/conflicts; verify backend/
   DB restart, second-browser restore and two-user isolation.
4. Device management/export/deletion/recovery. Test mail verification/reset before
   public registration; private users can be manually provisioned.
5. Typed C restore plus authenticated execution bridge; then optional saved
   history/variables/ans.
6. Verify revoked sessions, cross-user access, sign-out/cache isolation, lost
   writes, outages, migration rollback, recovery and mixed load before deployment.

Use Secure/HttpOnly/SameSite cookies with server expiry, never browser-stored
credentials. A changed tunnel origin requires signing in again, not losing DB
data. Define deletion/backup expiry explicitly.

[Django auth](https://docs.djangoproject.com/en/5.2/topics/auth/default/) ·
[Sessions](https://docs.djangoproject.com/en/5.2/topics/http/sessions/) ·
[Password storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html) ·
[Session management](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html)

## Verification

Application tests link without HTTP and check isolated stores, preserved values,
eviction and destruction. Session/CLI/HTTP/fault suites cover adapters; installed
consumers reject private target exports. [Testing](../TESTING.md).
