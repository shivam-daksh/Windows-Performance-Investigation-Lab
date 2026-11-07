#include "benchmark/benchmark_runner.h"
#include "diagnostics/etw_provider.h"
#include "workload/contended_workload.h"
#include "workload/optimized_workload.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "[FAIL] Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while (false)

namespace perf_lab::test {

// Mock ETW provider for testing lifecycle event emissions
class MockEtwProvider final : public IEtwProvider {
public:
    uint32_t workload_started_count{0};
    uint32_t worker_started_count{0};
    uint32_t worker_completed_count{0};
    uint32_t workload_completed_count{0};
    uint32_t benchmark_run_completed_count{0};

    [[nodiscard]] bool is_registered() const noexcept override {
        return true;
    }

    void log_workload_started(std::string_view, uint32_t, uint64_t, uint64_t) override {
        ++workload_started_count;
    }

    void log_worker_started(std::string_view, uint32_t, uint64_t) override {
        ++worker_started_count;
    }

    void log_worker_completed(std::string_view, uint32_t, uint64_t, double) override {
        ++worker_completed_count;
    }

    void log_workload_completed(std::string_view, uint32_t, uint64_t, uint64_t, double, bool) override {
        ++workload_completed_count;
    }

    void log_benchmark_run_completed(std::string_view, uint32_t, uint32_t, double, double, bool) override {
        ++benchmark_run_completed_count;
    }
};

void test_statistics_odd_runs() {
    std::vector<SingleRunResult> runs(5);
    const double times[5] = {100.0, 300.0, 200.0, 500.0, 400.0};
    const double ops[5] = {1000.0, 3000.0, 2000.0, 5000.0, 4000.0};

    for (size_t i = 0; i < 5; ++i) {
        runs[i].elapsed_ms = times[i];
        runs[i].operations_per_second = ops[i];
    }

    const auto stats = calculate_statistics(runs);
    TEST_ASSERT(std::abs(stats.mean_elapsed_ms - 300.0) < 1e-6);
    TEST_ASSERT(std::abs(stats.median_elapsed_ms - 300.0) < 1e-6);
    TEST_ASSERT(std::abs(stats.min_elapsed_ms - 100.0) < 1e-6);
    TEST_ASSERT(std::abs(stats.max_elapsed_ms - 500.0) < 1e-6);
    TEST_ASSERT(std::abs(stats.mean_ops_per_second - 3000.0) < 1e-6);
    TEST_ASSERT(std::abs(stats.median_ops_per_second - 3000.0) < 1e-6);

    std::cout << "[PASS] test_statistics_odd_runs\n";
}

void test_statistics_even_runs() {
    std::vector<SingleRunResult> runs(4);
    const double times[4] = {100.0, 400.0, 200.0, 500.0};
    const double ops[4] = {1000.0, 4000.0, 2000.0, 5000.0};

    for (size_t i = 0; i < 4; ++i) {
        runs[i].elapsed_ms = times[i];
        runs[i].operations_per_second = ops[i];
    }

    const auto stats = calculate_statistics(runs);
    TEST_ASSERT(std::abs(stats.mean_elapsed_ms - 300.0) < 1e-6);
    // Median of {100, 200, 400, 500} is (200 + 400) / 2 = 300
    TEST_ASSERT(std::abs(stats.median_elapsed_ms - 300.0) < 1e-6);
    TEST_ASSERT(std::abs(stats.min_elapsed_ms - 100.0) < 1e-6);
    TEST_ASSERT(std::abs(stats.max_elapsed_ms - 500.0) < 1e-6);

    std::cout << "[PASS] test_statistics_even_runs\n";
}

void test_statistics_empty() {
    std::vector<SingleRunResult> runs;
    const auto stats = calculate_statistics(runs);
    TEST_ASSERT(stats.mean_elapsed_ms == 0.0);
    TEST_ASSERT(stats.median_elapsed_ms == 0.0);
    TEST_ASSERT(stats.min_elapsed_ms == 0.0);
    TEST_ASSERT(stats.max_elapsed_ms == 0.0);

    std::cout << "[PASS] test_statistics_empty\n";
}

void test_csv_generation() {
    const std::filesystem::path test_dir = "test_results_dir";
    std::filesystem::create_directories(test_dir);
    const auto csv_path = test_dir / "test_benchmark.csv";

    if (std::filesystem::exists(csv_path)) {
        std::filesystem::remove(csv_path);
    }

    std::vector<SingleRunResult> runs(2);
    runs[0].timestamp_utc = "2026-11-08T00:00:00Z";
    runs[0].workload_name = "baseline";
    runs[0].threads = 4;
    runs[0].operations_per_worker = 1000;
    runs[0].total_operations = 4000;
    runs[0].run_number = 1;
    runs[0].elapsed_ms = 12.345;
    runs[0].operations_per_second = 324017.8;
    runs[0].final_counter = 4000;
    runs[0].success = true;

    runs[1].timestamp_utc = "2026-11-08T00:00:01Z";
    runs[1].workload_name = "optimized";
    runs[1].threads = 4;
    runs[1].operations_per_worker = 1000;
    runs[1].total_operations = 4000;
    runs[1].run_number = 1;
    runs[1].elapsed_ms = 0.543;
    runs[1].operations_per_second = 7366482.5;
    runs[1].final_counter = 4000;
    runs[1].success = true;

    TEST_ASSERT(BenchmarkRunner::write_csv_report(csv_path, runs));
    TEST_ASSERT(std::filesystem::exists(csv_path));

    std::ifstream in(csv_path);
    std::string line;
    std::getline(in, line);
    TEST_ASSERT(line == "timestamp_utc,workload,threads,operations_per_worker,total_operations,"
                        "run_number,elapsed_ms,operations_per_second,final_counter,success");

    std::getline(in, line);
    TEST_ASSERT(line.find("baseline") != std::string::npos);
    TEST_ASSERT(line.find("true") != std::string::npos);

    std::getline(in, line);
    TEST_ASSERT(line.find("optimized") != std::string::npos);
    TEST_ASSERT(line.find("true") != std::string::npos);

    in.close();
    std::filesystem::remove_all(test_dir);
    std::cout << "[PASS] test_csv_generation\n";
}

void test_etw_lifecycle_events() {
    MockEtwProvider mock_etw;
    BenchmarkRunner runner(mock_etw);

    BenchmarkConfig config;
    config.mode = WorkloadMode::Baseline;
    config.threads = 2;
    config.operations_per_worker = 500;
    config.runs = 2;
    config.output_dir = "test_etw_output";

    ContendedWorkload workload;
    // 1 warm-up run + 2 measured runs = 3 workload runs total
    const auto result = runner.run_workload(workload, config, "baseline");

    TEST_ASSERT(result.all_succeeded);
    TEST_ASSERT(result.runs.size() == 2);

    // Warm-up (1) + 2 measured runs = 3 workload starts & completions
    TEST_ASSERT(mock_etw.workload_started_count == 3);
    TEST_ASSERT(mock_etw.workload_completed_count == 3);

    // Each run has 2 worker threads: 3 runs * 2 threads = 6 worker starts & completions
    TEST_ASSERT(mock_etw.worker_started_count == 6);
    TEST_ASSERT(mock_etw.worker_completed_count == 6);

    // Measured runs: 2 benchmark run completion events
    TEST_ASSERT(mock_etw.benchmark_run_completed_count == 2);

    std::filesystem::remove_all("test_etw_output");
    std::cout << "[PASS] test_etw_lifecycle_events\n";
}

} // namespace perf_lab::test

int main() {
    using namespace perf_lab::test;
    test_statistics_odd_runs();
    test_statistics_even_runs();
    test_statistics_empty();
    test_csv_generation();
    test_etw_lifecycle_events();

    std::cout << "All benchmark tests passed successfully.\n";
    return 0;
}
