#include "neuro_improve.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {int32_t values[NM_IMPROVE_PARAMETERS];int64_t score;unsigned occupied;} cell;
static uint32_t draw(uint32_t *state) {
    uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;*state=x;return x;
}
static int32_t bounded(int64_t v,int32_t lo,int32_t hi) {
    return v<lo?lo:v>hi?hi:(int32_t)v;
}
static void change(const nm_improve_spec *s,int32_t *v,uint32_t i,int delta) {
    v[i]=bounded((int64_t)v[i]+delta,s->lower[i],s->upper[i]);
}
int nm_improve_run(const nm_improve_spec *s,nm_improve_evaluate evaluate,void *data,
                   nm_improve_result *out,nm_improve_trace *trace,uint32_t cap) {
    if(!s || !evaluate || !out || !trace || !s->count || s->count>NM_IMPROVE_PARAMETERS ||
       !s->budget || s->budget>NM_IMPROVE_BUDGET || cap<s->budget ||
       !s->group_width || s->group_width>s->count) return -1;
    for(uint32_t i=0;i<s->count;i++)
        if(s->lower[i]>s->upper[i] || s->initial[i]<s->lower[i] || s->initial[i]>s->upper[i])return -1;
    cell *archive=calloc(NM_IMPROVE_ARCHIVE,sizeof(*archive));
    if(!archive)return -1;
    nm_improve_result r={0};
    memcpy(r.parameters,s->initial,s->count*sizeof(int32_t));
    r.random_state=s->seed?s->seed:UINT32_C(0x9e3779b9);
    for(unsigned i=0;i<NM_IMPROVE_OPERATORS;i++)r.operator_credit[i]=8;
    nm_improve_score initial,validation;
    if(evaluate(r.parameters,s->count,0,data,&initial) || initial.behavior>=NM_IMPROVE_ARCHIVE ||
       evaluate(r.parameters,s->count,1,data,&validation))goto fail;
    r.train_score=initial.score;r.validation_score=validation.score;
    archive[initial.behavior].occupied=1;archive[initial.behavior].score=initial.score;
    memcpy(archive[initial.behavior].values,r.parameters,s->count*sizeof(int32_t));
    r.archive_occupied=1;r.attempts=1;
    trace[0]=(nm_improve_trace){.attempt=0,.operator_id=UINT32_MAX,.parent_slot=UINT32_MAX,
        .radius=1,.random_state=r.random_state,.train_score=initial.score,
        .validation_score=validation.score,.champion_train=initial.score,
        .champion_validation=validation.score,.behavior=initial.behavior,.archive_replaced=1,.admitted=1};
    memcpy(trace[0].operator_credit,r.operator_credit,sizeof(r.operator_credit));
    uint32_t radius=1,stagnation=0;
    for(uint32_t attempt=1;attempt<s->budget;attempt++) {
        uint32_t total=0;for(unsigned i=0;i<NM_IMPROVE_OPERATORS;i++)total+=r.operator_credit[i];
        uint32_t pick=draw(&r.random_state)%total,op=0;
        while(pick>=r.operator_credit[op]){pick-=r.operator_credit[op];op++;}
        int32_t candidate[NM_IMPROVE_PARAMETERS];
        memcpy(candidate,r.parameters,s->count*sizeof(int32_t));
        uint32_t parent=UINT32_MAX;
        if(draw(&r.random_state)%3u==0) {
            uint32_t slots[NM_IMPROVE_ARCHIVE],n=0;
            for(unsigned i=0;i<NM_IMPROVE_ARCHIVE;i++)if(archive[i].occupied)slots[n++]=i;
            parent=slots[draw(&r.random_state)%n];
            memcpy(candidate,archive[parent].values,s->count*sizeof(int32_t));
        }
        int direction=(draw(&r.random_state)&1u)?1:-1;
        uint32_t first=draw(&r.random_state)%s->count;
        if(op==0)change(s,candidate,first,direction*(int)radius);
        if(op==1) {
            change(s,candidate,first,direction*(int)radius);
            uint32_t second=draw(&r.random_state)%s->count;
            change(s,candidate,second,((draw(&r.random_state)&1u)?1:-1)*(int)radius);
        }
        if(op==2) {
            uint32_t start=(first/s->group_width)*s->group_width;
            for(uint32_t i=start;i<s->count && i<start+s->group_width;i++)
                change(s,candidate,i,((draw(&r.random_state)&1u)?1:-1)*(int)radius);
        }
        if(op==3)for(uint32_t i=0;i<s->count;i++)
            if(draw(&r.random_state)%8u==0)change(s,candidate,i,((draw(&r.random_state)&1u)?1:-1)*(int)radius);
        if(op==4) {
            uint32_t slots[NM_IMPROVE_ARCHIVE],n=0;
            for(unsigned i=0;i<NM_IMPROVE_ARCHIVE;i++)if(archive[i].occupied)slots[n++]=i;
            cell *other=&archive[slots[draw(&r.random_state)%n]];
            for(uint32_t i=0;i<s->count;i++)if(draw(&r.random_state)&1u)candidate[i]=other->values[i];
            change(s,candidate,first,direction*(int)radius);
        }
        nm_improve_score score;
        if(evaluate(candidate,s->count,0,data,&score) || score.behavior>=NM_IMPROVE_ARCHIVE)goto fail;
        unsigned replaced=0,admitted=0;
        cell *slot=&archive[score.behavior];
        if(!slot->occupied || score.score>slot->score) {
            if(!slot->occupied)r.archive_occupied++;
            slot->occupied=1;slot->score=score.score;
            memcpy(slot->values,candidate,s->count*sizeof(int32_t));replaced=1;
        }
        int64_t val=INT64_MIN;
        if(score.score>=r.train_score) {
            nm_improve_score check;
            if(evaluate(candidate,s->count,1,data,&check))goto fail;
            val=check.score;
            if(val>=r.validation_score && (score.score>r.train_score || val>r.validation_score)) {
                memcpy(r.parameters,candidate,s->count*sizeof(int32_t));
                r.train_score=score.score;r.validation_score=val;r.admitted++;admitted=1;
            }
        }
        unsigned credit=admitted?6:replaced?2:0;
        if(credit) {
            r.operator_credit[op]+=credit;
            if(r.operator_credit[op]>64)r.operator_credit[op]=64;
        }else if(r.operator_credit[op]>1)r.operator_credit[op]--;
        stagnation=admitted?0:stagnation+1;
        if(stagnation && stagnation%16==0)radius=radius==8?1:radius+1;
        if(admitted && radius>1)radius--;
        trace[attempt]=(nm_improve_trace){.attempt=attempt,.operator_id=op,.parent_slot=parent,
            .radius=radius,.random_state=r.random_state,.train_score=score.score,.validation_score=val,
            .champion_train=r.train_score,.champion_validation=r.validation_score,
            .behavior=score.behavior,.archive_replaced=replaced,.admitted=admitted};
        memcpy(trace[attempt].operator_credit,r.operator_credit,sizeof(r.operator_credit));
        r.attempts++;
    }
    free(archive);*out=r;return 0;
fail:
    free(archive);return -1;
}
