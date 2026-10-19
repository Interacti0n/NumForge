#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numforge/engine.h>
#include <numforge/polynomial.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

#define RESULT_CAPSULE "numforge.Result.v1"
#define EXPRESSION_CAPSULE "numforge.Expression.v1"

typedef struct ModuleState { PyObject *error; } ModuleState;
typedef struct Policy {
    NumForgeContext *context;
    uint64_t milliseconds;
    size_t allocation_bytes, single_allocation;
} Policy;

static PyObject *raise_status(PyObject *module, NumForgeStatus status, size_t offset)
{
    static const char *names[] = {"ok", "null_argument", "invalid_argument", "invalid_token",
        "syntax_error", "division_by_zero", "value_too_large", "scale_overflow", "out_of_memory",
        "time_limit", "memory_limit", "unsupported", "argument_count", "undefined_variable"};
    ModuleState *state = PyModule_GetState(module);
    PyObject *error = PyObject_CallFunction(state->error, "s", numforge_status_to_string(status));
    if (error == NULL) return NULL;
    const char *name = (unsigned)status < sizeof(names)/sizeof(names[0]) ? names[status] : "unknown";
    PyObject *label = PyUnicode_FromString(name), *position = PyLong_FromSize_t(offset);
    if (label == NULL || position == NULL || PyObject_SetAttrString(error, "status", label) < 0 ||
        PyObject_SetAttrString(error, "offset", position) < 0) {
        Py_XDECREF(label); Py_XDECREF(position); Py_DECREF(error); return NULL;
    }
    Py_DECREF(label); Py_DECREF(position); PyErr_SetObject(state->error, error); Py_DECREF(error);
    return NULL;
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
