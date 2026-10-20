#include "bindings/python/native_internal.h"
#include <numforge/polynomial.h>
#include <numforge/runtime.h>

PyObject *nf_python_solve(PyObject *module, PyObject *args)
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
            value = nf_python_result_wrap(converted);
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
