#include "primitiveq.h"
#include <stdlib.h>
#include <string.h>

static unsigned parity(uint32_t x){return (unsigned)__builtin_parity(x);}
static pq_row multiply(pq_row a,pq_row b){
    pq_row c={a.x^b.x,a.z^b.z,(uint8_t)((a.phase+b.phase+2u*parity(a.z&b.x))&3u)};
    return c;
}
static int valid(const pq_state *s,unsigned q){return s && s->n && s->n<=PQ_MAX_QUBITS && q<s->n;}
int pq_init(pq_state *s,unsigned n){
    if(!s||!n||n>PQ_MAX_QUBITS)return -1;
    memset(s,0,sizeof(*s));s->n=(uint8_t)n;
    for(unsigned i=0;i<n;i++){s->rows[i].x=1u<<i;s->rows[n+i].z=1u<<i;}
    return 0;
}
int pq_h(pq_state *s,unsigned q){
    if(!valid(s,q))return -1;
    uint32_t bit=1u<<q;
    for(unsigned i=0;i<2u*s->n;i++){
        pq_row *r=&s->rows[i];unsigned x=!!(r->x&bit),z=!!(r->z&bit);
        if(x&&z)r->phase=(uint8_t)((r->phase+2u)&3u);
        if(x!=z){r->x^=bit;r->z^=bit;}
    }
    return 0;
}
int pq_s(pq_state *s,unsigned q){
    if(!valid(s,q))return -1;
    uint32_t bit=1u<<q;
    for(unsigned i=0;i<2u*s->n;i++)if(s->rows[i].x&bit){
        s->rows[i].phase=(uint8_t)((s->rows[i].phase+1u)&3u);
        s->rows[i].z^=bit;
    }
    return 0;
}
int pq_cnot(pq_state *s,unsigned control,unsigned target){
    if(!valid(s,control)||!valid(s,target)||control==target)return -1;
    uint32_t c=1u<<control,t=1u<<target;
    for(unsigned i=0;i<2u*s->n;i++){
        pq_row *r=&s->rows[i];
        /* A CNOT conjugates canonical X-before-Z factors without reordering. */
        if(r->x&c)r->x^=t;
        if(r->z&t)r->z^=c;
    }
    return 0;
}
int pq_pauli(pq_state *s,unsigned q,char op){
    if(!valid(s,q)||(op!='X'&&op!='Y'&&op!='Z'))return -1;
    uint32_t b=1u<<q;
    for(unsigned i=0;i<2u*s->n;i++){
        pq_row *r=&s->rows[i];
        unsigned flip=(op=='X'?!!(r->z&b):op=='Z'?!!(r->x&b):!!((r->x^r->z)&b));
        if(flip)r->phase=(uint8_t)((r->phase+2u)&3u);
    }
    return 0;
}
int pq_measure_z(pq_state *s,unsigned q,uint64_t draw,unsigned *result,unsigned *random_branch){
    if(!valid(s,q)||!result||!random_branch)return -1;
    const uint32_t b=1u<<q;unsigned pivot=s->n;
    while(pivot<2u*s->n && !(s->rows[pivot].x&b))pivot++;
    if(pivot<2u*s->n){
        for(unsigned i=0;i<2u*s->n;i++)if(i!=pivot&&(s->rows[i].x&b))
            s->rows[i]=multiply(s->rows[i],s->rows[pivot]);
        s->rows[pivot-s->n]=s->rows[pivot];
        *result=(unsigned)(draw&1u);*random_branch=1;
        s->rows[pivot]=(pq_row){0,b,(uint8_t)(*result?2:0)};
    }else{
        pq_row scratch={0,0,0};
        for(unsigned i=0;i<s->n;i++)if(s->rows[i].x&b)
            scratch=multiply(scratch,s->rows[i+s->n]);
        if(scratch.x||scratch.z!=b||(scratch.phase&1u))return -1;
        *result=(unsigned)(scratch.phase>>1);*random_branch=0;
    }
    return 0;
}
int pq_reset(pq_state *s,unsigned q,uint64_t draw,unsigned *random_branch){
    unsigned r=0;
    if(pq_measure_z(s,q,draw,&r,random_branch))return -1;
    if(r)return pq_pauli(s,q,'X');
    return 0;
}
static const pq_row five_generators[5]={
    {9u,6u,0},{18u,12u,0},{5u,24u,0},{10u,17u,0},{0,31u,0}
};
static unsigned symplectic(pq_row a,pq_row b){
    return parity((a.x&b.z)^(a.z&b.x));
}
int pq_init_five_code(pq_state *s){
    if(!s)return -1;
    pq_state next;
    if(pq_init(&next,5))return -1;
    for(unsigned i=0;i<5;i++){
        next.rows[5+i]=five_generators[i];
        for(unsigned j=0;j<i;j++)if(symplectic(five_generators[i],five_generators[j]))return -1;
    }
    /* Complete the independent stabilizers with dual destabilizers over GF(2).
     * Candidate ordering is fixed so encoded preparation is byte reproducible. */
    for(unsigned i=0;i<5;i++){
        int found=0;
        for(unsigned x=0;x<32&&!found;x++)for(unsigned z=0;z<32&&!found;z++){
            pq_row d={x,z,(uint8_t)(parity(x&z)?1u:0u)};
            int okay=1;
            for(unsigned j=0;j<5;j++)if(symplectic(d,next.rows[5+j])!=(unsigned)(i==j))okay=0;
            for(unsigned j=0;j<i;j++)if(symplectic(d,next.rows[j]))okay=0;
            if(okay){next.rows[i]=d;found=1;}
        }
        if(!found)return -1;
    }
    *s=next;return 0;
}
int pq_five_syndrome(const pq_state *s,unsigned *syndrome){
    if(!s||!syndrome||s->n!=5)return -1;
    unsigned value=0;
    for(unsigned i=0;i<5;i++){
        pq_row r=s->rows[5+i];
        if(r.x!=five_generators[i].x||r.z!=five_generators[i].z||
           (r.phase!=0&&r.phase!=2))return -1;
        if(i<4)value|=(unsigned)(r.phase>>1)<<i;
    }
    *syndrome=value;return 0;
}
int pq_five_correct(pq_state *s,unsigned *syndrome,unsigned *correction){
    if(!s||!syndrome||!correction||pq_five_syndrome(s,syndrome))return -1;
    *correction=0;
    if(!*syndrome)return 0;
    pq_row errors[15];uint32_t values[15];
    for(unsigned q=0;q<5;q++)for(unsigned kind=0;kind<3;kind++){
        unsigned index=3u*q+kind;
        errors[index]=(pq_row){kind!=1?1u<<q:0u,kind!=0?1u<<q:0u,0};
    }
    if(pq_syndrome_scalar(five_generators,4,5,errors,15,values))return -1;
    unsigned matches=0,index=0;
    for(unsigned k=0;k<15;k++)if(values[k]==*syndrome){matches++;index=k;}
    if(matches!=1)return -1;
    static const char operations[]={'X','Z','Y'};
    if(pq_pauli(s,index/3u,operations[index%3u]))return -1;
    unsigned after=0;if(pq_five_syndrome(s,&after)||after)return -1;
    *correction=index+1u;return 0;
}
static int syndrome_args(const pq_row *g,unsigned n_g,unsigned n,const pq_row *e,size_t count,uint32_t *out){
    if(!g||!e||!out||!n||n>PQ_MAX_QUBITS||!n_g||n_g>n||!count)return -1;
    const uint32_t mask=(1u<<n)-1u;
    for(unsigned j=0;j<n_g;j++)if(((g[j].x|g[j].z)&~mask)||g[j].phase>3u)return -1;
    for(size_t i=0;i<count;i++)if(((e[i].x|e[i].z)&~mask)||e[i].phase>3u)return -1;
    return 0;
}
int pq_syndrome_scalar(const pq_row *g,unsigned n_g,unsigned n,const pq_row *e,size_t count,uint32_t *out){
    if(syndrome_args(g,n_g,n,e,count,out))return -1;
    for(size_t i=0;i<count;i++){
        uint32_t v=0;
        for(unsigned j=0;j<n_g;j++)v|=parity((e[i].x&g[j].z)^(e[i].z&g[j].x))<<j;
        out[i]=v;
    }
    return 0;
}
int pq_syndrome_vector(const pq_row *g,unsigned n_g,unsigned n,const pq_row *e,size_t count,uint32_t *out){
    if(syndrome_args(g,n_g,n,e,count,out))return -1;
    for(size_t offset=0;offset<count;offset+=64){
        size_t width=count-offset<64?count-offset:64;
        uint64_t xp[PQ_MAX_QUBITS]={0},zp[PQ_MAX_QUBITS]={0};
        for(size_t k=0;k<width;k++)for(unsigned b=0;b<n;b++){
            xp[b]|=(uint64_t)((e[offset+k].x>>b)&1u)<<k;
            zp[b]|=(uint64_t)((e[offset+k].z>>b)&1u)<<k;
        }
        for(size_t k=0;k<width;k++)out[offset+k]=0;
        for(unsigned j=0;j<n_g;j++){
            uint64_t plane=0;
            for(unsigned b=0;b<n;b++){
                if(g[j].z&(1u<<b))plane^=xp[b];
                if(g[j].x&(1u<<b))plane^=zp[b];
            }
            for(size_t k=0;k<width;k++)out[offset+k]|=(uint32_t)((plane>>k)&1u)<<j;
        }
    }
    return 0;
}
struct pq_batch {
    unsigned n;size_t count,blocks;
    /* For each block, X bit planes followed by Z bit planes. */
    uint64_t planes[];
};
int pq_batch_prepare(const pq_row *e,unsigned n,size_t count,pq_batch **out){
    if(!e||!out||!n||n>PQ_MAX_QUBITS||!count||count>1048576u)return -1;
    uint32_t mask=(1u<<n)-1u;
    for(size_t k=0;k<count;k++)if(((e[k].x|e[k].z)&~mask)||e[k].phase>3u)return -1;
    size_t blocks=(count+63u)/64u;
    pq_batch *b=calloc(1,sizeof(*b)+blocks*2u*n*sizeof(uint64_t));
    if(!b)return -1;
    b->n=n;b->count=count;b->blocks=blocks;
    for(size_t k=0;k<count;k++){
        uint64_t *p=b->planes+(k/64u)*2u*n;
        uint64_t bit=UINT64_C(1)<<(k%64u);
        uint32_t x=e[k].x,z=e[k].z;
        while(x){unsigned q=(unsigned)__builtin_ctz(x);p[q]|=bit;x&=x-1u;}
        while(z){unsigned q=(unsigned)__builtin_ctz(z);p[n+q]|=bit;z&=z-1u;}
    }
    *out=b;return 0;
}
int pq_batch_syndrome(const pq_batch *b,const pq_row *g,unsigned count,uint32_t *out){
    if(!b||!g||!out||!count||count>b->n)return -1;
    uint32_t mask=(1u<<b->n)-1u;
    for(unsigned j=0;j<count;j++)if(((g[j].x|g[j].z)&~mask)||g[j].phase>3u)return -1;
    for(size_t block=0;block<b->blocks;block++){
        const uint64_t *p=b->planes+block*2u*b->n;
        size_t width=b->count-block*64u<64u?b->count-block*64u:64u;
        uint64_t syndrome[PQ_MAX_QUBITS];
        for(unsigned j=0;j<count;j++){
            uint64_t plane=0;uint32_t x=g[j].x,z=g[j].z;
            while(x){unsigned q=(unsigned)__builtin_ctz(x);plane^=p[b->n+q];x&=x-1u;}
            while(z){unsigned q=(unsigned)__builtin_ctz(z);plane^=p[q];z&=z-1u;}
            syndrome[j]=plane;
        }
        for(size_t k=0;k<width;k++){
            uint32_t value=0;for(unsigned j=0;j<count;j++)value|=(uint32_t)((syndrome[j]>>k)&1u)<<j;
            out[block*64u+k]=value;
        }
    }
    return 0;
}
void pq_batch_free(pq_batch *b){free(b);}
