#include "fast_graph.h"
#include "graph_analysis.h"
#include "exact_functions.h"
#include "../internal/numforge_alloc.h"
#include <numforge/runtime.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CalculatorStatus graph_argument(CalculatorValue *value,const char *text,const CalculatorContext *context)
{
    memset(value,0,sizeof(*value));value->context=*context;value->kind=CALCULATOR_VALUE_RATIONAL;value->independent=true;
    value->number=bigdecimal_create();value->rational=bigrational_create();
    CalculatorStatus status=value->number&&value->rational?CALCULATOR_OK:CALCULATOR_OUT_OF_MEMORY;
    if(status==CALCULATOR_OK)status=calculator_from_decimal_status(bigdecimal_set_string(value->number,text));
    if(status==CALCULATOR_OK)status=calculator_from_rational_status(bigrational_from_bigdecimal(value->rational,value->number));
    return status;
}
static CalculatorStatus graph_projection(const BigDecimal *number,double *value)
{
    char *text=NULL;CalculatorStatus status=calculator_from_decimal_status(bigdecimal_format_mode(number,16,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,128U,&text));
    if(status==CALCULATOR_OK){char *end=NULL;errno=0;double projected=strtod(text,&end);*value=errno==ERANGE||!isfinite(projected)||end==text||*end?NAN:projected;}
    free(text);return status;
}
static CalculatorStatus graph_actual_point(ApplicationExpression *expression,const CalculatorValue *argument,ApplicationFastGraphPoint *point,CalculatorError *error)
{
    ApplicationExpressionSample *sample=NULL;CalculatorStatus status=application_expression_sample(expression,argument,1U,&sample,error);
    if(status==CALCULATOR_OK){
        point->kind=application_graph_point_kind(sample);point->y=NAN;point->derivative=NAN;point->connect=false;
        point->approximate=sample->value.kind==CALCULATOR_VALUE_DECIMAL||sample->value.kind==CALCULATOR_VALUE_COMPLEX_DECIMAL;
        if(point->kind==APPLICATION_GRAPH_REAL)status=graph_projection(sample->value.number,&point->y);
    }
    application_expression_samples_destroy(sample,1U);return status;
}
CalculatorStatus application_graph_integer_points(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,bool automatic,const CalculatorSession *session,const CalculatorContext *context,CalculatorError *error)
{
    calculator_error_clear(error);
    if(!result||!input||!context){calculator_error_set(error,CALCULATOR_NULL_ARGUMENT,0U);return CALCULATOR_NULL_ARGUMENT;}
    if(!isfinite(minimum)||!isfinite(maximum)||maximum<=minimum||width<320U||width>2048U){calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,0U);return CALCULATOR_INVALID_ARGUMENT;}
    CalculatorStatus status=calculator_context_validate(context,error);if(status!=CALCULATOR_OK)return status;
    bool owner=numforge_budget_begin((uint64_t)context->time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    ApplicationExpression *expression=NULL;ApplicationFastGraph *graph=NULL;
    status=parameter?application_expression_prepare(&expression,input,parameter,session,context,error):application_expression_prepare_auto(&expression,input,session,context,error);
    if(status==CALCULATOR_OK&&automatic&&!application_expression_graph_integer_domain(expression))status=CALCULATOR_NOT_IMPLEMENTED;
    if(status==CALCULATOR_OK&&(fabs(minimum)>9007199254740991.0||fabs(maximum)>9007199254740991.0))status=CALCULATOR_VALUE_TOO_LARGE;
    if(status==CALCULATOR_OK){graph=numforge_calloc(1U,sizeof(*graph));if(!graph)status=CALCULATOR_OUT_OF_MEMORY;}
    if(status==CALCULATOR_OK){
        graph->points=numforge_calloc(256U,sizeof(*graph->points));if(!graph->points)status=CALCULATOR_OUT_OF_MEMORY;
        graph->minimum=NAN;graph->maximum=NAN;graph->discrete=true;
        const char *name=application_expression_parameter(expression);memcpy(graph->parameter,name,strlen(name)+1U);
    }
    double low=ceil(minimum),high=floor(maximum),total=high-low+1,limit=fmin(256,(double)(width/8U));
    double stride=fmax(1,ceil(total/(limit-1))),first=ceil(low/stride)*stride;
    if(status==CALCULATOR_OK){graph->point_stride=stride;graph->limited=stride>1;}
    for(size_t i=0;status==CALCULATOR_OK&&i<(size_t)limit;++i){
        double x=first+(double)i*stride;if(x>high||low>high)break;
        if(x<low||!numforge_budget_check()){status=calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);break;}
        char text[32];(void)snprintf(text,sizeof(text),"%.0f",x);CalculatorValue argument={0};
        status=graph_argument(&argument,text,context);
        ApplicationFastGraphPoint *point=&graph->points[graph->count];point->x=x;
        if(status==CALCULATOR_OK)status=graph_actual_point(expression,&argument,point,error);
        calculator_value_destroy(&argument);
        if(status==CALCULATOR_OK){++graph->count;++graph->exact_probes;if(point->kind==APPLICATION_GRAPH_REAL&&isfinite(point->y)){
            graph->minimum=isfinite(graph->minimum)?fmin(graph->minimum,point->y):point->y;graph->maximum=isfinite(graph->maximum)?fmax(graph->maximum,point->y):point->y;
        }}
    }
    status=calculator_budget_status(status);
    if(status==CALCULATOR_OK){*result=graph;graph=NULL;}else calculator_error_set(error,status,error?error->offset:0U);
    application_fast_graph_destroy(graph);application_expression_destroy(expression);if(owner)numforge_budget_end();return status;
}

typedef struct VerifiedSample { double x,y,difference;int sign;bool real,approximate; } VerifiedSample;
typedef struct VerifyWork { ApplicationExpression *first,*second;const CalculatorContext *context;CalculatorError *error;size_t probes;bool limited; } VerifyWork;
static CalculatorStatus verify_sample(VerifyWork *work,double x,bool pair,VerifiedSample *result)
{
    *result=(VerifiedSample){0};result->x=x;
    if(work->probes+(pair?2U:1U)>512U||numforge_budget_available_ms()<50U||numforge_budget_available_bytes()<2U*1024U*1024U){work->limited=true;return CALCULATOR_OK;}
    char text[32];(void)snprintf(text,sizeof(text),"%.16g",x);
    CalculatorValue argument={0};ApplicationExpressionSample *first=NULL,*second=NULL;BigDecimal *difference=NULL;BigRational *rational=NULL;
    CalculatorStatus status=graph_argument(&argument,text,work->context);
    if(status==CALCULATOR_OK){++work->probes;status=application_expression_sample(work->first,&argument,1U,&first,work->error);}
    if(status==CALCULATOR_OK&&pair){++work->probes;status=application_expression_sample(work->second,&argument,1U,&second,work->error);}
    if(status==CALCULATOR_OK&&application_graph_point_kind(first)==APPLICATION_GRAPH_REAL&&(!pair||application_graph_point_kind(second)==APPLICATION_GRAPH_REAL)){
        difference=bigdecimal_create();if(!difference)status=CALCULATOR_OUT_OF_MEMORY;
        if(status==CALCULATOR_OK){
            if(!pair)status=calculator_from_decimal_status(bigdecimal_copy(difference,first->value.number));
            else if(first->value.rational&&second->value.rational){
                rational=bigrational_create();if(!rational)status=CALCULATOR_OUT_OF_MEMORY;
                if(status==CALCULATOR_OK)status=calculator_from_rational_status(bigrational_sub(rational,first->value.rational,second->value.rational));
                if(status==CALCULATOR_OK)status=calculator_from_rational_status(bigrational_to_bigdecimal(difference,rational,work->context->division_scale,BIGDECIMAL_ROUND_HALF_EVEN));
            }else status=calculator_from_decimal_status(bigdecimal_sub(difference,first->value.number,second->value.number));
        }
        if(status==CALCULATOR_OK)status=graph_projection(first->value.number,&result->y);
        if(status==CALCULATOR_OK)status=graph_projection(difference,&result->difference);
        if(status==CALCULATOR_OK)status=calculator_from_decimal_status(bigdecimal_sign(&result->sign,difference));
        result->real=status==CALCULATOR_OK&&isfinite(result->y)&&isfinite(result->difference);
        result->approximate=first->value.kind==CALCULATOR_VALUE_DECIMAL||(pair&&second->value.kind==CALCULATOR_VALUE_DECIMAL);
    }
    calculator_value_destroy(&argument);application_expression_samples_destroy(first,1U);application_expression_samples_destroy(second,1U);bigdecimal_destroy(difference);bigrational_destroy(rational);return status;
}
static void verified_output(ApplicationGraphVerifiedPoint *result,const VerifiedSample *sample,bool root)
{
    *result=(ApplicationGraphVerifiedPoint){true,sample->approximate,sample->x,root?0:sample->y,fabs(sample->difference)};
}
static CalculatorStatus verify_candidate(VerifyWork *work,const ApplicationGraphCandidate *candidate,ApplicationGraphVerifiedPoint *result)
{
    bool pair=candidate->kind==APPLICATION_GRAPH_INTERSECTION||candidate->kind==APPLICATION_GRAPH_POINT_INTERSECTION;
    bool root=candidate->kind==APPLICATION_GRAPH_ROOT;VerifiedSample center={0},left={0},right={0};
    CalculatorStatus status=verify_sample(work,candidate->x,pair,&center);if(status!=CALCULATOR_OK||work->limited||!center.real)return status;
    if(candidate->kind==APPLICATION_GRAPH_VALUE){verified_output(result,&center,false);return CALCULATOR_OK;}
    if(center.sign==0){verified_output(result,&center,root);return CALCULATOR_OK;}
    const double tiny=fmin(candidate->y_tolerance,1e-24*fmax(1,fabs(center.y)));
    if(candidate->kind==APPLICATION_GRAPH_POINT_INTERSECTION){if(fabs(center.difference)<=tiny)verified_output(result,&center,false);return CALCULATOR_OK;}
    status=verify_sample(work,candidate->low,pair,&left);if(status==CALCULATOR_OK&&!work->limited)status=verify_sample(work,candidate->high,pair,&right);
    if(status!=CALCULATOR_OK||work->limited||!left.real||!right.real)return status;
    if(left.sign!=0&&right.sign!=0&&left.sign==right.sign){
        // Tangencies/cusps: require a much smaller actual C residual on both
        // sides. Decimal rounding can recover a simple exact cusp coordinate;
        // a positive minimum is never silently snapped using pixel tolerance.
        if(fabs(center.difference)<=tiny&&fabs(left.difference)>fabs(center.difference)*1e4&&fabs(right.difference)>fabs(center.difference)*1e4){verified_output(result,&center,root);return CALCULATOR_OK;}
        for(int digits=15;digits>=12&&!work->limited;--digits){char text[32];(void)snprintf(text,sizeof(text),"%.*g",digits,candidate->x);double x=strtod(text,NULL);
            if(x<candidate->low||x>candidate->high||fabs(x-candidate->x)>candidate->x_tolerance)continue;
            VerifiedSample rounded={0};status=verify_sample(work,x,pair,&rounded);if(status!=CALCULATOR_OK)return status;
            if(rounded.real&&rounded.sign==0){verified_output(result,&rounded,root);return CALCULATOR_OK;}
        }return CALCULATOR_OK;
    }
    if(left.sign==0){verified_output(result,&left,root);return CALCULATOR_OK;}
    if(right.sign==0){verified_output(result,&right,root);return CALCULATOR_OK;}
    VerifiedSample best=center;
    if(fabs(best.difference)<=fmin(candidate->y_tolerance,1e-11*fmax(1,fabs(best.y)))){verified_output(result,&best,root);return CALCULATOR_OK;}
    if(center.x>left.x&&center.x<right.x){if(center.sign==left.sign)left=center;else right=center;}
    for(unsigned iteration=0;iteration<24U&&!work->limited;++iteration){
        double denominator=fabs(left.difference)+fabs(right.difference),fraction=fabs(left.difference)/denominator;
        if(!isfinite(fraction)||fraction<.05||fraction>.95)fraction=.5;
        double x=left.x+(right.x-left.x)*fraction;if(x<=left.x||x>=right.x)break;
        VerifiedSample mid={0};status=verify_sample(work,x,pair,&mid);if(status!=CALCULATOR_OK||!mid.real)return status;
        if(fabs(mid.difference)<fabs(best.difference))best=mid;
        if(mid.sign==0){verified_output(result,&mid,root);return CALCULATOR_OK;}
        if(mid.sign==left.sign)left=mid;else right=mid;
        double tolerance=fmin(candidate->y_tolerance,1e-11*fmax(1,fabs(best.y)));
        if(fabs(best.difference)<=tolerance){verified_output(result,&best,root);return CALCULATOR_OK;}
    }
    double tolerance=fmin(candidate->y_tolerance,1e-11*fmax(1,fabs(best.y)));
    if(!work->limited&&fabs(best.difference)<=tolerance)verified_output(result,&best,root);
    return CALCULATOR_OK;
}
CalculatorStatus application_graph_verify(ApplicationGraphVerifiedPoint *result,bool *limited,const char *input,const char *other,
    const ApplicationGraphCandidate *candidates,size_t count,const CalculatorContext *context,CalculatorError *error)
{
    calculator_error_clear(error);
    if(!result||!limited||!input||!candidates||!context){calculator_error_set(error,CALCULATOR_NULL_ARGUMENT,0U);return CALCULATOR_NULL_ARGUMENT;}
    if(count<1U||count>APPLICATION_GRAPH_MAX_CANDIDATES){calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,0U);return CALCULATOR_INVALID_ARGUMENT;}
    bool pair=false;
    for(size_t i=0;i<count;++i){const ApplicationGraphCandidate *c=&candidates[i];
        if((unsigned)c->kind>(unsigned)APPLICATION_GRAPH_POINT_INTERSECTION||!isfinite(c->x)||!isfinite(c->low)||!isfinite(c->high)||!isfinite(c->x_tolerance)||!isfinite(c->y_tolerance)||c->low>c->x||c->high<c->x||c->x_tolerance<=0||c->y_tolerance<=0){calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,0U);return CALCULATOR_INVALID_ARGUMENT;}
        if(c->kind==APPLICATION_GRAPH_INTERSECTION||c->kind==APPLICATION_GRAPH_POINT_INTERSECTION)pair=true;
    }
    if(pair&&(!other||!*other)){calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,0U);return CALCULATOR_INVALID_ARGUMENT;}
    CalculatorStatus status=calculator_context_validate(context,error);if(status!=CALCULATOR_OK)return status;
    bool owner=numforge_budget_begin((uint64_t)context->time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    VerifyWork work={0};work.context=context;work.error=error;
    ApplicationGraphVerifiedPoint temporary[APPLICATION_GRAPH_MAX_CANDIDATES]={0};
    status=application_expression_prepare_auto(&work.first,input,NULL,context,error);
    if(status==CALCULATOR_OK&&pair)status=application_expression_prepare_auto(&work.second,other,NULL,context,error);
    for(size_t i=0;i<count&&status==CALCULATOR_OK&&!work.limited;++i)status=verify_candidate(&work,&candidates[i],&temporary[i]);
    status=calculator_budget_status(status);
    if(status==CALCULATOR_OK){memcpy(result,temporary,count*sizeof(*result));*limited=work.limited;}else calculator_error_set(error,status,error?error->offset:0U);
    application_expression_destroy(work.first);application_expression_destroy(work.second);if(owner)numforge_budget_end();return status;
}
