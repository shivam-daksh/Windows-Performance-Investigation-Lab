#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#include <TraceLoggingProvider.h>

// Declare the TraceLogging provider handle
TRACELOGGING_DECLARE_PROVIDER(g_hPerfLabProvider);
#endif

namespace perf_lab {

class IEtwProvider {
public:
    virtual ~IEtwProvider() = default;

    [[nodiscard]] virtual bool is_registered() const noexcept = 0;

    virtual void log_workload_started(std::string_view workload_mode,
                                      uint32_t thread_count,
                                      uint64_t ops_per_worker,
                                      uint64_t total_ops) = 0;

    virtual void log_worker_started(std::string_view workload_mode,
                                    uint32_t worker_id,
                                    uint64_t ops_per_worker) = 0;

    virtual void log_worker_completed(std::string_view workload_mode,
                                      uint32_t worker_id,
                                      uint64_t ops_completed,
                                      double elapsed_ms) = 0;

    virtual void log_workload_completed(std::string_view workload_mode,
                                        uint32_t thread_count,
                                        uint64_t total_ops,
                                        uint64_t final_counter,
                                        double elapsed_ms,
                                        bool success) = 0;

    virtual void log_benchmark_run_completed(std::string_view workload_mode,
                                             uint32_t run_number,
                                             uint32_t total_runs,
                                             double elapsed_ms,
                                             double ops_per_sec,
                                             bool success) = 0;
};

class EtwProvider final : public IEtwProvider {
public:
    EtwProvider();
    ~EtwProvider() override;

    EtwProvider(const EtwProvider&) = delete;
    EtwProvider& operator=(const EtwProvider&) = delete;

    EtwProvider(EtwProvider&& other) noexcept;
    EtwProvider& operator=(EtwProvider&& other) noexcept;

    [[nodiscard]] bool is_registered() const noexcept override;

    void log_workload_started(std::string_view workload_mode,
                              uint32_t thread_count,
                              uint64_t ops_per_worker,
                              uint64_t total_ops) override;

    void log_worker_started(std::string_view workload_mode,
                            uint32_t worker_id,
                            uint64_t ops_per_worker) override;

    void log_worker_completed(std::string_view workload_mode,
                              uint32_t worker_id,
                              uint64_t ops_completed,
                              double elapsed_ms) override;

    void log_workload_completed(std::string_view workload_mode,
                                uint32_t thread_count,
                                uint64_t total_ops,
                                uint64_t final_counter,
                                double elapsed_ms,
                                bool success) override;

    void log_benchmark_run_completed(std::string_view workload_mode,
                                     uint32_t run_number,
                                     uint32_t total_runs,
                                     double elapsed_ms,
                                     double ops_per_sec,
                                     bool success) override;

private:
    bool m_registered{false};
};

class NullEtwProvider final : public IEtwProvider {
public:
    [[nodiscard]] bool is_registered() const noexcept override {
        return false;
    }

    void log_workload_started(std::string_view, uint32_t, uint64_t, uint64_t) override {}
    void log_worker_started(std::string_view, uint32_t, uint64_t) override {}
    void log_worker_completed(std::string_view, uint32_t, uint64_t, double) override {}
    void log_workload_completed(std::string_view, uint32_t, uint64_t, uint64_t, double, bool) override {}
    void log_benchmark_run_completed(std::string_view, uint32_t, uint32_t, double, double, bool) override {}
};

} // namespace perf_lab
