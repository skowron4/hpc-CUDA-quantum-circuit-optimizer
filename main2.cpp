#include <chrono>
#include "CPUAlgorithm.h"

int main() {
    CPUAlgorithm cpuAlgorithm;
    auto start_time = std::chrono::high_resolution_clock::now();
//    cpuAlgorithm.runSingleCore(5, 80, 24, 0.5);
    cpuAlgorithm.runMultiCore(5, 80, 24, 0.5);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Czas obliczen: " << elapsed.count() << "s\n";

    return 0;
}