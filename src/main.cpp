#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <omp.h>

// --- QuickSort Hilfsfunktion ---
int partition(std::vector<int>& arr, int low, int high) {
    int pivot = arr[high];
    int i = (low - 1);
    for (int j = low; j <= high - 1; j++) {
        if (arr[j] < pivot) {
            i++;
            std::swap(arr[i], arr[j]);
        }
    }
    std::swap(arr[i + 1], arr[high]);
    return (i + 1);
}

// --- QuickSort (Sequentiell) ---
void quickSortSeq(std::vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quickSortSeq(arr, low, pi - 1);
        quickSortSeq(arr, pi + 1, high);
    }
}

// --- Naives Paralleles QuickSort ---
void quickSortNaive(std::vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);

        #pragma omp task 
        quickSortNaive(arr, low, pi - 1);

        #pragma omp task 
        quickSortNaive(arr, pi + 1, high);
    }
}

// --- MergeSort Hilfsfunktion ---
void merge(std::vector<int>& arr, int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;
    std::vector<int> L(n1), R(n2);
    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) arr[k++] = L[i++];
        else arr[k++] = R[j++];
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
}

// --- MergeSort (Sequentiell) ---
void mergeSortSeq(std::vector<int>& arr, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        mergeSortSeq(arr, l, m);
        mergeSortSeq(arr, m + 1, r);
        merge(arr, l, m, r);
    }
}

// --- Naives Paralleles MergeSort ---
void mergeSortNaive(std::vector<int>& arr, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;

        #pragma omp task 
        mergeSortNaive(arr, l, m);

        #pragma omp task 
        mergeSortNaive(arr, m + 1, r);

        #pragma omp taskwait 
        merge(arr, l, m, r);
    }
}

// --- Hilfsfunktionen ---
void fillRandom(std::vector<int>& arr, int size) {
    arr.clear();
    std::mt19937 gen(42); 
    std::uniform_int_distribution<> dis(1, 1000000);
    for (int i = 0; i < size; ++i) arr.push_back(dis(gen));
}

int main() {
    const int DATA_SIZE = 100000; 
    std::vector<int> originalData;
    fillRandom(originalData, DATA_SIZE);

    std::vector<int> data;
    double start, end;

    std::cout << "Benchmark mit " << DATA_SIZE << " Elementen auf " 
              << omp_get_max_threads() << " Threads.\n" << std::endl;

    // --- QUICK SORT ---
    std::cout << "--- QuickSort ---" << std::endl;
    
    // Sequentiell
    data = originalData;
    start = omp_get_wtime();
    quickSortSeq(data, 0, data.size() - 1);
    end = omp_get_wtime();
    std::cout << "Sequentiell: " << (end - start) << " Sek." << std::endl;

    // Parallel
    data = originalData;
    start = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single
        quickSortNaive(data, 0, data.size() - 1);
    }
    end = omp_get_wtime();
    std::cout << "Parallel:     " << (end - start) << " Sek.\n" << std::endl;


    // --- MERGE SORT ---
    std::cout << "--- MergeSort ---" << std::endl;

    // Sequentiell
    data = originalData;
    start = omp_get_wtime();
    mergeSortSeq(data, 0, data.size() - 1);
    end = omp_get_wtime();
    std::cout << "Sequentiell: " << (end - start) << " Sek." << std::endl;

    // Parallel
    data = originalData;
    start = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single
        mergeSortNaive(data, 0, data.size() - 1);
    }
    end = omp_get_wtime();
    std::cout << "Parallel:     " << (end - start) << " Sek." << std::endl;

    return 0;
}