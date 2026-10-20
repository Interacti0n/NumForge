#include "src/application/graphs/graph.h"
#include "src/application/expressions/fast_expression.h"
#include "benchmarks/common/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Case {const char *name,*expression,*low,*high;int reference;} Case;
static const Case cases[]={
 {"sine","sin(x)","-10","10",0},
 {"sine33","sin(33x)","-10","10",1},
 {"mixed","sin(3x)+cos(2x)","-10","10",2},
 {"nested","sin(3x)*exp(-x*x/20)","-10","10",3},
 {"tangent","tan(x)","-10","10",4},
 {"reciprocal","1/x","-10","10",5},
 {"sqrt_domain","sqrt(x)","-10","10",6},
 {"log_domain","ln(x)","-10","10",7},
 {"sine_wide","sin(3x)","-1000","1000",8},
 {"sine_million","sin(x)","-1000000","1000000",0},
 {"sine_trillion","sin(x)","-1000000000000","1000000000000",0},
 {"cancellation","(10000000000000000+x)-10000000000000000","-10","10",9}};
static double reference(int kind,double x){switch(kind){
 case 0:return sin(x);case 1:return sin(33*x);case 2:return sin(3*x)+cos(2*x);
 case 3:return sin(3*x)*exp(-x*x/20);case 4:return tan(x);case 5:return 1/x;
 case 6:return sqrt(x);case 7:return log(x);case 8:return sin(3*x);default:return x;}}
static double project(const BigDecimal *value){char *text=NULL;double number=NAN;
 if(bigdecimal_to_string(value,&text)==BIGDECIMAL_OK)number=strtod(text,NULL);
 free(text);return number;}
int main(int argc,char **argv)
{
 int repeats=argc>1?atoi(argv[1]):3;if(repeats<1||repeats>10||argc>2)return 2;
 int failed=0;
 puts("case,repeat,exact129_ms,compile_ms,fast129_ms,fast8192_ms,fast8192_calls,fast8192_bytes,real,non_real,uncertain,exact_error,libm_error");
 for(size_t c=0;c<sizeof(cases)/sizeof(cases[0]);++c)for(int run=0;run<repeats;++run){
  CalculatorContext context;calculator_context_init(&context);CalculatorError error;
  const Case *item=&cases[c];BigDecimal *number=bigdecimal_create();BigRational *low=bigrational_create(),*high=bigrational_create();
  ApplicationGraph *graph=NULL;ApplicationExpression *prepared=NULL;ApplicationFastExpression *fast=NULL;
  if(!number||!low||!high)return 2;
  if(bigdecimal_set_string(number,item->low)!=BIGDECIMAL_OK||bigrational_from_bigdecimal(low,number)!=BIGRATIONAL_OK||
   bigdecimal_set_string(number,item->high)!=BIGDECIMAL_OK||bigrational_from_bigdecimal(high,number)!=BIGRATIONAL_OK)return 2;
  double started=benchmark_seconds();CalculatorStatus status=application_graph_sample(&graph,item->expression,"x",low,high,129U,NULL,&context,&error);
  double exact_ms=(benchmark_seconds()-started)*1000;
  if(status!=CALCULATOR_OK)return 2;
  started=benchmark_seconds();status=application_expression_prepare_auto(&prepared,item->expression,NULL,&context,&error);
  if(status==CALCULATOR_OK)status=application_expression_fast_compile(&fast,prepared,&error);
  double compile_ms=(benchmark_seconds()-started)*1000;if(status!=CALCULATOR_OK)return 2;
  double x[129];ApplicationFastSample sampled[129];
  for(size_t i=0;i<129U;++i)x[i]=project(graph->coordinates[i].number);
  started=benchmark_seconds();status=application_fast_expression_sample(fast,x,129U,sampled);
  double fast_ms=(benchmark_seconds()-started)*1000;if(status!=CALCULATOR_OK)return 2;
  double exact_error=0;
  for(size_t i=0;i<129U;++i)if(sampled[i].kind==APPLICATION_FAST_REAL){
   if(application_graph_point_kind(&graph->samples[i])!=APPLICATION_GRAPH_REAL){failed=1;continue;}
   double expected=project(graph->samples[i].value.number),gap=fabs(sampled[i].value-expected)/fmax(1,fabs(expected));
   if(!isfinite(gap)||gap>1e-9)failed=1;
   if(gap>exact_error)exact_error=gap;
  }
  double *dense=numforge_malloc(8192U*sizeof(*dense));ApplicationFastSample *result=numforge_malloc(8192U*sizeof(*result));
  if(!dense||!result)return 2;
  double a=strtod(item->low,NULL),b=strtod(item->high,NULL);
  for(size_t i=0;i<8192U;++i)dense[i]=a+(b-a)*(double)i/8191;
  numforge_alloc_stats_reset();started=benchmark_seconds();status=application_fast_expression_sample(fast,dense,8192U,result);
  double dense_ms=(benchmark_seconds()-started)*1000;size_t calls=numforge_alloc_stats_calls(),bytes=numforge_alloc_stats_bytes();
  if(status!=CALCULATOR_OK)return 2;
  size_t real=0,non_real=0,uncertain=0;double libm_error=0;
  for(size_t i=0;i<8192U;++i){
   if(result[i].kind==APPLICATION_FAST_REAL){++real;double expected=reference(item->reference,dense[i]),gap=fabs(expected-result[i].value)/fmax(1,fabs(expected));
    if(!isfinite(gap)||gap>1e-9)failed=1;
    if(gap>libm_error)libm_error=gap;
   }else if(result[i].kind==APPLICATION_FAST_NON_REAL)++non_real;else ++uncertain;
  }
  printf("%s,%d,%.6f,%.6f,%.6f,%.6f,%zu,%zu,%zu,%zu,%zu,%.6g,%.6g\n",item->name,run,exact_ms,compile_ms,fast_ms,dense_ms,calls,bytes,real,non_real,uncertain,exact_error,libm_error);fflush(stdout);
  free(dense);free(result);application_graph_destroy(graph);application_expression_destroy(prepared);application_fast_expression_destroy(fast);
  bigdecimal_destroy(number);bigrational_destroy(low);bigrational_destroy(high);
 }
 return failed;
}
