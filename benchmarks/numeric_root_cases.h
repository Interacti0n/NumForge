#ifndef NUMFORGE_NUMERIC_ROOT_CASES_H
#define NUMFORGE_NUMERIC_ROOT_CASES_H

/* References are analytic values or independently published constants, never
 * solver output. A NULL reference means a deliberately non-found diagnostic. */
typedef struct NumericRootCase {
    const char *name, *equation, *lower, *upper, *reference;
    ApplicationNumericRootKind outcome;
} NumericRootCase;
static const NumericRootCase numeric_root_cases[] = {
    {"cos_fixed", "cos(x)=x", "0", "1", "0.739085133215160641655312087673873404", APPLICATION_ROOT_FOUND},
    {"log_two", "exp(x)=2", "0", "2", "0.693147180559945309417232121458176568", APPLICATION_ROOT_FOUND},
    {"log_inverse", "ln(x)=1", "1", "4", "2.718281828459045235360287471352662498", APPLICATION_ROOT_FOUND},
    {"cubic", "x^3=2", "0", "2", "1.259921049894873164767210607278228351", APPLICATION_ROOT_FOUND},
    {"flat_cubic", "(x-0.1234)^3=0", "0", "1", "0.1234", APPLICATION_ROOT_FOUND},
    {"steep_linear", "1E20*(x-0.1234)=0", "0", "1", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"tiny_linear", "1E-100*(x-0.1234)=0", "0", "1", "0.1234", APPLICATION_ROOT_FOUND},
    {"large_offset", "x=1E100", "0", "2E100", "1E100", APPLICATION_ROOT_FOUND},
    {"large_cancellation", "x+1E100=1E100+1/3", "0", "1", "0.333333333333333333333333333333333333", APPLICATION_ROOT_FOUND},
    {"wide_linear", "x=1/3", "-1E30", "1E30", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"narrow_linear", "x=0.1234", "0.123399999999999", "0.123400000000002", "0.1234", APPLICATION_ROOT_FOUND},
    {"sqrt_boundary", "sqrt(x)=0.1", "0", "1", "0.01", APPLICATION_ROOT_FOUND},
    {"rapid_sine", "sin(1000*x)=0", "0.003", "0.0033", "0.003141592653589793238462643383279503", APPLICATION_ROOT_FOUND},
    {"steep_exp", "exp(100*x)=2", "0", "1", "0.006931471805599453094172321214581766", APPLICATION_ROOT_FOUND},
    {"huge_exp_target", "exp(x)=1E100", "0", "300", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"log_near_boundary", "ln(x)=-100", "1E-50", "1", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"multiple_roots", "(x-1)*(x-2)*(x-3)=0", "0", "4", "2", APPLICATION_ROOT_FOUND},
    {"absolute_corner", "abs(x-0.1234)=x", "0", "1", "0.0617", APPLICATION_ROOT_FOUND},
    {"nested_log_pole", "1/ln(ln(sqrt(x)))=0", "2", "10", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"tangent", "(x-0.1234)^2=0", "0", "1", NULL, APPLICATION_ROOT_NOT_BRACKETED},
    {"positive", "x^2+1=0", "-1", "1", NULL, APPLICATION_ROOT_NOT_BRACKETED},
    {"pole_sampled", "1/x=0", "-1", "1", NULL, APPLICATION_ROOT_DOMAIN_GAP},
    {"pole_hidden", "1/(x-0.1234)=0", "0", "1", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"pole_scaled", "1E-100/(x-0.1234)=0", "0", "1", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"jump", "floor(x)=0.5", "-1", "1", NULL, APPLICATION_ROOT_ITERATION_LIMIT},
    {"plateau", "round(sin(x);0)=0", "-1", "1", NULL, APPLICATION_ROOT_PRECISION_LIMIT},
    {"invalid_domain", "sqrt(x)=2", "-1", "5", NULL, APPLICATION_ROOT_DOMAIN_GAP}
};
#endif
