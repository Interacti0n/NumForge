#include "bindings/python/native_internal.h"
#include <numforge/matrix.h>
#include <numforge/runtime.h>
#include <string.h>

#define LINEAR_CAPSULE "numforge.LinearSolution.v1"
#define MATRIX_CAPSULE "numforge.Matrix.v1"

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
    if (!nf_python_dictionary_put(output, "kind", PyUnicode_FromString(kind)) ||
        !nf_python_dictionary_put(output, "rank", PyLong_FromSize_t(rank)) ||
        !nf_python_dictionary_put(output, "nullity", PyLong_FromSize_t(nullity)) ||
        !nf_python_dictionary_put(output, "particular", matrix_view(numforge_linear_solution_particular(solution), owner)) ||
        !nf_python_dictionary_put(output, "nullspace", matrix_view(numforge_linear_solution_nullspace(solution), owner)) ||
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
    NumForgeStatus budget = nf_python_budget_status(NUMFORGE_OK);
    if (budget != NUMFORGE_OK) return nf_python_raise_status(module, budget, 0);
    static const char *names[] = {"ok", "null_argument", "invalid_argument", "dimension_mismatch",
        "singular", "out_of_memory", "value_too_large", "scale_overflow"};
    const char *name = (unsigned)status < sizeof(names)/sizeof(names[0]) ? names[status] : "unknown";
    return nf_python_raise_named(module, numforge_matrix_status_to_string(status), name, 0);
}
PyObject *nf_python_matrix_create(PyObject *module, PyObject *args)
{
    Py_ssize_t rows, columns; PyObject *items, *options;
    if (!PyArg_ParseTuple(args, "nnOO:matrix_create", &rows, &columns, &items, &options)) return NULL;
    if (rows <= 0 || columns <= 0 || rows > PY_SSIZE_T_MAX / columns) {
        PyErr_SetString(PyExc_ValueError, "matrix dimensions must be positive and fit the platform"); return NULL;
    }
    PyObject *values = PySequence_Tuple(items);
    if (values == NULL) return NULL;
    Py_ssize_t count = rows * columns;
    if (PySequence_Fast_GET_SIZE(values) != count) {
        Py_DECREF(values); PyErr_SetString(PyExc_ValueError, "matrix entry count does not match shape"); return NULL;
    }
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **entries = NULL; const BigRational **borrowed = NULL; NumForgeMatrix *matrix = NULL;
    PyObject *output = NULL; NumForgeStatus status = NUMFORGE_OK;
    status = nf_python_exact_inputs(values, count, "matrix entries must be exact real values", &entries, &borrowed);
    if (status != NUMFORGE_OK || PyErr_Occurred()) goto done;
    if (status == NUMFORGE_OK) {
        NumForgeMatrixStatus created = numforge_matrix_create(&matrix, (size_t)rows, (size_t)columns, borrowed);
        if (created != NUMFORGE_MATRIX_OK) { matrix_error(module, created); goto done; }
        output = matrix_wrap(matrix); matrix = NULL;
    }
done:
    if (status != NUMFORGE_OK) nf_python_raise_status(module, nf_python_budget_status(status), 0);
    nf_python_exact_inputs_destroy(entries, borrowed, count); numforge_matrix_destroy(matrix);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context); Py_DECREF(values);
    return output;
}
PyObject *nf_python_matrix_shape(PyObject *module, PyObject *args)
{
    (void)module; PyObject *capsule;
    if (!PyArg_ParseTuple(args, "O:matrix_shape", &capsule)) return NULL;
    const NumForgeMatrix *matrix = PyCapsule_GetPointer(capsule, MATRIX_CAPSULE);
    if (matrix == NULL) return NULL;
    return Py_BuildValue("nn", (Py_ssize_t)numforge_matrix_rows(matrix), (Py_ssize_t)numforge_matrix_columns(matrix));
}
PyObject *nf_python_matrix_operation(PyObject *module, PyObject *args)
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
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) return NULL;
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
        if (status == NUMFORGE_MATRIX_OK) output = nf_python_fraction_pair(module, value);
    } else {
        output = PyTuple_New((Py_ssize_t)numforge_matrix_rows(a));
        for (size_t i = 0; output && i < numforge_matrix_rows(a); ++i) {
            PyObject *row = PyTuple_New((Py_ssize_t)numforge_matrix_columns(a));
            if (row == NULL) { Py_CLEAR(output); break; }
            PyTuple_SET_ITEM(output, (Py_ssize_t)i, row);
            for (size_t j = 0; j < numforge_matrix_columns(a); ++j) {
                PyObject *pair = nf_python_fraction_pair(module, numforge_matrix_entry(a, i, j));
                if (pair == NULL) { Py_CLEAR(output); break; }
                PyTuple_SET_ITEM(row, (Py_ssize_t)j, pair);
            }
        }
    }
    if (status != NUMFORGE_MATRIX_OK || nf_python_budget_status(NUMFORGE_OK) != NUMFORGE_OK) {
        Py_CLEAR(output);
        if (!PyErr_Occurred()) matrix_error(module, status);
    }
    numforge_matrix_destroy(matrix); bigrational_destroy(value);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context);
    return output;
}
