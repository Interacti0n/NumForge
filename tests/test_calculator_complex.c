#include <unity.h>
#include "session.h"
#include "session_api.h"
#include "numforge_alloc.h"
#include "value_internal.h"
#include <stdlib.h>
#include <string.h>

static CalculatorContext context;
void setUp(void) { calculator_context_init(&context); }
void tearDown(void) { numforge_test_allocator_end();numforge_budget_end(); }
static void check(const char *expression,const char *expected,CalculatorValueKind kind)
{
    CalculatorValue value={0};CalculatorError error;char *text=NULL;
    TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK,calculator_compute_value(expression,&context,&value,&error),expression);
    TEST_ASSERT_EQUAL(kind,value.kind);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_format_value(&value,&context,&text));
    TEST_ASSERT_EQUAL_STRING(expected,text);free(text);calculator_value_destroy(&value);
}
static void test_exact_and_mixed_arithmetic(void)
{
    check("complex(1/3;1/3)","1/3 + (1/3)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("complex(0;1)^2","-1",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("pow(complex(1;1);-2)","-(1/2)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("complex(1;1)^-1","1/2 - (1/2)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("2+complex(1/3;-2/3)","7/3 - (2/3)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("complex(2;3)*complex(4;-5)","23 + 2*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("complex(1;1)/complex(1;-1)","i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("-complex(2;3)","-2 - 3*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("complex(1;0)+sqrt(2)","2.4142135624",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    context.significant_division=false;
    check("complex(1;2)+3","4 + 2*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
}
static void test_domains(void)
{
    const char *inputs[]={"complex(1;2)^0.5","complex(1;2)^complex(1;0)","sin(complex(1;2))",
        "complex(complex(1;2);3)","complex(1;2)!","sqrt(complex(1;2))"};
    for(size_t i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++) {
        CalculatorValue value={0};CalculatorError error;
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_compute_value(inputs[i],&context,&value,&error));
        TEST_ASSERT_NULL(value.number);calculator_value_destroy(&value);
    }
    CalculatorValue value={0};CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,calculator_compute_value("complex(1;2)/0",&context,&value,&error));
    TEST_ASSERT_EQUAL(CALCULATOR_DIMENSION_ERROR,calculator_compute_value("qty(complex(1;2);\"m\")",&context,&value,&error));
    TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_compute_value("complex(1;2)",&context,&value,&error));
    BigDecimal *scalar=bigdecimal_create();TEST_ASSERT_NOT_NULL(scalar);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_materialize_exact(scalar,&value,&context));
    CalculatorValue real={0};
    TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_compute_value_with_answer("2+2",&context,&value,NULL,&real,&error));
    calculator_value_destroy(&real);
    bigdecimal_destroy(scalar);calculator_value_destroy(&value);
}
static void compute(CalculatorSession *session,uint64_t revision,bool commit,const char *input,const char *expected)
{
    char *text=NULL;CalculatorError error;
    TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_OK,calculator_session_compute(session,revision,commit,input,&context,&text,&error,NULL),input);
    TEST_ASSERT_EQUAL_STRING(expected,text);free(text);
}
static void test_sessions_and_snapshots(void)
{
    ApplicationClientStore *store=calloc(1,sizeof(*store));TEST_ASSERT_NOT_NULL(store);
    CalculatorSession *session=application_client_session(store,"cccccccccccccccccccccccccccccccc",true);TEST_ASSERT_NOT_NULL(session);
    compute(session,1,true,"I = 7","7");
    compute(session,2,true,"z = complex(1/3;1/3)","1/3 + (1/3)*i");
    compute(session,3,false,"z^2","(2/9)*i");
    compute(session,4,true,"z^2","(2/9)*i");
    compute(session,5,true,"ans*9","2*i");
    compute(session,6,true,"I+z","22/3 + (1/3)*i");
    char *json=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,numforge_web_session_request(store,"GET","/api/session/variables?client=cccccccccccccccccccccccccccccccc&full=1","",&json));
    TEST_ASSERT_NOT_NULL(strstr(json,"\"schema_version\":2"));
    TEST_ASSERT_NOT_NULL(strstr(json,"\"kind\":\"complex_rational\""));
    TEST_ASSERT_NOT_NULL(strstr(json,"\"components\":{\"real\":\"1/3\",\"imaginary\":\"1/3\"}"));free(json);
    TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_session_mutate(session,7,CALCULATOR_SESSION_CLEAR_HISTORY));
    compute(session,8,true,"ans-z","7");
    ApplicationConversion converted={0};CalculatorError error;const char *code=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,application_conversion_compute(session,"z","m","cm",&context,&converted,&error,&code));
    TEST_ASSERT_EQUAL_STRING("complex_not_allowed",code);
    compute(session,9,true,"qty(2;\"m\")","2 m");
    application_client_store_destroy(store);free(store);
}
static void test_imaginary_unit_and_projections(void)
{
    check("i^2","-1",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("2+3i","2 + 3*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("(1+i)(1-i)","2",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("re(1/3+2i)","1/3",CALCULATOR_VALUE_RATIONAL);
    check("im(1+2/3*i)","2/3",CALCULATOR_VALUE_RATIONAL);
    check("conj(1/3+2/3*i)","1/3 - (2/3)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("abs(3/5+4/5*i)","1",CALCULATOR_VALUE_RATIONAL);
    check("abs(1+i)","1.4142135624",CALCULATOR_VALUE_DECIMAL);
    check("arg(i)","1.5707963268",CALCULATOR_VALUE_DECIMAL);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("arg(i)","1.5707963268",CALCULATOR_VALUE_DECIMAL);
    check("re(e^(π*i))","-1",CALCULATOR_VALUE_DECIMAL);
    check("exp(0*i)","1",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("exp(i)","0.5403023059 + 0.8414709848*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("e^i","0.5403023059 + 0.8414709848*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("re(pow(e;π*i))","-1",CALCULATOR_VALUE_DECIMAL);
    check("re(2)","2",CALCULATOR_VALUE_RATIONAL);
    check("im(2)","0",CALCULATOR_VALUE_RATIONAL);
    check("conj(2)","2",CALCULATOR_VALUE_RATIONAL);
    check("re(1+i)+im(2+i)","2",CALCULATOR_VALUE_RATIONAL);
    check("sum(re(1+i);im(2+i))","2",CALCULATOR_VALUE_RATIONAL);
    check("sqrt(re(4+i))","2",CALCULATOR_VALUE_RATIONAL);
    check("sin(re(i))","0",CALCULATOR_VALUE_DECIMAL);
    check("re(3+i)!","6",CALCULATOR_VALUE_RATIONAL);
    check("complex(re(1+i);im(2+i))","1 + i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("re(π+i)","3.1415926536",CALCULATOR_VALUE_DECIMAL);
    check("im(π+i)","1",CALCULATOR_VALUE_DECIMAL);
    check("conj(π+i)","3.1415926536 - i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    const char *invalid[]={"arg(0)","arg(0*i)","2^i","i!"};
    for(size_t n=0;n<sizeof(invalid)/sizeof(invalid[0]);n++) {
        CalculatorValue value={0};CalculatorError error;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_INVALID_ARGUMENT,calculator_compute_value(invalid[n],&context,&value,&error),invalid[n]);
        calculator_value_destroy(&value);
    }
    CalculatorSession session={0};CalculatorError error;char *text=NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_session_compute(&session,1,true,"i=7",&context,&text,&error,NULL));
    TEST_ASSERT_EQUAL_UINT(0,session.variable_count);
    compute(&session,2,true,"I=7","7");
    compute(&session,3,true,"x=2","2");compute(&session,4,true,"y=3","3");compute(&session,5,true,"xy=10","10");
    compute(&session,6,true,"x*y","6");compute(&session,7,true,"xy","10");
    compute(&session,8,true,"I+i","7 + i");
    calculator_session_destroy(&session);
}
static void test_allocation_failures(void)
{
    const char *inputs[]={"w=z^-2","w=conj(z)","w=abs(z)","w=sum(re(z);im(z))"};
    for(size_t input=0;input<sizeof(inputs)/sizeof(inputs[0]);input++) {
    size_t count=0;
    for(size_t failure=0;;failure++) {
        CalculatorSession *session=calloc(1,sizeof(*session));TEST_ASSERT_NOT_NULL(session);
        compute(session,1,true,"z=complex(1/3;2/3)","1/3 + (2/3)*i");
        numforge_test_allocator_begin(failure);char *text=NULL;CalculatorError error;
        CalculatorStatus status=calculator_session_compute(session,2,true,inputs[input],&context,&text,&error,NULL);
        if(failure==0)count=numforge_test_allocator_call_count();numforge_test_allocator_end();
        if(failure==0){TEST_ASSERT_EQUAL(CALCULATOR_OK,status);free(text);}
        else{
            TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,status);TEST_ASSERT_NULL(text);
            TEST_ASSERT_EQUAL(1,session->count);TEST_ASSERT_EQUAL(1,session->variable_count);
            char *saved=NULL;TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_format_value(calculator_session_answer(session),&context,&saved));
            TEST_ASSERT_EQUAL_STRING("1/3 + (2/3)*i",saved);free(saved);
        }
        calculator_session_destroy(session);free(session);if(failure==count)break;
    }
    }
}
int main(void)
{
    UNITY_BEGIN();RUN_TEST(test_exact_and_mixed_arithmetic);RUN_TEST(test_domains);
    RUN_TEST(test_sessions_and_snapshots);RUN_TEST(test_imaginary_unit_and_projections);RUN_TEST(test_allocation_failures);return UNITY_END();
}
