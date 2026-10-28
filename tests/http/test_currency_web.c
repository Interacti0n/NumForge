#include "apps/web/http/currency_web.h"
#include "src/runtime/numforge_alloc.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {numforge_test_allocator_end();numforge_budget_end();}

static void test_supplied_rates_round_once_without_losing_large_amounts(void)
{
    const char *cases[][3]={
        {"2","100\n1\n1.2","120"},
        {"2","9007199254740993\n1\n1.2","10808639105689191.6"},
        {"2","1.005\n1\n1","1"}, {"2","1.015\n1\n1","1.02"},
        {"2","-1.005\n1\n1","-1"}, {"2","100.5\n24\n1.2","5.02"},
        {"8","1\n3\n1","0.33333333"}, {"0","1.234\n1\n160","197"},
        {"4","-.0001\n1\n160","-0.016"}, {"2","+0\n1\n1","0"}
    };
    for(size_t i=0U;i<sizeof(cases)/sizeof(cases[0]);++i){
        char target[64],expected[256],*response=NULL;
        (void)snprintf(target,sizeof(target),"/api/currency/convert?places=%s",cases[i][0]);
        (void)snprintf(expected,sizeof(expected),"{\"ok\":true,\"approximate\":true,\"result\":\"%s\"}",cases[i][2]);
        TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_currency_request(target,cases[i][1],strlen(cases[i][1]),&response));
        TEST_ASSERT_EQUAL_STRING(expected,response);free(response);
    }
}
static void test_invalid_requests_have_no_result(void)
{
    const char *bodies[]={"","1\n1","1\n1\n1\n1","1\n0\n1","1\n1\n0","1\n-1\n1","1E3\n1\n1","1+2\n1\n1","1,2\n1\n1","1\n1\nNaN","1\n1\n0.0000000000001","1\n1\n1234567890123","1\r\n1\r\n1"};
    for(size_t i=0U;i<sizeof(bodies)/sizeof(bodies[0]);++i){
        char *response=NULL;TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,numforge_web_currency_request("/api/currency/convert?places=2",bodies[i],strlen(bodies[i]),&response));
        TEST_ASSERT_NOT_NULL(response);TEST_ASSERT_NULL(strstr(response,"\"result\""));free(response);
    }
    const char *targets[]={"/api/currency/convert","/api/currency/convert?places=9","/api/currency/convert?places=-1","/api/currency/convert?places=02","/api/currency/convert?places=2&places=4","/api/currency/convert?places=2&client=x"};
    for(size_t i=0U;i<sizeof(targets)/sizeof(targets[0]);++i){char *response=NULL;TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,numforge_web_currency_request(targets[i],"1\n1\n1",5U,&response));free(response);}
    char embedded[]="1\n1\n1\0x",*response=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,numforge_web_currency_request("/api/currency/convert?places=2",embedded,sizeof(embedded)-1U,&response));free(response);
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT,numforge_web_currency_request(NULL,NULL,0U,NULL));
}
static void test_maximum_body_retains_all_amount_digits(void)
{
    char body[309],amount[257],*response=NULL;
    char expected[sizeof(amount)+sizeof("\"result\":\"\"")];
    memset(amount,'1',256U);amount[200]='.';amount[256]='\0';
    (void)snprintf(body,sizeof(body),"%s\n999999999999.123456789012\n999999999999.123456789012",amount);
    TEST_ASSERT_EQUAL_UINT(308U,strlen(body));
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_currency_request("/api/currency/convert?places=8",body,308U,&response));
    amount[209]='\0';(void)snprintf(expected,sizeof(expected),"\"result\":\"%s\"",amount);
    TEST_ASSERT_NOT_NULL(strstr(response,expected));free(response);
}
static void test_allocation_failures_cannot_publish_success(void)
{
    char *initial=NULL;numforge_test_allocator_begin(0U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_currency_request("/api/currency/convert?places=2","100\n1\n1.2",9U,&initial));
    size_t count=numforge_test_allocator_call_count();numforge_test_allocator_end();free(initial);
    for(size_t i=1U;i<=count;++i){
        char *response=NULL;numforge_test_allocator_begin(i);
        CalculatorStatus status=numforge_web_currency_request("/api/currency/convert?places=2","100\n1\n1.2",9U,&response);
        numforge_test_allocator_end();
        if(status!=CALCULATOR_OK && response) TEST_ASSERT_NULL(strstr(response,"\"ok\":true"));
        free(response);
    }
}
int main(void)
{
    UNITY_BEGIN();RUN_TEST(test_supplied_rates_round_once_without_losing_large_amounts);
    RUN_TEST(test_invalid_requests_have_no_result);RUN_TEST(test_maximum_body_retains_all_amount_digits);
    RUN_TEST(test_allocation_failures_cannot_publish_success);
    return UNITY_END();
}
