#include "qmemory.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

int main(){
    auto network=std::make_unique<nm_network>();
    nm_init(network.get());
    assert(!nm_add_neuron(network.get(),0,0,10,65536,0));
    assert(!nm_add_neuron(network.get(),1,0,10,65536,0));
    assert(!nm_add_edge(network.get(),0,1,-9,1,3,2,4,-100,100));
    nm_qmemory::Weights memory(*network);
    auto initial=memory.root();
    for(unsigned bit=0;bit<32;bit++)for(unsigned qubit=0;qubit<5;qubit++)
        for(char op:std::string("XYZ")){
            memory.fault(0,bit,qubit,op);
            network->edges[0].weight=777;
            assert(memory.restore(*network)==1);
            assert(network->edges[0].weight==-9);
            assert(memory.root()==initial);
        }
    memory.fault(0,0,0,'X');
    try {memory.fault(0,0,1,'Z');assert(false);}catch(const std::runtime_error &){}
    network->edges[0].weight=17;
    assert(memory.restore(*network)==1 && network->edges[0].weight==-9);
    network->edges[0].weight=13;
    memory.capture(*network);
    assert(memory.root()!=initial);
    network->edges[0].weight=777;
    assert(memory.restore(*network)==0 && network->edges[0].weight==13);
    return 0;
}
