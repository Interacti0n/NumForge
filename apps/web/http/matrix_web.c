#include "apps/web/http/matrix_web.h"
#include "src/application/matrices/matrix.h"
#include "src/engine/api/status.h"
#include "src/runtime/numforge_alloc.h"
#include <numforge/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATRIX_JSON_CAPACITY (128U*1024U)
static bool append(char *json, size_t *used, const char *text)
{
    size_t length=strlen(text);
    if(length>=MATRIX_JSON_CAPACITY-*used) return false;
    memcpy(json+*used,text,length+1U);*used+=length;return true;
}
CalculatorStatus numforge_web_matrix_request(const char *target, const char *body,
    size_t length, char **response)
{
    if(!target || !body || !response) return CALCULATOR_NULL_ARGUMENT;
    static const char prefix[]="/api/matrix?operation=";
    *response=NULL;
    CalculatorContext context;calculator_context_init(&context);
    bool owner=numforge_budget_begin((uint64_t)context.time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    CalculatorStatus status=CALCULATOR_INVALID_ARGUMENT;
    ApplicationMatrixResult *result=NULL;
    CalculatorError error;calculator_error_clear(&error);
    size_t failed_matrix=SIZE_MAX, failed_cell=SIZE_MAX;
    char input[6001];size_t used=0U;
    const char *operation=NULL;
    if(length<=6000U && !memchr(body,'\0',length) && strncmp(target,prefix,sizeof(prefix)-1U)==0) {
        operation=target+sizeof(prefix)-1U;
        for(size_t i=0U;i<length;++i)
            if(body[i]!='\r' || i+1U==length || body[i+1U]!='\n') input[used++]=body[i];
        input[used]='\0';
        char *b=strstr(input,"\n\n");
        if(b) {*b='\0';b+=2;}
        status=application_matrix_compute(&result,operation,input,b,&context,&error,&failed_matrix,&failed_cell);
    }
    char *json=NULL;
    if(status==CALCULATOR_OK) {
        json=malloc(MATRIX_JSON_CAPACITY);
        if(!json) status=CALCULATOR_OUT_OF_MEMORY;
        else {
            used=0U;json[0]='\0';
            bool valid=append(json,&used,"{\"ok\":true,\"exact\":true,");
            if(result->scalar) valid=valid && append(json,&used,"\"scalar\":\"") && append(json,&used,result->scalar) && append(json,&used,"\"");
            else {
                valid=valid && append(json,&used,"\"matrix\":[");
                for(size_t row=0U;row<numforge_matrix_rows(result->matrix) && valid;++row) {
                    valid=append(json,&used,row ? ",[" : "[");
                    for(size_t column=0U;column<numforge_matrix_columns(result->matrix) && valid;++column) {
                        char *value=NULL;
                        status=calculator_from_rational_status(bigrational_to_string(numforge_matrix_entry(result->matrix,row,column),&value));
                        valid=status==CALCULATOR_OK && append(json,&used,column ? ",\"" : "\"") && append(json,&used,value) && append(json,&used,"\"");
                        free(value);
                    }
                    valid=valid && append(json,&used,"]");
                }
                valid=valid && append(json,&used,"]");
            }
            valid=valid && append(json,&used,"}");
            if(!valid && status==CALCULATOR_OK) status=CALCULATOR_VALUE_TOO_LARGE;
        }
    }
    status=calculator_budget_status(status);
    if(status!=CALCULATOR_OK) {
        free(json);json=malloc(512U);
        if(json) (void)snprintf(json,512U,
            "{\"ok\":false,\"status\":\"%s\",\"error\":\"%s\",\"matrix\":%d,\"cell\":%d}",
            calculator_status_to_string(status),calculator_status_to_string(status),
            failed_matrix==SIZE_MAX ? -1 : (int)failed_matrix,failed_cell==SIZE_MAX ? -1 : (int)failed_cell);
    }
    application_matrix_result_destroy(result);
    *response=json;if(owner) numforge_budget_end();return status;
}
