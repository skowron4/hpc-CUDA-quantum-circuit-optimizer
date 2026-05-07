#include "CPUAlgorithm.h"
#include <algorithm>
#include <omp.h>
#include "CircuitGenerator.h"
#include <numeric>
#include <climits>

#ifdef _MSC_VER
#include <intrin.h>
#endif

inline int get_trailing_zeros(unsigned int mask) {
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward(&index, mask);
    return (int)index;
#else
    return __builtin_ctz(mask);
#endif
}

int CPUAlgorithm::calculateCost(
        std::vector<int>& local_indices,
        const std::vector<Group>& groups_data,
        const std::vector<int>& allowed_swaps,
        long long config_mask,
        int initial_center_qubit
) {
    size_t num_groups = local_indices.size();
    size_t num_swaps_avail = allowed_swaps.size();

    unsigned int mask_temp = config_mask;

    while (mask_temp > 0) {
        int bit_index = get_trailing_zeros(mask_temp);

        mask_temp &= (mask_temp - 1);

        if (bit_index < (int)num_swaps_avail) {
            int idx = allowed_swaps[bit_index];
            if (idx >= 0 && idx < (int)num_groups - 1) {
                std::swap(local_indices[idx], local_indices[idx + 1]);
            }
        }
    }

    int cost = 0;
    int current_center = initial_center_qubit;

    for (size_t i = 0; i < num_groups; ++i) {
        const Group& current_group = groups_data[local_indices[i]];

        int q1 = current_group.control_two_qubit_gate;
        int q2 = current_group.target_two_qubit_gate;

        if (q1 == current_center || q2 == current_center) continue;

        cost++;

        int next_target = q1;

        for (size_t j = i + 1; j < num_groups; ++j) {
            const Group& future_group = groups_data[local_indices[j]];
            int nq1 = future_group.control_two_qubit_gate;
            int nq2 = future_group.target_two_qubit_gate;

            bool uses_q1 = (nq1 == q1 || nq2 == q1);
            bool uses_q2 = (nq1 == q2 || nq2 == q2);

            if (uses_q1 != uses_q2) {
                if (uses_q1) next_target = q1;
                else next_target = q2;

                break;
            }
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

CPUAlgorithm::Result CPUAlgorithm::solveParallel(const OptimizationData& data, long long start_range = 0, long long end_range = LONG_MAX) {
    long long num_configs = 1LL << data.allowed_swaps.size();
    num_configs = std::min(num_configs, end_range);
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
        for (long long mask = start_range; mask < num_configs; ++mask) {
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

CPUAlgorithm::Result CPUAlgorithm::runSingleCore(int num_qubits, int max_gates, int seed, double probability) {
    OptimizationData data = prepareData(num_qubits, max_gates, seed, probability, 31);
    Result result = solveSerial(data);
    finalizeAndPrint(data, result);

    return result;
}

CPUAlgorithm::Result CPUAlgorithm::runMultiCore(int num_qubits, int max_gates, int seed, double probability) {
    OptimizationData data = prepareData(num_qubits, max_gates, seed, probability, 33);
    Result result = solveParallel(data);
    finalizeAndPrint(data, result);

    return result;
}

CPUAlgorithm::Result CPUAlgorithm::solveDE(const OptimizationData& data,
                                           int pop_size,
                                           int max_gen,
                                           int cores,
                                           double F,
                                           double CR) {
    int num_groups = (int)data.initial_groups.size();
    int num_swaps = (int)data.allowed_swaps.size();

    // POPRAWKA 2: Bezpiecznie ustaw liczbę wątków i dopasuj rozmiar wektora generatorów
    if (cores > 0) {
        omp_set_num_threads(cores);
    }
    int max_threads = omp_get_max_threads();

    std::vector<std::mt19937> thread_gens;
    std::random_device rd;
    for (int i = 0; i < max_threads; ++i) {
        thread_gens.emplace_back(rd());
    }

    // Populacja: wektory wag (priorytetów)
    std::vector<std::vector<double>> population(pop_size, std::vector<double>(num_groups));
    std::vector<int> costs(pop_size, INT_MAX);
    std::vector<long long> masks(pop_size, 0);

    long long global_best_mask = 0;
    int global_best_cost = INT_MAX;

    auto evaluate = [&](const std::vector<double>& weights) -> std::pair<int, long long> {
        long long mask = 0;
        for (int i = 0; i < num_swaps; ++i) {
            int idx = data.allowed_swaps[i];
            if (weights[idx] > weights[idx + 1]) {
                mask |= (1LL << i);
            }
        }
        std::vector<int> temp_indices(num_groups);
        std::iota(temp_indices.begin(), temp_indices.end(), 0);
        int cost = calculateCost(temp_indices, data.initial_groups, data.allowed_swaps, mask, 0);
        return {cost, mask};
    };

    std::cout << "Start Differential Evolution (" << pop_size << " osobnikow, "
              << max_gen << " pokolen, OpenMP - " << max_threads << " watkow)...\n";

    // Inicjalizacja populacji
#pragma omp parallel for
    for (int i = 0; i < pop_size; ++i) {
        int tid = omp_get_thread_num();
        // POPRAWKA 3: Dystrybucje lokalne dla wątku - unikanie konfliktów pamięci
        std::uniform_real_distribution<double> dist_01_local(0.0, 1.0);

        for (int j = 0; j < num_groups; ++j) {
            population[i][j] = dist_01_local(thread_gens[tid]);
        }
        auto res = evaluate(population[i]);
        costs[i] = res.first;
        masks[i] = res.second;
    }

    for (int i = 0; i < pop_size; ++i) {
        if (costs[i] < global_best_cost) {
            global_best_cost = costs[i];
            global_best_mask = masks[i];
        }
    }

    // Główna pętla ewolucji
    for (int gen = 0; gen < max_gen; ++gen) {

#pragma omp parallel for
        for (int i = 0; i < pop_size; ++i) {
            int tid = omp_get_thread_num();
            auto& gen_local = thread_gens[tid];

            // Lokalne instancje dystrybucji na każdą iterację wątku
            std::uniform_int_distribution<int> pop_dist(0, pop_size - 1);
            std::uniform_int_distribution<int> dist_group(0, num_groups - 1);
            std::uniform_real_distribution<double> dist_01_local(0.0, 1.0);

            int a, b, c;
            do { a = pop_dist(gen_local); } while (a == i);
            do { b = pop_dist(gen_local); } while (b == i || b == a);
            do { c = pop_dist(gen_local); } while (c == i || c == a || c == b);

            int R = dist_group(gen_local);
            std::vector<double> trial = population[i];

            for (int j = 0; j < num_groups; ++j) {
                if (dist_01_local(gen_local) < CR || j == R) {
                    trial[j] = population[a][j] + F * (population[b][j] - population[c][j]);

                    if (trial[j] < 0.0) trial[j] = 0.0;
                    if (trial[j] > 1.0) trial[j] = 1.0;
                }
            }

            auto res = evaluate(trial);
            int trial_cost = res.first;

            if (trial_cost <= costs[i]) {
                population[i] = trial;
                costs[i] = trial_cost;
                masks[i] = res.second;
            }
        }

        // Zwykła sekwencyjna aktualizacja najlepszego kosztu (po synchronizacji)
        for (int i = 0; i < pop_size; ++i) {
            if (costs[i] < global_best_cost) {
                global_best_cost = costs[i];
                global_best_mask = masks[i];
            }
        }
    }

    return { global_best_mask, global_best_cost };
}

CPUAlgorithm::Result CPUAlgorithm::runDE(int num_qubits,
                         int max_gates,
                         int seed,
                         double probability,
                         int pop_size,
                         int max_gen,
                         int cores,
                         double F,
                         double CR) {
    // Nie ograniczamy tak bardzo swapów - DE poradzi sobie z dużymi przestrzeniami
    OptimizationData data = prepareData(num_qubits, max_gates, seed, probability, 64);

    Result result = solveDE(data, pop_size, max_gen, cores, F, CR);
    finalizeAndPrint(data, result);

    return result;
}