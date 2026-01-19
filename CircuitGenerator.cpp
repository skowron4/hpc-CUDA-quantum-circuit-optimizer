#include "CircuitGenerator.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <ctime>


CircuitGenerator::CircuitGenerator(int qubits) : num_qubits(qubits), next_id(1) {
    rng.seed(std::time(nullptr));
}

CircuitGenerator::CircuitGenerator(int qubits, int seed) : num_qubits(qubits), next_id(1) {
    rng.seed(seed);
}

void CircuitGenerator::single(int target, std::string name) {
    if (target >= num_qubits) {
        std::cerr << "[Gen Error] Qubit " << target << " poza zakresem!" << std::endl;
        return;
    }
    circuit.push_back({next_id++, name, SINGLE_QUBIT, target, -1});
}

void CircuitGenerator::two(int control, int target, std::string name) {
    if (control >= num_qubits || target >= num_qubits) {
        std::cerr << "[Gen Error] Qubit poza zakresem!" << std::endl;
        return;
    }
    if (control == target) {
        std::cerr << "[Gen Error] Control i Target musza byc rozne!" << std::endl;
        return;
    }
    circuit.push_back({next_id++, name, TWO_QUBIT, target, control});
}

void CircuitGenerator::h(int target) { single(target, "h"); }
void CircuitGenerator::x(int target) { single(target, "x"); }
void CircuitGenerator::y(int target) { single(target, "y"); }
void CircuitGenerator::cx(int control, int target) { two(control, target, "cx"); }

void CircuitGenerator::generate_random(int target_two_qubit_count, double prob_single_qubit) {
    std::uniform_real_distribution<> dist_prob(0.0, 1.0);
    std::uniform_int_distribution<> dist_qubit(0, num_qubits - 1);

    int current_two_qubit_count = 0;

    if (prob_single_qubit >= 1.0) {
        std::cerr << "[Gen Error] Prawdopodobienstwo bramki 1-kubitowej nie moze byc 1.0 (nieskonczona petla)!" << std::endl;
        return;
    }

    while (current_two_qubit_count < target_two_qubit_count) {

        bool is_single_qubit = dist_prob(rng) < prob_single_qubit;

        if (is_single_qubit) {
            int q = dist_qubit(rng);
            double type = dist_prob(rng);

            if (type < 0.33) h(q);
            else if (type < 0.66) x(q);
            else y(q);
        }
        else {
            if (num_qubits > 1) {
                int c = dist_qubit(rng);
                int t = dist_qubit(rng);
                while (t == c) { t = dist_qubit(rng); }

                cx(c, t);

                current_two_qubit_count++;
            } else break;
        }
    }
}

std::vector<Gate> CircuitGenerator::get_circuit() const {
    return circuit;
}

void CircuitGenerator::clear() {
    circuit.clear();
    next_id = 1;
}

void CircuitGenerator::print() const {
    if (circuit.empty()) {
        std::cout << "Obwód jest pusty!!\n";
        return;
    }

    std::vector<std::stringstream> lines(num_qubits);

    for (int i = 0; i < num_qubits; ++i) lines[i] << "q" << i << ": ";

    for (const auto& gate : circuit) {
        int max_len = 0;
        for (const auto& ss : lines) {
            int len = ss.str().length();
            if (len > max_len) max_len = len;
        }

        for (auto& ss : lines) {
            while (ss.str().length() < max_len) ss << "-";
        }

        if (gate.type == SINGLE_QUBIT) {
            for (int i = 0; i < num_qubits; ++i) {
                if (i == gate.target) {
                    std::string g_name = gate.name;
                    std::transform(g_name.begin(), g_name.end(), g_name.begin(), ::toupper);
                    lines[i] << "-[" << g_name << "]-";
                }
                else {
                    int gap = gate.name.length() + 4;
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
                if (i == c) lines[i] << "--o--";
                else if (i == t) lines[i] << "-(+)-";
                else if (i > min_q && i < max_q) lines[i] << "--|--";
                else lines[i] << "-----";
            }
        }
    }

    std::cout << "\n=== WIZUALIZACJA OBWODU ===\n";
    for (int i = 0; i < num_qubits; ++i) {
        std::cout << lines[i].str() << "\n";
    }

    std::cout << "============================\n";
}