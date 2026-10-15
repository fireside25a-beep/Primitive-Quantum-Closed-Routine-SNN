#ifndef NM_QMEMORY_READOUT_H
#define NM_QMEMORY_READOUT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* For the exact ordered five-qubit code, decode one four-sign syndrome and
 * logical-Z read bit per encoded weight bit. The caller supplies an independent
 * expected weight-bit snapshot and guarantees at most one Pauli fault per cell.
 * No outputs are authoritative unless status is zero. Status 1 means malformed
 * input, 2 means the recovered bit disagrees with the expected snapshot.
 * A syndrome alone cannot recover a logical bit or authenticate a device. */
void nm_qmemory_decode_observed(const int8_t *syndromes,
                                const int8_t *raw_logical,
                                const int8_t *expected,
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
