#ifndef NUMFORGE_BIGINT_INTERNAL_H
#define NUMFORGE_BIGINT_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <numforge/bigint.h>

#define BIGINT_LIMB_BITS 64U
#define BIGINT_MAX_LIMBS (SIZE_MAX / BIGINT_LIMB_BITS)

/*
------------------------------------------------------------------------------------------------------------------------------
    BigInt representation invariants:

    - limbs store the absolute value only.
    - limbs use base 2^64 in little-endian order.
    - size is the number of significant limbs.
    - if size > 0, limbs[size - 1] != 0.
    - size == 0 represents zero.
    - zero is never negative.
    - is_negative applies only to non-zero values.

    Internal magnitude helpers do not modify is_negative. Public arithmetic
    operations are responsible for signed semantics.
------------------------------------------------------------------------------------------------------------------------------
*/

struct BigInt
{
    uint64_t *limbs;
    size_t size;
    size_t capacity;
    bool is_negative;
};

/* Private normalization of a temporary magnitude, in blocks of up to 19
 * decimal zeros. On cancellation the temporary may be partially reduced. */
BigIntStatus bigint_strip_decimal_zeros(BigInt *value, uint64_t *removed);

/* Shared implementation helpers used by the split BigInt modules.
 * Implementation: src/bigint/bigint.c. */
BigIntStatus bigint_size_add(size_t a, size_t b, size_t *out);
BigIntStatus bigint_size_mul(size_t a, size_t b, size_t *out);
void bigint_normalize(BigInt *value);
int bigint_wrap_uint64(BigInt *value, uint64_t *storage, uint64_t number);
void bigint_commit(BigInt *destination, BigInt *temporary);
BigIntStatus bigint_set_uint64(BigInt *value, uint64_t number);
BigIntStatus bigint_multiply_by_uint64(BigInt *value, uint64_t multiplier);
BigIntStatus bigint_add_uint64(BigInt *value, uint64_t addend);
uint64_t bigint_divide_by_uint64(BigInt *value, uint64_t divisor);
int bigint_compare_abs(const BigInt *a, const BigInt *b);
BigIntStatus bigint_add_abs(BigInt *result, const BigInt *a, const BigInt *b);
BigIntStatus bigint_subtract_abs(BigInt *result, const BigInt *a, const BigInt *b);
void bigint_multiply_u64_u64(uint64_t a, uint64_t b, uint64_t *high, uint64_t *low);
/* Private atomic Karatsuba kernel; threshold is a recursive leaf cutoff.
 * Dispatch checks optional workspace against the active resource budget. */
BigIntStatus bigint_mul_karatsuba(BigInt *out, const BigInt *a, const BigInt *b, size_t threshold);
bool bigint_karatsuba_fits(size_t na, size_t nb, size_t threshold);
/* Owned scratch for repeated magnitude division. Zero-initialize and free both
 * buffers after use. Output must not alias inputs/scratch and may change on
 * failure; callers publish only completed results. The divisor must be nonzero. */
typedef struct BigIntDivisionWorkspace
{
    BigInt numerator;
    BigInt divisor;
} BigIntDivisionWorkspace;
BigIntStatus bigint_divmod_abs_workspace(BigInt *quotient, BigInt *remainder,
    const BigInt *a, const BigInt *b, BigIntDivisionWorkspace *workspace);
BigIntStatus bigint_mod_abs_workspace(BigInt *remainder, const BigInt *a, const BigInt *b,
    BigIntDivisionWorkspace *workspace);
BigIntStatus bigint_divmod_abs(BigInt *quotient, BigInt *remainder, const BigInt *a, const BigInt *b);
size_t bigint_bit_length(const BigInt *value);
/* Exact decimal magnitude length; no output serialization, unchanged on error. */
BigIntStatus bigint_decimal_digits(const BigInt *value, size_t *digits);
int bigint_get_bit(const BigInt *value, size_t bit_index);
BigIntStatus bigint_set_bit(BigInt *value, size_t bit_index);

#endif
