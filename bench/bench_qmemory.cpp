#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" void nm_qmemory_decode(const std::int8_t *,std::size_t,std::int32_t *,int *);

static bool scalar(const std::int8_t *bits,std::size_t count,std::int32_t *out){
    for(std::size_t edge=0;edge<count;edge++){
        std::uint32_t word=0;
        for(unsigned bit=0;bit<32;bit++){
            auto digit=bits[edge*32+bit];
            if(digit!=0&&digit!=1)return false;
            word|=std::uint32_t(digit)<<bit;
        }
        out[edge]=static_cast<std::int32_t>(word);
    }
    return true;
}
int main(){
    for(std::size_t count:{64u,256u,1024u}){
        std::vector<std::int8_t> bits(count*32);
        std::vector<std::int32_t> native(count),reference(count);
        std::uint32_t seed=0x9e3779b9u;
        for(auto &bit:bits){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;bit=seed&1u;}
        if(!scalar(bits.data(),count,reference.data()))return 4;
        int status=0;
        nm_qmemory_decode(bits.data(),count,native.data(),&status);
        if(status||native!=reference)return 1;
        constexpr int repeats=500;
        for(int i=0;i<50;i++){
            nm_qmemory_decode(bits.data(),count,native.data(),&status);
            if(status||!scalar(bits.data(),count,reference.data()))return 4;
        }
        for(int sample=0;sample<5;sample++){
            auto timed=[&](bool fortran){
                auto begin=std::chrono::steady_clock::now();
                for(int i=0;i<repeats;i++){
                    if(fortran){nm_qmemory_decode(bits.data(),count,native.data(),&status);
                        if(status)return std::chrono::nanoseconds(-1);}
                    else {if(!scalar(bits.data(),count,reference.data()))return std::chrono::nanoseconds(-1);
                        if(reference[(unsigned)i%count]!=native[(unsigned)i%count])return std::chrono::nanoseconds(-1);}
                }
                return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin);
            };
            std::chrono::nanoseconds native_time,scalar_time;
            if(sample%2){scalar_time=timed(false);native_time=timed(true);}
            else {native_time=timed(true);scalar_time=timed(false);}
            if(native_time.count()<0||scalar_time.count()<0)return 5;
            std::printf("%zu,%d,%lld,%lld\n",count,sample,
                (long long)native_time.count(),(long long)scalar_time.count());
        }
    }
}
