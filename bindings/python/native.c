#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numforge/engine.h>
#include <numforge/polynomial.h>
#include <numforge/matrix.h>
#include <numforge/statistics.h>
#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

#define RESULT_CAPSULE "numforge.Result.v1"
#define EXPRESSION_CAPSULE "numforge.Expression.v1"
#define MATRIX_CAPSULE "numforge.Matrix.v1"
#define LINEAR_CAPSULE "numforge.LinearSolution.v1"

typedef struct ModuleState { PyObject *error; } ModuleState;
typedef struct Policy {
    NumForgeContext *context;
    uint64_t milliseconds;
    size_t allocation_bytes, single_allocation;
} Policy;

static PyObject *raise_named(PyObject *module, const char *message, const char *name, size_t offset)
{
    ModuleState *state = PyModule_GetState(module);
    PyObject *error = PyObject_CallFunction(state->error, "s", message);
    if (error == NULL) return NULL;
    PyObject *label = PyUnicode_FromString(name), *position = PyLong_FromSize_t(offset);
    if (label == NULL || position == NULL || PyObject_SetAttrString(error, "status", label) < 0 ||
        PyObject_SetAttrString(error, "offset", position) < 0) {
        Py_XDECREF(label); Py_XDECREF(position); Py_DECREF(error); return NULL;
    }
    Py_DECREF(label); Py_DECREF(position); PyErr_SetObject(state->error, error); Py_DECREF(error);
    return NULL;
}
static PyObject *raise_status(PyObject *module, NumForgeStatus status, size_t offset)
{
    static const char *names[] = {"ok", "null_argument", "invalid_argument", "invalid_token",
        "syntax_error", "division_by_zero", "value_too_large", "scale_overflow", "out_of_memory",
        "time_limit", "memory_limit", "unsupported", "argument_count", "undefined_variable"};
    const char *name = (unsigned)status < sizeof(names)/sizeof(names[0]) ? names[status] : "unknown";
    return raise_named(module, numforge_status_to_string(status), name, offset);
}

static NumForgeStatus rational_status(BigRationalStatus status)
{
    switch (status) {
        case BIGRATIONAL_OK: return NUMFORGE_OK;
        case BIGRATIONAL_OUT_OF_MEMORY: return NUMFORGE_OUT_OF_MEMORY;
        case BIGRATIONAL_DIVISION_BY_ZERO: return NUMFORGE_DIVISION_BY_ZERO;
        case BIGRATIONAL_VALUE_TOO_LARGE: return NUMFORGE_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW: return NUMFORGE_SCALE_OVERFLOW;
        default: return NUMFORGE_INVALID_ARGUMENT;
    }
}
static NumForgeStatus integer_status(BigIntStatus status)
{
    switch (status) {
        case BIGINT_OK: return NUMFORGE_OK;
        case BIGINT_OUT_OF_MEMORY: return NUMFORGE_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO: return NUMFORGE_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE: return NUMFORGE_VALUE_TOO_LARGE;
        default: return NUMFORGE_INVALID_ARGUMENT;
    }
}
static NumForgeStatus decimal_status(BigDecimalStatus status)
{
    switch (status) {
        case BIGDECIMAL_OK: return NUMFORGE_OK;
        case BIGDECIMAL_OUT_OF_MEMORY: return NUMFORGE_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE: return NUMFORGE_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return NUMFORGE_SCALE_OVERFLOW;
        default: return NUMFORGE_INVALID_ARGUMENT;
    }
}
static NumForgeStatus budget_status(NumForgeStatus status)
{
    (void)numforge_budget_check();
    if (numforge_budget_failure() == NUMFORGE_BUDGET_TIME) return NUMFORGE_TIME_LIMIT;
    if (numforge_budget_failure() == NUMFORGE_BUDGET_MEMORY) return NUMFORGE_MEMORY_LIMIT;
    return status;
}
static int policy_parse(PyObject *module, PyObject *object, Policy *policy)
{
    memset(policy, 0, sizeof(*policy));
    if (!PyTuple_Check(object) || PyTuple_Size(object) != 5) {
        PyErr_SetString(PyExc_TypeError, "policy must be a five-item tuple"); return 0;
    }
    long long digits = PyLong_AsLongLong(PyTuple_GetItem(object, 0));
    long rounding = PyLong_AsLong(PyTuple_GetItem(object, 1));
    unsigned long long milliseconds = PyLong_AsUnsignedLongLong(PyTuple_GetItem(object, 2));
    size_t bytes = PyLong_AsSize_t(PyTuple_GetItem(object, 3));
    size_t single = PyLong_AsSize_t(PyTuple_GetItem(object, 4));
    if (PyErr_Occurred()) return 0;
    policy->context = numforge_context_create();
    if (policy->context == NULL) { raise_status(module, NUMFORGE_OUT_OF_MEMORY, 0U); return 0; }
    NumForgeStatus status = numforge_context_set_precision(policy->context, (int64_t)digits);
    if (status == NUMFORGE_OK && (rounding < 0 || rounding > 5)) status = NUMFORGE_INVALID_ARGUMENT;
    if (status == NUMFORGE_OK) status = numforge_context_set_rounding(policy->context, (BigDecimalRoundingMode)rounding);
    if (status == NUMFORGE_OK) status = numforge_context_set_limits(policy->context, (uint64_t)milliseconds, bytes, single);
    if (status != NUMFORGE_OK) {
        numforge_context_destroy(policy->context); policy->context = NULL;
        raise_status(module, status, 0U); return 0;
    }
    policy->milliseconds = (uint64_t)milliseconds; policy->allocation_bytes = bytes; policy->single_allocation = single;
    return 1;
}
static void result_destroy(PyObject *capsule)
{
    NumForgeResult *result = PyCapsule_GetPointer(capsule, RESULT_CAPSULE);
    if (result != NULL) numforge_result_destroy(result); else PyErr_Clear();
}
static PyObject *result_wrap(NumForgeResult *result)
{
    PyObject *capsule = PyCapsule_New(result, RESULT_CAPSULE, result_destroy);
    if (capsule == NULL) numforge_result_destroy(result);
    return capsule;
}
static void expression_destroy(PyObject *capsule)
{
    NumForgeExpression *expression = PyCapsule_GetPointer(capsule, EXPRESSION_CAPSULE);
    if (expression != NULL) numforge_expression_destroy(expression); else PyErr_Clear();
}

static PyObject *native_number(PyObject *module, PyObject *args)
{
    const char *kind, *text, *denominator = "1";
    if (!PyArg_ParseTuple(args, "ss|s:number", &kind, &text, &denominator)) return NULL;
    if (strcmp(kind, "integer") != 0 && strcmp(kind, "rational") != 0 && strcmp(kind, "decimal") != 0) {
        PyErr_SetString(PyExc_ValueError, "invalid numeric kind"); return NULL;
    }
    bool owner = numforge_budget_begin(5000U, 64U*1024U*1024U, 512U*1024U);
    NumForgeResult *result = NULL;
    BigInt *top = bigint_create(), *bottom = bigint_create();
    BigRational *rational = bigrational_create(); BigDecimal *decimal = bigdecimal_create();
    NumForgeStatus status = top && bottom && rational && decimal ? NUMFORGE_OK : NUMFORGE_OUT_OF_MEMORY;
    if (status == NUMFORGE_OK && strcmp(kind, "decimal") == 0) {
        bool integral = false;
        status = decimal_status(bigdecimal_set_string(decimal, text));
        if (status == NUMFORGE_OK) status = decimal_status(bigdecimal_is_integer(&integral, decimal));
        if (status == NUMFORGE_OK) status = integral ? decimal_status(bigdecimal_to_bigint(top, decimal)) :
            rational_status(bigrational_from_bigdecimal(rational, decimal));
        if (status == NUMFORGE_OK) status = integral ? numforge_result_from_integer(&result, top, NULL, NULL) :
            numforge_result_from_rational(&result, rational, NULL, NULL);
    } else if (status == NUMFORGE_OK) {
        status = integer_status(bigint_set_string(top, text));
        if (status == NUMFORGE_OK && strcmp(kind, "rational") == 0) {
            status = integer_status(bigint_set_string(bottom, denominator));
            if (status == NUMFORGE_OK) status = rational_status(bigrational_set_fraction(rational, top, bottom));
            if (status == NUMFORGE_OK) status = numforge_result_from_rational(&result, rational, NULL, NULL);
        } else if (status == NUMFORGE_OK) status = numforge_result_from_integer(&result, top, NULL, NULL);
    }
    status = budget_status(status);
    bigint_destroy(top); bigint_destroy(bottom); bigrational_destroy(rational); bigdecimal_destroy(decimal);
    if (owner) numforge_budget_end();
    if (status != NUMFORGE_OK) { numforge_result_destroy(result); return raise_status(module, status, 0U); }
    return result_wrap(result);
}

static PyObject *native_evaluate(PyObject *module, PyObject *args)
{
    const char *text; PyObject *options;
    if (!PyArg_ParseTuple(args, "sO:evaluate", &text, &options)) return NULL;
    Policy policy;
    if (!policy_parse(module, options, &policy)) return NULL;
    NumForgeResult *result = NULL; NumForgeError error = {NUMFORGE_OK, 0U};
    NumForgeStatus status = numforge_evaluate(&result, text, policy.context, &error);
    numforge_context_destroy(policy.context);
    if (status != NUMFORGE_OK) return raise_status(module, status, error.offset);
    return result_wrap(result);
}
static PyObject *native_prepare(PyObject *module, PyObject *args)
{
    const char *text; PyObject *sequence;
    if (!PyArg_ParseTuple(args, "sO:prepare", &text, &sequence)) return NULL;
    PyObject *names = PySequence_Fast(sequence, "parameters must be a sequence");
    if (names == NULL) return NULL;
    Py_ssize_t count = PySequence_Fast_GET_SIZE(names);
    if (count > 32) { Py_DECREF(names); return raise_status(module, NUMFORGE_INVALID_ARGUMENT, 0U); }
    const char *parameters[32];
    for (Py_ssize_t i = 0; i < count; ++i) {
        Py_ssize_t length;
        parameters[i] = PyUnicode_AsUTF8AndSize(PySequence_Fast_GET_ITEM(names, i), &length);
        if (parameters[i] == NULL) { Py_DECREF(names); return NULL; }
        if (memchr(parameters[i], '\0', (size_t)length) != NULL) {
            Py_DECREF(names); PyErr_SetString(PyExc_ValueError, "parameter contains NUL"); return NULL;
        }
    }
    NumForgeExpression *expression = NULL; NumForgeError error = {NUMFORGE_OK, 0U};
    NumForgeStatus status = numforge_expression_prepare_parameters(&expression, text, parameters, (size_t)count, NULL, &error);
    Py_DECREF(names);
    if (status != NUMFORGE_OK) return raise_status(module, status, error.offset);
    PyObject *capsule = PyCapsule_New(expression, EXPRESSION_CAPSULE, expression_destroy);
    if (capsule == NULL) numforge_expression_destroy(expression);
    return capsule;
}
static PyObject *native_parameters(PyObject *module, PyObject *args)
{
    (void)module; PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:parameters", &capsule)) return NULL;
    NumForgeExpression *expression = PyCapsule_GetPointer(capsule, EXPRESSION_CAPSULE);
    if (expression == NULL) return NULL;
    size_t count = numforge_expression_parameter_count(expression);
    PyObject *names = PyTuple_New((Py_ssize_t)count);
    if (names == NULL) return NULL;
    for (size_t i = 0U; i < count; ++i) {
        PyObject *name = PyUnicode_FromString(numforge_expression_parameter_name(expression, i));
        if (name == NULL) { Py_DECREF(names); return NULL; }
        PyTuple_SET_ITEM(names, (Py_ssize_t)i, name);
    }
    return names;
}
static PyObject *native_evaluate_prepared(PyObject *module, PyObject *args)
{
    PyObject *capsule, *sequence, *options;
    if (!PyArg_ParseTuple(args, "OOO:evaluate_prepared", &capsule, &sequence, &options)) return NULL;
    NumForgeExpression *expression = PyCapsule_GetPointer(capsule, EXPRESSION_CAPSULE);
    if (expression == NULL) return NULL;
    PyObject *values = PySequence_Fast(sequence, "arguments must be a sequence");
    if (values == NULL) return NULL;
    Py_ssize_t count = PySequence_Fast_GET_SIZE(values);
    if (count > 32) { Py_DECREF(values); return raise_status(module, NUMFORGE_ARGUMENT_COUNT, 0U); }
    const NumForgeResult *arguments[32];
    for (Py_ssize_t i = 0; i < count; ++i) {
        arguments[i] = PyCapsule_GetPointer(PySequence_Fast_GET_ITEM(values, i), RESULT_CAPSULE);
        if (arguments[i] == NULL) { Py_DECREF(values); return NULL; }
    }
    Policy policy;
    if (!policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    NumForgeResult *result = NULL; NumForgeError error = {NUMFORGE_OK, 0U};
    NumForgeStatus status = numforge_expression_evaluate_arguments(&result, expression, arguments, (size_t)count, policy.context, &error);
    numforge_context_destroy(policy.context); Py_DECREF(values);
    if (status != NUMFORGE_OK) return raise_status(module, status, error.offset);
    return result_wrap(result);
}
static PyObject *native_info(PyObject *module, PyObject *args)
{
    (void)module; PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:info", &capsule)) return NULL;
    NumForgeResult *result = PyCapsule_GetPointer(capsule, RESULT_CAPSULE);
    if (result == NULL) return NULL;
    static const char *names[] = {"invalid", "integer", "rational", "decimal", "complex_rational", "complex_decimal"};
    return Py_BuildValue("sO", names[numforge_result_kind(result)], numforge_result_is_exact(result) ? Py_True : Py_False);
}
static PyObject *native_text(PyObject *module, PyObject *args)
{
    PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:text", &capsule)) return NULL;
    NumForgeResult *result = PyCapsule_GetPointer(capsule, RESULT_CAPSULE);
    if (result == NULL) return NULL;
    char *text = NULL;
    NumForgeStatus status = numforge_result_to_string(result, &text);
    if (status != NUMFORGE_OK) return raise_status(module, status, 0U);
    PyObject *output = PyUnicode_FromString(text); free(text); return output;
}

static PyObject *fraction_pair(PyObject *module, const BigRational *value)
{
    if (value == NULL) return Py_NewRef(Py_None);
    BigInt *top = bigint_create(), *bottom = bigint_create(); char *a = NULL, *b = NULL;
    NumForgeStatus status = top && bottom ? NUMFORGE_OK : NUMFORGE_OUT_OF_MEMORY;
    if (status == NUMFORGE_OK) status = rational_status(bigrational_get_numerator(top, value));
    if (status == NUMFORGE_OK) status = rational_status(bigrational_get_denominator(bottom, value));
    if (status == NUMFORGE_OK) {
        a = bigint_to_string(top); b = bigint_to_string(bottom);
        if (a == NULL || b == NULL) status = NUMFORGE_OUT_OF_MEMORY;
    }
    PyObject *pair = status == NUMFORGE_OK ? Py_BuildValue("ss", a, b) : NULL;
    free(a); free(b); bigint_destroy(top); bigint_destroy(bottom);
    if (status != NUMFORGE_OK) return raise_status(module, budget_status(status), 0U);
    return pair;
}
static PyObject *native_fraction(PyObject *module, PyObject *args)
{
    PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:fraction", &capsule)) return NULL;
    NumForgeResult *result = PyCapsule_GetPointer(capsule, RESULT_CAPSULE);
    if (result == NULL) return NULL;
    bool owner = numforge_budget_begin(5000U, 64U*1024U*1024U, 512U*1024U);
    PyObject *output = NULL;
    const BigRational *rational = numforge_result_rational(result);
    const BigInt *integer = numforge_result_integer(result);
    if (rational != NULL) output = fraction_pair(module, rational);
    else if (integer != NULL) {
        char *text = bigint_to_string(integer);
        if (text == NULL) raise_status(module, budget_status(NUMFORGE_OUT_OF_MEMORY), 0U);
        else { output = Py_BuildValue("ss", text, "1"); free(text); }
    } else PyErr_SetString(PyExc_TypeError, "result is not an exact real integer or fraction");
    NumForgeStatus status = budget_status(NUMFORGE_OK);
    if (owner) numforge_budget_end();
    if (status != NUMFORGE_OK) { Py_XDECREF(output); return raise_status(module, status, 0U); }
    return output;
}
static int dictionary_put(PyObject *dictionary, const char *name, PyObject *value)
{
    if (value == NULL) return 0;
    int status = PyDict_SetItemString(dictionary, name, value); Py_DECREF(value); return status == 0;
}
static PyObject *native_solve(PyObject *module, PyObject *args)
{
    PyObject *capsules[3], *options;
    if (!PyArg_ParseTuple(args, "OOOO:solve", &capsules[0], &capsules[1], &capsules[2], &options)) return NULL;
    const NumForgeResult *inputs[3];
    for (size_t i = 0U; i < 3U; ++i) {
        inputs[i] = PyCapsule_GetPointer(capsules[i], RESULT_CAPSULE);
        if (inputs[i] == NULL) return NULL;
        if (numforge_result_integer(inputs[i]) == NULL && numforge_result_rational(inputs[i]) == NULL) {
            PyErr_SetString(PyExc_TypeError, "solver coefficients must be exact real values"); return NULL;
        }
    }
    Policy policy;
    if (!policy_parse(module, options, &policy)) return NULL;
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational *coefficients[3] = {0}; NumForgePolynomialSolution *solution = NULL;
    NumForgeStatus status = NUMFORGE_OK;
    for (size_t i = 0U; i < 3U && status == NUMFORGE_OK; ++i) {
        coefficients[i] = bigrational_create();
        if (coefficients[i] == NULL) status = NUMFORGE_OUT_OF_MEMORY;
        else status = rational_status(numforge_result_integer(inputs[i]) != NULL ?
            bigrational_from_bigint(coefficients[i], numforge_result_integer(inputs[i])) :
            bigrational_copy(coefficients[i], numforge_result_rational(inputs[i])));
    }
    if (status == NUMFORGE_OK) status = rational_status(numforge_polynomial_solve(&solution, coefficients[0], coefficients[1], coefficients[2]));
    status = budget_status(status);
    PyObject *output = NULL, *values = NULL, *multiplicities = NULL;
    if (status != NUMFORGE_OK) { raise_status(module, status, 0U); goto done; }
    size_t count = numforge_polynomial_root_count(solution);
    values = PyTuple_New((Py_ssize_t)count); multiplicities = PyTuple_New((Py_ssize_t)count); output = PyDict_New();
    if (values == NULL || multiplicities == NULL || output == NULL) goto failed;
    for (size_t i = 0U; i < count; ++i) {
        PyObject *value = NULL;
        if (numforge_polynomial_rational_offset(solution) == NULL) value = Py_NewRef(Py_None);
        else {
            BigRationalComplex *exact = NULL; NumForgeResult *converted = NULL;
            status = rational_status(numforge_polynomial_root_exact(&exact, solution, i));
            if (status == NUMFORGE_OK && !numforge_polynomial_is_imaginary(solution)) {
                BigRational *real = bigrational_create();
                if (real == NULL) status = NUMFORGE_OUT_OF_MEMORY;
                else if (bigrationalcomplex_get_real(real, exact) != BIGCOMPLEX_OK) status = NUMFORGE_OUT_OF_MEMORY;
                if (status == NUMFORGE_OK) status = numforge_result_from_rational(&converted, real, policy.context, NULL);
                bigrational_destroy(real);
            } else if (status == NUMFORGE_OK) status = numforge_result_from_complex_rational(&converted, exact, policy.context, NULL);
            bigrationalcomplex_destroy(exact);
            if (status != NUMFORGE_OK) { numforge_result_destroy(converted); raise_status(module, budget_status(status), 0U); goto failed; }
            value = result_wrap(converted);
        }
        PyObject *multiplicity = PyLong_FromUnsignedLong(numforge_polynomial_root_multiplicity(solution, i));
        if (value == NULL || multiplicity == NULL) { Py_XDECREF(value); Py_XDECREF(multiplicity); goto failed; }
        PyTuple_SET_ITEM(values, (Py_ssize_t)i, value); PyTuple_SET_ITEM(multiplicities, (Py_ssize_t)i, multiplicity);
    }
    const char *kind = numforge_polynomial_kind(solution) == NUMFORGE_POLYNOMIAL_FINITE ? "finite" :
        numforge_polynomial_kind(solution) == NUMFORGE_POLYNOMIAL_ALL ? "all" : "none";
    if (!dictionary_put(output, "kind", PyUnicode_FromString(kind)) ||
        !dictionary_put(output, "degree", PyLong_FromUnsignedLong(numforge_polynomial_degree(solution))) ||
        !dictionary_put(output, "center", fraction_pair(module, numforge_polynomial_center(solution))) ||
        !dictionary_put(output, "radicand", fraction_pair(module, numforge_polynomial_radicand(solution))) ||
        !dictionary_put(output, "offset", fraction_pair(module, numforge_polynomial_rational_offset(solution))) ||
        !dictionary_put(output, "discriminant", fraction_pair(module, numforge_polynomial_discriminant(solution))) ||
        !dictionary_put(output, "imaginary", PyBool_FromLong(numforge_polynomial_is_imaginary(solution)))) goto failed;
    if (PyDict_SetItemString(output, "values", values) < 0 || PyDict_SetItemString(output, "multiplicities", multiplicities) < 0) goto failed;
    status = budget_status(NUMFORGE_OK);
    if (status != NUMFORGE_OK) { raise_status(module, status, 0U); goto failed; }
    goto done;
failed:
    Py_CLEAR(output);
done:
    Py_XDECREF(values); Py_XDECREF(multiplicities); numforge_polynomial_destroy(solution);
    for (size_t i = 0U; i < 3U; ++i) bigrational_destroy(coefficients[i]);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context);
    return output;
}

/* Own both arrays even after partial conversion so callers share one cleanup. */
static NumForgeStatus exact_inputs(PyObject *values, Py_ssize_t count, const char *error,
    BigRational ***entries, const BigRational ***borrowed)
{
    if ((size_t)count > SIZE_MAX / sizeof(**entries)) return NUMFORGE_VALUE_TOO_LARGE;
    *entries = numforge_calloc((size_t)count, sizeof(**entries));
    *borrowed = numforge_calloc((size_t)count, sizeof(**borrowed));
    if (*entries == NULL || *borrowed == NULL) return NUMFORGE_OUT_OF_MEMORY;
    for (Py_ssize_t i = 0; i < count; ++i) {
        const NumForgeResult *input = PyCapsule_GetPointer(PySequence_Fast_GET_ITEM(values, i), RESULT_CAPSULE);
        if (input == NULL) return NUMFORGE_OK; /* Preserve the Python exception. */
        if (numforge_result_integer(input) == NULL && numforge_result_rational(input) == NULL) {
            PyErr_SetString(PyExc_TypeError, error); return NUMFORGE_OK;
        }
        (*entries)[i] = bigrational_create(); (*borrowed)[i] = (*entries)[i];
        if ((*entries)[i] == NULL) return NUMFORGE_OUT_OF_MEMORY;
        NumForgeStatus status = rational_status(numforge_result_integer(input) != NULL ?
            bigrational_from_bigint((*entries)[i], numforge_result_integer(input)) :
            bigrational_copy((*entries)[i], numforge_result_rational(input)));
        if (status != NUMFORGE_OK) return status;
    }
    return NUMFORGE_OK;
}
static void exact_inputs_destroy(BigRational **entries, const BigRational **borrowed, Py_ssize_t count)
{
    if (entries) for (Py_ssize_t i = 0; i < count; ++i) bigrational_destroy(entries[i]);
    free(entries); free(borrowed);
}

static PyObject *native_describe(PyObject *module, PyObject *args)
{
    PyObject *items, *options;
    if (!PyArg_ParseTuple(args, "OO:describe", &items, &options)) return NULL;
    PyObject *values = PySequence_Fast(items, "samples must be a sequence");
    if (values == NULL) return NULL;
    Py_ssize_t count = PySequence_Fast_GET_SIZE(values);
    if (count == 0) { Py_DECREF(values); return raise_status(module, NUMFORGE_INVALID_ARGUMENT, 0); }
    Policy policy;
    if (!policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **entries = NULL; const BigRational **borrowed = NULL;
    NumForgeStatisticsSummary *summary = NULL; PyObject *output = NULL; NumForgeStatus status = NUMFORGE_OK;
    status = exact_inputs(values, count, "statistics samples must be exact real values", &entries, &borrowed);
    if (status != NUMFORGE_OK || PyErr_Occurred()) goto done;
    if (status == NUMFORGE_OK) status = rational_status(numforge_statistics_describe(&summary, borrowed, (size_t)count));
    if (status != NUMFORGE_OK) goto done;
    output = PyDict_New();
    if (output == NULL) goto done;
    if (!dictionary_put(output, "count", PyLong_FromSize_t(numforge_statistics_summary_count(summary))) ||
        !dictionary_put(output, "sum", fraction_pair(module, numforge_statistics_summary_sum(summary))) ||
        !dictionary_put(output, "minimum", fraction_pair(module, numforge_statistics_summary_minimum(summary))) ||
        !dictionary_put(output, "maximum", fraction_pair(module, numforge_statistics_summary_maximum(summary))) ||
        !dictionary_put(output, "mean", fraction_pair(module, numforge_statistics_summary_mean(summary))) ||
        !dictionary_put(output, "variance_population", fraction_pair(module, numforge_statistics_summary_variance_population(summary))) ||
        !dictionary_put(output, "variance_sample", fraction_pair(module, numforge_statistics_summary_variance_sample(summary)))) Py_CLEAR(output);
done:
    status = budget_status(status);
    if (status != NUMFORGE_OK) {
        Py_CLEAR(output);
        if (!PyErr_Occurred()) raise_status(module, status, 0);
    }
    exact_inputs_destroy(entries, borrowed, count); numforge_statistics_summary_destroy(summary);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context); Py_DECREF(values); return output;
}

static void matrix_destroy(PyObject *capsule)
{
    NumForgeMatrix *matrix = PyCapsule_GetPointer(capsule, MATRIX_CAPSULE);
    if (matrix != NULL) numforge_matrix_destroy(matrix); else PyErr_Clear();
}
static PyObject *matrix_wrap(NumForgeMatrix *matrix)
{
    PyObject *capsule = PyCapsule_New(matrix, MATRIX_CAPSULE, matrix_destroy);
    if (capsule == NULL) numforge_matrix_destroy(matrix);
    return capsule;
}
static void linear_destroy(PyObject *capsule)
{
    NumForgeLinearSolution *solution = PyCapsule_GetPointer(capsule, LINEAR_CAPSULE);
    if (solution != NULL) numforge_linear_solution_destroy(solution); else PyErr_Clear();
}
static void borrowed_matrix_destroy(PyObject *capsule)
{
    /* The analysis owns this immutable matrix; retain it until the last view. */
    PyObject *owner = PyCapsule_GetContext(capsule);
    Py_XDECREF(owner);
}
static PyObject *matrix_view(const NumForgeMatrix *matrix, PyObject *owner)
{
    if (matrix == NULL) return Py_NewRef(Py_None);
    PyObject *view = PyCapsule_New((void *)matrix, MATRIX_CAPSULE, borrowed_matrix_destroy);
    if (view == NULL) return NULL;
    if (PyCapsule_SetContext(view, owner) < 0) { Py_DECREF(view); return NULL; }
    Py_INCREF(owner); return view;
}
static PyObject *linear_wrap(NumForgeLinearSolution *solution)
{
    PyObject *owner = PyCapsule_New(solution, LINEAR_CAPSULE, linear_destroy);
    if (owner == NULL) { numforge_linear_solution_destroy(solution); return NULL; }
    size_t rank = numforge_linear_solution_rank(solution), nullity = numforge_linear_solution_nullity(solution);
    PyObject *output = PyDict_New(), *pivots = PyTuple_New((Py_ssize_t)rank), *free_columns = PyTuple_New((Py_ssize_t)nullity);
    if (!output || !pivots || !free_columns) goto failed;
    for (size_t i = 0; i < rank; ++i) {
        PyObject *column = PyLong_FromSize_t(numforge_linear_solution_pivot_column(solution, i));
        if (column == NULL) goto failed;
        PyTuple_SET_ITEM(pivots, (Py_ssize_t)i, column);
    }
    for (size_t i = 0; i < nullity; ++i) {
        PyObject *column = PyLong_FromSize_t(numforge_linear_solution_free_column(solution, i));
        if (column == NULL) goto failed;
        PyTuple_SET_ITEM(free_columns, (Py_ssize_t)i, column);
    }
    const char *kind = numforge_linear_solution_kind(solution) == NUMFORGE_LINEAR_NONE ? "none" :
        numforge_linear_solution_kind(solution) == NUMFORGE_LINEAR_UNIQUE ? "unique" : "infinite";
    if (!dictionary_put(output, "kind", PyUnicode_FromString(kind)) ||
        !dictionary_put(output, "rank", PyLong_FromSize_t(rank)) ||
        !dictionary_put(output, "nullity", PyLong_FromSize_t(nullity)) ||
        !dictionary_put(output, "particular", matrix_view(numforge_linear_solution_particular(solution), owner)) ||
        !dictionary_put(output, "nullspace", matrix_view(numforge_linear_solution_nullspace(solution), owner)) ||
        PyDict_SetItemString(output, "pivot_columns", pivots) < 0 ||
        PyDict_SetItemString(output, "free_columns", free_columns) < 0) goto failed;
    goto done;
failed:
    Py_CLEAR(output);
done:
    Py_XDECREF(pivots); Py_XDECREF(free_columns); Py_DECREF(owner); return output;
}
static PyObject *matrix_error(PyObject *module, NumForgeMatrixStatus status)
{
    NumForgeStatus budget = budget_status(NUMFORGE_OK);
    if (budget != NUMFORGE_OK) return raise_status(module, budget, 0);
    static const char *names[] = {"ok", "null_argument", "invalid_argument", "dimension_mismatch",
        "singular", "out_of_memory", "value_too_large", "scale_overflow"};
    const char *name = (unsigned)status < sizeof(names)/sizeof(names[0]) ? names[status] : "unknown";
    return raise_named(module, numforge_matrix_status_to_string(status), name, 0);
}
static PyObject *native_matrix_create(PyObject *module, PyObject *args)
{
    Py_ssize_t rows, columns; PyObject *items, *options;
    if (!PyArg_ParseTuple(args, "nnOO:matrix_create", &rows, &columns, &items, &options)) return NULL;
    if (rows <= 0 || columns <= 0 || rows > PY_SSIZE_T_MAX / columns) {
        PyErr_SetString(PyExc_ValueError, "matrix dimensions must be positive and fit the platform"); return NULL;
    }
    PyObject *values = PySequence_Fast(items, "matrix entries must be a sequence");
    if (values == NULL) return NULL;
    Py_ssize_t count = rows * columns;
    if (PySequence_Fast_GET_SIZE(values) != count) {
        Py_DECREF(values); PyErr_SetString(PyExc_ValueError, "matrix entry count does not match shape"); return NULL;
    }
    Policy policy;
    if (!policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **entries = NULL; const BigRational **borrowed = NULL; NumForgeMatrix *matrix = NULL;
    PyObject *output = NULL; NumForgeStatus status = NUMFORGE_OK;
    status = exact_inputs(values, count, "matrix entries must be exact real values", &entries, &borrowed);
    if (status != NUMFORGE_OK || PyErr_Occurred()) goto done;
    if (status == NUMFORGE_OK) {
        NumForgeMatrixStatus created = numforge_matrix_create(&matrix, (size_t)rows, (size_t)columns, borrowed);
        if (created != NUMFORGE_MATRIX_OK) { matrix_error(module, created); goto done; }
        output = matrix_wrap(matrix); matrix = NULL;
    }
done:
    if (status != NUMFORGE_OK) raise_status(module, budget_status(status), 0);
    exact_inputs_destroy(entries, borrowed, count); numforge_matrix_destroy(matrix);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context); Py_DECREF(values);
    return output;
}
static PyObject *native_matrix_shape(PyObject *module, PyObject *args)
{
    (void)module; PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:matrix_shape", &capsule)) return NULL;
    const NumForgeMatrix *matrix = PyCapsule_GetPointer(capsule, MATRIX_CAPSULE);
    if (matrix == NULL) return NULL;
    return Py_BuildValue("nn", (Py_ssize_t)numforge_matrix_rows(matrix), (Py_ssize_t)numforge_matrix_columns(matrix));
}
static PyObject *native_matrix_operation(PyObject *module, PyObject *args)
{
    const char *operation; PyObject *capsule, *other, *options;
    if (!PyArg_ParseTuple(args, "sOOO:matrix_operation", &operation, &capsule, &other, &options)) return NULL;
    const NumForgeMatrix *a = PyCapsule_GetPointer(capsule, MATRIX_CAPSULE), *b = NULL;
    if (a == NULL) return NULL;
    bool multiply = strcmp(operation, "multiply") == 0, solve = strcmp(operation, "solve") == 0;
    bool determinant = strcmp(operation, "determinant") == 0, rows = strcmp(operation, "rows") == 0;
    bool rank = strcmp(operation, "rank") == 0, general = strcmp(operation, "solve_general") == 0;
    if (!multiply && !solve && !determinant && !rows && !rank && !general) {
        PyErr_SetString(PyExc_ValueError, "unknown matrix operation"); return NULL;
    }
    if (multiply || solve || general) {
        b = PyCapsule_GetPointer(other, MATRIX_CAPSULE); if (b == NULL) return NULL;
    }
    Policy policy;
    if (!policy_parse(module, options, &policy)) return NULL;
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    NumForgeMatrixStatus status = NUMFORGE_MATRIX_OK;
    NumForgeMatrix *matrix = NULL; BigRational *value = NULL; PyObject *output = NULL;
    if (multiply || solve) {
        status = multiply ? numforge_matrix_multiply(&matrix, a, b) : numforge_matrix_solve(&matrix, a, b);
        if (status == NUMFORGE_MATRIX_OK) { output = matrix_wrap(matrix); matrix = NULL; }
    } else if (rank) {
        size_t coefficient_rank = 0;
        status = numforge_matrix_rank(&coefficient_rank, a);
        if (status == NUMFORGE_MATRIX_OK) output = PyLong_FromSize_t(coefficient_rank);
    } else if (general) {
        NumForgeLinearSolution *solution = NULL;
        status = numforge_matrix_solve_general(&solution, a, b);
        if (status == NUMFORGE_MATRIX_OK) output = linear_wrap(solution);
    } else if (determinant) {
        value = bigrational_create();
        status = value ? numforge_matrix_determinant(value, a) : NUMFORGE_MATRIX_OUT_OF_MEMORY;
        if (status == NUMFORGE_MATRIX_OK) output = fraction_pair(module, value);
    } else {
        output = PyTuple_New((Py_ssize_t)numforge_matrix_rows(a));
        for (size_t i = 0; output && i < numforge_matrix_rows(a); ++i) {
            PyObject *row = PyTuple_New((Py_ssize_t)numforge_matrix_columns(a));
            if (row == NULL) { Py_CLEAR(output); break; }
            PyTuple_SET_ITEM(output, (Py_ssize_t)i, row);
            for (size_t j = 0; j < numforge_matrix_columns(a); ++j) {
                PyObject *pair = fraction_pair(module, numforge_matrix_entry(a, i, j));
                if (pair == NULL) { Py_CLEAR(output); break; }
                PyTuple_SET_ITEM(row, (Py_ssize_t)j, pair);
            }
        }
    }
    if (status != NUMFORGE_MATRIX_OK || budget_status(NUMFORGE_OK) != NUMFORGE_OK) {
        Py_CLEAR(output);
        if (!PyErr_Occurred()) matrix_error(module, status);
    }
    numforge_matrix_destroy(matrix); bigrational_destroy(value);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context);
    return output;
}

typedef struct RootCallback {
    PyObject *module, *function;
    BigInt *numerator;
    NumForgeStatus status;
    bool continuous;
} RootCallback;

static NumForgeNumericalStatus root_sample(BigDecimal *value, NumForgeRootSample *sample,
    const BigRational *x, int64_t places, void *user)
{
    RootCallback *work = user;
    PyObject *coordinate = fraction_pair(work->module, x);
    if (coordinate == NULL) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    PyObject *returned = PyObject_CallFunction(work->function, "OL", coordinate, (long long)places);
    Py_DECREF(coordinate);
    if (returned == NULL) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    if (returned == Py_None) { Py_DECREF(returned); return NUMFORGE_NUMERICAL_OK; }
    const NumForgeResult *result = PyCapsule_GetPointer(returned, RESULT_CAPSULE);
    if (result == NULL) { Py_DECREF(returned); return NUMFORGE_NUMERICAL_CALLBACK_FAILED; }
    const BigInt *integer = numforge_result_integer(result);
    const BigRational *rational = numforge_result_rational(result);
    const BigDecimal *decimal = numforge_result_decimal(result);
    bool zero = false;
    if (integer) {
        zero = bigint_is_zero(integer);
        work->status = decimal_status(bigdecimal_from_bigint(value, integer));
    } else if (rational) {
        work->status = rational_status(bigrational_get_numerator(work->numerator, rational));
        if (work->status == NUMFORGE_OK) zero = bigint_is_zero(work->numerator);
        if (work->status == NUMFORGE_OK) work->status = rational_status(
            bigrational_to_bigdecimal(value, rational, places, BIGDECIMAL_ROUND_HALF_EVEN));
    } else if (decimal) work->status = decimal_status(bigdecimal_copy(value, decimal));
    else PyErr_SetString(PyExc_TypeError, "root samples must be real values or None");
    sample->real = work->status == NUMFORGE_OK && !PyErr_Occurred();
    sample->exact_zero = sample->real && zero && numforge_result_is_exact(result);
    Py_DECREF(returned);
    return sample->real ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
static NumForgeNumericalStatus root_domain(bool *verified, const BigRational *lower,
    const BigRational *upper, void *user)
{
    (void)lower; (void)upper;
    *verified = ((RootCallback *)user)->continuous;
    return NUMFORGE_NUMERICAL_OK;
}
static NumForgeStatus numerical_status(NumForgeNumericalStatus status)
{
    switch (status) {
        case NUMFORGE_NUMERICAL_OK: return NUMFORGE_OK;
        case NUMFORGE_NUMERICAL_NULL_ARGUMENT: return NUMFORGE_NULL_ARGUMENT;
        case NUMFORGE_NUMERICAL_OUT_OF_MEMORY: return NUMFORGE_OUT_OF_MEMORY;
        case NUMFORGE_NUMERICAL_VALUE_TOO_LARGE: return NUMFORGE_VALUE_TOO_LARGE;
        case NUMFORGE_NUMERICAL_SCALE_OVERFLOW: return NUMFORGE_SCALE_OVERFLOW;
        default: return NUMFORGE_INVALID_ARGUMENT;
    }
}
static PyObject *native_find_root(PyObject *module, PyObject *args)
{
    PyObject *function, *items, *settings, *options; const char *residual_text;
    if (!PyArg_ParseTuple(args, "OOsOO:find_root", &function, &items, &residual_text, &settings, &options)) return NULL;
    if (!PyCallable_Check(function) || !PyTuple_Check(settings) || PyTuple_Size(settings) != 4) {
        PyErr_SetString(PyExc_TypeError, "invalid root callback/settings"); return NULL;
    }
    NumForgeRootOptions root_options; numforge_root_options_init(&root_options);
    root_options.decimal_places = PyLong_AsLongLong(PyTuple_GetItem(settings, 0));
    root_options.max_iterations = PyLong_AsSize_t(PyTuple_GetItem(settings, 1));
    root_options.max_evaluations = PyLong_AsSize_t(PyTuple_GetItem(settings, 2));
    int continuous = PyObject_IsTrue(PyTuple_GetItem(settings, 3));
    if (PyErr_Occurred()) return NULL;
    PyObject *values = PySequence_Fast(items, "root bounds/tolerance must be a sequence");
    if (values == NULL) return NULL;
    if (PySequence_Fast_GET_SIZE(values) != 3) {
        Py_DECREF(values); PyErr_SetString(PyExc_ValueError, "expected two bounds and x tolerance"); return NULL;
    }
    Policy policy;
    if (!policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **entries = NULL; const BigRational **borrowed = NULL;
    BigDecimal *tolerance = NULL; NumForgeRoot *root = NULL;
    RootCallback work = {module, function, NULL, NUMFORGE_OK, continuous != 0};
    NumForgeStatus status = exact_inputs(values, 3, "root bounds and x tolerance must be exact real values", &entries, &borrowed);
    PyObject *output = NULL; char *residual = NULL;
    if (PyErr_Occurred()) goto done;
    if (status == NUMFORGE_OK) {
        tolerance = bigdecimal_create(); work.numerator = bigint_create();
        if (!tolerance || !work.numerator) status = NUMFORGE_OUT_OF_MEMORY;
    }
    if (status == NUMFORGE_OK) status = decimal_status(bigdecimal_set_string(tolerance, residual_text));
    root_options.x_tolerance = entries ? entries[2] : NULL;
    root_options.residual_tolerance = tolerance;
    if (status == NUMFORGE_OK) {
        NumForgeNumericalStatus solved = numforge_root_solve(&root, root_sample, root_domain,
            &work, entries[0], entries[1], &root_options);
        status = solved == NUMFORGE_NUMERICAL_CALLBACK_FAILED && work.status != NUMFORGE_OK ?
            work.status : numerical_status(solved);
    }
    status = budget_status(status);
    if (PyErr_Occurred()) goto done; /* Preserve callback exceptions, including KeyboardInterrupt. */
    if (status != NUMFORGE_OK) { raise_status(module, status, 0); goto done; }
    if (numforge_root_residual(root)) status = decimal_status(bigdecimal_to_string(numforge_root_residual(root), &residual));
    if (status != NUMFORGE_OK) { raise_status(module, budget_status(status), 0); goto done; }
    static const char *kinds[] = {"found", "not_bracketed", "domain_gap", "domain_unverified", "iteration_limit", "precision_limit"};
    output = PyDict_New();
    if (output && (!dictionary_put(output, "kind", PyUnicode_FromString(kinds[numforge_root_kind(root)])) ||
        !dictionary_put(output, "x", fraction_pair(module, numforge_root_x(root))) ||
        !dictionary_put(output, "lower", fraction_pair(module, numforge_root_lower(root))) ||
        !dictionary_put(output, "upper", fraction_pair(module, numforge_root_upper(root))) ||
        !dictionary_put(output, "residual", residual ? PyUnicode_FromString(residual) : Py_NewRef(Py_None)) ||
        !dictionary_put(output, "exact_zero", PyBool_FromLong(numforge_root_exact_zero(root))) ||
        !dictionary_put(output, "iterations", PyLong_FromSize_t(numforge_root_iterations(root))) ||
        !dictionary_put(output, "evaluations", PyLong_FromSize_t(numforge_root_evaluations(root))))) Py_CLEAR(output);
    status = budget_status(NUMFORGE_OK);
    if (status != NUMFORGE_OK && !PyErr_Occurred()) { Py_CLEAR(output); raise_status(module, status, 0); }
done:
    free(residual); numforge_root_destroy(root); bigint_destroy(work.numerator); bigdecimal_destroy(tolerance);
    exact_inputs_destroy(entries, borrowed, 3); Py_DECREF(values);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context);
    return output;
}

static PyMethodDef methods[] = {
    {"number", native_number, METH_VARARGS, "Copy a lossless numeric input through C."},
    {"evaluate", native_evaluate, METH_VARARGS, "Evaluate with the C engine."},
    {"prepare", native_prepare, METH_VARARGS, "Prepare a C expression."},
    {"parameters", native_parameters, METH_VARARGS, "Read declared parameter names."},
    {"evaluate_prepared", native_evaluate_prepared, METH_VARARGS, "Evaluate borrowed C arguments."},
    {"info", native_info, METH_VARARGS, "Query stored numeric kind and exactness."},
    {"text", native_text, METH_VARARGS, "Serialize a stored C result."},
    {"fraction", native_fraction, METH_VARARGS, "Copy exact rational components."},
    {"solve", native_solve, METH_VARARGS, "Solve exact polynomial coefficients in C."},
    {"find_root", native_find_root, METH_VARARGS, "Run bounded C bisection with a Python sample callback."},
    {"describe", native_describe, METH_VARARGS, "Compute an exact C descriptive statistics summary."},
    {"matrix_create", native_matrix_create, METH_VARARGS, "Copy exact real entries into a C matrix."},
    {"matrix_shape", native_matrix_shape, METH_VARARGS, "Read a C matrix shape."},
    {"matrix_operation", native_matrix_operation, METH_VARARGS, "Run an exact C matrix operation."},
    {NULL, NULL, 0, NULL}
};
static int module_traverse(PyObject *module, visitproc visit, void *arg)
{ ModuleState *state = PyModule_GetState(module); Py_VISIT(state->error); return 0; }
static int module_clear(PyObject *module)
{ ModuleState *state = PyModule_GetState(module); Py_CLEAR(state->error); return 0; }
static void module_free(void *module) { (void)module_clear(module); }
static struct PyModuleDef definition = {
    PyModuleDef_HEAD_INIT, "_native", "NumForge public C API adapter.", sizeof(ModuleState),
    methods, NULL, module_traverse, module_clear, module_free
};
PyMODINIT_FUNC PyInit__native(void)
{
    PyObject *module = PyModule_Create(&definition);
    if (module == NULL) return NULL;
    ModuleState *state = PyModule_GetState(module);
    state->error = PyErr_NewException("numforge.Error", PyExc_RuntimeError, NULL);
    if (state->error == NULL || PyModule_AddObjectRef(module, "Error", state->error) < 0 ||
        PyModule_AddStringConstant(module, "core_version", NUMFORGE_CORE_VERSION) < 0) { Py_DECREF(module); return NULL; }
    PyObject *maximum = PyLong_FromSize_t(SIZE_MAX);
    if (maximum == NULL || PyModule_AddObject(module, "MAX_ALLOCATION", maximum) < 0) {
        Py_XDECREF(maximum); Py_DECREF(module); return NULL;
    }
    return module;
}
