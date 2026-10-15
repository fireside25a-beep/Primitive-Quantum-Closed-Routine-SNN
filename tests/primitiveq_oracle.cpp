#include "primitiveq.h"
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>

using Complex=std::complex<double>;
using Vector=std::array<Complex,8>;
static constexpr double tolerance=1e-9;
static std::uint32_t next(std::uint32_t &s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
static void gate(Vector &v,unsigned n,unsigned q,char op){
    const unsigned stride=1u<<q;
    for(unsigned i=0;i<(1u<<n);i++)if(!(i&stride)){
        auto a=v[i],b=v[i|stride];
        if(op=='H'){v[i]=(a+b)/std::sqrt(2.);v[i|stride]=(a-b)/std::sqrt(2.);}
        if(op=='S')v[i|stride]=b*Complex(0,1);
        if(op=='X'){v[i]=b;v[i|stride]=a;}
        if(op=='Y'){v[i]=Complex(0,-1)*b;v[i|stride]=Complex(0,1)*a;}
        if(op=='Z')v[i|stride]=-b;
    }
}
static void cnot(Vector &v,unsigned n,unsigned c,unsigned t){
    for(unsigned i=0;i<(1u<<n);i++)if((i&(1u<<c))&&!(i&(1u<<t)))
        std::swap(v[i],v[i|(1u<<t)]);
}
static double probability(const Vector &v,unsigned n,unsigned q){
    double p=0;for(unsigned i=0;i<(1u<<n);i++)if(i&(1u<<q))p+=std::norm(v[i]);return p;
}
static void collapse(Vector &v,unsigned n,unsigned q,unsigned result,double p){
    const double divisor=std::sqrt(result?p:1-p);
    for(unsigned i=0;i<(1u<<n);i++){
        if(!!(i&(1u<<q))!=!!result)v[i]=0.;else v[i]/=divisor;
    }
}
int main(){
    for(unsigned n=1;n<=3;n++)for(unsigned trial=0;trial<256;trial++){
        pq_state s;assert(pq_init(&s,n)==0);
        Vector v{};v[0]=1.;
        std::uint32_t seed=0x97f4a62du+n*1009u+trial;
        for(unsigned step=0;step<64;step++){
            const auto x=next(seed);unsigned q=x%n;
            unsigned op=(x>>12)%8;
            if(op<=4){
                const char g="HSXYZ"[op];
                if(g=='H')assert(!pq_h(&s,q));
                else if(g=='S')assert(!pq_s(&s,q));
                else assert(!pq_pauli(&s,q,g));
                gate(v,n,q,g);
            }else if(op==5&&n>1){
                unsigned t=(q+1+(x>>16)%(n-1))%n;
                assert(!pq_cnot(&s,q,t));cnot(v,n,q,t);
            }else{
                const double p=probability(v,n,q);
                const unsigned draw=next(seed)&1u;
                unsigned outcome=0,random=0;
                assert(!pq_measure_z(&s,q,draw,&outcome,&random));
                unsigned expected=p< tolerance?0:p>1-tolerance?1:draw;
                unsigned is_random=p>tolerance&&p<1-tolerance;
                if(outcome!=expected||random!=is_random){
                    std::fprintf(stderr,"primitiveq oracle mismatch n=%u trial=%u step=%u p=%.12f actual=%u random=%u\n",
                                 n,trial,step,p,outcome,random);
                    return 1;
                }
                collapse(v,n,q,outcome,p);
                if(op==7){
                    if(outcome){assert(!pq_pauli(&s,q,'X'));gate(v,n,q,'X');}
                }
            }
            for(unsigned probe=0;probe<n;probe++){
                auto copy=s;unsigned observed=0,random=0;
                assert(!pq_measure_z(&copy,probe,0,&observed,&random));
                double p=probability(v,n,probe);
                if(observed!=(p>1-tolerance)||random!=(p>tolerance&&p<1-tolerance)){
                    std::fprintf(stderr,"primitiveq probe mismatch n=%u trial=%u step=%u probe=%u p=%.12f\n",
                                 n,trial,step,probe,p);
                    return 1;
                }
            }
        }
    }
    std::puts("primitiveq independent statevector oracle: PASS (49,152 randomized transitions)");
}
