# BigDecimal design

Public API: `include/numforge/bigdecimal.h`; implementation:
`src/core/bigdecimal/`. Values use BigInt, never binary floating point.

## Source layout

The source manifest separates lifecycle/canonicalization, conversion/comparison,
arithmetic/aggregation/statistics, division, roots, transcendental/logarithm,
hyperbolic/trigonometric functions, constants and formatting. Workspace and
representation helpers stay private in the numeric library.

## Current API

[Public C API](../reference/C_API.md#additional-numeric-operations) owns the
operation inventory. Mutations compute privately and commit only after success.
Aligned operands are borrowed; comparison avoids text conversion. Rounding,
division and series reuse per-call buffers/owners, never global scratch.
Significant-digit counting uses bit bounds or 19-digit magnitude chunks;
division aligns coefficients exactly before rounding.

Private sin/cos and sinh/cosh pairs share argument reduction/exponential work.
Both outputs publish atomically, must differ and may alias the input. They retain
scalar guard/rounding rules and expose no installed paired API/cache. Near zero,
sinh keeps its cancellation-safe series.

## Representation

`value = coefficient * 10^(-scale)`: owned BigInt coefficient and signed int64
scale. Positive scale represents fractional places; negative scale represents
whole-number zeros. Consumers see only the opaque type.

## Canonical form

The coefficient is nonnull; zero is exactly coefficient/scale zero. A nonzero
coefficient is not divisible by ten. Successful parsing/arithmetic/rounding
normalize once: `1.2300` becomes coefficient 123, scale 2.

## Arithmetic rules

### Parsing and formatting

Parsing accepts sign, point and optional e/E exponent, but no whitespace.
`bigdecimal_to_string` gives ordinary decimal text. Separate result formatting
supports scientific/prefix output without changing values; conservative
log10(2) bounds skip unnecessary conversions, with exact fallback and a second
notation check after rounding carries.

### Addition and subtraction

Align to the larger scale, then add/subtract coefficients. Unrepresentable scale
differences return VALUE_TOO_LARGE. Equal-scale operands can be borrowed until
commit, including aliases.

### Multiplication

Normalize the coefficient product before checked scale addition; cross-factor
zeros can rescue an intermediate overflow. Only final overflow fails. Zero has
scale zero. Normalization uses inverse division in 27-zero blocks from 128 limbs
when scratch fits; small/constrained cases retain up to 19-zero blocks. Binary
divisibility/modulo-five checks reject already normalized coefficients without
allocation; checkpoints cover scans.

### Division

Scale/significant precision and rounding are explicit. Modes: toward/away from
zero, floor, ceiling, half-up and half-even. Rounded success returns OK; it does
not itself report discarded information. Exact-first division preserves finite
results; otherwise it follows the requested policy. No mutable global precision.

## Error model

Statuses cover null argument, allocation, invalid input, division by zero,
value size and scale overflow. Failures preserve destinations. Root/degree and
scale arithmetic are checked; application limits do not redefine numeric APIs.

## Testing and future work

For canonical `C*10^-s`, a finite kth root exists iff s is divisible by k and
abs(C) is a perfect kth power. Otherwise exponent normalization and integer
Newton iteration produce a guarded floor root; a sticky tail supports six
rounding modes.

Approximate algorithms retain these choices:

- exp: halve to `[-0.5,0.5]`, Taylor series with 24 guard digits, then square.
- ln fallback: repeated roots into `[0.9,1.1]`, `2*atanh((x-1)/(x+1))`, restore
  powers of two. Suitable 2000–10000-digit calls use bounded theta/AGM interval
  work with tail/pi enclosures and adaptive endpoint-rounding agreement.
- Trigonometry: radians; π reduction includes requested precision, 24 guard
  digits and integer magnitude. atan uses reciprocal/half-angle reduction;
  asin/acos preserve endpoint distance through `(1-x)*(1+x)` and complementary
  formulas. tan divides the paired sin/cos values.
- Hyperbolics: cancellation-safe series near zero; stable exponential ratios
  elsewhere. Inverses use guarded roots/logs; acosh requires x≥1, atanh −1<x<1.
- Constants: legacy factory retains 500-place values. Precision-aware calls use
  stored values through 500 significant digits, then Gauss–Legendre π,
  exp(1) and `(1+sqrt(5))/2`, with 24 guards and no shared mutable cache.

Intermediates use half-even; final rounding uses the caller's mode. General
composition remains approximate, without universal correct-rounding guarantees.
The fixed-guard AGM prototype stays benchmark-only.

Property/oracle tests cover canonical form, precision, aliases, domains and
high precision; fault tests cover unchanged outputs and cleanup. Future work:
lower interval-allocation traffic and stronger enclosure-based rounding.
[Logarithm evidence](../performance/baselines/LOGARITHM.md).

## BigInt boundary

Private numeric helpers supply normalization, coefficient projection, bit/digit
counts, small-factor arithmetic and division scratch. `from_bigint` copies
coefficients directly, without text round trips. Both layouts stay opaque;
calculator/web code never includes numeric internal headers.
