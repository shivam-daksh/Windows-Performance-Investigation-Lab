# Benchmark Methodology

## 1. Overview and Core Principles

Benchmarking concurrent systems on modern multi-core x86-64 hardware requires rigorous experimental controls. Microbenchmarks are susceptible to numerous confounding factors, including:
- Dynamic CPU frequency scaling (Intel Speed Shift / Turbo Boost, AMD Precision Boost).
- Operating system background interrupts (DPCs, ISRs, scheduler quantum exhaustion).
- Cold-start anomalies (cold cache lines, unmapped page faults, branch predictor cold starts).
- Thread initialization skew (worker 0 finishing before worker 7 starts).

This document outlines the engineering methodology employed by `perf_lab.exe` to guarantee reproducible, statistically valid measurements.

---

## 2. Timing and Precision Instrumentation

### 2.1 Clock Selection: `std::chrono::steady_clock`
All wall-clock measurements utilize `std::chrono::steady_clock`. On Windows platforms with the MSVC runtime, `std::chrono::steady_clock` maps directly to the high-resolution hardware abstraction layer:
- **Underlying Primitive**: `QueryPerformanceCounter` (QPC) and `QueryPerformanceFrequency`.
- **Hardware Source**: Invariant Time Stamp Counter (RDTSC/RDTSCP) or High Precision Event Timer (HPET).
- **Properties**:
  - **Monotonic**: The timer values never decrease, eliminating negative durations during NTP or daylight saving adjustments.
  - **High Resolution**: Sub-microsecond granularity (typically < 100 ns).
  - **Thread-Safe**: Invariant across multiple CPU sockets and physical cores.

`std::chrono::system_clock` is deliberately **not** used for duration measurement because it reflects wall-clock calendar time and is subject to system clock skew and leap second adjustments.

---

## 3. Mitigating Confounding Variables

### 3.1 Coordinated Thread Launch via Countdown Latch
A common flaw in multithreaded benchmarks is launching worker threads in a sequential loop:
```cpp
// FLAWED APPROACH: Thread 0 begins executing before Thread N is even spawned!
for (uint32_t i = 0; i < threads; ++i) {
    workers.emplace_back([&]() { do_work(); });
}
```
In this flawed pattern, earlier threads may complete significant portions of their work before later threads finish spawning, reducing observable contention.

**The Solution**:
The lab uses a C++20 `std::latch` initialized to `threads + 1`:
```cpp
std::latch start_latch(threads + 1);

for (uint32_t id = 0; id < threads; ++id) {
    workers.emplace_back([&]() {
        // Synchronize all workers before hot loop
        start_latch.arrive_and_wait();
        // Contention begins simultaneously
    });
}

// Coordinator releases all threads at the exact same instant
auto start = std::chrono::steady_clock::now();
start_latch.arrive_and_wait();
```
This guarantees that all worker threads enter the hot loop simultaneously, maximizing contention on the shared synchronization primitive.

---

### 3.2 Cold-Start Elimination (Warm-Up Run)
Before collecting measured benchmark iterations, the benchmark engine executes a dedicated **warm-up iteration**:
- **Purpose**:
  1. Promotes code and data structures into CPU L1/L2 caches.
  2. Resolves lazy page faults and memory mapping allocations.
  3. Transitions CPU cores from low-power C-states (e.g., C6/C7 sleep) into high-performance P-states (operating frequency).
  4. Primes hardware branch prediction buffers (BTB).
- **Data Integrity**: The warm-up run is logged but strictly excluded from the final statistical aggregation and CSV dataset.

---

### 3.3 Preventing Compiler Loop Elimination
Optimizing C++ compilers (`MSVC /O2`, `Clang -O3`) recognize pure loops that increment variables without external side effects and collapse them into closed-form arithmetic ($O(1)$):
```cpp
// An optimizing compiler would collapse this loop to: local_counter = operations;
for (uint64_t i = 0; i < operations; ++i) {
    ++local_counter;
}
```
In `ContendedWorkload`, the mutex acquisition prevents this optimization. However, in `OptimizedWorkload`, without synchronization, an optimizing compiler would eliminate the loop entirely, distorting the comparison between synchronization strategies.

**The Solution**:
In `OptimizedWorkload`, the thread-local counter is qualified with `volatile`:
```cpp
volatile uint64_t local_counter = 0;
for (uint64_t i = 0; i < operations_per_worker; ++i) {
    local_counter = local_counter + 1;
}
```
Under C++20 semantics, accesses to volatile variables constitute observable side effects. The compiler is legally prohibited from eliding or collapsing the iterations, guaranteeing that $M$ true increment cycles execute on the CPU while incurring zero mutex overhead.

---

## 4. Statistical Metrics and Aggregation

Because operating system scheduling introduces non-deterministic latency spikes, evaluating a single execution is insufficient. The engine executes $N$ measured runs (default: 5) and computes:

| Metric | Calculation | Rationale |
| :--- | :--- | :--- |
| **Mean Execution Time** | $\frac{1}{N}\sum t_i$ | Arithmetic average across all runs. |
| **Median Execution Time** | Middle value of sorted times | Resilient to OS scheduling interruptions and DPC spikes. |
| **Min Execution Time** | $\min(t_i)$ | Theoretical maximum performance under minimal OS interference. |
| **Max Execution Time** | $\max(t_i)$ | Worst-case tail latency under heavy background load. |
| **Throughput (Ops/sec)** | $\frac{\text{Total Operations}}{\text{Elapsed Seconds}}$ | Normalized processing capacity per second. |

### Why Median is the Primary Comparator
When benchmarking on general-purpose operating systems, background services (antivirus scans, Windows Update, network interrupts) occasionally preempt benchmark threads. These preemption events skew the **mean**. The **median** provides a robust estimator of central tendency that ignores single-run tail latency anomalies.

---

## 5. Result Verification and Failure Handling

To guarantee the validity of benchmark findings, the engine enforces strict verification:
1. **Total Logical Operations Check**:
   $$\text{Final Counter} \equiv \text{Threads} \times \text{Operations Per Worker}$$
   If any race condition, lost update, or arithmetic corruption occurs, the run is immediately marked as failed (`success = false`).
2. **Transparent Error Logging**: Failed runs are recorded in `benchmark_results.csv` with their corresponding error message and flagged as unsuccessful.
3. **No Fictional Numbers**: If an execution fails or cannot be measured, zero or error markers are returned; synthetic or extrapolated numbers are never generated.
