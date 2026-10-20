#include "bindings/python/native_internal.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

PyObject *nf_python_raise_named(PyObject *module, const char *message, const char *name, size_t offset)
{
    NFPythonModuleState *state = PyModule_GetState(module);
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
PyObject *nf_python_raise_status(PyObject *module, NumForgeStatus status, size_t offset)
{
    static const char *names[] = {"ok", "null_argument", "invalid_argument", "invalid_token",
        "syntax_error", "division_by_zero", "value_too_large", "scale_overflow", "out_of_memory",
        "time_limit", "memory_limit", "unsupported", "argument_count", "undefined_variable"};
    const char *name = (unsigned)status < sizeof(names)/sizeof(names[0]) ? names[status] : "unknown";
    return nf_python_raise_named(module, numforge_status_to_string(status), name, offset);
}

NumForgeStatus nf_python_rational_status(BigRationalStatus status)
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
NumForgeStatus nf_python_integer_status(BigIntStatus status)
{
    switch (status) {
        case BIGINT_OK: return NUMFORGE_OK;
        case BIGINT_OUT_OF_MEMORY: return NUMFORGE_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO: return NUMFORGE_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE: return NUMFORGE_VALUE_TOO_LARGE;
        default: return NUMFORGE_INVALID_ARGUMENT;
    }
}
NumForgeStatus nf_python_decimal_status(BigDecimalStatus status)
{
    switch (status) {
        case BIGDECIMAL_OK: return NUMFORGE_OK;
        case BIGDECIMAL_OUT_OF_MEMORY: return NUMFORGE_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE: return NUMFORGE_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return NUMFORGE_SCALE_OVERFLOW;
        default: return NUMFORGE_INVALID_ARGUMENT;
    }
}
NumForgeStatus nf_python_budget_status(NumForgeStatus status)
{
    (void)numforge_budget_check();
    if (numforge_budget_failure() == NUMFORGE_BUDGET_TIME) return NUMFORGE_TIME_LIMIT;
    if (numforge_budget_failure() == NUMFORGE_BUDGET_MEMORY) return NUMFORGE_MEMORY_LIMIT;
    return status;
}
int nf_python_policy_parse(PyObject *module, PyObject *object, NFPythonPolicy *policy)
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
    if (policy->context == NULL) { nf_python_raise_status(module, NUMFORGE_OUT_OF_MEMORY, 0U); return 0; }
    NumForgeStatus status = numforge_context_set_precision(policy->context, (int64_t)digits);
    if (status == NUMFORGE_OK && (rounding < 0 || rounding > 5)) status = NUMFORGE_INVALID_ARGUMENT;
    if (status == NUMFORGE_OK) status = numforge_context_set_rounding(policy->context, (BigDecimalRoundingMode)rounding);
    if (status == NUMFORGE_OK) status = numforge_context_set_limits(policy->context, (uint64_t)milliseconds, bytes, single);
    if (status != NUMFORGE_OK) {
        numforge_context_destroy(policy->context); policy->context = NULL;
        nf_python_raise_status(module, status, 0U); return 0;
    }
    policy->milliseconds = (uint64_t)milliseconds; policy->allocation_bytes = bytes; policy->single_allocation = single;
    return 1;
}
PyObject *nf_python_fraction_pair(PyObject *module, const BigRational *value)
{
    if (value == NULL) return Py_NewRef(Py_None);
    BigInt *top = bigint_create(), *bottom = bigint_create(); char *a = NULL, *b = NULL;
    NumForgeStatus status = top && bottom ? NUMFORGE_OK : NUMFORGE_OUT_OF_MEMORY;
    if (status == NUMFORGE_OK) status = nf_python_rational_status(bigrational_get_numerator(top, value));
    if (status == NUMFORGE_OK) status = nf_python_rational_status(bigrational_get_denominator(bottom, value));
    if (status == NUMFORGE_OK) {
        a = bigint_to_string(top); b = bigint_to_string(bottom);
        if (a == NULL || b == NULL) status = NUMFORGE_OUT_OF_MEMORY;
    }
    PyObject *pair = status == NUMFORGE_OK ? Py_BuildValue("ss", a, b) : NULL;
    free(a); free(b); bigint_destroy(top); bigint_destroy(bottom);
    if (status != NUMFORGE_OK) return nf_python_raise_status(module, nf_python_budget_status(status), 0U);
    return pair;
}
int nf_python_dictionary_put(PyObject *dictionary, const char *name, PyObject *value)
{
    if (value == NULL) return 0;
    int status = PyDict_SetItemString(dictionary, name, value); Py_DECREF(value); return status == 0;
}
/* Own both arrays even after partial conversion so callers share one cleanup. */
NumForgeStatus nf_python_exact_inputs(PyObject *values, Py_ssize_t count, const char *error,
    BigRational ***entries, const BigRational ***borrowed)
{
    if ((size_t)count > SIZE_MAX / sizeof(**entries)) return NUMFORGE_VALUE_TOO_LARGE;
    *entries = numforge_calloc((size_t)count, sizeof(**entries));
    *borrowed = numforge_calloc((size_t)count, sizeof(**borrowed));
    if (*entries == NULL || *borrowed == NULL) return NUMFORGE_OUT_OF_MEMORY;
    for (Py_ssize_t i = 0; i < count; ++i) {
        const NumForgeResult *input = PyCapsule_GetPointer(PySequence_Fast_GET_ITEM(values, i), NF_PYTHON_RESULT_CAPSULE);
        if (input == NULL) return NUMFORGE_OK; /* Preserve the Python exception. */
        if (numforge_result_integer(input) == NULL && numforge_result_rational(input) == NULL) {
            PyErr_SetString(PyExc_TypeError, error); return NUMFORGE_OK;
        }
        (*entries)[i] = bigrational_create(); (*borrowed)[i] = (*entries)[i];
        if ((*entries)[i] == NULL) return NUMFORGE_OUT_OF_MEMORY;
        NumForgeStatus status = nf_python_rational_status(numforge_result_integer(input) != NULL ?
            bigrational_from_bigint((*entries)[i], numforge_result_integer(input)) :
            bigrational_copy((*entries)[i], numforge_result_rational(input)));
        if (status != NUMFORGE_OK) return status;
    }
    return NUMFORGE_OK;
}
void nf_python_exact_inputs_destroy(BigRational **entries, const BigRational **borrowed, Py_ssize_t count)
{
    if (entries) for (Py_ssize_t i = 0; i < count; ++i) bigrational_destroy(entries[i]);
    free(entries); free(borrowed);
}

static void result_destroy(PyObject *capsule)
{
    NumForgeResult *result = PyCapsule_GetPointer(capsule, NF_PYTHON_RESULT_CAPSULE);
    if (result != NULL) numforge_result_destroy(result); else PyErr_Clear();
}
PyObject *nf_python_result_wrap(NumForgeResult *result)
{
    PyObject *capsule = PyCapsule_New(result, NF_PYTHON_RESULT_CAPSULE, result_destroy);
    if (capsule == NULL) numforge_result_destroy(result);
    return capsule;
}
