#include "Group.h"

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

    // Bufor na bramki, które wystąpiły ZANIM dany qubit wziął udział w bramce 2-kubitowej
    std::vector<Gate> initial_buffer;

    // Mapa: ID Qubita -> Indeks grupy bramek w wektorze 'groups'
    std::map<int, int> qubit_owner_group_gates;

    int group_counter = 0;

    for (const auto& gate : circuit) {
        if (gate.type == TWO_QUBIT) {
            // 1. Tworzymy nową grupę
            Group new_group(group_counter, gate.target, gate.control); // Używamy konstruktora

            // 2. Sprawdzamy bufor startowy
            std::vector<Gate> remaining_buffer;
            for (const auto& buffered_gate : initial_buffer) {
                if (buffered_gate.target == gate.control || buffered_gate.target == gate.target) {
                    new_group.gates.push_back(buffered_gate);
                } else {
                    remaining_buffer.push_back(buffered_gate);
                }
            }
            initial_buffer = remaining_buffer; // Aktualizacja bufora

            // 3. Dodajemy samą bramkę 2-kubitową
            new_group.gates.push_back(gate);

            // 4. Zapisujemy grupę
            groups.push_back(new_group);

            // 5. Aktualizujemy mapę właścicieli
            qubit_owner_group_gates[gate.control] = group_counter;
            qubit_owner_group_gates[gate.target]  = group_counter;

            group_counter++;
        }
        else { // SINGLE_QUBIT
            int q = gate.target;

            // Sprawdzamy, czy ten qubit ma już przypisaną grupę
            if (qubit_owner_group_gates.count(q)) {
                // TAK: Dopisujemy do istniejącej grupy
                int owner_id = qubit_owner_group_gates[q];

                // Zabezpieczenie (na wszelki wypadek, choć logika licznika powinna być spójna)
                if (owner_id < groups.size()) {
                    groups[owner_id].gates.push_back(gate);
                }
            } else {
                // NIE: Qubit jeszcze nie miał bramki 2-kubitowej -> idzie do bufora
                initial_buffer.push_back(gate);
            }
        }
    }

    // --- Sprzątanie (Orphans) ---
    if (!initial_buffer.empty()) {
        if (!groups.empty()) {
            // Doklej do ostatniej
            groups.back().gates.insert(
                groups.back().gates.end(),
                initial_buffer.begin(),
                initial_buffer.end()
            );
        } else {
            // Warstwa zerowa, jeśli brak bramek 2-kubitowych
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