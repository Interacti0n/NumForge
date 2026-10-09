#include <numforge/bigcomplex.h>
#include <numforge/bigrationalcomplex.h>
#include "numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void setUp(void) { numforge_budget_end(); }
void tearDown(void) { numforge_test_allocator_end(); numforge_budget_end(); }
static BigComplex *number(const char *re, const char *im) {
    BigComplex *v=bigcomplex_create(); TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,re,im)); return v;
}
static void text_is(const BigComplex *v, const char *expected) {
    char *text=NULL; TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_to_string(v,&text));
    TEST_ASSERT_EQUAL_STRING(expected,text); free(text);
}
static void test_lifecycle_and_text(void) {
    BigComplex *v=bigcomplex_create(); bool zero=false;
    TEST_ASSERT_NOT_NULL(v); text_is(v,"0");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_is_zero(&zero,v)); TEST_ASSERT_TRUE(zero);
    const char *cases[][3]={{"0","1","i"},{"0","-1","-i"},{"2","1","2 + i"},
        {"2","-1","2 - i"},{"-2.50","-3.25","-2.5 - 3.25*i"},{"0","2","2*i"},{"-0","-0","0"}};
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,cases[i][0],cases[i][1])); text_is(v,cases[i][2]);
    }
    BigDecimal *part=bigdecimal_create();
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(part,"7"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_parts(v,part,part));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(part,"8")); text_is(v,"7 + 7*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_get_imaginary(part,v));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_from_bigdecimal(v,part)); text_is(v,"7");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_copy(v,v)); text_is(v,"7");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_set_strings(v,"9","bad")); text_is(v,"7");
    bigdecimal_destroy(part); bigcomplex_destroy(v); bigcomplex_destroy(NULL);
}
static void test_arithmetic_and_aliasing(void) {
    BigComplex *a=number("2","3"), *b=number("4","-5"), *r=bigcomplex_create();
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_add(r,a,b)); text_is(r,"6 - 2*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_sub(r,a,b)); text_is(r,"-2 + 8*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_mul(r,a,b)); text_is(r,"23 + 2*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_mul(a,a,b)); text_is(a,"23 + 2*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_conjugate(b,b)); text_is(b,"4 + 5*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_negate(b,b)); text_is(b,"-4 - 5*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"0","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_mul(a,a,a)); text_is(a,"-1");
    BigDecimal *norm=bigdecimal_create();
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_abs_squared(norm,b));
    char *s=NULL; TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_to_string(norm,&s)); TEST_ASSERT_EQUAL_STRING("41",s); free(s);
    for(int n=-8;n<=8;n++) {
        char x[24],y[24],re[24],im[24]; snprintf(x,sizeof x,"%d",n);snprintf(y,sizeof y,"%d",n+1);
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,x,y));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,"3","-2"));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_mul(b,a,b));
        snprintf(re,sizeof re,"%d",5*n+2);snprintf(im,sizeof im,"%d",n+3);
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(r,re,im));
        bool equal=false; TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_equal(&equal,b,r)); TEST_ASSERT_TRUE(equal);
    }
    bigdecimal_destroy(norm); bigcomplex_destroy(a);bigcomplex_destroy(b);bigcomplex_destroy(r);
}
static void test_division_and_contracts(void) {
    BigComplex *a=number("1","1"), *b=number("1","-1"), *r=number("9","8");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_div(r,a,b,1,BIGDECIMAL_ROUND_HALF_EVEN)); text_is(r,"i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,"3","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_div(r,a,b,4,BIGDECIMAL_ROUND_HALF_EVEN)); text_is(r,"0.3333 + 0.3333*i");
    const char *expected[]={"0.33 + 0.33*i","0.34 + 0.34*i","0.33 + 0.33*i","0.34 + 0.34*i","0.33 + 0.33*i","0.33 + 0.33*i"};
    for(int mode=0;mode<6;mode++) {
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_div(r,a,b,2,(BigDecimalRoundingMode)mode));text_is(r,expected[mode]);
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,"2","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_div(b,a,b,1,BIGDECIMAL_ROUND_HALF_EVEN)); text_is(b,"0.5 + 0.5*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"-1","-1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,"3","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_div(a,a,b,2,BIGDECIMAL_ROUND_FLOOR)); text_is(a,"-0.34 - 0.34*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"1","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,"0","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_DIVISION_BY_ZERO,bigcomplex_div(r,a,b,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"0.33 + 0.33*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_div(r,a,b,0,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_div(r,a,a,4,(BigDecimalRoundingMode)99));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_add(r,NULL,a));
    bool flag=true;TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_equal(&flag,a,NULL));TEST_ASSERT_TRUE(flag);
    char marker='x', *text=&marker;TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_to_string(NULL,&text));TEST_ASSERT_EQUAL_PTR(&marker,text);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"1E100","1E-100"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_div(a,a,a,34,BIGDECIMAL_ROUND_HALF_EVEN));text_is(a,"1");
    bigcomplex_destroy(a);bigcomplex_destroy(b);bigcomplex_destroy(r);
}
static void decimal_is(const BigDecimal *v, const char *expected) {
    char *s=NULL; TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_to_string(v,&s));
    TEST_ASSERT_EQUAL_STRING(expected,s); free(s);
}
static void test_powers_and_modulus(void) {
    BigComplex *a=number("1","1"), *r=number("9","8");
    BigDecimal *d=bigdecimal_create(); TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow_int(r,a,3,1,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"-2 + 2*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow_int(a,a,-2,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(a,"-0.5*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"3","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow_int(r,a,-2,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"0.1111");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"0","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow_int(r,a,INT64_MIN,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"1");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow_int(r,a,INT64_MAX,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"-i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"0","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow_int(r,a,0,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"1");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_DIVISION_BY_ZERO,bigcomplex_pow_int(r,a,-1,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"1");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_pow_int(r,a,0,0,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"1");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_pow_int(r,NULL,1,4,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_abs(d,a,4,BIGDECIMAL_ROUND_HALF_EVEN));decimal_is(d,"0");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"-3","4"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_abs(d,a,1,BIGDECIMAL_ROUND_HALF_EVEN));decimal_is(d,"5");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"1","1"));
    const char *roots[]={"1.4142","1.4143","1.4142","1.4143","1.4142","1.4142"};
    for(int mode=0;mode<6;mode++) {
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_abs(d,a,5,(BigDecimalRoundingMode)mode));decimal_is(d,roots[mode]);
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_abs(d,a,5,(BigDecimalRoundingMode)99));decimal_is(d,"1.4142");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_abs(d,NULL,5,BIGDECIMAL_ROUND_HALF_EVEN));decimal_is(d,"1.4142");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"1E100","1E-100"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_abs(d,a,5,BIGDECIMAL_ROUND_CEILING));
    BigDecimal *expected=bigdecimal_create();int cmp=1;
    TEST_ASSERT_NOT_NULL(expected);TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(expected,"1.0001E100"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_compare(&cmp,d,expected));TEST_ASSERT_EQUAL(0,cmp);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"3E-100","4E-100"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_abs(d,a,1,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(expected,"5E-100"));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_compare(&cmp,d,expected));TEST_ASSERT_EQUAL(0,cmp);
    bigdecimal_destroy(expected);bigdecimal_destroy(d);bigcomplex_destroy(a);bigcomplex_destroy(r);
}
static void formatted_is(const BigComplex *v, int64_t places, BigDecimalRoundingMode rounding,
    BigDecimalFormatMode mode, const char *expected) {
    char *s=NULL;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format(v,places,rounding,mode,strlen(expected),&s));
    TEST_ASSERT_EQUAL_STRING(expected,s);free(s);
    char marker='x';s=&marker;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_VALUE_TOO_LARGE,bigcomplex_format(v,places,rounding,mode,strlen(expected)-1,&s));
    TEST_ASSERT_EQUAL_PTR(&marker,s);
}
static void test_formatting(void) {
    BigComplex *v=number("1.234","-2.345");
    formatted_is(v,2,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_PLAIN,"1.23 - 2.34*i");
    formatted_is(v,2,BIGDECIMAL_ROUND_FLOOR,BIGDECIMAL_FORMAT_PLAIN,"1.23 - 2.35*i");
    formatted_is(v,2,BIGDECIMAL_ROUND_CEILING,BIGDECIMAL_FORMAT_PLAIN,"1.24 - 2.34*i");
    text_is(v,"1.234 - 2.345*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"0.001","-0.001"));
    formatted_is(v,2,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,"0");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"2","-1"));
    formatted_is(v,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_PLAIN,"2 - i");
    formatted_is(v,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_SCIENTIFIC,"2E+0 - 1E+0*i");
    formatted_is(v,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_MATHEMATICAL,"2 × 10^0 - (1 × 10^0)*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"0","-1"));
    formatted_is(v,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,"-i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"1E100","2E-100"));
    formatted_is(v,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,"1E+100 + 2E-100*i");
    char marker='x',*s=&marker;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_VALUE_TOO_LARGE,bigcomplex_format(v,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_PLAIN,32,&s));
    TEST_ASSERT_EQUAL_PTR(&marker,s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_format(v,-2,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,32,&s));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_format(v,-1,(BigDecimalRoundingMode)99,BIGDECIMAL_FORMAT_AUTO,32,&s));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_format(v,-1,BIGDECIMAL_ROUND_HALF_EVEN,(BigDecimalFormatMode)99,32,&s));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_format(NULL,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,32,&s));
    TEST_ASSERT_EQUAL_PTR(&marker,s);bigcomplex_destroy(v);
}
static void test_exact_rationals_and_forms(void) {
    BigRationalComplex *q=bigrationalcomplex_create(),*r=bigrationalcomplex_create();
    BigRational *part=bigrational_create();BigInt *n=bigint_create(),*d=bigint_create();
    BigComplex *z=bigcomplex_create();BigDecimal *angle=bigdecimal_create();
    TEST_ASSERT_NOT_NULL(q);TEST_ASSERT_NOT_NULL(r);TEST_ASSERT_NOT_NULL(part);TEST_ASSERT_NOT_NULL(n);TEST_ASSERT_NOT_NULL(d);TEST_ASSERT_NOT_NULL(z);TEST_ASSERT_NOT_NULL(angle);
    TEST_ASSERT_EQUAL(BIGINT_OK,bigint_set_string(n,"1"));TEST_ASSERT_EQUAL(BIGINT_OK,bigint_set_string(d,"3"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_set_fraction(part,n,d));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_set_parts(q,part,part));
    char *s=NULL;TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(q,SIZE_MAX,&s));TEST_ASSERT_EQUAL_STRING("1/3 + (1/3)*i",s);free(s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_pow_int(r,q,-1));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(r,SIZE_MAX,&s));TEST_ASSERT_EQUAL_STRING("3/2 - (3/2)*i",s);free(s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_mul(r,r,q));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(r,1,&s));TEST_ASSERT_EQUAL_STRING("1",s);free(s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_pow_int(q,q,2));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(q,SIZE_MAX,&s));TEST_ASSERT_EQUAL_STRING("(2/9)*i",s);free(s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_bigcomplex(z,q,4,BIGDECIMAL_ROUND_HALF_EVEN));text_is(z,"0.2222*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(z,"1","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format_form(z,BIGCOMPLEX_FORM_TRIGONOMETRIC,12,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,80,&s));
    TEST_ASSERT_EQUAL_STRING("cos(0) + i*sin(0)",s);free(s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format_form(z,BIGCOMPLEX_FORM_EXPONENTIAL,12,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,80,&s));
    TEST_ASSERT_EQUAL_STRING("e^(i*(0))",s);free(s);
    const char *quadrants[][3]={{"1","1","0.785398"},{"-1","1","2.35619"},{"-1","-1","-2.35619"},{"1","-1","-0.785398"},{"0","1","1.5708"},{"0","-1","-1.5708"},{"-1","0","3.14159"}};
    for(size_t i=0;i<sizeof(quadrants)/sizeof(quadrants[0]);i++){
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(z,quadrants[i][0],quadrants[i][1]));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_arg(angle,z,6,BIGDECIMAL_ROUND_HALF_EVEN));decimal_is(angle,quadrants[i][2]);
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format_form(z,BIGCOMPLEX_FORM_EXPONENTIAL,12,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,80,&s));
    TEST_ASSERT_EQUAL_STRING("e^(i*(π))",s);free(s);
    char marker='x';s=&marker;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_VALUE_TOO_LARGE,bigcomplex_format_form(z,BIGCOMPLEX_FORM_EXPONENTIAL,12,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,1,&s));TEST_ASSERT_EQUAL_PTR(&marker,s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(z,"0","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_arg(angle,z,6,BIGDECIMAL_ROUND_HALF_EVEN));decimal_is(angle,"3.14159");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format_form(z,BIGCOMPLEX_FORM_EXPONENTIAL,12,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,1,&s));TEST_ASSERT_EQUAL_STRING("0",s);free(s);
    BigDecimal *radius=bigdecimal_create();TEST_ASSERT_NOT_NULL(radius);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(radius,"2"));TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(angle,"0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_polar(z,radius,angle,12,BIGDECIMAL_ROUND_HALF_EVEN));text_is(z,"2");
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(angle,"0.7"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_polar(z,radius,angle,16,BIGDECIMAL_ROUND_HALF_EVEN));
    formatted_is(z,10,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_PLAIN,"1.5296843746 + 1.2884353745*i");
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(angle,"0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_polar(z,radius,angle,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(radius,"-1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_set_polar(z,radius,angle,12,BIGDECIMAL_ROUND_HALF_EVEN));text_is(z,"2");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_from_bigcomplex(r,z));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(r,1,&s));TEST_ASSERT_EQUAL_STRING("2",s);free(s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_format_form(r,BIGCOMPLEX_FORM_EXPONENTIAL,12,-1,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,80,&s));
    TEST_ASSERT_EQUAL_STRING("(2)*e^(i*(0))",s);free(s);
    bigdecimal_destroy(radius);
    bigrationalcomplex_destroy(q);bigrationalcomplex_destroy(r);bigrational_destroy(part);bigint_destroy(n);bigint_destroy(d);bigcomplex_destroy(z);bigdecimal_destroy(angle);
}
static void test_rational_failure_contracts(void) {
    BigRationalComplex *a=bigrationalcomplex_create(),*r=bigrationalcomplex_create(),*zero=bigrationalcomplex_create();
    BigRational *part=bigrational_create();BigInt *n=bigint_create();BigComplex *z=number("1.25","-0.5");
    TEST_ASSERT_NOT_NULL(a);TEST_ASSERT_NOT_NULL(r);TEST_ASSERT_NOT_NULL(zero);TEST_ASSERT_NOT_NULL(part);TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_EQUAL(BIGINT_OK,bigint_set_string(n,"7"));TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_from_bigint(part,n));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_from_bigcomplex(a,z));
    for(int op=0;op<11;op++){
        size_t count=0;
        for(size_t failure=0;;failure++){
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_from_bigint(part,n));
            TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_set_parts(r,part,part));
            TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(z,"9","8"));
            numforge_test_allocator_begin(failure);BigComplexStatus status;char marker='x',*s=&marker;
            switch(op){
                case 0:status=bigrationalcomplex_copy(r,a);break;
                case 1:status=bigrationalcomplex_add(r,a,a);break;
                case 2:status=bigrationalcomplex_sub(r,a,a);break;
                case 3:status=bigrationalcomplex_mul(r,a,a);break;
                case 4:status=bigrationalcomplex_div(r,a,a);break;
                case 5:status=bigrationalcomplex_pow_int(r,a,-2);break;
                case 6:status=bigrationalcomplex_to_bigcomplex(z,a,8,BIGDECIMAL_ROUND_HALF_EVEN);break;
                case 7:status=bigrationalcomplex_from_bigcomplex(r,z);break;
                case 8:status=bigrationalcomplex_to_string(a,80,&s);break;
                case 9:status=bigrationalcomplex_conjugate(r,a);break;
                default:status=bigrationalcomplex_abs_squared(part,a);break;
            }
            if(failure==0)count=numforge_test_allocator_call_count();numforge_test_allocator_end();
            if(failure==0){TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,status);if(op==8)free(s);}
            else{
                TEST_ASSERT_EQUAL(BIGCOMPLEX_OUT_OF_MEMORY,status);text_is(z,"9 + 8*i");
                char *unchanged=NULL;TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(r,80,&unchanged));
                TEST_ASSERT_EQUAL_STRING("7 + 7*i",unchanged);free(unchanged);if(op==8)TEST_ASSERT_EQUAL_PTR(&marker,s);
                TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_to_string(part,&unchanged));
                TEST_ASSERT_EQUAL_STRING("7",unchanged);free(unchanged);
            }if(failure==count)break;
        }
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_DIVISION_BY_ZERO,bigrationalcomplex_div(r,a,zero));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_DIVISION_BY_ZERO,bigrationalcomplex_pow_int(r,zero,-1));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_pow_int(r,zero,0));
    char *s=NULL;TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(r,1,&s));TEST_ASSERT_EQUAL_STRING("1",s);free(s);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(z,"0","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_from_bigcomplex(a,z));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_pow_int(a,a,INT64_MIN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(a,1,&s));TEST_ASSERT_EQUAL_STRING("1",s);free(s);
    TEST_ASSERT_TRUE(numforge_budget_begin(1000,0,0));
    TEST_ASSERT_NULL(bigrationalcomplex_create());
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OUT_OF_MEMORY,bigrationalcomplex_copy(r,a));
    numforge_budget_end();
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(r,1,&s));TEST_ASSERT_EQUAL_STRING("1",s);free(s);
    bigrationalcomplex_destroy(a);bigrationalcomplex_destroy(r);bigrationalcomplex_destroy(zero);bigrational_destroy(part);bigint_destroy(n);bigcomplex_destroy(z);
}
static void test_complex_exponential_and_exact_helpers(void) {
    BigComplex *v=number("1","1");char *text=NULL;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_exp(v,v,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format(v,8,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,80,&text));
    TEST_ASSERT_EQUAL_STRING("1.46869394 + 2.28735529*i",text);free(text);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_exp(v,v,0,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_exp(v,NULL,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"3","4"));
    BigRationalComplex *r=bigrationalcomplex_create();BigRational *squared=bigrational_create();
    TEST_ASSERT_NOT_NULL(r);TEST_ASSERT_NOT_NULL(squared);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_from_bigcomplex(r,v));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_abs_squared(squared,r));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK,bigrational_to_string(squared,&text));TEST_ASSERT_EQUAL_STRING("25",text);free(text);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_conjugate(r,r));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigrationalcomplex_to_string(r,80,&text));TEST_ASSERT_EQUAL_STRING("3 - 4*i",text);free(text);
    bigrational_destroy(squared);bigrationalcomplex_destroy(r);bigcomplex_destroy(v);
}
static void test_principal_square_root(void) {
    const char *cases[][3]={{"3","4","2 + i"},{"3","-4","2 - i"},{"-3","4","1 + 2*i"},
        {"-3","-4","1 - 2*i"},{"-1","0","i"},{"0","0","0"},{"4","0","2"},
        {"-15241578750190521","0","123456789*i"}};
    BigComplex *v=bigcomplex_create();TEST_ASSERT_NOT_NULL(v);
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,cases[i][0],cases[i][1]));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_sqrt(v,v,4,BIGDECIMAL_ROUND_HALF_EVEN));
        text_is(v,cases[i][2]);
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"1","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_sqrt(v,v,12,BIGDECIMAL_ROUND_HALF_EVEN));char *text=NULL;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format(v,10,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,100,&text));
    TEST_ASSERT_EQUAL_STRING("1.0986841135 + 0.4550898606*i",text);free(text);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_sqrt(v,v,0,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_sqrt(v,v,INT64_MAX,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_sqrt(v,NULL,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"-1E100","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_sqrt(v,v,12,BIGDECIMAL_ROUND_HALF_EVEN));
    BigComplex *expected=number("5E-51","1E50");bool equal=false;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_equal(&equal,v,expected));TEST_ASSERT_TRUE(equal);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"1E100","-1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_sqrt(v,v,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(expected,"1E50","-5E-51"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_equal(&equal,v,expected));TEST_ASSERT_TRUE(equal);
    bigcomplex_destroy(expected);bigcomplex_destroy(v);
}
static void test_principal_logarithm(void) {
    const char *cases[][3]={{"1","0","0"},{"-1","0","3.1415926536*i"},
        {"0","1","1.5707963268*i"},{"0","-1","-1.5707963268*i"},
        {"1","1","0.3465735903 + 0.7853981634*i"},
        {"-3","-4","1.6094379124 - 2.2142974356*i"}};
    BigComplex *v=bigcomplex_create(),*r=number("9","8");char *text=NULL;
    TEST_ASSERT_NOT_NULL(v);
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,cases[i][0],cases[i][1]));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_ln(v,v,12,BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format(v,10,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,100,&text));
        TEST_ASSERT_EQUAL_STRING(cases[i][2],text);free(text);text=NULL;
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"0","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_ln(r,v,12,BIGDECIMAL_ROUND_HALF_EVEN));text_is(r,"9 + 8*i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_ln(r,v,0,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_ln(r,v,INT64_MAX,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_ln(r,v,12,(BigDecimalRoundingMode)99));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_ln(r,NULL,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"1","1E-40"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_ln(r,v,12,BIGDECIMAL_ROUND_HALF_EVEN));
    BigComplex *expected=number("5E-81","1E-40");bool equal=false;
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_equal(&equal,r,expected));TEST_ASSERT_TRUE(equal);
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(v,"-1","-1E-40"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_ln(r,v,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format(r,10,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,100,&text));
    TEST_ASSERT_EQUAL_STRING("5E-81 - 3.1415926536*i",text);free(text);
    bigcomplex_destroy(expected);bigcomplex_destroy(r);bigcomplex_destroy(v);
}
static void test_principal_powers(void) {
    const char *cases[][5]={{"0","1","0","1","0.2078795764"},
        {"2","0","0","1","0.7692389014 + 0.6389612763*i"},
        {"-1","0","0.3","0","0.5877852523 + 0.8090169944*i"},{"0","0","0.5","0","0"},
        {"0","0","0","0","1"}};
    BigComplex *a=bigcomplex_create(),*b=bigcomplex_create();char *text=NULL;
    TEST_ASSERT_NOT_NULL(a);TEST_ASSERT_NOT_NULL(b);
    for(size_t n=0;n<sizeof(cases)/sizeof(cases[0]);n++) {
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,cases[n][0],cases[n][1]));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,cases[n][2],cases[n][3]));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow(b,a,b,12,BIGDECIMAL_ROUND_HALF_EVEN));
        TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format(b,10,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,100,&text));
        TEST_ASSERT_EQUAL_STRING(cases[n][4],text);free(text);text=NULL;
    }
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"0","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,"-0.5","0"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_DIVISION_BY_ZERO,bigcomplex_pow(b,a,b,12,BIGDECIMAL_ROUND_HALF_EVEN));text_is(b,"-0.5");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(b,"1","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_pow(b,a,b,12,BIGDECIMAL_ROUND_HALF_EVEN));text_is(b,"1 + i");
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_pow(b,a,b,INT64_MAX,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_pow(b,a,b,0,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_INVALID_ARGUMENT,bigcomplex_pow(b,a,b,12,(BigDecimalRoundingMode)99));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_NULL_ARGUMENT,bigcomplex_pow(b,a,NULL,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(a,"0","1"));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_pow(a,a,a,12,BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_format(a,10,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,100,&text));
    TEST_ASSERT_EQUAL_STRING("0.2078795764",text);free(text);
    bigcomplex_destroy(a);bigcomplex_destroy(b);
}
static void test_allocation_failures(void) {
    BigComplex *a=number("1.2","-3.4"), *b=number("5.6","7.8"), *r=number("9","8");
    BigDecimal *d=bigdecimal_create(); TEST_ASSERT_NOT_NULL(d);
    for(int operation=0;operation<15;operation++) {
        size_t count=0;
        for(size_t failure=0;;failure++) {
            TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,bigcomplex_set_strings(r,"9","8"));
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK,bigdecimal_set_string(d,"7"));
            numforge_test_allocator_begin(failure);
            BigComplexStatus status;
            char marker='x', *text=&marker;
            switch(operation) {
                case 0: status=bigcomplex_set_strings(r,"5","6");break;
                case 1: status=bigcomplex_copy(r,a);break;
                case 2: status=bigcomplex_mul(r,a,b);break;
                case 3: status=bigcomplex_div(r,a,b,12,BIGDECIMAL_ROUND_HALF_EVEN);break;
                case 4: status=bigcomplex_conjugate(r,a);break;
                case 5: status=bigcomplex_to_string(a,&text);break;
                case 6: status=bigcomplex_pow_int(r,a,-3,12,BIGDECIMAL_ROUND_HALF_EVEN);break;
                case 7: status=bigcomplex_abs(d,a,12,BIGDECIMAL_ROUND_HALF_EVEN);break;
                case 8: status=bigcomplex_format(a,2,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_MATHEMATICAL,80,&text);break;
                case 9: status=bigcomplex_arg(d,a,8,BIGDECIMAL_ROUND_HALF_EVEN);break;
                case 10: status=bigcomplex_format_form(a,BIGCOMPLEX_FORM_EXPONENTIAL,8,3,BIGDECIMAL_ROUND_HALF_EVEN,BIGDECIMAL_FORMAT_AUTO,80,&text);break;
                case 11: status=bigcomplex_exp(r,a,8,BIGDECIMAL_ROUND_HALF_EVEN);break;
                case 12: status=bigcomplex_sqrt(r,a,8,BIGDECIMAL_ROUND_HALF_EVEN);break;
                case 13: status=bigcomplex_ln(r,a,8,BIGDECIMAL_ROUND_HALF_EVEN);break;
                default: status=bigcomplex_pow(r,a,b,8,BIGDECIMAL_ROUND_HALF_EVEN);break;
            }
            if(failure==0) count=numforge_test_allocator_call_count();
            numforge_test_allocator_end();
            if(failure==0) { TEST_ASSERT_EQUAL(BIGCOMPLEX_OK,status);if(operation==5 || operation==8 || operation==10)free(text); }
            else { TEST_ASSERT_EQUAL(BIGCOMPLEX_OUT_OF_MEMORY,status);text_is(r,"9 + 8*i");decimal_is(d,"7");if(operation==5 || operation==8 || operation==10)TEST_ASSERT_EQUAL_PTR(&marker,text); }
            if(failure==count)break;
        }
    }
    TEST_ASSERT_TRUE(numforge_budget_begin(1000,0,0));
    TEST_ASSERT_NULL(bigcomplex_create());
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_MEMORY,numforge_budget_failure());numforge_budget_end();
    bigdecimal_destroy(d);bigcomplex_destroy(a);bigcomplex_destroy(b);bigcomplex_destroy(r);
}
int main(void) {
    UNITY_BEGIN();RUN_TEST(test_lifecycle_and_text);RUN_TEST(test_arithmetic_and_aliasing);
    RUN_TEST(test_division_and_contracts);RUN_TEST(test_powers_and_modulus);RUN_TEST(test_formatting);RUN_TEST(test_exact_rationals_and_forms);RUN_TEST(test_rational_failure_contracts);RUN_TEST(test_complex_exponential_and_exact_helpers);RUN_TEST(test_principal_square_root);RUN_TEST(test_principal_logarithm);RUN_TEST(test_principal_powers);RUN_TEST(test_allocation_failures);return UNITY_END();
}
