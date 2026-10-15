#pragma once
#include <cstdint>
#include <string_view>

// Read-only, optional telemetry. Never consumes model entropy or waits for a reader.
namespace nm_observer {
class Observer {
    int write_=-1, guard_=-1;
    unsigned limit_=512;
    std::uint64_t sequence_=0, dropped_=0;
    bool ended_=false;
    void frame(std::string_view kind, std::string_view payload) noexcept;
public:
    Observer() noexcept;
    ~Observer();
    Observer(const Observer&)=delete;
    Observer& operator=(const Observer&)=delete;
    void begin(std::string_view mode, std::string_view scenario) noexcept;
    void tick(std::uint64_t tick, std::string_view record) noexcept;
    void finish(std::string_view receipt_root) noexcept;
};
}
