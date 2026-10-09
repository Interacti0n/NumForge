#include "complex_evaluator.h"
#include "exact_evaluator.h"
#include "evaluator.h"
#include "value_internal.h"
#include "exact_functions.h"
#include <numforge/runtime.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

CalculatorStatus calculator_from_complex_status(BigComplexStatus status)
{
    switch (status) {
        case BIGCOMPLEX_OK: return CALCULATOR_OK;
        case BIGCOMPLEX_NULL_ARGUMENT: return CALCULATOR_NULL_ARGUMENT;
        case BIGCOMPLEX_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case BIGCOMPLEX_DIVISION_BY_ZERO: return CALCULATOR_DIVISION_BY_ZERO;
        case BIGCOMPLEX_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case BIGCOMPLEX_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}
#define mapped calculator_from_complex_status
bool calculator_expression_has_complex(const CalculatorExpression *e, const CalculatorValue *answer)
{
    if (e == NULL) return false;
    switch (e->type) {
        case CALCULATOR_EXPRESSION_IMAGINARY: return true;
        case CALCULATOR_EXPRESSION_ANSWER: return calculator_value_is_complex(answer);
        case CALCULATOR_EXPRESSION_VARIABLE: return calculator_value_is_complex(e->data.variable.value);
        case CALCULATOR_EXPRESSION_UNARY: return calculator_expression_has_complex(e->data.unary.operand, answer);
        case CALCULATOR_EXPRESSION_POSTFIX: return calculator_expression_has_complex(e->data.postfix.operand, answer);
        case CALCULATOR_EXPRESSION_BINARY: return calculator_expression_has_complex(e->data.binary.left, answer) || calculator_expression_has_complex(e->data.binary.right, answer);
        case CALCULATOR_EXPRESSION_CALL:
            if (e->data.call.function->implementation == CALCULATOR_FUNCTION_COMPLEX ||
                e->data.call.function->implementation == CALCULATOR_FUNCTION_REAL_PART ||
                e->data.call.function->implementation == CALCULATOR_FUNCTION_IMAGINARY_PART ||
                e->data.call.function->implementation == CALCULATOR_FUNCTION_CONJUGATE ||
                e->data.call.function->implementation == CALCULATOR_FUNCTION_ARGUMENT) return true;
            for (size_t i=0; i<e->data.call.count; i++)
                if (calculator_expression_has_complex(e->data.call.arguments[i], answer)) return true;
            return false;
        default: return false;
    }
}
static bool exact(const CalculatorValue *v)
{
    return v->kind == CALCULATOR_VALUE_INTEGER || v->kind == CALCULATOR_VALUE_RATIONAL || v->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL;
}
static CalculatorStatus rational_part(BigRational *result, const CalculatorValue *v)
{
    BigRationalStatus s = v->kind == CALCULATOR_VALUE_INTEGER ? bigrational_from_bigint(result,v->integer) : bigrational_copy(result,v->rational);
    return s == BIGRATIONAL_OK ? CALCULATOR_OK : s == BIGRATIONAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
}
static CalculatorStatus real_decimal(BigDecimal *result, const CalculatorValue *v, const CalculatorContext *context)
{
    if (calculator_value_is_complex(v)) return CALCULATOR_INVALID_ARGUMENT;
    if (v->kind != CALCULATOR_VALUE_DECIMAL) return calculator_materialize_exact(result,v,context);
    BigDecimalStatus s=bigdecimal_copy(result,v->number);
    return s == BIGDECIMAL_OK ? CALCULATOR_OK : s == BIGDECIMAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_VALUE_TOO_LARGE;
}
static CalculatorStatus as_exact(BigRationalComplex *result, const CalculatorValue *v)
{
    if (v->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) return mapped(bigrationalcomplex_copy(result,v->complex_rational));
    BigRational *re=bigrational_create(), *im=bigrational_create();
    CalculatorStatus status=CALCULATOR_OUT_OF_MEMORY;
    if (re != NULL && im != NULL) {
        status=rational_part(re,v);
        if (status == CALCULATOR_OK) status=mapped(bigrationalcomplex_set_parts(result,re,im));
    }
    bigrational_destroy(re); bigrational_destroy(im); return status;
}
static CalculatorStatus as_decimal(BigComplex *result, const CalculatorValue *v, const CalculatorContext *context)
{
    if (v->kind == CALCULATOR_VALUE_COMPLEX_DECIMAL) return mapped(bigcomplex_copy(result,v->complex_decimal));
    if (v->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) return mapped(bigrationalcomplex_to_bigcomplex(result,v->complex_rational,context->division_scale,context->rounding));
    BigDecimal *re=bigdecimal_create();
    if (re == NULL) return CALCULATOR_OUT_OF_MEMORY;
    CalculatorStatus status=real_decimal(re,v,context);
    if (status == CALCULATOR_OK) status=mapped(bigcomplex_from_bigdecimal(result,re));
    bigdecimal_destroy(re); return status;
}
static CalculatorStatus construct(CalculatorValue *result, const CalculatorValue *re,
    const CalculatorValue *im, const CalculatorContext *context)
{
    if (calculator_value_is_complex(re) || calculator_value_is_complex(im) || re->quantity || im->quantity) return CALCULATOR_INVALID_ARGUMENT;
    CalculatorStatus status=CALCULATOR_OUT_OF_MEMORY;
    if (exact(re) && exact(im)) {
        BigRational *a=bigrational_create(), *b=bigrational_create();
        result->kind=CALCULATOR_VALUE_COMPLEX_RATIONAL;
        result->complex_rational=bigrationalcomplex_create();
        if (a != NULL && b != NULL && result->complex_rational != NULL) {
            status=rational_part(a,re);
            if (status == CALCULATOR_OK) status=rational_part(b,im);
            if (status == CALCULATOR_OK) status=mapped(bigrationalcomplex_set_parts(result->complex_rational,a,b));
        }
        bigrational_destroy(a); bigrational_destroy(b);
    } else {
        BigDecimal *a=bigdecimal_create(), *b=bigdecimal_create();
        result->kind=CALCULATOR_VALUE_COMPLEX_DECIMAL;
        result->complex_decimal=bigcomplex_create();
        if (a != NULL && b != NULL && result->complex_decimal != NULL) {
            status=real_decimal(a,re,context);
            if (status == CALCULATOR_OK) status=real_decimal(b,im,context);
            if (status == CALCULATOR_OK) status=mapped(bigcomplex_set_parts(result->complex_decimal,a,b));
        }
        bigdecimal_destroy(a); bigdecimal_destroy(b);
    }
    return status;
}
static CalculatorStatus exponent_value(const CalculatorValue *v, const CalculatorContext *context, int64_t *exponent)
{
    if (calculator_value_is_complex(v) || v->quantity) return CALCULATOR_INVALID_ARGUMENT;
    BigInt *integer=bigint_create(), *denominator=bigint_create();
    BigDecimal *decimal=bigdecimal_create();char *text=NULL;
    CalculatorStatus status=CALCULATOR_OUT_OF_MEMORY;
    if (integer == NULL || denominator == NULL || decimal == NULL) goto done;
    if (v->kind == CALCULATOR_VALUE_RATIONAL) {
        if (bigrational_get_denominator(denominator,v->rational) != BIGRATIONAL_OK) goto done;
        if (!bigint_is_one(denominator)) { status=CALCULATOR_INVALID_ARGUMENT;goto done; }
        if (bigrational_get_numerator(integer,v->rational) != BIGRATIONAL_OK) goto done;
    } else {
        status=real_decimal(decimal,v,context);if (status != CALCULATOR_OK) goto done;
        BigDecimalStatus ds=bigdecimal_to_bigint(integer,decimal);
        if (ds != BIGDECIMAL_OK) {status=ds == BIGDECIMAL_OUT_OF_MEMORY ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_INVALID_ARGUMENT;goto done;}
    }
    text=bigint_to_string(integer);if (text == NULL) {status=CALCULATOR_OUT_OF_MEMORY;goto done;}
    char *end;errno=0;long long parsed=strtoll(text,&end,10);
    if (errno != 0 || *end != '\0') {status=CALCULATOR_VALUE_TOO_LARGE;goto done;}
    *exponent=(int64_t)parsed;status=CALCULATOR_OK;
done:free(text);bigint_destroy(integer);bigint_destroy(denominator);bigdecimal_destroy(decimal);return status;
}
static CalculatorStatus arithmetic(CalculatorValue *r,const CalculatorValue *a,const CalculatorValue *b,
    CalculatorBinaryOperator op,const CalculatorContext *context)
{
    if (a->quantity || b->quantity) return CALCULATOR_DIMENSION_ERROR;
    int64_t exponent=0;
    CalculatorStatus status=op == CALCULATOR_BINARY_POWER ? exponent_value(b,context,&exponent) : CALCULATOR_OK;
    if (op == CALCULATOR_BINARY_POWER && status == CALCULATOR_INVALID_ARGUMENT) {
        BigComplex *x=bigcomplex_create(),*y=bigcomplex_create();
        r->kind=CALCULATOR_VALUE_COMPLEX_DECIMAL;r->complex_decimal=bigcomplex_create();
        status=CALCULATOR_OUT_OF_MEMORY;
        CalculatorContext working=*context;working.division_scale+=12;
        if (x && y && r->complex_decimal) {
            status=as_decimal(x,a,&working);
            if (status == CALCULATOR_OK) status=as_decimal(y,b,&working);
            if (status == CALCULATOR_OK) status=mapped(bigcomplex_pow(
                r->complex_decimal,x,y,context->division_scale,context->rounding));
        }
        bigcomplex_destroy(x);bigcomplex_destroy(y);return status;
    }
    if (status != CALCULATOR_OK) return status;
    bool use_exact=exact(a) && (op == CALCULATOR_BINARY_POWER || exact(b));
    status=CALCULATOR_OUT_OF_MEMORY;
    if (use_exact) {
        BigRationalComplex *x=bigrationalcomplex_create(), *y=bigrationalcomplex_create();
        r->kind=CALCULATOR_VALUE_COMPLEX_RATIONAL;r->complex_rational=bigrationalcomplex_create();
        if (x != NULL && y != NULL && r->complex_rational != NULL) {
            status=as_exact(x,a);
            if (status == CALCULATOR_OK && op != CALCULATOR_BINARY_POWER) status=as_exact(y,b);
            if (status == CALCULATOR_OK) {
                BigComplexStatus s;
                switch (op) {
                    case CALCULATOR_BINARY_ADD:s=bigrationalcomplex_add(r->complex_rational,x,y);break;
                    case CALCULATOR_BINARY_SUBTRACT:s=bigrationalcomplex_sub(r->complex_rational,x,y);break;
                    case CALCULATOR_BINARY_MULTIPLY:s=bigrationalcomplex_mul(r->complex_rational,x,y);break;
                    case CALCULATOR_BINARY_DIVIDE:s=bigrationalcomplex_div(r->complex_rational,x,y);break;
                    default:s=bigrationalcomplex_pow_int(r->complex_rational,x,exponent);break;
                }
                status=mapped(s);
            }
        }
        bigrationalcomplex_destroy(x);bigrationalcomplex_destroy(y);
    } else {
        BigComplex *x=bigcomplex_create(), *y=bigcomplex_create();
        r->kind=CALCULATOR_VALUE_COMPLEX_DECIMAL;r->complex_decimal=bigcomplex_create();
        if (x != NULL && y != NULL && r->complex_decimal != NULL) {
            status=as_decimal(x,a,context);
            if (status == CALCULATOR_OK && op != CALCULATOR_BINARY_POWER) status=as_decimal(y,b,context);
            if (status == CALCULATOR_OK) {
                BigComplexStatus s;
                switch (op) {
                    case CALCULATOR_BINARY_ADD:s=bigcomplex_add(r->complex_decimal,x,y);break;
                    case CALCULATOR_BINARY_SUBTRACT:s=bigcomplex_sub(r->complex_decimal,x,y);break;
                    case CALCULATOR_BINARY_MULTIPLY:s=bigcomplex_mul(r->complex_decimal,x,y);break;
                    case CALCULATOR_BINARY_DIVIDE:s=bigcomplex_div(r->complex_decimal,x,y,context->division_scale,context->rounding);break;
                    default:s=bigcomplex_pow_int(r->complex_decimal,x,exponent,context->division_scale,context->rounding);break;
                }
                status=mapped(s);
            }
        }
        bigcomplex_destroy(x);bigcomplex_destroy(y);
    }
    return status;
}
/* Borrow evaluated real operands for the existing scalar evaluator. This also
 * permits sin(re(z)), sum(re(z);im(z)), etc. without evaluating children twice. */
static CalculatorStatus real_operation(CalculatorValue *result, const CalculatorExpression *expression,
    const CalculatorValue *a, const CalculatorValue *b, const CalculatorContext *context,
    const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error)
{
    CalculatorExpression operation=*expression, left={0},right={0};
    left.type=right.type=CALCULATOR_EXPRESSION_VARIABLE;
    left.data.variable.value=a;right.data.variable.value=b;
    if (expression->type == CALCULATOR_EXPRESSION_BINARY) {
        operation.data.binary.left=&left;operation.data.binary.right=&right;
    } else if (expression->type == CALCULATOR_EXPRESSION_UNARY) operation.data.unary.operand=&left;
    else operation.data.postfix.operand=&left;
    return calculator_evaluate_complex(result,&operation,context,answer,random_state,error);
}
/* A rational principal root exists exactly when both computed components
 * have proven rational square roots. NOT_IMPLEMENTED requests decimal fallback. */
static CalculatorStatus exact_square_root(BigRationalComplex **result,const BigRationalComplex *value)
{
    BigRational *squared=bigrational_create(),*radius=bigrational_create(),*re=bigrational_create();
    BigRational *im=bigrational_create(),*u=bigrational_create(),*v=bigrational_create(),*two=bigrational_create();
    BigInt *integer=bigint_create();BigRationalComplex *root=bigrationalcomplex_create();
    CalculatorStatus status=CALCULATOR_OUT_OF_MEMORY;
    if (!squared || !radius || !re || !im || !u || !v || !two || !integer || !root) goto done;
    status=mapped(bigrationalcomplex_abs_squared(squared,value));
    if (status == CALCULATOR_OK) status=calculator_exact_sqrt(radius,squared);
    if (status == CALCULATOR_OK) status=mapped(bigrationalcomplex_get_real(re,value));
    if (status == CALCULATOR_OK) status=mapped(bigrationalcomplex_get_imaginary(im,value));
    if (status == CALCULATOR_OK) status=calculator_from_integer_status(bigint_set_string(integer,"2"));
    if (status == CALCULATOR_OK) status=calculator_from_rational_status(bigrational_from_bigint(two,integer));
    if (status == CALCULATOR_OK) status=calculator_from_rational_status(bigrational_add(u,radius,re));
    if (status == CALCULATOR_OK) status=calculator_from_rational_status(bigrational_div(u,u,two));
    if (status == CALCULATOR_OK) status=calculator_exact_sqrt(u,u);
    if (status == CALCULATOR_OK) status=calculator_from_rational_status(bigrational_sub(v,radius,re));
    if (status == CALCULATOR_OK) status=calculator_from_rational_status(bigrational_div(v,v,two));
    if (status == CALCULATOR_OK) status=calculator_exact_sqrt(v,v);
    if (status == CALCULATOR_OK) status=calculator_from_rational_status(bigrational_get_numerator(integer,im));
    if (status == CALCULATOR_OK && bigint_is_negative(integer)) status=calculator_from_rational_status(bigrational_negate(v,v));
    if (status == CALCULATOR_OK) status=mapped(bigrationalcomplex_set_parts(root,u,v));
    if (status == CALCULATOR_OK) {*result=root;root=NULL;}
done:
    bigrational_destroy(squared);bigrational_destroy(radius);bigrational_destroy(re);bigrational_destroy(im);
    bigrational_destroy(u);bigrational_destroy(v);bigrational_destroy(two);bigint_destroy(integer);
    bigrationalcomplex_destroy(root);return status;
}
typedef BigComplexStatus (*ComplexUnaryOperation)(BigComplex *,const BigComplex *,int64_t,BigDecimalRoundingMode);
static ComplexUnaryOperation complex_unary_operation(CalculatorFunctionImplementation function)
{
    switch (function) {
        case CALCULATOR_FUNCTION_EXP: return bigcomplex_exp;
        case CALCULATOR_FUNCTION_LN: return bigcomplex_ln;
        case CALCULATOR_FUNCTION_SQRT: return bigcomplex_sqrt;
        case CALCULATOR_FUNCTION_SIN: return bigcomplex_sin;
        case CALCULATOR_FUNCTION_COS: return bigcomplex_cos;
        case CALCULATOR_FUNCTION_TAN: return bigcomplex_tan;
        case CALCULATOR_FUNCTION_SINH: return bigcomplex_sinh;
        case CALCULATOR_FUNCTION_COSH: return bigcomplex_cosh;
        case CALCULATOR_FUNCTION_TANH: return bigcomplex_tanh;
        case CALCULATOR_FUNCTION_ASIN: return bigcomplex_asin;
        case CALCULATOR_FUNCTION_ACOS: return bigcomplex_acos;
        case CALCULATOR_FUNCTION_ATAN: return bigcomplex_atan;
        default: return NULL;
    }
}
static CalculatorStatus complex_function(CalculatorValue *result, const CalculatorValue *a,
    CalculatorFunctionImplementation function, const CalculatorContext *context)
{
    if (a->quantity) return CALCULATOR_DIMENSION_ERROR;
    CalculatorStatus status=CALCULATOR_OUT_OF_MEMORY;
    if (function == CALCULATOR_FUNCTION_SQRT && a->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) {
        status=exact_square_root(&result->complex_rational,a->complex_rational);
        if (status == CALCULATOR_OK) {result->kind=CALCULATOR_VALUE_COMPLEX_RATIONAL;return status;}
        if (status != CALCULATOR_NOT_IMPLEMENTED) return status;
    }
    if (function == CALCULATOR_FUNCTION_REAL_PART || function == CALCULATOR_FUNCTION_IMAGINARY_PART) {
        if (!calculator_value_is_complex(a)) {
            if (function == CALCULATOR_FUNCTION_REAL_PART) return calculator_value_copy(result,a);
            result->kind=CALCULATOR_VALUE_RATIONAL;result->rational=bigrational_create();
            return result->rational ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
        }
        if (a->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) {
            result->kind=CALCULATOR_VALUE_RATIONAL;result->rational=bigrational_create();
            if (result->rational) status=mapped(function == CALCULATOR_FUNCTION_REAL_PART ?
                bigrationalcomplex_get_real(result->rational,a->complex_rational) :
                bigrationalcomplex_get_imaginary(result->rational,a->complex_rational));
        } else {
            result->number=bigdecimal_create();
            if (result->number) status=mapped(function == CALCULATOR_FUNCTION_REAL_PART ?
                bigcomplex_get_real(result->number,a->complex_decimal) :
                bigcomplex_get_imaginary(result->number,a->complex_decimal));
        }
        return status;
    }
    if (function == CALCULATOR_FUNCTION_CONJUGATE) {
        if (!calculator_value_is_complex(a)) return calculator_value_copy(result,a);
        result->kind=a->kind;
        if (a->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) {
            result->complex_rational=bigrationalcomplex_create();
            if (result->complex_rational) status=mapped(bigrationalcomplex_conjugate(result->complex_rational,a->complex_rational));
        } else {
            result->complex_decimal=bigcomplex_create();
            if (result->complex_decimal) status=mapped(bigcomplex_conjugate(result->complex_decimal,a->complex_decimal));
        }
        return status;
    }
    if (function == CALCULATOR_FUNCTION_ABS && a->kind == CALCULATOR_VALUE_COMPLEX_RATIONAL) {
        BigRational *squared=bigrational_create();result->rational=bigrational_create();
        if (!squared || !result->rational) {bigrational_destroy(squared);return CALCULATOR_OUT_OF_MEMORY;}
        status=mapped(bigrationalcomplex_abs_squared(squared,a->complex_rational));
        if (status == CALCULATOR_OK) status=calculator_exact_sqrt(result->rational,squared);
        if (status == CALCULATOR_OK) result->kind=CALCULATOR_VALUE_RATIONAL;
        else if (status == CALCULATOR_NOT_IMPLEMENTED) {
            bigrational_destroy(result->rational);result->rational=NULL;
            result->number=bigdecimal_create();
            status=result->number ? calculator_from_rational_status(bigrational_to_bigdecimal(
                result->number,squared,context->division_scale+12,context->rounding)) : CALCULATOR_OUT_OF_MEMORY;
            if (status == CALCULATOR_OK) status=calculator_from_decimal_status(bigdecimal_sqrt(
                result->number,result->number,context->division_scale,context->rounding));
        }
        bigrational_destroy(squared);return status;
    }
    BigComplex *projected=bigcomplex_create();
    if (!projected) return CALCULATOR_OUT_OF_MEMORY;
    CalculatorContext working=*context;working.division_scale+=12;
    status=as_decimal(projected,a,&working);
    ComplexUnaryOperation operation=complex_unary_operation(function);
    if (status == CALCULATOR_OK && operation != NULL) {
        result->kind=CALCULATOR_VALUE_COMPLEX_DECIMAL;result->complex_decimal=bigcomplex_create();
        status=result->complex_decimal ? mapped(operation(result->complex_decimal,projected,
            context->division_scale,context->rounding)) : CALCULATOR_OUT_OF_MEMORY;
    } else if (status == CALCULATOR_OK) {
        result->number=bigdecimal_create();
        status=result->number ? mapped(function == CALCULATOR_FUNCTION_ABS ?
            bigcomplex_abs(result->number,projected,context->division_scale,context->rounding) :
            bigcomplex_arg(result->number,projected,context->division_scale,context->rounding)) : CALCULATOR_OUT_OF_MEMORY;
    }
    bigcomplex_destroy(projected);return status;
}
static CalculatorStatus complex_call(CalculatorValue *result, const CalculatorExpression *expression,
    const CalculatorContext *context, const CalculatorValue *answer,uint64_t *random_state,CalculatorError *error)
{
    size_t count=expression->data.call.count;
    CalculatorValue *values=numforge_calloc(count,sizeof(*values));
    CalculatorExpression *bindings=numforge_calloc(count,sizeof(*bindings));
    CalculatorExpression **children=numforge_calloc(count,sizeof(*children));
    CalculatorStatus status=values && bindings && children ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    bool any_complex=false;
    for (size_t i=0;status == CALCULATOR_OK && i<count;i++) {
        status=calculator_evaluate_complex(&values[i],expression->data.call.arguments[i],context,answer,random_state,error);
        any_complex=any_complex || calculator_value_is_complex(&values[i]);
        bindings[i].type=CALCULATOR_EXPRESSION_VARIABLE;
        bindings[i].offset=expression->data.call.arguments[i]->offset;
        bindings[i].data.variable.value=&values[i];children[i]=&bindings[i];
    }
    if (status != CALCULATOR_OK) goto done;
    CalculatorFunctionImplementation function=expression->data.call.function->implementation;
    if (function == CALCULATOR_FUNCTION_COMPLEX) status=construct(result,&values[0],&values[1],context);
    else if (function == CALCULATOR_FUNCTION_REAL_PART || function == CALCULATOR_FUNCTION_IMAGINARY_PART ||
        function == CALCULATOR_FUNCTION_CONJUGATE || function == CALCULATOR_FUNCTION_ARGUMENT ||
        (any_complex && (function == CALCULATOR_FUNCTION_ABS || complex_unary_operation(function) != NULL)))
        status=complex_function(result,&values[0],function,context);
    else if (function == CALCULATOR_FUNCTION_LOG && any_complex) {
        if (values[0].quantity || (count == 2 && values[1].quantity)) {
            status=CALCULATOR_DIMENSION_ERROR;goto done;
        }
        BigComplex *value=bigcomplex_create(),*base=bigcomplex_create();
        result->kind=CALCULATOR_VALUE_COMPLEX_DECIMAL;result->complex_decimal=bigcomplex_create();
        status=CALCULATOR_OUT_OF_MEMORY;
        CalculatorContext working=*context;working.division_scale+=12;
        if (value && base && result->complex_decimal) {
            status=as_decimal(value,&values[0],&working);
            if (status == CALCULATOR_OK) status=count == 1 ? mapped(bigcomplex_set_strings(base,"10","0")) :
                as_decimal(base,&values[1],&working);
            if (status == CALCULATOR_OK) status=mapped(bigcomplex_log(
                result->complex_decimal,value,base,context->division_scale,context->rounding));
        }
        bigcomplex_destroy(value);bigcomplex_destroy(base);
    }
    else if (function == CALCULATOR_FUNCTION_POWER && any_complex) {
        const CalculatorExpression *base=expression->data.call.arguments[0];
        if (calculator_value_is_complex(&values[1]) && base->type == CALCULATOR_EXPRESSION_CONSTANT &&
            base->data.constant.constant == CALCULATOR_CONSTANT_E)
            status=complex_function(result,&values[1],CALCULATOR_FUNCTION_EXP,context);
        else status=arithmetic(result,&values[0],&values[1],CALCULATOR_BINARY_POWER,context);
    } else if (any_complex) status=CALCULATOR_INVALID_ARGUMENT;
    else {
        CalculatorExpression operation=*expression;operation.data.call.arguments=children;
        /* Special projection calls have been handled above, so this borrowed
         * call contains no complex nodes and follows the ordinary evaluator. */
        status=calculator_evaluate_complex(result,&operation,context,answer,random_state,error);
    }
done:
    if (values) for(size_t i=0;i<count;i++) calculator_value_destroy(&values[i]);
    free(values);free(bindings);free(children);return status;
}
CalculatorStatus calculator_evaluate_complex(CalculatorValue *result,
    const CalculatorExpression *e,const CalculatorContext *context,
    const CalculatorValue *answer,uint64_t *random_state,CalculatorError *error)
{
    CalculatorValue a={0},b={0};CalculatorStatus status=CALCULATOR_OK;
    result->context=*context;
    if (!calculator_expression_has_complex(e,answer)) {
        result->number=bigdecimal_create();if (result->number == NULL) {status=CALCULATOR_OUT_OF_MEMORY;goto done;}
        if (context->significant_division && calculator_exact_supported(e,answer)) {
            status=calculator_evaluate_exact(&result->rational,e,answer,error);
            if (status == CALCULATOR_OK) {result->kind=CALCULATOR_VALUE_RATIONAL;status=calculator_materialize_exact(result->number,result,context);goto done;}
            if (status != CALCULATOR_NOT_IMPLEMENTED) goto done;
            status=CALCULATOR_OK;
            calculator_error_clear(error);
        }
        BigDecimal *answer_decimal=bigdecimal_create();if (answer_decimal == NULL) {status=CALCULATOR_OUT_OF_MEMORY;goto done;}
        if (answer != NULL && !calculator_value_is_complex(answer)) status=real_decimal(answer_decimal,answer,context);
        if (status == CALCULATOR_OK) status=calculator_evaluate_with_answer(result->number,e,context,
            answer != NULL && !calculator_value_is_complex(answer) ? answer_decimal : NULL,answer,random_state,error);
        bigdecimal_destroy(answer_decimal);goto done;
    }
    if (e->type == CALCULATOR_EXPRESSION_VARIABLE || e->type == CALCULATOR_EXPRESSION_ANSWER) {
        status=calculator_value_copy(result,e->type == CALCULATOR_EXPRESSION_VARIABLE ? e->data.variable.value : answer);goto done;
    }
    if (e->type == CALCULATOR_EXPRESSION_IMAGINARY) {
        a.kind=b.kind=CALCULATOR_VALUE_INTEGER;a.integer=bigint_create();b.integer=bigint_create();
        status=a.integer && b.integer ? calculator_from_integer_status(bigint_set_string(b.integer,"1")) : CALCULATOR_OUT_OF_MEMORY;
        if (status == CALCULATOR_OK) status=construct(result,&a,&b,context);
        goto finish;
    }
    if (e->type == CALCULATOR_EXPRESSION_CALL) {
        status=complex_call(result,e,context,answer,random_state,error);goto finish;
    }
    const CalculatorExpression *left=NULL,*right=NULL;
    CalculatorBinaryOperator op=CALCULATOR_BINARY_ADD;
    if (e->type == CALCULATOR_EXPRESSION_BINARY) {
        left=e->data.binary.left;right=e->data.binary.right;op=e->data.binary.operation;
    } else if (e->type == CALCULATOR_EXPRESSION_UNARY) left=e->data.unary.operand;
    else if (e->type == CALCULATOR_EXPRESSION_POSTFIX) left=e->data.postfix.operand;
    else {status=CALCULATOR_INVALID_ARGUMENT;goto done;}
    status=calculator_evaluate_complex(&a,left,context,answer,random_state,error);if (status != CALCULATOR_OK) goto done;
    if (right != NULL) {status=calculator_evaluate_complex(&b,right,context,answer,random_state,error);if (status != CALCULATOR_OK) goto done;}
    if (!calculator_value_is_complex(&a) && (right == NULL || !calculator_value_is_complex(&b)))
        status=real_operation(result,e,&a,&b,context,answer,random_state,error);
    else if (e->type == CALCULATOR_EXPRESSION_BINARY && op == CALCULATOR_BINARY_POWER &&
        calculator_value_is_complex(&b) && left->type == CALCULATOR_EXPRESSION_CONSTANT &&
        left->data.constant.constant == CALCULATOR_CONSTANT_E)
        status=complex_function(result,&b,CALCULATOR_FUNCTION_EXP,context);
    else if (e->type == CALCULATOR_EXPRESSION_UNARY) {
        if (e->data.unary.operation == CALCULATOR_UNARY_PLUS) status=calculator_value_copy(result,&a);
        else {
            b.kind=CALCULATOR_VALUE_INTEGER;b.integer=bigint_create();
            status=b.integer == NULL ? CALCULATOR_OUT_OF_MEMORY : arithmetic(result,&b,&a,CALCULATOR_BINARY_SUBTRACT,context);
        }
    } else if (e->type == CALCULATOR_EXPRESSION_POSTFIX) {
        if (e->data.postfix.operation == CALCULATOR_POSTFIX_FACTORIAL) {status=CALCULATOR_INVALID_ARGUMENT;goto done;}
        b.kind=CALCULATOR_VALUE_INTEGER;b.integer=bigint_create();
        if (b.integer == NULL || bigint_set_string(b.integer,e->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ? "2" : "3") != BIGINT_OK) status=CALCULATOR_OUT_OF_MEMORY;
        else status=arithmetic(result,&a,&b,CALCULATOR_BINARY_POWER,context);
    } else status=arithmetic(result,&a,&b,op,context);
finish:
    if (status == CALCULATOR_OK && result->number == NULL) {
        result->number=bigdecimal_create();if (result->number == NULL) status=CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK && (result->kind == CALCULATOR_VALUE_INTEGER || result->kind == CALCULATOR_VALUE_RATIONAL))
        status=calculator_materialize_exact(result->number,result,context);
done:
    calculator_value_destroy(&a);calculator_value_destroy(&b);
    if (status != CALCULATOR_OK) {
        calculator_value_destroy(result);
        if (error == NULL || error->status != status) calculator_error_set(error,status,e->offset);
    }
    return status;
}
