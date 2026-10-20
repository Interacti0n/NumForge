#ifndef NUMFORGE_PYTHON_NATIVE_INTERNAL_H
#define NUMFORGE_PYTHON_NATIVE_INTERNAL_H
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numforge/engine.h>

/* Private binding support, never installed as a numeric/engine API. Capsule
 * names and module state must agree across every adapter translation unit. */
#define NF_PYTHON_RESULT_CAPSULE "numforge.Result.v1"
typedef struct NFPythonModuleState { PyObject *error; } NFPythonModuleState;
typedef struct NFPythonPolicy {
    NumForgeContext *context;
    uint64_t milliseconds;
    size_t allocation_bytes, single_allocation;
} NFPythonPolicy;

PyObject *nf_python_raise_named(PyObject *module, const char *message, const char *name, size_t offset);
PyObject *nf_python_raise_status(PyObject *module, NumForgeStatus status, size_t offset);
NumForgeStatus nf_python_rational_status(BigRationalStatus status);
NumForgeStatus nf_python_integer_status(BigIntStatus status);
NumForgeStatus nf_python_decimal_status(BigDecimalStatus status);
NumForgeStatus nf_python_budget_status(NumForgeStatus status);
int nf_python_policy_parse(PyObject *module, PyObject *object, NFPythonPolicy *policy);
PyObject *nf_python_fraction_pair(PyObject *module, const BigRational *value);
int nf_python_dictionary_put(PyObject *dictionary, const char *name, PyObject *value);
/* Own partially constructed arrays too; always destroy after this call.
 * A pending Python exception can accompany OK and must be checked by the caller. */
NumForgeStatus nf_python_exact_inputs(PyObject *values, Py_ssize_t count, const char *error,
    BigRational ***entries, const BigRational ***borrowed);
void nf_python_exact_inputs_destroy(BigRational **entries, const BigRational **borrowed, Py_ssize_t count);
/* Consume an owned result even when capsule creation fails. */
PyObject *nf_python_result_wrap(NumForgeResult *result);

PyObject *nf_python_find_root(PyObject *module, PyObject *args);
PyObject *nf_python_find_roots(PyObject *module, PyObject *args);
PyObject *nf_python_visit_grid(PyObject *module, PyObject *args);
PyObject *nf_python_solve(PyObject *module, PyObject *args);
PyObject *nf_python_describe(PyObject *module, PyObject *args);
PyObject *nf_python_covariance(PyObject *module, PyObject *args);
PyObject *nf_python_linear_regression(PyObject *module, PyObject *args);
PyObject *nf_python_matrix_create(PyObject *module, PyObject *args);
PyObject *nf_python_matrix_shape(PyObject *module, PyObject *args);
PyObject *nf_python_matrix_operation(PyObject *module, PyObject *args);
#endif
