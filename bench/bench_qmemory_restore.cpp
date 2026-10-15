#include "qmemory.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

int main(){
    for(unsigned count:{64u,256u,1024u}){
        auto net=std::make_unique<nm_network>();nm_init(net.get());
        if(nm_add_neuron(net.get(),0,0,10,65536,0)||
           nm_add_neuron(net.get(),1,0,10,65536,0))return 1;
        for(unsigned i=0;i<count;i++)if(nm_add_edge(net.get(),0,1,
            static_cast<std::int32_t>(i*341u)-157923,1,3,2,4,-200000,200000))return 2;
        nm_qmemory::Weights memory(*net);
        std::vector<std::int32_t> reference;
        for(unsigned i=0;i<count;i++)reference.push_back(net->edges[i].weight);
        for(int i=0;i<3;i++)if(memory.restore(*net))return 3;
        for(unsigned sample=0;sample<5;sample++){
            auto begin=std::chrono::steady_clock::now();
            for(int repeat=0;repeat<10;repeat++){
                if(memory.restore(*net))return 4;
                for(unsigned i=0;i<count;i++)if(net->edges[i].weight!=reference[i])return 5;
            }
            auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now()-begin).count();
            std::printf("%u,%u,%lld\n",count,sample,static_cast<long long>(ns));
        }
    }
}
