#pragma once
#include <string>
namespace nm_world_cli {
struct Result {std::string receipt,graph,qmem_cells;bool admitted=false;};
Result run(const std::string& scenario,bool export_cells=false);
Result improve(const std::string& scenario);
Result interaction(const std::string& scenario);
}
