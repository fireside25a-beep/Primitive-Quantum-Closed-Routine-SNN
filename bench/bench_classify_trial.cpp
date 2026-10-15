#include "primitiveq.h"
#include "qmemory_classify_ispc.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

int main(){
    static_assert(sizeof(pq_row)==12);
    pq_state zero{};
    if(pq_init_five_code(&zero))return 1;
    pq_state one=zero;
    for(unsigned q=0;q<5;q++)if(pq_pauli(&one,q,'X'))return 2;
    for(int count:{4096,32768,131072}){
        std::vector<pq_row> cells((size_t)count*10);
        std::vector<std::int8_t> scalar((size_t)count),vector((size_t)count);
        for(int i=0;i<count;i++)std::memcpy(&cells[(size_t)i*10],(i&1)?one.rows:zero.rows,120);
        auto scalar_run=[&](){for(int i=0;i<count;i++){
            const auto *cell=&cells[(size_t)i*10];
            scalar[(size_t)i]=std::memcmp(cell,zero.rows,120)==0?0:
                std::memcmp(cell,one.rows,120)==0?1:2;
        }};
        auto ispc_run=[&](){ispc::nm_qmemory_classify(
            reinterpret_cast<const std::uint32_t*>(cells.data()),count,
            reinterpret_cast<const std::uint32_t*>(zero.rows),
            reinterpret_cast<const std::uint32_t*>(one.rows),vector.data());};
        scalar_run();ispc_run();if(scalar!=vector)return 3;
        for(int sample=0;sample<5;sample++){
            auto timed=[&](auto work){auto t=std::chrono::steady_clock::now();
                for(int r=0;r<5;r++)work();
                return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();};
            auto a=sample&1?timed(scalar_run):timed(ispc_run);
            auto b=sample&1?timed(ispc_run):timed(scalar_run);
            if(scalar!=vector)return 4;
            std::printf("%d,%d,%lld,%lld\n",count,sample,(long long)(sample&1?a:b),(long long)(sample&1?b:a));
        }
    }
}
