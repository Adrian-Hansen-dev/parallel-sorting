#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <omp.h>

// --- QuickSort (In-Place) ---
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

void quickSort(std::vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

// --- MergeSort ---
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

void mergeSort(std::vector<int>& arr, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        mergeSort(arr, l, m);
        mergeSort(arr, m + 1, r);
        merge(arr, l, m, r);
    }
}

// --- Hilfsfunktionen für Benchmarking ---
void fillRandom(std::vector<int>& arr, int size) {
    std::mt19937 gen(42); // Fester Seed für Vergleichbarkeit 
    std::uniform_int_distribution<> dis(1, 1000000);
    for (int i = 0; i < size; ++i) arr.push_back(dis(gen));
}

int main() {
    const int DATA_SIZE = 100000; // Ausreichend große Datenmenge [cite: 10, 16]
    
    // QuickSort Benchmark
    std::vector<int> dataQS;
    fillRandom(dataQS, DATA_SIZE);
    
    double startQS = omp_get_wtime();
    quickSort(dataQS, 0, dataQS.size() - 1);
    double endQS = omp_get_wtime();
    
    std::cout << "QuickSort (Sequentiell): " << (endQS - startQS) << " Sekunden" << std::endl;

    // MergeSort Benchmark
    std::vector<int> dataMS;
    fillRandom(dataMS, DATA_SIZE);
    
    double startMS = omp_get_wtime();
    mergeSort(dataMS, 0, dataMS.size() - 1);
    double endMS = omp_get_wtime();
    
    std::cout << "MergeSort (Sequentiell): " << (endMS - startMS) << " Sekunden" << std::endl;

    return 0;
}