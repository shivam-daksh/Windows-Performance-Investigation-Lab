#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace perf_lab {

enum class WorkloadMode {
    Baseline,
    Optimized,
    Compare
};

[[nodiscard]] constexpr std::string_view to_string(WorkloadMode mode) noexcept {
    switch (mode) {
        case WorkloadMode::Baseline:
            return "baseline";
        case WorkloadMode::Optimized:
            return "optimized";
        case WorkloadMode::Compare:
            return "compare";
    }
    return "unknown";
}

struct BenchmarkConfig {
    WorkloadMode mode{WorkloadMode::Compare};
    uint32_t threads{0};                       // 0 indicates auto-detect hardware concurrency
    uint64_t operations_per_worker{1'000'000}; // Default 1,000,000 per worker
    uint32_t runs{5};                          // Default 5 runs
    std::filesystem::path output_dir{"results"};
};

} // namespace perf_lab
