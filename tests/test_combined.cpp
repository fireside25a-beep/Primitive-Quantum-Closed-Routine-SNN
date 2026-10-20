#include "qmemory.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

using World=std::unique_ptr<nm_world,decltype(&nm_world_free)>;
static World setup(){
    World w(nm_world_new(5,100000),nm_world_free);assert(w);
    assert(!nm_add_neuron(w->network,0,0,10,65536,0));
    assert(!nm_add_neuron(w->network,1,0,10,65536,0));
    assert(!nm_add_edge(w->network,0,1,2,1,4,2,8,-100,100));
    assert(!nm_world_enable_five_code(w.get()));
    assert(!nm_world_add_node(w.get(),80,0,100,0));
    assert(!nm_world_add_node(w.get(),20,1,100,0));
    assert(!nm_world_add_link(w.get(),{0,1,0,1,32768,0,100,100}));
    assert(!nm_world_add_rule(w.get(),{0,0,0,0,0,'X'}));
    assert(!nm_world_add_rule(w.get(),{0,0,0,1,10,'Q'}));
    return w;
}
static void equal(const nm_world &a,const nm_world &b){
    assert(!std::memcmp(a.network,b.network,sizeof(nm_network)));
    auto x=a,y=b;x.network=nullptr;y.network=nullptr;
    assert(!std::memcmp(&x,&y,sizeof(x)));
}
static void refuses(World &w,nm_qmemory::Weights &memory,const nm_world_input &input,
                    std::span<const nm_qmemory::Fault> faults){
    auto net=std::make_unique<nm_network>(*w->network);
    auto before=*w;before.network=net.get();auto root=memory.root();
    try {memory.step(*w,input,faults);assert(false);}catch(const std::runtime_error &){}
    equal(*w,before);assert(memory.root()==root);
}
int main(){
    nm_world_input input{};input.stimulus_neuron=0;input.stimulus_current=10;
    unsigned cases=0;
    for(unsigned bit=0;bit<32;bit++)for(unsigned q=0;q<5;q++)for(char op:{'X','Y','Z'}){
        auto w=setup(),reference=setup();nm_qmemory::Weights memory(*w->network);
        auto original=memory.root();w->network->edges[0].weight=777;
        nm_qmemory::Fault fault{0,0,bit,q,op};
        auto tick=memory.step(*w,input,{&fault,1});
        nm_world_report expected{};assert(!nm_world_step(reference.get(),&input,&expected));
        equal(*w,*reference);assert(w->network->tick==1);
        assert(tick.memory_corrected==1&&tick.world.corrected_rules==2);
        assert(tick.world.syndromes[1]==8&&tick.memory_before==original&&tick.memory_after==original);
        assert(w->network->pending[1][1]==12);
        nm_world_input next{};auto second=memory.step(*w,next);
        assert(!nm_world_step(reference.get(),&next,&expected));equal(*w,*reference);
        assert(w->network->tick==2&&second.world.spikes.count==1&&second.world.spikes.ids[0]==1);
        // The established STDP law is 2 + 4*(8-1+1)/8 = 6.
        assert(w->network->edges[0].weight==6&&second.memory_after!=original);
        w->network->edges[0].weight=-777;
        nm_qmemory::Fault learned{2,0,bit,q,op};
        auto third=memory.step(*w,next,{&learned,1});
        assert(!nm_world_step(reference.get(),&next,&expected));equal(*w,*reference);
        assert(third.memory_corrected==1&&w->network->edges[0].weight==6);
        cases++;
    }
    for(unsigned round=0;round<3;round++)for(unsigned generator=0;generator<4;generator++){
        auto w=setup();nm_qmemory::Weights memory(*w->network);
        auto flipped=input;flipped.readout_flip[1]=1+4*round+generator;
        nm_qmemory::Fault fault{0,0,31,4,'Y'};
        auto tick=memory.step(*w,flipped,{&fault,1});
        assert(tick.memory_corrected==1&&tick.world.syndromes[1]==8&&w->network->tick==1);
    }
    auto w=setup();nm_qmemory::Weights memory(*w->network);
    nm_qmemory::Fault one{0,0,0,0,'X'};
    // Refusal after the neuron step and correction must undo all four layers.
    w->network->edges[0].weight=777;
    w->network->pending[1][1]=std::numeric_limits<std::int64_t>::max()-2;
    refuses(w,memory,input,{&one,1});
    w->network->pending[1][1]=0;
    assert(memory.step(*w,input,{&one,1}).memory_corrected==1);
    w=setup();nm_qmemory::Weights fresh(*w->network);
    nm_qmemory::Fault duplicate[2]={one,{0,0,0,1,'Z'}};
    refuses(w,fresh,input,duplicate);
    nm_qmemory::Fault wrong{1,0,0,0,'X'};refuses(w,fresh,input,{&wrong,1});
    w->rules[1]={0,1,0,0,0,'Z'};
    refuses(w,fresh,input,{&one,1});
    w->rules[1]={0,0,0,1,10,'Q'};
    assert(fresh.step(*w,input,{&one,1}).world.corrected_rules==2);
    w=setup();nm_qmemory::Weights pair(*w->network);
    nm_qmemory::Fault separate[2]={one,{0,0,31,4,'Y'}};
    auto two=pair.step(*w,input,separate);
    assert(two.memory_corrected==2&&two.world.corrected_rules==2&&w->network->tick==1);
    std::cout<<"combined transaction: "<<cases<<" memory faults, next-tick feedback, STDP recapture, 12 readout flips and cross-layer rollback: PASS\n";
}
