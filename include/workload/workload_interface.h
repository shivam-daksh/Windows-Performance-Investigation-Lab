#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "diagnostics/etw_provider.h"

namespace perf_lab {

struct WorkloadMetrics {
    uint64_t total_operations{0};
    uint64_t final_counter{0};
    double elapsed_ms{0.0};
    double operations_per_second{0.0};
    bool success{false};
    std::string error_message;
};

class IWorkload {
public:
    virtual ~IWorkload() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] virtual WorkloadMetrics run(uint32_t threads,
                                              uint64_t operations_per_worker,
                                              IEtwProvider& etw) = 0;
};

} // namespace perf_lab
