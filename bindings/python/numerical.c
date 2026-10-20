#include "bindings/python/native_internal.h"
#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include <stdlib.h>

typedef struct RootCallback {
    PyObject *module, *function;
    BigInt *numerator;
    NumForgeStatus status;
    bool continuous;
} RootCallback;

static NumForgeNumericalStatus root_callback(BigDecimal *value, BigRational *proposal, NumForgeRootSample *sample,
    const BigRational *x, int64_t places, void *user)
{
    RootCallback *work = user;
    PyObject *coordinate = nf_python_fraction_pair(work->module, x);
    if (coordinate == NULL) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    PyObject *returned = PyObject_CallFunction(work->function, "OL", coordinate, (long long)places);
    Py_DECREF(coordinate);
    if (returned == NULL) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    if (returned == Py_None) { Py_DECREF(returned); return NUMFORGE_NUMERICAL_OK; }
    const NumForgeResult *result = PyCapsule_GetPointer(returned, NF_PYTHON_RESULT_CAPSULE);
    if (result == NULL) { Py_DECREF(returned); return NUMFORGE_NUMERICAL_CALLBACK_FAILED; }
    const BigInt *integer = numforge_result_integer(result);
    const BigRational *rational = numforge_result_rational(result);
    const BigDecimal *decimal = numforge_result_decimal(result);
    bool zero = false;
    if (integer) {
        zero = bigint_is_zero(integer);
        work->status = nf_python_decimal_status(bigdecimal_from_bigint(value, integer));
        if (proposal && work->status == NUMFORGE_OK)
            work->status = nf_python_rational_status(bigrational_from_bigint(proposal, integer));
    } else if (rational) {
        work->status = nf_python_rational_status(bigrational_get_numerator(work->numerator, rational));
        if (work->status == NUMFORGE_OK) zero = bigint_is_zero(work->numerator);
        if (work->status == NUMFORGE_OK) work->status = nf_python_rational_status(
            bigrational_to_bigdecimal(value, rational, places, BIGDECIMAL_ROUND_HALF_EVEN));
        if (proposal && work->status == NUMFORGE_OK)
            work->status = nf_python_rational_status(bigrational_copy(proposal, rational));
    } else if (decimal) {
        work->status = nf_python_decimal_status(bigdecimal_copy(value, decimal));
        if (proposal && work->status == NUMFORGE_OK)
            work->status = nf_python_rational_status(bigrational_from_bigdecimal(proposal, decimal));
    }
    else PyErr_SetString(PyExc_TypeError, "root samples must be real values or None");
    sample->real = work->status == NUMFORGE_OK && !PyErr_Occurred();
    sample->exact_zero = sample->real && zero && numforge_result_is_exact(result);
    Py_DECREF(returned);
    return sample->real ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
static NumForgeNumericalStatus root_sample(BigDecimal *value, NumForgeRootSample *sample,
    const BigRational *x, int64_t places, void *user)
{ return root_callback(value, NULL, sample, x, places, user); }
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
typedef struct GridCallback { PyObject *module, *function; } GridCallback;
static NumForgeNumericalStatus grid_visit(const BigRational *x, size_t index, void *user)
{
    GridCallback *work = user;
    PyObject *coordinate = nf_python_fraction_pair(work->module, x), *position = NULL, *returned = NULL;
    if (coordinate) position = PyLong_FromSize_t(index);
    if (position) returned = PyObject_CallFunctionObjArgs(work->function, coordinate, position, NULL);
    Py_XDECREF(coordinate); Py_XDECREF(position);
    if (returned == NULL) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    Py_DECREF(returned); /* Visit return values do not decide traversal. */
    return NUMFORGE_NUMERICAL_OK;
}
PyObject *nf_python_visit_grid(PyObject *module, PyObject *args)
{
    PyObject *items, *count_object, *function, *options;
    if (!PyArg_ParseTuple(args, "OOOO:visit_grid", &items, &count_object, &function, &options)) return NULL;
    size_t count = PyLong_AsSize_t(count_object);
    if (PyErr_Occurred()) return NULL;
    if (!PyCallable_Check(function)) { PyErr_SetString(PyExc_TypeError, "visit must be callable"); return NULL; }
    PyObject *values = PySequence_Fast(items, "grid bounds must be a sequence");
    if (values == NULL) return NULL;
    if (PySequence_Fast_GET_SIZE(values) != 2) {
        Py_DECREF(values); PyErr_SetString(PyExc_ValueError, "expected two grid bounds"); return NULL;
    }
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **entries = NULL; const BigRational **borrowed = NULL;
    NumForgeStatus status = nf_python_exact_inputs(values, 2, "grid bounds must be exact real values", &entries, &borrowed);
    GridCallback work = {module, function}; PyObject *output = NULL;
    if (status == NUMFORGE_OK && !PyErr_Occurred())
        status = numerical_status(numforge_grid_visit(entries[0], entries[1], count, grid_visit, &work));
    status = nf_python_budget_status(status);
    if (!PyErr_Occurred()) {
        if (status == NUMFORGE_OK) output = Py_NewRef(Py_None);
        else nf_python_raise_status(module, status, 0);
    }
    nf_python_exact_inputs_destroy(entries, borrowed, 2); Py_DECREF(values);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context);
    return output;
}

static PyObject *root_dictionary(PyObject *module, const NumForgeRoot *root)
{
    char *residual = NULL;
    NumForgeStatus status = numforge_root_residual(root) ?
        nf_python_decimal_status(bigdecimal_to_string(numforge_root_residual(root), &residual)) : NUMFORGE_OK;
    if (status != NUMFORGE_OK) { free(residual); return nf_python_raise_status(module, nf_python_budget_status(status), 0); }
    static const char *kinds[] = {"found", "not_bracketed", "domain_gap", "domain_unverified", "iteration_limit", "precision_limit"};
    PyObject *output = PyDict_New();
    if (output && (!nf_python_dictionary_put(output, "kind", PyUnicode_FromString(kinds[numforge_root_kind(root)])) ||
        !nf_python_dictionary_put(output, "x", nf_python_fraction_pair(module, numforge_root_x(root))) ||
        !nf_python_dictionary_put(output, "lower", nf_python_fraction_pair(module, numforge_root_lower(root))) ||
        !nf_python_dictionary_put(output, "upper", nf_python_fraction_pair(module, numforge_root_upper(root))) ||
        !nf_python_dictionary_put(output, "residual", residual ? PyUnicode_FromString(residual) : Py_NewRef(Py_None)) ||
        !nf_python_dictionary_put(output, "exact_zero", PyBool_FromLong(numforge_root_exact_zero(root))) ||
        !nf_python_dictionary_put(output, "iterations", PyLong_FromSize_t(numforge_root_iterations(root))) ||
        !nf_python_dictionary_put(output, "evaluations", PyLong_FromSize_t(numforge_root_evaluations(root))))) Py_CLEAR(output);
    free(residual);
    return output;
}
static PyObject *scan_dictionary(PyObject *module, const NumForgeRootScan *scan)
{
    size_t count = numforge_root_scan_count(scan);
    if (count > PY_SSIZE_T_MAX) return nf_python_raise_status(module, NUMFORGE_VALUE_TOO_LARGE, 0);
    PyObject *roots = PyTuple_New((Py_ssize_t)count), *output = NULL;
    if (roots == NULL) return NULL;
    for (size_t i = 0; i < count; ++i) {
        PyObject *root = root_dictionary(module, numforge_root_scan_root(scan, i));
        if (root == NULL) goto done;
        PyTuple_SET_ITEM(roots, (Py_ssize_t)i, root);
    }
    NumForgeRootScanStats stats; numforge_root_scan_stats(scan, &stats);
    static const char *kinds[] = {"finished", "root_limit", "evaluation_limit"};
    static const char *names[] = {"evaluations", "iterations", "scanned_segments", "domain_gaps",
        "rejected_brackets", "candidate_samples", "candidate_roots", "candidate_gaps"};
    const size_t values[] = {stats.evaluations, stats.iterations, stats.scanned_segments, stats.domain_gaps,
        stats.rejected_brackets, stats.candidate_samples, stats.candidate_roots, stats.candidate_gaps};
    output = PyDict_New();
    if (output && (!nf_python_dictionary_put(output, "kind", PyUnicode_FromString(kinds[numforge_root_scan_stop(scan)])) ||
        PyDict_SetItemString(output, "roots", roots) < 0)) Py_CLEAR(output);
    for (size_t i = 0; output && i < sizeof(values)/sizeof(values[0]); ++i)
        if (!nf_python_dictionary_put(output, names[i], PyLong_FromSize_t(values[i]))) Py_CLEAR(output);
done:
    Py_DECREF(roots);
    return output;
}
static PyObject *native_root_search(PyObject *module, PyObject *args, bool scanning)
{
    PyObject *function, *items, *settings, *options; const char *residual_text;
    if (!PyArg_ParseTuple(args, "OOsOO:root_search", &function, &items, &residual_text, &settings, &options)) return NULL;
    if (!PyCallable_Check(function) || !PyTuple_Check(settings) || PyTuple_Size(settings) != (scanning ? 7 : 4)) {
        PyErr_SetString(PyExc_TypeError, "invalid root callback/settings"); return NULL;
    }
    NumForgeRootOptions root_options; numforge_root_options_init(&root_options);
    root_options.decimal_places = PyLong_AsLongLong(PyTuple_GetItem(settings, 0));
    root_options.max_iterations = PyLong_AsSize_t(PyTuple_GetItem(settings, 1));
    root_options.max_evaluations = PyLong_AsSize_t(PyTuple_GetItem(settings, 2));
    int continuous = PyObject_IsTrue(PyTuple_GetItem(settings, 3));
    NumForgeRootScanOptions scan_options; numforge_root_scan_options_init(&scan_options);
    if (scanning) {
        scan_options.segments = PyLong_AsSize_t(PyTuple_GetItem(settings, 4));
        scan_options.max_roots = PyLong_AsSize_t(PyTuple_GetItem(settings, 5));
        scan_options.max_evaluations = PyLong_AsSize_t(PyTuple_GetItem(settings, 6));
    }
    if (PyErr_Occurred()) return NULL;
    PyObject *values = PySequence_Fast(items, "root bounds/tolerance must be a sequence");
    if (values == NULL) return NULL;
    if (PySequence_Fast_GET_SIZE(values) != 3) {
        Py_DECREF(values); PyErr_SetString(PyExc_ValueError, "expected two bounds and x tolerance"); return NULL;
    }
    NFPythonPolicy policy;
    if (!nf_python_policy_parse(module, options, &policy)) { Py_DECREF(values); return NULL; }
    bool owner = numforge_budget_begin(policy.milliseconds, policy.allocation_bytes, policy.single_allocation);
    BigRational **entries = NULL; const BigRational **borrowed = NULL;
    BigDecimal *tolerance = NULL; NumForgeRoot *root = NULL; NumForgeRootScan *scan = NULL;
    RootCallback work = {module, function, NULL, NUMFORGE_OK, continuous != 0};
    NumForgeStatus status = nf_python_exact_inputs(values, 3, "root bounds and x tolerance must be exact real values", &entries, &borrowed);
    PyObject *output = NULL;
    if (PyErr_Occurred()) goto done;
    if (status == NUMFORGE_OK) {
        tolerance = bigdecimal_create(); work.numerator = bigint_create();
        if (!tolerance || !work.numerator) status = NUMFORGE_OUT_OF_MEMORY;
    }
    if (status == NUMFORGE_OK) status = nf_python_decimal_status(bigdecimal_set_string(tolerance, residual_text));
    root_options.x_tolerance = entries ? entries[2] : NULL;
    root_options.residual_tolerance = tolerance;
    scan_options.root_options = root_options;
    if (status == NUMFORGE_OK) {
        NumForgeNumericalStatus solved = scanning ?
            numforge_roots_scan(&scan, root_callback, root_domain, &work, entries[0], entries[1], &scan_options) :
            numforge_root_solve(&root, root_sample, root_domain, &work, entries[0], entries[1], &root_options);
        status = solved == NUMFORGE_NUMERICAL_CALLBACK_FAILED && work.status != NUMFORGE_OK ?
            work.status : numerical_status(solved);
    }
    status = nf_python_budget_status(status);
    if (PyErr_Occurred()) goto done; /* Preserve callback exceptions, including KeyboardInterrupt. */
    if (status != NUMFORGE_OK) { nf_python_raise_status(module, status, 0); goto done; }
    output = scanning ? scan_dictionary(module, scan) : root_dictionary(module, root);
    status = nf_python_budget_status(NUMFORGE_OK);
    if (status != NUMFORGE_OK && !PyErr_Occurred()) { Py_CLEAR(output); nf_python_raise_status(module, status, 0); }
done:
    numforge_root_destroy(root); numforge_root_scan_destroy(scan);
    bigint_destroy(work.numerator); bigdecimal_destroy(tolerance);
    nf_python_exact_inputs_destroy(entries, borrowed, 3); Py_DECREF(values);
    if (owner) numforge_budget_end();
    numforge_context_destroy(policy.context);
    return output;
}
PyObject *nf_python_find_root(PyObject *module, PyObject *args)
{ return native_root_search(module, args, false); }
PyObject *nf_python_find_roots(PyObject *module, PyObject *args)
{ return native_root_search(module, args, true); }
