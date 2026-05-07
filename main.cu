#include <iostream>
#include <vector>
#include <future>
#include <mutex>
#include <chrono>
#include "CPUAlgorithm.h"


CPUAlgorithm::Result runGPUCalculation(const CPUAlgorithm::OptimizationData& data, long long start_range, long long end_range);

int main() {
    CPUAlgorithm cpuAlgo;

    auto data = cpuAlgo.prepareData(5, 125, 24, 0.5, 40);

    long long total_configs = 1LL << data.allowed_swaps.size();

    std::cout << "Liczba permutacji do sprawdzenia: " << total_configs << "\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    CPUAlgorithm::Result final_result = runGPUCalculation(data, 0, total_configs);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "\n=== WYNIK KONCOWY ===\n";
    std::cout << "Czas calkowity: " << elapsed.count() << " s\n";
    std::cout << "Najlepszy koszt: " << final_result.min_cost << "\n";
    std::cout << "Konfiguracja (ID): " << final_result.best_config << "\n";
    std::cout << "---------------------\n";

    cpuAlgo.finalizeAndPrint(data, final_result);

    return 0;
}