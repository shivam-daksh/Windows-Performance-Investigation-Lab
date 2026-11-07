#include "diagnostics/etw_provider.h"

#include <iostream>
#include <string>

#if defined(_WIN32)

// Define the TraceLogging provider with unique GUID:
// {b0ee99b7-9835-4d51-933a-df6647dbd99f}
TRACELOGGING_DEFINE_PROVIDER(
    g_hPerfLabProvider,
    "Shivam.PerformanceInvestigationLab",
    (0xb0ee99b7, 0x9835, 0x4d51, 0x93, 0x3a, 0xdf, 0x66, 0x47, 0xdb, 0xd9, 0x9f));

#endif

namespace perf_lab {

EtwProvider::EtwProvider() {
#if defined(_WIN32)
    const HRESULT hr = TraceLoggingRegister(g_hPerfLabProvider);
    if (SUCCEEDED(hr)) {
        m_registered = true;
    } else {
        std::cerr << "[WARN] ETW TraceLogging registration failed with HRESULT 0x"
                  << std::hex << hr << std::dec << ". Tracing disabled.\n";
        m_registered = false;
    }
#else
    // On non-Windows platforms (e.g. macOS during development/testing),
    // ETW is not supported by the OS.
    m_registered = false;
#endif
}

EtwProvider::~EtwProvider() {
#if defined(_WIN32)
    if (m_registered) {
        TraceLoggingUnregister(g_hPerfLabProvider);
        m_registered = false;
    }
#endif
}

EtwProvider::EtwProvider(EtwProvider&& other) noexcept
    : m_registered(other.m_registered) {
    other.m_registered = false;
}

EtwProvider& EtwProvider::operator=(EtwProvider&& other) noexcept {
    if (this != &other) {
#if defined(_WIN32)
        if (m_registered) {
            TraceLoggingUnregister(g_hPerfLabProvider);
        }
#endif
        m_registered = other.m_registered;
        other.m_registered = false;
    }
    return *this;
}

bool EtwProvider::is_registered() const noexcept {
    return m_registered;
}

void EtwProvider::log_workload_started(std::string_view workload_mode,
                                      uint32_t thread_count,
                                      uint64_t ops_per_worker,
                                      uint64_t total_ops) {
#if defined(_WIN32)
    if (m_registered) {
        const std::string mode_str(workload_mode);
        TraceLoggingWrite(
            g_hPerfLabProvider,
            "WorkloadStarted",
            TraceLoggingString(mode_str.c_str(), "WorkloadMode"),
            TraceLoggingUInt32(thread_count, "ThreadCount"),
            TraceLoggingUInt64(ops_per_worker, "OperationsPerWorker"),
            TraceLoggingUInt64(total_ops, "TotalOperations")
        );
    }
#else
    (void)workload_mode;
    (void)thread_count;
    (void)ops_per_worker;
    (void)total_ops;
#endif
}

void EtwProvider::log_worker_started(std::string_view workload_mode,
                                    uint32_t worker_id,
                                    uint64_t ops_per_worker) {
#if defined(_WIN32)
    if (m_registered) {
        const std::string mode_str(workload_mode);
        TraceLoggingWrite(
            g_hPerfLabProvider,
            "WorkerStarted",
            TraceLoggingString(mode_str.c_str(), "WorkloadMode"),
            TraceLoggingUInt32(worker_id, "WorkerId"),
            TraceLoggingUInt64(ops_per_worker, "OperationsPerWorker")
        );
    }
#else
    (void)workload_mode;
    (void)worker_id;
    (void)ops_per_worker;
#endif
}

void EtwProvider::log_worker_completed(std::string_view workload_mode,
                                      uint32_t worker_id,
                                      uint64_t ops_completed,
                                      double elapsed_ms) {
#if defined(_WIN32)
    if (m_registered) {
        const std::string mode_str(workload_mode);
        TraceLoggingWrite(
            g_hPerfLabProvider,
            "WorkerCompleted",
            TraceLoggingString(mode_str.c_str(), "WorkloadMode"),
            TraceLoggingUInt32(worker_id, "WorkerId"),
            TraceLoggingUInt64(ops_completed, "OperationsCompleted"),
            TraceLoggingFloat64(elapsed_ms, "ElapsedMilliseconds")
        );
    }
#else
    (void)workload_mode;
    (void)worker_id;
    (void)ops_completed;
    (void)elapsed_ms;
#endif
}

void EtwProvider::log_workload_completed(std::string_view workload_mode,
                                        uint32_t thread_count,
                                        uint64_t total_ops,
                                        uint64_t final_counter,
                                        double elapsed_ms,
                                        bool success) {
#if defined(_WIN32)
    if (m_registered) {
        const std::string mode_str(workload_mode);
        TraceLoggingWrite(
            g_hPerfLabProvider,
            "WorkloadCompleted",
            TraceLoggingString(mode_str.c_str(), "WorkloadMode"),
            TraceLoggingUInt32(thread_count, "ThreadCount"),
            TraceLoggingUInt64(total_ops, "TotalOperations"),
            TraceLoggingUInt64(final_counter, "FinalCounter"),
            TraceLoggingFloat64(elapsed_ms, "ElapsedMilliseconds"),
            TraceLoggingBool(success, "Success")
        );
    }
#else
    (void)workload_mode;
    (void)thread_count;
    (void)total_ops;
    (void)final_counter;
    (void)elapsed_ms;
    (void)success;
#endif
}

void EtwProvider::log_benchmark_run_completed(std::string_view workload_mode,
                                             uint32_t run_number,
                                             uint32_t total_runs,
                                             double elapsed_ms,
                                             double ops_per_sec,
                                             bool success) {
#if defined(_WIN32)
    if (m_registered) {
        const std::string mode_str(workload_mode);
        TraceLoggingWrite(
            g_hPerfLabProvider,
            "BenchmarkRunCompleted",
            TraceLoggingString(mode_str.c_str(), "WorkloadMode"),
            TraceLoggingUInt32(run_number, "RunNumber"),
            TraceLoggingUInt32(total_runs, "TotalRuns"),
            TraceLoggingFloat64(elapsed_ms, "ElapsedMilliseconds"),
            TraceLoggingFloat64(ops_per_sec, "OperationsPerSecond"),
            TraceLoggingBool(success, "Success")
        );
    }
#else
    (void)workload_mode;
    (void)run_number;
    (void)total_runs;
    (void)elapsed_ms;
    (void)ops_per_sec;
    (void)success;
#endif
}

} // namespace perf_lab
