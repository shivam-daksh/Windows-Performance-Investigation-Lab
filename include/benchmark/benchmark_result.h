#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace perf_lab {

struct SingleRunResult {
    std::string timestamp_utc;
    std::string workload_name;
    uint32_t threads{0};
    uint64_t operations_per_worker{0};
    uint64_t total_operations{0};
    uint32_t run_number{0};
    double elapsed_ms{0.0};
    double operations_per_second{0.0};
    uint64_t final_counter{0};
    bool success{false};
    std::string error_message;
};

struct AggregatedStatistics {
    double mean_elapsed_ms{0.0};
    double median_elapsed_ms{0.0};
    double min_elapsed_ms{0.0};
    double max_elapsed_ms{0.0};
    double mean_ops_per_second{0.0};
    double median_ops_per_second{0.0};
};

struct WorkloadBenchmarkResult {
    std::string workload_name;
    uint32_t threads{0};
    uint64_t operations_per_worker{0};
    uint64_t total_operations{0};
    std::vector<SingleRunResult> runs;
    AggregatedStatistics stats;
    bool all_succeeded{false};
};

struct BenchmarkSuiteResult {
    std::vector<WorkloadBenchmarkResult> workload_results;
    std::string csv_path;
    std::string summary_path;
    bool overall_success{false};
};

[[nodiscard]] AggregatedStatistics calculate_statistics(const std::vector<SingleRunResult>& runs);

} // namespace perf_lab
