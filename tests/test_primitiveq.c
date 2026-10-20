#include "primitiveq.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

int main(void){
    pq_state q,a,b;
    assert(!pq_init(&q,2));
    assert(!pq_h(&q,0));assert(!pq_cnot(&q,0,1));
    a=q;b=q;
    unsigned first,second,r;
    assert(!pq_measure_z(&a,0,0,&first,&r)&&first==0&&r==1);
    assert(!pq_measure_z(&a,1,1,&second,&r)&&second==0&&r==0);
    assert(!pq_measure_z(&b,0,1,&first,&r)&&first==1&&r==1);
    assert(!pq_measure_z(&b,1,0,&second,&r)&&second==1&&r==0);
    assert(!pq_reset(&b,0,0,&r));
    assert(!pq_measure_z(&b,0,0,&first,&r)&&first==0&&r==0);
    assert(!pq_init(&q,1));assert(!pq_h(&q,0));assert(!pq_s(&q,0));
    assert(!pq_s(&q,0));assert(!pq_h(&q,0));
    assert(!pq_measure_z(&q,0,0,&first,&r)&&first==1&&r==0);
    assert(pq_cnot(&q,0,0));assert(pq_h(&q,4));
    const pq_row g[4]={{1,18,1},{2,5,1},{4,10,1},{8,20,1}};
    pq_row e[257];uint32_t x[257],y[257];
    uint32_t seed=UINT32_C(0x5195f0e7);
    for(unsigned k=0;k<257;k++){
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        e[k]=(pq_row){seed&31u,(seed>>5)&31u,0};
    }
    assert(!pq_syndrome_scalar(g,4,5,e,257,x));
    assert(!pq_syndrome_vector(g,4,5,e,257,y));
    assert(!memcmp(x,y,sizeof(x)));
    pq_batch *batch=NULL;
    assert(!pq_batch_prepare(e,5,257,&batch));
    memset(y,0,sizeof(y));
    assert(!pq_batch_syndrome(batch,g,4,y));
    assert(!memcmp(x,y,sizeof(x)));
    pq_batch_free(batch);
    /* XZZXI, IXZZX, XIXZZ, ZXIXZ: the [[5,1,3]] generators.
       Every single-qubit X/Y/Z error has a unique nonzero syndrome. */
    const pq_row five[4]={{9,6,0},{18,12,0},{5,24,0},{10,17,0}};
    for(unsigned i=0;i<4;i++)for(unsigned j=0;j<i;j++)
        assert(!(__builtin_parity((five[i].x&five[j].z)^(five[i].z&five[j].x))));
    pq_row faults[15];uint32_t syndromes[15];
    for(unsigned qubit=0;qubit<5;qubit++)for(unsigned kind=0;kind<3;kind++)
        faults[3*qubit+kind]=(pq_row){kind!=1?1u<<qubit:0u,
                                      kind!=0?1u<<qubit:0u,0};
    assert(!pq_syndrome_scalar(five,4,5,faults,15,syndromes));
    unsigned seen=0;
    for(unsigned i=0;i<15;i++){assert(syndromes[i]>0&&syndromes[i]<16);
        assert(!(seen&(1u<<syndromes[i])));seen|=1u<<syndromes[i];}
    assert(seen==0xfffeu);
    /* Encoded |0_L>, all 15 possible single physical Pauli faults and recovery. */
    assert(!pq_init_five_code(&q));
    unsigned syndrome=99,correction=99;
    assert(!pq_five_syndrome(&q,&syndrome)&&syndrome==0);
    assert(!pq_five_syndrome_circuit(&q,&syndrome)&&syndrome==0);
    unsigned logical_bit=99;
    assert(!pq_five_logical_z_circuit(&q,&logical_bit)&&logical_bit==0);
    for(unsigned round=0;round<3;round++)for(unsigned gen=0;gen<4;gen++)
        assert(!pq_five_syndrome_circuit_readout(&q,round,gen,&syndrome)&&syndrome==0);
    assert(pq_five_syndrome_circuit_readout(&q,3,0,&syndrome));
    assert(pq_five_syndrome_circuit_readout(&q,0,4,&syndrome));
    assert(pq_five_syndrome_circuit_readout(&q,UINT_MAX,0,NULL));
    assert(!pq_five_correct(&q,&syndrome,&correction)&&!correction);
    for(unsigned qubit=0;qubit<5;qubit++)for(unsigned kind=0;kind<3;kind++){
        pq_state fault=q;
        const char op[3]={'X','Z','Y'};
        assert(!pq_pauli(&fault,qubit,op[kind]));
        assert(!pq_five_syndrome(&fault,&syndrome));
        assert(syndrome==syndromes[3*qubit+kind]);
        assert(!pq_five_syndrome_circuit(&fault,&syndrome));
        assert(syndrome==syndromes[3*qubit+kind]);
        assert(!pq_five_logical_z_circuit(&fault,&logical_bit));
        assert(logical_bit==(unsigned)(kind!=1));
        for(unsigned round=0;round<3;round++)for(unsigned gen=0;gen<4;gen++){
            unsigned measured=0;
            assert(!pq_five_syndrome_circuit_readout(&fault,round,gen,&measured));
            assert(measured==syndrome);
            pq_state recovered=fault;
            unsigned sign=0,code=0;
            assert(!pq_five_correct_readout(&recovered,round,gen,&sign,&code));
            assert(sign==syndrome&&code==3*qubit+kind+1);
            assert(!memcmp(&recovered,&q,sizeof(q)));
        }
        assert(!pq_five_correct(&fault,&syndrome,&correction));
        assert(correction==3*qubit+kind+1);
        assert(!pq_five_syndrome(&fault,&syndrome)&&!syndrome);
        assert(!memcmp(&fault,&q,sizeof(q)));
    }
    /* The same decoder also preserves the negative logical-Z codeword. */
    for(unsigned physical=0;physical<5;physical++)assert(!pq_pauli(&q,physical,'X'));
    assert(q.rows[9].phase==2);
    assert(!pq_five_logical_z_circuit(&q,&logical_bit)&&logical_bit==1);
    for(unsigned qubit=0;qubit<5;qubit++)for(unsigned kind=0;kind<3;kind++){
        pq_state fault=q;
        const char op[3]={'X','Z','Y'};
        assert(!pq_pauli(&fault,qubit,op[kind]));
        assert(!pq_five_correct(&fault,&syndrome,&correction));
        assert(correction==3*qubit+kind+1);
        assert(!memcmp(&fault,&q,sizeof(q)));
    }
    assert(pq_init_five_code(NULL));
    assert(pq_five_syndrome(&a,&syndrome));
    return 0;
}
