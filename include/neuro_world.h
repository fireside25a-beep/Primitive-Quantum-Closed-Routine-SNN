#ifndef NEURO_WORLD_H
#define NEURO_WORLD_H
#include "neuro_mantle.h"
#include "primitiveq.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NM_WORLD_NODES 64u
#define NM_WORLD_LINKS 256u
#define NM_WORLD_RULES 32u
#define NM_WORLD_MASS_MAX UINT64_C(1000000000000)
typedef struct {
    uint64_t mass;
    uint16_t sensor_neuron;
    uint64_t sensor_threshold;
    int64_t sensor_current;
} nm_world_node;
typedef struct {
    uint16_t from,to,gate_neuron,context_node;
    uint32_t rate_q16,bonus_q16;
    uint64_t cap,context_threshold;
} nm_world_link;
typedef struct {
    uint16_t trigger,a,b,output_neuron;
    int64_t output_current;
    char op;
} nm_world_rule;
typedef struct nm_world {
    nm_network *network;
    pq_state quantum;
    nm_world_node nodes[NM_WORLD_NODES];
    nm_world_link links[NM_WORLD_LINKS];
    nm_world_rule rules[NM_WORLD_RULES];
    uint16_t node_count,link_count,rule_count;
    uint64_t clock_ns;
    uint8_t five_code,five_faults;
} nm_world;
typedef struct {
    uint16_t stimulus_neuron;
    int64_t stimulus_current;
    /* Indexed by rule. Caller records the provenance of these draw values. */
    uint64_t draws[NM_WORLD_RULES];
} nm_world_input;
typedef struct {
    nm_spikes spikes;
    uint64_t mass_before,mass_after,flux_total;
    uint32_t fired_rules,measurement_rules,measurement_ones,random_rules;
    uint32_t corrected_rules;
    uint8_t syndromes[NM_WORLD_RULES],corrections[NM_WORLD_RULES];
} nm_world_report;
nm_world *nm_world_new(unsigned qubits,uint64_t clock_ns);
void nm_world_free(nm_world *);
int nm_world_enable_five_code(nm_world *);
int nm_world_add_node(nm_world *,uint64_t mass,uint16_t sensor,uint64_t threshold,int64_t current);
int nm_world_add_link(nm_world *,nm_world_link);
int nm_world_add_rule(nm_world *,nm_world_rule);
/* Whole SNN/field/quantum transition is staged and committed atomically. */
int nm_world_step(nm_world *,const nm_world_input *,nm_world_report *);
#ifdef __cplusplus
}
#endif
#endif
