#ifndef NM_QMEMORY_HPP
#define NM_QMEMORY_HPP
#include "neuro_mantle.h"
#include "primitiveq.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nm_qmemory {
struct Fault {
    std::uint64_t tick;
    std::size_t edge;
    unsigned bit, qubit;
    char op;
};
class Weights {
    struct Cell { pq_row rows[10]; };
    static_assert(sizeof(Cell)==10*sizeof(pq_row));
    std::vector<Cell> cells_;
    std::vector<std::uint8_t> fault_count_;
    std::vector<std::int32_t> encoded_words_;
    std::size_t edges_;
    std::size_t active_faults_=0;
    bool initialized_=false;
public:
    explicit Weights(const nm_network &network);
    void capture(const nm_network &network);
    void fault(std::size_t edge,unsigned bit,unsigned qubit,char op);
    unsigned restore(nm_network &network);
    std::string root() const;
    std::size_t edges() const { return edges_; }
};
void verify_certificate_identity(const std::string &bytes);
}
#endif
