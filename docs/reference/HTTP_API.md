# Local calculator HTTP API

## Local HTTP API

Complex expressions use the same evaluation/session endpoints, e.g.
`complex(1/3;1/3)^2`, `2+3i`, `re(z)`, `im(z)`, `conj(z)`, `abs(z)`, `arg(z)`
and `exp(z)`. Lowercase `i` is reserved; uppercase `I` remains a variable.
`sqrt(z)` / `√(z)` accepts explicit complex values and returns the principal
root, with proven rational roots staying exact. `sqrt(-1)` remains a real-domain
error; use `sqrt(-1+0i)` for exact `i`.
`z^w` and `pow(z;w)` accept principal noninteger and complex powers when
either operand is complex, via public C `bigcomplex_pow`. Integer powers
retain exact arithmetic. General powers use exp(w*ln(z)), radians and the
principal logarithm branch; both-real operands retain the integer restriction.
Zero to zero is one; zero to a positive real exponent is zero; negative real
and nonreal exponents of zero fail. Approximate snapshots retain complex
components even for a displayed real result. Failed assignments preserve state.

`sinh(z)`, `cosh(z)` and `tanh(z)` accept complex input through public C APIs.
Complex angles are radians independent of RAD/DEG; snapshots remain approximate
complex values. Sinh/cosh can grow exponentially. Tanh uses a scaled decaying
exponential for large real parts, preserving tiny imaginary tails; scale and
runtime limits still apply. Near imaginary-axis poles errors can amplify;
computed zero denominators fail. Failed commits preserve session state.

`sin(z)`, `cos(z)` and `tan(z)` use the public complex C APIs for
complex-typed input, including zero imaginary components. Angles are radians,
independent of `angle=deg`; real-only inputs keep RAD/DEG behavior. Results
are approximate complex snapshots. Tangent uses guarded sin/cos division;
near-pole errors and resource/scale limits apply. Complex inverse hyperbolic
calls remain unsupported. Failed commits preserve session state.

`asin(z)`, `acos(z)` and `atan(z)` and their arc/arcus aliases use the public
complex C APIs for complex-typed arguments, returning principal branches in
radians even with `angle=deg`. Real-only inputs keep RAD/DEG and their domains.
Snapshots remain complex decimal approximations. For example asin(i)≈0.881373587*i,
asin(2+0i)≈pi/2-1.3169578969*i and acos(2+0i)≈1.3169578969*i.
Atan(±i) is invalid; failed commits leave ans, variables and history unchanged.
See [complex cut conventions and precision](BIGCOMPLEX.md#principal-inverse-trigonometry).

`log(z)` uses base 10, and `log(z;b)` computes principal ln(z)/ln(b) when
either argument is complex, through public C `bigcomplex_log`. Zero input
and bases zero or one are invalid; negative and nonreal bases are supported.
Both-real calls retain their positive-domain restrictions. Results are
approximate complex snapshots even when displaying a real number. Near-unit
bases amplify numerical error; failed commits preserve ans and variables.

`ln(z)` accepts explicit complex input and returns an approximate principal
logarithm in radians, imaginary part in (-pi, pi]. Zero is invalid; real
`ln(-1)` remains an error, while `ln(-1+0i)` uses the positive pi branch.
Arguments and complex exponentials use radians regardless of `angle=deg`.
Exponentials are approximate and can retain small numerical residuals.
An optional final output option
`&form=cartesian|trig|exp` follows precision, angle and notation, and precedes
client/revision/action suffixes. Omission selects Cartesian. Example:
`POST /api/evaluate?precision=10&angle=rad&notation=auto&form=exp` with body
`complex(-1;0)` displays `e^(i*(π))`. Real values ignore the selected form.
Polar angles are radians even when `angle=deg` applies to real function calls.

Session/cached complex responses provide `copy` as a lossless Cartesian
constructor `complex(re;im)`, or null when it cannot fit the input limit.
This copy is independent of display rounding/form and is not a typed snapshot
of approximation provenance. Stateless responses still provide display text;
use typed session snapshots for authoritative components and their kind.

The calculator layout keeps all controls within the viewport while the result
is collapsed to five lines. Compact spacing and proportional scaling adapt to
short windows. Expanding a long result allows vertical page scrolling; collapsing
it restores the fitted layout. This layout policy does not apply to the API guide.

The web UI shows expression errors beside a short excerpt, marking the reported
token with `⟦…⟧` and including its one-based Unicode character position. An error
after the last character is described as being at the end of the expression.
The marker identifies where the error was detected, not necessarily the only
incorrect character. Resource and connection failures have no expression marker.
The HTTP error fields remain unchanged.

`numforge_web` serves the calculator. Its evaluation endpoint is:

```text
POST /api/evaluate?precision=10&angle=rad HTTP/1.1
Host: 127.0.0.1:8765
Content-Type: text/plain; charset=utf-8
Content-Length: 6

π / 2
```

A successful response is HTTP 200:

```json
{"ok":true,"result":"1.5707963268"}
```

A short exact fraction can additionally return
`{"ok":true,"result":"1/3","approx":"0.3333333333"}`. The `approx` field is
only a display hint; `result` remains the exact value.

Invalid expressions, unsupported precision values, and arithmetic errors
return HTTP 400. Calculator errors use this JSON shape:

```json
{"ok":false,"error":"division by zero at column 3","status":"division by zero","column":3}
```

`Content-Length` is required for `POST` requests; omitting it returns HTTP 411.
Browser requests that include `Origin` must come from this server's own
`http://127.0.0.1:8765` or `http://localhost:8765` origin; other origins return
HTTP 403. `--origin https://host` explicitly allows one additional origin for
an HTTPS tunnel/reverse proxy; see [hosting](../guides/HOSTING.md).
Native clients may omit `Origin`. `precision` is optional: it
accepts a non-negative whole number or `full`; if omitted, it defaults to `10`.
`angle` accepts `rad` or `deg` and defaults to `rad`; when supplied it follows
`precision` in the query string. Optional `notation=auto|plain|scientific|math|fraction`
follows `angle` and defaults to `auto`. A successful mathematical response adds
`"copy":"1.23E+45"` (or `"copy":null` if the result cannot fit the parser's
input range or size limit). Fraction results may add `approx` in any notation
that displays the exact fraction; other responses retain the existing fields.
Legacy clients may append `&client=<32 lowercase hex digits>&revision=<N>`
after the normal options. `N` is a positive increasing integer up to
9007199254740991. These parameters opt into a bounded per-client cache;
successful responses then also contain `"cached":true` or `false`.
Without them, the endpoint remains stateless and its response shape is unchanged.
The browser generates a fresh random client ID for each page load, not a cookie
or shared local-storage identity. The server holds at most eight clients with
one successful numeric value each; FIFO eviction frees the old value. Older or
duplicate revisions may compute a response but cannot replace the stored value.
Errors preserve the last successful value. This is not authentication or a
public multiuser service.

The browser adds a final `&action=start|preview|commit|delete-variable` after client/revision.
For example, start a page session with a POST to
`/api/evaluate?precision=10&angle=rad&client=<32 lowercase hex digits>&revision=1&action=start`
and an empty body. Start returns `{"ok":true,"result":""}` and is idempotent
for an existing ID. Then send expressions with action `preview` or `commit` and
increasing revisions. Start does not consume an evaluation revision.

`delete-variable` accepts a case-sensitive variable name as the plain-text body,
reclaims its slot and preserves ans/history/RNG. It requires an existing session
and a fresh revision; the same successful name/revision can be retried before
another request. See [Variables](../guides/VARIABLES.md) for deletion and UI semantics.

Preview does not change `ans` or history. Commit confirms a successful internal
value and adds history atomically; failed calculations preserve both. `ans` is
initially undefined and always refers to the stored value, not its display.
Increasing precision does not recompute its original expression. An exact
stored `ans`, such as `1/3`, can be displayed at a higher precision; a
previously approximated `ans` retains its original digits. Repeating the latest
successful commit with the same revision, expression and settings returns
the original result; conflicting or older revisions return `stale session request`.
An unresolved confirmation must be retried with its original ID before sending
another confirmation. A new intentional Enter/`=` uses a new revision.

For expressions containing `rand`, a preview keeps the same draws across
automatic repeat requests and precision changes. A successful commit adopts
those draws and advances the session generator once per occurrence. A new
intentional commit makes fresh draws. Failed requests and replay of the same
successful commit do not advance it. Each session has its own generator state;
resetting starts a new sequence; reloading retains the session. One-shot and legacy HTTP requests
draw afresh and do not cache random expressions.

The session pool is separate from the legacy cache: eight sessions, FIFO
eviction, 16 confirmed entries each and less than 64 MiB retained calculator
history per session (excluding variables, conversion history and previews).
Each entry stores input, internal value, context and display. Evaluation
of an unknown/evicted session returns `session expired; reload the page` and
never starts another session implicitly. Reload, language and guide navigation
reuse the tab ID and read its current revision from the server. New session
explicitly resets existing state through the session API. Server restart loses all sessions. IDs are
not authentication. History buttons restore only input, so expressions with
`ans` use the current answer when evaluated again. CLI `history` lists its
session entries; `reset` clears them and ans, retaining precision, angle and
notation settings.

Output precision and notation are configurable in the UI; working precision remains
automatic. Changing notation only reformats the retained numeric value.
Matching input and working context allow reformatting without another
evaluation. Exact typed integer and rational results, including divisions and
proven roots, can also be reformatted when working precision changes.
Approximate constants, irrational roots and transcendental calls require
recalculation when working precision changes.
Changing RAD/DEG also invalidates reuse. `full` keeps the existing 34-digit working
policy, not infinite precision. More requested digits do not certify accuracy
under cancellation; no rounded/inexact guarantee is inferred from the output.

Out-of-memory calculation failures return HTTP 500 with the same JSON fields.
Malformed HTTP requests return JSON HTTP 400. Oversized bodies return JSON
HTTP 413; a request that does not finish arriving within two seconds returns
JSON HTTP 408. The receive deadline covers the complete headers and body and
is not restarted by each byte. Responses also have a bounded send deadline.
Unknown routes return plain-text HTTP 404; the UI tolerates non-JSON/network
failures and ignores responses superseded by a new calculation or input edit.
The UI distinguishes connection failures from unexpected server responses and
offers retry with Enter. Calculation errors retain their status and source
position, with argument rules for recognized function-domain/arity errors.
These hints do not change the HTTP error schema or numerical API.
Transfer-Encoding is unsupported and rejected; use Content-Length framing.

The local server accepts expressions up to 4096 bytes and listens only on
loopback, using port 8765 by default. `numforge_web --port N` selects another
port from 1 through 65535, and `--no-browser` suppresses automatic browser
launching on Windows. The `NUMFORGE_WEB_NO_BROWSER=1` environment setting
also suppresses launching for headless runs. Browser origins must match the
selected loopback port or the exact additional origin configured with `--origin`.
Socket binding remains loopback-only. Forwarding headers do not grant origin access.
Error columns are one-based Unicode character positions; the calculator
internals retain zero-based UTF-8 byte offsets so source tokens remain lossless.
The example body above is exactly six UTF-8 bytes and has no trailing newline.

See [Web/API coverage](WEB_API_PARITY.md) for available calls and remaining gaps,
and [Unit HTTP API](UNIT_HTTP_API.md) for catalogue and conversion routes.
The [session HTTP API](SESSION_HTTP_API.md) adds read-only typed variables/history,
explicit lifecycle actions, a function registry and confirmed conversion history.
Its reads do not consume evaluation revisions or alter calculator previews.
