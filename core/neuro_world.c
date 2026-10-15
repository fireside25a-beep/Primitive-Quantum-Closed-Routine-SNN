#include "neuro_world.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

nm_world *nm_world_new(unsigned qubits,uint64_t clock_ns){
    if(!clock_ns||clock_ns>UINT64_C(1000000000))return NULL;
    nm_world *w=calloc(1,sizeof(*w));if(!w)return NULL;
    w->network=calloc(1,sizeof(*w->network));
    if(!w->network||pq_init(&w->quantum,qubits)){
        nm_world_free(w);return NULL;
    }
    w->clock_ns=clock_ns;return w;
}
void nm_world_free(nm_world *w){if(w){free(w->network);free(w);}}
int nm_world_enable_five_code(nm_world *w){
    if(!w||w->quantum.n!=5||w->rule_count||w->five_code)return -1;
    if(pq_init_five_code(&w->quantum))return -1;
    w->five_code=1;w->five_faults=0;return 0;
}
int nm_world_add_node(nm_world *w,uint64_t mass,uint16_t sensor,uint64_t threshold,int64_t current){
    if(!w||w->node_count>=NM_WORLD_NODES||mass>NM_WORLD_MASS_MAX||
       sensor>=NM_MAX_NEURONS||!w->network->neurons[sensor].configured||
       threshold>NM_WORLD_MASS_MAX||current<0)return -1;
    w->nodes[w->node_count++]=(nm_world_node){mass,sensor,threshold,current};return 0;
}
int nm_world_add_link(nm_world *w,nm_world_link e){
    if(!w||w->link_count>=NM_WORLD_LINKS||e.from>=w->node_count||e.to>=w->node_count||
       e.from==e.to||e.rate_q16>65536u||e.bonus_q16>65536u||
       e.cap>NM_WORLD_MASS_MAX||e.context_node>=w->node_count||
       e.context_threshold>NM_WORLD_MASS_MAX||
       (e.gate_neuron!=UINT16_MAX &&
         (e.gate_neuron>=NM_MAX_NEURONS||!w->network->neurons[e.gate_neuron].configured)))return -1;
    w->links[w->link_count++]=e;return 0;
}
int nm_world_add_rule(nm_world *w,nm_world_rule r){
    if(!w||w->rule_count>=NM_WORLD_RULES||r.trigger>=NM_MAX_NEURONS||
       !w->network->neurons[r.trigger].configured||r.a>=w->quantum.n||
       (w->five_code ?
           !(r.op=='X'||r.op=='Y'||r.op=='Z'||r.op=='Q') : r.op=='Q')||
       (r.op=='Q'&&(r.a||r.b||r.output_current<=0))||
       (r.op=='C'&&(r.b>=w->quantum.n||r.b==r.a))||
       !(r.op=='H'||r.op=='S'||r.op=='C'||r.op=='X'||r.op=='Y'||
         r.op=='Z'||r.op=='M'||r.op=='R'||r.op=='Q')||
       (r.op=='R' && r.output_current!=0) ||
       ((r.op=='M'||r.op=='R'||r.op=='Q') &&
        (r.output_neuron>=NM_MAX_NEURONS||
         !w->network->neurons[r.output_neuron].configured||r.output_current<0)))return -1;
    w->rules[w->rule_count++]=r;return 0;
}
static int has_spike(const nm_spikes *spikes,uint16_t id){
    for(size_t k=0;k<spikes->count;k++)if(spikes->ids[k]==id)return 1;
    return 0;
}
int nm_world_step(nm_world *w,const nm_world_input *input,nm_world_report *output){
    if(!w||!input||!output||!w->network||w->network->tick==UINT64_MAX)return -1;
    nm_network *next=malloc(sizeof(*next));if(!next)return -1;
    memcpy(next,w->network,sizeof(*next));
    pq_state quantum=w->quantum;
    uint8_t five_faults=w->five_faults;
    uint64_t mass[NM_WORLD_NODES],remaining[NM_WORLD_NODES];
    nm_world_report result={0};
    for(unsigned i=0;i<w->node_count;i++){
        mass[i]=remaining[i]=w->nodes[i].mass;
        if(__builtin_add_overflow(result.mass_before,mass[i],&result.mass_before))goto fail;
        if(w->nodes[i].sensor_current&&mass[i]>=w->nodes[i].sensor_threshold &&
           nm_inject(next,w->nodes[i].sensor_neuron,next->tick,w->nodes[i].sensor_current))goto fail;
    }
    if(input->stimulus_current &&
       nm_inject(next,input->stimulus_neuron,next->tick,input->stimulus_current))goto fail;
    if(nm_step(next,&result.spikes))goto fail;
    for(unsigned i=0;i<w->rule_count;i++)if(input->readout_flip[i] &&
        (w->rules[i].op!='Q'||input->readout_flip[i]>12u||
         !has_spike(&result.spikes,w->rules[i].trigger)))goto fail;
    for(unsigned i=0;i<w->link_count;i++){
        nm_world_link e=w->links[i];
        uint64_t old_from=w->nodes[e.from].mass,old_to=w->nodes[e.to].mass;
        if(old_from<=old_to)continue;
        uint32_t rate=e.rate_q16;
        if(e.gate_neuron!=UINT16_MAX && has_spike(&result.spikes,e.gate_neuron) &&
           w->nodes[e.context_node].mass>=e.context_threshold){
            uint64_t sum=(uint64_t)rate+e.bonus_q16;
            rate=(uint32_t)(sum>65536u?65536u:sum);
        }
        uint64_t flux=((old_from-old_to)*rate)/65536u;
        if(flux>e.cap)flux=e.cap;
        if(flux>remaining[e.from])flux=remaining[e.from];
        remaining[e.from]-=flux;
        if(mass[e.from]<flux||mass[e.to]>NM_WORLD_MASS_MAX-flux||
           __builtin_add_overflow(result.flux_total,flux,&result.flux_total))goto fail;
        mass[e.from]-=flux;mass[e.to]+=flux;
    }
    for(unsigned i=0;i<w->node_count;i++)
        if(__builtin_add_overflow(result.mass_after,mass[i],&result.mass_after))goto fail;
    if(result.mass_after!=result.mass_before)goto fail;
    for(unsigned i=0;i<w->rule_count;i++){
        nm_world_rule r=w->rules[i];if(!has_spike(&result.spikes,r.trigger))continue;
        result.fired_rules|=(uint32_t)1<<i;
        int rc=0;unsigned bit=0,random=0;
        switch(r.op){
          case 'H':rc=pq_h(&quantum,r.a);break;
          case 'S':rc=pq_s(&quantum,r.a);break;
          case 'C':rc=pq_cnot(&quantum,r.a,r.b);break;
          case 'X':case 'Y':case 'Z':
            if(w->five_code&&five_faults>=1)goto fail;
            rc=pq_pauli(&quantum,r.a,r.op);
            if(!rc&&w->five_code)five_faults++;
            break;
          case 'Q':{
            unsigned syndrome=0,correction=0;
            unsigned flip=input->readout_flip[i];
            rc=flip?pq_five_correct_readout(&quantum,(flip-1u)/4u,(flip-1u)%4u,
                                            &syndrome,&correction):
                    pq_five_correct(&quantum,&syndrome,&correction);
            if(rc)goto fail;
            result.syndromes[i]=(uint8_t)syndrome;
            result.corrections[i]=(uint8_t)correction;
            result.measurement_rules|=(uint32_t)1<<i;
            if(correction)result.corrected_rules|=(uint32_t)1<<i;
            if(syndrome){
                result.measurement_ones|=(uint32_t)1<<i;
                if(nm_inject(next,r.output_neuron,next->tick,r.output_current))goto fail;
            }
            five_faults=0;
            break;
          }
          case 'M':rc=pq_measure_z(&quantum,r.a,input->draws[i],&bit,&random);break;
          case 'R':rc=pq_reset(&quantum,r.a,input->draws[i],&random);break;
          default:rc=-1;
        }
        if(rc)goto fail;
        if(r.op=='M'||r.op=='R'){
            result.measurement_rules|=(uint32_t)1<<i;
            if(random)result.random_rules|=(uint32_t)1<<i;
            if(bit){
                result.measurement_ones|=(uint32_t)1<<i;
                if(nm_inject(next,r.output_neuron,next->tick,r.output_current))goto fail;
            }
        }
    }
    memcpy(w->network,next,sizeof(*next));w->quantum=quantum;w->five_faults=five_faults;
    for(unsigned i=0;i<w->node_count;i++)w->nodes[i].mass=mass[i];
    *output=result;free(next);return 0;
fail:
    free(next);return -1;
}
