#ifndef NUMFORGE_DECIMAL_PROFILE_H
#define NUMFORGE_DECIMAL_PROFILE_H

#include <numforge/bigint.h>

/* Separate benchmark-only scopes. The HTTP/formatter phase contract is intact.
 * Exclusive time: nested integer calls are charged to CORE, not ALIGNMENT.
 * Normalization includes coefficient zero stripping. No hooks in normal builds. */
typedef enum NumForgeDecimalPhase {
    NUMFORGE_DECIMAL_OTHER,
    NUMFORGE_DECIMAL_ALIGNMENT,
    NUMFORGE_DECIMAL_NORMALIZE,
    NUMFORGE_DECIMAL_CORE,
    NUMFORGE_DECIMAL_PHASES
} NumForgeDecimalPhase;

#ifdef NUMFORGE_ENABLE_ALLOC_STATS
void numforge_decimal_profile_reset(void);
void numforge_decimal_profile_stop(void);
int numforge_decimal_profile_complete(void);
double numforge_decimal_profile_seconds(NumForgeDecimalPhase phase);
NumForgeDecimalPhase numforge_decimal_enter(NumForgeDecimalPhase phase);
void numforge_decimal_leave(NumForgeDecimalPhase previous);
BigIntStatus numforge_decimal_add(BigInt *, const BigInt *, const BigInt *);
BigIntStatus numforge_decimal_sub(BigInt *, const BigInt *, const BigInt *);
BigIntStatus numforge_decimal_mul(BigInt *, const BigInt *, const BigInt *);
BigIntStatus numforge_decimal_div(BigInt *, const BigInt *, const BigInt *);
BigIntStatus numforge_decimal_mod(BigInt *, const BigInt *, const BigInt *);
BigIntStatus numforge_decimal_pow(BigInt *, const BigInt *, const BigInt *);
BigIntStatus numforge_decimal_gcd(BigInt *, const BigInt *, const BigInt *);
BigIntStatus numforge_decimal_div_mod(BigInt *, BigInt *, const BigInt *, const BigInt *);
#ifndef NUMFORGE_DECIMAL_PROFILE_NO_REDIRECT
#define bigint_add numforge_decimal_add
#define bigint_sub numforge_decimal_sub
#define bigint_mul numforge_decimal_mul
#define bigint_div numforge_decimal_div
#define bigint_mod numforge_decimal_mod
#define bigint_pow numforge_decimal_pow
#define bigint_gcd numforge_decimal_gcd
#define bigint_div_mod numforge_decimal_div_mod
#endif
#else
#define numforge_decimal_enter(phase) ((void)(phase), NUMFORGE_DECIMAL_OTHER)
#define numforge_decimal_leave(previous) ((void)(previous))
#endif
#endif
