# Architecture Specification

## 1. Executive Overview

The **Windows Performance Investigation Lab** is a high-performance, native Windows C++20 systems engineering testbed designed to demonstrate, measure, and analyze the architectural and hardware costs of multi-threaded lock contention. 

Rather than functioning as a synthetic monitoring dashboard, this project isolates the fundamental mechanics of operating system synchronization, cache coherence bus invalidation, context switching, and event tracing via Windows Performance Toolkit (WPT).

```
+-----------------------------------------------------------------------------------+
|                                 perf_lab.exe                                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  +--------------------+   +-----------------------+   +------------------------+  |
|  |     CLI Parser     |   |   Benchmark Runner    |   |     ETW Provider       |  |
|  | (Strict Validation)|   | (Warmup, Stats, CSV)  |   | (TraceLoggingProvider) |  |
|  +---------+----------+   +-----------+-----------+   +-----------+------------+  |
|            |                          |                               |           |
|            +--------------------------+-------------------------------+           |
|                                       |                                           |
|                  +--------------------+--------------------+                      |
|                  |                                         |                      |
|                  v                                         v                      |
|      +-----------------------+                 +------------------------+         |
|      |  ContendedWorkload    |                 |   OptimizedWorkload    |         |
|      | (Baseline Mutex)      |                 |  (Thread-Local Stack)  |         |
|      +-----------+-----------+                 +-----------+------------+         |
+------------------|-----------------------------------------|----------------------+
                   |                                         |
                   v                                         v
       +-----------------------+                 +------------------------+
       | Multi-Threaded Hot-   |                 | Independent Partitioned|
       | Path Mutex Lock       |                 | Local In-Stack Loops   |
       +-----------+-----------+                 +-----------+------------+
                   |                                         |
                   v                                         v
       +-----------------------+                 +------------------------+
       | Shared Cache Line     |                 | L1-Local Accesses      |
       | Invalidation (MESI)   |                 | (Zero Bus Contention)  |
       +-----------+-----------+                 +-----------+------------+
                   |                                         |
                   v                                         v
       +-----------------------+                 +------------------------+
       | Kernel Context Switch |                 | Final Sequential       |
       | (KiCommitThreadWait)  |                 | Vector Aggregation     |
       +-----------------------+                 +------------------------+
```

---

## 2. Workload Architecture

### 2.1 Baseline Workload (`ContendedWorkload`)

The baseline workload deliberately forces multi-threaded lock contention on a shared memory location to reproduce real-world synchronization bottlenecks.

```
Worker 0 ----+
Worker 1 ----+
             |---> [ std::mutex m_mutex ] ---> [ Shared Counter: uint64_t ]
Worker 2 ----+
Worker N ----+
```

#### Execution Mechanics
1. **Thread Launch Synchronization**: All $N$ worker threads synchronize at a C++20 `std::latch` barrier initialized with $N + 1$. This eliminates thread initialization skew, ensuring every thread starts contending at the exact same physical time.
2. **Hot Loop Execution**: Each thread performs $M$ iterations. Within each iteration, the worker acquires a shared `std::mutex` via `std::lock_guard`, increments a single 64-bit counter `uint64_t shared_counter`, and releases the lock.
3. **Hardware Level (MESI/MOESI Protocol)**:
   - Modifying `shared_counter` requires the core to obtain **Exclusive (E)** or **Modified (M)** cache-line ownership.
   - Cores continually send bus invalidation requests to other cores.
   - Cache lines bounce across L1/L2/L3 caches, generating severe memory bus pressure and pipeline stalls.
4. **Operating System Level (Windows Kernel)**:
   - When a thread fails to acquire the mutex in user space, MSVC's runtime transitions the thread to a kernel wait state (`WaitOnAddress` or `NtWaitForAlertByThreadId`).
   - The Windows scheduler enters `ntoskrnl.exe!KiCommitThreadWait`, moving the thread from `Running` to `Waiting`.
   - When the lock holder releases the mutex, the kernel transitions waiting threads to `Ready`, causing involuntary context switches (`CSwitch`), CPU pipeline flushes, and TLB invalidation.

---

### 2.2 Optimized Workload (`OptimizedWorkload`)

The optimized workload executes the exact same logical computation (identical number of total increments) while eliminating inter-thread contention.

```
Worker 0 ---> [ Thread-Local Counter on Worker 0 Stack ]
Worker 1 ---> [ Thread-Local Counter on Worker 1 Stack ]
Worker 2 ---> [ Thread-Local Counter on Worker 2 Stack ]
Worker N ---> [ Thread-Local Counter on Worker N Stack ]
                                |
                     (Join All Worker Threads)
                                |
                                v
               [ Sequential Reduction Step ] ---> Final Total
```

#### Execution Mechanics
1. **Thread-Local Isolation**: Each worker increments its own stack-allocated counter variable (`volatile uint64_t local_counter`). Because each thread possesses a private 1 MB stack in separate virtual memory pages, no two workers share a cache line.
2. **False Sharing Elimination**: Neither workers nor adjacent counters share 64-byte L1 cache lines, completely eliminating cache line bouncing.
3. **Volatile Loop Semantics**: Using `volatile` ensures the optimizing compiler emits genuine memory increment instructions rather than folding the loop into a single arithmetic constant ($O(1)$).
4. **Post-Join Reduction**: Once all worker threads are joined, the coordinator thread aggregates each partitioned result sequentially into `final_counter`. The final counter matches the baseline workload identically.

---

## 3. ETW Instrumentation Architecture

Event Tracing for Windows (ETW) provides low-overhead, kernel-buffered instrumentation. The application uses Microsoft's **TraceLogging** API (`TraceLoggingProvider.h`).

### 3.1 Provider Specification
- **Provider Name**: `Shivam.PerformanceInvestigationLab`
- **Provider GUID**: `{b0ee99b7-9835-4d51-933a-df6647dbd99f}`
- **Provider Registration**: RAII wrapper `EtwProvider` calls `TraceLoggingRegister` during startup and `TraceLoggingUnregister` during shutdown.
- **Fail-Safe Operation**: If ETW registration fails or the application runs without tracing active, execution proceeds without error.

```
                  +--------------------------------+
                  | Shivam.PerformanceInvestigation|
                  |             Lab                |
                  +---------------+----------------+
                                  |
         +------------------------+------------------------+
         |                        |                        |
         v                        v                        v
+------------------+     +------------------+     +------------------+
| WorkloadStarted  |     |  WorkerStarted   |     | WorkerCompleted  |
| - WorkloadMode   |     | - WorkloadMode   |     | - WorkloadMode   |
| - ThreadCount    |     | - WorkerId       |     | - WorkerId       |
| - OpsPerWorker   |     | - OpsPerWorker   |     | - OpsCompleted   |
| - TotalOps       |     +------------------+     | - ElapsedMs      |
+--------+---------+                              +------------------+
         |                                                 |
         +------------------------+------------------------+
                                  |
                                  v
                       +----------------------+
                       |  WorkloadCompleted   |
                       | - WorkloadMode       |
                       | - ThreadCount        |
                       | - TotalOps           |
                       | - FinalCounter       |
                       | - ElapsedMs          |
                       | - Success            |
                       +----------+-----------+
                                  |
                                  v
                       +----------------------+
                       |BenchmarkRunCompleted |
                       | - WorkloadMode       |
                       | - RunNumber          |
                       | - TotalRuns          |
                       | - ElapsedMs          |
                       | - OpsPerSec          |
                       | - Success            |
                       +----------------------+
```

### 3.2 Event Overhead Management
To prevent observer effects (where tracing itself slows down the hot execution loop), no ETW events are emitted inside the inner increment loop. Tracing is restricted exclusively to lifecycle boundaries:
1. `WorkloadStarted`: Emitted when the workload is initialized.
2. `WorkerStarted`: Emitted when an individual worker thread begins.
3. `WorkerCompleted`: Emitted when a worker finishes its loop.
4. `WorkloadCompleted`: Emitted after all workers are joined and verified.
5. `BenchmarkRunCompleted`: Emitted at the completion of each measured iteration.

---

## 4. WPR Profile Architecture (`PerformanceLab.wprp`)

The Windows Performance Recorder Profile coordinates system-wide kernel providers and user-mode TraceLogging events into a unified `.etl` trace.

### Key Components
1. **NT Kernel Logger (`SystemCollector`)**:
   - `SampledProfile`: Captures instruction pointer samples at 1 kHz to identify hot code paths.
   - `CSwitch`: Records every thread context switch with call stacks.
   - `ReadyThread`: Captures the thread that unblocked another thread, highlighting lock release events.
   - `ProcessThread` & `Loader`: Maps binary images, DLLs, and symbol addresses.
2. **User Session (`EventCollector`)**:
   - Captures `Shivam.PerformanceInvestigationLab` events matching GUID `b0ee99b7-9835-4d51-933a-df6647dbd99f`.
   - Correlates user-mode phases (warm-up, run start, worker lifecycle) directly with kernel scheduling events in WPA.

---

## 5. Memory and Concurrency Safety

1. **Deterministic Lifetime**: Threads are managed via RAII wrappers (`ManagedThread` / `std::jthread`), guaranteeing that threads are joined prior to destruction.
2. **Zero Leaked Handles**: Windows handles and kernel events are wrapped in RAII constructs; no raw Win32 thread handles or synchronization primitives are left dangling.
3. **No Dynamic Allocations in Hot Paths**: Inner loops perform zero heap allocations, ensuring that memory allocators (`malloc` / `HeapAlloc`) do not introduce confounding lock contention.
