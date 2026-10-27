#include "apps/web/http/currency_web.h"
#include "src/engine/api/status.h"
#include "src/runtime/numforge_alloc.h"
#include <numforge/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool decimal(const char *value, bool amount)
{
    size_t length=strlen(value),whole=0U,fraction=0U;
    bool dot=false,nonzero=false;
    if(length==0U || length>(amount ? 256U : 25U)) return false;
    if(amount && (*value=='+' || *value=='-')) ++value;
    for(;*value;++value) {
        if(*value=='.' && !dot) {dot=true;continue;}
        if(*value<'0' || *value>'9') return false;
        if(dot) ++fraction;else ++whole;
        nonzero=nonzero || *value!='0';
    }
    return whole+fraction>0U && whole<=(amount ? 200U : 12U) &&
        fraction<=(amount ? 100U : 12U) && (amount || nonzero);
}

CalculatorStatus numforge_web_currency_request(const char *target,
    const char *body, size_t length, char **response)
{
    if(!response) return CALCULATOR_NULL_ARGUMENT;
    *response=NULL;
    static const char prefix[]="/api/currency/convert?places=";
    CalculatorStatus status=CALCULATOR_INVALID_ARGUMENT;
    BigDecimal *amount=NULL,*from=NULL,*to=NULL,*product=NULL,*result=NULL;
    char input[309],*number=NULL;
    bool owner=numforge_budget_begin(CALCULATOR_DEFAULT_TIME_LIMIT_MS,
        CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    if(!target || !body || length>=sizeof(input) || memchr(body,'\0',length) ||
        strncmp(target,prefix,sizeof(prefix)-1U)!=0) goto done;
    const char *places=target+sizeof(prefix)-1U;
    if(places[0]<'0' || places[0]>'8' || places[1]!='\0') goto done;
    memcpy(input,body,length);input[length]='\0';
    char *source=strchr(input,'\n');if(!source) goto done;*source++='\0';
    char *destination=strchr(source,'\n');if(!destination) goto done;*destination++='\0';
    if(!decimal(input,true) || !decimal(source,false) || !decimal(destination,false)) goto done;
    amount=bigdecimal_create();from=bigdecimal_create();to=bigdecimal_create();
    product=bigdecimal_create();result=bigdecimal_create();
    if(!amount || !from || !to || !product || !result) {status=CALCULATOR_OUT_OF_MEMORY;goto done;}
    BigDecimalStatus operation=bigdecimal_set_string(amount,input);
    if(operation==BIGDECIMAL_OK) operation=bigdecimal_set_string(from,source);
    if(operation==BIGDECIMAL_OK) operation=bigdecimal_set_string(to,destination);
    if(operation==BIGDECIMAL_OK) operation=bigdecimal_mul(product,amount,to);
    if(operation==BIGDECIMAL_OK) operation=bigdecimal_div(result,product,from,places[0]-'0',BIGDECIMAL_ROUND_HALF_EVEN);
    if(operation==BIGDECIMAL_OK) operation=bigdecimal_to_string(result,&number);
    status=calculator_from_bigdecimal_status(operation);
done:
    if(status==CALCULATOR_OK) {
        size_t capacity=strlen(number)+64U;
        *response=malloc(capacity);
        if(*response) (void)snprintf(*response,capacity,"{\"ok\":true,\"approximate\":true,\"result\":\"%s\"}",number);
        else status=CALCULATOR_OUT_OF_MEMORY;
    }
    if(!*response) {
        *response=malloc(128U);
        if(*response) (void)snprintf(*response,128U,"{\"ok\":false,\"status\":\"%s\"}",calculator_status_to_string(status));
        else status=CALCULATOR_OUT_OF_MEMORY;
    }
    free(number);bigdecimal_destroy(amount);bigdecimal_destroy(from);bigdecimal_destroy(to);
    bigdecimal_destroy(product);bigdecimal_destroy(result);
    if(owner) numforge_budget_end();
    return status;
}
