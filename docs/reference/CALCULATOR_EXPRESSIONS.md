# Calculator expression reference

## Automatic complex domains

Dimensionless calculator inputs automatically use complex arithmetic where the
real domain ends: sqrt(x<0), ln/log of negative input or base, asin/acos outside
[-1,1], acosh(x<1), atanh outside [-1,1], negative even roots and noninteger
powers of negative bases. No explicit +0i is required. Variables, ans and nested
calls follow the same rule. Real inverse-trigonometric results follow RAD/DEG;
complex results always use radians, including automatically promoted results.
Real odd roots remain real. Singularities (ln(0), atanh(±1), atan(±i), zero
logarithm bases, base 1 and division by zero) remain errors. Integer-only and
ordering/statistical functions retain their documented domains; this does not
add gamma-factorial, complex ordering or complex unit quantities.

Public BigDecimal APIs retain their real domains. Promotion belongs to the typed
calculator; numerical complex work uses the public BigComplex C API.
`asinh`, `acosh`, `atanh` and their arc/arcus aliases now accept complex inputs.
They return approximate principal branches; see [BigComplex](BIGCOMPLEX.md).

## Calculator expressions

`complex(re;im)` constructs a complex value from two real, dimensionless
arguments. For example `complex(0;1)^2` is `-1` and
`complex(1/3;1/3)^2` is `(2/9)*i`. Supported operations are unary signs,
addition, subtraction, multiplication, division and signed integer powers
(`^`, `pow`, `²`, `³`); real integer exponents use signed 64-bit range.
Exact integer powers remain rational-complex. Noninteger or complex exponents
use approximate principal powers, with automatic complex promotion for negative
real bases and noninteger exponents. Positive real bases keep real results. Mixing an approximate input
(constants, irrational functions, decimal division mode) explicitly projects
exact components at working precision and produces decimal-complex values.
Finite decimal literals remain exact in the default exact evaluation policy.

Lowercase `i` is the reserved imaginary unit; `2+3i`, `2+3*i` and
`complex(2;3)` are equivalent. Uppercase `I` is still a valid variable.
Adjacent ASCII names remain a single identifier: `xi` is a variable, while
`x*i` is a product. `x`, `y` and `xy` are independent variables.
Complex values retain their type even when the imaginary component is
zero. Real-only calls, complex arguments inside `complex(...)`, quantities and
unit conversions reject complex values rather than discarding a component.
Negative dimensionless real arguments are promoted to the complex domain:
`sqrt(-1) = i`, `sqrt(-4/9) = (2/3)*i`. This also applies to `√(x)`,
variables and composed expressions. Nonnegative real arguments retain real
results; quantity roots keep their existing dimensional rules.
Explicit complex arguments select the principal root: `sqrt(-1+0i) = i`,
`sqrt(3+4i) = 2+i`, `sqrt(-3-4i) = 1-2i`. The real component is nonnegative;
the imaginary sign follows the input, with positive imaginary roots on the
negative real axis. Zero maps to zero. Rational component roots stay exact;
other roots use approximate decimal components. `√(z)` is the same operation.

`re(z)` and `im(z)` return real scalar components with exact fractions intact;
`conj(z)` preserves the representation and negates only the imaginary component.
On real values, `re` and `conj` return the value and `im` returns exact zero.
`abs(z)` returns an exact rational magnitude when possible, otherwise an
approximate decimal square root of the squared modulus. `arg(z)` returns an
approximate principal argument in radians, (-pi, pi], independently of RAD/DEG;
zero is invalid. Real functions can consume extracted components, e.g. `sin(re(z))`.

`exp(z)` supports complex inputs. An explicit Euler constant base in `e^z` or
`pow(e;z)` also accepts complex exponents via exp(re)*(cos(im)+i*sin(im)), radians.
Other bases accept complex exponents through principal powers. These numerical
exponentials do not infer symbolic identities: `e^(π*i)` may retain a tiny
imaginary residual.

`z^w` and `pow(z;w)` with either operand complex use exp(w*ln(z)) for
noninteger or complex exponents, with radians and the principal logarithm
branch (-pi, pi]. For example `i^i` is approximately `0.2078795764`,
`2^i` is `0.7692389014 + 0.6389612763*i`, and `(-1+0i)^(1/2)`
approaches `i` and may retain a tiny real residual. Real noninteger exponents
are accepted too: `2^0.5` stays real; `(-1)^0.5` selects the principal complex
root. `cbrt(-8)` and `root(-8;3)` retain their real result -2, whereas
`(-8)^(1/3)` is a principal complex power. Even roots of negative real numbers
promote automatically; explicit complex `root`/`cbrt` return principal roots.
Zero to zero is one; zero to a positive real exponent is zero; negative
real exponents fail with division by zero and nonreal exponents are invalid.
Integer powers retain exact arithmetic; complex-typed exponents use the
approximate path even when their imaginary part is zero. Exact inputs are
projected at guarded working precision. No correctly-rounded guarantee or
symbolic identity simplification is provided. Multivalued powers and algebraic
identities across branch cuts are not implied.

`ln(z)` accepts complex arguments and automatically promotes negative real input and returns the approximate principal
natural logarithm ln(|z|)+i*arg(z), in radians regardless of RAD/DEG.
Its imaginary component lies in (-pi, pi]; negative real values use +pi,
with a -pi limit below the branch cut. Zero is invalid. For example,
Both `ln(-1)` and `ln(-1+0i)` display approximately `3.1415926536*i`. Exact rational components are projected to decimals at guarded
working precision.

`log(z)` uses base 10; `log(z;b)` uses ln(z)/ln(b) with the same principal
branches and radians when either argument is complex or negative real. For example `log(i)`
is approximately `0.6821881769*i`, `log(-1+0i;i)=2`, and `log(-i;i)=-1`.
Zero input and bases zero or one are invalid. Negative real and nonreal bases
are allowed, with automatic promotion. Positive real input/base keep real
results and their existing real numerical policy. Results remain approximate complex values even if
they display a real number. Bases close to one amplify numerical errors;
principal log and power are not general inverse identities across branch cuts.
`sin(z)`, `cos(z)` and `tan(z)` accept complex inputs in radians, independently
of RAD/DEG. For example `sin(i)` is approximately `1.1752011936*i`,
`cos(i)` is `1.5430806348` and `tan(i)` is `0.761594156*i`.
Complex-typed zero-imaginary inputs also use radians; `sin(90)` in DEG is one,
while `sin(90+0i)` computes radians. Real-only calls keep existing RAD/DEG and
pole rules. `tan` is the guarded quotient sin(z)/cos(z); near real-axis poles
errors can be amplified. Finite decimal pi approximations are not symbolic
poles, and tiny residuals remain. Large imaginary parts can exhaust scale or
runtime limits, even when the tangent itself is bounded. Exact components
project at guarded precision; results stay approximate complex values.
`sinh(z)`, `cosh(z)` and `tanh(z)` also accept complex inputs; the imaginary
angle is always in radians, independently of RAD/DEG. For example `sinh(i)`
is approximately `0.8414709848*i`, `cosh(i)` is `0.5403023059` and
`tanh(1+i)` is `1.0839233273 + 0.2717525853*i`. Results retain approximate
complex types, including zero imaginary components. Real-only calls keep
their existing scalar behavior. Sinh/cosh grow with large real components.
Tanh uses a scaled decaying exponential for |re|>0.5 and a guarded quotient
for smaller real parts; tiny imaginary tails are preserved and may hit scale
limits. Near imaginary-axis poles, errors amplify; finite decimal pi inputs
are not symbolic poles.
`asin(z)`, `acos(z)`, `atan(z)` and their arc/arcus aliases accept complex
inputs and return approximate principal branches in radians, even in DEG.
Asin/acos outside [-1,1] promote automatically. For example asin(i)≈0.881373587*i,
asin(2+0i)≈1.5707963268-1.3169578969*i and acos(2+0i)≈1.3169578969*i.
Atan(±i) is invalid. On the imaginary-axis cuts atan(2i)≈pi/2+0.5493061443*i
and atan(-2i)≈-pi/2-0.5493061443*i. Failed commits preserve ans, variables
and history. See [BigComplex](BIGCOMPLEX.md#principal-inverse-trigonometry)
for cut conventions, working precision and numerical limitations.

The web form selector changes display between Cartesian, trigonometric and
exponential form without changing the stored value. Polar angles are always
radians, independently of RAD/DEG for real trigonometric calls; see
[BigComplex](BIGCOMPLEX.md) for approximation and branch rules. Auto/fraction
Cartesian display preserves exact rational components. Copying a complex result
uses `complex(re;im)` with stored Cartesian components, independently of display.

The calculator is currently an application layer, not a public C header. It
accepts decimal numbers with `.` or `,` as the decimal separator, optional
uppercase-`E` scientific exponent notation, `π`, `e`, and `φ` constants,
whitespace, parentheses, unary `+`/`-`, postfix `²`, `³`, and `!`, explicit or
implicit multiplication, and binary `+`, `-`, `*`, `/`, `^`.

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

Examples: `0.1 + 0.2`, `π / 2`, `πe`, `10π`, `2(3 + 4)`,
`-(2.5E-1) * 8`, `(12.5 - 2.5) / 4`, `1.5^3`, `12²`, `2³`, and `5!`. Each
constant is prepared at the calculator's working precision, using the stored
500-place value for ordinary requests and dynamic calculation above it.
Standalone lowercase `e` means
Euler's constant, so `5e`
means `5 * e` and `1e3` means `1 * e * 3`. Scientific notation always uses
uppercase `E`: `5E-1` means `0.5` and `1E3` means `1000`. Powers use binary
exponentiation with exact BigDecimal multiplication, so decimal bases are valid
when the exponent is a whole number. The typed calculator keeps rational bases
and integer exponents exact, including negative exponents; an approximate base
uses a reciprocal at working precision. `2^3^2` means
`2^(3^2)`; `0^0` is `1`, and zero to a negative exponent is a division-by-zero
error. Decimal exponents are not implemented. Session variables are described in
[Variables](../guides/VARIABLES.md). Integer functions
use the public BigInt core. Exact rational powers and proven roots use BigInt
and BigRational operations; approximate roots use the public BigDecimal core.

The calculator retains exact BigInt or canonical BigRational values for
arithmetic trees made of decimal literals, `ans`, unary signs and `+`, `-`,
`*`, `/`, integer powers and postfix square/cube/factorial. Decimal literals,
including comma input, are converted exactly; `0.1+0.2` is internally `3/10`,
and a denominator of one is stored as BigInt. `sqrt`, `cbrt` and `root` keep a
fraction when both numerator and denominator have proven integer roots, so
`sqrt(4/9)` is internally `2/3`.

Exact typed calls also cover `pow`, `abs`, `sign`, `min`, `max`, `sum`, `product`,
`mean`, `median`, `geomean`, `harmean`, `variance`, `stdevp`, `stdev`,
`floor`, `ceil`, `trunc`, `round`, `factorial`, `isqrt`, `gcd`,
`lcm`, `mod`, `npr` and `ncr` when
their arguments are exact and satisfy the existing domains. Irrational roots,
constants and other functions use the decimal path. Exact subtrees are converted
at the current working precision where that path needs them. Auto output uses
a reduced fraction when its denominator is at most 10000, its text is at most
16 characters including sign and slash, and it is at least two characters
shorter than the rounded decimal display. Fraction mode shows exact non-integers
as `a/b` up to 16 characters; longer fractions and approximate results use Auto
decimal notation. Exact integers have no `/1`. Decimal output alone does not prove
mathematical exactness.
When the local HTTP result is a displayed exact fraction, its optional
`approx` field contains a separate half-even decimal hint with up to 10
decimal places. It uses scientific notation when a very small nonzero value
would otherwise round to zero. The browser shows this beneath the fraction;
copying and history still use only the exact `result`.

Repeated decimal separators (`1.2.3`, `1,2,3`) and adjacent numeric tokens
(`2 3`, `2 .3`) are errors. Delimited products like `(2)3`, `3!2` and `2²3`
remain valid. Implicit multiplication shares the left-associative precedence
of `*` and `/`, so `6/2(1+2)` is `9`.
Factorial uses `bigint_factorial` and requires a non-negative whole number no
greater than 100000, matching the underlying BigInt API.
Non-terminating division defaults to 34 significant digits with half-even
rounding. The complete CLI/HTTP calculation has a five-second monotonic time
budget, including parsing and output formatting. Expensive BigInt parsing,
multiplication, division and decimal conversion loops check cancellation too.
Exceeding the deadline returns `CALCULATOR_TIME_LIMIT` (`TLE`). This is
cooperative cancellation, not an OS-enforced hard real-time deadline.

Application limits are 64 MiB of cumulative allocation requests per calculation,
512 KiB per allocation, 4096 UTF-8 input bytes, 65536 output bytes, and 10000
selected output places. These byte limits are not character counts.
Freed allocations still count toward the cumulative work budget; it is not a
measurement of process RSS. Resource limits return `value too large`, not TLE.
These limits do not change unrestricted public BigInt/BigDecimal calls.
An allowed factorial input (including 100000) may still exceed the time or
memory budget; the input limit is not a completion guarantee.
For example, 100000! has 456574 decimal digits: use scientific/auto notation
with finite output precision. Its complete plain/fraction output exceeds the
65536-byte display limit. Full precision does not remove resource limits.

Parser and AST depth are limited to 256 levels, with at most 256 arguments per call. Inputs that exceed the limit
return `CALCULATOR_VALUE_TOO_LARGE` instead of risking process stack overflow.

### Named calls

Names contain lowercase ASCII letters only and require parentheses. Arguments
use semicolons, not commas: `pow(1,5;2)` is `2.25`. `/api/functions` reports
the current registry; recognition is separate from numerical implementation:

| Calls | Current calculation support |
| --- | --- |
| `complex(re;im)`, `re(z)`, `im(z)`, `conj(z)`, `arg(z)` | Typed complex construction, scalar components, exact conjugation and approximate principal argument in radians. See the complex rules above. |
| `pow(x;y)`, `factorial(n)` | Active aliases of `x^y` and `n!`, with identical domains and limits. |
| `abs(x)`, `sign(x)`, `min(a;b;…)`, `max(a;b;…)` | Active: absolute value, sign −1/0/1 and minimum/maximum of at least two arguments. |
| `rand()`, `rand(x)`, `rand(x;y)` | Random decimal in `[0,1)`, `[0,x)` for `x > 0`, or `[x,y)` for `x < y`. Each occurrence draws independently. |
| `sum(a;b;…)`, `product(a;b;…)`, `mean(a;b;…)` | One to 256 decimal arguments. Sum and product are exact. Mean divides the exact sum by the count, preserving a terminating decimal or rounding a recurring result to working precision. |
| `median(a;b;…)`, `geomean(a;b;…)`, `harmean(a;b;…)` | Exact median; geometric mean of non-negative values; harmonic mean of positive nonzero values. One to 256 arguments. |
| `variance(a;b;…)`, `stdevp(a;b;…)`, `stdev(a;b;…)` | Population variance and standard deviation use denominator `n` and accept at least one value. Sample `stdev` uses `n−1` and requires at least two values. |
| `floor(x)`, `ceil(x)`, `trunc(x)`, `round(x)`, `round(x;n)` | Active decimal rounding. `round` uses half-even, defaults to zero places, and accepts a signed integer n; negative n selects tens, hundreds, and larger powers. |
| `gcd(a;b)`, `lcm(a;b)` | Integer arguments; non-negative GCD/LCM. `gcd(0;0) = 0`; LCM is zero if either argument is zero. |
| `mod(a;b)` | Integer remainder after division truncating toward zero; nonzero remainder has the dividend's sign. `mod(-7;3) = -1`; zero divisor is an error. |
| `npr(n;r)`, `ncr(n;r)` | Exact permutations and combinations without repetition. Both arguments are integers and require `0 ≤ r ≤ n`; `npr(5;2) = 20`, `ncr(5;2) = 10`. |
| `isqrt(n)` | Floor of the square root of a non-negative integer: `isqrt(15) = 3`. |
| `sqrt(x)`, `cbrt(x)`, `root(x;n)` | Active real roots; `√(x)` aliases `sqrt(x)`. Negative dimensionless real inputs to `sqrt` are promoted to principal complex roots; nonnegative real inputs stay real. Cube roots accept negative real x. `root` accepts integer n from 1 to 10000, and negative x with odd n stays real; even n promotes to the principal complex root. |
| `exp(x)`, `ln(x)`, `log(x)`, `log(x;b)` | Active. `exp` and `ln` accept explicit complex arguments with the rules above. Negative real arguments/bases promote automatically; zero input and bases 0 or 1 remain invalid. `ln` uses base e, one-argument `log` uses base 10. Explicit complex `log` follows the principal-base rules above. |
| `sin(x)`, `cos(x)`, `tan(x)`, `asin(x)`, `acos(x)`, `atan(x)` | Active. Real inputs/results use RAD/DEG. Complex inputs/results always use radians, including zero imaginary parts; inverse calls select principal branches. Real `asin`/`acos` outside `[-1,1]` promote automatically; complex `atan(±i)` fails. Exact degree poles such as `tan(90)` are rejected in DEG mode. |
| `sinh(x)`, `cosh(x)`, `tanh(x)`, `asinh(x)`, `acosh(x)`, `atanh(x)` | Active and independent of RAD/DEG. All accept complex inputs. `acosh(x<1)` and `atanh(|x|>1)` promote automatically; `atanh(±1)` remains invalid. |
| `radians(x)`, `degrees(x)` | Active explicit conversions, independent of the selected angle mode. |

Each inverse trigonometric or hyperbolic calculator call also accepts both
`arc` and `arcus` prefixes: `asin` → `arcsin`/`arcussin`, `acos` →
`arccos`/`arcuscos`, `atan` → `arctan`/`arcustan`, and likewise
`asinh`/`acosh`/`atanh` → `arcsinh`/`arccosh`/`arctanh` and
`arcussinh`/`arcuscosh`/`arcustanh`. These are parser aliases for the
same calculator operations, arity, domain and angle mode.

Wrong arity returns `wrong number of arguments` at the function name; unknown
names return `invalid token`. Nesting and implicit products work, for
example `pow(2;factorial(3))` and `2pow(2;3)`.

`rand()` draws uniformly from the 34-place decimal grid `0/10^34` through
`(10^34-1)/10^34`. The ranged forms apply exact decimal multiplication and
addition to this draw. They are discrete, not a continuous distribution, and
are not cryptographically secure. The internal value is below the upper bound;
rounding the displayed result to fewer places can show that bound. Output
precision does not change the draw. Each call in one expression advances the
private calculator generator independently, in evaluation order.

Selection calls accept all finite decimal values without introducing rounding;
their arguments follow the normal working-precision policy. Min/max evaluate
every argument left to right and propagate all errors. Ties retain the first
value. Negative zero has sign 0. Named rounding calls deliberately change the
value but do not depend on the calculator's display precision.

Roots first preserve exact finite decimal results, even when they exceed working
precision. Other roots are rounded to max(34, N+4) significant digits with the
context rounding mode (normally half-even), then formatted at the requested
output precision. Full output uses 34 working digits for irrational roots; it
does not mean infinite precision. `sqrt(2)` displays `1.4142135624` by default,
`cbrt(-8)` is exactly `-2`, and `root(32;5)` is exactly `2`. Zero/negative/fractional
degrees are invalid; degrees above 10000 are too large. Time and memory limits
still apply, particularly to high degree/high precision combinations. Like
division, rounded roots can introduce small errors in subsequent arithmetic.

The tokenizer reads complete letter sequences: `exp` is one name, whereas
`1e3` remains `1*e*3`, `πe` remains `π*e`, `e(2)` remains `e*2`, and `1E3`
is 1000. Separate adjacent ASCII names with `*`: `ee` and `esin` are single
names, not products; sessions may define them as variables. Numeric suffixes
such as `log2` are not supported.


See [Variables](../guides/VARIABLES.md), [Quantity expressions](QUANTITIES.md) and
[Unit conversion](UNITS.md) for state and dimensional arithmetic.
