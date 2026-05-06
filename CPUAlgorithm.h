#ifndef OBLICZENIA_CPUALGORITHM_H
#define OBLICZENIA_CPUALGORITHM_H

#include "Group.h"

#define MAX_GATES 80
#define MAX_SWAPS 40
#define NUM_QUBITS 5


class CPUAlgorithm {
public:
    void runSingleCore(int num_qubits, int max_gates, int seed, double probability);
    void runMultiCore(int num_qubits, int max_gates, int seed, double probability);

    struct OptimizationData {
        std::vector<Group> initial_groups;
        std::vector<int> allowed_swaps;
        int num_qubits;
    };

    struct Result {
        long long best_config;
        int min_cost;
    };
    Result solveParallel(const OptimizationData& data, long long start_range, long long end_range);
    OptimizationData prepareData(int num_qubits, int max_gates, int seed, double probability, int limit_swaps);
    void finalizeAndPrint(const OptimizationData& data, Result result);

    Result solveDE(const OptimizationData& data, int pop_size = 100, int max_gen = 500, double F = 0.8, double CR = 0.2);
    void runDE(int num_qubits, int max_gates, int seed, double probability, int pop_size = 100, int max_gen = 500);

private:

    void printWithSwaps(const std::vector<Gate>& circuit, int num_qubits);
    Result solveSerial(const OptimizationData& data);
    int determineNextCenter(const std::vector<Group>& groups, int current_center, int num_groups);
    int calculateCost(
            std::vector<int>& local_indices,
            const std::vector<Group>& groups_data,
            const std::vector<int>& allowed_swaps,
            long long config_mask,
            int initial_center_qubit);
    void isValidCircuit(int allowed_swaps_size, int hard_limit_swaps);
    void printGroupPerm(std::vector<int> &allowed_swaps);


};


#endif //OBLICZENIA_CPUALGORITHM_H
