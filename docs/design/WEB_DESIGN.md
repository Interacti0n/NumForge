# Browser and result design

This page describes transport/presentation decisions. Use the
[calculator guide](../guides/CALCULATOR_WEB.md) for controls,
[HTTP reference](../reference/HTTP_API.md) for request schemas and
[application design](APPLICATION_DESIGN.md) for ownership.

## Local web interface

CMake embeds editable `apps/web/assets/` files in the server. Rebuilding updates
the executable; runtime does not locate a separate asset directory. See the
[asset source guide](../../apps/web/assets/README.md) for edit/build rules.

Calculator, Units/Currencies, Graphs, Equations and Matrices are implemented. Guide/API is a separate
utility page. Login/register pages remain informational; there is no account or
persistence backend. Internal navigation replaces tool content while retaining
the shared header/footer and same-tab workspace. Browser history uses the same
transition. Language changes preserve the active workflow.

The browser inserts expression text and sends it to C. Function availability and
arity come from the C registry; localized search descriptions/aliases do not add
another mathematics implementation. UI state, camera and layout stay in clients.
Generated responses are checked against request generations so stale work does
not replace newer input. Aborting a fetch does not replace C resource limits.

The server is loopback-bound with a bounded worker pool. Absolute monotonic receive/send
deadlines prevent slow clients from extending a request indefinitely. Origin,
framing, input/response bounds and remote-proxy configuration belong to the
[HTTP contract](../reference/HTTP_API.md) and [hosting guide](../guides/HOSTING.md).
The tested operating profile and its limits are documented in the hosting guide.

## Shared workbench layout

Tool pages share the indigo/violet/cyan palette, panel treatment, focus styles
and spacing. Roomy desktop windows use the available height between navigation
and footer; wide monitors expand the app frame up to 1920px. Short/narrow windows
retain normal page scrolling. Long lists, numeric output and large matrices
scroll within their own bounded areas, without hiding primary actions.

The calculator aligns its session with the keypad; graphs align input/preview
above history/library. Equations use input/solutions above examples/recent tasks.
Matrices give grids the full left column and divide the right into results/recent
tasks. Units align converter/categories above result/history. Currencies separate
input and result beside the rate table. Empty results show an actionable hint,
never a fabricated calculation or marketing filler.

`input-history.js` owns input-only equation/matrix recents: up to eight per tool,
20,000 UTF-8 bytes per task and 64,000 bytes per stored collection. Entries are
deduplicated, removable and local to the current browser tab. Restoring cancels
pending work, restores inputs and waits for explicit calculation. It is separate
from typed calculator history, graph sampling and planned account persistence.

## Retained results and automatic precision

`CalculatorValue` owns an unformatted numeric projection plus its exact/approximate
typed representation, context and precision-independence flag. Zero-initialize
and destroy before reuse. Formatting changes presentation, not the saved number.

The legacy evaluation cache retains one successful value/expression per client.
Its identity includes exact input text, rounding, angle and working division
policy/precision, excluding output scale/notation. Context-dependent results are
recomputed when working precision changes; exact values can be reformatted.
Error paths preserve the previous successful value.

The application owns separate bounded FIFO pools for sessions and legacy caches.
The browser uses explicit sessions; stateless and legacy HTTP clients remain
supported. Numeric objects, full snapshots and visible UI strings have separate
lifetimes. Read [HTTP caching](../reference/HTTP_API.md) and
[session snapshots](../reference/SESSION_HTTP_API.md) for limits and retry rules.

## Confirmed state and history

Preview evaluates with temporary state. Confirmation stages the owned value,
expression and display before committing them atomically. Clearing history
preserves ans through independently owned storage. Reset destroys session values;
release retains a bounded tombstone. Session identity, revision ordering and
FIFO eviction belong to the application layer, not numeric algorithms.

The browser serializes confirmations. Editing can invalidate presentation but
cannot undo a sent commit. An uncertain response is retried with the original
revision and captured input/settings; a late successful commit enters history
without overwriting a newer input's displayed result.

Each session owns non-cryptographic RNG state. Preview starts from a local copy
and records start/end states. Reformatting reuses the saved value; recalculation
at a changed working precision replays from the same start state. Successful
confirmation adopts the ending state atomically. Errors preserve confirmed RNG
state. Random expressions do not reuse confirmed history or legacy cached values.

Graph sampling/cache/inspection behavior is documented in the
[screen graph reference](../reference/SCREEN_GRAPH_HTTP_API.md#browser-refinement);
equation adapters have their own [design](POLYNOMIAL_EQUATIONS.md).
Cache, session, HTTP and browser tests verify these boundaries; see
[testing](../TESTING.md).
