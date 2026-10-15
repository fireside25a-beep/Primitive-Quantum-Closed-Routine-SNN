#ifndef NM_QMEMORY_READOUT_H
#define NM_QMEMORY_READOUT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Decode four stabilizer readout signs and a raw logical-Z eigenvalue per
 * encoded weight bit. The caller verifies the corrected tableau against the
 * canonical codeword selected by the decoded bit before committing any data.
 * The declared noise model permits at most one Pauli fault per codeword.
 * No outputs are authoritative unless status is zero. */
void nm_qmemory_decode_observed(const int8_t *syndromes,
                                const int8_t *raw_logical,
                                size_t edge_count,
                                int8_t *logical,
                                int8_t *corrections,
                                int32_t *weights,
                                int *status);
void nm_qmemory_decode(const int8_t *bits,size_t edge_count,
                       int32_t *weights,int *status);
#ifdef __cplusplus
}
#endif
#endif
