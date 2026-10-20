# BigInt design

`BigInt` is NumForge's signed arbitrary-precision integer foundation. Its
public API is declared in `include/numforge/bigint.h`; this document records
the internal representation, semantic decisions, and maintenance boundaries.

The public API also includes `bigint_isqrt`, which computes the floor square
root without floating-point conversion and preserves the destination on error.

## Source layout

| File | Responsibility |
| --- | --- |
| `bigint.c` | Representation helpers, magnitude primitives, status text, and object lifecycle. |
| `conversion.c` | Parsing and decimal string conversion. |
| `comparison.c` | Signed comparison and basic value predicates. |
| `arithmetic.c` | Sign operations, arithmetic, division, modulo, and integer powers. |
| `multiplication.c` | Private bounded-scratch Karatsuba kernel and workspace preflight. |
| `factorial.c` | Budget-admitted product tree and per-call multiplication workspace ownership. |
| `normalization.c` | Exact inverse division for decimal-zero stripping in private coefficients. |
| `bitwise.c` | Bitwise operations and shifts. |
| `number_theory.c` | GCD, LCM, factorial, permutations, and combinations. |
| `primality.c` | Parity, Miller-Rabin primality testing, and perfect-square detection. |
| `roots.c` | Floor integer square root. |
| `bigint_internal.h` | Private representation and declarations shared only by these modules. |

The installed public API remains in `include/numforge/bigint.h`. Helpers from
`bigint_internal.h` are implementation details and are not consumer API.

## Representation and invariants

The magnitude is stored as a little-endian array of base-2^64 limbs:

```text
value = sign × sum(limbs[i] × 2^(64 × i))
```

The private `BigInt` structure maintains these invariants:

- `limbs` contains the absolute magnitude only;
- `size` is the number of significant limbs;
- if `size > 0`, the most significant limb is non-zero;
- `size == 0` represents zero;
- zero is never negative;
- `capacity >= size` and owns the limb allocation.

`bigint_normalize()` is the single source of truth for removing high zero
limbs and clearing a zero sign. Public code must never create a negative zero.

## Ownership and failure safety

`bigint_create()` returns an owned zero value and `bigint_destroy()` releases
it; destroying `NULL` is safe. `bigint_to_string()` allocates a C string owned
by the caller, which must release it with `free()`.

Mutating operations provide a strong failure guarantee: when they return an
error, their destination is unchanged. Operations that need temporary storage
compute into fresh buffers or temporary objects and commit only after success.
The test-enabled build routes allocations through an internal deterministic
fault injector. `allocation_failure_tests` fails every allocation site in each
covered operation and verifies both `BIGINT_OUT_OF_MEMORY` propagation and the
unchanged destination. Linux CI combines this with AddressSanitizer to detect
leaks in failure cleanup.

The private allocation boundary also supports an optional thread-local
calculator budget in production. Parsing, multiplication, division and decimal
conversion check it inside their expensive loops. Cancellation unwinds through
the existing allocation-failure paths, preserving destinations; the calculator
translates its recorded reason into time-limit or resource-limit status.
No budget is active for ordinary public numeric calls. The public API and
standard `free()` ownership contract are unchanged.

Arithmetic supports output/input aliasing unless documented otherwise:

```c
bigint_add(value, value, other);
bigint_mul(value, value, value);
bigint_div_mod(quotient, remainder, quotient, remainder);
```

The only exception is `bigint_div_mod`: quotient and remainder must be
different objects.

## Decimal conversion

Parsing accepts an optional sign followed by decimal digits. It parses into a
temporary value, so malformed input and allocation failures preserve the old
destination.

Formatting and parsing use chunks of up to 19 decimal digits because 10^19
fits in `uint64_t`. This reduces the number of full-magnitude operations
compared with processing one decimal digit at a time.

Formatting repeatedly divides a scratch magnitude by `10^19`. On GCC/Clang
targets with `__int128` and MSVC x64, its 128-by-64 primitive uses the native
compiler operation. Other targets, including the supported 32-bit build, retain
the checked bit-serial fallback. Both paths have the same `high < divisor`
precondition and quotient/remainder semantics; public conversion is unchanged.

## Arithmetic semantics

- Addition, subtraction, multiplication, division, and modulo are signed.
- Division truncates toward zero.
- Magnitude division handles zero, smaller and equal operands directly. A
  single-limb divisor uses successive 128-by-64-bit divisions. Larger divisors
  use normalized radix-2^64 long division, quotient-estimate correction and
  add-back when needed. Portable builds retain the scalar bit-serial 128-by-64
  helper; the complete large-number division no longer runs bit by bit.
- Division reads its inputs without duplicating them, writes fresh quotient
  and remainder temporaries and commits only after all reads succeed. Budget
  checkpoints cover normalization, quotient digits and blocks of inner work.
- The modulo result has the dividend's sign, matching C `%` semantics.
- `0^0` is defined as `1`.
- Powers reject negative exponents.
- Factorial accepts `0 <= n <= BIGINT_FACTORIAL_MAX_N`; this is the `UINT64_MAX`
  counter representation bound. Runtime budgets govern practical work. From
  8192 a conservative allocation preflight selects a balanced product tree with
  reusable per-depth Karatsuba scratch. Smaller or constrained requests retain
  grouped scalar multiplication.
  Admission sums integer factor bit lengths over power-of-two ranges and plans
  scratch with the shared allocation capacity rule.
  Factorial scratch at each depth uses separate equal sum/cross buffers to
  reduce the largest request while retaining combined capacity and growth;
  see [split-workspace verification](../performance/baselines/FACTORIAL.md).
  In both paths consecutive leaf factors are packed into a `uint64_t`
  product, checking `product > UINT64_MAX / factor` before multiplying. Each
  completed group multiplies a private BigInt accumulator once. The budget is
  checked for every factor, every 256 limbs of grouped multiplication and before
  publishing. The counter exits before overflowing; only a successful final
  value is committed.
- Permutations and combinations require `0 <= r <= n`. Permutations use the
  falling product directly. Combinations use symmetry and exact division after
  each multiplication, avoiding full factorial intermediates.
- GCD is non-negative; LCM is computed as `(|a| / gcd(a, b)) × |b|` to reduce
  intermediate growth.

GCD reuses private remainder and normalized-division buffers across Euclidean
steps. It computes only the remainder and rotates owned magnitudes rather than
allocating an unused quotient on every step. Cleanup and cancellation preserve
the original destination, including aliases. The installed API is unchanged.

## Bitwise operations

AND, OR, and XOR currently accept only non-negative operands. This avoids
pretending that a sign-magnitude representation has an implicit infinite
two's-complement width. `bigint_not` is defined algebraically for every value
as `-(a + 1)`. Shifts preserve the documented truncation-toward-zero behavior.

## Primality and squares

Primality uses Miller-Rabin witnesses that are deterministic for values fitting
in `uint64_t`; for larger values the result is a strong probable-prime result,
not a certificate. Perfect-square detection builds an integer square root bit
by bit and verifies the resulting square. Both public checks return a status
separately from their boolean output, preserving the output on allocation or
argument failure.

## Performance boundaries

Multiplication selects a private Karatsuba kernel when the smaller operand has
at least 128 limbs and is greater than half the larger operand. Recursive leaves
use classical multiplication below 32 limbs. A single per-call scratch arena
serves all recursion; no recursive allocations or global scratch are used.
Optional result/scratch allocation sizes are checked against the remaining
thread-local budget before allocation. If the workspace does not fit, the
original classical path remains available. Allocation/deadline failures preserve
the destination and all permitted aliases. The public API and resource limits
are unchanged. See the [integration measurements](../performance/baselines/ARITHMETIC.md).

Future candidates, after profiling, include:

- further scratch reuse in repeated products, after the existing bounded factorial workspace;
- divide-and-conquer conversion if measurements show that full decimal output
  remains a bottleneck after the native single-limb division fast path;
- cached powers used repeatedly by higher-level decimal operations.

These must preserve the representation and public semantic contracts above.


## Explicit numeral bases

`bigint_set_string_base` and `bigint_to_string_base` support bases 2–36 using
ASCII digits and explicit output case. The parser validates the entire input
before computing into a temporary. Parsing uses maximal uint64 radix chunks;
formatting repeatedly divides a scratch magnitude by the corresponding chunk
power, pads internal chunks, then reverses the digits. Overflow checks bound
buffer sizes; budgets are checked between chunks. Existing decimal entry points
retain their optimized 19-digit conversion. See [API](../API.md)
for ownership and syntax. Independent Node BigInt references and injected
allocation failures cover both directions.
