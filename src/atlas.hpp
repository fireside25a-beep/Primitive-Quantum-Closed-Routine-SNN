#ifndef NM_ATLAS_HPP
#define NM_ATLAS_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nm_atlas {
struct Result {
    std::string receipt;
    std::string divergence_svg;
    std::string frontier_svg;
    std::string interaction_svg;
    std::string protocol;
    std::string protocol_receipt;
    bool admitted = false;
    bool has_solution = false;
};
using Executor = std::function<std::string(const std::string&)>;
Result run(const std::string& baseline, const std::string& plan,
           std::uint64_t clock_ns, std::uint64_t ticks,
           const std::vector<std::uint16_t>& configured, const Executor& execute);
Result refusal(const std::string& baseline, const std::string& plan, const std::string& reason);
}
#endif
