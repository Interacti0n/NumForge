#include "graph.h"
#include "session_api.h"
#include "benchmark_clock.h"
#include "../src/internal/numforge_alloc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Timings exclude destruction and independent-reference projection. Allocation
 * live tracking stays off: its bookkeeping would dominate tiny math objects. */
typedef struct GraphCase {const char *name,*expression,*low,*high;int reference,precision;} GraphCase;
static const GraphCase cases[]={
 {"line","3*x+2","-10","10",0,34},{"square","x*x","-10","10",1,34},
 {"sine","sin(x)","-10","10",2,34},{"sine3","sin(3*x)","-10","10",3,34},
 {"sine33","sin(33*x)","-10","10",10,34},
 {"sine_image","sin(x)","-100","611",2,34},
 {"mixed","sin(3*x)+cos(2*x)","-10","10",4,34},
 {"nested","sin(3*x)*exp(-x*x/20)","-10","10",5,34},
 {"log","ln(1+x*x)","-1000","1000",6,34},
 {"sine_wide","sin(3*x)","-1000","1000",3,34},
 {"sine_million","sin(x)","-1000000","1000000",2,34},
 {"oscillation","sin(x*x)","-100","100",7,34},
 {"pole","1/(x-0.1)","-10","10",8,34},
 {"tangent","tan(x)","-10","10",9,34},
 {"reciprocal","1/x","-10","10",11,34},
 {"shifted_pole","1/(x-2)","-10","10",12,34},
 {"sqrt_domain","sqrt(x)","-10","10",13,34},
 {"log_domain","ln(x)","-10","10",14,34},
 {"sine100","sin(3*x)","-1","1",3,100},
 {"line_trillion","3*x+2","-1000000000000","1000000000000",0,34},
 {"log_trillion","ln(1+x*x)","-1000000000000","1000000000000",6,34}};
static double reference(int kind,double x){switch(kind){case 0:return 3*x+2;case 1:return x*x;case 2:return sin(x);case 3:return sin(3*x);case 4:return sin(3*x)+cos(2*x);case 5:return sin(3*x)*exp(-x*x/20);case 6:return log1p(x*x);case 7:return sin(x*x);case 8:return 1/(x-.1);case 10:return sin(33*x);case 11:return 1/x;case 12:return 1/(x-2);case 13:return sqrt(x);case 14:return log(x);default:return tan(x);}}
static double project(const BigDecimal *value){char *text=NULL;double result=NAN;if(bigdecimal_to_string(value,&text)==BIGDECIMAL_OK){result=strtod(text,NULL);free(text);}return result;}
/* Point objects only, using the adapter's 16-place projection. Outside timing;
 * root metadata is additional. This does not bypass or enlarge HTTP limits. */
static size_t projected_bytes(const ApplicationGraph *graph,const char *expression){
 size_t bytes=0;
 for(size_t i=0;i<graph->count;++i){char *x=NULL,*y=NULL;
  if(bigdecimal_format_mode(graph->coordinates[i].number,16,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,128U,&x)!=BIGDECIMAL_OK)return 0;
  ApplicationGraphPointKind kind=application_graph_point_kind(&graph->samples[i]);
  if(kind==APPLICATION_GRAPH_REAL&&bigdecimal_format_mode(graph->samples[i].value.number,16,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,128U,&y)!=BIGDECIMAL_OK){free(x);return 0;}
  const char *kinds[]={"real","domain_error","complex","quantity"};
  bytes+=(size_t)snprintf(NULL,0,"%s{\"connect\":%s,\"x\":\"%s\",\"kind\":\"%s\",\"y\":%s%s%s,\"source_approximate\":%s}",
   i?",":"",graph->connect[i]?"true":"false",x,kinds[kind],y?"\"":"",y?y:"null",y?"\"":"",
   graph->samples[i].error.status==CALCULATOR_OK&&(graph->samples[i].value.kind==CALCULATOR_VALUE_DECIMAL||graph->samples[i].value.kind==CALCULATOR_VALUE_COMPLEX_DECIMAL)?"true":"false");
  if(kind==APPLICATION_GRAPH_DOMAIN_ERROR)bytes+=(size_t)snprintf(NULL,0,",\"status\":\"%s\",\"column\":%zu",calculator_status_to_string(graph->samples[i].error.status),calculator_error_column(expression,graph->samples[i].error.offset));
  free(x);free(y);
 }
 return bytes;
}
int main(int argc,char **argv){
 int repeats=argc>1?atoi(argv[1]):3;if(repeats<1||repeats>10||argc>3)return 2;
 size_t point_limit=argc>2?(size_t)strtoul(argv[2],NULL,10):APPLICATION_GRAPH_MAX_ADAPTIVE_SAMPLES;
 if(point_limit!=1024U&&point_limit!=2048U&&point_limit!=4096U&&point_limit!=8192U)return 2;
 if(argc>2)fprintf(stderr,"cap=%zu coordinate_bytes=%zu sample_bytes=%zu single_allocation_limit=%u\n",point_limit,
  point_limit*sizeof(CalculatorValue),point_limit*sizeof(ApplicationExpressionSample),(unsigned)CALCULATOR_SINGLE_ALLOCATION);
 int inaccurate=0;
 puts("case,mode,repeat,status,ms,calls,bytes,points,limited,point_error,chord_error,screen_error,connected_fraction,point_cap,projected_point_bytes");
 for(size_t c=0;c<sizeof(cases)/sizeof(cases[0]);++c)for(int mode=0;mode<3;++mode)for(int run=0;run<repeats;++run){
  if(argc>2&&mode!=1)continue;
  const GraphCase *item=&cases[c];CalculatorContext context;calculator_context_init(&context);
  context.division_scale=item->precision;
  BigDecimal *value=bigdecimal_create();BigRational *low=bigrational_create(),*high=bigrational_create();
  if(!value||!low||!high)return 2;
  if(bigdecimal_set_string(value,item->low)!=BIGDECIMAL_OK||bigrational_from_bigdecimal(low,value)!=BIGRATIONAL_OK||bigdecimal_set_string(value,item->high)!=BIGDECIMAL_OK||bigrational_from_bigdecimal(high,value)!=BIGRATIONAL_OK)return 2;
  ApplicationGraph *graph=NULL;CalculatorError error;char *response=NULL;ApplicationClientStore store={0};
  numforge_alloc_stats_reset();double start=benchmark_seconds();
  CalculatorStatus status;
  if(mode==0)status=application_graph_sample(&graph,item->expression,"x",low,high,129,NULL,&context,&error);
  else if(mode==1)status=application_graph_adaptive_limit(&graph,item->expression,NULL,low,high,point_limit,NULL,&context,&error);
  else{char target[160];snprintf(target,sizeof(target),"/api/graph/plot?min=%s&max=%s&precision=%d",item->low,item->high,item->precision);status=numforge_web_session_request(&store,"POST",target,item->expression,&response);}
  double elapsed=(benchmark_seconds()-start)*1000;size_t calls=numforge_alloc_stats_calls(),bytes=numforge_alloc_stats_bytes();
  double point_error=0,chord_error=0,screen_error=0,connected_fraction=0;
  double view_span=graph&&graph->view_minimum&&graph->view_maximum?project(graph->view_maximum)-project(graph->view_minimum):0;
  if(status==CALCULATOR_OK&&graph){for(size_t i=0;i<graph->count;++i){if(application_graph_point_kind(&graph->samples[i])!=APPLICATION_GRAPH_REAL)continue;double x=project(graph->coordinates[i].number),y=project(graph->samples[i].value.number),expected=reference(item->reference,x);double e=fabs(y-expected)/fmax(1,fabs(expected));if(e>point_error)point_error=e;
   if(!isfinite(e)||e>1e-9)inaccurate=1;
   if(i&&mode==1&&graph->connect[i]){double a=project(graph->coordinates[i-1].number),ya=project(graph->samples[i-1].value.number);connected_fraction+=(x-a)/(strtod(item->high,NULL)-strtod(item->low,NULL));for(int probe=1;probe<=3;++probe){double t=(double)probe/4,xx=a+(x-a)*t,yy=ya+(y-ya)*t,truth=reference(item->reference,xx);double gap=fabs(yy-truth)/fmax(1,fabs(truth));if(gap>chord_error)chord_error=gap;double screen=fabs(yy-truth)/fmax(1,view_span);if(screen>screen_error)screen_error=screen;}}
  }}
  size_t points=graph?graph->count:0;bool limited=graph?graph->limited:false;
  if(response&&status==CALCULATOR_OK){const char *count=strstr(response,"\"count\":");if(count)points=(size_t)strtoull(count+8,NULL,10);limited=strstr(response,"\"limited\":true")!=NULL;}
  size_t projection=mode==1&&graph?projected_bytes(graph,item->expression):0;
  printf("%s,%s,%d,%d,%.3f,%zu,%zu,%zu,%d,%.6g,%.6g,%.6g,%.6g,%zu,%zu\n",item->name,mode==0?"uniform129":mode==1?"adaptive":"http",run,(int)status,elapsed,calls,bytes,points,limited,point_error,chord_error,screen_error,connected_fraction,mode==1?point_limit:mode==0?129U:APPLICATION_GRAPH_MAX_ADAPTIVE_SAMPLES,projection);fflush(stdout);
  application_graph_destroy(graph);free(response);bigdecimal_destroy(value);bigrational_destroy(low);bigrational_destroy(high);application_client_store_destroy(&store);
 }
 return inaccurate;
}
