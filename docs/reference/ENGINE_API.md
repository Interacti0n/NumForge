# Standalone expression engine C API

Status: implemented in the current source tree, after 2.4.0. Previously published
2.4.0 SDK archives do not contain this API. It is the first public engine subset,
not a symbolic system or new REST endpoint. A separate source-built
[Python adapter](../guides/PYTHON.md) now uses this API. The separate
[exact polynomial solver](POLYNOMIAL_API.md) is available directly in the numeric
library, without the engine.

## Build and link

The engine is enabled by default independently of applications:

```sh
cmake -S . -B build/engine -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_APPS=OFF
cmake --build build/engine --config Release
cmake --install build/engine --config Release --prefix build/engine-installed
```

External CMake consumers use:

```cmake
find_package(NumForge CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE NumForge::engine)
```

Include `<numforge/engine.h>`. The engine transitively links `NumForge::numforge`
and required platform libraries. No source-tree include directory, server, Node,
Python or UI assets are needed by a consumer. `NumForge::numforge` remains the
standalone numeric target and does not acquire a parser dependency.

For a numeric-only source build, disable `NUMFORGE_BUILD_ENGINE`,
`NUMFORGE_BUILD_APPS`, `BUILD_TESTING`, fuzzers and benchmarks. The engine header
may still be installed, but `NumForge::engine` is absent and cannot be linked.

## Evaluate and preserve the value

This complete example is compiled from `examples/sdk/engine.c`:

```c
#include <numforge/engine.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    NumForgeContext *context = numforge_context_create();
    NumForgeResult *result = NULL;
    NumForgeError error = {NUMFORGE_OK, 0};
    char *text = NULL;
    int exit_code = 1;
    if (context == NULL) goto cleanup;
    if (numforge_context_set_precision(context, 1000) != NUMFORGE_OK) goto cleanup;
    if (numforge_evaluate(&result, "sqrt(2)", context, &error) != NUMFORGE_OK) {
        fprintf(stderr, "%s at byte %zu\n", numforge_status_to_string(error.status), error.offset);
        goto cleanup;
    }
    if (numforge_result_to_string(result, &text) != NUMFORGE_OK) goto cleanup;
    printf("sqrt(2) = %s\n", text);
    printf("exact = %s\n", numforge_result_is_exact(result) ? "yes" : "no");
    exit_code = 0;
cleanup:
    free(text);
    numforge_result_destroy(result);
    numforge_context_destroy(context);
    return exit_code;
}
```

Decimal serialization may be scientific notation. The stored number is available
through `numforge_result_decimal()` for direct BigDecimal operations; it is not
reparsed from display text. Corresponding borrowed accessors expose BigInt,
BigRational, BigComplex and BigRationalComplex. The wrong accessor returns NULL.
Copy a borrowed value with its numeric API to keep it after result destruction.

## Prepared expressions

`numforge_expression_prepare(&expression, input, context, &error)` copies all
needed input into an owned AST. `numforge_expression_evaluate(&result, expression,
context, &error)` can reuse it sequentially at different working precisions without
parsing again. Release it with `numforge_expression_destroy()`. Preparation checks
grammar and capability support; numeric domains are checked during evaluation.
Results are independent of both the expression and the context lifetimes.

Explicit parameters are supported through
`numforge_expression_prepare_parameters(&expression, input, names, count, context, &error)`.
Declare up to 32 distinct case-sensitive names of 1..31 ASCII letters, excluding
built-in function/constant names, `i` and `ans`. Declarations and source are copied;
unused declarations are allowed. Undeclared variables return UNDEFINED_VARIABLE
at their source byte offset. Invalid/duplicate declarations return INVALID_ARGUMENT
at offset zero; absent required arrays/names return NULL_ARGUMENT.

`numforge_expression_parameter_count()` and `numforge_expression_parameter_name()`
describe declaration order. `numforge_expression_evaluate_arguments(&result,
expression, arguments, count, context, &error)` accepts an array of borrowed
`const NumForgeResult *` in that order. Count must match, including unused parameters;
otherwise ARGUMENT_COUNT. NULL required arrays/elements return NULL_ARGUMENT.
No argument is retained after success or failure. Repeated argument handles and
an output slot previously holding an argument are supported; the caller still owns
the old handle. The zero-argument evaluate function reports ARGUMENT_COUNT for
parameterized expressions. Handles remain thread-confined.

Construct arguments directly from BigInt, BigRational, BigDecimal, BigComplex or
BigRationalComplex with the matching `numforge_result_from_*()` function. These
copy numeric objects, never convert through text. Integer/rational inputs remain
exact; decimal input retains conservative approximate provenance, even if it is
the finite decimal 0.3. All constructors preserve output on failure, use optional
context budgets, and copy their policy. Increasing working precision cannot recover
digits already lost in an approximate argument. See the compiled
`examples/sdk/parameters.c` example (`x*x+y` with x=3 and y=4 yields exact 13).

Application graph/session prepared handles remain private; persistent variables,
assignments and user function definitions are outside this public parameter API.

## Supported expressions and numerical meaning

The engine uses the existing [expression grammar](CALCULATOR_EXPRESSIONS.md):
decimal literals, constants, arithmetic, fractions, factorials and implemented
deterministic scalar real/complex calls. It uses radians. Arguments use the
existing semicolon separator. `sqrt(-1)` returns the principal complex value.

The initial public subset rejects `ans`, random calls, quantity/conversion calls
and pending functions as UNSUPPORTED, including nested occurrences. Unbound
variables without explicit declarations return UNDEFINED_VARIABLE. Assignments are outside the expression grammar
and produce a syntax/token error. No session state is created or modified.

Precision sets requested **working significant digits**, from 1 through 10000;
it is neither decimal places nor display digits. Defaults are 34 digits and
half-even rounding. Existing algorithms can use minimum working precision
(commonly 34) and internal guards. Exact integer/rational operations remain exact,
so setting precision to 1 does not round `1/3` or `2^128` to one digit.
Serialization applies no additional display rounding. Composed expressions can
lose accuracy through cancellation/conditioning; this setting does not certify
the final number of correct digits. See [numeric contracts](C_API.md) and
[numerical testing limitations](../testing/NUMERIC_TESTING.md).

`numforge_result_is_exact()` reports a proven exact integer/rational/rational-complex
path. Decimal paths are conservatively approximate, even when the mathematical
result happens to be exact. For example `0.1+0.2` retains exact `3/10`; `sqrt(2)`
is approximate. This API does not return certified intervals/error bounds and does
not promise general correctly rounded transcendental or composed complex results.

## Ownership, failures and limits

All opaque handles are owned and thread-confined. Their destroy functions accept
NULL. A NULL operation context selects the defaults without requiring allocation.
Contexts must not be mutated while an operation uses them. Do not concurrently
use the same prepared expression/result; independent handles can be used on
separate threads. There is no mutable global evaluation context.

On failure, destination handle pointers and serialized-text pointers remain
unchanged. On success, a new allocation is assigned and any previous destination
is still the caller's responsibility. Text uses standard `free()`, under the same
compatible C-runtime requirement as the rest of the SDK. Optional `NumForgeError`
reports a status and zero-based UTF-8 **byte** offset; success clears it. Use enum
statuses for logic, never diagnostic strings. Non-source failures use offset zero.

| Status | Meaning |
| --- | --- |
| OK | New result/handle/text committed successfully |
| NULL_ARGUMENT / INVALID_ARGUMENT | Required pointer absent or invalid policy/numeric domain |
| INVALID_TOKEN / SYNTAX_ERROR | Malformed input |
| ARGUMENT_COUNT | Invalid call arity |
| UNDEFINED_VARIABLE | No variable binding is available |
| UNSUPPORTED | Capability outside the initial public subset |
| DIVISION_BY_ZERO | Singular numeric operation |
| VALUE_TOO_LARGE / SCALE_OVERFLOW | Structural/output/numeric representation limit |
| OUT_OF_MEMORY | Allocation failure without a resource-budget failure |
| TIME_LIMIT / MEMORY_LIMIT | Cooperative deadline/cumulative or single allocation limit |

Limits: 4096 input bytes, AST depth 256, at most 256 call arguments, root degree
at most 10000, serialization at most 65536 bytes. Defaults: 10000 ms, 64 MiB
cumulative requested allocations and 512 KiB per allocation. Set operation
budgets with `numforge_context_set_limits()`. Zero denies work; SIZE_MAX disables
either memory limit; milliseconds must fit INT64_MAX. Direct numeric APIs keep
their existing contracts and do not inherit these engine structural caps.

One-shot evaluation shares its budget across preparation and evaluation. Separate
prepare/evaluate calls each own a fresh budget unless the caller has already
started a [runtime scope](C_API.md#optional-resource-scope). An existing outer scope takes
precedence, remains active and retains failure state. Serialization uses a fresh
scope with the result's copied evaluation policy, or inherits the caller's scope.
Budgets are cooperative; they do not provide hard process isolation.

## Verification and interface availability

`engine_tests` checks typed exact/complex values, 1000-digit sqrt(2) against the
existing independently generated frozen reference, six-rounding-mode parity with
the calculator evaluator, prepared-handle lifetime/precision changes, UTF-8 error
offsets, atomic failures, resource inheritance and exhaustive representative
allocation failures. Installed C/C++ consumers compile using only public headers.
The packaged SDK contains a compiled 1000-digit example when the engine is present.
`engine_parameters_tests` additionally checks declared/undeclared names, ownership,
typed input copies, exact/approximate provenance, repeated/aliased arguments,
1000-digit parameterized evaluation, allocation failure and cancellation recovery.

No mathematical algorithm is duplicated or optimized by this wrapper. Existing
[decimal/math benchmarks](../performance/protocols/DECIMAL.md) remain the
algorithm performance evidence; this change claims no speedup.

| Interface | Availability |
| --- | --- |
| C / C++ | Public numeric calls and this engine API |
| CLI / local HTTP / Web | Existing interfaces over the same parser/evaluator; session/quantity capabilities remain application-owned |
| Python | Experimental source-built CPython adapter; see its guide for supported subset and packaging |
| WASM / versioned public REST service | Planned; no endpoint or deployment added here |
