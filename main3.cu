#include <iostream>
#include <vector>
#include <future>
#include <mutex>
#include <chrono>
#include "CPUAlgorithm.h"

CPUAlgorithm::Result runGPUCalculation(const CPUAlgorithm::OptimizationData& data, long long start_range, long long end_range);
std::mutex print_mutex;

int main() {
    CPUAlgorithm cpuAlgo;
    double gpu_ratio = 1;

    auto data = cpuAlgo.prepareData(5, 160, 24, 0.5, 60);
    long long total_configs = 1LL << data.allowed_swaps.size();

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


    double cpu_duration = 0.0;
    double gpu_duration = 0.0;


    auto cpu_task = [&](long long start, long long end) {

        auto t_start = std::chrono::high_resolution_clock::now();

        CPUAlgorithm::Result res = { -1, INT_MAX };

        if (start < end) {
            res = cpuAlgo.solveParallel(data, start, end);
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = t_end - t_start;
        cpu_duration = diff.count(); // Zapisujemy czas do zmiennej zewnętrznej

        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << ">>> [INFO] CPU SKONCZYLO prace! Czas: " << cpu_duration << "s | Koszt: " << res.min_cost << std::endl;
        return res;
    };

    auto gpu_task = [&](long long start, long long end) {

        auto t_start = std::chrono::high_resolution_clock::now();

        CPUAlgorithm::Result res = { -1, INT_MAX };

        if (start < end) {
            res = runGPUCalculation(data, start, end);
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = t_end - t_start;
        gpu_duration = diff.count(); // Zapisujemy czas

        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << ">>> [INFO] GPU SKONCZYLO prace! Czas: " << gpu_duration << "s | Koszt: " << res.min_cost << std::endl;
        return res;
    };


    auto total_start_time = std::chrono::high_resolution_clock::now();

    auto cpu_future = std::async(std::launch::async, cpu_task, 0, split_point);
    auto gpu_future = std::async(std::launch::async, gpu_task, split_point, total_configs);


    CPUAlgorithm::Result cpu_result = cpu_future.get();
    CPUAlgorithm::Result gpu_result = gpu_future.get();

    auto total_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_elapsed = total_end_time - total_start_time;


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