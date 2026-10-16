#include "graph_analysis.h"
#include "benchmark_clock.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv)
{
    int repeats=argc>1?atoi(argv[1]):3;if(argc>2||repeats<1||repeats>10)return 2;
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
