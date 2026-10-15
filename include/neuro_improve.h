#ifndef NEURO_IMPROVE_H
#define NEURO_IMPROVE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NM_IMPROVE_PARAMETERS 512u
#define NM_IMPROVE_ARCHIVE 16u
#define NM_IMPROVE_OPERATORS 5u
#define NM_IMPROVE_BUDGET 2048u
typedef struct {
    uint32_t count,budget,seed,group_width;
    int32_t initial[NM_IMPROVE_PARAMETERS];
    int32_t lower[NM_IMPROVE_PARAMETERS],upper[NM_IMPROVE_PARAMETERS];
} nm_improve_spec;
typedef struct {int64_t score;uint32_t behavior;} nm_improve_score;
typedef int (*nm_improve_evaluate)(const int32_t *,uint32_t,unsigned,void *,nm_improve_score *);
typedef struct {
    uint32_t attempt,operator_id,parent_slot,radius,random_state;
    int64_t train_score,validation_score,champion_train,champion_validation;
    uint32_t behavior,archive_replaced,admitted;
    uint32_t operator_credit[NM_IMPROVE_OPERATORS];
} nm_improve_trace;
typedef struct {
    int32_t parameters[NM_IMPROVE_PARAMETERS];
    int64_t train_score,validation_score;
    uint32_t attempts,admitted,archive_occupied,random_state;
    uint32_t operator_credit[NM_IMPROVE_OPERATORS];
} nm_improve_result;
/* Phase 0: training reward and behavior descriptor. Phase 1: validation.
 * Callback failure aborts without replacing the caller's result object.
 * The champion never decreases on either declared finite score set.
 */
int nm_improve_run(const nm_improve_spec *,nm_improve_evaluate,void *,
                   nm_improve_result *,nm_improve_trace *,uint32_t trace_capacity);
#ifdef __cplusplus
}
#endif
#endif
