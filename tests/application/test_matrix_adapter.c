#include "src/application/matrices/matrix.h"
#include "apps/web/http/matrix_web.h"
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}
static CalculatorContext context;
static CalculatorStatus compute(ApplicationMatrixResult **out,const char *op,const char *a,const char *b)
{
    calculator_context_init(&context);
    return application_matrix_compute(out,op,a,b,&context,NULL,NULL,NULL);
}
static void test_exact_matrix_operations(void)
{
    ApplicationMatrixResult *out=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,"determinant","1/2 0.25\n-2 3",NULL));
    TEST_ASSERT_EQUAL_STRING("2",out->scalar);application_matrix_result_destroy(out);out=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,"rank","1 2 3\n2 4 6",NULL));
    TEST_ASSERT_EQUAL_STRING("1",out->scalar);application_matrix_result_destroy(out);out=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,"solve","2 0\n0 4","1 2\n2 4"));
    char *cell=NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_to_string(numforge_matrix_entry(out->matrix,0,0),&cell));
    TEST_ASSERT_EQUAL_STRING("1/2",cell);free(cell);application_matrix_result_destroy(out);
    out=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,"multiply","1 2\n3 4","5\n6"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_to_string(numforge_matrix_entry(out->matrix,1,0),&cell));
    TEST_ASSERT_EQUAL_STRING("39",cell);free(cell);application_matrix_result_destroy(out);
}
static void test_domains_positions_and_atomic_failures(void)
{
    ApplicationMatrixResult *sentinel=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&sentinel,"rank","0",NULL));
    ApplicationMatrixResult *out=sentinel;
    const char *invalid[]={"","1 2\n3","1e1000000","sqrt(2)","x=2","1/0","1//2","1,2","--1","1 2 3 4 5 6 7 8 9"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,compute(&out,"rank",invalid[i],NULL));
        TEST_ASSERT_EQUAL_PTR(sentinel,out);
    }
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,compute(&out,"determinant","1 2",NULL));
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,compute(&out,"solve","1 2\n2 4","1\n2"));
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,"determinant","1 2\n2 4",NULL));
    TEST_ASSERT_EQUAL_STRING("0",out->scalar);application_matrix_result_destroy(out);out=sentinel;
    size_t matrix=SIZE_MAX,cell=SIZE_MAX;CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,application_matrix_compute(&out,"multiply","1 2","1\n1/0",&context,&error,&matrix,&cell));
    TEST_ASSERT_EQUAL(1,matrix);TEST_ASSERT_EQUAL(1,cell);TEST_ASSERT_EQUAL_PTR(sentinel,out);
    application_matrix_result_destroy(sentinel);
}
static void test_allocation_and_outer_budget_failures(void)
{
    ApplicationMatrixResult *sentinel=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&sentinel,"rank","1",NULL));
    ApplicationMatrixResult *out=NULL;
    const char *operations[]={"solve","solve_general"};
    for(size_t operation=0U;operation<2U;++operation) {
        numforge_test_allocator_begin(0U);out=NULL;
        TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,operations[operation],"2 1\n1 3","1\n2"));
        size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();application_matrix_result_destroy(out);
        for(size_t i=1U;i<=calls;++i) {
            out=sentinel;numforge_test_allocator_begin(i);
            CalculatorStatus status=compute(&out,operations[operation],"2 1\n1 3","1\n2");
            numforge_test_allocator_end();
            TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,status);TEST_ASSERT_EQUAL_PTR(sentinel,out);
        }
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(0U,SIZE_MAX,SIZE_MAX));out=sentinel;
    TEST_ASSERT_EQUAL(CALCULATOR_TIME_LIMIT,compute(&out,"rank","1",NULL));
    numforge_budget_end();TEST_ASSERT_EQUAL_PTR(sentinel,out);
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX,8U,8U));
    TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,compute(&out,"rank","1",NULL));
    numforge_budget_end();TEST_ASSERT_EQUAL_PTR(sentinel,out);application_matrix_result_destroy(sentinel);
}
static void test_general_families_and_http_failures(void)
{
    ApplicationMatrixResult *out=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,"solve_general","1 2 3\n2 4 6","1\n2"));
    TEST_ASSERT_EQUAL(NUMFORGE_LINEAR_INFINITE,numforge_linear_solution_kind(out->solution));
    TEST_ASSERT_EQUAL(2,numforge_linear_solution_nullity(out->solution));
    TEST_ASSERT_EQUAL(1,numforge_linear_solution_free_column(out->solution,0));
    char *value=NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_to_string(numforge_matrix_entry(numforge_linear_solution_nullspace(out->solution),0,1),&value));
    TEST_ASSERT_EQUAL_STRING("-3",value);free(value);application_matrix_result_destroy(out);out=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,compute(&out,"solve_general","1 2\n2 4","1\n3"));
    TEST_ASSERT_EQUAL(NUMFORGE_LINEAR_NONE,numforge_linear_solution_kind(out->solution));
    application_matrix_result_destroy(out);out=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,compute(&out,"solve_general","1","1 2"));
    const char *body="1 2\n\n3";char *json=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_matrix_request("/api/matrix?operation=solve_general",body,strlen(body),&json));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true,\"exact\":true,\"solution\":{\"kind\":\"infinite\",\"rank\":1,\"nullity\":1,\"particular\":[[\"3\"],[\"0\"]],\"basis\":[[\"-2\"],[\"1\"]],\"free_columns\":[1]}}",json);free(json);
    numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_matrix_request("/api/matrix?operation=solve_general",body,strlen(body),&json));
    size_t calls=numforge_test_allocator_call_count();numforge_test_allocator_end();free(json);
    for(size_t i=1U;i<=calls;++i) {
        json=NULL;numforge_test_allocator_begin(i);
        CalculatorStatus status=numforge_web_matrix_request("/api/matrix?operation=solve_general",body,strlen(body),&json);
        numforge_test_allocator_end();TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,status);
        if(json) {TEST_ASSERT_NOT_NULL(strstr(json,"\"ok\":false"));TEST_ASSERT_EQUAL_CHAR('}',json[strlen(json)-1U]);}
        free(json);
    }
}
static void test_http_schema_and_bounded_body(void)
{
    const char *body="1 2\r\n3 4\r\n\r\n5\r\n6";char *json=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_matrix_request("/api/matrix?operation=multiply",body,strlen(body),&json));
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true,\"exact\":true,\"matrix\":[[\"17\"],[\"39\"]]}",json);free(json);
    TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,numforge_web_matrix_request("/api/matrix?operation=rank&operation=solve","1",1,&json));free(json);
    TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,numforge_web_matrix_request("/api/matrix?operation=rank","1\0x",3,&json));free(json);
    char large[6002];memset(large,'1',sizeof(large));
    TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,numforge_web_matrix_request("/api/matrix?operation=rank",large,sizeof(large),&json));free(json);
}
int main(void)
{
    UNITY_BEGIN();RUN_TEST(test_exact_matrix_operations);RUN_TEST(test_domains_positions_and_atomic_failures);
    RUN_TEST(test_allocation_and_outer_budget_failures);RUN_TEST(test_http_schema_and_bounded_body);
    RUN_TEST(test_general_families_and_http_failures);return UNITY_END();
}
