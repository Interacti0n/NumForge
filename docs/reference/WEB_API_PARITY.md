# Web actions and HTTP API coverage

Audited against the current implementation on 8 October 2026. This page records
the implemented HTTP contract. A mathematical or session
operation exposed by the web should also have a documented HTTP contract usable
by another client. The web should consume that same contract.

The implemented [application boundaries](../design/APPLICATION_DESIGN.md) separate
expression evaluation, session/client ownership and HTTP adapters. No database
is added; saved values remain bounded application-owned memory.

## Available calls

| Web operation | Current HTTP equivalent | Coverage |
| --- | --- | --- |
| Calculate, format, choose RAD/DEG or precision | `POST /api/evaluate`, expression body and output options | Available |
| Read-only calculator preview | `action=preview` with session ID and revision | Available |
| Confirm result, update ans/history | `action=commit` | Available |
| Create or replace a variable | Commit `x=2/3` | Available |
| Read one variable or ans | `GET /api/session/value?client=…&name=x` (or `ans`) | Available, typed snapshot |
| Delete one variable, reclaim capacity | `action=delete-variable`, name body | Available |
| Start a new empty session | `action=start` with a fresh client ID | Available; start on an existing ID preserves it |
| Show all variables and their values | `GET /api/session/variables` | Available, paginated authoritative values |
| Show/retrieve calculator history | `GET /api/session/history`, optional saved `id` | Available, original values/settings |
| Reset/release/clear history | `POST /api/session`, `action=reset`, `release` or `clear-history` | Available; clearing history preserves ans |
| Discover functions, aliases and arities | `GET /api/functions` | Available; web consumes the registry |
| List unit catalogue and sources | `GET /api/units` | Available |
| Validate compatibility, preview a conversion | `POST /api/convert` | Available |
| Confirm a conversion | `POST /api/conversions?action=commit` | Available, independent ordered history |
| List/restore/clear conversion history | `GET /api/conversions`, optional saved `id`; `POST` with `action=clear` | Available, no reevaluation |

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

## Contract for future tools

Every new mathematical or state operation must ship with its application-layer
operation, documented HTTP contract and web integration in the same feature.
Use the versioned [session API](SESSION_HTTP_API.md) for saved values and contexts.
Tests must cover independent clients, expiration, capacity, retry/stale mutations,
failed operations, exact/approximate/Quantity values and read-only preservation.
Graphs and solvers must share the C evaluator rather than implement mathematics
again in JavaScript. Presentation remains a client responsibility.

HTTP coverage is distinct from a public C calculator/session ABI. The installed
SDK contains the numerical library; private application headers are not exported.
