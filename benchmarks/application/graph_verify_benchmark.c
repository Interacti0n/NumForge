#include "src/application/graphs/graph_analysis.h"
#include "benchmarks/common/benchmark_clock.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_cache(int repeats)
{
    puts("case,repeat,reuse,ms,evaluations,sample_requests,cache_hits,confirmed,limited");
    const char *names[]={"duplicate_sine","distinct_sine","shared_bracket","pair_sine","pole","rational_cancellation"};
    const char *first[]={"sin(x)","sin(33*x)","sin(x)","sin(33*x)","1/x","1000000000000000000000000000000+x"};
    const char *second[]={NULL,NULL,NULL,"cos(31*x)",NULL,"1000000000000000000000000000001"};
    for(size_t scene=0;scene<6U;++scene)for(int run=1;run<=repeats;++run){
        ApplicationGraphCandidate candidates[32];
        for(size_t i=0;i<32U;++i){
            double x=3.1415,low=3.1,high=3.2;
            if(scene==1U){x=((double)i-16)*3.141592653589793/33+.00001;low=x-.001;high=x+.001;}
            if(scene==2U)x=3.141+((double)i/32)*.001;
            if(scene==3U){x=3.141592653589793/128+((double)i-16)*3.141592653589793/32+.000001;low=x-.001;high=x+.001;}
            if(scene==4U){x=.01;low=-1;high=1;}
            if(scene==5U){x=1;low=.5;high=1.5;}
            candidates[i]=(ApplicationGraphCandidate){scene==3U||scene==5U?APPLICATION_GRAPH_INTERSECTION:APPLICATION_GRAPH_ROOT,x,low,high,1e-6,1e-8};
        }
        ApplicationGraphVerifiedPoint baseline[32]={0};bool baseline_limited=false;
        for(int reuse=0;reuse<=1;++reuse){
            CalculatorContext context;calculator_context_init(&context);context.time_limit_ms=1000;
            CalculatorError error;ApplicationGraphVerifiedPoint output[32]={0};bool limited=false;ApplicationGraphVerificationStats stats={0};
            double start=benchmark_seconds();
            CalculatorStatus status=application_graph_verify_profile(output,&limited,first[scene],second[scene],candidates,32U,&context,&error,reuse!=0,&stats);
            double ms=(benchmark_seconds()-start)*1000;size_t confirmed=0;
            if(status!=CALCULATOR_OK||stats.evaluations>512U)return 1;
            for(size_t i=0;i<32U;++i){
                if(output[i].confirmed)++confirmed;
                if(scene==4U&&output[i].confirmed)return 1;
                if(scene==5U&&(!output[i].confirmed||output[i].x!=1||output[i].y!=1e30))return 1;
                if(reuse&&baseline[i].confirmed&&(!output[i].confirmed||fabs(output[i].x-baseline[i].x)>1e-10||output[i].residual>1e-8))return 1;
            }
            if(!reuse){for(size_t i=0;i<32U;++i)baseline[i]=output[i];baseline_limited=limited;}
            if(reuse&&!baseline_limited&&limited)return 1;
            printf("%s,%d,%d,%.6f,%zu,%zu,%zu,%zu,%d\n",names[scene],run,reuse,ms,stats.evaluations,stats.sample_requests,stats.cache_hits,confirmed,limited?1:0);
        }
    }
    return 0;
}

static int run_refinement(int repeats)
{
    const char *names[]={"distinct_sine","pair_sine","shifted_pole","step_jump","skew_cubic","logarithm","tangent","positive_minimum"};
    const char *first[]={"sin(33*x)","sin(33*x)","1/(x-0.1234)","floor(x)-0.5","x^3-2","ln(x)-1","(x-0.1234)^2","(x-0.1234)^2+0.000000000001"};
    puts("case,repeat,brent,ms,evaluations,steps,bisections,interpolations,confirmed,non_real,residual_rejections,iteration_limits,coordinate_limits,unprocessed,limited");
    for(size_t scene=0;scene<8U;++scene)for(int run=1;run<=repeats;++run){
        ApplicationGraphCandidate candidates[32];
        for(size_t i=0;i<32U;++i){
            double x=((double)i-16)*3.141592653589793/33+.00001,low=x-.001,high=x+.001;
            if(scene==1U){x=3.141592653589793/128+((double)i-16)*3.141592653589793/32+.000001;low=x-.001;high=x+.001;}
            if(scene==2U){x=.1235;low=.12;high=.13;}
            if(scene==3U){x=1;low=.9;high=1.1;}
            if(scene==4U){x=1.2;low=0;high=10;}
            if(scene==5U){x=2.7;low=.01;high=100;}
            if(scene>=6U){x=.1234;low=.12;high=.13;}
            candidates[i]=(ApplicationGraphCandidate){scene==1U?APPLICATION_GRAPH_INTERSECTION:APPLICATION_GRAPH_ROOT,x,low,high,1e-6,1e-8};
        }
        ApplicationGraphVerifiedPoint baseline[32]={0};
        for(int brent=0;brent<=1;++brent){
            CalculatorContext context;calculator_context_init(&context);context.time_limit_ms=1000;
            CalculatorError error;ApplicationGraphVerifiedPoint points[32]={0};ApplicationGraphVerificationStats stats={0};bool limited=false;
            double started=benchmark_seconds();
            CalculatorStatus status=application_graph_verify_experiment(points,&limited,first[scene],scene==1U?"cos(31*x)":NULL,candidates,32U,&context,&error,true,brent!=0,&stats);
            double ms=(benchmark_seconds()-started)*1000;
            if(status!=CALCULATOR_OK||stats.evaluations>512U)return 1;
            for(size_t i=0;i<32U;++i){
                if((scene==2U||scene==3U||scene==7U)&&points[i].confirmed)return 1;
                if(points[i].confirmed){
                    double x=points[i].x,expected=scene==0U?sin(33*x):scene==1U?sin(33*x)-cos(31*x):scene==4U?x*x*x-2:scene==5U?log(x)-1:(x-.1234)*(x-.1234);
                    if(!isfinite(expected)||fabs(expected)>1e-8)return 1;
                }
                if(brent&&baseline[i].confirmed&&(!points[i].confirmed||fabs(points[i].x-baseline[i].x)>1e-6))return 1;
                if(!brent)baseline[i]=points[i];
            }
            printf("%s,%d,%d,%.6f,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d\n",names[scene],run,brent,ms,stats.evaluations,stats.refinement_steps,stats.bisections,stats.interpolations,stats.confirmed,stats.non_real,stats.residual_rejections,stats.iteration_limits,stats.coordinate_limits,stats.unprocessed,limited?1:0);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "cache";
    char *end = NULL;
    long repeats = argc > 2 ? strtol(argv[2], &end, 10) : 3;
    if (argc > 3 || (strcmp(mode, "cache") && strcmp(mode, "refinement")) ||
        repeats < 1 || repeats > 10 || (argc > 2 && (end == argv[2] || *end))) {
        fputs("Usage: graph_verify_benchmark [cache|refinement] [repeats 1..10]\n", stderr);
        return 2;
    }
    return !strcmp(mode, "cache") ? run_cache((int)repeats) : run_refinement((int)repeats);
}
