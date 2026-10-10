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
static void test_complex_aggregates(void)
{
    check("sum(1/3;i;2/3)","1 + i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("product(1+i;1-i;1/3)","2/3",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("mean(1/3;i;2/3)","1/3 + (1/3)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sum(i;-i)","0",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("product(i;0)","0",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("mean(i)","i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sum(i;sqrt(2);-i)","1.4142135624",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("product(i;sqrt(2))","1.4142135624*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("mean(i;sqrt(2))","0.7071067812 + 0.5*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sum(1;i;sum(2;-i))","3",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sum(sqrt(-1);1)","1 + i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("mean(1;2;3)","2",CALCULATOR_VALUE_INTEGER);
    check("sum(1E100;i;-1E100)","i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    char many[1024]="sum(i";size_t used=5;
    for(size_t i=1;i<256;i++) {many[used++]=';';many[used++]='i';}
    many[used]=')';many[used+1]='\0';check(many,"256*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    CalculatorValue value={0};CalculatorError error;
    many[used++]=';';many[used++]='i';many[used++]=')';many[used]='\0';
    TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,calculator_compute_value(many,&context,&value,&error));
    calculator_value_destroy(&value);
    const char *invalid[]={"sum()","product()","mean()","median(i;1)","min(i;1)","variance(i;1)"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,calculator_compute_value(invalid[i],&context,&value,&error));
        calculator_value_destroy(&value);
    }
    TEST_ASSERT_EQUAL(CALCULATOR_DIMENSION_ERROR,calculator_compute_value("sum(i;qty(1;\"m\"))",&context,&value,&error));
    calculator_value_destroy(&value);
    context.significant_division=false;
    check("mean(i;1)","0.5 + 0.5*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
}
static void test_domains(void)
{
    const char *inputs[]={"(0*i)^i",
        "complex(complex(1;2);3)","complex(1;2)!"};
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
    const char *invalid[]={"arg(0)","arg(0*i)","i!"};
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
static void test_principal_square_roots(void)
{
    check("sqrt(3+4i)","2 + i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(3-4i)","2 - i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(-3+4i)","1 + 2*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(-3-4i)","1 - 2*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(-1)","i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("√(-4)","2*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(-1/9)","(1/3)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(1-5)+3","3 + 2*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(-1)^2","-1",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(sqrt(-1)^2)","i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(-2)","1.4142135624*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sqrt(-1E-100)","(1/100000000000000000000000000000000000000000000000000)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    context.significant_division=false;
    check("sqrt(-4)","2*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    context.significant_division=true;
    check("sqrt(-1+0i)","i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("√(-1+0i)","i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(0*i)","0",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(-1/9+0i)","(1/3)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt((1/3+i/7)^2)","1/3 + (1/7)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt((-1/3+i/7)^2)","1/3 - (1/7)*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("sqrt(1+i)","1.0986841135 + 0.4550898606*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sqrt(i)","0.7071067812 + 0.7071067812*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sqrt(-i)","0.7071067812 - 0.7071067812*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    CalculatorSession session={0};
    compute(&session,1,true,"z=sqrt(3+4i)","2 + i");
    compute(&session,2,true,"z^2","3 + 4*i");
    calculator_session_destroy(&session);
}
static void test_principal_logarithms(void)
{
    check("ln(1+0i)","0",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("ln(-1+0i)","3.1415926536*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("ln(i)","1.5707963268*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("ln(-i)","-1.5707963268*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("ln(1+i)","0.3465735903 + 0.7853981634*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("ln(3+4i)","1.6094379124 + 0.927295218*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("ln(-3-4i)","1.6094379124 - 2.2142974356*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("exp(ln(2+3i))","2 + 3*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("re(ln(1+i))","0.3465735903",CALCULATOR_VALUE_DECIMAL);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("ln(i)","1.5707963268*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    const char *invalid[]={"ln(0*i)","ln(0)"};
    for(size_t n=0;n<sizeof(invalid)/sizeof(invalid[0]);n++) {
        CalculatorValue value={0};CalculatorError error;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_INVALID_ARGUMENT,calculator_compute_value(invalid[n],&context,&value,&error),invalid[n]);
        calculator_value_destroy(&value);
    }
    CalculatorSession session={0};
    compute(&session,1,true,"z=ln(i)","1.5707963268*i");
    compute(&session,2,true,"exp(ln(2+3i))","2 + 3*i");
    char *text=NULL;CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_session_compute(&session,3,true,"z=ln(0*i)",&context,&text,&error,NULL));
    TEST_ASSERT_NULL(text);TEST_ASSERT_EQUAL_UINT(1,session.variable_count);
    compute(&session,4,false,"ans","2 + 3*i");
    calculator_session_destroy(&session);
}
static void test_principal_powers(void)
{
    check("i^i","0.2078795764",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("2^i","0.7692389014 + 0.6389612763*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("pow(2;i)","0.7692389014 + 0.6389612763*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("complex(1;2)^complex(1;0)","1 + 2*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("(-1+0i)^0.3","0.5877852523 + 0.8090169944*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("(0*i)^(1/2)","0",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("(0*i)^0","1",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("(1+i)^3","-2 + 2*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    CalculatorValue half={0};CalculatorError half_error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_compute_value("(-1+0i)^(1/2)",&context,&half,&half_error));
    BigDecimal *part=bigdecimal_create(),*bound=bigdecimal_create();int comparison=0;
    TEST_ASSERT_NOT_NULL(part);TEST_ASSERT_NOT_NULL(bound);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_get_real(part,half.complex_decimal));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_abs(part,part));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(bound,"1E-30"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_compare(&comparison,part,bound));TEST_ASSERT_TRUE(comparison<0);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_get_imaginary(part,half.complex_decimal));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(bound,"1"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_compare(&comparison,part,bound));TEST_ASSERT_EQUAL(0,comparison);
    bigdecimal_destroy(part);bigdecimal_destroy(bound);calculator_value_destroy(&half);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("2^i","0.7692389014 + 0.6389612763*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    CalculatorSession session={0};char *text=NULL;CalculatorError error;
    compute(&session,1,true,"z=i^i","0.2078795764");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_session_compute(&session,2,true,"z=(0*i)^i",&context,&text,&error,NULL));
    TEST_ASSERT_NULL(text);TEST_ASSERT_EQUAL_UINT(1,session.variable_count);
    compute(&session,3,false,"ans","0.2078795764");
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,calculator_session_compute(&session,4,true,"z=(0*i)^(-1/2)",&context,&text,&error,NULL));
    TEST_ASSERT_EQUAL(CALCULATOR_DIVISION_BY_ZERO,calculator_session_compute(&session,5,true,"0^(-1/2)",&context,&text,&error,NULL));
    calculator_session_destroy(&session);
}
static void test_base_logarithms(void)
{
    check("log(i)","0.6821881769*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("log(i;i)","1",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("log(-1+0i;i)","2",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("log(-i;i)","-1",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("log(i;-1)","0.5",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("log(100;10+0i)","2",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("log(i)","0.6821881769*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    const char *invalid[]={"log(0*i)","log(i;1)","log(i;0)"};
    for(size_t n=0;n<sizeof(invalid)/sizeof(invalid[0]);n++) {
        CalculatorValue value={0};CalculatorError error;
        TEST_ASSERT_EQUAL_MESSAGE(CALCULATOR_INVALID_ARGUMENT,calculator_compute_value(invalid[n],&context,&value,&error),invalid[n]);
        calculator_value_destroy(&value);
    }
    CalculatorSession session={0};char *text=NULL;CalculatorError error;
    compute(&session,1,true,"z=log(i;i)","1");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_session_compute(&session,2,true,"z=log(i;1)",&context,&text,&error,NULL));
    TEST_ASSERT_NULL(text);TEST_ASSERT_EQUAL_UINT(1,session.variable_count);
    compute(&session,3,false,"ans","1");calculator_session_destroy(&session);
}
static void test_complex_trigonometry(void)
{
    check("sin(i)","1.1752011936*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("cos(i)","1.5430806348",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tan(i)","0.761594156*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sin(1+i)","1.2984575814 + 0.6349639148*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("cos(1+i)","0.8337300251 - 0.9888977058*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tan(1+i)","0.2717525853 + 1.0839233273*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sin(0*i)","0",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("cos(0*i)","1",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tan(0*i)","0",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("re(sin(1+i)^2+cos(1+i)^2)","1",CALCULATOR_VALUE_DECIMAL);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("sin(1+i)","1.2984575814 + 0.6349639148*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("cos(i)","1.5430806348",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tan(1+i)","0.2717525853 + 1.0839233273*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sin(90)","1",CALCULATOR_VALUE_DECIMAL);
    CalculatorSession session={0};char *text=NULL;CalculatorError error;
    compute(&session,1,true,"z=tan(i)","0.761594156*i");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_session_compute(&session,2,true,"z=tan(90)",&context,&text,&error,NULL));
    TEST_ASSERT_NULL(text);TEST_ASSERT_EQUAL_UINT(1,session.variable_count);
    compute(&session,3,false,"ans","0.761594156*i");calculator_session_destroy(&session);
}
static void test_complex_hyperbolic(void)
{
    check("sinh(i)","0.8414709848*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("cosh(i)","0.5403023059",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(i)","1.5574077247*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("sinh(1+i)","0.6349639148 + 1.2984575814*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("cosh(1+i)","0.8337300251 + 0.9888977058*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(1+i)","1.0839233273 + 0.2717525853*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(-1+i)","-1.0839233273 + 0.2717525853*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(1-i)","1.0839233273 - 0.2717525853*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(0.25+0.5i)","0.3124206925 + 0.5045007027*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(0*i)","0",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("cosh(0*i)","1",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(1E-40+0i)","1E-40",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("tanh(1E100+0i)","1",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("re(cosh(1+i)^2-sinh(1+i)^2)","1",CALCULATOR_VALUE_DECIMAL);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("tanh(1+i)","1.0839233273 + 0.2717525853*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    CalculatorSession session={0};char *text=NULL;CalculatorError error;
    compute(&session,1,true,"z=tanh(i)","1.5574077247*i");
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_session_compute(&session,2,true,"z=atanh(1)",&context,&text,&error,NULL));
    TEST_ASSERT_NULL(text);TEST_ASSERT_EQUAL_UINT(1,session.variable_count);
    compute(&session,3,false,"ans","1.5574077247*i");calculator_session_destroy(&session);
}
static void test_complex_inverse_trigonometry(void)
{
    check("asin(i)","0.881373587*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("asin(0.1+1E-100i)","0.1001674212 + 1.0050378153E-100*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("asin(-0.1-1E-100i)","-0.1001674212 - 1.0050378153E-100*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("acos(1-1E-60+0i)","1.4142135624E-30",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("asin(1+i)","0.6662394325 + 1.0612750619*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("acos(1+i)","0.9045568943 - 1.0612750619*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("atan(1+i)","1.0172219679 + 0.4023594781*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("asin(2+0i)","1.5707963268 - 1.3169578969*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("acos(2+0i)","1.3169578969*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("atan(2i)","1.5707963268 + 0.5493061443*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("atan(-2i)","-1.5707963268 - 0.5493061443*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("asin(0.5)","30",CALCULATOR_VALUE_DECIMAL);
    check("arcsin(0.5+0i)","0.5235987756",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    CalculatorSession session={0};char *text=NULL;CalculatorError error;
    compute(&session,1,true,"z=asin(i)","0.881373587*i");
    const char *bad[]={"z=atan(i)","z=atan(-i)","z=atanh(1)"};
    for(size_t j=0;j<3;j++) {
        TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT,calculator_session_compute(&session,j+2,true,bad[j],&context,&text,&error,NULL));
        TEST_ASSERT_NULL(text);TEST_ASSERT_EQUAL_UINT(1,session.variable_count);
    }
    compute(&session,5,false,"ans","0.881373587*i");calculator_session_destroy(&session);
}
static void test_automatic_complex_domains(void)
{
    check("ln(-1)","3.1415926536*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("log(-1)","1.3643763538*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("log(-1;-1)","1",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("asin(2)","1.5707963268 - 1.3169578969*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("acos(2)","1.3169578969*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("acosh(-1)","3.1415926536*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("acosh(-2)","1.3169578969 + 3.1415926536*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("atanh(2)","0.5493061443 - 1.5707963268*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("atanh(-2)","-0.5493061443 + 1.5707963268*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("asinh(1+i)","1.0612750619 + 0.6662394325*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("acosh(1+i)","1.0612750619 + 0.9045568943*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("atanh(1+i)","0.4023594781 + 1.0172219679*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("root(-4;2)","2*i",CALCULATOR_VALUE_COMPLEX_RATIONAL);
    check("root(-16;4)","1.4142135624 + 1.4142135624*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("root(-8;3)","-2",CALCULATOR_VALUE_INTEGER);
    check("cbrt(-8)","-2",CALCULATOR_VALUE_INTEGER);
    check("cbrt(i)","0.8660254038 + 0.5*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("2^0.5","1.4142135624",CALCULATOR_VALUE_DECIMAL);
    check("pow(-1;0.3)","0.5877852523 + 0.8090169944*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("(-1)^0.3","0.5877852523 + 0.8090169944*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("ln(-1)+sqrt(-1)","4.1415926536*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    context.angle_unit=CALCULATOR_ANGLE_DEGREES;
    check("asin(2)","1.5707963268 - 1.3169578969*i",CALCULATOR_VALUE_COMPLEX_DECIMAL);
    check("asin(0.5)","30",CALCULATOR_VALUE_DECIMAL);
    CalculatorSession session={0};
    compute(&session,1,true,"x=-1","-1");
    compute(&session,2,true,"z=ln(x)","3.1415926536*i");
    compute(&session,3,false,"re(exp(z))","-1");calculator_session_destroy(&session);
}
static bool sqrt_allocation_only=false;
static bool promotion_allocation_only=false;
static bool aggregate_allocation_only=false;
static size_t allocation_shard=0;
static size_t allocation_shards=1;
static void test_allocation_failures(void)
{
    const char *inputs[]={"w=ln(-1)","w=log(-1;-1)","w=asin(2)","w=acosh(-1)","w=atanh(2)",
        "w=asinh(i)","w=root(-4;2)","w=root(-16;4)","w=2^0.5","w=pow(-1;0.3)","w=z^-2","w=conj(z)","w=abs(z)","w=sum(re(z);im(z))",
        "w=sqrt(-1/9)","w=sqrt(-2)","w=sqrt(4/9)+1/3","w=sqrt(3+4i)","w=sqrt(z)","w=ln(z)","w=z^i","w=log(i;i)",
        "w=sin(i)","w=cos(i)","w=tan(i)","w=sinh(0*i)","w=cosh(0*i)",
        "w=tanh(0*i)","w=tanh(0.1+0.1i)","w=tanh(1+i)",
        "w=asin(0*i)","w=acos(1+0i)","w=atan(0*i)",
        "w=sum(z;i;1/3)","w=product(z;i;1/3)","w=mean(z;i;1/3)",
        "w=sum(z;0.5+i)","w=product(z;0.5+i)","w=mean(z;0.5+i)"};
    for(size_t input=0;input<sizeof(inputs)/sizeof(inputs[0]);input++) {
    if (promotion_allocation_only && input>=10) continue;
    if (aggregate_allocation_only && strncmp(inputs[input],"w=sum(z;",8)!=0 &&
        strncmp(inputs[input],"w=product(z;",12)!=0 && strncmp(inputs[input],"w=mean(z;",9)!=0) continue;
    if (sqrt_allocation_only && strncmp(inputs[input],"w=sqrt(",7)!=0) continue;
    size_t count=0;
    for(size_t failure=0;;) {
        CalculatorSession *session=calloc(1,sizeof(*session));TEST_ASSERT_NOT_NULL(session);
        compute(session,1,true,"z=complex(1/3;2/3)","1/3 + (2/3)*i");
        numforge_test_allocator_begin(failure);char *text=NULL;CalculatorError error;
        CalculatorStatus status=calculator_session_compute(session,2,true,inputs[input],&context,&text,&error,NULL);
        if(failure==0)count=numforge_test_allocator_call_count();
        numforge_test_allocator_end();
        if(failure==0){TEST_ASSERT_EQUAL(CALCULATOR_OK,status);free(text);}
        else{
            TEST_ASSERT_NOT_EQUAL(CALCULATOR_OK,status);TEST_ASSERT_NULL(text);
            TEST_ASSERT_EQUAL(1,session->count);TEST_ASSERT_EQUAL(1,session->variable_count);
            char *saved=NULL;TEST_ASSERT_EQUAL(CALCULATOR_OK,calculator_format_value(calculator_session_answer(session),&context,&saved));
            TEST_ASSERT_EQUAL_STRING("1/3 + (2/3)*i",saved);free(saved);
        }
        calculator_session_destroy(session);free(session);
        if(failure==0) {
            if(count<=allocation_shard) break;
            failure=allocation_shard+1;
        } else {
            if(count-failure<allocation_shards) break;
            failure+=allocation_shards;
        }
    }
    }
}
int main(int argc,char **argv)
{
    if(argc==4 && strcmp(argv[1],"--allocation-shard")==0) {
        char *end=NULL;
        long shard=strtol(argv[2],&end,10);
        if(!*argv[2] || *end || shard<0 || shard>63) return 2;
        long shards=strtol(argv[3],&end,10);
        if(!*argv[3] || *end || shards<1 || shards>64 || shard>=shards) return 2;
        allocation_shard=(size_t)shard;allocation_shards=(size_t)shards;
        UNITY_BEGIN();RUN_TEST(test_allocation_failures);return UNITY_END();
    }
    promotion_allocation_only=argc==2 && strcmp(argv[1],"--promotion-allocation-only")==0;
    aggregate_allocation_only=argc==2 && strcmp(argv[1],"--aggregate-allocation-only")==0;
    sqrt_allocation_only=argc==2 && strcmp(argv[1],"--sqrt-allocation-only")==0;
    UNITY_BEGIN();RUN_TEST(test_exact_and_mixed_arithmetic);RUN_TEST(test_domains);RUN_TEST(test_complex_aggregates);
    RUN_TEST(test_sessions_and_snapshots);RUN_TEST(test_imaginary_unit_and_projections);
    RUN_TEST(test_principal_square_roots);RUN_TEST(test_principal_logarithms);RUN_TEST(test_principal_powers);RUN_TEST(test_base_logarithms);RUN_TEST(test_complex_trigonometry);RUN_TEST(test_complex_hyperbolic);RUN_TEST(test_complex_inverse_trigonometry);RUN_TEST(test_automatic_complex_domains);
    if(argc!=2 || strcmp(argv[1],"--numeric-only")!=0)RUN_TEST(test_allocation_failures);
    return UNITY_END();
}
