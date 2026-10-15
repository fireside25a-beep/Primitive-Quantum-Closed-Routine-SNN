#include "qmemory_readout.h"
#include <limits.h>

/* C11 backend for hosts without a Fortran compiler. It implements the same
 * public ABI and ordered [[5,1,3]] lookup as kernels/qmemory_decode.f90. */
void nm_qmemory_decode_observed(const int8_t *syndromes,const int8_t *raw_logical,
                                size_t count,int8_t *logical,int8_t *corrections,
                                int32_t *weights,int *status){
    static const int8_t code[16]={0,4,14,7,8,2,10,9,1,11,5,6,13,3,15,12};
    if(!status)return;
    *status=1;
    if(!syndromes||!raw_logical||!logical||!corrections||!weights||
       !count||count>SIZE_MAX/32u)return;
    for(size_t i=0;i<count*32u;i++)if(syndromes[i]<0||syndromes[i]>15||
        raw_logical[i]<0||raw_logical[i]>1)return;
    for(size_t edge=0;edge<count;edge++){
        uint32_t word=0;
        for(unsigned bit=0;bit<32;bit++){
            size_t i=edge*32u+bit;
            int8_t correction=code[(unsigned)syndromes[i]];
            int8_t flip=(int8_t)(correction && ((correction-1)%3)!=1);
            int8_t value=(int8_t)(raw_logical[i]^flip);
            logical[i]=value;
            corrections[i]=correction;
            word|=(uint32_t)value<<bit;
        }
        int64_t signed_word=word<=INT32_MAX?(int64_t)word:(int64_t)word-INT64_C(4294967296);
        weights[edge]=(int32_t)signed_word;
    }
    *status=0;
}
void nm_qmemory_decode(const int8_t *bits,size_t count,int32_t *weights,int *status){
    if(!status)return;
    *status=1;
    if(!bits||!weights||!count||count>SIZE_MAX/32u)return;
    for(size_t i=0;i<count*32u;i++)if(bits[i]!=0&&bits[i]!=1)return;
    for(size_t edge=0;edge<count;edge++){
        uint32_t word=0;
        for(unsigned bit=0;bit<32;bit++)word|=(uint32_t)bits[edge*32u+bit]<<bit;
        int64_t signed_word=word<=INT32_MAX?(int64_t)word:(int64_t)word-INT64_C(4294967296);
        weights[edge]=(int32_t)signed_word;
    }
    *status=0;
}
