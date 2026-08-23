# Calculator design

The calculator is an application layer over `BigDecimal`. Its source stays in
`src/calculator/` rather than the public include tree until its external API is
stable.

## Modules

| Module | Responsibility |
| --- | --- |
| `calculator.c` | Shared status strings, error reporting, and evaluation defaults. |
| `tokenizer.c` | Converts source text into location-aware tokens. Implemented for decimal literals, whitespace, operators, and parentheses. |
| `parser.c` | Converts tokens into an opaque expression tree (AST). Implemented as recursive descent with unary, multiplicative, and additive precedence layers. |
| `evaluator.c` | Evaluates the AST to `BigDecimal` using `CalculatorContext`. Implemented for unary signs and the four initial binary operators. |

The dependencies run in one direction:

```text
input -> tokenizer -> parser/AST -> evaluator -> BigDecimal
```

`main.c` will later own the interactive loop and call this pipeline. It should
not contain tokenization, parsing, or arithmetic rules itself.

## Initial grammar

```text
expression  := term (('+' | '-') term)*
term        := unary (('*' | '/') unary)*
unary       := ('+' | '-') unary | primary
primary     := NUMBER | '(' expression ')'
```

`NUMBER` uses the BigDecimal input grammar: optional decimal point and an
optional `e`/`E` exponent. The sign is always a separate `PLUS` or `MINUS`
token, which keeps unary and binary operators unambiguous.

Exponentiation, variables, and functions are intentionally outside this first
grammar. Add them only with explicit precedence and domain rules.

## Evaluation policy and errors

`CalculatorContext` holds a division scale and a BigDecimal rounding mode.
The default is 34 decimal places with half-even rounding. It avoids hidden
global precision and makes one expression deterministic for one context.

`CalculatorError` reports a `CalculatorStatus` and a zero-based byte offset in
the input. The first tokenizer/parser implementation should set the offset to
the token or character that caused the error. Evaluation errors that do not
belong to a token (for example division by zero) should identify the operator.

## Implementation order

1. **Complete:** tokenizer for whitespace, numeric literals, operators,
   parentheses, and exact offsets.
2. **Complete:** recursive-descent parser for the grammar above; every AST node
   owns its children and number text safely.
3. **Complete:** evaluator maps AST operators to BigDecimal operations and
   propagates arithmetic errors to the responsible operator.
4. **Complete:** CLI reads one expression and displays either a result or a
   source-positioned diagnostic.
