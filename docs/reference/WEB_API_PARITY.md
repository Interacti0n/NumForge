# Web actions and HTTP API coverage

Audited against the current implementation on 8 October 2026. This page records
implemented calls separately from proposed endpoints. A mathematical or session
operation exposed by the web should also have a documented HTTP contract usable
by another client. The web should consume that same contract.

The implemented [application boundaries](../design/APPLICATION_DESIGN.md) separate
expression evaluation, session/client ownership and HTTP adapters. No database
is added; missing routes below remain separate implementation work.

## Available calls

| Web operation | Current HTTP equivalent | Coverage |
| --- | --- | --- |
| Calculate, format, choose RAD/DEG or precision | `POST /api/evaluate`, expression body and output options | Available |
| Read-only calculator preview | `action=preview` with session ID and revision | Available |
| Confirm result, update ans/history | `action=commit` | Available |
| Create or replace a variable | Commit `x=2/3` | Available |
| Read one variable or ans | Preview `x` or `ans` | Available as expression evaluation |
| Delete one variable, reclaim capacity | `action=delete-variable`, name body | Available |
| Start a new empty session | `action=start` with a fresh client ID | Available; start on an existing ID preserves it |
| Show all variables and their values | Web retains confirmed display snapshots | Missing a server list endpoint |
| Show/retrieve calculator history | Web retains display entries; C session owns confirmed values | Missing a server history endpoint |
| Reset/release the existing session | Web reload creates a fresh ID | Missing an explicit reset/release endpoint |
| Discover supported functions, aliases and arities | Function buttons/help are defined in web assets | Missing a machine-readable function catalogue |
| List unit catalogue and sources | `GET /api/units` | Available |
| Validate compatibility, preview a conversion | `POST /api/convert` | Available |
| Confirm a conversion and obtain an authoritative value | `POST /api/convert` with `snapshot=1` | Available; conversion does not mutate calculator state |
| List, restore or clear confirmed conversion history | Tab-local browser state, including original snapshot/settings | Missing server history operations |

See [API](../API.md), [Variables](../guides/VARIABLES.md) and [Unit HTTP API](UNIT_HTTP_API.md)
for the implemented parameter grammar, limits and errors. Graphs, equation
solving and accounts are placeholders and have no implemented web/API behavior.

Input editing, caret insertion, clipboard access, language, tab selection,
expansion and scrolling are client presentation actions. HTTP clients receive
the underlying values and choose their own presentation. Swapping units is a
new conversion request with exchanged `from`/`to`; no independent mathematical
swap endpoint is necessary.

## Deletion example that works today

PowerShell, using `curl.exe` explicitly. Set the base URL to the running server.
This example creates an independent session; it does not delete a variable from
an already-open browser session. Use that browser's client ID and its next
revision to operate on that session instead. IDs are not authentication.

```powershell
$base = 'http://127.0.0.1:18770'
$client = [guid]::NewGuid().ToString('N')
$endpoint = "$base/api/evaluate?precision=10&angle=rad&client=$client"

# Start, then store x.
curl.exe -sS -X POST -H 'Content-Length: 0' "$endpoint&revision=1&action=start"
curl.exe -sS -H 'Content-Type: text/plain' --data-raw 'x=5' "$endpoint&revision=2&action=commit"

# Delete x; success: {"ok":true,"result":""}.
curl.exe -sS -H 'Content-Type: text/plain' --data-raw 'x' "$endpoint&revision=3&action=delete-variable"

# x is now undefined; ans still holds 5.
curl.exe -sS -H 'Content-Type: text/plain' --data-raw 'x' "$endpoint&revision=4&action=preview"
curl.exe -sS -H 'Content-Type: text/plain' --data-raw 'ans' "$endpoint&revision=5&action=preview"
```

An uncertain deletion retries the same name/revision before another session
request. Older assignments cannot be replayed to undo deletion. A valid missing
name is a successful no-op. Deletion preserves ans, history and random state.

## Proposed implementation order

These are design tasks, not currently supported routes.

1. Add a read-only session API for listing variables and calculator history,
   including current revision, capacity and ans availability. Web lists must
   use the server response rather than infer state from a bounded local history.
   Reading must not consume revisions, draw random numbers or change previews.
2. Define a versioned value representation before exposing full saved values:
   exact integers/rationals, finite approximations, dimensions, selected unit,
   temperature point/interval semantics and original computation settings.
   Display text is not the stored mathematical value. Bound aggregate output,
   paginate lists and allow separately requested full values; never silently
   omit entries or round exact values because the response is large.
3. Add explicit session reset/release operations with ordered, retryable
   mutations. Separate clearing calculator history from clearing ans; today ans
   borrows the newest history value, so deleting history needs an ownership
   change or an explicitly documented reset contract first.
4. Expose the function registry as a machine-readable catalogue and let the web
   use it for discoverable names/aliases/arities. Keep domain/help metadata
   synchronized without moving mathematical rules to JavaScript.
5. Move confirmed conversion history behind a separate session API: confirm,
   list/get and clear, using authoritative value snapshots and original settings.
   Restore must not reevaluate old expressions against changed variables.
   Keep converter history independent from calculator ans/history and preserve
   current tab isolation, capacity limits and retry guarantees.

Choose final route names, response schema and versioning before implementation.
Every new web tool should add its underlying API and integration tests in the
same feature. Tests should cover independent clients, expiration, capacity,
replay/stale mutations, failed operations, exact/approximate/Quantity values and
the absence of state changes during read-only access.

HTTP API coverage is distinct from a public C calculator/session API. The
calculator is currently an application layer with private headers; publishing
its C ABI needs its own ownership, lifecycle and compatibility contract.
