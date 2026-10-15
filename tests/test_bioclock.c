#include "neuro_bioclock.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint64_t value=0;
    uint32_t q=0;
    assert(!nm_bio_to_ticks(100000,300000,64,&value)&&value==3);
    assert(!nm_bio_time_ns(100000,value,&value)&&value==300000);
    assert(!nm_bio_leak_q16(100000,1000000,&q)&&q==59578);
    assert(nm_bio_to_ticks(100000,300001,64,&value));
    assert(nm_bio_to_ticks(0,300000,64,&value));
    assert(nm_bio_to_ticks(100000,6500000,64,&value));
    assert(nm_bio_time_ns(1000000000,UINT64_MAX,&value));
    assert(nm_bio_leak_q16(1000000000,1,&q));
    assert(nm_bio_leak_q16(1,1000000000000ULL,&q));
    assert(nm_bio_leak_q16(100000,0,&q));
    puts("biological grid arithmetic: PASS");
    return 0;
}
