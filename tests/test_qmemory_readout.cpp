#include "qmemory_readout.h"
#include <array>
#include <cassert>
#include <cstdint>

int main(){
    // Derived independently from XZZXI, IXZZX, XIXZZ, ZXIXZ by
    // s_j = parity(x & g_j.z) xor parity(z & g_j.x).
    constexpr std::array<std::array<std::int8_t,3>,5> syndrome={{
        {{8,13,5}},{{1,11,10}},{{3,7,4}},{{6,15,9}},{{12,14,2}}
    }}; // columns X,Y,Z
    constexpr std::array<std::int32_t,2> words={{-9,0x12345678}};
    for(const auto word:words)for(unsigned qubit=0;qubit<5;qubit++)
        for(unsigned op=0;op<3;op++)for(unsigned target_bit:{0u,7u,31u}){
            std::array<std::int8_t,32> observed{},raw{},expected{},decoded{},codes{};
            const std::uint32_t bits=static_cast<std::uint32_t>(word);
            for(unsigned bit=0;bit<32;bit++){
                expected[bit]=static_cast<std::int8_t>((bits>>bit)&1u);
                raw[bit]=expected[bit];
            }
            observed[target_bit]=syndrome[qubit][op];
            if(op!=2)raw[target_bit]^=1; // X and Y anticommute with ZZZZZ
            std::int32_t recovered=0;int status=-1;
            nm_qmemory_decode_observed(observed.data(),raw.data(),1,
                                       decoded.data(),codes.data(),&recovered,&status);
            assert(status==0 && recovered==word && decoded==expected);
            const unsigned code=3*qubit+(op==0?1u:op==2?2u:3u);
            assert(static_cast<unsigned>(codes[target_bit])==code);
            for(unsigned bit=0;bit<32;bit++)if(bit!=target_bit)assert(codes[bit]==0);
            raw[target_bit]^=1;
            nm_qmemory_decode_observed(observed.data(),raw.data(),1,
                                       decoded.data(),codes.data(),&recovered,&status);
            assert(status==0 && decoded[target_bit]!=expected[target_bit]);
            // A raw bit changed independently of a tableau is detected by the
            // C caller's canonical codeword check, not by this arithmetic kernel.
        }
    std::array<std::int8_t,32> syndrome_bits{},raw{},decoded{},codes{};
    std::int32_t recovered=0;int status=-1;
    syndrome_bits[0]=16;
    nm_qmemory_decode_observed(syndrome_bits.data(),raw.data(),1,
                               decoded.data(),codes.data(),&recovered,&status);
    assert(status==1);
    syndrome_bits[0]=-1;
    nm_qmemory_decode_observed(syndrome_bits.data(),raw.data(),1,
                               decoded.data(),codes.data(),&recovered,&status);
    assert(status==1);
    syndrome_bits[0]=0;raw[0]=2;
    nm_qmemory_decode_observed(syndrome_bits.data(),raw.data(),1,
                               decoded.data(),codes.data(),&recovered,&status);
    assert(status==1);
    raw[0]=0;
    nm_qmemory_decode_observed(syndrome_bits.data(),raw.data(),0,
                               decoded.data(),codes.data(),&recovered,&status);
    assert(status==1);
}
