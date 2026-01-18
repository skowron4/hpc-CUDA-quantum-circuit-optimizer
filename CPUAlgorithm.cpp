//
// Created by User on 16.01.2026.
//

#include "CPUAlgorithm.h"
#include <algorithm>
#include <omp.h>
#include "CircuitGenerator.h"
#include <numeric>
#include <climits>

int CPUAlgorithm::calculateCost(
        std::vector<int>& local_indices,
        const std::vector<Group>& groups_data,
        const std::vector<int>& allowed_swaps,
        long long config_mask,
        int initial_center_qubit
) {
    int num_allowed_swaps = (int)allowed_swaps.size();
    int num_groups = (int)local_indices.size();

    for (int i = 0; i < num_allowed_swaps; ++i) {
        if ((config_mask >> i) & 1) {
            int idx = allowed_swaps[i];
            if (idx < num_groups - 1) {
                std::swap(local_indices[idx], local_indices[idx + 1]);
            }
        }
    }

    int cost = 0;
    int current_center = initial_center_qubit;

    for (int i = 0; i < num_groups; ++i) {
        const Group& current_group = groups_data[local_indices[i]];

        int q1 = current_group.control_two_qubit_gate;
        int q2 = current_group.target_two_qubit_gate;

        if (q1 == current_center || q2 == current_center) continue;

        cost++;
        int next_target = q1;
        bool found = false;

        for (int j = i + 1; !found && j < num_groups; ++j) {
            const Group& future_group = groups_data[local_indices[j]];
            int nq1 = future_group.control_two_qubit_gate;
            int nq2 = future_group.target_two_qubit_gate;

            bool u1 = (nq1 == q1 || nq2 == q1);
            bool u2 = (nq1 == q2 || nq2 == q2);

            if (u1 && !u2) { next_target = q1; found = true; }
            else if (u2 && !u1) { next_target = q2; found = true; }
        }

        current_center = next_target;
    }

    return cost;
}

int CPUAlgorithm::determineNextCenter(const std::vector<Group>& groups, int current_idx, int num_groups) {
    int q1 = groups[current_idx].control_two_qubit_gate;
    int q2 = groups[current_idx].target_two_qubit_gate;

    int next_target = q1;

    for (int j = current_idx + 1; j < num_groups; ++j) {
        int next_q1 = groups[j].control_two_qubit_gate;
        int next_q2 = groups[j].target_two_qubit_gate;

        bool uses_q1 = (next_q1 == q1 || next_q2 == q1);
        bool uses_q2 = (next_q1 == q2 || next_q2 == q2);

        if (uses_q1 && !uses_q2) return q1;
        if (uses_q2 && !uses_q1) return q2;
    }

    return next_target;
}

void CPUAlgorithm::printWithSwaps(const std::vector<Gate>& circuit, int num_qubits) {
    if (circuit.empty()) return;

    std::vector<std::stringstream> lines(num_qubits);
    for (int i = 0; i < num_qubits; ++i) lines[i] << "q" << i << ": ";

    for (const auto& gate : circuit) {
        int max_len = 0;
        for (const auto& ss : lines)
            if ((int)ss.str().length() > max_len) max_len = (int)ss.str().length();

        for (auto& ss : lines)
            while ((int)ss.str().length() < max_len) ss << "-";

        if (gate.type == SINGLE_QUBIT) {
            for (int i = 0; i < num_qubits; ++i) {
                if (i == gate.target) {
                    std::string g_name = gate.name;
                    if(g_name.length() > 3) g_name = g_name.substr(0, 3);
                    std::transform(g_name.begin(), g_name.end(), g_name.begin(), ::toupper);
                    lines[i] << "-[" << g_name << "]-";
                } else {
                    lines[i] << "-----";
                }
            }
        }
        else if (gate.type == TWO_QUBIT) {
            int c = gate.control;
            int t = gate.target;
            int min_q = std::min(c, t);
            int max_q = std::max(c, t);
            bool is_swap = (gate.name == "SWAP");

            for (int i = 0; i < num_qubits; ++i) {
                if (i > min_q && i < max_q) {
                    lines[i] << "--|--";
                }
                else if (is_swap && (i == c || i == t)) {
                    lines[i] << "--x--"; // SWAP
                }
                else if (!is_swap && i == c) {
                    lines[i] << "--o--"; // Control
                }
                else if (!is_swap && i == t) {
                    lines[i] << "-(+)-"; // Target
                }
                else {
                    lines[i] << "-----";
                }
            }
        }
    }

    std::cout << "\n=== FINALNY OBWOD Z BRAMKAMI SWAP ===\n";
    for (int i = 0; i < num_qubits; ++i) {
        std::cout << lines[i].str() << "\n";
    }
    std::cout << "=====================================\n";
}

void CPUAlgorithm::isValidCircuit(int allowed_swaps_size, int hard_limit_swaps) {
    if (allowed_swaps_size > hard_limit_swaps) {
        std::cout << "!!! UWAGA: Zbyt duzo kombinacji (>" << hard_limit_swaps << ") !!!\n";
        exit(1);
    }
}

void CPUAlgorithm::printGroupPerm(std::vector<int> &allowed_swaps) {
    std::cout << "Dozwolone permutacje grup ("<< allowed_swaps.size() << "):" << std::endl;
    for (const auto &perm: allowed_swaps) {
        std::cout << perm << " ";
    }
    std::cout << "\n";
}

CPUAlgorithm::OptimizationData CPUAlgorithm::prepareData(int num_qubits, int max_gates, int seed, double probability, int limit_swaps) {
    CircuitGenerator gen(num_qubits, seed);
    gen.generate_random(max_gates, probability);

    std::vector<Gate> my_circuit = gen.get_circuit();
    std::vector<Group> host_group = Group::fromCircuit(my_circuit);
    std::vector<int> allowed_swaps = Group::getGroupPermutation(host_group);

    gen.print();
    Group::printGroups(host_group, num_qubits);

    isValidCircuit(allowed_swaps.size(), limit_swaps);
    printGroupPerm(allowed_swaps);

    return { host_group, allowed_swaps, num_qubits };
}

CPUAlgorithm::Result CPUAlgorithm::solveSerial(const OptimizationData& data){
    long long num_configs = 1LL << data.allowed_swaps.size();
    int min_cost = INT_MAX;
    long long best_config = -1;

    std::vector<int> base_indices(data.initial_groups.size());
    std::iota(base_indices.begin(), base_indices.end(), 0);

    // Wektor robocz
    std::vector<int> temp_indices = base_indices;

    std::cout << "Start CPU Single-Core Optimized (" << num_configs << " permutacji)...\n";

    for (long long mask = 0; mask < num_configs; ++mask) {
        std::copy(base_indices.begin(), base_indices.end(), temp_indices.begin());

        int cost = calculateCost(temp_indices, data.initial_groups, data.allowed_swaps, mask, 0);

        if (cost < min_cost) {
            min_cost = cost;
            best_config = mask;
        }
    }
    return { best_config, min_cost };
}

CPUAlgorithm::Result CPUAlgorithm::solveParallel(const OptimizationData& data) {
    long long num_configs = 1LL << data.allowed_swaps.size();
    int global_min_cost = INT_MAX;
    long long global_best_config = -1;

    // Tworzymy bazowy wektor indeksów: [0, 1, 2, 3, ..., N]
    std::vector<int> base_indices(data.initial_groups.size());
    std::iota(base_indices.begin(), base_indices.end(), 0);

    std::cout << "Start CPU Multicore Optimized (" << omp_get_max_threads() << " watkow)...\n";

#pragma omp parallel
    {
        std::vector<int> thread_local_indices = base_indices;

        int local_min = INT_MAX;
        long long local_best = -1;

#pragma omp for schedule(static)
        for (long long mask = 0; mask < num_configs; ++mask) {
            std::copy(base_indices.begin(), base_indices.end(), thread_local_indices.begin());

            int cost = calculateCost(thread_local_indices, data.initial_groups, data.allowed_swaps, mask, 0);

            if (cost < local_min) {
                local_min = cost;
                local_best = mask;
            }
        }

#pragma omp critical
        {
            if (local_min < global_min_cost) {
                global_min_cost = local_min;
                global_best_config = local_best;
            }
        }
    }

    return { global_best_config, global_min_cost };
}

void CPUAlgorithm::finalizeAndPrint(const OptimizationData& data, Result result) {
    std::cout << "Najlepsza konfiguracja (maska): " << result.best_config << " Koszt: " << result.min_cost << std::endl;

    std::vector<Group> final_groups = data.initial_groups;
    int num_groups = (int)final_groups.size();
    int num_swaps = (int)data.allowed_swaps.size();

    for (int i = 0; i < num_swaps; ++i) {
        if ((result.best_config >> i) & 1) {
            int idx = data.allowed_swaps[i];
            if (idx < num_groups - 1)
                std::swap(final_groups[idx], final_groups[idx + 1]);
        }
    }

    std::cout << "Kolejnosc ID warstw: ";
    for (const auto& g : final_groups) std::cout << g.id << " ";
    std::cout << "\n";

    std::vector<Gate> full_circuit;
    int current_center = 0;

    for (int i = 0; i < num_groups; ++i) {
        int q1 = final_groups[i].control_two_qubit_gate;
        int q2 = final_groups[i].target_two_qubit_gate;

        if (q1 != current_center && q2 != current_center) {
            int next_center = determineNextCenter(final_groups, i, num_groups);

            full_circuit.push_back({0, "SWAP", TWO_QUBIT, next_center, current_center});

            current_center = next_center;
        }

        full_circuit.insert(full_circuit.end(), final_groups[i].gates.begin(), final_groups[i].gates.end());
    }

    printWithSwaps(full_circuit, data.num_qubits);
}

void CPUAlgorithm::runSingleCore(int num_qubits, int max_gates, int seed, double probability) {
    OptimizationData data = prepareData(num_qubits, max_gates, seed, probability, 31);
    Result result = solveSerial(data);
    finalizeAndPrint(data, result);
}

void CPUAlgorithm::runMultiCore(int num_qubits, int max_gates, int seed, double probability) {
    OptimizationData data = prepareData(num_qubits, max_gates, seed, probability, 33);
    Result result = solveParallel(data);
    finalizeAndPrint(data, result);
}