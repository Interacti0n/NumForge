#include "bindings/python/native_internal.h"
#include <numforge/polynomial.h>
#include <numforge/matrix.h>
#include <numforge/statistics.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

#define EXPRESSION_CAPSULE "numforge.Expression.v1"
#define MATRIX_CAPSULE "numforge.Matrix.v1"
#define LINEAR_CAPSULE "numforge.LinearSolution.v1"

static void result_destroy(PyObject *capsule)
{
    NumForgeResult *result = PyCapsule_GetPointer(capsule, NF_PYTHON_RESULT_CAPSULE);
    if (result != NULL) numforge_result_destroy(result); else PyErr_Clear();
}
static PyObject *result_wrap(NumForgeResult *result)
{
    PyObject *capsule = PyCapsule_New(result, NF_PYTHON_RESULT_CAPSULE, result_destroy);
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
    return result_wrap(result);
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
    return result_wrap(result);
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
    return result_wrap(result);
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
    bool owner = numforge_budget_begin(5000U, 64U*1024U*1024U, 512U*1024U);
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
static PyObject *native_solve(PyObject *module, PyObject *args)
{
    PyObject *capsules[3], *options;
    if (!PyArg_ParseTuple(args, "OOOO:solve", &capsules[0], &capsules[1], &capsules[2], &options)) return NULL;
    const NumForgeResult *inputs[3];
    for (size_t i = 0U; i < 3U; ++i) {
        inputs[i] = PyCapsule_GetPointer(capsules[i], NF_PYTHON_RESULT_CAPSULE);
        if (inputs[i] == NULL) return NULL;
        if (numforge_result_integer(inputs[i]) == NULL && numforge_result_rational(inputs[i]) == NULL) {
            PyErr_SetString(PyExc_TypeError, "solver coefficients must be exact real values"); return NULL;
        }
    }
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) return NULL;
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational *coefficients[3] = {0}; NumForgePolynomialSolution *solution = NULL;
    NumForgeStatus status = NUMFORGE_OK;
    for (size_t i = 0U; i < 3U && status == NUMFORGE_OK; ++i) {
        coefficients[i] = bigrational_create();
        if (coefficients[i] == NULL) status = NUMFORGE_OUT_OF_MEMORY;
        else status = nf_python_rational_status(numforge_result_integer(inputs[i]) != NULL ?
            bigrational_from_bigint(coefficients[i], numforge_result_integer(inputs[i])) :
            bigrational_copy(coefficients[i], numforge_result_rational(inputs[i])));
    }
    if (status == NUMFORGE_OK) status = nf_python_rational_status(numforge_polynomial_solve(&solution, coefficients[0], coefficients[1], coefficients[2]));
    status = nf_python_budget_status(status);
    PyObject *output = NULL, *values = NULL, *multiplicities = NULL;
    if (status != NUMFORGE_OK) { nf_python_raise_status(module, status, 0U); goto done; }
    size_t count = numforge_polynomial_root_count(solution);
    values = PyTuple_New((Py_ssize_t)count); multiplicities = PyTuple_New((Py_ssize_t)count); output = PyDict_New();
    if (values == NULL || multiplicities == NULL || output == NULL) goto failed;
    for (size_t i = 0U; i < count; ++i) {
        PyObject *value = NULL;
        if (numforge_polynomial_rational_offset(solution) == NULL) value = Py_NewRef(Py_None);
        else {
            BigRationalComplex *exact = NULL; NumForgeResult *converted = NULL;
            status = nf_python_rational_status(numforge_polynomial_root_exact(&exact, solution, i));
            if (status == NUMFORGE_OK && !numforge_polynomial_is_imaginary(solution)) {
                BigRational *real = bigrational_create();
                if (real == NULL) status = NUMFORGE_OUT_OF_MEMORY;
                else if (bigrationalcomplex_get_real(real, exact) != BIGCOMPLEX_OK) status = NUMFORGE_OUT_OF_MEMORY;
                if (status == NUMFORGE_OK) status = numforge_result_from_rational(&converted, real, policy.context, NULL);
                bigrational_destroy(real);
            } else if (status == NUMFORGE_OK) status = numforge_result_from_complex_rational(&converted, exact, policy.context, NULL);
            bigrationalcomplex_destroy(exact);
            if (status != NUMFORGE_OK) { numforge_result_destroy(converted); nf_python_raise_status(module, nf_python_budget_status(status), 0U); goto failed; }
            value = result_wrap(converted);
        }
        PyObject *multiplicity = PyLong_FromUnsignedLong(numforge_polynomial_root_multiplicity(solution, i));
        if (value == NULL || multiplicity == NULL) { Py_XDECREF(value); Py_XDECREF(multiplicity); goto failed; }
        PyTuple_SET_ITEM(values, (Py_ssize_t)i, value); PyTuple_SET_ITEM(multiplicities, (Py_ssize_t)i, multiplicity);
    }
    const char *kind = numforge_polynomial_kind(solution) == NUMFORGE_POLYNOMIAL_FINITE ? "finite" :
        numforge_polynomial_kind(solution) == NUMFORGE_POLYNOMIAL_ALL ? "all" : "none";
    if (!nf_python_dictionary_put(output, "kind", PyUnicode_FromString(kind)) ||
        !nf_python_dictionary_put(output, "degree", PyLong_FromUnsignedLong(numforge_polynomial_degree(solution))) ||
        !nf_python_dictionary_put(output, "center", nf_python_fraction_pair(module, numforge_polynomial_center(solution))) ||
        !nf_python_dictionary_put(output, "radicand", nf_python_fraction_pair(module, numforge_polynomial_radicand(solution))) ||
        !nf_python_dictionary_put(output, "offset", nf_python_fraction_pair(module, numforge_polynomial_rational_offset(solution))) ||
        !nf_python_dictionary_put(output, "discriminant", nf_python_fraction_pair(module, numforge_polynomial_discriminant(solution))) ||
        !nf_python_dictionary_put(output, "imaginary", PyBool_FromLong(numforge_polynomial_is_imaginary(solution)))) goto failed;
    if (PyDict_SetItemString(output, "values", values) < 0 || PyDict_SetItemString(output, "multiplicities", multiplicities) < 0) goto failed;
    status = nf_python_budget_status(NUMFORGE_OK);
    if (status != NUMFORGE_OK) { nf_python_raise_status(module, status, 0U); goto failed; }
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

static PyObject *native_describe(PyObject *module, PyObject *args)
{
    PyObject *items, *options;
    if (!PyArg_ParseTuple(args, "OO:describe", &items, &options)) return NULL;
    PyObject *values = PySequence_Fast(items, "samples must be a sequence");
    if (values == NULL) return NULL;
    Py_ssize_t count = PySequence_Fast_GET_SIZE(values);
    if (count == 0) { Py_DECREF(values); return nf_python_raise_status(module, NUMFORGE_INVALID_ARGUMENT, 0); }
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **entries = NULL; const BigRational **borrowed = NULL;
    NumForgeStatisticsSummary *summary = NULL; PyObject *output = NULL; NumForgeStatus status = NUMFORGE_OK;
    status = nf_python_exact_inputs(values, count, "statistics samples must be exact real values", &entries, &borrowed);
    if (status != NUMFORGE_OK || PyErr_Occurred()) goto done;
    if (status == NUMFORGE_OK) status = nf_python_rational_status(numforge_statistics_describe(&summary, borrowed, (size_t)count));
    if (status != NUMFORGE_OK) goto done;
    output = PyDict_New();
    if (output == NULL) goto done;
    if (!nf_python_dictionary_put(output, "count", PyLong_FromSize_t(numforge_statistics_summary_count(summary))) ||
        !nf_python_dictionary_put(output, "sum", nf_python_fraction_pair(module, numforge_statistics_summary_sum(summary))) ||
        !nf_python_dictionary_put(output, "minimum", nf_python_fraction_pair(module, numforge_statistics_summary_minimum(summary))) ||
        !nf_python_dictionary_put(output, "maximum", nf_python_fraction_pair(module, numforge_statistics_summary_maximum(summary))) ||
        !nf_python_dictionary_put(output, "mean", nf_python_fraction_pair(module, numforge_statistics_summary_mean(summary))) ||
        !nf_python_dictionary_put(output, "variance_population", nf_python_fraction_pair(module, numforge_statistics_summary_variance_population(summary))) ||
        !nf_python_dictionary_put(output, "variance_sample", nf_python_fraction_pair(module, numforge_statistics_summary_variance_sample(summary)))) Py_CLEAR(output);
done:
    status = nf_python_budget_status(status);
    if (status != NUMFORGE_OK) {
        Py_CLEAR(output);
        if (!PyErr_Occurred()) nf_python_raise_status(module, status, 0);
    }
    nf_python_exact_inputs_destroy(entries, borrowed, count); numforge_statistics_summary_destroy(summary);
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
    {"find_root", nf_python_find_root, METH_VARARGS, "Run bounded C bisection with a Python sample callback."},
    {"find_roots", nf_python_find_roots, METH_VARARGS, "Run bounded C root scanning with a Python sample callback."},
    {"visit_grid", nf_python_visit_grid, METH_VARARGS, "Visit exact rational coordinates from the public C grid walker."},
    {"describe", native_describe, METH_VARARGS, "Compute an exact C descriptive statistics summary."},
    {"matrix_create", native_matrix_create, METH_VARARGS, "Copy exact real entries into a C matrix."},
    {"matrix_shape", native_matrix_shape, METH_VARARGS, "Read a C matrix shape."},
    {"matrix_operation", native_matrix_operation, METH_VARARGS, "Run an exact C matrix operation."},
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
