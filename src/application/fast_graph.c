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
    double phase_vertices[APPLICATION_FAST_DENSE_CARRIERS];size_t phase_vertex_count;
    double scale,tolerance,coarse_width,y_minimum,y_maximum,tail_tolerance;size_t limit;bool smooth,screen_floor,camera,exponential,exponential_turns,quadratic_phase,hermite;
} Work;
static bool real(const ApplicationFastGraphPoint *p){return p->kind==APPLICATION_GRAPH_REAL&&isfinite(p->y);}
static CalculatorStatus probe(Work *work,double x,ApplicationFastProbe *result)
{return work->tail_tolerance>0?application_fast_expression_probe_screen(work->fast,x,work->tail_tolerance,result):work->screen_floor?application_fast_expression_probe_derivative(work->fast,x,result):application_fast_expression_probe(work->fast,x,result);}
void application_fast_graph_destroy(ApplicationFastGraph *graph){if(graph){free(graph->points);free(graph->bands);free(graph);}}
static double phase_error(const ApplicationFastDenseCarrier *term,double x)
{return 64*DBL_EPSILON*(fabs(term->phase_square*x)*fabs(x)+fabs(term->phase*x)+fabs(term->phase_offset));}
static bool phase_column(const ApplicationFastDenseCarrier *term,double a,double b,double *low,double *high,bool *dense)
{
    double pa=fma(fma(term->phase_square,a,term->phase),a,term->phase_offset);
    double pb=fma(fma(term->phase_square,b,term->phase),b,term->phase_offset);
    double da=fma(2*term->phase_square,a,term->phase),db=fma(2*term->phase_square,b,term->phase);
    double ea=phase_error(term,a),eb=phase_error(term,b);
    double eda=64*DBL_EPSILON*(fabs(2*term->phase_square*a)+fabs(term->phase));
    double edb=64*DBL_EPSILON*(fabs(2*term->phase_square*b)+fabs(term->phase));
    if(!isfinite(pa)||!isfinite(pb)||!isfinite(da)||!isfinite(db)||!isfinite(ea)||!isfinite(eb)||
        fmax(fabs(pa),fabs(pb))>1e6)return false;
    *low=fmin(pa-ea,pb-eb);*high=fmax(pa+ea,pb+eb);
    *dense=((da>eda&&db>edb)||(da<-eda&&db<-edb))&&
        fabs(pb-pa)>=6.28318530717958647693*(1+64*DBL_EPSILON)+ea+eb;
    if(term->phase_square!=0){double vertex=-0.5*(term->phase/term->phase_square);
        if(isfinite(vertex)&&vertex>a&&vertex<b){
            double value=fma(fma(term->phase_square,vertex,term->phase),vertex,term->phase_offset),error=phase_error(term,vertex);
            if(!isfinite(value)||!isfinite(error)||fabs(value)>1e6)return false;
            *low=fmin(*low,value-error);*high=fmax(*high,value+error);*dense=false;
        }
    }
    return isfinite(*low)&&isfinite(*high)&&(*dense||(*low>=-1e6&&*high<=1e6));
}
/* Bounds include interior trigonometric extrema, not just endpoint samples.
 * Phase/roundoff margins remain approximate screen arithmetic. */
static void trig_range(double a,double b,bool cosine,double *low,double *high)
{
    const double pi=3.14159265358979323846,turn=6.28318530717958647693;
    if(b-a>=turn){*low=-1;*high=1;return;}
    double ya=cosine?cos(a):sin(a),yb=cosine?cos(b):sin(b);
    double peak=cosine?0:pi/2,trough=cosine?pi:-pi/2;
    *low=fmax(-1,fmin(ya,yb)-16*DBL_EPSILON);*high=fmin(1,fmax(ya,yb)+16*DBL_EPSILON);
    if(ceil((a-peak)/turn)<=floor((b-peak)/turn))*high=1;
    if(ceil((a-trough)/turn)<=floor((b-trough)/turn))*low=-1;
}
static CalculatorStatus dense_bands(Work *work,double minimum,double maximum,size_t width)
{
    ApplicationFastDenseModel model={0};
    CalculatorStatus status=application_fast_expression_dense_model(work->fast,&model);
    if(status==CALCULATOR_NOT_IMPLEMENTED)return CALCULATOR_OK;
    if(status!=CALCULATOR_OK)return status;
    for(size_t j=0;j<model.count;++j)if(model.carriers[j].phase_square!=0){
        work->quadratic_phase=true;
        double vertex=-0.5*(model.carriers[j].phase/model.carriers[j].phase_square);
        if(isfinite(vertex)&&vertex>minimum&&vertex<maximum)work->phase_vertices[work->phase_vertex_count++]=vertex;
    }
    const double turn=6.28318530717958647693;
    double step=(maximum-minimum)/(double)width;
    bool potential=false;
    for(size_t j=0;j<model.count;++j){double phase_step=fabs(model.carriers[j].phase)*step;
        if(model.carriers[j].phase_square!=0||(isfinite(phase_step)&&phase_step>=turn*(1+64*DBL_EPSILON)))potential=true;
    }
    if(!potential)return CALCULATOR_OK;
    work->graph->bands=numforge_calloc(width,sizeof(*work->graph->bands));
    if(!work->graph->bands)return CALCULATOR_OUT_OF_MEMORY;
    for(size_t i=0;i<width;++i){
        if(!numforge_budget_check())return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
        double a=minimum+(maximum-minimum)*(double)i/(double)width;
        double b=i+1U==width?maximum:minimum+(maximum-minimum)*(double)(i+1U)/(double)width;
        if(b<=a)continue;
        double low=0,high=0;bool valid=true,dense=false;
        for(size_t j=0;j<model.count;++j){const ApplicationFastDenseCarrier *term=&model.carriers[j];
            double phase_low=0,phase_high=0,tl=-1,th=1;bool carrier_dense=false;
            double aa=fma(term->amplitude,a,term->amplitude_offset),ab=fma(term->amplitude,b,term->amplitude_offset);
            if(!phase_column(term,a,b,&phase_low,&phase_high,&carrier_dense)||!isfinite(aa)||!isfinite(ab)){valid=false;break;}
            dense=dense||carrier_dense;
            if(!carrier_dense)trig_range(phase_low,phase_high,term->cosine,&tl,&th);
            double error=64*DBL_EPSILON*fmax(fabs(term->amplitude*a)+fabs(term->amplitude_offset),fabs(term->amplitude*b)+fabs(term->amplitude_offset));
            double al=fmin(aa,ab)-error,ah=fmax(aa,ab)+error;
            double ll=al*tl,lh=al*th,hl=ah*tl,hh=ah*th;
            double term_low=fmin(fmin(ll,lh),fmin(hl,hh)),term_high=fmax(fmax(ll,lh),fmax(hl,hh));
            if(!isfinite(ll)||!isfinite(lh)||!isfinite(hl)||!isfinite(hh)){valid=false;break;}
            low=nextafter(low+term_low,-INFINITY);high=nextafter(high+term_high,INFINITY);
        }
        if(!valid||!dense)continue;
        double ya=fma(model.offset,a,model.offset_value),yb=fma(model.offset,b,model.offset_value);
        low=nextafter(low+fmin(ya,yb),-INFINITY);high=nextafter(high+fmax(ya,yb),INFINITY);
        if(!isfinite(ya)||!isfinite(yb)||!isfinite(low)||!isfinite(high)||low>high)continue;
        work->graph->bands[work->graph->band_count++]=(ApplicationFastGraphBand){a,b,low,high};
    }
    return CALCULATOR_OK;
}
static bool band_covered(const ApplicationFastGraph *graph,double low,double high)
{
    size_t left=0,right=graph->band_count;
    while(left<right){size_t middle=left+(right-left)/2;if(graph->bands[middle].high<=low)left=middle+1U;else right=middle;}
    double end=low;
    for(size_t i=left;i<graph->band_count&&end<high;++i){
        if(graph->bands[i].low>end)return false;
        end=fmax(end,graph->bands[i].high);
    }
    return end>=high;
}
static CalculatorStatus project(const BigDecimal *number,double *value)
{
    char *text=NULL;CalculatorStatus status=calculator_from_decimal_status(bigdecimal_format_mode(number,
        16,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,128U,&text));
    if(status==CALCULATOR_OK){char *end=NULL;errno=0;double x=strtod(text,&end);*value=errno==ERANGE||!isfinite(x)||end==text||*end?NAN:x;}
    free(text);return status;
}
static CalculatorStatus point(Work *work,double x,ApplicationFastGraphPoint *result,ApplicationFastProbe *observed)
{
    CalculatorStatus status=probe(work,x,observed);
    if(status!=CALCULATOR_OK)return status;
    *result=(ApplicationFastGraphPoint){x,NAN,APPLICATION_GRAPH_REAL,false,true,false,NAN};
    if(observed->sample.kind==APPLICATION_FAST_REAL){result->y=observed->sample.value;result->derivative=observed->derivative;return CALCULATOR_OK;}
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
        if(result->kind==APPLICATION_GRAPH_REAL){status=project(sample->value.number,&result->y);result->derivative=observed->derivative;}}
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
/* A phase vertex can hide between equal seed phases. Evaluate the original
 * expression there, without assigning an output extremum or accepting edges. */
static CalculatorStatus quadratic_phase_seeds(Work *work)
{
    ApplicationFastGraph *graph=work->graph;
    for(size_t i=0;i<work->phase_vertex_count;++i){double x=work->phase_vertices[i];size_t index=0;
        while(index<graph->count&&graph->points[index].x<x)++index;
        if(index<graph->count&&graph->points[index].x==x)continue;
        if(graph->count>=work->limit){graph->limited=true;return CALCULATOR_OK;}
        ApplicationFastProbe observed={0};ApplicationFastGraphPoint value={0};
        CalculatorStatus status=point(work,x,&value,&observed);if(status!=CALCULATOR_OK)return status;
        insert(graph,index,value);
    }
    return CALCULATOR_OK;
}
static bool turn(double a,double b)
{return isfinite(a)&&isfinite(b)&&((a<0&&b>0)||(a>0&&b<0));}
/* Search argument turns before exp flattens its tails to screen zero. These
 * are discovery seeds, not accepted curve edges or synthetic function values. */
static CalculatorStatus exponential_seeds(Work *work)
{
    if(!work->screen_floor||!work->exponential)return CALCULATOR_OK;
    ApplicationFastGraph *graph=work->graph;
    for(size_t edge=32U;edge>0;--edge){
        double start=graph->points[edge-1U].x,end=graph->points[edge].x;
        ApplicationFastProbe a={0},c={0};CalculatorStatus status=probe(work,start,&a);
        if(status==CALCULATOR_OK)status=probe(work,end,&c);
        if(status!=CALCULATOR_OK)return status;
        if(!a.complete||!c.complete||a.exponential_count!=c.exponential_count)continue;
        for(size_t slot=0;slot<a.exponential_count;++slot){
            double dl=a.exponential_slopes[slot],dr=c.exponential_slopes[slot];
            bool endpoint=(dl==0&&isfinite(dr)&&dr!=0)||(dr==0&&isfinite(dl)&&dl!=0);
            if(!turn(dl,dr)&&!endpoint)continue;
            double left=start,right=end;bool found=endpoint;double x=dl==0?start:end;
            for(unsigned attempt=0;!found&&attempt<40U;++attempt){
                if(graph->count>=work->limit||numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){graph->limited=true;return CALCULATOR_OK;}
                x=left+(right-left)/2;if(x<=left||x>=right)break;
                ApplicationFastProbe b={0};status=probe(work,x,&b);if(status!=CALCULATOR_OK)return status;
                if(!b.complete||slot>=b.exponential_count||!safe(&a,&b,&c,.5))break;
                double d=b.exponential_slopes[slot];if(!isfinite(d))break;
                if(d==0||attempt==39U){found=true;break;}
                if((dl<0&&d<0)||(dl>0&&d>0)){left=x;dl=d;}else right=x;
            }
            if(found){
                if(graph->count>=work->limit||numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){graph->limited=true;return CALCULATOR_OK;}
                ApplicationFastGraphPoint value={0};ApplicationFastProbe b={0};status=point(work,x,&value,&b);
                if(status!=CALCULATOR_OK)return status;
                if(!real(&value))continue;
                work->exponential_turns=true;
                size_t index=edge;while(index<graph->count&&graph->points[index].x<x)++index;
                if(x==start)index=edge-1U;
                value.turn_anchor=true;
                if(index<graph->count&&graph->points[index].x==x)graph->points[index].turn_anchor=true;
                else insert(graph,index,value);
                // A curvature-based spacing estimate supplies actual shoulder
                // seeds even when the viewing interval dwarfs the feature.
                double curvature=(c.exponential_slopes[slot]-a.exponential_slopes[slot])/(end-start);
                double spacing=sqrt(2/fabs(curvature));
                for(int shoulder=-2;shoulder<=2;++shoulder){
                    if(!shoulder||!isfinite(spacing)||spacing<=0)continue;
                    double sx=x+shoulder*spacing;if(sx<=start||sx>=end||sx==x)continue;
                    if(graph->count>=work->limit){graph->limited=true;return CALCULATOR_OK;}
                    status=point(work,sx,&value,&b);if(status!=CALCULATOR_OK)return status;
                    if(!real(&value))continue;
                    index=edge;while(index<graph->count&&graph->points[index].x<sx)++index;
                    if(index<graph->count&&graph->points[index].x==sx)continue;
                    insert(graph,index,value);
                }
            }
        }
    }
    return CALCULATOR_OK;
}
/* A guarded derivative bracket can reveal a narrow rational feature hidden
 * between seed values. Use an actual evaluation as the split, then let normal
 * refinement validate both neighbouring edges. Never manufacture a connection. */
static CalculatorStatus turning_split(Work *work,const ApplicationFastGraphPoint *a,
    const ApplicationFastGraphPoint *c,const ApplicationFastProbe *ap,
    const ApplicationFastProbe *cp,ApplicationFastGraphPoint *b,ApplicationFastProbe *bp)
{
    if(b->derivative==0){b->turn_anchor=true;return CALCULATOR_OK;}
    double left=a->x,right=c->x,dl=a->derivative;
    bool interpolated=work->hermite&&work->quadratic_phase;
    unsigned attempts=interpolated?12U:32U;
    double phase_limit=interpolated?3:0.5;
    for(unsigned attempt=0U;attempt<attempts;++attempt){
        if(numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){work->graph->limited=true;return CALCULATOR_OK;}
        double x=left+(right-left)/2;if(x<=left||x>=right)break;
        ApplicationFastProbe observed={0};ApplicationFastGraphPoint value={0};
        CalculatorStatus status=probe(work,x,&observed);
        if(status!=CALCULATOR_OK)return status;
        // An off-screen candidate needs no optional stationary search. In
        // particular, do not spend exact probes chasing an invisible pole.
        double margin=work->tolerance*work->scale*2;
        if(!isfinite(observed.sample.value)||(!interpolated&&work->camera&&
            (observed.sample.value<work->y_minimum-margin||observed.sample.value>work->y_maximum+margin)))return CALCULATOR_OK;
        if(observed.sample.kind==APPLICATION_FAST_REAL)
            value=(ApplicationFastGraphPoint){x,observed.sample.value,APPLICATION_GRAPH_REAL,false,true,false,observed.derivative};
        else status=point(work,x,&value,&observed);
        if(status!=CALCULATOR_OK)return status;
        if(!real(&value)||(!interpolated&&work->camera&&(value.y<work->y_minimum-margin||value.y>work->y_maximum+margin))||
            !isfinite(observed.derivative)||!safe(ap,&observed,cp,phase_limit))return CALCULATOR_OK;
        if(observed.sample.kind==APPLICATION_FAST_UNCERTAIN||observed.derivative==0||attempt+1U==attempts){
            value.turn_anchor=true;*b=value;
            *bp=observed;return CALCULATOR_OK;
        }
        if((dl<0&&observed.derivative<0)||(dl>0&&observed.derivative>0)){left=x;dl=observed.derivative;}
        else right=x;
    }
    return CALCULATOR_OK;
}
static double cubic_midpoint(const ApplicationFastGraphPoint *a,const ApplicationFastGraphPoint *b)
{
    double dx=b->x-a->x,dy=b->y-a->y,secant=dy/dx;
    if(!isfinite(a->derivative)||!isfinite(b->derivative)||!isfinite(secant)||dx<=0||dy==0)return a->y/2+b->y/2;
    double alpha=fmax(0,a->derivative/secant),beta=fmax(0,b->derivative/secant),norm=hypot(alpha,beta);
    if(!isfinite(alpha)||!isfinite(beta)||!isfinite(norm))return NAN;
    if(norm>3){alpha*=3/norm;beta*=3/norm;}
    return a->y/2+b->y/2+dy*(alpha-beta)/8;
}
static CalculatorStatus hermite_check(Work *work,const ApplicationFastGraphPoint *a,
    const ApplicationFastGraphPoint *b,const ApplicationFastGraphPoint *c,
    const ApplicationFastProbe *ap,const ApplicationFastProbe *bp,const ApplicationFastProbe *cp,bool *accepted)
{
    *accepted=false;
    if(!isfinite(a->derivative)||!isfinite(b->derivative)||!isfinite(c->derivative)||!safe(ap,bp,cp,3))return CALCULATOR_OK;
    ApplicationFastProbe left={0},right={0};
    CalculatorStatus status=probe(work,a->x+(b->x-a->x)/2,&left);
    if(status==CALCULATOR_OK)status=probe(work,b->x+(c->x-b->x)/2,&right);
    if(status!=CALCULATOR_OK)return status;
    if(left.sample.kind!=APPLICATION_FAST_REAL||right.sample.kind!=APPLICATION_FAST_REAL||
        !safe(ap,&left,bp,1.5)||!safe(bp,&right,cp,1.5))return CALCULATOR_OK;
    double low=cubic_midpoint(a,b),high=cubic_midpoint(b,c);
    *accepted=isfinite(low)&&isfinite(high)&&
        fabs(left.sample.value/work->scale-low/work->scale)<=work->tolerance/4&&
        fabs(right.sample.value/work->scale-high/work->scale)<=work->tolerance/4;
    return CALCULATOR_OK;
}
static CalculatorStatus refine(Work *work)
{
    typedef struct Interval {size_t left;unsigned depth;} Interval;
    Interval *queue=numforge_malloc(work->limit*sizeof(*queue));if(!queue)return CALCULATOR_OUT_OF_MEMORY;
    size_t head=0,pending=work->graph->count-1U;CalculatorStatus status=CALCULATOR_OK;
    for(size_t i=0;i<pending;++i)queue[i]=(Interval){i,0};
    while(pending){
        ApplicationFastGraph *graph=work->graph;
        Interval interval=queue[head];head=(head+1U)%work->limit;--pending;
        size_t left=interval.left;ApplicationFastGraphPoint a=graph->points[left],c=graph->points[left+1U],b={0};
        /* A separately classified envelope covers dense continuous detail.
         * Never promote its boundaries into accepted point connections. */
        if(band_covered(graph,a.x,c.x))continue;
        if(graph->count>=work->limit||numforge_budget_available_bytes()<3U*1024U*1024U||numforge_budget_available_ms()<300U){graph->limited=true;break;}
        double middle=a.x+(c.x-a.x)/2;if(middle<=a.x||middle>=c.x||interval.depth>=16U){graph->limited=true;continue;}
        ApplicationFastProbe ap={0},bp={0},cp={0};
        status=probe(work,a.x,&ap);
        if(status==CALCULATOR_OK)status=probe(work,c.x,&cp);
        if(status==CALCULATOR_OK)status=point(work,middle,&b,&bp);
        if(status!=CALCULATOR_OK)break;
        double ad=a.turn_anchor?0:a.derivative,cd=c.turn_anchor?0:c.derivative;
        bool turning=work->screen_floor&&(!work->smooth||(work->hermite&&work->quadratic_phase))&&real(&a)&&real(&b)&&real(&c)&&
            (turn(ad,b.derivative)||turn(b.derivative,cd)||turn(ad,cd));
        // A trend can hide the sign change in the first probes. Explore a
        // sampled bend in f' until screen scale, then let actual turns take over.
        double gradient=fmax(fabs(ad),fmax(fabs(b.derivative),fabs(cd)));
        bool bending=work->screen_floor&&!work->smooth&&c.x-a.x>work->coarse_width&&
            isfinite(ad)&&isfinite(b.derivative)&&isfinite(cd)&&
            (b.derivative<fmin(ad,cd)-gradient*1e-6||b.derivative>fmax(ad,cd)+gradient*1e-6);
        if(turning&&turn(ad,cd)&&safe(&ap,&bp,&cp,work->hermite&&work->quadratic_phase?3:0.5)){
            status=turning_split(work,&a,&c,&ap,&cp,&b,&bp);
            if(status!=CALCULATOR_OK)break;
        }
        insert(graph,left+1U,b);graph->points[left+2U].connect=false;
        double phase_limit=work->smooth&&!work->quadratic_phase&&c.x-a.x<=work->coarse_width?3:0.5;
        /* A restricted continuous AST can use two half-pixel chords once
         * horizontal detail is below the requested screen resolution. Keep
         * phase/domain checks: this must not join poles or hidden periods. */
        bool short_smooth=work->screen_floor&&work->smooth&&!work->exponential_turns&&!work->quadratic_phase&&c.x-a.x<=work->coarse_width;
        // A root is not necessarily the midpoint: always recheck both children
        // at their own midpoints before accepting the new neighbouring edges.
        bool accepted=!turning&&!bending&&real(&a)&&real(&b)&&real(&c)&&safe(&ap,&bp,&cp,phase_limit)&&
            (short_smooth||fabs(b.y/work->scale-(a.y/work->scale+c.y/work->scale)/2)<=work->tolerance);
        if(!accepted&&!turning&&work->hermite&&work->quadratic_phase&&work->smooth&&real(&a)&&real(&b)&&real(&c)){
            status=hermite_check(work,&a,&b,&c,&ap,&bp,&cp,&accepted);if(status!=CALCULATOR_OK)break;
            if(accepted)graph->subpixel_hermite=true;
        }
        bool empty=!real(&a)&&!real(&b)&&!real(&c);
        // Spend detail on the visible curve. Monotone guarded probes in a small
        // interval outside the camera may stop without accepting any edge.
        // A changed y view must request fresh data before these gaps become visible.
        double margin=work->tolerance*work->scale*2;
        bool clipped=!accepted&&!work->smooth&&work->camera&&c.x-a.x<=8*work->coarse_width&&
            real(&a)&&real(&b)&&real(&c)&&safe(&ap,&bp,&cp,0.5)&&
            isfinite(a.derivative)&&isfinite(b.derivative)&&isfinite(c.derivative)&&
            same_sign(a.derivative,b.derivative,c.derivative)&&
            ((a.y>work->y_maximum+margin&&b.y>work->y_maximum+margin&&c.y>work->y_maximum+margin)||
             (a.y<work->y_minimum-margin&&b.y<work->y_minimum-margin&&c.y<work->y_minimum-margin));
        if(clipped)graph->screen_clipped=true;
        if(accepted){graph->points[left+1U].connect=true;graph->points[left+2U].connect=true;}
        for(size_t i=0;i<pending;++i)if(queue[(head+i)%work->limit].left>left)++queue[(head+i)%work->limit].left;
        if(!accepted&&!empty&&!clipped){queue[(head+pending++)%work->limit]=(Interval){left,interval.depth+1U};queue[(head+pending++)%work->limit]=(Interval){left+1U,interval.depth+1U};}
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
static CalculatorStatus sample_view(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,double vertical_span,double y_minimum,double y_maximum,bool camera,bool coarse,bool dense,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error,bool hermite)
{
    calculator_error_clear(error);
    if(!result||!input||!context){calculator_error_set(error,CALCULATOR_NULL_ARGUMENT,0U);return CALCULATOR_NULL_ARGUMENT;}
    if(!isfinite(minimum)||!isfinite(maximum)||maximum<=minimum||!isfinite(vertical_span)||vertical_span<0||(dense&&vertical_span==0)||width<320U||width>2048U||height<160U||height>1200U)return CALCULATOR_INVALID_ARGUMENT;
    if(!isfinite(maximum-minimum))return CALCULATOR_NOT_IMPLEMENTED;
    if(maximum-minimum<fmax(fabs(minimum),fabs(maximum))*DBL_EPSILON*256)return CALCULATOR_NOT_IMPLEMENTED;
    CalculatorStatus status=calculator_context_validate(context,error);if(status!=CALCULATOR_OK)return status;
    bool owner=numforge_budget_begin((uint64_t)context->time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    Work work={0};work.context=context;work.error=error;work.screen_floor=vertical_span>0;work.limit=coarse?width*2U:APPLICATION_FAST_GRAPH_MAX_POINTS;work.hermite=hermite;
    work.tail_tolerance=vertical_span/(double)height/64;
    status=parameter?application_expression_prepare(&work.exact,input,parameter,session,context,error):application_expression_prepare_auto(&work.exact,input,session,context,error);
    if(status==CALCULATOR_OK){work.smooth=application_expression_graph_smooth(work.exact);status=application_expression_fast_compile(&work.fast,work.exact,error);if(status==CALCULATOR_INVALID_ARGUMENT)status=CALCULATOR_NOT_IMPLEMENTED;}
    if(status==CALCULATOR_OK){work.graph=numforge_calloc(1U,sizeof(*work.graph));if(!work.graph)status=CALCULATOR_OUT_OF_MEMORY;}
    if(status==CALCULATOR_OK){work.graph->points=numforge_calloc(work.limit,sizeof(*work.graph->points));if(!work.graph->points)status=CALCULATOR_OUT_OF_MEMORY;}
    work.camera=camera;work.y_minimum=y_minimum;work.y_maximum=y_maximum;
    size_t uncertain=0;double low=NAN,high=NAN;bool complex_recovery=false;
    for(size_t i=0;status==CALCULATOR_OK&&i<33U;++i){
        ApplicationFastProbe probe={0};double x=i==32U?maximum:minimum+(maximum-minimum)*(double)i/32;
        status=point(&work,x,&work.graph->points[i],&probe);if(status!=CALCULATOR_OK)break;
        if(probe.exponential_count)work.exponential=true;
        ++work.graph->count;
        ApplicationGraphPointKind kind=work.graph->points[i].kind;
        // Confirmed complex/domain samples do not make the real portion
        // numerically uncertain. Exact evaluation remains the classifier.
        if(probe.sample.kind==APPLICATION_FAST_UNCERTAIN&&kind!=APPLICATION_GRAPH_COMPLEX&&kind!=APPLICATION_GRAPH_DOMAIN_ERROR)++uncertain;
        if(probe.non_real_intermediate&&kind==APPLICATION_GRAPH_REAL)complex_recovery=true;
        if(real(&work.graph->points[i])){double y=work.graph->points[i].y;low=isnan(low)?y:fmin(low,y);high=isnan(high)?y:fmax(high,y);}
    }
    if(status==CALCULATOR_OK&&(uncertain>16U||complex_recovery))status=CALCULATOR_NOT_IMPLEMENTED;
    if(status==CALCULATOR_OK&&dense&&work.screen_floor)status=dense_bands(&work,minimum,maximum,width);
    if(status==CALCULATOR_OK)status=quadratic_phase_seeds(&work);
    if(status==CALCULATOR_OK)status=exponential_seeds(&work);
    if(status==CALCULATOR_OK){
        if(work.exponential)for(size_t i=0;i<work.graph->count;++i)if(real(&work.graph->points[i])){low=isnan(low)?work.graph->points[i].y:fmin(low,work.graph->points[i].y);high=isnan(high)?work.graph->points[i].y:fmax(high,work.graph->points[i].y);}
        work.graph->minimum=low;work.graph->maximum=high;
        memcpy(work.graph->parameter,application_expression_parameter(work.exact),strlen(application_expression_parameter(work.exact))+1U);
        work.scale=fmax(fabs(low),fabs(high));if(!isfinite(work.scale)||work.scale==0)work.scale=1;
        double span=(high-low)/work.scale;if(!isfinite(span))span=high/work.scale-low/work.scale;if(!isfinite(span)||span<=0)span=1;
        if(vertical_span>0)span=vertical_span/work.scale;
        /* Acceptance emits two half-chords. For smooth local curvature their
         * error is about a quarter of the parent midpoint deviation. */
        work.tolerance=span*(coarse?8:2)/(double)height;work.coarse_width=(maximum-minimum)/(double)width;
        status=refine(&work);if(status==CALCULATOR_OK&&!work.graph->subpixel_hermite)status=peaks(&work);
        if(status==CALCULATOR_OK&&!work.graph->subpixel_hermite)status=stationary(&work);
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
CalculatorStatus application_fast_graph_sample_view_dense(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,double vertical_span,bool coarse,bool dense,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error)
{
    return sample_view(result,input,parameter,minimum,maximum,width,height,vertical_span,0,0,false,coarse,dense,session,context,error,false);
}
CalculatorStatus application_fast_graph_sample_camera(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,double y_minimum,double y_maximum,bool coarse,bool dense,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error)
{
    if(!isfinite(y_minimum)||!isfinite(y_maximum)||y_maximum<=y_minimum||!isfinite(y_maximum-y_minimum)){
        calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,0U);return CALCULATOR_INVALID_ARGUMENT;
    }
    return sample_view(result,input,parameter,minimum,maximum,width,height,y_maximum-y_minimum,y_minimum,y_maximum,true,coarse,dense,session,context,error,false);
}
CalculatorStatus application_fast_graph_sample_camera_hermite(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,double y_minimum,double y_maximum,bool coarse,bool dense,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error)
{
    if(!isfinite(y_minimum)||!isfinite(y_maximum)||y_maximum<=y_minimum||!isfinite(y_maximum-y_minimum)){
        calculator_error_set(error,CALCULATOR_INVALID_ARGUMENT,0U);return CALCULATOR_INVALID_ARGUMENT;
    }
    return sample_view(result,input,parameter,minimum,maximum,width,height,y_maximum-y_minimum,y_minimum,y_maximum,true,coarse,dense,session,context,error,true);
}
CalculatorStatus application_fast_graph_sample_view(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,double vertical_span,bool coarse,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error)
{
    return application_fast_graph_sample_view_dense(result,input,parameter,minimum,maximum,width,height,vertical_span,coarse,false,session,context,error);
}
CalculatorStatus application_fast_graph_sample(ApplicationFastGraph **result,const char *input,const char *parameter,
    double minimum,double maximum,size_t width,size_t height,bool coarse,const CalculatorSession *session,
    const CalculatorContext *context,CalculatorError *error)
{
    return application_fast_graph_sample_view(result,input,parameter,minimum,maximum,width,height,0,coarse,session,context,error);
}
