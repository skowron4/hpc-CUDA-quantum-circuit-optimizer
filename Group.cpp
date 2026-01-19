#include "Group.h"
#include <algorithm>

std::string Group::toString() const {
    std::stringstream ss;
    ss << "=== GRUPA " << id << " ===(" << target_two_qubit_gate << "," << control_two_qubit_gate << ")";
    if (gates.empty()) {
        ss << " (Pusta)";
    } else {
        ss << "\n";
        for (const auto& gate : gates) {
            ss << "  -> " << gate.name << " (ID:" << gate.id << ")";
            if (gate.type == TWO_QUBIT)
                 ss << " [q" << gate.control << ", q" << gate.target << "]";
            else
                 ss << " [q" << gate.target << "]";
            ss << "\n";
        }
    }
    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const Group& group) {
    os << group.toString();
    return os;
}

std::vector<Group> Group::fromCircuit(const std::vector<Gate>& circuit) {
    std::vector<Group> groups;

    std::vector<Gate> initial_buffer;

    std::map<int, int> qubit_owner_group_gates;

    int group_counter = 0;

    for (const auto& gate : circuit) {
        if (gate.type == TWO_QUBIT) {
            Group new_group(group_counter, gate.target, gate.control); // Używamy konstruktora

            std::vector<Gate> remaining_buffer;
            for (const auto& buffered_gate : initial_buffer) {
                if (buffered_gate.target == gate.control || buffered_gate.target == gate.target) {
                    new_group.gates.push_back(buffered_gate);
                } else {
                    remaining_buffer.push_back(buffered_gate);
                }
            }
            initial_buffer = remaining_buffer; // Aktualizacja bufora

            new_group.gates.push_back(gate);

            groups.push_back(new_group);

            qubit_owner_group_gates[gate.control] = group_counter;
            qubit_owner_group_gates[gate.target]  = group_counter;

            group_counter++;
        }
        else {
            int q = gate.target;

            if (qubit_owner_group_gates.count(q)) {
                int owner_id = qubit_owner_group_gates[q];

                if (owner_id < groups.size()) {
                    groups[owner_id].gates.push_back(gate);
                }
            } else {
                initial_buffer.push_back(gate);
            }
        }
    }

    if (!initial_buffer.empty()) {
        if (!groups.empty()) {
            groups.back().gates.insert(
                groups.back().gates.end(),
                initial_buffer.begin(),
                initial_buffer.end()
            );
        } else {
            std::cout << "HALOOOOOOO KARTUS coś niedobrego sie dzieje" << std::endl;
            Group l(0, -1, -1);
            l.gates = initial_buffer;
            groups.push_back(l);
        }
    }

    return groups;
}


std::vector<int> Group::getGroupPermutation(const std::vector<Group>& groups) {
    std::vector<int> permutation;

    if (groups.size() < 2) {
        return permutation;
    }

    for (size_t i = 0; i < groups.size() - 1; ++i) {
        const Group& current = groups[i];
        const Group& next = groups[i + 1];

        bool hasConflict = false;

        if (current.target_two_qubit_gate == next.target_two_qubit_gate ||
            current.target_two_qubit_gate == next.control_two_qubit_gate ||
            current.control_two_qubit_gate == next.target_two_qubit_gate ||
            current.control_two_qubit_gate == next.control_two_qubit_gate) {
            hasConflict = true;
            }

        if (!hasConflict) {
            permutation.push_back(current.id);
            i++;
        }
    }

    return permutation;
}

void Group::printGroups(const std::vector<Group>& groups, int num_qubits) {
    std::cout << "\n=== WIZUALIZACJA PODZIALU NA GRUPY ===\n";

    for (const auto& group : groups) {
        std::cout << "Grupa ID: " << group.id << "\n";

        std::vector<std::stringstream> lines(num_qubits);

        for (int i = 0; i < num_qubits; ++i) lines[i] << "q" << i << ": ";

        for (const auto& gate : group.gates) {
            int max_len = 0;
            for (const auto& ss : lines) {
                if (ss.str().length() > max_len) max_len = ss.str().length();
            }
            for (auto& ss : lines) {
                while (ss.str().length() < max_len) ss << "-";
            }

            if (gate.type == SINGLE_QUBIT) {
                for (int i = 0; i < num_qubits; ++i) {
                    if (i == gate.target) {
                        std::string g_name = gate.name;
                        std::transform(g_name.begin(), g_name.end(), g_name.begin(), ::toupper);
                        lines[i] << "--[" << g_name << "]--";
                    } else {
                        int gap = gate.name.length() + 6;
                        for (int j = 0; j < gap; ++j) lines[i] << "-";
                    }
                }
            }
            else if (gate.type == TWO_QUBIT) {
                int c = gate.control;
                int t = gate.target;
                int min_q = std::min(c, t);
                int max_q = std::max(c, t);

                for (int i = 0; i < num_qubits; ++i) {
                    if (i == c) lines[i] << "---*---";
                    else if (i == t) lines[i] << "--(+)--";
                    else if (i > min_q && i < max_q) lines[i] << "---|---";
                    else lines[i] << "-------";
                }
            }
        }

        for (int i = 0; i < num_qubits; ++i) {
            std::cout << lines[i].str() << "\n";
        }
        std::cout << "----------------------------------------\n";
    }
}