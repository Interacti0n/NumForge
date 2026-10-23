#include "bindings/python/native_internal.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

#define EXPRESSION_CAPSULE "numforge.Expression.v1"

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
    bool owner = numforge_budget_begin(10000U, 64U*1024U*1024U, 512U*1024U);
    NumForgeResult *result = NULL;
    BigInt *top = bigint_create(), *bottom = bigint_create();
    BigRational *rational = bigrational_create(); BigDecimal *decimal = bigdecimal_create();
    NumForgeStatus status = top && bottom && rational && decimal ? NUMFORGE_OK : NUMFORGE_OUT_OF_MEMORY;
    if (status == NUMFORGE_OK && strcmp(kind, "decimal") == 0) {
        bool integral = false;
        status = nf_python_decimal_status(bigdecimal_set_string(decimal, text));
        if (status == NUMFORGE_OK) status = nf_python_decimal_status(bigdecimal_is_integer(&integral, decimal));
        if (status == NUMFORGE_OK) status = integral ? nf_python_decimal_status(bigdecimal_to_bigint(top, decimal)) :
            nf_python_rational_status(bigrational_from_bigdecimal(rational, decimal));
        if (status == NUMFORGE_OK) status = integral ? numforge_result_from_integer(&result, top, NULL, NULL) :
            numforge_result_from_rational(&result, rational, NULL, NULL);
    } else if (status == NUMFORGE_OK) {
        status = nf_python_integer_status(bigint_set_string(top, text));
        if (status == NUMFORGE_OK && strcmp(kind, "rational") == 0) {
            status = nf_python_integer_status(bigint_set_string(bottom, denominator));
            if (status == NUMFORGE_OK) status = nf_python_rational_status(bigrational_set_fraction(rational, top, bottom));
            if (status == NUMFORGE_OK) status = numforge_result_from_rational(&result, rational, NULL, NULL);
        } else if (status == NUMFORGE_OK) status = numforge_result_from_integer(&result, top, NULL, NULL);
    }
    status = nf_python_budget_status(status);
    bigint_destroy(top); bigint_destroy(bottom); bigrational_destroy(rational); bigdecimal_destroy(decimal);
    if (owner) numforge_budget_end();
    if (status != NUMFORGE_OK) { numforge_result_destroy(result); return nf_python_raise_status(module, status, 0U); }
    return nf_python_result_wrap(result);
}

static PyObject *native_evaluate(PyObject *module, PyObject *args)
{
    const char *text; PyObject *options;
    if (!PyArg_ParseTuple(args, "sO:evaluate", &text, &options)) return NULL;
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) return NULL;
    NumForgeResult *result = NULL; NumForgeError error = {NUMFORGE_OK, 0U};
    NumForgeStatus status = numforge_evaluate(&result, text, policy.context, &error);
    numforge_context_destroy(policy.context);
    if (status != NUMFORGE_OK) return nf_python_raise_status(module, status, error.offset);
    return nf_python_result_wrap(result);
}
static PyObject *native_prepare(PyObject *module, PyObject *args)
{
    const char *text; PyObject *sequence;
    if (!PyArg_ParseTuple(args, "sO:prepare", &text, &sequence)) return NULL;
    PyObject *names = PySequence_Fast(sequence, "parameters must be a sequence");
    if (names == NULL) return NULL;
    Py_ssize_t count = PySequence_Fast_GET_SIZE(names);
    if (count > 32) { Py_DECREF(names); return nf_python_raise_status(module, NUMFORGE_INVALID_ARGUMENT, 0U); }
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
    if (status != NUMFORGE_OK) return nf_python_raise_status(module, status, error.offset);
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
    if (count > 32) { Py_DECREF(values); return nf_python_raise_status(module, NUMFORGE_ARGUMENT_COUNT, 0U); }
    const NumForgeResult *arguments[32];
    for (Py_ssize_t i = 0; i < count; ++i) {
        arguments[i] = PyCapsule_GetPointer(PySequence_Fast_GET_ITEM(values, i), NF_PYTHON_RESULT_CAPSULE);
        if (arguments[i] == NULL) { Py_DECREF(values); return NULL; }
    }
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    NumForgeResult *result = NULL; NumForgeError error = {NUMFORGE_OK, 0U};
    NumForgeStatus status = numforge_expression_evaluate_arguments(&result, expression, arguments, (size_t)count, policy.context, &error);
    numforge_context_destroy(policy.context); Py_DECREF(values);
    if (status != NUMFORGE_OK) return nf_python_raise_status(module, status, error.offset);
    return nf_python_result_wrap(result);
}
static PyObject *native_info(PyObject *module, PyObject *args)
{
    (void)module; PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:info", &capsule)) return NULL;
    NumForgeResult *result = PyCapsule_GetPointer(capsule, NF_PYTHON_RESULT_CAPSULE);
    if (result == NULL) return NULL;
    static const char *names[] = {"invalid", "integer", "rational", "decimal", "complex_rational", "complex_decimal"};
    return Py_BuildValue("sO", names[numforge_result_kind(result)], numforge_result_is_exact(result) ? Py_True : Py_False);
}
static PyObject *native_text(PyObject *module, PyObject *args)
{
    PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:text", &capsule)) return NULL;
    NumForgeResult *result = PyCapsule_GetPointer(capsule, NF_PYTHON_RESULT_CAPSULE);
    if (result == NULL) return NULL;
    char *text = NULL;
    NumForgeStatus status = numforge_result_to_string(result, &text);
    if (status != NUMFORGE_OK) return nf_python_raise_status(module, status, 0U);
    PyObject *output = PyUnicode_FromString(text); free(text); return output;
}

static PyObject *native_fraction(PyObject *module, PyObject *args)
{
    PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:fraction", &capsule)) return NULL;
    NumForgeResult *result = PyCapsule_GetPointer(capsule, NF_PYTHON_RESULT_CAPSULE);
    if (result == NULL) return NULL;
    bool owner = numforge_budget_begin(10000U, 64U*1024U*1024U, 512U*1024U);
    PyObject *output = NULL;
    const BigRational *rational = numforge_result_rational(result);
    const BigInt *integer = numforge_result_integer(result);
    if (rational != NULL) output = nf_python_fraction_pair(module, rational);
    else if (integer != NULL) {
        char *text = bigint_to_string(integer);
        if (text == NULL) nf_python_raise_status(module, nf_python_budget_status(NUMFORGE_OUT_OF_MEMORY), 0U);
        else { output = Py_BuildValue("ss", text, "1"); free(text); }
    } else PyErr_SetString(PyExc_TypeError, "result is not an exact real integer or fraction");
    NumForgeStatus status = nf_python_budget_status(NUMFORGE_OK);
    if (owner) numforge_budget_end();
    if (status != NUMFORGE_OK) { Py_XDECREF(output); return nf_python_raise_status(module, status, 0U); }
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
    {"solve", nf_python_solve, METH_VARARGS, "Solve exact polynomial coefficients in C."},
    {"find_root", nf_python_find_root, METH_VARARGS, "Run bounded C bisection with a Python sample callback."},
    {"integrate", nf_python_integrate, METH_VARARGS, "Run bounded C Simpson quadrature with a Python sample callback."},
    {"find_roots", nf_python_find_roots, METH_VARARGS, "Run bounded C root scanning with a Python sample callback."},
    {"visit_grid", nf_python_visit_grid, METH_VARARGS, "Visit exact rational coordinates from the public C grid walker."},
    {"describe", nf_python_describe, METH_VARARGS, "Compute an exact C descriptive statistics summary."},
    {"covariance", nf_python_covariance, METH_VARARGS, "Compute exact paired covariance in C."},
    {"linear_regression", nf_python_linear_regression, METH_VARARGS, "Fit an exact least-squares line in C."},
    {"matrix_create", nf_python_matrix_create, METH_VARARGS, "Copy exact real entries into a C matrix."},
    {"matrix_shape", nf_python_matrix_shape, METH_VARARGS, "Read a C matrix shape."},
    {"matrix_operation", nf_python_matrix_operation, METH_VARARGS, "Run an exact C matrix operation."},
    {NULL, NULL, 0, NULL}
};
static int module_traverse(PyObject *module, visitproc visit, void *arg)
{ NFPythonModuleState *state = PyModule_GetState(module); Py_VISIT(state->error); return 0; }
static int module_clear(PyObject *module)
{ NFPythonModuleState *state = PyModule_GetState(module); Py_CLEAR(state->error); return 0; }
static void module_free(void *module) { (void)module_clear(module); }
static struct PyModuleDef definition = {
    PyModuleDef_HEAD_INIT, "_native", "NumForge public C API adapter.", sizeof(NFPythonModuleState),
    methods, NULL, module_traverse, module_clear, module_free
};
PyMODINIT_FUNC PyInit__native(void)
{
    PyObject *module = PyModule_Create(&definition);
    if (module == NULL) return NULL;
    NFPythonModuleState *state = PyModule_GetState(module);
    state->error = PyErr_NewException("numforge.Error", PyExc_RuntimeError, NULL);
    if (state->error == NULL || PyModule_AddObjectRef(module, "Error", state->error) < 0 ||
        PyModule_AddStringConstant(module, "core_version", NUMFORGE_CORE_VERSION) < 0) { Py_DECREF(module); return NULL; }
    PyObject *maximum = PyLong_FromSize_t(SIZE_MAX);
    if (maximum == NULL || PyModule_AddObject(module, "MAX_ALLOCATION", maximum) < 0) {
        Py_XDECREF(maximum); Py_DECREF(module); return NULL;
    }
    return module;
}
