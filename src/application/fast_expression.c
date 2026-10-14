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
    FastInstruction *code;size_t count,phase_count,guard_count;CalculatorContext context;
};
typedef struct FastCell { double value,error;ApplicationFastKind kind;double derivative;bool differentiable; } FastCell;

void application_fast_expression_destroy(ApplicationFastExpression *expression)
{if(expression){free(expression->code);free(expression);}}

static CalculatorStatus emit(ApplicationFastExpression *program,FastOp operation,double value,double error)
{
    if(program->count>=FAST_INSTRUCTIONS)return CALCULATOR_VALUE_TOO_LARGE;
    if(operation>=F_SIN&&operation<=F_TAN){if(program->phase_count>=64U)return CALCULATOR_INVALID_ARGUMENT;++program->phase_count;}
    if(operation==F_DIVIDE||operation==F_POWER||operation==F_SQRT||operation==F_LN){if(program->guard_count>=64U)return CALCULATOR_INVALID_ARGUMENT;++program->guard_count;}
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

/* Recognize a single affine sin/cos carrier from the compiled C expression.
 * No text rewriting, guessed extrema or variable denominators are allowed. */
CalculatorStatus application_fast_expression_dense_model(const ApplicationFastExpression *program,
    ApplicationFastDenseModel *result)
{
    typedef struct Cell {ApplicationFastDenseModel model;bool carrier;} Cell;
    if(!program||!result)return CALCULATOR_NULL_ARGUMENT;
    if(program->phase_count!=1U)return CALCULATOR_NOT_IMPLEMENTED;
    Cell stack[CALCULATOR_MAX_EXPRESSION_DEPTH+1U];size_t used=0;
    for(size_t i=0;i<program->count;++i){
        if(!numforge_budget_check())return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
        FastInstruction op=program->code[i];Cell a={0},b={0},r={0};
        if(op.operation==F_CONSTANT||op.operation==F_X){
            if(used>=CALCULATOR_MAX_EXPRESSION_DEPTH+1U)return CALCULATOR_NOT_IMPLEMENTED;
            r.model.offset=op.operation==F_X?1:0;r.model.offset_value=op.operation==F_X?0:op.value;
            stack[used++]=r;continue;
        }
        if(!used)return CALCULATOR_NOT_IMPLEMENTED;
        a=stack[--used];bool binary=op.operation>=F_ADD&&op.operation<=F_POWER;
        if(binary){if(!used)return CALCULATOR_NOT_IMPLEMENTED;b=a;a=stack[--used];}
        double scale=1;
        switch(op.operation){
        case F_NEGATE:r=a;scale=-1;break;
        case F_ADD:case F_SUBTRACT:
            if(a.carrier&&b.carrier)return CALCULATOR_NOT_IMPLEMENTED;
            r=a.carrier?a:b;r.carrier=a.carrier||b.carrier;
            if(op.operation==F_SUBTRACT&&!a.carrier){r.model.amplitude=-r.model.amplitude;r.model.amplitude_offset=-r.model.amplitude_offset;}
            r.model.offset=a.model.offset+(op.operation==F_ADD?1:-1)*b.model.offset;
            r.model.offset_value=a.model.offset_value+(op.operation==F_ADD?1:-1)*b.model.offset_value;
            break;
        case F_MULTIPLY:
            if(a.carrier&&b.carrier)return CALCULATOR_NOT_IMPLEMENTED;
            if(b.carrier){Cell swap=a;a=b;b=swap;}
            if(b.model.offset!=0&&(a.model.amplitude!=0||a.model.offset!=0))return CALCULATOR_NOT_IMPLEMENTED;
            r=a;r.model.amplitude=a.model.amplitude*b.model.offset_value+a.model.amplitude_offset*b.model.offset;
            r.model.amplitude_offset=a.model.amplitude_offset*b.model.offset_value;
            r.model.offset=a.model.offset*b.model.offset_value+a.model.offset_value*b.model.offset;
            r.model.offset_value=a.model.offset_value*b.model.offset_value;
            break;
        case F_DIVIDE:
            if(b.carrier||b.model.offset!=0||b.model.offset_value==0)return CALCULATOR_NOT_IMPLEMENTED;
            r=a;scale=1/b.model.offset_value;break;
        case F_SIN:case F_COS:
            if(a.carrier||a.model.offset==0)return CALCULATOR_NOT_IMPLEMENTED;
            r.carrier=true;r.model.amplitude_offset=1;
            scale=program->context.angle_unit==CALCULATOR_ANGLE_DEGREES?0.017453292519943295:1;
            r.model.phase=a.model.offset*scale;r.model.phase_offset=a.model.offset_value*scale;scale=1;
            break;
        default:
            /* Constant-only operations may be folded; nonlinear amplitude,
             * phase, clipping and domain-sensitive transformations are deferred. */
            if(a.carrier||a.model.offset!=0||(binary&&(b.carrier||b.model.offset!=0)))return CALCULATOR_NOT_IMPLEMENTED;
            switch(op.operation){
            case F_SQUARE:r.model.offset_value=a.model.offset_value*a.model.offset_value;break;
            case F_CUBE:r.model.offset_value=a.model.offset_value*a.model.offset_value*a.model.offset_value;break;
            case F_POWER:r.model.offset_value=pow(a.model.offset_value,b.model.offset_value);break;
            case F_EXP:r.model.offset_value=exp(a.model.offset_value);break;
            case F_LN:if(a.model.offset_value<=0)return CALCULATOR_NOT_IMPLEMENTED;r.model.offset_value=log(a.model.offset_value);break;
            case F_SQRT:if(a.model.offset_value<0)return CALCULATOR_NOT_IMPLEMENTED;r.model.offset_value=sqrt(a.model.offset_value);break;
            case F_CBRT:r.model.offset_value=cbrt(a.model.offset_value);break;
            case F_ABS:r.model.offset_value=fabs(a.model.offset_value);break;
            default:return CALCULATOR_NOT_IMPLEMENTED;
            }
            break;
        }
        r.model.amplitude*=scale;r.model.amplitude_offset*=scale;r.model.offset*=scale;r.model.offset_value*=scale;
        if(!isfinite(r.model.amplitude)||!isfinite(r.model.amplitude_offset)||!isfinite(r.model.offset)||!isfinite(r.model.offset_value)||!isfinite(r.model.phase)||!isfinite(r.model.phase_offset))return CALCULATOR_NOT_IMPLEMENTED;
        stack[used++]=r;
    }
    if(used!=1U||!stack[0].carrier||(stack[0].model.amplitude==0&&stack[0].model.amplitude_offset==0))return CALCULATOR_NOT_IMPLEMENTED;
    *result=stack[0].model;return CALCULATOR_OK;
}

/* Error propagation is a conservative fallback heuristic, not certified interval
 * arithmetic. Libm values are only for approximate screen-space plotting. */
static ApplicationFastSample evaluate(const ApplicationFastExpression *program,double x,ApplicationFastProbe *probe,bool derive)
{
    FastCell stack[CALCULATOR_MAX_EXPRESSION_DEPTH+1U];size_t used=0;
    for(size_t i=0;i<program->count;++i){
        FastInstruction op=program->code[i];FastCell a={0},b=a,r=a;
        if(op.operation==F_CONSTANT||op.operation==F_X){
            if(used>=CALCULATOR_MAX_EXPRESSION_DEPTH+1U)return (ApplicationFastSample){0,APPLICATION_FAST_UNCERTAIN};
            stack[used++]=(FastCell){op.operation==F_X?x:op.value,op.operation==F_X?fabs(x)*4*DBL_EPSILON:op.error,APPLICATION_FAST_REAL,op.operation==F_X?1:0,true};continue;
        }
        if(!used)return (ApplicationFastSample){0,APPLICATION_FAST_UNCERTAIN};
        a=stack[--used];
        bool binary=op.operation>=F_ADD&&op.operation<=F_POWER;
        if(binary){if(!used)return (ApplicationFastSample){0,APPLICATION_FAST_UNCERTAIN};b=a;a=stack[--used];}
        // A complex intermediate can become real again (e.g. abs(sqrt(-1))).
        // Only the exact evaluator may resolve such composed expressions.
        bool uncertain=a.kind!=APPLICATION_FAST_REAL||b.kind!=APPLICATION_FAST_REAL;
        if(uncertain&&(!derive||!a.differentiable||(binary&&!b.differentiable)||!isfinite(a.value)||!isfinite(b.value))){r.kind=APPLICATION_FAST_UNCERTAIN;stack[used++]=r;continue;}
        double av=a.value,bv=b.value;
        if(probe&&(op.operation==F_DIVIDE||op.operation==F_POWER||op.operation==F_SQRT||op.operation==F_LN)){
            int kind=op.operation==F_DIVIDE?1:op.operation==F_LN?2:op.operation==F_SQRT?3:4;
            probe->guards[probe->guard_count++]=(ApplicationFastGuard){op.operation==F_DIVIDE?bv:av,bv,kind};
        }
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
            if(probe){size_t slot=probe->phase_count++;probe->phases[slot]=angle;probe->phase_kinds[slot]=op.operation==F_COS?0:op.operation==F_SIN?1:-1;}
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
            if(probe&&r.kind==APPLICATION_FAST_NON_REAL)probe->non_real_intermediate=true;
        break;}
            if(av<=8*a.error){r.kind=APPLICATION_FAST_UNCERTAIN;
        break;}r.value=log(av);r.error=a.error/av;
        break;
        case F_SQRT:
            if(av<0){r.kind=av<-8*a.error?APPLICATION_FAST_NON_REAL:APPLICATION_FAST_UNCERTAIN;
            if(probe&&r.kind==APPLICATION_FAST_NON_REAL)probe->non_real_intermediate=true;
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
        if(derive){
            r.differentiable=r.kind==APPLICATION_FAST_REAL&&a.differentiable&&(!binary||b.differentiable);
            if(r.differentiable)switch(op.operation){
            case F_NEGATE:r.derivative=-a.derivative;break;
            case F_SQUARE:r.derivative=2*av*a.derivative;break;
            case F_CUBE:r.derivative=3*av*av*a.derivative;break;
            case F_ADD:r.derivative=a.derivative+b.derivative;break;
            case F_SUBTRACT:r.derivative=a.derivative-b.derivative;break;
            case F_MULTIPLY:r.derivative=a.derivative*bv+av*b.derivative;break;
            case F_DIVIDE:r.derivative=(a.derivative-r.value*b.derivative)/bv;break;
            case F_POWER:
                if(b.derivative==0&&bv==trunc(bv))
                    r.derivative=bv==0?0:bv==1?a.derivative:bv*pow(av,bv-1)*a.derivative;
                else if(av>0)r.derivative=r.value*(b.derivative*log(av)+bv*a.derivative/av);
                else r.differentiable=false;
                break;
            case F_SIN:case F_COS:case F_TAN:{
                double factor=program->context.angle_unit==CALCULATOR_ANGLE_DEGREES?0.017453292519943295:1;
                double angle=av*factor;
                if(op.operation==F_SIN)r.derivative=cos(angle)*factor*a.derivative;
                else if(op.operation==F_COS)r.derivative=-sin(angle)*factor*a.derivative;
                else{double cosine=cos(angle);r.derivative=factor*a.derivative/(cosine*cosine);}
                break;
            }
            case F_EXP:r.derivative=r.value*a.derivative;break;
            case F_LN:r.derivative=a.derivative/av;break;
            case F_SQRT:case F_CBRT:
                if(r.value==0)r.differentiable=false;
                else r.derivative=a.derivative/(op.operation==F_SQRT?2*r.value:3*r.value*r.value);
                break;
            case F_ABS:
                if(av==0)r.differentiable=false;
                else r.derivative=(av<0?-1:1)*a.derivative;
                break;
            default:r.differentiable=false;break;
            }
            if(!isfinite(r.derivative))r.differentiable=false;
        }
        if(op.operation!=F_NEGATE&&op.operation!=F_ABS)r.error+=fabs(r.value)*4*DBL_EPSILON;
        if(!isfinite(r.value)||!isfinite(r.error)||r.error>1e-8*fmax(DBL_MIN,fabs(r.value)))r.kind=APPLICATION_FAST_UNCERTAIN;
        if(uncertain)r.kind=APPLICATION_FAST_UNCERTAIN;
        if(!isfinite(r.value)||!isfinite(r.error))r.differentiable=false;
        stack[used++]=r;
    }
    if(probe&&derive&&used==1&&stack[0].differentiable)probe->derivative=stack[0].derivative;
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
        pending[i]=evaluate(expression,coordinates[i],NULL,false);
    }
    status=calculator_budget_status(status);
    if(status==CALCULATOR_OK)memcpy(result,pending,count*sizeof(*pending));
    free(pending);if(owner)numforge_budget_end();
    return status;
}
static CalculatorStatus derivative_probe(const ApplicationFastExpression *expression,
    double coordinate,ApplicationFastProbe *result,bool derive)
{
    if(!expression||!result)return CALCULATOR_NULL_ARGUMENT;
    if(!isfinite(coordinate))return CALCULATOR_INVALID_ARGUMENT;
    bool owner=numforge_budget_begin((uint64_t)expression->context.time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    CalculatorStatus status=CALCULATOR_OK;ApplicationFastProbe pending={0};pending.derivative=NAN;
    if(!numforge_budget_check())status=calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    if(status==CALCULATOR_OK){pending.sample=evaluate(expression,coordinate,&pending,derive);pending.complete=pending.phase_count==expression->phase_count&&pending.guard_count==expression->guard_count;}
    status=calculator_budget_status(status);if(status==CALCULATOR_OK)*result=pending;
    if(owner)numforge_budget_end();
    return status;
}
CalculatorStatus application_fast_expression_probe(const ApplicationFastExpression *expression,double coordinate,ApplicationFastProbe *result)
{return derivative_probe(expression,coordinate,result,false);}
CalculatorStatus application_fast_expression_probe_derivative(const ApplicationFastExpression *expression,double coordinate,ApplicationFastProbe *result)
{return derivative_probe(expression,coordinate,result,true);}
