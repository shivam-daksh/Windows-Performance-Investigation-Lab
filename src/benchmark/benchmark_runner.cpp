#include "benchmark/benchmark_runner.h"
#include "workload/contended_workload.h"
#include "workload/optimized_workload.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <thread>

namespace perf_lab {

AggregatedStatistics calculate_statistics(const std::vector<SingleRunResult>& runs) {
    AggregatedStatistics stats{};
    if (runs.empty()) {
        return stats;
    }

    std::vector<double> elapsed_times;
    std::vector<double> throughputs;
    elapsed_times.reserve(runs.size());
    throughputs.reserve(runs.size());

    double sum_elapsed = 0.0;
    double sum_throughput = 0.0;

    for (const auto& run : runs) {
        elapsed_times.push_back(run.elapsed_ms);
        throughputs.push_back(run.operations_per_second);
        sum_elapsed += run.elapsed_ms;
        sum_throughput += run.operations_per_second;
    }

    const double count = static_cast<double>(runs.size());
    stats.mean_elapsed_ms = sum_elapsed / count;
    stats.mean_ops_per_second = sum_throughput / count;

    std::sort(elapsed_times.begin(), elapsed_times.end());
    std::sort(throughputs.begin(), throughputs.end());

    stats.min_elapsed_ms = elapsed_times.front();
    stats.max_elapsed_ms = elapsed_times.back();

    const size_t n = elapsed_times.size();
    if (n % 2 == 1) {
        stats.median_elapsed_ms = elapsed_times[n / 2];
        stats.median_ops_per_second = throughputs[n / 2];
    } else {
        stats.median_elapsed_ms = (elapsed_times[(n / 2) - 1] + elapsed_times[n / 2]) / 2.0;
        stats.median_ops_per_second = (throughputs[(n / 2) - 1] + throughputs[n / 2]) / 2.0;
    }

    return stats;
}

std::string BenchmarkRunner::format_utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};

#if defined(_WIN32)
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

BenchmarkRunner::BenchmarkRunner(IEtwProvider& etw_provider)
    : m_etw(etw_provider) {}

WorkloadBenchmarkResult BenchmarkRunner::run_workload(IWorkload& workload,
                                                      const BenchmarkConfig& config,
                                                      std::string_view mode_tag) {
    WorkloadBenchmarkResult result{};
    result.workload_name = std::string(workload.name());
    result.threads = config.threads;
    result.operations_per_worker = config.operations_per_worker;
    result.total_operations = static_cast<uint64_t>(config.threads) * config.operations_per_worker;
    result.runs.reserve(config.runs);

    std::cout << "\n>>> Starting workload: " << workload.name()
              << " (" << config.threads << " threads, "
              << config.operations_per_worker << " ops/thread)\n";

    // 1. Warm-up execution (not recorded in benchmark dataset)
    std::cout << "    [1/2] Executing warm-up run... " << std::flush;
    const auto warmup_metrics = workload.run(config.threads, config.operations_per_worker, m_etw);
    if (!warmup_metrics.success) {
        std::cerr << "FAILED! (" << warmup_metrics.error_message << ")\n";
        result.all_succeeded = false;
        return result;
    }
    std::cout << "Done (" << std::fixed << std::setprecision(2)
              << warmup_metrics.elapsed_ms << " ms).\n";

    // 2. Measured benchmark runs
    std::cout << "    [2/2] Executing " << config.runs << " measured run(s)...\n";
    bool all_ok = true;

    for (uint32_t run_idx = 1; run_idx <= config.runs; ++run_idx) {
        const std::string timestamp = format_utc_timestamp();
        const auto metrics = workload.run(config.threads, config.operations_per_worker, m_etw);

        SingleRunResult run_res{};
        run_res.timestamp_utc = timestamp;
        run_res.workload_name = std::string(mode_tag);
        run_res.threads = config.threads;
        run_res.operations_per_worker = config.operations_per_worker;
        run_res.total_operations = metrics.total_operations;
        run_res.run_number = run_idx;
        run_res.elapsed_ms = metrics.elapsed_ms;
        run_res.operations_per_second = metrics.operations_per_second;
        run_res.final_counter = metrics.final_counter;
        run_res.success = metrics.success;
        run_res.error_message = metrics.error_message;

        m_etw.log_benchmark_run_completed(mode_tag, run_idx, config.runs,
                                          metrics.elapsed_ms, metrics.operations_per_second,
                                          metrics.success);

        std::cout << "        Run " << run_idx << "/" << config.runs << ": "
                  << std::fixed << std::setprecision(2) << metrics.elapsed_ms << " ms | "
                  << std::setprecision(0) << metrics.operations_per_second << " ops/sec "
                  << (metrics.success ? "[OK]" : "[FAIL]") << "\n";

        if (!metrics.success) {
            all_ok = false;
            std::cerr << "        Error in run " << run_idx << ": "
                      << metrics.error_message << "\n";
        }

        result.runs.push_back(std::move(run_res));
    }

    result.all_succeeded = all_ok;
    result.stats = calculate_statistics(result.runs);
    return result;
}

bool BenchmarkRunner::write_csv_report(const std::filesystem::path& file_path,
                                       const std::vector<SingleRunResult>& all_runs) {
    const bool file_exists = std::filesystem::exists(file_path);
    std::ofstream out(file_path, std::ios::app);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Failed to open CSV file for writing: " << file_path << "\n";
        return false;
    }

    if (!file_exists || std::filesystem::file_size(file_path) == 0) {
        out << "timestamp_utc,workload,threads,operations_per_worker,total_operations,"
               "run_number,elapsed_ms,operations_per_second,final_counter,success\n";
    }

    for (const auto& r : all_runs) {
        out << r.timestamp_utc << ","
            << r.workload_name << ","
            << r.threads << ","
            << r.operations_per_worker << ","
            << r.total_operations << ","
            << r.run_number << ","
            << std::fixed << std::setprecision(3) << r.elapsed_ms << ","
            << std::setprecision(2) << r.operations_per_second << ","
            << r.final_counter << ","
            << (r.success ? "true" : "false") << "\n";
    }

    return true;
}

bool BenchmarkRunner::write_summary_report(const std::filesystem::path& file_path,
                                           const BenchmarkConfig& config,
                                           const BenchmarkSuiteResult& suite_result) {
    std::ofstream out(file_path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Failed to open summary file for writing: " << file_path << "\n";
        return false;
    }

    out << "================================================================================\n";
    out << "                 WINDOWS PERFORMANCE INVESTIGATION LAB - SUMMARY\n";
    out << "================================================================================\n";
    out << "Timestamp UTC       : " << format_utc_timestamp() << "\n";
    out << "Workload Mode       : " << to_string(config.mode) << "\n";
    out << "Thread Count        : " << config.threads << "\n";
    out << "Operations / Worker : " << config.operations_per_worker << "\n";
    out << "Total Logical Ops   : " << (static_cast<uint64_t>(config.threads) * config.operations_per_worker) << "\n";
    out << "Measured Runs       : " << config.runs << "\n";
    out << "Overall Status      : " << (suite_result.overall_success ? "PASSED" : "FAILED") << "\n";
    out << "--------------------------------------------------------------------------------\n\n";

    out << std::left << std::setw(32) << "Workload"
        << std::right << std::setw(14) << "Mean (ms)"
        << std::setw(14) << "Median (ms)"
        << std::setw(14) << "Min (ms)"
        << std::setw(14) << "Max (ms)"
        << std::setw(18) << "Mean Ops/Sec"
        << std::setw(18) << "Median Ops/Sec"
        << "\n";
    out << std::string(124, '-') << "\n";

    const WorkloadBenchmarkResult* baseline_res = nullptr;
    const WorkloadBenchmarkResult* optimized_res = nullptr;

    for (const auto& w : suite_result.workload_results) {
        if (w.workload_name.find("Baseline") != std::string::npos) {
            baseline_res = &w;
        } else if (w.workload_name.find("Optimized") != std::string::npos) {
            optimized_res = &w;
        }

        out << std::left << std::setw(32) << w.workload_name
            << std::right << std::fixed << std::setprecision(2)
            << std::setw(14) << w.stats.mean_elapsed_ms
            << std::setw(14) << w.stats.median_elapsed_ms
            << std::setw(14) << w.stats.min_elapsed_ms
            << std::setw(14) << w.stats.max_elapsed_ms
            << std::setw(18) << std::setprecision(0) << w.stats.mean_ops_per_second
            << std::setw(18) << w.stats.median_ops_per_second
            << "\n";
    }

    out << std::string(124, '-') << "\n\n";

    if (baseline_res && optimized_res && baseline_res->all_succeeded && optimized_res->all_succeeded) {
        out << "COMPARATIVE ANALYSIS:\n";
        out << "--------------------------------------------------------------------------------\n";
        if (baseline_res->stats.mean_elapsed_ms > 0.0 && optimized_res->stats.mean_elapsed_ms > 0.0) {
            const double speedup = baseline_res->stats.mean_elapsed_ms / optimized_res->stats.mean_elapsed_ms;
            const double throughput_mult = (baseline_res->stats.mean_ops_per_second > 0.0)
                ? (optimized_res->stats.mean_ops_per_second / baseline_res->stats.mean_ops_per_second)
                : 0.0;
            const double latency_reduction_pct =
                ((baseline_res->stats.mean_elapsed_ms - optimized_res->stats.mean_elapsed_ms) /
                 baseline_res->stats.mean_elapsed_ms) * 100.0;

            out << "Throughput Speedup Factor : " << std::fixed << std::setprecision(2) << throughput_mult << "x\n";
            out << "Latency Reduction         : " << std::fixed << std::setprecision(2) << latency_reduction_pct << "%\n";
            out << "Execution Time Ratio      : " << std::fixed << std::setprecision(2) << speedup << "x faster\n";
        }
        out << "\nNote: All values are derived from actual system clock measurements taken during\n"
               "this benchmark run without artificial extrapolation.\n";
    }

    out << "================================================================================\n";
    return true;
}

BenchmarkSuiteResult BenchmarkRunner::execute(const BenchmarkConfig& config) {
    BenchmarkSuiteResult suite_result{};
    suite_result.overall_success = true;

    BenchmarkConfig active_config = config;
    if (active_config.threads == 0) {
        const unsigned int hw = std::thread::hardware_concurrency();
        active_config.threads = (hw > 0) ? hw : 4;
    }

    std::error_code ec;
    std::filesystem::create_directories(active_config.output_dir, ec);
    if (ec) {
        std::cerr << "[ERROR] Could not create output directory: " << ec.message() << "\n";
        suite_result.overall_success = false;
        return suite_result;
    }

    std::vector<SingleRunResult> all_runs;

    if (active_config.mode == WorkloadMode::Baseline || active_config.mode == WorkloadMode::Compare) {
        ContendedWorkload contended;
        auto result = run_workload(contended, active_config, "baseline");
        if (!result.all_succeeded) {
            suite_result.overall_success = false;
        }
        for (const auto& r : result.runs) {
            all_runs.push_back(r);
        }
        suite_result.workload_results.push_back(std::move(result));
    }

    if (active_config.mode == WorkloadMode::Optimized || active_config.mode == WorkloadMode::Compare) {
        OptimizedWorkload optimized;
        auto result = run_workload(optimized, active_config, "optimized");
        if (!result.all_succeeded) {
            suite_result.overall_success = false;
        }
        for (const auto& r : result.runs) {
            all_runs.push_back(r);
        }
        suite_result.workload_results.push_back(std::move(result));
    }

    const auto csv_file = active_config.output_dir / "benchmark_results.csv";
    const auto summary_file = active_config.output_dir / "benchmark_summary.txt";

    suite_result.csv_path = csv_file.string();
    suite_result.summary_path = summary_file.string();

    write_csv_report(csv_file, all_runs);
    write_summary_report(summary_file, active_config, suite_result);

    // Print quick summary to stdout
    std::cout << "\n================================================================================\n";
    std::cout << "BENCHMARK COMPLETE\n";
    std::cout << "CSV Results saved to    : " << csv_file << "\n";
    std::cout << "Summary Report saved to : " << summary_file << "\n";
    std::cout << "================================================================================\n";

    return suite_result;
}

} // namespace perf_lab
