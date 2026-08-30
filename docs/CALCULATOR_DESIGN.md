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
`²`, `³`, and `!`, then sends the complete expression to the same web adapter used by
`POST /api/evaluate`. Typing `,` directly is also valid because the tokenizer
accepts both decimal separators.

The page sends the selected output scale as `?precision=N`; its full-output
checkbox sends `?precision=full`. The visible root, absolute-value,
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
number up to `BIGINT_FACTORIAL_MAX_N`, and reports an invalid-argument error
for other inputs.

Adjacent primaries imply multiplication at the normal multiplicative
precedence. This covers `πe`, `10π`, `5e`, `2(2 + 2)`, and `(1 + 2)(3 + 4)`.
The tokenizer keeps scientific notation unambiguous: `5E-1` and `1E3` remain
one numeric token, while `5e` becomes `5 * e` and `1e3` becomes `1 * e * 3`.
Only the exact UTF-8 symbols `π`, `e`, and `φ` are constants; ASCII `pi` and
`phi` remain available for future variable names.

## Evaluation policy and errors

`CalculatorContext` holds a division scale, output scale, and a BigDecimal
rounding mode. Division defaults to 34 decimal places with half-even rounding.
Output defaults to 10 decimal places; requesting more output places increases
division precision by four guard digits. The special output scale `-1` means
full output and does not add a final rescale. This avoids hidden global
precision and makes one expression deterministic for one context.

`formatter.c` changes only presentation: it applies the requested output scale
with the context's rounding mode, then switches to scientific notation when a
non-zero result has exponent at least `10` or at most `-10`. Exact operations
remain exact until that optional final formatting step.

For values with a negative internal scale, such as `1E100000`, the formatter
builds scientific notation directly from the coefficient and scale. It never
allocates the 100001-character ordinary-decimal form just to discard its zeroes.
An unusually large coefficient still uses the exact `BigInt` conversion; a
bounded radix conversion for that separate case remains an optimization task.

`CalculatorError` reports a `CalculatorStatus` and a zero-based byte offset in
the input. The tokenizer and parser identify the token or character that caused
the error. Evaluation errors that belong to an operation, such as division by
zero, identify the operator.

## Implementation order

1. **Complete:** tokenizer for whitespace, numeric literals, constant
   identifiers, operators, parentheses, and exact offsets.
2. **Complete:** recursive-descent parser for the grammar above; every AST node
   owns its children and number text safely.
3. **Complete:** evaluator maps AST operators and constants to BigDecimal
   values and propagates arithmetic errors to the responsible operator.
4. **Complete:** CLI reads one expression and displays either a result or a
   source-positioned diagnostic.
5. **Complete:** local web adapter evaluates plain expression text and output
   precision through the same pipeline; the server returns JSON and never
   performs arithmetic in browser JavaScript.
