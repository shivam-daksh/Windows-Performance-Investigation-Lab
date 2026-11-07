# Performance Investigation Findings Report

> **Note**: This template is designed for recording empirical benchmark and trace findings when running `perf_lab.exe` on a native Windows system with WPR and WPA. Do not fill this with synthetic or fabricated numbers; record only real experimental data.

---

## 1. System and Hardware Environment

| Attribute | Specification |
| :--- | :--- |
| **System Model** | *(e.g. Dell Precision 5820 / Custom Desktop)* |
| **Operating System** | *(e.g. Windows 11 Enterprise 23H2 Build 22631.3007)* |
| **Processor (CPU)** | *(e.g. AMD Ryzen 9 7950X / Intel Core i9-14900K)* |
| **Physical Cores / Threads** | *(e.g. 16 Cores / 32 Threads)* |
| **Base / Boost Frequency** | *(e.g. 4.5 GHz / 5.7 GHz)* |
| **L1 / L2 / L3 Cache** | *(e.g. 1 MB L1 / 16 MB L2 / 64 MB L3)* |
| **System Memory (RAM)** | *(e.g. 64 GB DDR5-6000)* |
| **Power Plan** | *(e.g. High Performance / Balanced)* |

---

## 2. Benchmark Parameters

- **Execution Command**:
  ```powershell
  .\build\Release\perf_lab.exe --mode compare --threads <N> --operations <M> --runs <R>
  ```
- **Thread Count**: `<N>`
- **Operations per Worker**: `<M>`
- **Total Logical Operations**: `<N x M>`
- **Measured Runs**: `<R>`

---

## 3. Measured Benchmark Results

*(Extracted directly from `results/benchmark_summary.txt` and `results/benchmark_results.csv`)*

| Metric | Baseline (Contended Mutex) | Optimized (Thread-Local) | Delta / Ratio |
| :--- | :--- | :--- | :--- |
| **Mean Execution Time (ms)** | | | |
| **Median Execution Time (ms)** | | | |
| **Min Execution Time (ms)** | | | |
| **Max Execution Time (ms)** | | | |
| **Mean Throughput (Ops/sec)**| | | |
| **Median Throughput (Ops/sec)**| | | |
| **Throughput Speedup** | *(e.g. 50x - 120x)* | | |
| **Latency Reduction (%)** | *(e.g. 98.5%)* | | |

---

## 4. WPA Trace Analysis Observations

### 4.1 CPU Usage (Precise / Context Switches)
- **Baseline Workload**:
  - Total Context Switch Count:
  - Context Switches per Second:
  - Primary Wait Functions Observed in Call Stack:
    - `ntoskrnl.exe!KiCommitThreadWait`
    - `ntdll.dll!WaitOnAddress` / `ntdll.dll!RtlpWaitOnCriticalSection`
  - Average Thread Wait Duration:
- **Optimized Workload**:
  - Total Context Switch Count:
  - Context Switches per Second:
  - Average Thread Wait Duration:
  - Comparison Note:

### 4.2 CPU Usage (Sampled)
- **Baseline Workload**:
  - CPU Utilization across worker cores:
  - Hotspot Functions:
- **Optimized Workload**:
  - CPU Utilization across worker cores:
  - Hotspot Functions:

### 4.3 ETW Application Events
- **Provider Name**: `Shivam.PerformanceInvestigationLab`
- **Provider GUID**: `{b0ee99b7-9835-4d51-933a-df6647dbd99f}`
- **Events Verified in Trace**:
  - [ ] `WorkloadStarted`
  - [ ] `WorkerStarted`
  - [ ] `WorkerCompleted`
  - [ ] `WorkloadCompleted`
  - [ ] `BenchmarkRunCompleted`

---

## 5. Architectural Conclusions

1. **Impact of Mutex Lock Contention**:
   *(Summarize how cache line bouncing, CPU pipeline stalls, and kernel context switches degraded throughput in the baseline workload.)*

2. **Benefits of Thread-Local Partitioning**:
   *(Explain how eliminating cross-thread synchronization and preventing false sharing allowed near-linear core scaling in the optimized workload.)*

3. **Limitations of Test Setup**:
   *(Note any system factors, such as thermal throttling, core pinning, hyperthreading / SMT effects, or background services that affected measurements.)*
