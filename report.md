# Parallel Sorting – Benchmark Report

## 1. Benchmarking Setup

**Environment**

| Property | Value |
|---|---|
| Machine | MacBook Pro 2020 |
| CPU | Apple M1 (ARM64) |
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

---

## 4. Performance Investigation: MergeSort Heap Allocation Bottleneck

### 4.1 Observation

After re-running the threshold sweep we noticed that MergeSort did not behave as expected: lowering the threshold — which increases the number of parallel tasks — should improve speedup, but instead the speedup *dropped* as we went below threshold 2 000.

| Threshold | Time (s) | Speedup |
|---:|---:|---:|
| 500 | 0.082 | 2.71× |
| 1 000 | 0.069 | 3.22× |
| **2 000** | **0.061** | **3.64×** |
| 5 000 | 0.069 | 3.23× |
| 10 000 | 0.068 | 3.25× |

QuickSort showed no such anomaly — its speedup decreased smoothly as threshold grew, exactly as theory predicts. The difference pointed us toward something specific to the merge step.

### 4.2 Root Cause

We traced the problem to the `merge()` function:

```cpp
void merge(std::vector<int>& arr, int l, int m, int r) {
    std::vector<int> L(n1), R(n2);   // two heap allocations on every call
    ...
}
```

Every call to `merge()` performs two dynamic allocations (`new[]`) and two deallocations (`delete[]`). In the sequential version this is harmless. In the parallel version, all 8 threads call `merge()` concurrently at different levels of the recursion tree. The system allocator uses internal locks to manage the heap, so those concurrent allocations serialise on the lock — threads that should be doing useful work end up queuing for `malloc`.

This explains the inverted behaviour: a *lower* threshold creates *more* concurrent parallel tasks, which means *more* simultaneous `merge()` calls, which means *more* allocator contention. Extra parallelism was actively making things slower. The "drop in the middle" of the threshold sweep was not a load-balancing or granularity problem — it was an allocator bottleneck hiding behind what looked like a tuning curve.

The same allocation overhead also slowed down the sequential variant: the entire recursion tree executes O(N log N) allocations across its lifetime, each with non-trivial overhead.

### 4.3 Fix

We pre-allocate a single scratch buffer of size N once, outside the parallel region, and pass a raw pointer into it through the recursion. Each task operates on the slice `buf[l..r]`, which is disjoint from every other task's slice — so there are no data races and no false sharing of live data.

```cpp
// Allocated once in main(), before any parallel work:
std::vector<int> buf(N);

// merge() now uses the caller's slice instead of allocating:
void merge(std::vector<int>& arr, int* buf, int l, int m, int r) {
    int n1 = m - l + 1;
    for (int i = 0; i < n1; i++) buf[l + i] = arr[l + i];  // copy left half only
    int i = l, j = m + 1, k = l;
    while (i <= m && j <= r)
        arr[k++] = (buf[i] <= arr[j]) ? buf[i++] : arr[j++];
    while (i <= m) arr[k++] = buf[i++];
}
```

We also reduced the copy from two halves to one: the right half already sits in `arr` at the correct positions, so only the left half needs to be saved to scratch before the merge overwrites it.

### 4.4 Results After Fix

| Threshold | Before (speedup) | After (speedup) | Improvement |
|---:|---:|---:|---:|
| 500 | 2.71× | 4.25× | +57% |
| 1 000 | 3.22× | 4.13× | +28% |
| 2 000 | 3.64× | 4.31× | +18% |
| 5 000 | 3.23× | 4.20× | +30% |
| 10 000 | 3.25× | 4.23× | +30% |
| 20 000 | 3.32× | 4.24× | +28% |
| 50 000 | 3.27× | 4.22× | +29% |
| 100 000 | 3.15× | 4.05× | +29% |

Two things stand out. First, the non-monotonic dip is gone — speedup is now flat and consistent across all thresholds (~4.1–4.3×), which matches the expected shape for a threshold-based parallel algorithm. Second, the sequential baseline itself improved significantly (0.22 s → 0.10 s), because eliminating per-call allocations reduces overhead even in single-threaded execution.

The lesson is that **dynamic memory allocation inside a hot parallel loop is a hidden serialisation point**. The symptoms (non-monotonic speedup, performance that gets worse as you add parallelism) can look like a scheduling or granularity problem, but the real cause is contention on the allocator lock.
