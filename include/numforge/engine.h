#ifndef NUMFORGE_ENGINE_H
#define NUMFORGE_ENGINE_H

#include <numforge/bigrationalcomplex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file engine.h
 * @brief Stateless, deterministic expressions over the shared C numeric core.
 * @details Link NumForge::engine. No server, sessions or application headers are
 * required. This initial API supports scalar real/complex expressions in radians.
 * Explicit parameters are supported; sessions, ans, random calls, assignments
 * and quantities are not supported.
 * See docs/reference/ENGINE_API.md for grammar, limits and numerical guarantees.
 */

typedef struct NumForgeContext NumForgeContext;
typedef struct NumForgeExpression NumForgeExpression;
typedef struct NumForgeResult NumForgeResult;

/** Operation outcome; messages are diagnostic text, not stable identifiers. */
typedef enum NumForgeStatus {
    NUMFORGE_OK = 0,
    NUMFORGE_NULL_ARGUMENT,
    NUMFORGE_INVALID_ARGUMENT,
    NUMFORGE_INVALID_TOKEN,
    NUMFORGE_SYNTAX_ERROR,
    NUMFORGE_DIVISION_BY_ZERO,
    NUMFORGE_VALUE_TOO_LARGE,
    NUMFORGE_SCALE_OVERFLOW,
    NUMFORGE_OUT_OF_MEMORY,
    NUMFORGE_TIME_LIMIT,
    NUMFORGE_MEMORY_LIMIT,
    NUMFORGE_UNSUPPORTED,
    NUMFORGE_ARGUMENT_COUNT,
    NUMFORGE_UNDEFINED_VARIABLE
} NumForgeStatus;

/** Diagnostic, including the zero-based UTF-8 byte offset (not a character column).
 * Non-source failures use offset zero. Optional error outputs are cleared on success.
 */
typedef struct NumForgeError {
    NumForgeStatus status;
    size_t offset;
} NumForgeError;

/** Authoritative stored type; decimal kinds conservatively report approximation. */
typedef enum NumForgeResultKind {
    NUMFORGE_RESULT_INVALID = 0,
    NUMFORGE_RESULT_INTEGER,
    NUMFORGE_RESULT_RATIONAL,
    NUMFORGE_RESULT_DECIMAL,
    NUMFORGE_RESULT_COMPLEX_RATIONAL,
    NUMFORGE_RESULT_COMPLEX_DECIMAL
} NumForgeResultKind;

/** Create an owned context, or NULL on allocation failure.
 * Defaults: 34 working significant digits, half-even rounding, radians,
 * 5000 ms, 64 MiB cumulative allocations, 512 KiB per allocation.
 * Contexts are thread-confined. NULL context arguments below select these defaults.
 */
NumForgeContext *numforge_context_create(void);
/** Release a context; NULL is accepted. Results/expressions do not borrow it. */
void numforge_context_destroy(NumForgeContext *context);
/** Set working significant digits (1..10000), preserving exact values.
 * This is not a certified final-expression accuracy or display-digit count.
 * @return OK, NULL_ARGUMENT or INVALID_ARGUMENT; failure preserves the context.
 */
NumForgeStatus numforge_context_set_precision(NumForgeContext *context, int64_t digits);
/** Set one of the six BigDecimal rounding modes. Failure preserves the context.
 * @return OK, NULL_ARGUMENT or INVALID_ARGUMENT.
 */
NumForgeStatus numforge_context_set_rounding(NumForgeContext *context, BigDecimalRoundingMode rounding);
/** Set cooperative operation budgets; zero limits deny work immediately.
 * SIZE_MAX disables either allocation limit. Milliseconds must be <= INT64_MAX.
 * Counts requested bytes cumulatively, not live memory. An existing caller-owned
 * runtime budget takes precedence and is never reset or ended by the engine.
 * @return OK, NULL_ARGUMENT or INVALID_ARGUMENT; failure preserves the context.
 */
NumForgeStatus numforge_context_set_limits(NumForgeContext *context, uint64_t milliseconds,
    size_t allocation_bytes, size_t single_allocation);

/** Parse and validate an owned expression, copying all needed input text.
 * @param[out] result Receives a new handle only on success; never frees an old handle.
 * @param[in] input NUL-terminated UTF-8, at most 4096 bytes.
 * @param[in] context Optional operation policy.
 * @param[out] error Optional diagnostic.
 * @return OK or a lexical, syntax, unsupported, argument or resource error.
 * A prepared expression is thread-confined; reuse it sequentially with different
 * contexts. This does not prepare parameters or bind variables.
 */
NumForgeStatus numforge_expression_prepare(NumForgeExpression **result, const char *input,
    const NumForgeContext *context, NumForgeError *error);
/** Prepare an expression with an explicit ordered parameter declaration.
 * Same ownership/errors/budgets as prepare(). Up to 32 distinct case-sensitive
 * names, each 1..31 ASCII letters, excluding constants, functions, i and ans.
 * Names are copied; unused parameters are allowed. Undeclared variables return
 * UNDEFINED_VARIABLE at their source offset. Invalid declarations return
 * INVALID_ARGUMENT at offset zero; NULL names/arrays return NULL_ARGUMENT.
 * A zero count accepts a NULL names array and is equivalent to prepare().
 */
NumForgeStatus numforge_expression_prepare_parameters(NumForgeExpression **result,
    const char *input, const char *const *names, size_t count,
    const NumForgeContext *context, NumForgeError *error);
/** Query declared parameters. NULL expression returns zero/NULL; an out-of-range
 * index returns NULL. Names are borrowed until expression destruction. */
size_t numforge_expression_parameter_count(const NumForgeExpression *expression);
const char *numforge_expression_parameter_name(const NumForgeExpression *expression, size_t index);
/** Release an expression; NULL is accepted. */
void numforge_expression_destroy(NumForgeExpression *expression);
/** Evaluate a prepared expression without parsing again.
 * @param[out] result New owned result on success; unchanged on any failure.
 * @param[in] expression Prepared handle; never modified or borrowed by the result.
 * @param[in] context Optional evaluation policy; preparation does not fix precision.
 * @param[out] error Optional diagnostic.
 * @return OK or a numeric/domain, unsupported, argument or resource error.
 * Existing destinations are not destroyed automatically. Exact-first evaluation
 * preserves supported rational operations; decimal paths have the existing
 * numerical approximation contracts, not general correctly-rounded guarantees.
 */
NumForgeStatus numforge_expression_evaluate(NumForgeResult **result,
    const NumForgeExpression *expression, const NumForgeContext *context, NumForgeError *error);
/** Evaluate with borrowed typed arguments in declaration order, without parsing
 * or converting them to text. Count must equal the declaration count, including
 * unused parameters (ARGUMENT_COUNT otherwise). NULL array/elements for a
 * nonzero count return NULL_ARGUMENT. Aliased arguments are accepted; the output
 * slot may hold one of the input handles, whose ownership remains with the caller.
 * No arguments are retained after success or failure. A copied approximate input
 * remains approximate; higher working precision cannot recover its lost digits.
 * The expression remains thread-confined. Failure preserves *result.
 * evaluate() is the zero-argument form and returns ARGUMENT_COUNT for parameters.
 */
NumForgeStatus numforge_expression_evaluate_arguments(NumForgeResult **result,
    const NumForgeExpression *expression, const NumForgeResult *const *arguments,
    size_t count, const NumForgeContext *context, NumForgeError *error);
/** Prepare and evaluate under one shared operation budget.
 * Same ownership, numerical and error contracts as the two operations above.
 */
NumForgeStatus numforge_evaluate(NumForgeResult **result, const char *input,
    const NumForgeContext *context, NumForgeError *error);

/** @name Construct owned arguments directly from public numeric objects
 * Copy the input without text conversion. Inputs remain caller-owned and may
 * be destroyed after success. Integer/rational/rational-complex inputs are exact;
 * decimal and decimal-complex inputs conservatively retain approximate provenance.
 * NULL context selects defaults. Operations share/inherit context budgets and
 * preserve *result on every failure. Return OK, NULL_ARGUMENT or a numeric/resource
 * status; optional error has offset zero. Exact scalar copies also create the
 * evaluator's decimal projection at working precision, without rounding the
 * authoritative exact value. No session or expression parsing is involved.
 * @{ */
NumForgeStatus numforge_result_from_integer(NumForgeResult **result, const BigInt *value,
    const NumForgeContext *context, NumForgeError *error);
NumForgeStatus numforge_result_from_rational(NumForgeResult **result, const BigRational *value,
    const NumForgeContext *context, NumForgeError *error);
NumForgeStatus numforge_result_from_decimal(NumForgeResult **result, const BigDecimal *value,
    const NumForgeContext *context, NumForgeError *error);
NumForgeStatus numforge_result_from_complex_rational(NumForgeResult **result, const BigRationalComplex *value,
    const NumForgeContext *context, NumForgeError *error);
NumForgeStatus numforge_result_from_complex_decimal(NumForgeResult **result, const BigComplex *value,
    const NumForgeContext *context, NumForgeError *error);
/** @} */

/** Release a result; NULL is accepted. */
void numforge_result_destroy(NumForgeResult *result);
/** Query the stored type; NULL returns INVALID. */
NumForgeResultKind numforge_result_kind(const NumForgeResult *result);
/** Whether the engine proved the stored result exact; NULL returns false.
 * A false result includes mathematically exact values on unproven decimal paths.
 */
bool numforge_result_is_exact(const NumForgeResult *result);
/** @name Borrowed typed accessors
 * Read-only pointers valid until result destruction; NULL for a different kind
 * or a NULL result. No conversion, allocation or decimal projection occurs.
 * Copy with the matching numeric API to obtain an independently owned value.
 * Do not mutate or destroy these borrowed objects.
 * @{ */
const BigInt *numforge_result_integer(const NumForgeResult *result);
const BigRational *numforge_result_rational(const NumForgeResult *result);
const BigDecimal *numforge_result_decimal(const NumForgeResult *result);
const BigRationalComplex *numforge_result_complex_rational(const NumForgeResult *result);
const BigComplex *numforge_result_complex_decimal(const NumForgeResult *result);
/** @} */
/** Serialize the stored value without additional display rounding.
 * @param[in] result Owned result; its evaluation budget settings are copied.
 * @param[out] text New NUL-terminated allocation, freed with standard free().
 * @return OK, NULL_ARGUMENT, OUT_OF_MEMORY, VALUE_TOO_LARGE, TIME_LIMIT or MEMORY_LIMIT.
 * Failure preserves *text. Success never frees a previous destination. Output
 * is bounded to 65536 bytes. Decimal notation may be scientific; serialization
 * is a value representation, not a promise of parser round-trip acceptance.
 */
NumForgeStatus numforge_result_to_string(const NumForgeResult *result, char **text);
/** Return static English diagnostic text; unknown values return "unknown status". */
const char *numforge_status_to_string(NumForgeStatus status);

#ifdef __cplusplus
}
#endif
#endif
