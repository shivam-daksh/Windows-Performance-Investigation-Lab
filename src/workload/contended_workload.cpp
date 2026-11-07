#include "workload/contended_workload.h"
#include "common/thread_compat.h"

#include <chrono>
#include <latch>
#include <mutex>
#include <vector>

namespace perf_lab {

WorkloadMetrics ContendedWorkload::run(uint32_t threads,
                                      uint64_t operations_per_worker,
                                      IEtwProvider& etw) {
    WorkloadMetrics metrics{};
    const uint64_t total_ops = static_cast<uint64_t>(threads) * operations_per_worker;
    metrics.total_operations = total_ops;

    if (threads == 0 || operations_per_worker == 0) {
        metrics.success = false;
        metrics.error_message = "Invalid configuration: threads and operations must be greater than zero.";
        return metrics;
    }

    etw.log_workload_started("baseline", threads, operations_per_worker, total_ops);

    std::mutex counter_mutex;
    uint64_t shared_counter{0};

    // Latch used to synchronize the launch of all worker threads so they begin
    // contending on the mutex simultaneously.
    std::latch start_latch(threads + 1);

    std::vector<ManagedThread> workers;
    workers.reserve(threads);

    for (uint32_t id = 0; id < threads; ++id) {
        workers.emplace_back([&, worker_id = id]() {
            etw.log_worker_started("baseline", worker_id, operations_per_worker);

            // Wait for all workers and the coordinator to reach this point
            start_latch.arrive_and_wait();

            const auto worker_start = std::chrono::steady_clock::now();

            // Hot loop: every iteration acquires the shared mutex, modifying
            // a single shared cache line and causing cache line bouncing,
            // thread preemption, and kernel synchronization waits.
            for (uint64_t i = 0; i < operations_per_worker; ++i) {
                std::lock_guard<std::mutex> lock(counter_mutex);
                ++shared_counter;
            }

            const auto worker_end = std::chrono::steady_clock::now();
            const double worker_elapsed_ms =
                std::chrono::duration<double, std::milli>(worker_end - worker_start).count();

            etw.log_worker_completed("baseline", worker_id, operations_per_worker, worker_elapsed_ms);
        });
    }

    // Release all workers simultaneously and begin timing
    const auto workload_start = std::chrono::steady_clock::now();
    start_latch.arrive_and_wait();

    // Join all workers
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    const auto workload_end = std::chrono::steady_clock::now();

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(workload_end - workload_start).count();

    metrics.elapsed_ms = elapsed_ms;
    metrics.final_counter = shared_counter;
    metrics.operations_per_second =
        (elapsed_ms > 0.0) ? (static_cast<double>(total_ops) / (elapsed_ms / 1000.0)) : 0.0;

    if (shared_counter == total_ops) {
        metrics.success = true;
    } else {
        metrics.success = false;
        metrics.error_message = "Counter mismatch: expected " + std::to_string(total_ops) +
                                ", but observed " + std::to_string(shared_counter);
    }

    etw.log_workload_completed("baseline", threads, total_ops, metrics.final_counter,
                               metrics.elapsed_ms, metrics.success);

    return metrics;
}

} // namespace perf_lab
