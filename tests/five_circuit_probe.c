#include "primitiveq.h"
#include <stdio.h>

int main(void){
    const char op[3]={'X','Y','Z'};
    for(unsigned logical=0;logical<2;logical++){
        pq_state basis;
        if(pq_init_five_code(&basis))return 1;
        if(logical)for(unsigned q=0;q<5;q++)if(pq_pauli(&basis,q,'X'))return 1;
        for(unsigned fault=0;fault<=15;fault++){
            pq_state state=basis;
            if(fault){unsigned q=(fault-1)/3u,k=(fault-1)%3u;
                if(pq_pauli(&state,q,op[k]))return 1;}
            unsigned syndrome=0,readout=0;
            if(pq_five_syndrome_circuit(&state,&syndrome)||
               pq_five_logical_z_circuit(&state,&readout))return 1;
            if(printf("%u %u %u %u\n",logical,fault,syndrome,readout)<0)return 1;
        }
    }
    return 0;
}
