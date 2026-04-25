#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>
#include <string>
#include <omp.h>

// ============================================================
// 1. SEQUENTIAL ALGORITHMS (baseline)
// ============================================================

int partition(std::vector<int>& arr, int low, int high) {
    // Median-of-three pivot to avoid worst-case on sorted input
    int mid = low + (high - low) / 2;
    if (arr[mid] < arr[low])  std::swap(arr[mid], arr[low]);
    if (arr[high] < arr[low]) std::swap(arr[high], arr[low]);
    if (arr[mid] < arr[high]) std::swap(arr[mid], arr[high]);
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (arr[j] <= pivot) {
            ++i;
            std::swap(arr[i], arr[j]);
        }
    }
    std::swap(arr[i + 1], arr[high]);
    return i + 1;
}

void quickSortSeq(std::vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quickSortSeq(arr, low, pi - 1);
        quickSortSeq(arr, pi + 1, high);
    }
}

// buf must be a pre-allocated scratch buffer of the same size as arr.
// Each recursive call uses the slice buf[l..r], which is disjoint across
// parallel tasks — no allocator contention, no false sharing of live data.
void merge(std::vector<int>& arr, int* buf, int l, int m, int r) {
    int n1 = m - l + 1;
    // Copy only the left half into the scratch slice; right half stays in arr.
    for (int i = 0; i < n1; i++) buf[l + i] = arr[l + i];

    int i = l, j = m + 1, k = l;
    while (i <= m && j <= r)
        arr[k++] = (buf[i] <= arr[j]) ? buf[i++] : arr[j++];
    while (i <= m) arr[k++] = buf[i++];
    // Remaining right-half elements are already in place.
}

void mergeSortSeq(std::vector<int>& arr, int* buf, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        mergeSortSeq(arr, buf, l, m);
        mergeSortSeq(arr, buf, m + 1, r);
        merge(arr, buf, l, m, r);
    }
}

// ============================================================
// 2. NAIVE PARALLEL (unlimited task spawning)
// ============================================================

void quickSortNaive(std::vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        #pragma omp task default(none) shared(arr) firstprivate(low, pi)
        quickSortNaive(arr, low, pi - 1);
        #pragma omp task default(none) shared(arr) firstprivate(pi, high)
        quickSortNaive(arr, pi + 1, high);
        #pragma omp taskwait
    }
}

void mergeSortNaive(std::vector<int>& arr, int* buf, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        #pragma omp task default(none) shared(arr, buf) firstprivate(l, m)
        mergeSortNaive(arr, buf, l, m);
        #pragma omp task default(none) shared(arr, buf) firstprivate(m, r)
        mergeSortNaive(arr, buf, m + 1, r);
        #pragma omp taskwait
        merge(arr, buf, l, m, r);
    }
}

// ============================================================
// 3. PARALLEL WITH THRESHOLD
// ============================================================

void quickSortThreshold(std::vector<int>& arr, int low, int high, int threshold) {
    if (low >= high) return;
    if (high - low < threshold) {
        // Fall back to sequential below the threshold
        quickSortSeq(arr, low, high);
        return;
    }
    int pi = partition(arr, low, high);
    #pragma omp task default(none) shared(arr) firstprivate(low, pi, threshold)
    quickSortThreshold(arr, low, pi - 1, threshold);
    #pragma omp task default(none) shared(arr) firstprivate(pi, high, threshold)
    quickSortThreshold(arr, pi + 1, high, threshold);
    #pragma omp taskwait
}

void mergeSortThreshold(std::vector<int>& arr, int* buf, int l, int r, int threshold) {
    if (l >= r) return;
    if (r - l < threshold) {
        mergeSortSeq(arr, buf, l, r);
        return;
    }
    int m = l + (r - l) / 2;
    #pragma omp task default(none) shared(arr, buf) firstprivate(l, m, threshold)
    mergeSortThreshold(arr, buf, l, m, threshold);
    #pragma omp task default(none) shared(arr, buf) firstprivate(m, r, threshold)
    mergeSortThreshold(arr, buf, m + 1, r, threshold);
    #pragma omp taskwait
    merge(arr, buf, l, m, r);
}

// ============================================================
// HELPERS
// ============================================================

void fillRandom(std::vector<int>& arr, int size, unsigned seed = 42) {
    arr.resize(size);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dis(1, 10'000'000);
    for (auto& v : arr) v = dis(gen);
}

bool isSorted(const std::vector<int>& arr) {
    for (size_t i = 1; i < arr.size(); i++)
        if (arr[i] < arr[i - 1]) return false;
    return true;
}

// Run a function NUM_RUNS times, return minimum wall-clock time (seconds)
template<typename Fn>
double benchMin(Fn fn, int runs = 5) {
    double best = 1e18;
    for (int i = 0; i < runs; i++) {
        double t0 = omp_get_wtime();
        fn();
        double t1 = omp_get_wtime();
        best = std::min(best, t1 - t0);
    }
    return best;
}

// ============================================================
// MAIN
// ============================================================

int main() {
    const int NUM_RUNS = 5;
    const int NUM_THREADS = omp_get_max_threads();

    std::cout << "=== Parallel Sorting Benchmark ===" << std::endl;
    std::cout << "Threads available: " << NUM_THREADS << std::endl;
    std::cout << "Reporting minimum of " << NUM_RUNS << " runs (seconds)\n\n";

    // ----------------------------------------------------------
    // Part 1 & 2: Sequential vs. Naive parallel for several sizes
    // ----------------------------------------------------------
    std::vector<int> sizes = {100'000, 500'000, 1'000'000, 5'000'000};

    std::cout << std::left
              << std::setw(12) << "Algorithm"
              << std::setw(10) << "Mode"
              << std::setw(12) << "Size"
              << std::setw(14) << "Time(s)"
              << std::setw(10) << "Speedup"
              << "OK?\n";
    std::cout << std::string(62, '-') << "\n";

    for (int size : sizes) {
        std::vector<int> orig;
        fillRandom(orig, size);
        std::vector<int> buf(size);  // scratch buffer — allocated once per size, reused across runs

        // QuickSort sequential
        double tQSeq = benchMin([&](){
            auto data = orig;
            quickSortSeq(data, 0, (int)data.size() - 1);
        }, NUM_RUNS);

        // QuickSort naive parallel
        double tQNaive = benchMin([&](){
            auto data = orig;
            #pragma omp parallel num_threads(NUM_THREADS)
            #pragma omp single nowait
            quickSortNaive(data, 0, (int)data.size() - 1);
        }, NUM_RUNS);

        // MergeSort sequential
        double tMSeq = benchMin([&](){
            auto data = orig;
            mergeSortSeq(data, buf.data(), 0, (int)data.size() - 1);
        }, NUM_RUNS);

        // MergeSort naive parallel
        double tMNaive = benchMin([&](){
            auto data = orig;
            #pragma omp parallel num_threads(NUM_THREADS)
            #pragma omp single nowait
            mergeSortNaive(data, buf.data(), 0, (int)data.size() - 1);
        }, NUM_RUNS);

        // Verify correctness once
        auto dataCheck = orig;
        quickSortSeq(dataCheck, 0, (int)dataCheck.size() - 1);
        bool qOk = isSorted(dataCheck);
        dataCheck = orig;
        mergeSortSeq(dataCheck, buf.data(), 0, (int)dataCheck.size() - 1);
        bool mOk = isSorted(dataCheck);

        auto row = [&](const std::string& alg, const std::string& mode, double t, double base, bool ok){
            std::cout << std::fixed << std::setprecision(6)
                      << std::left
                      << std::setw(12) << alg
                      << std::setw(10) << mode
                      << std::setw(12) << size
                      << std::setw(14) << t
                      << std::setw(10) << (base / t)
                      << (ok ? "yes" : "NO") << "\n";
        };

        row("QuickSort", "seq",   tQSeq,   tQSeq,   qOk);
        row("QuickSort", "naive", tQNaive, tQSeq,   qOk);
        row("MergeSort", "seq",   tMSeq,   tMSeq,   mOk);
        row("MergeSort", "naive", tMNaive, tMSeq,   mOk);
        std::cout << "\n";
    }

    // ----------------------------------------------------------
    // Part 3: Threshold sweep on large data
    // ----------------------------------------------------------
    const int BENCH_SIZE = 2'000'000;
    std::vector<int> orig;
    fillRandom(orig, BENCH_SIZE);
    std::vector<int> buf(BENCH_SIZE);  // scratch buffer for all mergesort variants

    std::cout << "\n=== Threshold Sweep (size=" << BENCH_SIZE << ", " << NUM_THREADS << " threads) ===\n";
    std::cout << std::left
              << std::setw(12) << "Algorithm"
              << std::setw(14) << "Threshold"
              << std::setw(14) << "Time(s)"
              << "Speedup vs seq\n";
    std::cout << std::string(54, '-') << "\n";

    // Baseline sequential times for speedup reference
    double tQSeqBase = benchMin([&](){
        auto data = orig;
        quickSortSeq(data, 0, (int)data.size() - 1);
    }, NUM_RUNS);

    double tMSeqBase = benchMin([&](){
        auto data = orig;
        mergeSortSeq(data, buf.data(), 0, (int)data.size() - 1);
    }, NUM_RUNS);

    std::cout << std::fixed << std::setprecision(6)
              << std::setw(12) << "QuickSort" << std::setw(14) << "sequential"
              << std::setw(14) << tQSeqBase << "1.000000\n";
    std::cout << std::setw(12) << "MergeSort" << std::setw(14) << "sequential"
              << std::setw(14) << tMSeqBase << "1.000000\n\n";

    std::vector<int> thresholds = {500, 1000, 2000, 5000, 10000, 20000, 50000, 100000};

    for (int thr : thresholds) {
        double tQ = benchMin([&](){
            auto data = orig;
            #pragma omp parallel num_threads(NUM_THREADS)
            #pragma omp single nowait
            quickSortThreshold(data, 0, (int)data.size() - 1, thr);
        }, NUM_RUNS);

        double tM = benchMin([&](){
            auto data = orig;
            #pragma omp parallel num_threads(NUM_THREADS)
            #pragma omp single nowait
            mergeSortThreshold(data, buf.data(), 0, (int)data.size() - 1, thr);
        }, NUM_RUNS);

        std::cout << std::setw(12) << "QuickSort"
                  << std::setw(14) << thr
                  << std::setw(14) << tQ
                  << (tQSeqBase / tQ) << "\n";
        std::cout << std::setw(12) << "MergeSort"
                  << std::setw(14) << thr
                  << std::setw(14) << tM
                  << (tMSeqBase / tM) << "\n";
    }

    return 0;
}
