#include "workload/optimized_workload.h"
#include "common/thread_compat.h"

#include <chrono>
#include <latch>
#include <vector>

namespace perf_lab {

WorkloadMetrics OptimizedWorkload::run(uint32_t threads,
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

    etw.log_workload_started("optimized", threads, operations_per_worker, total_ops);

    // Vector to store each worker's local result upon completion.
    // Each worker writes exclusively to its own dedicated index after finishing
    // its hot loop, eliminating lock contention and false sharing.
    std::vector<uint64_t> worker_results(threads, 0ULL);

    // Latch used to synchronize the launch of all worker threads so they begin
    // execution at the exact same instant as in the baseline workload.
    std::latch start_latch(threads + 1);

    std::vector<ManagedThread> workers;
    workers.reserve(threads);

    for (uint32_t id = 0; id < threads; ++id) {
        workers.emplace_back([&, worker_id = id]() {
            etw.log_worker_started("optimized", worker_id, operations_per_worker);

            // Wait for all workers and coordinator to synchronize
            start_latch.arrive_and_wait();

            const auto worker_start = std::chrono::steady_clock::now();

            // Hot loop: increments are purely local to this thread's stack.
            // Using volatile prevents the compiler from optimizing out the loop
            // to a constant addition, guaranteeing that genuine increments occur
            // without requiring cross-core cache invalidation or locks.
            volatile uint64_t local_counter = 0;
            for (uint64_t i = 0; i < operations_per_worker; ++i) {
                local_counter = local_counter + 1;
            }

            // Store local result into the worker's partitioned output slot
            worker_results[worker_id] = local_counter;

            const auto worker_end = std::chrono::steady_clock::now();
            const double worker_elapsed_ms =
                std::chrono::duration<double, std::milli>(worker_end - worker_start).count();

            etw.log_worker_completed("optimized", worker_id, operations_per_worker, worker_elapsed_ms);
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

    // Final reduction of all thread-local counters
    uint64_t aggregated_counter = 0;
    for (const uint64_t val : worker_results) {
        aggregated_counter += val;
    }

    metrics.elapsed_ms = elapsed_ms;
    metrics.final_counter = aggregated_counter;
    metrics.operations_per_second =
        (elapsed_ms > 0.0) ? (static_cast<double>(total_ops) / (elapsed_ms / 1000.0)) : 0.0;

    if (aggregated_counter == total_ops) {
        metrics.success = true;
    } else {
        metrics.success = false;
        metrics.error_message = "Counter mismatch: expected " + std::to_string(total_ops) +
                                ", but observed " + std::to_string(aggregated_counter);
    }

    etw.log_workload_completed("optimized", threads, total_ops, metrics.final_counter,
                               metrics.elapsed_ms, metrics.success);

    return metrics;
}

} // namespace perf_lab
