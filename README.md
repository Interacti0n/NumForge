# NumForge

NumForge is a C17 mathematics library. Its first stable component is
`BigInt`: a signed, arbitrary-precision integer type with decimal conversion,
arithmetic, bit operations, number-theory helpers, and a CMake build.

The `calculator` executable is currently a small benchmark/demo rather than
an interactive calculator. `BigDecimal` provides exact base-10 parsing,
formatting, arithmetic, rescaling, and rounded division on top of `BigInt`.

## Features

- Signed arbitrary-precision integers stored internally in base 2<sup>64</sup>.
- Decimal parsing and formatting.
- Addition, subtraction, multiplication, division, modulo, powers, GCD, LCM,
  and factorial.
- Bitwise operations and shifts.
- Perfect-square and Miller-Rabin probable-prime checks.
- Exact decimal arithmetic with configurable rounding for division and
  rescaling.
- Output/input aliasing for arithmetic operations, including
  `bigint_add(x, x, y)` and `bigint_div_mod(q, r, q, r)`.
- Unit tests, deterministic property tests, warnings-as-errors, and Linux CI.

## Requirements

- CMake 3.20 or later
- A compiler with C17 support
- Internet access on the first test-enabled CMake configure, so CMake can
  download the Unity test framework

## Build

### Build the library and demo

```sh
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build --parallel
```

### Build and run tests

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

With a multi-configuration generator such as Visual Studio, specify a
configuration:

```sh
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

For stricter local verification with GCC or Clang:

```sh
cmake -S . -B build -DBUILD_TESTING=ON \
  -DNUMFORGE_WARNINGS_AS_ERRORS=ON \
  -DNUMFORGE_ENABLE_SANITIZERS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`NUMFORGE_ENABLE_SANITIZERS` enables AddressSanitizer and
UndefinedBehaviorSanitizer on GCC and Clang.

## Using BigInt

Include the public header and link the `numforge` CMake target:

```c
#include <numforge/bigint.h>
```

Every `BigInt` is opaque and must be created with `bigint_create()` and
destroyed with `bigint_destroy()`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <numforge/bigint.h>

int main(void)
{
    BigInt *a = bigint_create();
    BigInt *b = bigint_create();
    BigInt *quotient = bigint_create();
    BigInt *remainder = bigint_create();

    if (a == NULL || b == NULL || quotient == NULL || remainder == NULL)
    {
        bigint_destroy(a);
        bigint_destroy(b);
        bigint_destroy(quotient);
        bigint_destroy(remainder);
        return 1;
    }

    BigIntStatus status = bigint_set_string(a, "123456789012345678901234567890");
    if (status == BIGINT_OK)
    {
        status = bigint_set_string(b, "97");
    }
    if (status == BIGINT_OK)
    {
        status = bigint_div_mod(quotient, remainder, a, b);
    }

    if (status == BIGINT_OK)
    {
        char *q = bigint_to_string(quotient);
        char *r = bigint_to_string(remainder);

        if (q != NULL && r != NULL)
        {
            printf("quotient = %s, remainder = %s\n", q, r);
        }

        free(q);
        free(r);
    }

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(quotient);
    bigint_destroy(remainder);
    return status == BIGINT_OK ? 0 : 1;
}
```

`bigint_to_string()` returns a heap-allocated string; release it with
`free()`. `bigint_set_string()` accepts an optional leading `+` or `-`, rejects
empty and non-numeric strings, and leaves its destination unchanged on error.

## Error handling and API rules

Most mutating operations return `BigIntStatus`:

| Status | Meaning |
| --- | --- |
| `BIGINT_OK` | The operation completed successfully. |
| `BIGINT_NULL_ARGUMENT` | A required pointer was `NULL`. |
| `BIGINT_OUT_OF_MEMORY` | An allocation failed or a required size overflowed `size_t`. |
| `BIGINT_DIVISION_BY_ZERO` | The divisor was zero. |
| `BIGINT_INVALID_ARGUMENT` | An argument is invalid, including identical quotient and remainder outputs. |
| `BIGINT_NEGATIVE_ARGUMENT` | The operation requires a non-negative input. |
| `BIGINT_VALUE_TOO_LARGE` | The input exceeds the supported practical range of an operation. |

`bigint_compare(a, b)` requires two non-`NULL` arguments. The predicate
functions (`bigint_is_zero`, `bigint_is_even`, and similar) return `false` for
`NULL`, except that no result should be inferred from a `NULL` value.

Unless stated otherwise, operations support output/input aliasing. The one
exception is `bigint_div_mod`: `quotient` and `remainder` must be different
objects. On that error, neither output is changed.

## Arithmetic semantics

- Division truncates toward zero.
- The remainder has the same sign as the dividend, matching C integer `%`
  semantics. For example, `-100 % 7 == -2`.
- `0^0` is defined as `1`.
- `bigint_pow` rejects negative exponents.
- `bigint_factorial` accepts `0 <= n <= BIGINT_FACTORIAL_MAX_N`.
- `bigint_and`, `bigint_or`, and `bigint_xor` accept only non-negative
  operands. `bigint_not` is defined for all values as `-(a + 1)`.
- Right shifts truncate toward zero, including for negative values.

`bigint_is_probable_prime()` is deterministic for values that fit in
`uint64_t`. For larger values it is a Miller-Rabin probable-prime test and is
not a cryptographic primality certificate.

## Public API overview

| Area | Functions |
| --- | --- |
| Lifetime and conversion | `bigint_create`, `bigint_destroy`, `bigint_copy`, `bigint_set_string`, `bigint_to_string` |
| Inspection | `bigint_compare`, `bigint_is_zero`, `bigint_is_one`, `bigint_is_negative`, `bigint_is_even`, `bigint_is_odd` |
| Arithmetic | `bigint_abs`, `bigint_negate`, `bigint_add`, `bigint_sub`, `bigint_mul`, `bigint_div`, `bigint_mod`, `bigint_div_mod`, `bigint_pow` |
| Number theory | `bigint_gcd`, `bigint_lcm`, `bigint_factorial`, `bigint_is_probable_prime`, `bigint_is_perfect_square` |
| Bits | `bigint_and`, `bigint_or`, `bigint_xor`, `bigint_not`, `bigint_shift_left`, `bigint_shift_right` |

See [the public header](include/numforge/bigint.h) for function
signatures and detailed per-function constraints. See
[the BigInt design](docs/BIGINT_DESIGN.md) for its representation,
semantic decisions, and optimization boundaries.

## Testing

The repository contains two independent test executables:

- `bigint_tests`: focused unit and regression tests.
- `bigint_property_tests`: deterministic generated tests of algebraic
  identities, multi-limb boundaries, and aliasing behavior.
- `bigdecimal_tests`: covers parsing, canonical form, exact arithmetic,
  aliasing, division, and rounding modes.

Both run through CTest when `BUILD_TESTING=ON`. GitHub Actions builds and runs
them on Linux with warnings treated as errors and sanitizers enabled.

## Project status and roadmap

`BigInt` is the current foundation. Remaining work is mostly performance
optimization for very large operands and additional test vectors. Planned work
includes:

1. Extend `BigDecimal` with property tests, larger generated decimal vectors,
   and performance optimizations. Its representation and implementation notes are in
   [the BigDecimal design](docs/BIGDECIMAL_DESIGN.md).
2. Tokenization, parsing, and AST evaluation for calculator expressions. Its
   module boundaries, grammar, and evaluation policy are in
   [the calculator design](docs/CALCULATOR_DESIGN.md).
3. An interactive calculator CLI with source-positioned diagnostics.

The public API is still pre-1.0 and may evolve.
