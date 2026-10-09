# Expression parser design

## Expression grammar

```text
expression  := term (('+' | '-') term)*
term        := unary (('*' | '/' | IMPLICIT_MULTIPLY) unary)*
unary       := ('+' | '-') unary | power
power       := postfix ('^' unary)?
postfix     := primary ('²' | '³' | '!')*
primary     := NUMBER | CONSTANT | 'ans' | '(' expression ')' | call
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

Session parsing resolves user-defined identifiers to borrowed typed values;
one-shot parsing retains its existing grammar. See [Variables](../guides/VARIABLES.md). The function registry recognizes the
names and arities listed in [Calculator expressions](../reference/CALCULATOR_EXPRESSIONS.md#named-calls). `pow` and `factorial` reuse existing operator paths.
`abs`, `sign`, `min` and `max` use decimal operations directly. Min/max retain
only the selected and current values, evaluating arguments left to right.
`sum`, `product`, and `mean` evaluate one to 256 arguments left to right and
delegate aggregation to the public BigDecimal API. Sum and product are exact.
Mean accumulates exactly and performs one exact-first division by the count;
only a recurring quotient depends on working precision.
`median` orders references without changing arguments and returns an exact
middle value or exact average. `geomean` accepts non-negative inputs and takes
the root of their exact product. `harmean` requires positive inputs and builds
the reciprocal sum as an exact numerator/denominator before its final division.
`variance` and `stdevp` use population denominator `n`; `stdev` uses sample
denominator `n−1` and requires two values. The library evaluates
`n*sum(d²)-sum(d)²` exactly for `d=x−x₀`, avoiding cancellation caused by a
rounded mean and avoiding huge squares from a shared offset.
Standard deviation uses guarded internal variance before the final root.
`floor`, `ceil`, `trunc`, and `round` call the corresponding public BigDecimal
operations. `round(x)` defaults to zero places; its optional second argument is
an exact signed 64-bit integer and may be negative. Midpoints use half-even.

`gcd`, `lcm` and `mod` reuse BigInt operations, accepting signed integer-valued
arguments (including `12.00`). `mod` is a truncating remainder, not Euclidean
modulo. `isqrt` requires a non-negative integer and uses decreasing integer
Newton iteration from a power-of-two upper bound; it stops at the floor root.
`npr` and `ncr` reuse the public exact combinatorics operations and require
non-negative integers with `r <= n`.
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
In these real BigDecimal kernels, degree 1 is identity and degrees 1..10000
are accepted, with negative arguments only for odd degrees. The typed calculator
automatically promotes negative even roots to principal complex roots through
the public BigComplex API; real odd roots remain real. Intermediate size and cooperative time limits still apply.

Exponential and logarithmic functions are implemented in
`src/bigdecimal/transcendental.c` and exposed through the public BigDecimal API.
In the real kernels, `exp(x)` computes e^x, `ln(x)` requires x > 0, `log(x)`
has base 10, and `log(x;b)` requires x > 0, b > 0 and b != 1. The typed
calculator promotes negative logarithm arguments/bases automatically through
BigComplex; zero input and bases 0 or 1 remain invalid. Argument reduction repeatedly
halves exponential inputs and repeatedly square-roots logarithm inputs before
evaluating guarded decimal series. The result is then reconstructed at the
requested significant precision. No binary floating-point conversion is used.
The evaluator uses at least 34 significant digits, or the higher context
precision, and all loops participate in the normal cooperative resource budget.

The typed evaluator similarly promotes asin/acos outside [-1,1], acosh below
1, atanh outside [-1,1] and fractional powers of negative bases. Complex
results always use radians; singularities and integer/ordering constraints
remain errors. See [automatic complex domains](../reference/CALCULATOR_EXPRESSIONS.md#automatic-complex-domains).

Public BigDecimal trigonometry always uses radians. `CalculatorContext` adds an
angle unit, defaulting to RAD. In DEG mode complete turns are removed exactly
before forward arguments are converted with
guarded π before the library call, and inverse results are converted back to
degrees; exact tangent poles at `90 + 180k` degrees are rejected before the
conversion. `radians(x)` and `degrees(x)` always perform the named conversion,
independent of context. The CLI commands `angle rad` and `angle deg`, HTTP
`angle=rad|deg`, and the browser selector all configure the same policy.
Before a forward trigonometric call, the calculator estimates the argument's
integer-digit magnitude and reevaluates its argument subtree with the guard
precision required by π-based reduction. This keeps symbolic values such as
`sin(π)` and `sin(1E50*π)` aligned with the reduction constant instead of
discarding the decisive low digits before the library call.

Call nodes own an argument-pointer array and child expressions; registry entries
have static lifetime. Array growth uses the fault-injectable allocator. Parse
failure frees partial children/arrays and preserves the caller's output handle.
Calls count toward both recursive parsing and AST depth limits (256); arity is
also capped at 256, including variadic selection and aggregate calls. Empty
calls and wrong argument counts report `ARGUMENT_COUNT` at the function name; malformed separators report
syntax errors. Function names are scanned as whole ASCII-letter sequences and
matched case-sensitively; underscores and numeric suffixes are not names.

`^` is right-associative and binds more tightly than unary signs and
multiplication. Thus `2^3^2` is `2^(3^2)` and `-2^2` is `-(2^2)`. Its evaluator
uses binary exponentiation: exact rational bases and whole-number exponents
stay rational, including negative exponents. Approximate bases use BigDecimal
with the current working precision. `0^0` is defined
as `1`; zero to a negative exponent is division by zero, and fractional
exponents return an invalid-argument error.

Postfix operators bind tighter than unary signs and multiplication, so `-2²`
is `-(2²)` and `(2 + 3)!` is valid. Square and cube preserve an exact
rational operand; `x²` is `x * x`, and `x³` is `(x * x) * x`.
Factorial delegates to `bigint_factorial`; it accepts only a non-negative whole
number up to 10000 in the calculator, and reports an invalid-argument error for
other inputs or `VALUE_TOO_LARGE` above that calculator limit.

Adjacent primaries, except two numeric tokens, imply multiplication at the normal multiplicative
precedence. This covers `πe`, `10π`, `5e`, `2(2 + 2)`, and `(1 + 2)(3 + 4)`.
The tokenizer keeps scientific notation unambiguous: `5E-1` and `1E3` remain
one numeric token, while `5e` becomes `5 * e` and `1e3` becomes `1 * e * 3`.
Only the exact UTF-8 symbols `π`, `e`, and `φ` are constants; ASCII `pi` and
`phi` are available as session variable names.

Two numeric tokens without an operator (`2 3`, `2 .3`) are syntax errors.
A repeated decimal separator (`1.2.3`, `1,2,3`, `1E3.4`) is a lexical error.
Whitespace does not make an operator. Delimited factors such as `(2)3`,
`3!2` and `2²3` remain valid. Implicit products have the same left-associative
precedence as explicit multiplication/division: `6/2(1+2)` is `9`.
`convert(value; "from"; "to")` uses one numeric child plus two owned,
case-sensitive catalogue IDs in the call AST. Quoted IDs are restricted tokens,
accepted only in the two unit positions, and introduce no string value type.
Exact evaluation delegates to the rational unit API; approximate evaluation
delegates to the decimal unit API. Function registration reserves `convert`,
while existing call visitors track variables, ans and randomness in its numeric
child. See [unit expression contract](../reference/UNITS.md#calculator-expressions).

`qty(value; "unit")` shares the restricted literal parser. Expressions containing
quantities enter the typed dimension evaluator; numeric-only subtrees and
dimension-validated operations delegate to the established exact/decimal
evaluators without reparsing text or resetting the runtime budget. Values retain
six dimension exponents, affine temperature semantics and an optional display
unit. Copying, session storage, caching and formatting preserve this metadata.
See [Quantity contracts and current scope](../reference/QUANTITIES.md).

Multi-argument calls use semicolons, e.g. `gcd(12;18)`, avoiding conflict with
decimal commas. `exp` is one identifier, not `e*x*p`; `e(2)` is still a
constant times a parenthesized expression. Adjacent ASCII names require `*`
(`ee` and `esin` are unknown names). `√` requires parentheses just like `sqrt`.

See [Calculator design](CALCULATOR_DESIGN.md) for evaluation and errors.
