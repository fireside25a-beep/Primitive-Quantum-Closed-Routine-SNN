#include "../training/optimizer.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

static int64_t objective(const int32_t *w, size_t count, void *unused) {
    (void)unused;
    assert(count==2);
    return -((int64_t)(w[0]-3)*(w[0]-3)+(int64_t)(w[1]+2)*(w[1]+2));
}
static int64_t fail(const int32_t *w, size_t n, void *unused) {
    (void)w;(void)n;(void)unused;
    return INT64_MIN;
}
int main(void) {
    nm_train_spec s={0};
    s.count=2;s.passes=12;s.evaluations=128;s.trace_capacity=128;
    s.lower[0]=s.lower[1]=-6;s.upper[0]=s.upper[1]=6;
    s.step[0]=s.step[1]=3;
    nm_train_trace t[128],v[128];
    nm_train_result r,u;
    assert(nm_train_optimize(&s,objective,NULL,&r,t)==0);
    assert(nm_train_optimize(&s,objective,NULL,&u,v)==0);
    assert(r.best_score==0 && r.parameters[0]==3 && r.parameters[1]==-2);
    assert(r.evaluations==u.evaluations);
    for(unsigned i=0;i<r.evaluations;i++)
        assert(t[i].evaluation==v[i].evaluation && t[i].coordinate==v[i].coordinate &&
               t[i].pass==v[i].pass && t[i].value==v[i].value &&
               t[i].score==v[i].score && t[i].incumbent==v[i].incumbent &&
               t[i].accepted==v[i].accepted);
    assert(nm_train_optimize(&s,fail,NULL,&r,t)<0);
    s.trace_capacity=1;
    assert(nm_train_optimize(&s,objective,NULL,&r,t)<0);
    nm_train_pattern samples[4]={0};
    samples[0].label=samples[1].label=0;
    samples[2].label=samples[3].label=1;
    samples[0].words[0]=1;samples[1].words[0]=3;
    samples[2].words[0]=8;samples[3].words[0]=8;
    uint64_t prototypes[32]={0};
    assert(nm_train_hamming_fit(samples,4,2,prototypes)==0);
    assert(prototypes[0]==1 && prototypes[16]==8);
    uint64_t query[16]={8};
    assert(nm_train_hamming_predict(prototypes,2,query)==1);
    query[0]=0;
    assert(nm_train_hamming_predict(prototypes,2,query)==0);
    samples[3].label=2;
    assert(nm_train_hamming_fit(samples,4,2,prototypes)<0);
    puts("native deterministic integer and Hamming training PASS");
    return 0;
}
