# Quantity expressions

`qty(value; "unit")` attaches a catalogue unit to a numeric expression in the
shared calculator parser. It is available through `/api/evaluate`, the existing
web calculator input and CLI. No new buttons or unit-input shorthand are needed.
IDs use the same quoted, case-sensitive grammar as `convert`: `"m"`, `"m2"`,
`"km/h"`, `"degC"`, `"MiB"`. `5m` is still ordinary expression syntax, not a
quantity literal. Unknown units report the position of the opening quote.

```text
qty(5; "m") * qty(5; "m")                 = 25 m²
qty(1; "km") + qty(500; "m")              = 1.5 km
qty(36; "km/h") * qty(10; "s")            = 100 m
qty(1; "m") / qty(100; "cm")              = 1
qty(2; "kg") * qty(3; "m") / qty(2; "s")^2 = 1.5 m*kg*s^-2
sqrt(qty(9; "m2"))                        = 3 m
```

Quantities retain authoritative integer/rational/decimal numeric ownership plus
dimensions, a temperature-point flag and an optional catalogue unit ID. Units
are not appended to the stored numeric string. Session variables, previews,
confirmed history, ans, value copies and cached formatting retain the metadata.
Assigning a scalar later replaces the entire typed value. Preview and failed
calculations do not alter confirmed variables, ans or RNG state.

## Arithmetic and display

Addition/subtraction require matching dimensions and compatible semantics.
The right operand is converted into the left operand's unit, which is retained.
A bare number cannot be added to a quantity, even if it is zero. Scaling by a
number preserves the quantity's unit. Products and quotients of quantities use
canonical metres, kilograms, seconds, temperature intervals in kelvin, bits and
radians. Named common results use m, m², m³ or m/s; other dimensions display as
products with integer exponents. Complete cancellation produces a scalar.
Angles and information remain distinct dimensions, preserving category meaning.

Integer powers, postfix square/cube and `pow` compose dimensions. Each dimension
exponent is bounded to -32…32; dimensional powers must use integer exponents in
that range. `sqrt`, `cbrt` and `root` require all dimension exponents to divide
evenly by a positive root degree (up to 32 for explicit `root`). Fractional
dimensions are not supported. Numerical domains still apply, including division
by zero and even roots of negative values. Cancelled scalars use ordinary
numeric function/exponent rules.

`abs`, `sign`, compatible `min`/`max`/`sum`/`mean`/`median`, and coordinate
rounding (`floor`, `ceil`, `trunc`, `round`) support quantities. `sign` returns
a scalar. `sin`, `cos` and `tan` accept explicit angle quantities and evaluate
them in radians independently of the RAD/DEG selector. Dimensionless numeric
arguments retain existing RAD/DEG behavior. Other functions reject dimensional
arguments; they never silently discard units. Further statistical and integer
function contracts are future extensions.

## Temperature points and intervals

Temperature points (`K`, `degC`, `degF`) are affine coordinates. Intervals
(`deltaK`, `deltaC`, `deltaF`) are linear quantities.

| Operation | Behavior |
| --- | --- |
| Point − point | Interval in the left point's temperature scale |
| Point +/− interval | Point in the point's unit |
| Interval + point | Point in the point's unit |
| Interval +/− interval | Interval in the left unit |
| Point + point; interval − point | Error |
| Multiplication/division/powers/roots of points | Error |

Negative temperature coordinates can be constructed with `qty(-10; "degC")`;
unary negation of a temperature point is rejected. No physical-domain checks
are imposed on coordinates below absolute zero. Min/max/mean/median and
coordinate rounding of compatible temperature points are supported; abs, sign
and sum of points are rejected.

## Conversion, API and limits

`convert` retains its numeric-return contract. With a quantity operand, the
declared source unit must match its dimensions and temperature semantics:
`convert(qty(1; "km"); "m"; "cm") = 100000`. To retain a target unit, wrap the
result: `qty(convert(x; "m"; "cm"); "cm")`. `qty` itself accepts only a scalar
value, preventing accidental relabelling of an existing quantity.

`POST /api/evaluate` continues to return the formatted `result` string, including
the unit for quantities. Authoritative quantity metadata stays in the internal
typed calculator value/session; this does not introduce a public C Quantity ABI
or a machine-readable quantity snapshot endpoint. Display text is not a
round-trip expression format, and copy text may contain display unit symbols.

The dedicated `/api/convert` route still takes a numeric coordinate plus source
and target query units. It rejects a quantity-valued expression with
`quantity_not_allowed`; explicitly use `convert(...)` to extract a numeric
coordinate first. This avoids applying a second, conflicting source unit or
dropping quantity metadata. Its separate snapshot/history contract is unchanged.

Invalid dimensional arithmetic reports `invalid quantity operation` at the
operator or function. Catalogue identity errors keep their existing unit error
positions. Existing input, AST depth, allocation, output and cooperative time
limits apply to the entire expression, including conversion and formatting.
Exact rational coordinates/factors remain exact. Approximate operands and
pi-based angle conversions retain the established working-precision policy;
this is not a certified correctly-rounded API.

General compound unit-string parsing, aliases for derived units such as newton,
direct `5 m` literals, fractional dimensions, quantity snapshots for HTTP,
unit-aware equations and further function contracts remain separate work.
