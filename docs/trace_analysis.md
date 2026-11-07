# Windows Performance Analyzer (WPA) Trace Analysis Guide

## 1. Introduction

This guide describes how to analyze performance traces captured by `scripts/capture_trace.ps1` using **Windows Performance Analyzer (WPA)**.

By inspecting Event Tracing for Windows (ETW) traces, systems engineers can visualize thread scheduling, quantify lock contention, observe kernel context switches, and confirm the exact timeline of application events.

---

## 2. Setting Up Windows Performance Analyzer (WPA)

### Prerequisites
- Install the **Windows Performance Toolkit (WPT)** included in the [Windows Assessment and Deployment Kit (Windows ADK)](https://learn.microsoft.com/en-us/windows-hardware/get-started/adk-install).
- Ensure `wpa.exe` is available.
- Configure symbols in WPA for call stack resolution:
  - In WPA: Go to `Trace` -> `Configure Symbol Paths`.
  - Add Microsoft Public Symbol Server: `srv*C:\Symbols*https://msdl.microsoft.com/download/symbols`.
  - Add the build directory containing `perf_lab.pdb`: `C:\path\to\Windows-Performance-Investigation-Lab\build\Release`.
  - Check `Load Symbols` (`Trace` -> `Load Symbols`).

---

## 3. Key WPA Views and Metrics

### 3.1 CPU Usage (Sampled)
- **Path**: `Computation` -> `CPU Usage (Sampled)`.
- **Mechanism**: The Windows kernel timer fires periodically (default: 1 kHz / 1 ms per sample) and records the instruction pointer and call stack of the active thread on each CPU core.
- **What to look for**:
  - In CPU-bound workloads, high sample counts cluster within computational loops.
  - In lock contention, sample counts may be lower than expected because threads spend most of their time asleep waiting for locks rather than executing instructions.

### 3.2 CPU Usage (Precise / Context Switches)
- **Path**: `Computation` -> `CPU Usage (Precise)`.
- **Mechanism**: Captures an event on **every single thread context switch**. This is the primary graph for diagnosing synchronization bottlenecks.
- **Key Columns**:
  - `New Process` / `New ThreadId`: The thread being switched **in**.
  - `Readying Process` / `Readying ThreadId`: The thread that unblocked the new thread (e.g., the lock releaser).
  - `Wait (us)`: Microseconds the thread spent asleep in a wait state before being awakened.
  - `Time Since Last (us)`: Microseconds the thread spent running on the CPU core.
  - `Count`: Number of context switches.

### 3.3 Application ETW Events (Generic Events)
- **Path**: `System Activity` -> `Generic Events`.
- **Filter**: Set `Provider Name` to `Shivam.PerformanceInvestigationLab`.
- **What to verify**:
  - `WorkloadStarted`: Marks the physical start of the workload.
  - `WorkerStarted`: Shows when each worker began execution.
  - `WorkerCompleted`: Shows when each worker finished its hot loop.
  - `WorkloadCompleted`: Shows completion metrics and counter validation.
  - `BenchmarkRunCompleted`: Confirms execution time and operations per second.

---

## 4. Distinguishing Performance Characteristics

| Symptom | CPU-Bound Execution | Lock Contention | Excessive Scheduling Overhead | Idle Waiting |
| :--- | :--- | :--- | :--- | :--- |
| **CPU Utilization** | High (near 100% across all worker cores) | Low to moderate (threads frequently sleeping) | Moderate (time lost in kernel scheduler) | Near zero |
| **Context Switch Rate** | Very low (rarely yields quantum) | Very high (thousands to millions per second) | Extremely high | Low |
| **Wait State Function** | None (active execution) | `ntoskrnl.exe!KiCommitThreadWait`, `WaitOnAddress`, `RtlpWaitOnCriticalSection` | `KiQuantumEnd`, `KiCommitThreadWait` | `NtWaitForSingleObject`, `NtDelayExecution` |
| **Instruction / Cycle (IPC)** | High | Low (pipeline flushes, cache invalidation) | Low | N/A |
| **Scaling with Cores** | Near linear | Negative scaling (more cores = more contention!) | Degraded | N/A |

---

## 5. Step-by-Step Investigation Procedure

### Phase A: Capture Baseline Workload
1. Open PowerShell with Administrator privileges (required for kernel ETW capture).
2. Execute:
   ```powershell
   .\scripts\capture_trace.ps1 -Mode baseline -Threads 8 -Operations 1000000
   ```
3. Verify that `results\trace_baseline.etl` is generated.

### Phase B: Open Baseline ETL in WPA
1. Launch WPA:
   ```cmd
   wpa.exe results\trace_baseline.etl
   ```
2. In the Graph Explorer:
   - Expand `System Activity` and drag `Generic Events` to the Analysis tab.
   - Expand `Computation` and drag `CPU Usage (Precise)` to the Analysis tab.
   - Expand `Computation` and drag `CPU Usage (Sampled)` to the Analysis tab.

### Phase C: Identify Baseline Contention Indicators
1. In `Generic Events`:
   - Filter by Provider `Shivam.PerformanceInvestigationLab`.
   - Highlight the interval between `WorkloadStarted` and `WorkloadCompleted`.
   - Right-click and choose **Zoom**.
2. In `CPU Usage (Precise)`:
   - Group by: `New Process` -> `New ThreadId` -> `Wait (us) [Sum]` -> `Count`.
   - Notice the high context switch count for `perf_lab.exe`.
   - Inspect the call stack for the wait transition: notice calls through `std::mutex::lock()` -> `ntdll.dll!RtlpWaitOnCriticalSection` or `WaitOnAddress` -> `ntoskrnl.exe!KiCommitThreadWait`.
   - Observe that worker threads spend significant wall time in `Waiting` state.

### Phase D: Capture Optimized Workload
1. In the same environment, capture the optimized trace:
   ```powershell
   .\scripts\capture_trace.ps1 -Mode optimized -Threads 8 -Operations 1000000
   ```
2. Verify that `results\trace_optimized.etl` is generated.

### Phase E: Compare Corresponding Trace Graphs
1. Open `results\trace_optimized.etl` in a separate WPA window.
2. In `Generic Events`:
   - Zoom into the `WorkloadStarted` to `WorkloadCompleted` interval.
   - Notice the dramatic reduction in total elapsed time on the timeline.
3. In `CPU Usage (Precise)`:
   - Notice that context switches for worker threads drop to near zero during the hot loop!
   - Each thread runs continuously on its assigned CPU core without involuntary preemption or blocking.
4. In `CPU Usage (Sampled)`:
   - Notice full CPU core utilization during the active phase.

### Phase F: Relate Trace Findings to Benchmark Numbers
1. Compare the timeline durations in WPA with the values in `results/benchmark_results.csv`.
2. Notice how the elimination of context switching directly accounts for the orders-of-magnitude increase in operations per second.

### Phase G: Record Conclusions
1. Document your empirical findings in `docs/findings_template.md`.
2. Clearly record your machine's hardware profile (CPU, physical cores, logical processors, RAM, Windows build).
