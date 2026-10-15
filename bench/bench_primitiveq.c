#define _POSIX_C_SOURCE 200809L
#include "primitiveq.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
int main(void){
    enum {N=65536,G=15,Q=16,REPEAT=40};
    pq_row *e=calloc(N,sizeof(*e));uint32_t *a=calloc(N,sizeof(*a)),*b=calloc(N,sizeof(*b));
    if(!e||!a||!b)return 1;
    pq_row g[G];uint32_t seed=0x198755ab;
    /* Dense upper-triangular independent Z generators commute by construction. */
    for(unsigned j=0;j<G;j++){
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        g[j]=(pq_row){0,(1u<<j)|(seed&(~((1u<<(j+1u))-1u))&65535u),0};
    }
    for(unsigned j=0;j<N;j++){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;e[j]=(pq_row){seed&65535u,(seed>>16)&65535u,0};}
    uint64_t start=ns();for(int i=0;i<REPEAT;i++)if(pq_syndrome_scalar(g,G,Q,e,N,a))return 2;
    uint64_t scalar=ns()-start;
    start=ns();for(int i=0;i<REPEAT;i++)if(pq_syndrome_vector(g,G,Q,e,N,b))return 2;
    uint64_t vector=ns()-start;
    start=ns();pq_batch *prepared=NULL;if(pq_batch_prepare(e,Q,N,&prepared))return 2;
    uint64_t prepare=ns()-start;
    start=ns();for(int i=0;i<REPEAT;i++)if(pq_batch_syndrome(prepared,g,G,b))return 2;
    uint64_t batched=ns()-start;
    uint64_t checksum=0;for(unsigned i=0;i<N;i++){if(a[i]!=b[i])return 3;checksum+=b[i];}
    printf("errors=%u generators=%u qubits=%u repetitions=%u scalar_ns=%llu bitplane_ns=%llu prepare_ns=%llu prepared_ns=%llu checksum=%llu\n",
       N,G,Q,REPEAT,(unsigned long long)scalar,(unsigned long long)vector,
       (unsigned long long)prepare,(unsigned long long)batched,(unsigned long long)checksum);
    pq_batch_free(prepared);
    free(e);free(a);free(b);return 0;
}
