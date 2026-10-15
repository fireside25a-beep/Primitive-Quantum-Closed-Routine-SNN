#include "optimizer.h"
#include <limits.h>
#include <string.h>

static int valid(const nm_train_spec *s) {
    if (!s || !s->count || s->count>NM_TRAIN_MAX_PARAMETERS ||
        !s->passes || s->passes>32 || !s->evaluations ||
        s->evaluations>NM_TRAIN_MAX_EVALUATIONS ||
        s->trace_capacity<s->evaluations) return 0;
    for (uint32_t i=0;i<s->count;i++)
        if(s->lower[i]>s->upper[i] || s->initial[i]<s->lower[i] ||
           s->initial[i]>s->upper[i] || s->step[i]<1) return 0;
    return 1;
}

int nm_train_optimize(const nm_train_spec *s, nm_train_score_fn score, void *data,
                      nm_train_result *out, nm_train_trace *trace) {
    if (!valid(s) || !score || !out || !trace) return -1;
    nm_train_result r={0};
    memcpy(r.parameters,s->initial,sizeof(r.parameters));
    r.best_score=score(r.parameters,s->count,data);
    if(r.best_score==INT64_MIN) return -1;
    r.evaluations=1;
    trace[0]=(nm_train_trace){0,UINT32_MAX,0,0,r.best_score,r.best_score,1};
    for(uint32_t pass=0;pass<s->passes && r.evaluations<s->evaluations;pass++) {
        int changed=0;
        for(uint32_t i=0;i<s->count && r.evaluations<s->evaluations;i++) {
            int32_t delta=s->step[i]>>pass;
            if(!delta) delta=1;
            for(int dir=-1;dir<=1;dir+=2) {
                if(r.evaluations>=s->evaluations) break;
                int64_t value=(int64_t)r.parameters[i]+(int64_t)dir*delta;
                if(value<s->lower[i] || value>s->upper[i]) continue;
                int32_t candidate[NM_TRAIN_MAX_PARAMETERS];
                memcpy(candidate,r.parameters,sizeof(candidate));
                candidate[i]=(int32_t)value;
                int64_t result=score(candidate,s->count,data);
                if(result==INT64_MIN) return -1;
                uint32_t attempt=r.evaluations++;
                int accept=result>r.best_score;
                if(accept) {
                    memcpy(r.parameters,candidate,sizeof(r.parameters));
                    r.best_score=result;
                    r.accepted++;
                    changed=1;
                }
                trace[attempt]=(nm_train_trace){attempt,i,pass,(int32_t)value,
                    result,r.best_score,(uint8_t)accept};
            }
        }
        if(!changed && pass+1>=s->passes) break;
    }
    *out=r;
    return 0;
}

int nm_train_hamming_fit(const nm_train_pattern *samples, size_t n,
                         uint16_t classes, uint64_t *prototypes) {
    if(!samples || !n || n>65535 || !classes || classes>32 || !prototypes) return -1;
    uint32_t count[32]={0};
    uint32_t ones[32][1024]={0};
    for(size_t j=0;j<n;j++) {
        uint16_t label=samples[j].label;
        if(label>=classes) return -1;
        count[label]++;
        for(unsigned k=0;k<16;k++) {
            uint64_t word=samples[j].words[k];
            for(unsigned bit=0;bit<64;bit++)
                ones[label][k*64+bit]+=(uint32_t)((word>>bit)&1u);
        }
    }
    for(unsigned label=0;label<classes;label++) {
        if(!count[label]) return -1;
        for(unsigned k=0;k<16;k++) {
            uint64_t word=0;
            for(unsigned bit=0;bit<64;bit++)
                if(2u*ones[label][k*64+bit]>count[label]) word|=UINT64_C(1)<<bit;
            prototypes[label*16+k]=word;
        }
    }
    return 0;
}

int nm_train_hamming_predict(const uint64_t *prototypes, uint16_t classes,
                             const uint64_t query[16]) {
    if(!prototypes || !query || !classes || classes>32) return -1;
    unsigned closest=1025;
    int result=-1;
    for(unsigned label=0;label<classes;label++) {
        unsigned distance=0;
        for(unsigned k=0;k<16;k++)
            distance+=(unsigned)__builtin_popcountll(prototypes[label*16+k]^query[k]);
        if(distance<closest) {closest=distance;result=(int)label;}
    }
    return result;
}
