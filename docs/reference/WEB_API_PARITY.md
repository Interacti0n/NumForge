# Web actions and HTTP API coverage

Web mathematical operations use the shared C application layer and documented
HTTP routes. Public C/Python availability is listed in the [API guide](../API.md);
private application/session headers are not installed SDK interfaces.

## Available calls

| Web operation | HTTP contract |
| --- | --- |
| Expressions, precision, notation and complex display | [Evaluation](HTTP_API.md) |
| Preview/commit, variables, answer, history and function registry | [Sessions](SESSION_HTTP_API.md) |
| Uniform, adaptive and screen-space graphs; marker verification | [Sampling](GRAPH_HTTP_API.md), [adaptive curves](ADAPTIVE_GRAPH_HTTP_API.md), [screen sampling](SCREEN_GRAPH_HTTP_API.md), [verification](GRAPH_ANALYSIS_HTTP_API.md) |
| Exact polynomial equations and bounded numerical root discovery | [Polynomial](EQUATION_HTTP_API.md), [numerical](NUMERIC_ROOT_HTTP_API.md) |
| Exact matrix determinant, rank, product, square solve and general linear-system families | [Matrix editor and HTTP](MATRIX_API.md#web-editor-and-http) |
| Unit catalogue, conversion and conversion history | [Units](UNIT_HTTP_API.md), [sessions](SESSION_HTTP_API.md) |

Accounts remain browser placeholders. There is no account API or database.
Input editing, clipboard, language and scrolling are presentation actions.
Swapping units sends a conversion with exchanged `from`/`to` values.

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

A new web mathematical/state operation must include its application operation,
documented HTTP contract and web integration. Graphs and solvers share the C
evaluator. Follow the [feature checklist](../project/FEATURE_CHECKLIST.md) and
[application boundaries](../design/APPLICATION_DESIGN.md).
Session changes also need independent-client, expiry/capacity, retry/stale,
failed-operation and read-only preservation coverage across supported value kinds.
