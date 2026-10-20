#include "bindings/python/native_internal.h"
#include <numforge/statistics.h>
#include <numforge/runtime.h>

enum PairedStatistic { POPULATION_COVARIANCE, SAMPLE_COVARIANCE, LINEAR_REGRESSION };

static PyObject *paired_statistics(PyObject *module, PyObject *left_items,
    PyObject *right_items, PyObject *options, enum PairedStatistic operation)
{
    PyObject *left = PySequence_Fast(left_items, "left samples must be a sequence");
    if (left == NULL) return NULL;
    PyObject *right = PySequence_Fast(right_items, "right samples must be a sequence");
    if (right == NULL) { Py_DECREF(left); return NULL; }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(left);
    if (count != PySequence_Fast_GET_SIZE(right) || count < (operation == POPULATION_COVARIANCE ? 1 : 2)) {
        Py_DECREF(left); Py_DECREF(right);
        PyErr_SetString(PyExc_ValueError, "paired statistics require equal datasets and enough observations"); return NULL;
    }
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) { Py_DECREF(left); Py_DECREF(right); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **x = NULL, **y = NULL; const BigRational **bx = NULL, **by = NULL;
    BigRational *result = NULL; NumForgeLinearRegression *fit = NULL; PyObject *output = NULL;
    NumForgeStatus status = nf_python_exact_inputs(left, count, "paired samples must be exact real values", &x, &bx);
    if (status != NUMFORGE_OK || PyErr_Occurred()) goto done;
    status = nf_python_exact_inputs(right, count, "paired samples must be exact real values", &y, &by);
    if (status != NUMFORGE_OK || PyErr_Occurred()) goto done;
    if (operation == LINEAR_REGRESSION) {
        status = nf_python_rational_status(numforge_statistics_linear_regression(&fit, bx, by, (size_t)count));
        if (status == NUMFORGE_OK) {
            output = PyDict_New();
            if (output && (!nf_python_dictionary_put(output, "count", PyLong_FromSize_t(numforge_linear_regression_count(fit))) ||
                !nf_python_dictionary_put(output, "slope", nf_python_fraction_pair(module, numforge_linear_regression_slope(fit))) ||
                !nf_python_dictionary_put(output, "intercept", nf_python_fraction_pair(module, numforge_linear_regression_intercept(fit))))) Py_CLEAR(output);
        }
    } else {
        result = bigrational_create();
        status = result ? nf_python_rational_status(numforge_statistics_covariance(result, bx, by,
            (size_t)count, operation == SAMPLE_COVARIANCE)) : NUMFORGE_OUT_OF_MEMORY;
        if (status == NUMFORGE_OK) output = nf_python_fraction_pair(module, result);
    }

done:
    status = nf_python_budget_status(status);
    if (status != NUMFORGE_OK) {
        Py_CLEAR(output);
        if (!PyErr_Occurred()) nf_python_raise_status(module, status, 0);
    }
    nf_python_exact_inputs_destroy(x, bx, count); nf_python_exact_inputs_destroy(y, by, count);
    bigrational_destroy(result); numforge_linear_regression_destroy(fit);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context); Py_DECREF(left); Py_DECREF(right); return output;
}

PyObject *nf_python_covariance(PyObject *module, PyObject *args)
{
    PyObject *left, *right, *sample, *options;
    if (!PyArg_ParseTuple(args, "OOOO:covariance", &left, &right, &sample, &options)) return NULL;
    if (!PyBool_Check(sample)) { PyErr_SetString(PyExc_TypeError, "sample must be a bool"); return NULL; }
    return paired_statistics(module, left, right, options, sample == Py_True ? SAMPLE_COVARIANCE : POPULATION_COVARIANCE);
}

PyObject *nf_python_linear_regression(PyObject *module, PyObject *args)
{
    PyObject *x, *y, *options;
    if (!PyArg_ParseTuple(args, "OOO:linear_regression", &x, &y, &options)) return NULL;
    return paired_statistics(module, x, y, options, LINEAR_REGRESSION);
}

PyObject *nf_python_describe(PyObject *module, PyObject *args)
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
