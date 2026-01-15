#include <iostream>
#include <vector>
#include "CircuitGenerator.h"
#include "Group.h"
#include <algorithm>
#include <windows.h>
#include <fstream>


#define MAX_GATES 80
#define MAX_SWAPS 40
#define NUM_QUBITS 5

void printWithSwaps(const std::vector<Gate>& circuit, int num_qubits) {
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
            int min_q = min(c, t);
            int max_q = max(c, t);
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


int calculateCost(
    std::vector<Group> &local_groups,
    const std::vector<int>& allowed_swaps,
    int config_mask,
    int initial_center_qubit
) {
    int num_allowed_swaps = (int)allowed_swaps.size();
    int num_group = (int)local_groups.size();

    for (int i = 0; i < num_allowed_swaps; ++i) {
        if ((config_mask >> i) & 1) {
            int idx = allowed_swaps[i];
            if (idx < num_group - 1) {
                std::swap(local_groups[idx], local_groups[idx + 1]);
            }
        }
    }

    int cost = 0;
    int current_center = initial_center_qubit;

    for (int i = 0; i < num_group; ++i) {
        int q1 = local_groups[i].control_two_qubit_gate;
        int q2 = local_groups[i].target_two_qubit_gate;

        bool q1_is_center = (q1 == current_center);
        bool q2_is_center = (q2 == current_center);

        if (q1_is_center || q2_is_center) {
            continue;
        } else {
            cost++;

            int next_target = -1;
            bool end = false;

            for (int j = i + 1; !end && j < num_group; ++j) {
                int next_q1 = local_groups[j].control_two_qubit_gate;
                int next_q2 = local_groups[j].target_two_qubit_gate;

                bool uses_q1 = (next_q1 == q1 || next_q2 == q1);
                bool uses_q2 = (next_q1 == q2 || next_q2 == q2);

                if (uses_q1 && !uses_q2) {
                    next_target = q1;
                    end = true;
                }
                if (uses_q2 && !uses_q1) {
                    next_target = q2;
                    end = true;
                }
            }

            if (next_target != -1) current_center = next_target;
            else current_center = q1;
        }
    }

    return cost;
}

int main() {
//    SetConsoleOutputCP(CP_UTF8);
    CircuitGenerator gen(NUM_QUBITS,24);
    // gen.single(0, "s1");
    // gen.single(1, "s2");
    // gen.two(0, 1, "d1");
    // gen.two(1,2, "d2");
    // gen.single(0, "s3");

    gen.generate_random(MAX_GATES, 0.5);

    std::vector<Gate> my_circuit = gen.get_circuit();

    std::vector<Group> host_group = Group::fromCircuit(my_circuit);

    std::vector<int> allowed_swaps = Group::getGroupPermutation(host_group);

    gen.print();
    Group::printGroups(host_group, NUM_QUBITS);
    const int hard_limit_swaps = 31;

    if (allowed_swaps.size() > hard_limit_swaps) {
        std::cout << "!!! UWAGA: Zbyt duzo kombinacji (>" << hard_limit_swaps << ") !!!\n";
        exit(1);
    }

    std::cout << "Dozwolone permutacje grup ("<< allowed_swaps.size() << "):" << std::endl;
    for (const auto &perm: allowed_swaps) {
        std::cout << perm << " ";
    }
    std::cout << std::endl;

    int num_groups = (int) host_group.size();
    int num_swaps = (int) allowed_swaps.size();

    // Ograniczenie, aby nie przekroczyć zakresu int przy shifcie (1 << 31 max)
    // Przy MAX_SWAPS 16 to bezpieczne (65536 iteracji)
    int num_configs = 1 << num_swaps;
    int initial_center_qubit = 0;

    std::cout << "Start optymalizacji CPU dla " << num_groups << " warstw i " << num_configs << " permutacji..." << std::endl;

    // 3. Pętla zamiast Kernela (CPU Brute-force / Exhaustive Search)
    int min_cost = INT_MAX;
    int best_config = -1;

    std::vector<Group> temp_group;

    for (int mask = 0; mask < num_configs; ++mask) {
        // Obliczamy koszt dla danej permutacji (przekazujemy kopię wektora grup przez wartość w funkcji)
        std::copy(host_group.begin(), host_group.end(), host_group.end());
        int cost = calculateCost(host_group, allowed_swaps, mask, initial_center_qubit);

        if (cost < min_cost) {
            min_cost = cost;
            best_config = mask;
        }
    }

    std::cout << "Najlepsza konfiguracja (maska): " << best_config << " Koszt: " << min_cost << std::endl;

    // 4. Odtwarzanie wyniku
    std::vector<Group> final_host_groups = host_group;

    // Aplikujemy najlepsze zamiany na finalnym wektorze
    for (int i = 0; i < num_swaps; ++i) {
        if ((best_config >> i) & 1) {
            int idx = allowed_swaps[i];
            if (idx < num_groups - 1)
                std::swap(final_host_groups[idx], final_host_groups[idx+1]);
        }
    }

    std::cout << "Kolejnosc ID warstw: ";
    for(const auto& g : final_host_groups) std::cout << g.id << " ";
    std::cout << "\n";

    // 5. Generowanie pełnego obwodu z dodanymi fizycznymi bramkami SWAP
    std::vector<Gate> full_circuit_with_swaps;
    int current_center = initial_center_qubit;

    for (int i = 0; i < num_groups; ++i) {
        int q1 = final_host_groups[i].control_two_qubit_gate;
        int q2 = final_host_groups[i].target_two_qubit_gate;

        bool needed_swap = false;
        int chosen_swap_target;

        // Sprawdzenie czy potrzebny SWAP (logika centrum)
        if (!(q1 == current_center || q2 == current_center)) {
            needed_swap = true;

            int next_target = -1;
            bool end = false;

            // Look-ahead
            for (int j = i + 1; !end && j < num_groups; ++j) {
                int next_q1 = final_host_groups[j].control_two_qubit_gate;
                int next_q2 = final_host_groups[j].target_two_qubit_gate;

                bool uses_q1 = (next_q1 == q1 || next_q2 == q1);
                bool uses_q2 = (next_q1 == q2 || next_q2 == q2);

                if (uses_q1 && !uses_q2) { next_target = q1; end = true; }
                if (uses_q2 && !uses_q1) { next_target = q2; end = true; }
            }

            if (next_target != -1) chosen_swap_target = next_target;
            else chosen_swap_target = q1;
        }

        // Dodanie bramki SWAP jeśli wymagana
        if (needed_swap) {
            Gate swap_gate;
            swap_gate.type = TWO_QUBIT;
            swap_gate.name = "SWAP";
            swap_gate.control = current_center;
            swap_gate.target = chosen_swap_target;

            full_circuit_with_swaps.push_back(swap_gate);

            current_center = chosen_swap_target;
        }

        // Dodanie bramek z grupy
        full_circuit_with_swaps.insert(
                full_circuit_with_swaps.end(),
                final_host_groups[i].gates.begin(),
                final_host_groups[i].gates.end()
        );
    }

    printWithSwaps(full_circuit_with_swaps, NUM_QUBITS);
    return 0;
}