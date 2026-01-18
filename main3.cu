#include <iostream>
#include <vector>
#include <future>
#include <mutex>
#include <chrono>  // Niezbędne do pomiaru czasu
#include "CPUAlgorithm.h"

// Deklaracja funkcji wrapper'a GPU (zdefiniowana w pliku GPUKernel.cu)
CPUAlgorithm::Result runGPUCalculation(const CPUAlgorithm::OptimizationData& data, long long start_range, long long end_range);
std::mutex print_mutex;

int main() {
    CPUAlgorithm cpuAlgo;
    double gpu_ratio = 0.995;

    // 2. Przygotowanie danych
    auto data = cpuAlgo.prepareData(5, 140, 1, 0.5, 40);
    long long total_configs = 1LL << data.allowed_swaps.size();

    // 3. OBLICZANIE PUNKTU PODZIAŁU
    long long gpu_workload = (long long)(total_configs * gpu_ratio);
    long long cpu_workload = total_configs - gpu_workload;
    long long split_point = cpu_workload;

    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "\n=== START HYBRYDY (CPU + GPU) ===\n";
        std::cout << "Ratio GPU: " << (gpu_ratio * 100) << "%\n";
        std::cout << "CPU Zakres: 0 -> " << split_point << "\n";
        std::cout << "GPU Zakres: " << split_point << " -> " << total_configs << "\n";
        std::cout << "---------------------------------\n";
    }

    // Zmienne do przechowywania czasu wykonania poszczególnych części
    // Są one przechwytywane przez referencję [&] w lambdach
    double cpu_duration = 0.0;
    double gpu_duration = 0.0;

    // ---------------------------------------------------------
    // DEFINICJA ZADAŃ (LAMBDY)
    // ---------------------------------------------------------

    // Zadanie dla CPU
    auto cpu_task = [&](long long start, long long end) {
        // START POMIARU CPU
        auto t_start = std::chrono::high_resolution_clock::now();

        CPUAlgorithm::Result res = { -1, INT_MAX };

        // Wykonanie pracy (tylko jeśli jest zakres)
        if (start < end) {
            res = cpuAlgo.solveParallel(data, start, end);
        }

        // KONIEC POMIARU CPU
        auto t_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = t_end - t_start;
        cpu_duration = diff.count(); // Zapisujemy czas do zmiennej zewnętrznej

        // Zgłoszenie zakończenia
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << ">>> [INFO] CPU SKONCZYLO prace! Czas: " << cpu_duration << "s | Koszt: " << res.min_cost << std::endl;
        return res;
    };

    // Zadanie dla GPU
    auto gpu_task = [&](long long start, long long end) {
        // START POMIARU GPU
        auto t_start = std::chrono::high_resolution_clock::now();

        CPUAlgorithm::Result res = { -1, INT_MAX };

        if (start < end) {
            res = runGPUCalculation(data, start, end);
        }

        // KONIEC POMIARU GPU
        auto t_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = t_end - t_start;
        gpu_duration = diff.count(); // Zapisujemy czas

        // Zgłoszenie zakończenia
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << ">>> [INFO] GPU SKONCZYLO prace! Czas: " << gpu_duration << "s | Koszt: " << res.min_cost << std::endl;
        return res;
    };

    // ---------------------------------------------------------
    // URUCHOMIENIE ASYNCHRONICZNE
    // ---------------------------------------------------------

    // Start pomiaru CAŁOŚCI (Wall clock time)
    auto total_start_time = std::chrono::high_resolution_clock::now();

    auto cpu_future = std::async(std::launch::async, cpu_task, 0, split_point);
    auto gpu_future = std::async(std::launch::async, gpu_task, split_point, total_configs);

    // ---------------------------------------------------------
    // OCZEKIWANIE NA WYNIKI
    // ---------------------------------------------------------

    CPUAlgorithm::Result cpu_result = cpu_future.get();
    CPUAlgorithm::Result gpu_result = gpu_future.get();

    // Koniec pomiaru CAŁOŚCI
    auto total_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_elapsed = total_end_time - total_start_time;

    // ---------------------------------------------------------
    // PODSUMOWANIE
    // ---------------------------------------------------------
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n=== PODSUMOWANIE CZASOWE ===\n";
    std::cout << "Czas CPU:     " << cpu_duration << " s\n";
    std::cout << "Czas GPU:     " << gpu_duration << " s\n";
    std::cout << "Czas CALOSCI: " << total_elapsed.count() << " s\n";
    std::cout << "----------------------------\n";

    CPUAlgorithm::Result final_result;
    if (cpu_result.min_cost < gpu_result.min_cost) {
        final_result = cpu_result;
        std::cout << "ZWYCIEZCA: CPU (Koszt: " << final_result.min_cost << ")\n";
    } else {
        final_result = gpu_result;
        std::cout << "ZWYCIEZCA: GPU (Koszt: " << final_result.min_cost << ")\n";
    }

    cpuAlgo.finalizeAndPrint(data, final_result);

    return 0;
}