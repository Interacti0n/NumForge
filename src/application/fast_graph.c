#include "fast_graph.h"
#include "fast_expression.h"
#include "exact_functions.h"
#include "value_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/runtime.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Work {
    ApplicationFastGraph *graph;ApplicationExpression *exact;ApplicationFastExpression *fast;
    const CalculatorContext *context;CalculatorError *error;
    double scale,tolerance,coarse_width;size_t limit;bool smooth,screen_floor;
} Work;
static bool real(const ApplicationFastGraphPoint *p){return p->kind==APPLICATION_GRAPH_REAL&&isfinite(p->y);}
static CalculatorStatus probe(Work *work,double x,ApplicationFastProbe *result)
{return work->screen_floor?application_fast_expression_probe_derivative(work->fast,x,result):application_fast_expression_probe(work->fast,x,result);}
void application_fast_graph_destroy(ApplicationFastGraph *graph){if(graph){free(graph->points);free(graph);}}
static CalculatorStatus project(const BigDecimal *number,double *value)
{
    char *text=NULL;CalculatorStatus status=calculator_from_decimal_status(bigdecimal_format_mode(number,
        16,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,128U,&text));
    if(status==CALCULATOR_OK){char *end=NULL;errno=0;double x=strtod(text,&end);*value=errno==ERANGE||!isfinite(x)||end==text||*end?NAN:x;}
    free(text);return status;
}
static CalculatorStatus point(Work *work,double x,ApplicationFastGraphPoint *result,ApplicationFastProbe *probe)
{
    CalculatorStatus status=work->screen_floor?application_fast_expression_probe_derivative(work->fast,x,probe):application_fast_expression_probe(work->fast,x,probe);
    if(status!=CALCULATOR_OK)return status;
    *result=(ApplicationFastGraphPoint){x,NAN,APPLICATION_GRAPH_REAL,false,true,NAN};
    if(probe->sample.kind==APPLICATION_FAST_REAL){result->y=probe->sample.value;result->derivative=probe->derivative;return CALCULATOR_OK;}
    if(work->graph->exact_probes>=128U||numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){result->kind=APPLICATION_GRAPH_UNRESOLVED;work->graph->limited=true;return CALCULATOR_OK;}
    ++work->graph->exact_probes;
    char text[32];(void)snprintf(text,sizeof(text),"%.17g",x);
    CalculatorValue coordinate={0};ApplicationExpressionSample *sample=NULL;
    coordinate.number=bigdecimal_create();coordinate.rational=bigrational_create();coordinate.kind=CALCULATOR_VALUE_RATIONAL;
    status=coordinate.number&&coordinate.rational?CALCULATOR_OK:CALCULATOR_OUT_OF_MEMORY;
    if(status==CALCULATOR_OK)status=calculator_from_decimal_status(bigdecimal_set_string(coordinate.number,text));
    if(status==CALCULATOR_OK)status=calculator_from_rational_status(bigrational_from_bigdecimal(coordinate.rational,coordinate.number));
    if(status==CALCULATOR_OK)status=application_expression_sample(work->exact,&coordinate,1U,&sample,work->error);
    if(status==CALCULATOR_OK){result->kind=application_graph_point_kind(sample);result->approximate=sample->value.kind==CALCULATOR_VALUE_DECIMAL||sample->value.kind==CALCULATOR_VALUE_COMPLEX_DECIMAL;
        if(result->kind==APPLICATION_GRAPH_REAL){status=project(sample->value.number,&result->y);result->derivative=probe->derivative;}}
    calculator_value_destroy(&coordinate);application_expression_samples_destroy(sample,1U);return status;
}
static bool same_sign(double a,double b,double c){return (a>0&&b>0&&c>0)||(a<0&&b<0&&c<0);}
static bool safe(const ApplicationFastProbe *a,const ApplicationFastProbe *b,const ApplicationFastProbe *c,double phase_limit)
{
    if(!a->complete||!b->complete||!c->complete||a->phase_count!=b->phase_count||a->phase_count!=c->phase_count||a->guard_count!=b->guard_count||a->guard_count!=c->guard_count)return false;
    for(size_t i=0;i<a->phase_count;++i){
        if(fabs(a->phases[i]-b->phases[i])>phase_limit||fabs(b->phases[i]-c->phases[i])>phase_limit||fabs(a->phases[i]-c->phases[i])>phase_limit)return false;
        if(a->phase_kinds[i]<0){
            if(a->sample.kind!=APPLICATION_FAST_REAL||b->sample.kind!=APPLICATION_FAST_REAL||c->sample.kind!=APPLICATION_FAST_REAL)return false;
            const double pi=3.14159265358979323846;
            double region=floor((a->phases[i]+pi/2)/pi);
            if(region!=floor((b->phases[i]+pi/2)/pi)||region!=floor((c->phases[i]+pi/2)/pi))return false;
        }
    }
    for(size_t i=0;i<a->guard_count;++i){double av=a->guards[i].value,bv=b->guards[i].value,cv=c->guards[i].value;
        int kind=a->guards[i].kind;
        if(kind==1&&!same_sign(av,bv,cv))return false;
        if(kind==2&&!(av>0&&bv>0&&cv>0))return false;
        if(kind==3&&!(av>=0&&bv>=0&&cv>=0))return false;
        if(kind==4){double exponent=a->guards[i].exponent;
            bool constant=exponent==b->guards[i].exponent&&exponent==c->guards[i].exponent;
            if(constant&&exponent==trunc(exponent)){if(exponent<0&&!same_sign(av,bv,cv))return false;}
            else if(!(av>0&&bv>0&&cv>0))return false;
        }
    }
    return true;
}
static void insert(ApplicationFastGraph *graph,size_t index,ApplicationFastGraphPoint value)
{memmove(graph->points+index+1U,graph->points+index,(graph->count-index)*sizeof(*graph->points));graph->points[index]=value;++graph->count;}
static CalculatorStatus refine(Work *work)
{
    typedef struct Interval {size_t left;unsigned depth;} Interval;
    Interval *queue=numforge_malloc(work->limit*sizeof(*queue));if(!queue)return CALCULATOR_OUT_OF_MEMORY;
    size_t head=0,pending=32;CalculatorStatus status=CALCULATOR_OK;
    for(size_t i=0;i<pending;++i)queue[i]=(Interval){i,0};
    while(pending){
        ApplicationFastGraph *graph=work->graph;
        if(graph->count>=work->limit||numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){graph->limited=true;break;}
        Interval interval=queue[head];head=(head+1U)%work->limit;--pending;
        size_t left=interval.left;ApplicationFastGraphPoint a=graph->points[left],c=graph->points[left+1U],b={0};
        double middle=a.x+(c.x-a.x)/2;if(middle<=a.x||middle>=c.x||interval.depth>=16U){graph->limited=true;continue;}
        ApplicationFastProbe ap={0},bp={0},cp={0};
        status=probe(work,a.x,&ap);
        if(status==CALCULATOR_OK)status=probe(work,c.x,&cp);
        if(status==CALCULATOR_OK)status=point(work,middle,&b,&bp);
        if(status!=CALCULATOR_OK)break;
        insert(graph,left+1U,b);graph->points[left+2U].connect=false;
        double phase_limit=work->smooth&&c.x-a.x<=work->coarse_width?3:0.5;
        /* A restricted continuous AST can use two half-pixel chords once
         * horizontal detail is below the requested screen resolution. Keep
         * phase/domain checks: this must not join poles or hidden periods. */
        bool short_smooth=work->screen_floor&&work->smooth&&c.x-a.x<=work->coarse_width;
        bool accepted=real(&a)&&real(&b)&&real(&c)&&safe(&ap,&bp,&cp,phase_limit)&&
            (short_smooth||fabs(b.y/work->scale-(a.y/work->scale+c.y/work->scale)/2)<=work->tolerance);
        bool empty=!real(&a)&&!real(&b)&&!real(&c);
        if(accepted){graph->points[left+1U].connect=true;graph->points[left+2U].connect=true;}
        for(size_t i=0;i<pending;++i)if(queue[(head+i)%work->limit].left>left)++queue[(head+i)%work->limit].left;
        if(!accepted&&!empty){queue[(head+pending++)%work->limit]=(Interval){left,interval.depth+1U};queue[(head+pending++)%work->limit]=(Interval){left+1U,interval.depth+1U};}
    }
    free(queue);return status;
}
static CalculatorStatus peaks(Work *work)
{
    if(!work->smooth)return CALCULATOR_OK;
    const double pi=3.14159265358979323846;
    for(size_t edge=1;edge<work->graph->count;++edge){
        ApplicationFastGraph *graph=work->graph;if(!graph->points[edge].connect)continue;
        ApplicationFastProbe a={0},b={0};
        CalculatorStatus status=probe(work,graph->points[edge-1U].x,&a);
        if(status==CALCULATOR_OK)status=probe(work,graph->points[edge].x,&b);
        if(status!=CALCULATOR_OK)return status;
        if(!a.complete||!b.complete||a.phase_count!=b.phase_count)continue;
        for(size_t phase=0;phase<a.phase_count;++phase){
            if(a.phase_kinds[phase]<0)continue;
            double pa=a.phases[phase],pb=b.phases[phase],offset=a.phase_kinds[phase]?pi/2:0;
            double target=offset+(floor((fmin(pa,pb)-offset)/pi)+1)*pi;
            if(target>=fmax(pa,pb)||fabs(target-pa)<1e-7||fabs(target-pb)<1e-7)continue;
            double lo=graph->points[edge-1U].x,hi=graph->points[edge].x;
            for(unsigned attempt=0;attempt<8U;++attempt){
                if(graph->count>=work->limit||numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){graph->limited=true;return CALCULATOR_OK;}
                double x=lo+(hi-lo)*(target-pa)/(pb-pa);if(!isfinite(x)||x<=lo||x>=hi)break;
                ApplicationFastGraphPoint value;ApplicationFastProbe observed={0};status=point(work,x,&value,&observed);
                if(status!=CALCULATOR_OK)return status;
                if(!real(&value)||!observed.complete||phase>=observed.phase_count)break;
                double measured=observed.phases[phase];
                if(fabs(measured-target)<1e-6){value.connect=true;insert(graph,edge,value);++edge;break;}
                bool lower=pb>pa?measured<target:measured>target;
                if(lower){lo=x;pa=measured;}else{hi=x;pb=measured;}
            }
        }
    }
    return CALCULATOR_OK;
}
/* Stationary candidates supplement phase anchors. Only search accepted edges,
 * keep a derivative sign bracket and insert actual values, never a synthetic y. */
static CalculatorStatus stationary(Work *work)
{
    if(!work->screen_floor||!work->smooth)return CALCULATOR_OK;
    ApplicationFastGraph *graph=work->graph;
    for(size_t edge=1U;edge<graph->count;++edge){
        ApplicationFastGraphPoint a=graph->points[edge-1U],b=graph->points[edge];
        if(!b.connect||!real(&a)||!real(&b)||!isfinite(a.derivative)||!isfinite(b.derivative))continue;
        if(!((a.derivative<0&&b.derivative>0)||(a.derivative>0&&b.derivative<0)))continue;
        double magnitude=fmax(fabs(a.derivative),fabs(b.derivative));
        if(fmin(fabs(a.derivative),fabs(b.derivative))<=magnitude*1e-8)continue;
        double left=a.x,right=b.x,dl=a.derivative,dr=b.derivative;
        ApplicationFastGraphPoint value={0};bool found=false;
        for(unsigned attempt=0U;attempt<24U;++attempt){
            if(graph->count>=work->limit||numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){graph->limited=true;return CALCULATOR_OK;}
            double fraction=fabs(dl)/(fabs(dl)+fabs(dr));
            if(!isfinite(fraction)||fraction<.1||fraction>.9)fraction=.5;
            double x=left+(right-left)*fraction;if(x<=left||x>=right)break;
            ApplicationFastProbe observed={0};CalculatorStatus status=point(work,x,&value,&observed);
            if(status!=CALCULATOR_OK)return status;
            if(!real(&value)||!observed.complete||!isfinite(value.derivative))break;
            if(fabs(value.derivative)<=magnitude*1e-8||right-left<=work->coarse_width*1e-4){found=true;break;}
            if((dl<0&&value.derivative<0)||(dl>0&&value.derivative>0)){left=x;dl=value.derivative;}
            else{right=x;dr=value.derivative;}
        }
        if(found){value.connect=true;insert(graph,edge,value);++edge;}
    }
    return CALCULATOR_OK;
}
CalculatorStatus application_fast_graph_sample_view(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,double vertical_span,bool coarse,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error)
{
    calculator_error_clear(error);
    if(!result||!input||!context){calculator_error_set(error,CALCULATOR_NULL_ARGUMENT,0U);return CALCULATOR_NULL_ARGUMENT;}
    if(!isfinite(minimum)||!isfinite(maximum)||maximum<=minimum||!isfinite(vertical_span)||vertical_span<0||width<320U||width>2048U||height<160U||height>1200U)return CALCULATOR_INVALID_ARGUMENT;
    if(!isfinite(maximum-minimum))return CALCULATOR_NOT_IMPLEMENTED;
    if(maximum-minimum<fmax(fabs(minimum),fabs(maximum))*DBL_EPSILON*256)return CALCULATOR_NOT_IMPLEMENTED;
    CalculatorStatus status=calculator_context_validate(context,error);if(status!=CALCULATOR_OK)return status;
    bool owner=numforge_budget_begin((uint64_t)context->time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    Work work={0};work.context=context;work.error=error;work.screen_floor=vertical_span>0;work.limit=coarse?width*2U:APPLICATION_FAST_GRAPH_MAX_POINTS;
    status=parameter?application_expression_prepare(&work.exact,input,parameter,session,context,error):application_expression_prepare_auto(&work.exact,input,session,context,error);
    if(status==CALCULATOR_OK){work.smooth=application_expression_graph_smooth(work.exact);status=application_expression_fast_compile(&work.fast,work.exact,error);if(status==CALCULATOR_INVALID_ARGUMENT)status=CALCULATOR_NOT_IMPLEMENTED;}
    if(status==CALCULATOR_OK){work.graph=numforge_calloc(1U,sizeof(*work.graph));if(!work.graph)status=CALCULATOR_OUT_OF_MEMORY;}
    if(status==CALCULATOR_OK){work.graph->points=numforge_calloc(work.limit,sizeof(*work.graph->points));if(!work.graph->points)status=CALCULATOR_OUT_OF_MEMORY;}
    size_t uncertain=0;double low=NAN,high=NAN;
    for(size_t i=0;status==CALCULATOR_OK&&i<33U;++i){
        ApplicationFastProbe probe={0};double x=i==32U?maximum:minimum+(maximum-minimum)*(double)i/32;
        status=point(&work,x,&work.graph->points[i],&probe);if(status!=CALCULATOR_OK)break;
        ++work.graph->count;if(probe.sample.kind==APPLICATION_FAST_UNCERTAIN)++uncertain;
        if(real(&work.graph->points[i])){double y=work.graph->points[i].y;low=isnan(low)?y:fmin(low,y);high=isnan(high)?y:fmax(high,y);}
    }
    if(status==CALCULATOR_OK&&uncertain>16U)status=CALCULATOR_NOT_IMPLEMENTED;
    if(status==CALCULATOR_OK){
        work.graph->minimum=low;work.graph->maximum=high;
        memcpy(work.graph->parameter,application_expression_parameter(work.exact),strlen(application_expression_parameter(work.exact))+1U);
        work.scale=fmax(fabs(low),fabs(high));if(!isfinite(work.scale)||work.scale==0)work.scale=1;
        double span=(high-low)/work.scale;if(!isfinite(span))span=high/work.scale-low/work.scale;if(!isfinite(span)||span<=0)span=1;
        if(vertical_span>0)span=vertical_span/work.scale;
        /* Acceptance emits two half-chords. For smooth local curvature their
         * error is about a quarter of the parent midpoint deviation. */
        work.tolerance=span*(coarse?8:2)/(double)height;work.coarse_width=(maximum-minimum)/(double)width;
        status=refine(&work);if(status==CALCULATOR_OK)status=peaks(&work);
        if(status==CALCULATOR_OK)status=stationary(&work);
        if(status==CALCULATOR_OK&&(work.smooth||!work.graph->limited||isnan(low))){
            low=NAN;high=NAN;for(size_t i=0;i<work.graph->count;++i)if(real(&work.graph->points[i])){double y=work.graph->points[i].y;low=isnan(low)?y:fmin(low,y);high=isnan(high)?y:fmax(high,y);}
            work.graph->minimum=low;work.graph->maximum=high;
        }
    }
    status=calculator_budget_status(status);application_expression_destroy(work.exact);application_fast_expression_destroy(work.fast);
    if(status==CALCULATOR_OK)*result=work.graph;else{application_fast_graph_destroy(work.graph);calculator_error_set(error,status,error?error->offset:0U);}
    if(owner)numforge_budget_end();
    return status;
}
CalculatorStatus application_fast_graph_sample(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,bool coarse,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error)
{
    return application_fast_graph_sample_view(result,input,parameter,minimum,maximum,width,height,0,coarse,session,context,error);
}
