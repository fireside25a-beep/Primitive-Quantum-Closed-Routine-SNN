#ifndef NM_PRIMITIVEQ_H
#define NM_PRIMITIVEQ_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PQ_MAX_QUBITS 16u
typedef struct { uint32_t x,z; uint8_t phase; } pq_row;
/* Hermitian stabilizer tableau: destabilizers [0,n), stabilizers [n,2n). */
typedef struct { uint8_t n; pq_row rows[2*PQ_MAX_QUBITS]; } pq_state;
int pq_init(pq_state *,unsigned n);
int pq_h(pq_state *,unsigned q);
int pq_s(pq_state *,unsigned q);
int pq_cnot(pq_state *,unsigned control,unsigned target);
int pq_pauli(pq_state *,unsigned q,char op);
/* A supplied draw affects only genuinely random measurements.
 * The caller owns draw consumption; random_branch reports whether it determined the result. */
int pq_measure_z(pq_state *,unsigned q,uint64_t draw,unsigned *result,unsigned *random_branch);
int pq_reset(pq_state *,unsigned q,uint64_t draw,unsigned *random_branch);
/* Prepare the encoded |0_L> of the certified [[5,1,3]] generator set.
 * Read four deterministic stabilizer eigenvalues, then correct one Pauli fault.
 * This decoder assumes at most one physical Pauli error since its last read. */
int pq_init_five_code(pq_state *);
int pq_five_syndrome(const pq_state *,unsigned *syndrome);
/* Three rounds of four generator measurements with a resettable sixth
 * ancilla. A single classical readout flip can be injected for testing.
 * UINT_MAX for flip_round disables injection. Input tableau is unchanged. */
int pq_five_syndrome_circuit(const pq_state *,unsigned *syndrome);
int pq_five_syndrome_circuit_readout(const pq_state *,unsigned flip_round,
                                    unsigned flip_generator,unsigned *syndrome);
/* Measure logical Z=ZZZZZ with a sixth ancilla. Only a deterministic bit
 * eigenstate is accepted here; a coherent logical superposition needs an
 * explicit measurement/draw contract and cannot serve as a classical weight. */
int pq_five_logical_z_circuit(const pq_state *,unsigned *bit);
int pq_five_correct(pq_state *,unsigned *syndrome,unsigned *correction);
int pq_five_correct_readout(pq_state *,unsigned flip_round,unsigned flip_generator,
                            unsigned *syndrome,unsigned *correction);
/* Pauli error syndromes for bounded commuting generators, packed by error index. */
int pq_syndrome_scalar(const pq_row *,unsigned generators,unsigned n,
                       const pq_row *,size_t count,uint32_t *out);
int pq_syndrome_vector(const pq_row *,unsigned generators,unsigned n,
                       const pq_row *,size_t count,uint32_t *out);
typedef struct pq_batch pq_batch;
int pq_batch_prepare(const pq_row *errors,unsigned n,size_t count,pq_batch **out);
int pq_batch_syndrome(const pq_batch *,const pq_row *generators,unsigned count,uint32_t *out);
void pq_batch_free(pq_batch *);
#ifdef __cplusplus
}
#endif
#endif
