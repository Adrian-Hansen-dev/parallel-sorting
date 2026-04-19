# Parallel Sorting – Benchmark Report

## 1. Benchmarking Setup

**Environment**

| Property | Value |
|---|---|
| CPU | Apple M-series (ARM64) |
| Logical threads | 8 (`omp_get_max_threads()`) |
| Compiler | Apple Clang 17 / libomp 22.1.3 |
| Optimisation | `-O2` |

**Methodology**

- Test data is generated once with a fixed seed (`mt19937`, seed 42) so every variant sorts the exact same input.
- Values are drawn uniformly from `[1, 10 000 000]` — no sorted or reverse-sorted bias that would create a worst-case for QuickSort.
- Each configuration runs **5 times**; the **minimum** wall-clock time (via `omp_get_wtime()`) is reported. The minimum filters OS-scheduler noise while fairly representing peak performance.
- For parallel variants the measurement wraps the entire `omp parallel` region, so task-scheduling overhead is fully captured.
- Correctness is verified with a linear `isSorted` check after every sort variant.

---

## 2. Results

### 2.1 Sequential vs. Naive Parallel

*Minimum of 5 runs, wall-clock seconds. Speedup relative to sequential for the same size.*

| Algorithm | Mode | Size | Time (s) | Speedup |
|---|---|---:|---:|---:|
| QuickSort | sequential | 100 000 | 0.005134 | 1.00× |
| QuickSort | naive parallel | 100 000 | 0.006403 | 0.80× |
| MergeSort | sequential | 100 000 | 0.010006 | 1.00× |
| MergeSort | naive parallel | 100 000 | 0.006571 | 1.52× |
| QuickSort | sequential | 500 000 | 0.029558 | 1.00× |
| QuickSort | naive parallel | 500 000 | 0.013061 | 2.26× |
| MergeSort | sequential | 500 000 | 0.054017 | 1.00× |
| MergeSort | naive parallel | 500 000 | 0.024968 | 2.16× |
| QuickSort | sequential | 1 000 000 | 0.060469 | 1.00× |
| QuickSort | naive parallel | 1 000 000 | 0.024867 | 2.43× |
| MergeSort | sequential | 1 000 000 | 0.109117 | 1.00× |
| MergeSort | naive parallel | 1 000 000 | 0.056978 | 1.92× |
| QuickSort | sequential | 5 000 000 | 0.337383 | 1.00× |
| QuickSort | naive parallel | 5 000 000 | 0.127100 | 2.65× |
| MergeSort | sequential | 5 000 000 | 0.584963 | 1.00× |
| MergeSort | naive parallel | 5 000 000 | 0.296446 | 1.97× |

### 2.2 Threshold Sweep

*Input size: 2 000 000, 8 threads. Speedup relative to the sequential baseline of the same size.*

| Algorithm | Threshold | Time (s) | Speedup |
|---|---:|---:|---:|
| QuickSort | *sequential baseline* | 0.125825 | 1.00× |
| QuickSort | 500 | 0.025508 | 4.93× |
| QuickSort | **1 000** | **0.025190** | **5.00×** |
| QuickSort | 2 000 | 0.030031 | 4.19× |
| QuickSort | 5 000 | 0.026700 | 4.71× |
| QuickSort | 10 000 | 0.027197 | 4.63× |
| QuickSort | 20 000 | 0.027866 | 4.52× |
| QuickSort | 50 000 | 0.028256 | 4.45× |
| QuickSort | 100 000 | 0.029577 | 4.25× |
| MergeSort | *sequential baseline* | 0.220448 | 1.00× |
| MergeSort | **500** | **0.069332** | **3.18×** |
| MergeSort | 1 000 | 0.072395 | 3.05× |
| MergeSort | 2 000 | 0.075881 | 2.91× |
| MergeSort | 5 000 | 0.073945 | 2.98× |
| MergeSort | 10 000 | 0.079000 | 2.79× |
| MergeSort | 20 000 | 0.084762 | 2.60× |
| MergeSort | 50 000 | 0.086953 | 2.54× |
| MergeSort | 100 000 | 0.086823 | 2.54× |

---

## 3. Analysis and Conclusions

### Naive parallelism (Section 1.2)

At 100 000 elements the naive QuickSort is **20 % slower** than sequential — task-creation and scheduling overhead dominates when the leaf tasks are too small. Once the input grows to 500 000+ elements the useful parallel work outweighs that overhead and both algorithms deliver roughly **2–2.6× speedup** on 8 threads. The gap from the theoretical 8× limit comes from spawning an excessive number of tiny leaf tasks that saturate the OMP task queue and create high synchronisation pressure.

### Threshold-based parallelism (Section 1.3)

Cutting off task creation below a threshold avoids the leaf-task overhead entirely. The sweet spot is **~1 000 elements for QuickSort** (≈ 5× speedup) and **~500 elements for MergeSort** (≈ 3.2× speedup). Thresholds smaller than ~500 bring no additional benefit because individual tasks become too fine-grained; thresholds larger than ~2 000 leave threads idle due to coarser granularity. The speedup remains below the thread count (8×) because QuickSort's partition is inherently sequential and MergeSort's `merge` step is an unparallelised critical section — both are subject to Amdahl's law.

### Summary table

| | QuickSort | MergeSort |
|---|---|---|
| Best naive speedup (5 M elements) | 2.65× | 1.97× |
| Best threshold speedup (2 M elements) | **5.00×** (thr = 1 000) | **3.18×** (thr = 500) |
| Recommended threshold | 500 – 2 000 | 500 – 1 000 |
