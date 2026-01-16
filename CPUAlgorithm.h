//
// Created by User on 16.01.2026.
//

#ifndef OBLICZENIA_CPUALGORITHM_H
#define OBLICZENIA_CPUALGORITHM_H

#include "Group.h"

#define MAX_GATES 80
#define MAX_SWAPS 40
#define NUM_QUBITS 5


class CPUAlgorithm {
public:
    void runSingleCore(int num_qubits, int max_gates, int seed, int probability);
    void runMultiCore(int num_qubits, int max_gates, int seed, int probability);
private:
    struct OptimizationData {
        std::vector<Group> initial_groups;
        std::vector<int> allowed_swaps;
        int num_qubits;
    };

    struct Result {
        long long best_config;
        int min_cost;
    };

    void printWithSwaps(const std::vector<Gate>& circuit, int num_qubits);
    OptimizationData prepareData(int num_qubits, int max_gates, int seed, int probability, int limit_swaps);
    Result solveSerial(const OptimizationData& data);
    Result solveParallel(const OptimizationData& data);
    void finalizeAndPrint(const OptimizationData& data, Result result);
    int determineNextCenter(const std::vector<Group>& groups, int current_center, int num_groups);
    int calculateCost(
            std::vector<Group> &local_groups,
            const std::vector<int>& allowed_swaps,
            long long config_mask,
            int initial_center_qubit);
    void isValidCircuit(int allowed_swaps_size, int hard_limit_swaps);
    void printGroupPerm(std::vector<int> &allowed_swaps);


};


#endif //OBLICZENIA_CPUALGORITHM_H
