# Calculator design

The calculator is an application layer over `BigDecimal`. Its source stays in
`src/calculator/` rather than the public include tree until its external API is
stable.

## Modules

| Module | Responsibility |
| --- | --- |
| `calculator.c` | Shared status strings, error reporting, and evaluation/output-precision defaults. |
| `constants.c` | Maps `π`, `e`, and `φ` to fixed 200-decimal-place BigDecimal approximations. |
| `tokenizer.c` | Converts source text into location-aware tokens. Implemented for decimal literals, identifiers, whitespace, binary and postfix operators, and parentheses. |
| `parser.c` | Converts tokens into an opaque expression tree (AST). Implemented as recursive descent with postfix, power, unary, multiplicative, and additive precedence layers. |
| `evaluator.c` | Evaluates the AST to `BigDecimal` using `CalculatorContext`. Implemented for unary signs, exact binary exponentiation, square, cube, factorial, and binary operators. |
| `formatter.c` | Rounds a completed result to the requested output scale and selects ordinary or scientific notation. |
| `src/main.c` | Interactive command-line shell around the calculator pipeline. |
| `src/web/web_api.c` | Text-to-result adapter used by the local web server. |
| `src/web/web_server.c` | Loopback-only HTTP server that serves the calculator page and `POST /api/evaluate`. |
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
clipboard API, with a local fallback. The visible root, absolute-value,
trigonometric, logarithmic, and exponential controls are disabled placeholders.
They document the intended UI surface, but do not currently add tokens or
affect evaluation.

## Initial grammar

```text
expression  := term (('+' | '-') term)*
term        := unary (('*' | '/' | IMPLICIT_MULTIPLY) unary)*
unary       := ('+' | '-') unary | power
power       := postfix ('^' unary)?
postfix     := primary ('²' | '³' | '!')*
primary     := NUMBER | CONSTANT | '(' expression ')'
CONSTANT    := π | e | φ
```

`NUMBER` uses the BigDecimal input grammar with a calculator-only extension:
`.` and `,` are equivalent decimal separators. It also accepts an optional
uppercase `E` exponent. Lowercase `e` is reserved for Euler's constant. The
sign is always a separate `PLUS` or `MINUS` token, which keeps unary and binary
operators unambiguous. The evaluator normalizes a comma to a point before
calling the public BigDecimal API.

Variables and general functions are intentionally outside this first grammar.
Add them only with explicit precedence and domain rules.

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
number up to 5000 in the calculator, and reports an invalid-argument error for
other inputs or `VALUE_TOO_LARGE` above that calculator limit.

Adjacent primaries imply multiplication at the normal multiplicative
precedence. This covers `πe`, `10π`, `5e`, `2(2 + 2)`, and `(1 + 2)(3 + 4)`.
The tokenizer keeps scientific notation unambiguous: `5E-1` and `1E3` remain
one numeric token, while `5e` becomes `5 * e` and `1e3` becomes `1 * e * 3`.
Only the exact UTF-8 symbols `π`, `e`, and `φ` are constants; ASCII `pi` and
`phi` remain available for future variable names.

## Evaluation policy and errors

`CalculatorContext` holds a division scale, output scale, and a BigDecimal
rounding mode, plus a soft CPU-time limit. Division defaults to 34 decimal
places with half-even rounding. Output defaults to 10 decimal places. For a
numeric output scale `N`, division uses `max(34, N + 4)` places so output has
four guard digits; `N` must be no greater than `INT64_MAX - 4`. The special
output scale `-1` means full output and skips the final output rescale, but
division still uses 34 places. Thus `full` preserves exact finite results but
does not make a recurring division infinite or exact. This avoids hidden
global precision and makes one expression deterministic for one context.

The default `time_limit_ms` is 5000. The evaluator checks the elapsed CPU time
between AST operations and during every binary-exponentiation iteration. It
returns `CALCULATOR_TIME_LIMIT`, rendered as `TLE` by the web adapter, once the
limit is exceeded. A BigInt primitive already in progress cannot be safely
interrupted, so this is a soft rather than a hard real-time bound.

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
