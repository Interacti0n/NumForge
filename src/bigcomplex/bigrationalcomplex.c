#include <numforge/bigrationalcomplex.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct BigRationalComplex { BigRational *real, *imaginary; };
static BigComplexStatus mapped(BigRationalStatus s) {
    switch(s) {
    case BIGRATIONAL_OK:return BIGCOMPLEX_OK;
    case BIGRATIONAL_NULL_ARGUMENT:return BIGCOMPLEX_NULL_ARGUMENT;
    case BIGRATIONAL_OUT_OF_MEMORY:return BIGCOMPLEX_OUT_OF_MEMORY;
    case BIGRATIONAL_INVALID_ARGUMENT:return BIGCOMPLEX_INVALID_ARGUMENT;
    case BIGRATIONAL_DIVISION_BY_ZERO:return BIGCOMPLEX_DIVISION_BY_ZERO;
    case BIGRATIONAL_VALUE_TOO_LARGE:return BIGCOMPLEX_VALUE_TOO_LARGE;
    case BIGRATIONAL_SCALE_OVERFLOW:return BIGCOMPLEX_SCALE_OVERFLOW;
    default:return BIGCOMPLEX_INVALID_ARGUMENT;
    }
}
BigRationalComplex *bigrationalcomplex_create(void) {
    BigRationalComplex *v=numforge_calloc(1,sizeof(*v)); if(!v)return NULL;
    v->real=bigrational_create();v->imaginary=bigrational_create();
    if(!v->real || !v->imaginary){bigrationalcomplex_destroy(v);return NULL;}return v;
}
void bigrationalcomplex_destroy(BigRationalComplex *v) {
    if(v){bigrational_destroy(v->real);bigrational_destroy(v->imaginary);free(v);}
}
static void commit(BigRationalComplex *r,BigRationalComplex *t){BigRationalComplex old=*r;*r=*t;*t=old;}
#define TRY(call) do {s=(call);if(s!=BIGRATIONAL_OK)goto done;}while(0)
BigComplexStatus bigrationalcomplex_set_parts(BigRationalComplex *r,const BigRational *re,const BigRational *im){
    if(!r || !re || !im)return BIGCOMPLEX_NULL_ARGUMENT;
    BigRationalComplex *t=bigrationalcomplex_create();if(!t)return BIGCOMPLEX_OUT_OF_MEMORY;
    BigRationalStatus s;TRY(bigrational_copy(t->real,re));TRY(bigrational_copy(t->imaginary,im));commit(r,t);
done:bigrationalcomplex_destroy(t);return mapped(s);
}
BigComplexStatus bigrationalcomplex_copy(BigRationalComplex *r,const BigRationalComplex *v){
    return v?bigrationalcomplex_set_parts(r,v->real,v->imaginary):BIGCOMPLEX_NULL_ARGUMENT;
}
BigComplexStatus bigrationalcomplex_get_real(BigRational *r,const BigRationalComplex *v){return v?mapped(bigrational_copy(r,v->real)):BIGCOMPLEX_NULL_ARGUMENT;}
BigComplexStatus bigrationalcomplex_conjugate(BigRationalComplex *r,const BigRationalComplex *v){
    if(!r || !v)return BIGCOMPLEX_NULL_ARGUMENT;
    BigRationalComplex *t=bigrationalcomplex_create();if(!t)return BIGCOMPLEX_OUT_OF_MEMORY;
    BigRationalStatus s;TRY(bigrational_copy(t->real,v->real));
    TRY(bigrational_negate(t->imaginary,v->imaginary));commit(r,t);
done:bigrationalcomplex_destroy(t);return mapped(s);
}
BigComplexStatus bigrationalcomplex_abs_squared(BigRational *r,const BigRationalComplex *v){
    if(!r || !v)return BIGCOMPLEX_NULL_ARGUMENT;
    BigRational *a=bigrational_create(),*b=bigrational_create();
    BigRationalStatus s=BIGRATIONAL_OUT_OF_MEMORY;if(!a || !b)goto done;
    TRY(bigrational_mul(a,v->real,v->real));TRY(bigrational_mul(b,v->imaginary,v->imaginary));
    TRY(bigrational_add(a,a,b));TRY(bigrational_copy(r,a));
done:bigrational_destroy(a);bigrational_destroy(b);return mapped(s);
}
BigComplexStatus bigrationalcomplex_get_imaginary(BigRational *r,const BigRationalComplex *v){return v?mapped(bigrational_copy(r,v->imaginary)):BIGCOMPLEX_NULL_ARGUMENT;}
static BigComplexStatus binary(BigRationalComplex *r,const BigRationalComplex *a,const BigRationalComplex *b,int op){
    if(!r || !a || !b)return BIGCOMPLEX_NULL_ARGUMENT;
    BigRationalComplex *t=bigrationalcomplex_create();
    BigRational *x=bigrational_create(),*y=bigrational_create(),*d=bigrational_create();
    BigRationalStatus s=BIGRATIONAL_OUT_OF_MEMORY;if(!t || !x || !y || !d)goto done;
    if(op<2){
        TRY(op==0?bigrational_add(t->real,a->real,b->real):bigrational_sub(t->real,a->real,b->real));
        TRY(op==0?bigrational_add(t->imaginary,a->imaginary,b->imaginary):bigrational_sub(t->imaginary,a->imaginary,b->imaginary));
    }else{
        TRY(bigrational_mul(x,a->real,b->real));TRY(bigrational_mul(y,a->imaginary,b->imaginary));
        TRY(op==2?bigrational_sub(t->real,x,y):bigrational_add(t->real,x,y));
        TRY(bigrational_mul(x,a->imaginary,b->real));TRY(bigrational_mul(y,a->real,b->imaginary));
        TRY(op==2?bigrational_add(t->imaginary,x,y):bigrational_sub(t->imaginary,x,y));
        if(op==3){
            TRY(bigrational_mul(x,b->real,b->real));TRY(bigrational_mul(y,b->imaginary,b->imaginary));TRY(bigrational_add(d,x,y));
            TRY(bigrational_div(t->real,t->real,d));TRY(bigrational_div(t->imaginary,t->imaginary,d));
        }
    }commit(r,t);
done:bigrationalcomplex_destroy(t);bigrational_destroy(x);bigrational_destroy(y);bigrational_destroy(d);return mapped(s);
}
BigComplexStatus bigrationalcomplex_add(BigRationalComplex *r,const BigRationalComplex *a,const BigRationalComplex *b){return binary(r,a,b,0);}
BigComplexStatus bigrationalcomplex_sub(BigRationalComplex *r,const BigRationalComplex *a,const BigRationalComplex *b){return binary(r,a,b,1);}
BigComplexStatus bigrationalcomplex_mul(BigRationalComplex *r,const BigRationalComplex *a,const BigRationalComplex *b){return binary(r,a,b,2);}
BigComplexStatus bigrationalcomplex_div(BigRationalComplex *r,const BigRationalComplex *a,const BigRationalComplex *b){return binary(r,a,b,3);}
static BigComplexStatus set_one(BigRationalComplex *v){
    BigInt *n=bigint_create();if(!n)return BIGCOMPLEX_OUT_OF_MEMORY;
    BigIntStatus s=bigint_set_string(n,"1");
    BigComplexStatus status=s==BIGINT_OK?mapped(bigrational_from_bigint(v->real,n)):BIGCOMPLEX_OUT_OF_MEMORY;
    bigint_destroy(n);return status;
}
BigComplexStatus bigrationalcomplex_pow_int(BigRationalComplex *r,const BigRationalComplex *v,int64_t exponent){
    if(!r || !v)return BIGCOMPLEX_NULL_ARGUMENT;
    BigRationalComplex *p=bigrationalcomplex_create(),*b=bigrationalcomplex_create(),*one=NULL;
    BigComplexStatus s=BIGCOMPLEX_OUT_OF_MEMORY;if(!p || !b)goto done;
    s=set_one(p);if(s!=BIGCOMPLEX_OK)goto done;s=bigrationalcomplex_copy(b,v);if(s!=BIGCOMPLEX_OK)goto done;
    uint64_t n=exponent<0?UINT64_C(0)-(uint64_t)exponent:(uint64_t)exponent;
    while(n){if(n&1U){s=bigrationalcomplex_mul(p,p,b);if(s!=BIGCOMPLEX_OK)goto done;}n>>=1;
        if(n){s=bigrationalcomplex_mul(b,b,b);if(s!=BIGCOMPLEX_OK)goto done;}}
    if(exponent<0){one=bigrationalcomplex_create();if(!one){s=BIGCOMPLEX_OUT_OF_MEMORY;goto done;}
        s=set_one(one);if(s!=BIGCOMPLEX_OK)goto done;s=bigrationalcomplex_div(p,one,p);if(s!=BIGCOMPLEX_OK)goto done;}
    commit(r,p);
done:bigrationalcomplex_destroy(p);bigrationalcomplex_destroy(b);bigrationalcomplex_destroy(one);return s;
}
BigComplexStatus bigrationalcomplex_to_bigcomplex(BigComplex *r,const BigRationalComplex *v,int64_t digits,BigDecimalRoundingMode mode){
    if(!r || !v)return BIGCOMPLEX_NULL_ARGUMENT;
    if(digits<1 || mode<BIGDECIMAL_ROUND_TOWARD_ZERO || mode>BIGDECIMAL_ROUND_HALF_EVEN)return BIGCOMPLEX_INVALID_ARGUMENT;
    BigDecimal *re=bigdecimal_create(),*im=bigdecimal_create();BigRationalStatus s=BIGRATIONAL_OUT_OF_MEMORY;
    if(!re || !im)goto done;TRY(bigrational_to_bigdecimal(re,v->real,digits,mode));TRY(bigrational_to_bigdecimal(im,v->imaginary,digits,mode));
    {BigComplexStatus status=bigcomplex_set_parts(r,re,im);bigdecimal_destroy(re);bigdecimal_destroy(im);return status;}
done:bigdecimal_destroy(re);bigdecimal_destroy(im);return mapped(s);
}
BigComplexStatus bigrationalcomplex_to_string(const BigRationalComplex *v,size_t limit,char **result){
    if(!v || !result)return BIGCOMPLEX_NULL_ARGUMENT;
    char *re=NULL,*im=NULL;BigRationalStatus s;
    TRY(bigrational_to_string(v->real,&re));TRY(bigrational_to_string(v->imaginary,&im));
    const char *mag=im+(im[0]=='-');bool unit=strcmp(mag,"1")==0,zero=strcmp(im,"0")==0,rz=strcmp(re,"0")==0;
    bool par=strchr(mag,'/')!=NULL;const char *coef=unit?"":mag;
    size_t a=strlen(re),b=strlen(coef);if(a>SIZE_MAX-10 || b>SIZE_MAX-10-a){s=BIGRATIONAL_VALUE_TOO_LARGE;goto done;}
    size_t term=b+(unit?1U:2U)+(par?2U:0U),len=zero?a:(rz?term+(im[0]=='-'?1U:0U):a+3U+term);
    if(len>limit){s=BIGRATIONAL_VALUE_TOO_LARGE;goto done;}
    char *text=numforge_malloc(len+1);if(!text){s=BIGRATIONAL_OUT_OF_MEMORY;goto done;}
    if(zero)(void)snprintf(text,len+1,"%s",re);
    else if(rz)(void)snprintf(text,len+1,"%s%s%s%s%si",im[0]=='-'?"-":"",par?"(":"",coef,par?")":"",unit?"":"*");
    else (void)snprintf(text,len+1,"%s %s %s%s%s%si",re,im[0]=='-'?"-":"+",par?"(":"",coef,par?")":"",unit?"":"*");
    *result=text;
done:free(re);free(im);return mapped(s);
}
BigComplexStatus bigrationalcomplex_from_bigcomplex(BigRationalComplex *r,const BigComplex *v){
    if(!r || !v)return BIGCOMPLEX_NULL_ARGUMENT;
    BigRationalComplex *t=bigrationalcomplex_create();BigDecimal *re=bigdecimal_create(),*im=bigdecimal_create();
    BigComplexStatus status=BIGCOMPLEX_OUT_OF_MEMORY;if(!t || !re || !im)goto done;
    status=bigcomplex_get_real(re,v);if(status!=BIGCOMPLEX_OK)goto done;
    status=bigcomplex_get_imaginary(im,v);if(status!=BIGCOMPLEX_OK)goto done;
    status=mapped(bigrational_from_bigdecimal(t->real,re));if(status!=BIGCOMPLEX_OK)goto done;
    status=mapped(bigrational_from_bigdecimal(t->imaginary,im));if(status!=BIGCOMPLEX_OK)goto done;
    commit(r,t);
done:bigrationalcomplex_destroy(t);bigdecimal_destroy(re);bigdecimal_destroy(im);return status;
}
BigComplexStatus bigrationalcomplex_format_form(const BigRationalComplex *v,BigComplexForm form,
    int64_t digits,int64_t places,BigDecimalRoundingMode rounding,BigDecimalFormatMode mode,size_t limit,char **result){
    if(!v || !result)return BIGCOMPLEX_NULL_ARGUMENT;
    if(form<BIGCOMPLEX_FORM_CARTESIAN || form>BIGCOMPLEX_FORM_EXPONENTIAL || digits<1 || digits>INT64_MAX-12 || places < -1 ||
        rounding<BIGDECIMAL_ROUND_TOWARD_ZERO || rounding>BIGDECIMAL_ROUND_HALF_EVEN || mode<BIGDECIMAL_FORMAT_AUTO || mode>BIGDECIMAL_FORMAT_MATHEMATICAL)
        return BIGCOMPLEX_INVALID_ARGUMENT;
    if(form==BIGCOMPLEX_FORM_CARTESIAN)return bigrationalcomplex_to_string(v,limit,result);
    BigComplex *decimal=bigcomplex_create();if(!decimal)return BIGCOMPLEX_OUT_OF_MEMORY;
    BigComplexStatus status=bigrationalcomplex_to_bigcomplex(decimal,v,digits+12,BIGDECIMAL_ROUND_HALF_EVEN);
    if(status==BIGCOMPLEX_OK)status=bigcomplex_format_form(decimal,form,digits,places,rounding,mode,limit,result);
    bigcomplex_destroy(decimal);return status;
}
