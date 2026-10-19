#include "src/application/graphs/fast_graph.h"
#include "src/application/graphs/graph_analysis.h"
#include "src/application/graphs/graph_projection.h"
#include "src/engine/api/status.h"
#include "src/runtime/numforge_alloc.h"
#include <numforge/runtime.h>
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
static CalculatorStatus graph_actual_point(ApplicationExpression *expression,const CalculatorValue *argument,ApplicationFastGraphPoint *point,CalculatorError *error)
{
    ApplicationExpressionSample *sample=NULL;CalculatorStatus status=application_expression_sample(expression,argument,1U,&sample,error);
    if(status==CALCULATOR_OK){
        point->kind=application_graph_point_kind(sample);point->y=NAN;point->derivative=NAN;point->connect=false;
        point->approximate=sample->value.kind==CALCULATOR_VALUE_DECIMAL||sample->value.kind==CALCULATOR_VALUE_COMPLEX_DECIMAL;
        if(point->kind==APPLICATION_GRAPH_REAL)status=application_graph_project(sample->value.number,&point->y);
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
typedef struct VerifyCacheEntry { char coordinate[32];VerifiedSample value;bool occupied,pair; } VerifyCacheEntry;
typedef enum VerifyReason { VERIFY_RESIDUAL,VERIFY_NON_REAL,VERIFY_ITERATIONS,VERIFY_COORDINATES } VerifyReason;
typedef struct VerifyWork {
    ApplicationExpression *first,*second;const CalculatorContext *context;CalculatorError *error;
    size_t probes,sample_requests,cache_hits;bool limited,reuse,brent;
    ApplicationGraphVerificationStats stats;VerifyReason reason;
    /* Four-way, request-local storage: bounded stack space, no retained
     * typed numbers, no cross-expression/session or cross-request reuse. */
    VerifyCacheEntry cache[128];
    unsigned char cache_next[32];
} VerifyWork;
static CalculatorStatus verify_sample(VerifyWork *work,double x,bool pair,VerifiedSample *result)
{
    *result=(VerifiedSample){0};result->x=x;
    if(!numforge_budget_check())return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    if(numforge_budget_available_ms()<50U||numforge_budget_available_bytes()<2U*1024U*1024U){work->limited=true;return CALCULATOR_OK;}
    ++work->sample_requests;
    char text[32];(void)snprintf(text,sizeof(text),"%.16g",x);
    unsigned hash=2166136261U;
    for(const unsigned char *p=(const unsigned char *)text;*p;++p)hash=(hash^*p)*16777619U;
    hash=(hash^(unsigned)pair)*16777619U;
    size_t bucket=(size_t)(hash%32U);
    if(work->reuse)for(size_t i=0;i<4U;++i){
        const VerifyCacheEntry *cached=&work->cache[bucket*4U+i];
        if(cached->occupied&&cached->pair==pair&&strcmp(cached->coordinate,text)==0){
            *result=cached->value;result->x=x;++work->cache_hits;return CALCULATOR_OK;
        }
    }
    if(work->probes+(pair?2U:1U)>512U){work->limited=true;return CALCULATOR_OK;}
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
        if(status==CALCULATOR_OK)status=application_graph_project(first->value.number,&result->y);
        if(status==CALCULATOR_OK)status=application_graph_project(difference,&result->difference);
        if(status==CALCULATOR_OK)status=calculator_from_decimal_status(bigdecimal_sign(&result->sign,difference));
        result->real=status==CALCULATOR_OK&&isfinite(result->y)&&isfinite(result->difference);
        result->approximate=first->value.kind==CALCULATOR_VALUE_DECIMAL||(pair&&second->value.kind==CALCULATOR_VALUE_DECIMAL);
    }
    if(status==CALCULATOR_OK&&work->reuse){
        /* Cache the complete original-expression check, including the rational
         * difference computed above, never a difference of rounded projections.
         * The key is the decimal coordinate actually passed to C, not a pixel. */
        VerifyCacheEntry *entry=&work->cache[bucket*4U+work->cache_next[bucket]];
        memcpy(entry->coordinate,text,strlen(text)+1U);entry->value=*result;entry->pair=pair;entry->occupied=true;
        work->cache_next[bucket]=(unsigned char)((work->cache_next[bucket]+1U)%4U);
    }
    calculator_value_destroy(&argument);application_expression_samples_destroy(first,1U);application_expression_samples_destroy(second,1U);bigdecimal_destroy(difference);bigrational_destroy(rational);return status;
}
static void verified_output(ApplicationGraphVerifiedPoint *result,const VerifiedSample *sample,bool root)
{
    *result=(ApplicationGraphVerifiedPoint){true,sample->approximate,sample->x,root?0:sample->y,fabs(sample->difference)};
}
static bool verified_residual(const ApplicationGraphCandidate *candidate,const VerifiedSample *sample)
{
    return fabs(sample->difference)<=fmin(candidate->y_tolerance,1e-11*fmax(1,fabs(sample->y)));
}
static CalculatorStatus verify_brent(VerifyWork *work,const ApplicationGraphCandidate *candidate,bool pair,bool root,
    VerifiedSample a,VerifiedSample b,VerifiedSample best,ApplicationGraphVerifiedPoint *result)
{
    // Brent-Dekker: retain an opposite-sign bracket, attempt secant/inverse
    // quadratic steps, otherwise bisect. Width alone never confirms a root.
    VerifiedSample c=a;double d=b.x-a.x,e=d;
    for(unsigned iteration=0;iteration<24U&&!work->limited;++iteration){
        if(b.sign==c.sign){c=a;d=b.x-a.x;e=d;}
        if(fabs(c.difference)<fabs(b.difference)){VerifiedSample old=b;a=b;b=c;c=old;}
        if(fabs(b.difference)<fabs(best.difference))best=b;
        if(b.sign==0||verified_residual(candidate,&best)){verified_output(result,&best,root);return CALCULATOR_OK;}
        double half=c.x*.5-b.x*.5,tolerance=2*DBL_EPSILON*fabs(b.x);
        tolerance=fmax(tolerance,fabs(nextafter(b.x,c.x)-b.x));
        if(fabs(half)<=tolerance){work->reason=VERIFY_COORDINATES;return CALCULATOR_OK;}
        bool interpolate=false;double step=half;
        if(fabs(e)>=tolerance&&fabs(a.difference)>fabs(b.difference)){
            double s=b.difference/a.difference,p,q;
            if(a.x==c.x){p=2*half*s;q=1-s;}
            else{
                q=a.difference/c.difference;double r=b.difference/c.difference;
                p=s*(2*half*q*(q-r)-(b.x-a.x)*(r-1));q=(q-1)*(r-1)*(s-1);
            }
            if(p>0)q=-q;else p=-p;
            if(isfinite(p)&&isfinite(q)&&q!=0&&2*p<fmin(3*half*q-fabs(tolerance*q),fabs(e*q))){step=p/q;interpolate=isfinite(step);}
        }
        if(interpolate){e=d;d=step;}else{d=half;e=half;}
        double x=b.x+(fabs(d)>tolerance?d:copysign(tolerance,half));
        if(!isfinite(x)||x<=fmin(b.x,c.x)||x>=fmax(b.x,c.x)){x=b.x*.5+c.x*.5;interpolate=false;d=half;e=half;}
        if(x<=fmin(b.x,c.x)||x>=fmax(b.x,c.x)){work->reason=VERIFY_COORDINATES;return CALCULATOR_OK;}
        ++work->stats.refinement_steps;
        if(interpolate)++work->stats.interpolations;else ++work->stats.bisections;
        a=b;VerifiedSample sample={0};CalculatorStatus status=verify_sample(work,x,pair,&sample);
        if(status!=CALCULATOR_OK||work->limited)return status;
        if(!sample.real){work->reason=VERIFY_NON_REAL;return CALCULATOR_OK;}
        b=sample;
        if(fabs(b.difference)<fabs(best.difference))best=b;
        if(b.sign==0||verified_residual(candidate,&best)){verified_output(result,&best,root);return CALCULATOR_OK;}
    }
    work->reason=VERIFY_ITERATIONS;return CALCULATOR_OK;
}
static CalculatorStatus verify_candidate(VerifyWork *work,const ApplicationGraphCandidate *candidate,ApplicationGraphVerifiedPoint *result)
{
    bool pair=candidate->kind==APPLICATION_GRAPH_INTERSECTION||candidate->kind==APPLICATION_GRAPH_POINT_INTERSECTION;
    bool root=candidate->kind==APPLICATION_GRAPH_ROOT;VerifiedSample center={0},left={0},right={0};
    CalculatorStatus status=verify_sample(work,candidate->x,pair,&center);if(status!=CALCULATOR_OK||work->limited)return status;
    if(!center.real){work->reason=VERIFY_NON_REAL;return CALCULATOR_OK;}
    if(candidate->kind==APPLICATION_GRAPH_VALUE){verified_output(result,&center,false);return CALCULATOR_OK;}
    if(center.sign==0){verified_output(result,&center,root);return CALCULATOR_OK;}
    const double tiny=fmin(candidate->y_tolerance,1e-24*fmax(1,fabs(center.y)));
    if(candidate->kind==APPLICATION_GRAPH_POINT_INTERSECTION){if(fabs(center.difference)<=tiny)verified_output(result,&center,false);return CALCULATOR_OK;}
    status=verify_sample(work,candidate->low,pair,&left);if(status==CALCULATOR_OK&&!work->limited)status=verify_sample(work,candidate->high,pair,&right);
    if(status!=CALCULATOR_OK||work->limited)return status;
    if(!left.real||!right.real){work->reason=VERIFY_NON_REAL;return CALCULATOR_OK;}
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
    if(verified_residual(candidate,&best)){verified_output(result,&best,root);return CALCULATOR_OK;}
    if(center.x>left.x&&center.x<right.x){if(center.sign==left.sign)left=center;else right=center;}
    if(work->brent)return verify_brent(work,candidate,pair,root,left,right,best,result);
    unsigned iterations=0;
    for(unsigned iteration=0;iteration<24U&&!work->limited;++iteration){
        double denominator=fabs(left.difference)+fabs(right.difference),fraction=fabs(left.difference)/denominator;
        bool bisect=!isfinite(fraction)||fraction<.05||fraction>.95;if(bisect)fraction=.5;
        double x=left.x+(right.x-left.x)*fraction;if(x<=left.x||x>=right.x){work->reason=VERIFY_COORDINATES;break;}
        ++iterations;++work->stats.refinement_steps;if(bisect)++work->stats.bisections;else ++work->stats.interpolations;
        VerifiedSample mid={0};status=verify_sample(work,x,pair,&mid);if(status!=CALCULATOR_OK||work->limited)return status;
        if(!mid.real){work->reason=VERIFY_NON_REAL;return CALCULATOR_OK;}
        if(fabs(mid.difference)<fabs(best.difference))best=mid;
        if(mid.sign==0){verified_output(result,&mid,root);return CALCULATOR_OK;}
        if(mid.sign==left.sign)left=mid;else right=mid;
        if(verified_residual(candidate,&best)){verified_output(result,&best,root);return CALCULATOR_OK;}
    }
    if(!work->limited&&verified_residual(candidate,&best))verified_output(result,&best,root);
    if(iterations==24U)work->reason=VERIFY_ITERATIONS;
    return CALCULATOR_OK;
}
CalculatorStatus application_graph_verify_experiment(ApplicationGraphVerifiedPoint *result,bool *limited,const char *input,const char *other,
    const ApplicationGraphCandidate *candidates,size_t count,const CalculatorContext *context,CalculatorError *error,bool reuse,bool brent,ApplicationGraphVerificationStats *stats)
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
    VerifyWork work={0};work.context=context;work.error=error;work.reuse=reuse;work.brent=brent;
    ApplicationGraphVerifiedPoint temporary[APPLICATION_GRAPH_MAX_CANDIDATES]={0};
    status=application_expression_prepare_auto(&work.first,input,NULL,context,error);
    if(status==CALCULATOR_OK&&pair)status=application_expression_prepare_auto(&work.second,other,NULL,context,error);
    size_t processed=0;
    for(size_t i=0;i<count&&status==CALCULATOR_OK&&!work.limited;++i){
        work.reason=VERIFY_RESIDUAL;status=verify_candidate(&work,&candidates[i],&temporary[i]);
        if(status!=CALCULATOR_OK||work.limited)break;
        ++processed;
        if(temporary[i].confirmed)++work.stats.confirmed;
        else if(work.reason==VERIFY_NON_REAL)++work.stats.non_real;
        else if(work.reason==VERIFY_ITERATIONS)++work.stats.iteration_limits;
        else if(work.reason==VERIFY_COORDINATES)++work.stats.coordinate_limits;
        else ++work.stats.residual_rejections;
    }
    status=calculator_budget_status(status);
    if(status==CALCULATOR_OK){memcpy(result,temporary,count*sizeof(*result));*limited=work.limited;work.stats.evaluations=work.probes;work.stats.sample_requests=work.sample_requests;work.stats.cache_hits=work.cache_hits;work.stats.unprocessed=count-processed;if(stats)*stats=work.stats;}else calculator_error_set(error,status,error?error->offset:0U);
    application_expression_destroy(work.first);application_expression_destroy(work.second);if(owner)numforge_budget_end();return status;
}
CalculatorStatus application_graph_verify_profile(ApplicationGraphVerifiedPoint *result,bool *limited,const char *input,const char *other,
    const ApplicationGraphCandidate *candidates,size_t count,const CalculatorContext *context,CalculatorError *error,bool reuse,ApplicationGraphVerificationStats *stats)
{
    return application_graph_verify_experiment(result,limited,input,other,candidates,count,context,error,reuse,true,stats);
}
CalculatorStatus application_graph_verify(ApplicationGraphVerifiedPoint *result,bool *limited,const char *input,const char *other,
    const ApplicationGraphCandidate *candidates,size_t count,const CalculatorContext *context,CalculatorError *error)
{
    return application_graph_verify_profile(result,limited,input,other,candidates,count,context,error,true,NULL);
}
