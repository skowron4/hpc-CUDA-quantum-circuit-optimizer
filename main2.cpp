#include <chrono>
#include <iomanip>
#include "CPUAlgorithm.h"
#include "iostream"
#include "vector"
#include "fstream"

int main() {
    CPUAlgorithm cpuAlgorithm;
//    auto start_time = std::chrono::high_resolution_clock::now();
    // cpuAlgorithm.runSingleCore(5, 80, 24, 0.5); //brute force 1-core
    // cpuAlgorithm.runMultiCore(5, 80, 24, 0.5); //brute force multi-core
//    cpuAlgorithm.runDE(5, 200, 24, 0.5, 100, 500); //differential evolution 1-core
//    auto end_time = std::chrono::high_resolution_clock::now();
//    std::chrono::duration<double> elapsed = end_time - start_time;
//    std::cout << "Czas obliczen: " << elapsed.count() << "s\n";
//    std::vector<CPUAlgorithm::Result> results;

    // Generowanie unikalnej nazwy pliku
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream  filename_ss;
    filename_ss << "wyniki_" << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S") << ".csv";
    std::string filename = filename_ss.str();

    // POPRAWKA 1 & 2: Używamy std::ofstream (do zapisu) i zmiennej 'filename'
    std::ofstream csv_file("../" + filename);
    if (!csv_file.is_open()) {
        std::cout << "Common... nie udalo sie otworzyc pliku " << filename << "\n";
        exit(1);
    }

    csv_file << "Proba,Czas,Config,Swap Gates\n";

    CPUAlgorithm::Result tmp_result;
    double czas;

    for (int i = 0; i < 10; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();
        // tmp_result = cpuAlgorithm.runSingleCore(5, 80, 24, 0.5);
        // tmp_result = cpuAlgorithm.runMultiCore(5, 80, 24, 0.5);
        tmp_result = cpuAlgorithm.runDE(5, 80, 24, 0.5, 100, 500, 1);
        auto stop_time = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed = stop_time - start_time;
        czas = elapsed.count();
        std::cout << "Iteracja " << i << " - Czas obliczen: " << czas << "s\n";

        // Uwaga: upewnij się, że tmp_result.best_config to std::string lub typ obsługujący operator <<
        csv_file << i << "," << czas << "," << tmp_result.best_config << "," << tmp_result.min_cost << "\n";
    }

    // POPRAWKA 3: Zamykamy plik dopiero PO zakończeniu pętli!
    csv_file.close();

    std::cout << "Pomyślnie zapisano wszystko do pliku: " << filename << "\n";

    return 0;
}