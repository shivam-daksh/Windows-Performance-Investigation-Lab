#pragma once

#include "benchmark/benchmark_config.h"
#include "benchmark/benchmark_result.h"
#include "diagnostics/etw_provider.h"
#include "workload/workload_interface.h"

#include <memory>
#include <string>
#include <vector>

namespace perf_lab {

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(IEtwProvider& etw_provider);
    ~BenchmarkRunner() = default;

    BenchmarkRunner(const BenchmarkRunner&) = delete;
    BenchmarkRunner& operator=(const BenchmarkRunner&) = delete;

    [[nodiscard]] BenchmarkSuiteResult execute(const BenchmarkConfig& config);

    [[nodiscard]] WorkloadBenchmarkResult run_workload(IWorkload& workload,
                                                       const BenchmarkConfig& config,
                                                       std::string_view mode_tag);

    static bool write_csv_report(const std::filesystem::path& file_path,
                                 const std::vector<SingleRunResult>& all_runs);

    static bool write_summary_report(const std::filesystem::path& file_path,
                                     const BenchmarkConfig& config,
                                     const BenchmarkSuiteResult& suite_result);

    static std::string format_utc_timestamp();

private:
    IEtwProvider& m_etw;
};

} // namespace perf_lab
