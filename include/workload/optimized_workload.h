#pragma once

#include "workload/workload_interface.h"

namespace perf_lab {

class OptimizedWorkload final : public IWorkload {
public:
    OptimizedWorkload() = default;
    ~OptimizedWorkload() override = default;

    [[nodiscard]] std::string_view name() const noexcept override {
        return "Optimized (Thread-Local Reduction)";
    }

    [[nodiscard]] WorkloadMetrics run(uint32_t threads,
                                      uint64_t operations_per_worker,
                                      IEtwProvider& etw) override;
};

} // namespace perf_lab
