#include <chrono>
#include "CPUAlgorithm.h"

int main() {
    CPUAlgorithm cpuAlgorithm;
    auto start_time = std::chrono::high_resolution_clock::now();
    // cpuAlgorithm.runSingleCore(5, 80, 24, 0.5); //brute force 1-core
    // cpuAlgorithm.runMultiCore(5, 80, 24, 0.5); //brute force multi-core
    cpuAlgorithm.runDE(5, 200, 24, 0.5, 100, 500); //differential evolution 1-core
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Czas obliczen: " << elapsed.count() << "s\n";

    return 0;
}