#include "diagnostics/etw_provider.h"
#include "workload/contended_workload.h"
#include "workload/optimized_workload.h"

#include <cstdlib>
#include <iostream>

#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "[FAIL] Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while (false)

namespace perf_lab::test {

void test_baseline_single_thread() {
    NullEtwProvider etw;
    ContendedWorkload workload;
    constexpr uint32_t threads = 1;
    constexpr uint64_t ops = 10'000;

    const auto metrics = workload.run(threads, ops, etw);
    TEST_ASSERT(metrics.success);
    TEST_ASSERT(metrics.total_operations == 10'000);
    TEST_ASSERT(metrics.final_counter == 10'000);
    TEST_ASSERT(metrics.elapsed_ms > 0.0);
    TEST_ASSERT(metrics.operations_per_second > 0.0);
    std::cout << "[PASS] test_baseline_single_thread\n";
}

void test_optimized_single_thread() {
    NullEtwProvider etw;
    OptimizedWorkload workload;
    constexpr uint32_t threads = 1;
    constexpr uint64_t ops = 10'000;

    const auto metrics = workload.run(threads, ops, etw);
    TEST_ASSERT(metrics.success);
    TEST_ASSERT(metrics.total_operations == 10'000);
    TEST_ASSERT(metrics.final_counter == 10'000);
    TEST_ASSERT(metrics.elapsed_ms > 0.0);
    TEST_ASSERT(metrics.operations_per_second > 0.0);
    std::cout << "[PASS] test_optimized_single_thread\n";
}

void test_baseline_multi_thread() {
    NullEtwProvider etw;
    ContendedWorkload workload;
    constexpr uint32_t threads = 4;
    constexpr uint64_t ops = 5'000;
    constexpr uint64_t expected_total = static_cast<uint64_t>(threads) * ops;

    const auto metrics = workload.run(threads, ops, etw);
    TEST_ASSERT(metrics.success);
    TEST_ASSERT(metrics.total_operations == expected_total);
    TEST_ASSERT(metrics.final_counter == expected_total);
    TEST_ASSERT(metrics.elapsed_ms > 0.0);
    std::cout << "[PASS] test_baseline_multi_thread\n";
}

void test_optimized_multi_thread() {
    NullEtwProvider etw;
    OptimizedWorkload workload;
    constexpr uint32_t threads = 4;
    constexpr uint64_t ops = 5'000;
    constexpr uint64_t expected_total = static_cast<uint64_t>(threads) * ops;

    const auto metrics = workload.run(threads, ops, etw);
    TEST_ASSERT(metrics.success);
    TEST_ASSERT(metrics.total_operations == expected_total);
    TEST_ASSERT(metrics.final_counter == expected_total);
    TEST_ASSERT(metrics.elapsed_ms > 0.0);
    std::cout << "[PASS] test_optimized_multi_thread\n";
}

void test_logical_work_equivalence() {
    NullEtwProvider etw;
    ContendedWorkload baseline;
    OptimizedWorkload optimized;

    constexpr uint32_t threads = 6;
    constexpr uint64_t ops = 8'000;
    constexpr uint64_t expected_total = static_cast<uint64_t>(threads) * ops;

    const auto baseline_metrics = baseline.run(threads, ops, etw);
    const auto optimized_metrics = optimized.run(threads, ops, etw);

    TEST_ASSERT(baseline_metrics.success);
    TEST_ASSERT(optimized_metrics.success);
    TEST_ASSERT(baseline_metrics.final_counter == expected_total);
    TEST_ASSERT(optimized_metrics.final_counter == expected_total);
    TEST_ASSERT(baseline_metrics.final_counter == optimized_metrics.final_counter);
    TEST_ASSERT(baseline_metrics.total_operations == optimized_metrics.total_operations);
    std::cout << "[PASS] test_logical_work_equivalence\n";
}

void test_invalid_parameters() {
    NullEtwProvider etw;
    ContendedWorkload baseline;
    OptimizedWorkload optimized;

    auto res1 = baseline.run(0, 100, etw);
    TEST_ASSERT(!res1.success);

    auto res2 = baseline.run(4, 0, etw);
    TEST_ASSERT(!res2.success);

    auto res3 = optimized.run(0, 100, etw);
    TEST_ASSERT(!res3.success);

    auto res4 = optimized.run(4, 0, etw);
    TEST_ASSERT(!res4.success);

    std::cout << "[PASS] test_invalid_parameters\n";
}

} // namespace perf_lab::test

int main() {
    using namespace perf_lab::test;
    test_baseline_single_thread();
    test_optimized_single_thread();
    test_baseline_multi_thread();
    test_optimized_multi_thread();
    test_logical_work_equivalence();
    test_invalid_parameters();

    std::cout << "All workload tests passed successfully.\n";
    return 0;
}
