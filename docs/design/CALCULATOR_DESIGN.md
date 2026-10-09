# Calculator design

The calculator is an application layer over `BigInt`, `BigRational` and `BigDecimal`. Its source stays in
`src/calculator/` rather than the public include tree; it is an application
client over the public numeric API.

Expression evaluation is the private `numforge_calculator` target. Session
ownership lives in `src/application/` / `numforge_application`; HTTP adapters
depend on that layer. See [Application design](APPLICATION_DESIGN.md).

## Modules

| Module | Responsibility |
| --- | --- |
| `calculator.c` | Shared status/error handling, precision defaults and the bounded complete `calculator_compute` pipeline used by CLI and HTTP. |
| `value.c` | Owns typed values, projects exact values to the requested decimal precision, and selects fraction or decimal output. |
| `src/application/session.c` | Private application session: confirmed values, bounded history, preview reuse and idempotent confirmation. |
| `src/application/client_store.c` | Transport-independent ownership of bounded session and legacy-cache pools. |
| `constants.c` | Maps `π`, `e`, and `φ` to the precision-aware public BigDecimal constant API. |
| `tokenizer.c` | Converts source text into location-aware tokens. Implemented for decimal literals, identifiers, whitespace, binary and postfix operators, and parentheses. |
| `parser.c` | Converts tokens into an opaque expression tree (AST). Implemented as recursive descent with postfix, power, unary, multiplicative, and additive precedence layers. |
| `evaluator.c` | Walks decimal and approximate AST branches using `CalculatorContext`; handles operators, random values and exact-to-decimal handoff. |
| `evaluator_functions.c` | Implements named decimal calls for integer, root, transcendental, hyperbolic, aggregate and rounding functions. |
| `evaluator_trigonometric.c` | Handles angle conversion, reduction and trigonometric calculator calls. |
| `exact_evaluator.c` | Walks exact AST branches with BigRational and hands exact integers or fractions to the typed result. |
| `exact_functions.c` | Implements exact powers, proven rational roots, selected integer operations and rounding. |
| `formatter.c` | Formats a completed result at the requested output scale and notation without changing the stored value. |
| `functions.c` | Immutable registry of named calls, accepted arities and implementation dispatch identifiers. |
| `src/main.c` | Interactive command-line shell around the calculator pipeline. |
| `src/web/web_api.c` | Text-to-result adapter used by the local web server. |
| `src/web/web_main.c` | Starts the loopback server, parses command-line options and opens the browser when requested. |
| `src/web/web_server.c` | Handles bounded HTTP connections, embedded assets, calculator sessions and `POST /api/evaluate`. |
| `src/web/http_request.c` | Bounded, socket-independent HTTP framing and header validation; returns incomplete, ready, malformed or oversized status. |
| `web/` | Editable SK/EN calculator and API HTML, shared CSS, and calculator JavaScript. |
| `cmake/EmbedWeb.cmake` | Generates the private web asset header in the build tree; it is never edited by hand. |

The dependencies run in one direction:

```text
input -> tokenizer -> parser/AST -> typed exact or decimal evaluator -> CalculatorValue -> formatted result
```

Both `main.c` and the web adapter call this pipeline. They own only transport,
input/output, and user-facing diagnostics; tokenization and arithmetic rules
remain in the calculator modules.


[Expression grammar](EXPRESSION_PARSER.md) and
[browser/result behavior](WEB_DESIGN.md) have separate design references.

## Evaluation policy and errors

`CalculatorContext` holds working division precision, output scale, notation,
rounding, angle unit, and a time budget. `significant_division` defaults to true: `division_scale`
then counts significant digits when an exact rational must become BigDecimal,
defaulting to 34 with half-even rounding. Exact rational divisions remain
unrounded internally within resource limits.
An explicitly false mode retains the internal legacy fixed-scale behavior;
the public BigDecimal division API always retains fixed-scale semantics.
Output defaults to 10 decimal places and accepts 0..10000. For output `N`,
working division precision is `max(34, N + 4)`. Full output (`-1`) skips final
rescaling and uses 34 working significant digits. It does not imply infinite
precision or undo intermediate rounding.

This is an operation-by-operation policy, not a guaranteed error bound for
the whole expression. Exact literal arithmetic uses reduced BigRational
values, collapsing a denominator of one to BigInt in the result. Thus
`(1E34+1)/1-1E34`, `((1E80+1)/8)*8-1E80` and `(1/3)*3-1`
evaluate exactly. Failure to fit the exact result returns an error, not a
rounded substitute. The rational becomes BigDecimal at a decimal display or
when an approximate branch needs it. Full output uses 34 working digits for
such conversion; it is not a request for infinite displayed digits.

These behaviors are regression-tested in `tests/test_calculator_contract.c`.
No automatic retry at higher precision, certified error estimate, or
`inexact`/`rounded` result metadata exists yet. Those require a separate
calculator-layer design; the stable numeric API is unchanged.

The private significant-division helper finds the coefficient-ratio exponent,
divides at the corresponding scale and then applies the original operand scales
using checked arithmetic. Thus compact huge/tiny magnitudes do not require huge
powers of ten merely to retain relative precision. For example `1E-40 / 1`
stays `1E-40`. Intermediate rounding and cancellation can still affect later
operations; there is no certified whole-expression error bound. Constants use
the same max(34, N+4) significant working precision as non-terminating division
for an N-place output request. A 500-place stored value serves ordinary
precision; higher requests calculate more digits dynamically. Each distinct
constant is copied from a small evaluation-local cache. If an operation requests
a different precision, that cache entry is recalculated at the requested
precision, so earlier calls cannot change the precision of later operands. Full output
retains the existing 34-significant-digit working policy.

`calculator_compute` opens one thread-local resource scope before parsing and
closes it after formatting and cleanup. Nested evaluator/formatter calls reuse
the existing scope; standalone calls open their own. Default limits are 5000
monotonic milliseconds, 64 MiB cumulative allocation volume, 512 KiB per
allocation and 65536 output bytes. `GetTickCount64`/`CLOCK_MONOTONIC` replace
the platform-dependent `clock()`. Budget checks inside expensive BigInt loops
cancel arithmetic and conversion safely; allocation requests are checked before
calling the system allocator. Allocation volume counts realloc requests in full
and is intentionally conservative rather than tracking live memory.

Time expiration maps to `CALCULATOR_TIME_LIMIT`; resource exhaustion maps to
`CALCULATOR_VALUE_TOO_LARGE`. Ordinary allocator failures remain out-of-memory.
Public numeric calls outside a scope retain their normal unrestricted behavior.
No thread is forcibly terminated and no OS process watchdog is used: cancellation
is cooperative at bounded-work checkpoints, not a hard real-time guarantee.
Test-only checkpoint expiration makes cancellation regressions deterministic.

Parser recursion and constructed AST depth are both capped at 256. This bounds
parser, evaluator, and destructor stack use for deeply nested parentheses,
unary chains, right-associated powers, and long left-associated expressions.
Exceeding the cap returns `CALCULATOR_VALUE_TOO_LARGE`.

`formatter.c` changes only presentation. Ordinary output is rescaled to the
requested number of decimal places with the context's rounding mode. When a
non-zero result has exponent at least `10` or at most `-10`, the scientific
path instead rounds the mantissa directly to at most that many places. Exact
operations remain exact until this optional final formatting step.

The notation setting selects Auto, plain, scientific (`1.23E+45`), mathematical
(`1.23 × 10^45`) or Fraction representation. Auto uses a reduced exact fraction
when its denominator is at most 10000, its complete text (including sign and
slash) is at most 16 characters, and that text saves at least two characters
against the rounded decimal display. Fraction mode shows an exact non-integer
as `a/b` up to 16 characters. Longer fractions and approximate results use Auto
decimal formatting; exact integers remain integers. Otherwise Auto checks the
rounded exponent first, then chooses scientific if the exponent magnitude is at
least 10 or plain text would exceed 80 characters.
Changing notation is excluded from numeric cache identity, so it reuses the
stored result and does not advance `rand` or change `ans`. Plain output above
the 65536-byte application limit fails without replacing the stored value.
The web response supplies parser-compatible `E` copy text for mathematical
notation, or disables copying when the parser range or input size is exceeded.
For a displayed exact fraction, the local web adapter supplies a separate
up-to-10-place half-even decimal hint. The browser presents it below the
fraction without changing the primary result, copy text, history or stored value.

For extreme positive or negative internal scales, such as `1E100000` or
`1E-100000`, the formatter builds scientific notation directly from the
coefficient and scale. It never allocates the enormous ordinary-decimal form
just to add or discard zeroes. An unusually large coefficient still uses the
exact `BigInt` conversion; a bounded radix conversion for that separate case
remains an optimization task.

Display output is not a universal serialization format. At extreme scales its
scientific exponent can lie outside the input parser's signed 64-bit range
(including after rounding carry). Such output remains displayable but may not
parse back. Full output round-trips exactly when the parser can represent it
and resource limits permit; this does not restore earlier division rounding.

`CalculatorError` reports a `CalculatorStatus` and a zero-based UTF-8 byte
offset in the input. The tokenizer and parser identify the token or character
that caused the error. Evaluation errors that belong to an operation, such as
division by zero, identify the operator. CLI and HTTP presentation convert that
offset to a one-based Unicode character column, so constants before an error do
not shift the displayed location.

The CLI and local HTTP adapter each accept at most 4096 input bytes. This is a
transport limit; the tokenizer and parser themselves do not impose a byte
length limit. Parser recursion and AST depth are independently capped as
described above.

## Current implementation status

The initial pipeline is complete end to end: tokenizer, owned AST, evaluator,
formatter, CLI, local web adapter, and bilingual browser interface. The browser
uses JavaScript only for UI and transport; arithmetic remains in the C process.


## Confirmed session variables

`CalculatorSession` owns up to 32 named `CalculatorValue` snapshots. Assignment
is a single top-level statement handled before parsing its RHS. Session parsing
creates variable nodes, then resolves names before either exact or approximate
evaluation. Nodes own names and borrow values for the duration of computation.
Exact integers/rationals retain their type; approximate values retain the saved
number and are not reevaluated at a new precision.

Confirmation prepares the result, history display and independent variable copy
before any state change. Failures preserve variable values, history, `ans` and
confirmed random state. Request revisions still advance on attempted evaluation.
Identical confirmation replays return the saved display without repeating an
assignment. Successful confirmations clear preview state; history cache reuse
is disabled for expressions that use variables. Context changes rematerialize
exact values when needed. Destroy/reset releases every owned variable.
