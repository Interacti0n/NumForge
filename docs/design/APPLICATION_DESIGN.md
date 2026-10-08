# Application and calculation boundaries

The application uses memory only; there is no database or account backend.
The public numeric library can still be built and installed alone.

| Target | Source | Responsibility |
| --- | --- | --- |
| `numforge` / installed `NumForge::numforge` | Numeric source directories | Numeric types, algorithms, unit conversion and optional caller-owned budgets. |
| `numforge_calculator` (private) | `src/calculator` | Tokenizer/AST, expression evaluation, Quantity arithmetic, owned values and numerical formatting. |
| `numforge_application` (private) | `src/application` | Session lifecycle, variables, ans/history, preview/replay, random-state ownership, confirmed conversions and bounded client/cache storage. |
| `numforge_client` (private HTTP adapters) | `src/web/web_api.c`, `unit_web.c`, `session_api.c`, `http_request.c` | Translate HTTP-oriented options into shared calls; frame requests and serialize unit responses. |
| `calculator` / `numforge_web` | CLI / server entry points | CLI interaction / sockets, route dispatch, JSON and embedded assets. |
| Browser | `web` | Input/caret, navigation, language, categories, tabs, clipboard and display mirrors. |

Dependency direction is HTTP adapter → application → expression engine →
numeric library. CLI links the application directly, without the HTTP adapter.
The expression target has no application/web include directories. All three
private support targets are excluded from the installed public package.

## Ownership

The expression engine receives explicit context and borrowed variable/ans
values, and returns an owned typed result. It does not own client IDs, session
pools, confirmation history, UI fields, accounts or persistence. For random
expressions the caller supplies state; the application decides whether preview
draws become confirmed state.

`CalculatorSession`, in `src/application/session.h`, owns variable/history
values, confirmed conversions and one preview. Clearing calculator history moves
its latest typed value into separately owned ans storage. Reset destroys values;
release retains a bounded tombstone until FIFO eviction. Confirmation, deletion and revision handling remain
shared by CLI and HTTP. UI summaries do not replace full stored values.
Formatting projects values without overwriting their exact/approximate/Quantity
representation.

`ApplicationClientStore` owns two independent fixed FIFO pools: eight sessions
and eight legacy evaluation caches. Read-only lookup never creates a session
or revives an expired one. Returned pointers are borrowed until eviction or
destruction. Invalid IDs do not allocate or evict; destruction releases all
owned values. Stores are independent instances, not mathematical globals.
The local server owns one instance and accesses it serially; this is not a
thread-safe public multiuser backend.

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

A future storage
adapter/database belongs behind the application layer, independent of math
algorithms. Accounts, ownership and authorization also belong there.

## Verification

`application_tests` links the application without HTTP and checks independent
stores, preservation, separate FIFO eviction, invalid IDs and destruction with
typed values. Existing calculator/session, CLI/HTTP, allocation-failure, fuzz
and benchmark checks keep their contracts. Package consumers reject accidental
export of private targets.
