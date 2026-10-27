#include "src/application/matrices/matrix.h"
#include "src/engine/api/status.h"
#include "src/runtime/numforge_alloc.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static CalculatorStatus matrix_status(NumForgeMatrixStatus status)
{
    switch(status) {
    case NUMFORGE_MATRIX_OK: return CALCULATOR_OK;
    case NUMFORGE_MATRIX_OUT_OF_MEMORY: return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    case NUMFORGE_MATRIX_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
    case NUMFORGE_MATRIX_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
    case NUMFORGE_MATRIX_SINGULAR: return CALCULATOR_DIVISION_BY_ZERO;
    default: return CALCULATOR_INVALID_ARGUMENT;
    }
}
static bool digits(const char *text)
{
    if(*text=='+' || *text=='-') ++text;
    if(!*text) return false;
    for(;*text;++text) if(*text<'0' || *text>'9') return false;
    return true;
}
static CalculatorStatus cell_value(BigRational *value, char *text)
{
    char *slash=strchr(text,'/');
    if(slash) {
        *slash='\0';
        if(!digits(text) || !digits(slash+1)) return CALCULATOR_INVALID_ARGUMENT;
        BigInt *n=bigint_create(), *d=bigint_create();
        CalculatorStatus status=n && d ? calculator_from_integer_status(bigint_set_string(n,text)) : CALCULATOR_OUT_OF_MEMORY;
        if(status==CALCULATOR_OK) status=calculator_from_integer_status(bigint_set_string(d,slash+1));
        if(status==CALCULATOR_OK) status=calculator_from_rational_status(bigrational_set_fraction(value,n,d));
        bigint_destroy(n);bigint_destroy(d);return status;
    }
    /* Limit the grammar before decimal parsing, especially unbounded exponents. */
    const char *p=text;
    if(*p=='+' || *p=='-') ++p;
    size_t count=0U, points=0U;
    for(;*p;++p) {
        if(*p=='.') ++points;
        else if(*p>='0' && *p<='9') ++count;
        else return CALCULATOR_INVALID_ARGUMENT;
    }
    if(!count || points>1U) return CALCULATOR_INVALID_ARGUMENT;
    BigDecimal *decimal=bigdecimal_create();
    CalculatorStatus status=decimal ? calculator_from_decimal_status(bigdecimal_set_string(decimal,text)) : CALCULATOR_OUT_OF_MEMORY;
    if(status==CALCULATOR_OK) status=calculator_from_rational_status(bigrational_from_bigdecimal(value,decimal));
    bigdecimal_destroy(decimal);return status;
}
static CalculatorStatus parse(NumForgeMatrix **result, const char *text,
    CalculatorError *error, size_t *failed_cell)
{
    BigRational *entries[64]={0};
    size_t used=0U, rows=0U, columns=0U, current=0U;
    const char *p=text;
    CalculatorStatus status=CALCULATOR_OK;
    size_t length=0U;
    while(length<=6000U && text[length]) ++length;
    if(length>6000U) return CALCULATOR_VALUE_TOO_LARGE;
    while(*p && status==CALCULATOR_OK) {
        if(!numforge_budget_check()) {status=calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);break;}
        if(*p==' ' || *p=='\t' || *p=='\r') {++p;continue;}
        if(*p=='\n') {
            if(!current || (columns && current!=columns) || rows>=APPLICATION_MATRIX_MAX_DIMENSION) {status=CALCULATOR_INVALID_ARGUMENT;break;}
            columns=current;current=0U;++rows;++p;continue;
        }
        const char *start=p;
        while(*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\n') ++p;
        size_t cell_length=(size_t)(p-start);
        if(used==64U || current==APPLICATION_MATRIX_MAX_DIMENSION || rows==APPLICATION_MATRIX_MAX_DIMENSION || cell_length>APPLICATION_MATRIX_MAX_CELL) {
            status=CALCULATOR_VALUE_TOO_LARGE;break;
        }
        char cell[APPLICATION_MATRIX_MAX_CELL+1U];memcpy(cell,start,cell_length);cell[cell_length]='\0';
        entries[used]=bigrational_create();
        status=entries[used] ? cell_value(entries[used],cell) : CALCULATOR_OUT_OF_MEMORY;
        if(status!=CALCULATOR_OK) {calculator_error_set(error,status,(size_t)(start-text));break;}
        ++used;++current;
    }
    if(status==CALCULATOR_OK && current) {
        if(columns && current!=columns) status=CALCULATOR_INVALID_ARGUMENT;
        else {columns=current;++rows;}
    }
    if(status==CALCULATOR_OK && (!rows || !columns)) status=CALCULATOR_INVALID_ARGUMENT;
    if(status==CALCULATOR_OK) status=matrix_status(numforge_matrix_create(result,rows,columns,(const BigRational *const *)entries));
    if(status!=CALCULATOR_OK) {
        if(failed_cell) *failed_cell=used;
        if(error && error->status==CALCULATOR_OK) calculator_error_set(error,status,(size_t)(p-text));
    }
    for(size_t i=0U;i<64U;++i) bigrational_destroy(entries[i]);
    return status;
}
void application_matrix_result_destroy(ApplicationMatrixResult *result)
{
    if(result) {numforge_matrix_destroy(result->matrix);numforge_linear_solution_destroy(result->solution);free(result->scalar);free(result);}
}
CalculatorStatus application_matrix_compute(ApplicationMatrixResult **result,
    const char *operation, const char *a, const char *b, const CalculatorContext *context,
    CalculatorError *error, size_t *failed_matrix, size_t *failed_cell)
{
    calculator_error_clear(error);
    if(failed_matrix) *failed_matrix=SIZE_MAX;
    if(failed_cell) *failed_cell=SIZE_MAX;
    if(!result || !operation || !a || !context) return CALCULATOR_NULL_ARGUMENT;
    bool general=strcmp(operation,"solve_general")==0;
    bool binary=general || strcmp(operation,"multiply")==0 || strcmp(operation,"solve")==0;
    if((!binary && strcmp(operation,"determinant")!=0 && strcmp(operation,"rank")!=0) || (binary && !b)) return CALCULATOR_INVALID_ARGUMENT;
    CalculatorStatus status=calculator_context_validate(context,error);
    if(status!=CALCULATOR_OK) return status;
    bool owner=numforge_budget_begin((uint64_t)context->time_limit_ms,CALCULATOR_ALLOCATION_BUDGET,CALCULATOR_SINGLE_ALLOCATION);
    NumForgeMatrix *matrices[2]={0};
    ApplicationMatrixResult *out=NULL;
    for(size_t i=0U;i<(binary ? 2U : 1U) && status==CALCULATOR_OK;++i) {
        status=parse(&matrices[i],i ? b : a,error,failed_cell);
        if(status!=CALCULATOR_OK && failed_matrix) *failed_matrix=i;
    }
    if(status==CALCULATOR_OK) {out=calloc(1U,sizeof(*out));if(!out) status=CALCULATOR_OUT_OF_MEMORY;}
    if(status==CALCULATOR_OK && general)
        status=matrix_status(numforge_matrix_solve_general(&out->solution,matrices[0],matrices[1]));
    else if(status==CALCULATOR_OK && binary) status=matrix_status(strcmp(operation,"multiply")==0 ?
        numforge_matrix_multiply(&out->matrix,matrices[0],matrices[1]) : numforge_matrix_solve(&out->matrix,matrices[0],matrices[1]));
    else if(status==CALCULATOR_OK && strcmp(operation,"rank")==0) {
        size_t rank=0U;status=matrix_status(numforge_matrix_rank(&rank,matrices[0]));
        if(status==CALCULATOR_OK) {
            out->scalar=malloc(24U);
            if(out->scalar) (void)snprintf(out->scalar,24U,"%zu",rank);else status=CALCULATOR_OUT_OF_MEMORY;
        }
    } else if(status==CALCULATOR_OK) {
        BigRational *det=bigrational_create();
        status=det ? matrix_status(numforge_matrix_determinant(det,matrices[0])) : CALCULATOR_OUT_OF_MEMORY;
        if(status==CALCULATOR_OK) status=calculator_from_rational_status(bigrational_to_string(det,&out->scalar));
        bigrational_destroy(det);
    }
    status=calculator_budget_status(status);
    if(status==CALCULATOR_OK) {*result=out;out=NULL;}
    else if(error && error->status==CALCULATOR_OK) calculator_error_set(error,status,0U);
    application_matrix_result_destroy(out);
    numforge_matrix_destroy(matrices[0]);numforge_matrix_destroy(matrices[1]);
    if(owner) numforge_budget_end();
    return status;
}
