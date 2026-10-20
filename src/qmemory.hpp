#ifndef NM_QMEMORY_HPP
#define NM_QMEMORY_HPP
#include "neuro_mantle.h"
#include "primitiveq.h"
#include "neuro_world.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nm_qmemory {
struct Fault {
    std::uint64_t tick;
    std::size_t edge;
    unsigned bit, qubit;
    char op;
};
struct TickReport {
    nm_world_report world{};
    unsigned memory_corrected=0;
    std::string memory_before,memory_after;
};
class Weights {
    struct Cell { pq_row rows[10]; };
    static_assert(sizeof(Cell)==10*sizeof(pq_row));
    std::vector<Cell> cells_;
    std::vector<std::uint8_t> fault_count_;
    std::size_t edges_;
    std::size_t active_faults_=0;
    void encode_edge(std::size_t edge,std::int32_t weight);
public:
    explicit Weights(const nm_network &network);
    void capture(const nm_network &network);
    void capture_updated(const nm_network &network,std::span<const std::int32_t> before);
    void fault(std::size_t edge,unsigned bit,unsigned qubit,char op);
    unsigned restore(nm_network &network);
    // Stage both memory and world, execute exactly one nm_world_step, and
    // publish only after restore, feedback and STDP re-encoding all succeed.
    TickReport step(nm_world &world,const nm_world_input &input,
                    std::span<const Fault> faults={});
    std::string root() const;
    std::string export_snapshot(std::uint64_t tick) const;
    std::size_t edges() const { return edges_; }
};
void verify_certificate_identity(const std::string &bytes);
}
#endif
