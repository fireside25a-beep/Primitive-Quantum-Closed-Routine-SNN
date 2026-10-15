#ifndef NM_TRAIN_OPTIMIZER_H
#define NM_TRAIN_OPTIMIZER_H
#include <stddef.h>
#include <stdint.h>

#define NM_TRAIN_MAX_PARAMETERS 64u
#define NM_TRAIN_MAX_EVALUATIONS 4096u
typedef int64_t (*nm_train_score_fn)(const int32_t *, size_t, void *);
typedef struct {
    uint32_t count, passes, evaluations, trace_capacity;
    int32_t initial[NM_TRAIN_MAX_PARAMETERS];
    int32_t lower[NM_TRAIN_MAX_PARAMETERS];
    int32_t upper[NM_TRAIN_MAX_PARAMETERS];
    int32_t step[NM_TRAIN_MAX_PARAMETERS];
} nm_train_spec;
typedef struct {
    int32_t parameters[NM_TRAIN_MAX_PARAMETERS];
    int64_t best_score;
    uint32_t evaluations, accepted;
} nm_train_result;
typedef struct {
    uint32_t evaluation, coordinate, pass;
    int32_t value;
    int64_t score, incumbent;
    uint8_t accepted;
} nm_train_trace;
/* A callback must be deterministic for identical parameters and fixed data.
 * INT64_MIN signals evaluator failure. No candidate replaces an equally
 * scoring incumbent; evaluation order and all arithmetic are specified. */
int nm_train_optimize(const nm_train_spec *, nm_train_score_fn, void *,
                      nm_train_result *, nm_train_trace *);

typedef struct { uint64_t words[16]; uint16_t label; } nm_train_pattern;
/* Fit one 1024-bit majority prototype per label 0..class_count-1.
 * Equal bit counts resolve to zero. Returns -1 on invalid input. */
int nm_train_hamming_fit(const nm_train_pattern *, size_t, uint16_t class_count,
                         uint64_t *prototype_words);
int nm_train_hamming_predict(const uint64_t *prototype_words,
                             uint16_t class_count, const uint64_t query[16]);
#endif
