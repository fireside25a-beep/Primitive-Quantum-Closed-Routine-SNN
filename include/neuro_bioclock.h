#ifndef NEURO_BIOCLOCK_H
#define NEURO_BIOCLOCK_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Nanoseconds are simulated time, never wall-clock time. */
#define NM_BIO_MAX_CLOCK_NS UINT64_C(1000000000)
#define NM_BIO_MAX_TAU_NS UINT64_C(1000000000000)

/* Accept only exactly representable grid times within the caller's bound. */
int nm_bio_to_ticks(uint64_t clock_ns, uint64_t time_ns, uint64_t max_ticks,
                    uint64_t *ticks);
int nm_bio_time_ns(uint64_t clock_ns, uint64_t ticks, uint64_t *time_ns);

/* Backward-Euler LIF retention tau/(tau+dt), rounded to nearest even Q16. */
int nm_bio_leak_q16(uint64_t clock_ns, uint64_t tau_ns, uint32_t *leak_q16);

#ifdef __cplusplus
}
#endif
#endif
