#pragma once

#include "workload/workload_interface.h"
#include <mutex>

namespace perf_lab {

class ContendedWorkload final : public IWorkload {
public:
    ContendedWorkload() = default;
    ~ContendedWorkload() override = default;

    [[nodiscard]] std::string_view name() const noexcept override {
        return "Baseline (Contended Mutex)";
    }

    [[nodiscard]] WorkloadMetrics run(uint32_t threads,
                                      uint64_t operations_per_worker,
                                      IEtwProvider& etw) override;
};

} // namespace perf_lab
