# Browser and result design

## Local web interface

`numforge_web` serves Slovak and English calculator, guide and future-tool pages and their CSS/JavaScript/PNG assets from the C
executable. CMake embeds the files from `web/` into a generated header; asset
routes use `/assets/calculator.css`, `/assets/api.css`, shared `/assets/chrome.css` and
`/assets/calculator.js`; `/assets/logo.png` supplies the header mark and favicon,
and `/assets/wordmark.png` supplies the guide introduction.
Editing a source asset and rebuilding updates the
executable without changing runtime file lookup or installation layout. Its active
tool menu links to the calculator (`/`), unit converter (`/units`) and future
graph (`/graph`) and equation solver (`/solve`) areas. Guide/API (`/api`) is a
separate header utility link, visible even when the mobile menu is closed.
Sign-in (`/login`)
and registration (`/register`)
are separate header actions. These future routes share a localized informational
page until their behavior is designed and implemented; there is no account,
payment or registration backend. On wide screens the header uses one row across
the page in the shared purple palette, with equally sized, individually marked
links and no frame around the group. Switching language keeps their positions
fixed. The desktop calculator remains fully in view at tested sizes. Internal
links replace the main content while the header and
footer remain mounted; Back and Forward use the same transition. On narrow
screens the menu opens from a button
with Escape and outside-click dismissal. The active
keypad inserts digits, parentheses, `.`, `+`, `-`, `*`, `/`, `π`, `e`, `φ`,
`^`, `²`, `³`, and `!`, then sends the complete expression to the same web
adapter used by `POST /api/evaluate`. Typing `,` directly is also valid because
the tokenizer accepts both decimal separators.

The server binds only to loopback and uses port 8765 by default. The
`--port 1-65535` option selects another port, with the same-origin check updated
to that port, while `--no-browser` suppresses automatic browser launching on
Windows. The CTest smoke test starts the actual executable on a temporary port
and exercises its HTTP transport over real sockets.

The page sends the selected output scale as `?precision=N`; its full-output
mode sends `?precision=full`. Auto sends 10; Custom uses the numeric field.
The RAD/DEG selector adds `&angle=rad` or
`&angle=deg`; the notation selector adds `&notation=auto|plain|scientific|math|fraction`.
Both settings are preserved when changing the page language. HTTP `POST` requests require an exact
`Content-Length`. If a browser sends an `Origin`, the server accepts only its
own loopback origins, preventing unrelated pages from triggering expensive
local calculations. Slovak and English routes use `?lang=sk` and `?lang=en`;
the result panel copies the displayed result through the browser clipboard API,
with a local fallback. Mathematical notation copies as parser-compatible `E`
text if it fits the input limits. Exponential, logarithmic, trigonometric,
hyperbolic, and angle-conversion controls are active. Basic abs/sign/min/max and aggregate
sum/product/mean controls, integer gcd/lcm/mod/isqrt controls
and sqrt/cbrt/root controls are active.

The browser presents the expression, output settings, result and keypad as a
single calculation flow. Function categories and the session/history panel are
beside it on wide screens and follow it on narrow screens. Function search
matches names and descriptions across all categories; clearing the search
restores the selected category. Search normalizes Slovak diacritics. Categories
are listed vertically in a panel distinct from the function buttons, with a
divider below search. Up to twelve recently used constants, operations and
functions are remembered; as many as fit appear in one row below the keypad,
with larger touch targets on narrow screens. Narrow screens scroll at normal scale so controls retain
usable touch sizes. The SK/EN API guide has a linked table of contents.
The expression field starts at one line and grows to five as text wraps;
longer input scrolls within the field without an expansion button.
Units input follows the same behavior with a three-line limit. Enter
confirms the calculation; Shift+Enter inserts a line break. A truncated result uses a visible
ellipsis and an explicit expansion button.
On desktop, the result uses the height available above the keypad before
offering full-result expansion. The calculator shell fits the viewport at ordinary window heights;
the entire right sidebar scrolls as one column, keeping functions, session
controls and History/Variables reachable in short windows. The left calculator
column also scrolls when needed. Opening a long
result shows a centered modal with its own scroll area, copy control and
outside-click dismissal, leaving the calculator in place. The API guide scrolls
only its text while the header and section menu remain fixed; at narrow widths
the menu becomes a horizontal strip. The full-width settings bar sits above the
expression and stays compact in height. The function library and session use
natural card heights inside the sidebar. The calculator and guide use colors
based on Melanie Brown's Deep Purple VS Code theme.
The six inverse trigonometric and hyperbolic calculator functions accept
both `arc` and `arcus` spellings alongside their short `a` names. The web
function search includes those aliases; all spellings dispatch to the same
operation and argument checks.
The calculator and guide footers show the CMake project version, MIT license,
copyright holder and GitHub repository.

Nonblocking sockets use absolute monotonic deadlines: two seconds for the
complete incoming request and two seconds for a response. Slow byte-by-byte
input cannot reset the receive budget. Oversized bodies return JSON 413, and
expired incoming requests return JSON 408. The server remains sequential and
loopback-only; these limits do not turn it into a public multiuser service.
The page invalidates pending results on input changes and uses request
generations to ignore stale responses. Aborting browser fetch is a UI measure;
the C pipeline independently enforces its own calculation budget.

## Retained results and automatic precision

`calculator_compute_value` creates a `CalculatorValue` owning the unformatted
BigDecimal projection and, for exact results, a tagged BigInt or BigRational.
It also stores the evaluation context and a precision-independence flag.
Zero-initialize this handle and destroy it before reuse. The original
`calculator_compute` remains a one-shot wrapper with a shared compute/format
resource budget; the installed numeric library API is unchanged.

The web adapter can retain one handle and the original expression per client.
Cache hits call only the formatter. Identity includes the exact input text,
rounding, angle mode and working division policy/precision, not output scale.
Working precision stays automatic at max(34, N+4), or 34 for full output.
A precision change in either direction recomputes context-dependent expressions.
An exact typed value can be reformatted across output precision changes,
including recurring rational quotients, proven roots and exact statistics.
Approximate values remain conservative on context changes. The type tag
does not claim a rigorous error bound.

The sequential server retains at most eight page IDs, evicts FIFO and destroys
the evicted handle. Each retained coefficient's limb allocation is bounded by
the existing 128 KiB single-allocation limit (roughly 1 MiB total limb storage,
plus fixed expressions/metadata). There is no TTL or disk persistence; reload
creates a new page ID. Formatting still has time, memory and output-size limits.
Monotonic client revisions prevent older work from replacing newer work; errors
do not discard the last successful value. Browser generation checks separately
prevent stale responses from appearing in the UI. Requests without a client ID
remain stateless. This legacy cache protocol is retained for existing clients;
the browser uses the explicit session protocol described below and requires
browser crypto for its page identifier.

Dedicated cache tests compare hits with fresh evaluation, including cancellation
of significant digits, context changes, errors and client isolation. Allocation
fault injection covers replacement and formatting without corrupting retained
values. HTTP/browser tests cover validation, eviction and page isolation.

## Confirmed state and history

`CalculatorSession` belongs to the application layer, not the installed numeric API.
The parser represents `ans` as its own AST node; evaluation borrows the last
confirmed typed value and copies its number and Quantity metadata. One-shot evaluation
has no answer and reports `ans is undefined`. No textual substitution, global
numeric state or alternate arithmetic path is involved.

`calculator_session_compute` accepts an explicit request revision and a commit
flag. Preview computes/formats without changing history. Confirmation prepares
the value, display and expression before changing state, including on allocation,
deadline or output-limit errors. A successful commit owns one history entry;
ans borrows the newest value; clearing history transfers it into owned
standalone storage so ans and variables remain available. Each entry retains its evaluation context and
original display. A changed output precision never recomputes stored ans.

History holds 16 entries, evicting the oldest. Each retained coefficient is
bounded by the pipeline's 128 KiB allocation limit, each display by 64 KiB and
each expression by 4096 bytes: history retains less than 4 MiB per session,
including fixed metadata. The preview owns one separate bounded value. The
server holds eight sessions with FIFO eviction; session eviction is reported
explicitly and never silently recreates a session on evaluation. Reload and
internal tool/language navigation preserve the tab ID. New session resets the
existing session explicitly. There is no disk persistence or TTL. Browser
history is an authoritative HTTP read projected into a bounded display mirror of
confirmed entries, numbered by successful confirmation even after older entries
drop from the 16-entry view; clicking an entry inserts its parser-compatible result at
the input cursor, and a separate button copies that result. CLI `history` lists
entries and `reset` destroys the session while retaining precision, angle and
notation settings.

The latest successful commit can be replayed with the same revision, expression
and settings; it returns the original display without recomputation or a second
history entry. Older or conflicting revisions fail. A commit clears preview
state. The latest confirmed value can still serve as a cache source when its AST
does not use ans; otherwise the next expression evaluates against the new answer.
Preview clearing is separate from session destruction.

`rand` is private calculator state. Each session owns a non-cryptographic
generator; a candidate evaluation uses a local state copy. A preview stores
the starting and ending states with its numeric value. Reformatting reuses the
value; recalculation with a changed working context starts at the same state
so its draws remain stable. A successful commit adopts the ending state and
its value atomically. Errors leave the committed generator state unchanged.
Random values are never reused from confirmed history or the legacy cache.
Each occurrence draws a new uniform 34-place decimal in `[0,1)`; the ranged
forms transform it exactly. Tests can supply a fixed private seed.

The browser serializes confirmations and suppresses previews while one is
unresolved. Editing can invalidate display but cannot abort a sent confirmation.
A late successful response still adds history without overwriting newer input's
display. On an uncertain network response, Enter retries the original request
ID and restores the captured expression/settings; it does not confirm a new expression until
that request resolves. Automatic retries never consume a new confirmation.

See [Calculator design](CALCULATOR_DESIGN.md) for evaluation and
[Application design](APPLICATION_DESIGN.md) for session ownership.
