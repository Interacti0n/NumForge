#include "fast_expression_internal.h"
#include "exact_functions.h"
#include "value_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/runtime.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FAST_INSTRUCTIONS 4096U
typedef enum FastOp { F_CONSTANT,F_X,F_NEGATE,F_SQUARE,F_CUBE,
    F_ADD,F_SUBTRACT,F_MULTIPLY,F_DIVIDE,F_POWER,
    F_SIN,F_COS,F_TAN,F_EXP,F_LN,F_SQRT,F_CBRT,F_ABS } FastOp;
typedef struct FastInstruction { FastOp operation;double value;double error; } FastInstruction;
struct ApplicationFastExpression {
    FastInstruction *code;size_t count;CalculatorContext context;
};
typedef struct FastCell { double value,error;ApplicationFastKind kind; } FastCell;

void application_fast_expression_destroy(ApplicationFastExpression *expression)
{if(expression){free(expression->code);free(expression);}}

static CalculatorStatus emit(ApplicationFastExpression *program,FastOp operation,double value,double error)
{
    if(program->count>=FAST_INSTRUCTIONS)return CALCULATOR_VALUE_TOO_LARGE;
    program->code[program->count++]=(FastInstruction){operation,value,error};return CALCULATOR_OK;
}
static CalculatorStatus decimal_constant(ApplicationFastExpression *program,const BigDecimal *value)
{
    char *text=NULL;CalculatorStatus status=calculator_from_decimal_status(bigdecimal_format_mode(value,
        16,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,128U,&text));
    if(status!=CALCULATOR_OK)return status;
    char *end=NULL;errno=0;double number=strtod(text,&end);
    bool usable=errno!=ERANGE&&end!=text&&*end=='\0'&&isfinite(number);
    free(text);if(!usable)return CALCULATOR_INVALID_ARGUMENT;
    return emit(program,F_CONSTANT,number,fabs(number)*4*DBL_EPSILON);
}
static CalculatorStatus typed_constant(ApplicationFastExpression *program,const CalculatorValue *value)
{
    if(value==NULL||value->number==NULL||calculator_value_is_complex(value)||value->quantity)return CALCULATOR_INVALID_ARGUMENT;
    BigDecimal *number=bigdecimal_create();if(!number)return CALCULATOR_OUT_OF_MEMORY;
    CalculatorStatus status=calculator_materialize_exact(number,value,&program->context);
    if(status==CALCULATOR_OK)status=decimal_constant(program,number);
    bigdecimal_destroy(number);return status;
}
static CalculatorStatus compile_node(ApplicationFastExpression *program,const CalculatorExpression *node,
    const CalculatorValue *parameter,const CalculatorValue *answer,CalculatorError *error)
{
    if(!numforge_budget_check())return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    CalculatorStatus status=CALCULATOR_OK;FastOp operation=F_CONSTANT;
    switch(node->type){
    case CALCULATOR_EXPRESSION_NUMBER:{
        char *end=NULL;errno=0;double value=strtod(node->data.number.text,&end);
        if(errno==ERANGE||!isfinite(value)||end==node->data.number.text||*end!='\0')status=CALCULATOR_INVALID_ARGUMENT;
        else {
            bool integer=true;for(const char *p=node->data.number.text;*p;++p)if(*p<'0'||*p>'9')integer=false;
            status=emit(program,F_CONSTANT,value,integer&&strlen(node->data.number.text)<=15U?0:fabs(value)*4*DBL_EPSILON);
        }break;
    }
    case CALCULATOR_EXPRESSION_CONSTANT:{
        BigDecimal *value=bigdecimal_create();if(!value)return CALCULATOR_OUT_OF_MEMORY;
        status=calculator_constant_set_value(value,node->data.constant.constant,34,BIGDECIMAL_ROUND_HALF_EVEN);
        if(status==CALCULATOR_OK)status=decimal_constant(program,value);
        bigdecimal_destroy(value);
        break;
    }
    case CALCULATOR_EXPRESSION_VARIABLE:
        status=node->data.variable.value==parameter?emit(program,F_X,0,0):typed_constant(program,node->data.variable.value);
        break;
    case CALCULATOR_EXPRESSION_ANSWER:status=typed_constant(program,answer);
        break;
    case CALCULATOR_EXPRESSION_UNARY:
        status=compile_node(program,node->data.unary.operand,parameter,answer,error);
        if(status==CALCULATOR_OK&&node->data.unary.operation==CALCULATOR_UNARY_MINUS)status=emit(program,F_NEGATE,0,0);
        break;
    case CALCULATOR_EXPRESSION_POSTFIX:
        if(node->data.postfix.operation==CALCULATOR_POSTFIX_FACTORIAL){status=CALCULATOR_INVALID_ARGUMENT;
        break;}
        status=compile_node(program,node->data.postfix.operand,parameter,answer,error);
        if(status==CALCULATOR_OK)status=emit(program,node->data.postfix.operation==CALCULATOR_POSTFIX_SQUARE?F_SQUARE:F_CUBE,0,0);
        break;
    case CALCULATOR_EXPRESSION_BINARY:
        status=compile_node(program,node->data.binary.left,parameter,answer,error);
        if(status==CALCULATOR_OK)status=compile_node(program,node->data.binary.right,parameter,answer,error);
        if(status==CALCULATOR_OK)status=emit(program,(FastOp)(F_ADD+node->data.binary.operation),0,0);
        break;
    case CALCULATOR_EXPRESSION_CALL:{
        CalculatorFunctionImplementation function=node->data.call.function->implementation;
        switch(function){
        case CALCULATOR_FUNCTION_SIN:operation=F_SIN;
        break;case CALCULATOR_FUNCTION_COS:operation=F_COS;
        break;
        case CALCULATOR_FUNCTION_TAN:operation=F_TAN;
        break;case CALCULATOR_FUNCTION_EXP:operation=F_EXP;
        break;
        case CALCULATOR_FUNCTION_LN:operation=F_LN;
        break;case CALCULATOR_FUNCTION_SQRT:operation=F_SQRT;
        break;
        case CALCULATOR_FUNCTION_CBRT:operation=F_CBRT;
        break;case CALCULATOR_FUNCTION_ABS:operation=F_ABS;
        break;
        case CALCULATOR_FUNCTION_POWER:operation=F_POWER;
        break;
        default:status=CALCULATOR_INVALID_ARGUMENT;
        break;
        }
        if(status==CALCULATOR_OK&&node->data.call.count!=(operation==F_POWER?2U:1U))status=CALCULATOR_INVALID_ARGUMENT;
        for(size_t i=0;status==CALCULATOR_OK&&i<node->data.call.count;++i)status=compile_node(program,node->data.call.arguments[i],parameter,answer,error);
        if(status==CALCULATOR_OK)status=emit(program,operation,0,0);
        break;
    }
    default:status=CALCULATOR_INVALID_ARGUMENT;
        break;
    }
    if(status!=CALCULATOR_OK)calculator_error_set(error,status,node->offset);
    return status;
}
CalculatorStatus application_fast_compile_tree(ApplicationFastExpression **result,const CalculatorExpression *tree,
    const CalculatorValue *parameter,const CalculatorValue *answer,const CalculatorContext *context,CalculatorError *error)
{
    calculator_error_clear(error);
    if(!result||!tree||!context){calculator_error_set(error,CALCULATOR_NULL_ARGUMENT,0U);return CALCULATOR_NULL_ARGUMENT;}
    bool owner=numforge_budget_begin((uint64_t)context->time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    ApplicationFastExpression *program=numforge_calloc(1U,sizeof(*program));
    CalculatorStatus status=program?CALCULATOR_OK:CALCULATOR_OUT_OF_MEMORY;
    if(program){program->context=*context;program->code=numforge_calloc(FAST_INSTRUCTIONS,sizeof(*program->code));if(!program->code)status=CALCULATOR_OUT_OF_MEMORY;}
    if(status==CALCULATOR_OK)status=compile_node(program,tree,parameter,answer,error);
    status=calculator_budget_status(status);
    if(status==CALCULATOR_OK)*result=program;else{application_fast_expression_destroy(program);calculator_error_set(error,status,error?error->offset:0U);}
    if(owner)numforge_budget_end();
    return status;
}

/* Error propagation is a conservative fallback heuristic, not certified interval
 * arithmetic. Libm values are only for experimental screen-space plotting. */
static ApplicationFastSample evaluate(const ApplicationFastExpression *program,double x)
{
    FastCell stack[CALCULATOR_MAX_EXPRESSION_DEPTH+1U];size_t used=0;
    for(size_t i=0;i<program->count;++i){
        FastInstruction op=program->code[i];FastCell a={0,0,APPLICATION_FAST_REAL},b=a,r=a;
        if(op.operation==F_CONSTANT||op.operation==F_X){
            if(used>=CALCULATOR_MAX_EXPRESSION_DEPTH+1U)return (ApplicationFastSample){0,APPLICATION_FAST_UNCERTAIN};
            stack[used++]=(FastCell){op.operation==F_X?x:op.value,op.operation==F_X?fabs(x)*4*DBL_EPSILON:op.error,APPLICATION_FAST_REAL};continue;
        }
        if(!used)return (ApplicationFastSample){0,APPLICATION_FAST_UNCERTAIN};
        a=stack[--used];
        bool binary=op.operation>=F_ADD&&op.operation<=F_POWER;
        if(binary){if(!used)return (ApplicationFastSample){0,APPLICATION_FAST_UNCERTAIN};b=a;a=stack[--used];}
        // A complex intermediate can become real again (e.g. abs(sqrt(-1))).
        // Only the exact evaluator may resolve such composed expressions.
        if(a.kind!=APPLICATION_FAST_REAL||b.kind!=APPLICATION_FAST_REAL){r.kind=APPLICATION_FAST_UNCERTAIN;stack[used++]=r;continue;}
        double av=a.value,bv=b.value;
        switch(op.operation){
        case F_NEGATE:r.value=-av;r.error=a.error;
        break;
        case F_SQUARE:r.value=av*av;r.error=2*fabs(av)*a.error;if(r.value==0&&av!=0)r.kind=APPLICATION_FAST_UNCERTAIN;
        break;
        case F_CUBE:r.value=av*av*av;r.error=3*av*av*a.error;if(r.value==0&&av!=0)r.kind=APPLICATION_FAST_UNCERTAIN;
        break;
        case F_ADD:r.value=av+bv;r.error=a.error+b.error+4*DBL_EPSILON*(fabs(av)+fabs(bv));
        break;
        case F_SUBTRACT:r.value=av-bv;r.error=a.error+b.error+4*DBL_EPSILON*(fabs(av)+fabs(bv));
        break;
        case F_MULTIPLY:r.value=av*bv;r.error=fabs(bv)*a.error+fabs(av)*b.error;if(r.value==0&&av!=0&&bv!=0)r.kind=APPLICATION_FAST_UNCERTAIN;
        break;
        case F_DIVIDE:
            if(fabs(bv)<=8*b.error){r.kind=APPLICATION_FAST_UNCERTAIN;
        break;}
            r.value=av/bv;r.error=(a.error+fabs(r.value)*b.error)/fabs(bv);if(r.value==0&&av!=0)r.kind=APPLICATION_FAST_UNCERTAIN;
        break;
        case F_POWER:
            if(av<=0&&(bv!=trunc(bv)||b.error!=0)){r.kind=APPLICATION_FAST_UNCERTAIN;
        break;}
            r.value=pow(av,bv);r.error=av!=0?fabs(bv*r.value/av)*a.error:0;
            if(av>0)r.error+=fabs(r.value*log(av))*b.error;
            if(r.value==0&&av!=0)r.kind=APPLICATION_FAST_UNCERTAIN;
        break;
        case F_SIN:case F_COS:case F_TAN:{
            double factor=program->context.angle_unit==CALCULATOR_ANGLE_DEGREES?0.017453292519943295:1;
            double angle=av*factor,uncertainty=a.error*fabs(factor)+fabs(angle)*4*DBL_EPSILON;
            if(fabs(angle)>1e6){r.kind=APPLICATION_FAST_UNCERTAIN;
        break;}
            if(op.operation==F_TAN){double cosine=cos(angle);if(fabs(cosine)<1e-8){r.kind=APPLICATION_FAST_UNCERTAIN;
        break;}r.value=tan(angle);r.error=uncertainty/(cosine*cosine);}
            else{r.value=op.operation==F_SIN?sin(angle):cos(angle);r.error=uncertainty;}break;
        }
        case F_EXP:r.value=exp(av);r.error=fabs(r.value)*a.error;if(r.value==0)r.kind=APPLICATION_FAST_UNCERTAIN;
        break;
        case F_LN:
            if(av<=0){r.kind=av<-8*a.error?APPLICATION_FAST_NON_REAL:APPLICATION_FAST_UNCERTAIN;
        break;}
            if(av<=8*a.error){r.kind=APPLICATION_FAST_UNCERTAIN;
        break;}r.value=log(av);r.error=a.error/av;
        break;
        case F_SQRT:
            if(av<0){r.kind=av<-8*a.error?APPLICATION_FAST_NON_REAL:APPLICATION_FAST_UNCERTAIN;
        break;}
            if(av<=8*a.error&&a.error!=0){r.kind=APPLICATION_FAST_UNCERTAIN;
        break;}r.value=sqrt(av);r.error=r.value?a.error/(2*r.value):0;
        break;
        case F_CBRT:r.value=cbrt(av);r.error=r.value?a.error/(3*r.value*r.value):a.error;
        break;
        case F_ABS:r.value=fabs(av);r.error=a.error;
        break;
        default:r.kind=APPLICATION_FAST_UNCERTAIN;
        break;
        }
        r.error+=fabs(r.value)*4*DBL_EPSILON;
        if(!isfinite(r.value)||!isfinite(r.error)||r.error>1e-8*fmax(DBL_MIN,fabs(r.value)))r.kind=APPLICATION_FAST_UNCERTAIN;
        stack[used++]=r;
    }
    return used==1?(ApplicationFastSample){stack[0].value,stack[0].kind}:(ApplicationFastSample){0,APPLICATION_FAST_UNCERTAIN};
}
CalculatorStatus application_fast_expression_sample(const ApplicationFastExpression *expression,
    const double *coordinates,size_t count,ApplicationFastSample *result)
{
    if(!expression||!coordinates||!result)return CALCULATOR_NULL_ARGUMENT;
    if(count==0||count>8192U)return CALCULATOR_INVALID_ARGUMENT;
    for(size_t i=0;i<count;++i)if(!isfinite(coordinates[i]))return CALCULATOR_INVALID_ARGUMENT;
    bool owner=numforge_budget_begin((uint64_t)expression->context.time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    ApplicationFastSample *pending=numforge_malloc(count*sizeof(*pending));
    CalculatorStatus status=pending?CALCULATOR_OK:CALCULATOR_OUT_OF_MEMORY;
    for(size_t i=0;status==CALCULATOR_OK&&i<count;++i){
        if(!numforge_budget_check()){status=calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
        break;}
        pending[i]=evaluate(expression,coordinates[i]);
    }
    status=calculator_budget_status(status);
    if(status==CALCULATOR_OK)memcpy(result,pending,count*sizeof(*pending));
    free(pending);if(owner)numforge_budget_end();
    return status;
}
