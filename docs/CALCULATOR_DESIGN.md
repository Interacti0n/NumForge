# Calculator design

The calculator is an application layer over `BigDecimal`. Its source stays in
`src/calculator/` rather than the public include tree; it is an application
client over the public numeric API.

## Modules

| Module | Responsibility |
| --- | --- |
| `calculator.c` | Shared status/error handling, precision defaults and the bounded complete `calculator_compute` pipeline used by CLI and HTTP. |
| `constants.c` | Maps `π`, `e`, and `φ` to fixed 500-decimal-place BigDecimal approximations. |
| `tokenizer.c` | Converts source text into location-aware tokens. Implemented for decimal literals, identifiers, whitespace, binary and postfix operators, and parentheses. |
| `parser.c` | Converts tokens into an opaque expression tree (AST). Implemented as recursive descent with postfix, power, unary, multiplicative, and additive precedence layers. |
| `evaluator.c` | Evaluates the AST to `BigDecimal` using `CalculatorContext`. Implements arithmetic, roots, integer calls, exponential and logarithmic calls, and selection functions. |
| `formatter.c` | Rounds a completed result to the requested output scale and selects ordinary or scientific notation. |
| `functions.c` | Immutable registry of named calls, accepted arities and implementation dispatch identifiers. |
| `src/main.c` | Interactive command-line shell around the calculator pipeline. |
| `src/web/web_api.c` | Text-to-result adapter used by the local web server. |
| `src/web/web_server.c` | Loopback-only HTTP server that serves the calculator page and `POST /api/evaluate`. |
| `src/web/http_request.c` | Bounded, socket-independent HTTP framing and header validation; returns incomplete, ready, malformed or oversized status. |
| `src/web/web_page.h` | Embedded calculator and API-guide pages. Active controls map to the current grammar; disabled controls indicate planned features only. |

The dependencies run in one direction:

```text
input -> tokenizer -> parser/AST -> evaluator -> BigDecimal -> formatted result
```

Both `main.c` and the web adapter call this pipeline. They own only transport,
input/output, and user-facing diagnostics; tokenization and arithmetic rules
remain in the calculator modules.

## Local web interface

`numforge_web` serves a self-contained page from the C executable. Its active
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
checkbox sends `?precision=full`. HTTP `POST` requests require an exact
`Content-Length`. If a browser sends an `Origin`, the server accepts only its
own loopback origins, preventing unrelated pages from triggering expensive
local calculations. Slovak and English routes use `?lang=sk` and `?lang=en`;
the result panel copies the currently displayed result through the browser
clipboard API, with a local fallback. Exponential and logarithmic controls are
active; the visible trigonometric and angle-conversion controls remain disabled
placeholders. Basic abs/sign/min/max controls, integer gcd/lcm/mod/isqrt controls
and sqrt/cbrt/root controls are active.

Nonblocking sockets use absolute monotonic deadlines: two seconds for the
complete incoming request and two seconds for a response. Slow byte-by-byte
input cannot reset the receive budget. Oversized bodies return JSON 413, and
expired incoming requests return JSON 408. The server remains sequential and
loopback-only; these limits do not turn it into a public multiuser service.
The page invalidates pending results on input changes and uses request
generations to ignore stale responses. Aborting browser fetch is a UI measure;
the C pipeline independently enforces its own calculation budget.

## Expression grammar

```text
expression  := term (('+' | '-') term)*
term        := unary (('*' | '/' | IMPLICIT_MULTIPLY) unary)*
unary       := ('+' | '-') unary | power
power       := postfix ('^' unary)?
postfix     := primary ('²' | '³' | '!')*
primary     := NUMBER | CONSTANT | '(' expression ')' | call
call        := FUNCTION '(' arguments ')' | '√' '(' expression ')'
arguments   := expression (';' expression)*
CONSTANT    := π | e | φ
```

`NUMBER` uses the BigDecimal input grammar with a calculator-only extension:
`.` and `,` are equivalent decimal separators. It also accepts an optional
uppercase `E` exponent. Standalone lowercase `e` is Euler's constant. The
sign is always a separate `PLUS` or `MINUS` token, which keeps unary and binary
operators unambiguous. The evaluator normalizes a comma to a point before
calling the public BigDecimal API.

Variables remain outside the grammar. The function registry recognizes the
names and arities listed in [API.md](API.md#named-calls). `pow` and `factorial` reuse existing operator paths.
`abs`, `sign`, `min` and `max` use decimal operations directly. Min/max retain
only the selected and current values, evaluating arguments left to right. Pending calls report `NOT_IMPLEMENTED` before
evaluating children. Numerical domains/rounding must be defined when enabling
each remaining implementation.

`gcd`, `lcm` and `mod` reuse BigInt operations, accepting signed integer-valued
arguments (including `12.00`). `mod` is a truncating remainder, not Euclidean
modulo. `isqrt` requires a non-negative integer and uses decreasing integer
Newton iteration from a power-of-two upper bound; it stops at the floor root.
No floating-point conversions or output-precision rounding are used. Fractional
evaluated arguments are rejected; prior arithmetic still follows the working
precision policy. Temporaries are cleaned up on domain, allocation and budget
failures without replacing the caller's destination.

Real roots are implemented in `src/bigdecimal/roots.c` and exposed through the
public BigDecimal API. The evaluator validates the calculator's degree limit
and passes working precision and rounding to these library calls. For canonical
`C * 10^-s`, a finite kth root exists exactly when `s` is divisible by k and
`abs(C)` is a perfect kth power; these results are not rounded internally.
Otherwise, exponent division normalizes the radicand to k times the working
precision plus one root guard digit, independent of the input's absolute scale.
Integer Newton iteration computes its floor root. A nonzero-tail marker plus
the guard digit lets BigDecimal rescaling implement all six rounding modes.
This rounds the root of the already evaluated argument, not an exact symbolic
expression. Roots use at least 34 working significant digits, or a higher
context division precision; full output retains this finite working precision.
Degree 1 is identity; degrees 1..10000 are accepted, with negative arguments
only for odd degrees. Intermediate size and cooperative time limits still apply.

Exponential and logarithmic functions are implemented in
`src/bigdecimal/transcendental.c` and exposed through the public BigDecimal API.
`exp(x)` computes e^x; `ln(x)` requires x > 0; `log(x)` has base 10; and
`log(x;b)` requires x > 0, b > 0 and b != 1. Argument reduction repeatedly
halves exponential inputs and repeatedly square-roots logarithm inputs before
evaluating guarded decimal series. The result is then reconstructed at the
requested significant precision. No binary floating-point conversion is used.
The evaluator uses at least 34 significant digits, or the higher context
precision, and all loops participate in the normal cooperative resource budget.

Call nodes own an argument-pointer array and child expressions; registry entries
have static lifetime. Array growth uses the fault-injectable allocator. Parse
failure frees partial children/arrays and preserves the caller's output handle.
Calls count toward both recursive parsing and AST depth limits (256); arity is
also capped at 256, including variadic min/max. Empty calls and wrong argument
counts report `ARGUMENT_COUNT` at the function name; malformed separators report
syntax errors. Function names are scanned as whole ASCII-letter sequences and
matched case-sensitively; underscores and numeric suffixes are not names.

`^` is right-associative and binds more tightly than unary signs and
multiplication. Thus `2^3^2` is `2^(3^2)` and `-2^2` is `-(2^2)`. Its evaluator
uses binary exponentiation: the base is an exact `BigDecimal`, while the
exponent must be a non-negative whole number represented as `BigInt`. This
keeps `1.5^3` exact while using logarithmically many BigDecimal multiplications.
`0^0` is defined as `1`; negative and fractional exponents currently return an
invalid-argument error.

Postfix operators bind tighter than unary signs and multiplication, so `-2²`
is `-(2²)` and `(2 + 3)!` is valid. Square and cube evaluate as exact
BigDecimal multiplication: `x²` is `x * x`, and `x³` is `(x * x) * x`.
Factorial delegates to `bigint_factorial`; it accepts only a non-negative whole
number up to 10000 in the calculator, and reports an invalid-argument error for
other inputs or `VALUE_TOO_LARGE` above that calculator limit.

Adjacent primaries, except two numeric tokens, imply multiplication at the normal multiplicative
precedence. This covers `πe`, `10π`, `5e`, `2(2 + 2)`, and `(1 + 2)(3 + 4)`.
The tokenizer keeps scientific notation unambiguous: `5E-1` and `1E3` remain
one numeric token, while `5e` becomes `5 * e` and `1e3` becomes `1 * e * 3`.
Only the exact UTF-8 symbols `π`, `e`, and `φ` are constants; ASCII `pi` and
`phi` remain available for future variable names.

Two numeric tokens without an operator (`2 3`, `2 .3`) are syntax errors.
A repeated decimal separator (`1.2.3`, `1,2,3`, `1E3.4`) is a lexical error.
Whitespace does not make an operator. Delimited factors such as `(2)3`,
`3!2` and `2²3` remain valid. Implicit products have the same left-associative
precedence as explicit multiplication/division: `6/2(1+2)` is `9`.
Multi-argument calls use semicolons, e.g. `gcd(12;18)`, avoiding conflict with
decimal commas. `exp` is one identifier, not `e*x*p`; `e(2)` is still a
constant times a parenthesized expression. Adjacent ASCII names require `*`
(`ee` and `esin` are unknown names). `√` requires parentheses just like `sqrt`.

## Evaluation policy and errors

`CalculatorContext` holds working division precision, output scale, rounding
and a time budget. `significant_division` defaults to true: `division_scale`
then counts significant digits for non-terminating division, defaulting to 34
with half-even rounding. Terminating division is exact within resource limits.
An explicitly false mode retains the internal legacy fixed-scale behavior;
the public BigDecimal division API always retains fixed-scale semantics.
Output defaults to 10 decimal places and accepts 0..10000. For output `N`,
working division precision is `max(34, N + 4)`. Full output (`-1`) skips final
rescaling and uses 34 working significant digits. It does not imply infinite
precision or undo intermediate rounding.

This is an operation-by-operation policy, not a guaranteed error bound for
the whole expression. Addition, subtraction and multiplication are exact on
their stored operands. The calculator reduces the coefficient denominator by
the GCD and removes factors of two and five. If no other factor remains,
division uses enough decimal places for the exact quotient before restoring
the operand scales. Thus `(1E34+1)/1-1E34` and `((1E80+1)/8)*8-1E80` give `1`.
Failure to fit the exact result returns an error, not a rounded substitute.
Non-terminating division still rounds: `(1/3)*3-1` is `-1E-34` by default.
Full output resets non-terminating division to 34 working digits;
it is not a request for the highest possible accuracy.

These behaviors are regression-tested in `tests/test_calculator_contract.c`.
No automatic retry at higher precision, certified error estimate, or
`inexact`/`rounded` result metadata exists yet. Those require a separate
calculator-layer design; the stable numeric API is unchanged.

The private significant-division helper finds the coefficient-ratio exponent,
divides at the corresponding scale and then applies the original operand scales
using checked arithmetic. Thus compact huge/tiny magnitudes do not require huge
powers of ten merely to retain relative precision. For example `1E-40 / 1`
stays `1E-40`. Intermediate rounding and cancellation can still affect later
operations; there is no certified whole-expression error bound. Constants have
500 stored decimal places regardless of the selected output limit.

`calculator_compute` opens one thread-local resource scope before parsing and
closes it after formatting and cleanup. Nested evaluator/formatter calls reuse
the existing scope; standalone calls open their own. Default limits are 5000
monotonic milliseconds, 64 MiB cumulative allocation volume, 128 KiB per
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
