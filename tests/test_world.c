#include "neuro_world.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    nm_world *w=nm_world_new(2,100000);
    assert(w);
    assert(!nm_add_neuron(w->network,0,0,10,0,0));
    assert(!nm_add_neuron(w->network,1,0,10,0,0));
    assert(!nm_world_add_node(w,80,1,80,10));
    assert(!nm_world_add_node(w,20,1,100,0));
    nm_world_link link={0,1,0,1,32768,32768,100,20};
    assert(!nm_world_add_link(w,link));
    assert(!nm_world_add_rule(w,(nm_world_rule){0,0,0,0,0,'H'}));
    assert(!nm_world_add_rule(w,(nm_world_rule){0,0,1,0,0,'C'}));
    assert(!nm_world_add_rule(w,(nm_world_rule){0,0,0,1,10,'M'}));
    assert(nm_world_add_rule(w,(nm_world_rule){0,0,0,1,10,'R'}));
    nm_world_input input={0};input.stimulus_neuron=0;input.stimulus_current=10;
    input.draws[2]=1;
    nm_world_report r;
    assert(!nm_world_step(w,&input,&r));
    assert(r.spikes.count==2&&r.mass_before==100&&r.mass_after==100);
    assert(r.flux_total==60&&w->nodes[0].mass==20&&w->nodes[1].mass==80);
    assert(r.measurement_rules==(1u<<2)&&r.measurement_ones==(1u<<2));
    assert(w->network->pending[1][1]==10);
    pq_state beforeq=w->quantum;
    uint64_t before_tick=w->network->tick;
    uint64_t before_mass=w->nodes[0].mass;
    /* An impossible injection must refuse before any of the three systems commits. */
    nm_world_input bad={0};bad.stimulus_neuron=1;bad.stimulus_current=INT64_MAX;
    assert(nm_world_step(w,&bad,&r));
    assert(w->network->tick==before_tick&&w->nodes[0].mass==before_mass);
    assert(!memcmp(&w->quantum,&beforeq,sizeof(beforeq)));
    nm_world_free(w);
    w=nm_world_new(5,100000);assert(w);
    assert(!nm_add_neuron(w->network,0,0,10,0,0));
    assert(!nm_add_neuron(w->network,1,0,10,0,0));
    assert(!nm_world_enable_five_code(w));
    assert(nm_world_add_rule(w,(nm_world_rule){0,0,0,1,10,'M'}));
    assert(!nm_world_add_rule(w,(nm_world_rule){0,0,0,0,0,'X'}));
    assert(!nm_world_add_rule(w,(nm_world_rule){0,0,0,1,10,'Q'}));
    nm_world_input one={0};one.stimulus_neuron=0;one.stimulus_current=10;
    assert(!nm_world_step(w,&one,&r));
    assert(r.syndromes[1]==8&&r.corrections[1]==1&&r.corrected_rules==2&&
           r.measurement_ones==2&&w->five_faults==0);
    unsigned first=99;assert(!pq_five_syndrome(&w->quantum,&first)&&first==0);
    nm_world_input zero={0};assert(!nm_world_step(w,&zero,&r));
    assert(r.spikes.count==1&&r.spikes.ids[0]==1);
    nm_world_free(w);
    w=nm_world_new(5,100000);assert(w);
    assert(!nm_add_neuron(w->network,0,0,10,0,0));
    assert(!nm_world_enable_five_code(w));
    assert(!nm_world_add_rule(w,(nm_world_rule){0,0,0,0,0,'X'}));
    assert(!nm_world_add_rule(w,(nm_world_rule){0,1,0,0,0,'Z'}));
    nm_network *original=malloc(sizeof(*original));assert(original);
    memcpy(original,w->network,sizeof(*original));pq_state original_q=w->quantum;
    one.stimulus_neuron=0;one.stimulus_current=10;
    assert(nm_world_step(w,&one,&r));
    assert(!memcmp(w->network,original,sizeof(*original))&&
           !memcmp(&w->quantum,&original_q,sizeof(original_q)));
    free(original);
    nm_world_free(w);
    return 0;
}
