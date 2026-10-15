#include "neuro_bioclock.h"
#include "neuro_mantle.h"

static int valid_clock(uint64_t clock_ns) {
    return clock_ns > 0 && clock_ns <= NM_BIO_MAX_CLOCK_NS;
}

int nm_bio_to_ticks(uint64_t clock_ns, uint64_t time_ns, uint64_t max_ticks,
                    uint64_t *ticks) {
    if (!ticks || !valid_clock(clock_ns) || time_ns % clock_ns) return -1;
    uint64_t count = time_ns / clock_ns;
    if (count > max_ticks) return -1;
    *ticks = count;
    return 0;
}

int nm_bio_time_ns(uint64_t clock_ns, uint64_t ticks, uint64_t *time_ns) {
    uint64_t value;
    if (!time_ns || !valid_clock(clock_ns) ||
        __builtin_mul_overflow(clock_ns, ticks, &value)) return -1;
    *time_ns = value;
    return 0;
}

int nm_bio_leak_q16(uint64_t clock_ns, uint64_t tau_ns, uint32_t *leak_q16) {
    if (!leak_q16 || !valid_clock(clock_ns) || !tau_ns ||
        tau_ns > NM_BIO_MAX_TAU_NS) return -1;
    const uint64_t numerator = (uint64_t)NM_Q16 * tau_ns;
    const uint64_t denominator = tau_ns + clock_ns;
    uint64_t q = numerator / denominator;
    const uint64_t remainder = numerator % denominator;
    const uint64_t twice = 2 * remainder;
    if (twice > denominator || (twice == denominator && (q & 1))) ++q;
    /* Refuse dynamics that quantize all decay away or erase the membrane. */
    if (!q || q >= (uint64_t)NM_Q16) return -1;
    *leak_q16 = (uint32_t)q;
    return 0;
}
