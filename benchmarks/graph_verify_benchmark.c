#include "graph_analysis.h"
#include "benchmark_clock.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Same inputs and numerical limits with reuse disabled/enabled. Shared-endpoint
 * and duplicate cases deliberately measure cache opportunity; distinct cases
 * expose lookup overhead. These are not a model of every browser request. */
int main(int argc,char **argv)
{
    int repeats=argc>1?atoi(argv[1]):3;
    if(argc>2||repeats<1||repeats>10)return 2;
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
