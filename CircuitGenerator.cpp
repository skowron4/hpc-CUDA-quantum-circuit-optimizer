#include "CircuitGenerator.h"
#include <iostream>
#include <ctime>

// Konstruktor
CircuitGenerator::CircuitGenerator(int qubits) : num_qubits(qubits), next_id(1) {
    rng.seed(std::time(nullptr));
}

// Podstawowa bramka 1-kubitowa
void CircuitGenerator::single(int target, std::string name) {
    if (target >= num_qubits) {
        std::cerr << "[Gen Error] Qubit " << target << " poza zakresem!" << std::endl;
        return;
    }
    circuit.push_back({next_id++, name, SINGLE_QUBIT, target, -1});
}

// Podstawowa bramka 2-kubitowa
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

// Implementacja skrótów
void CircuitGenerator::h(int target) { single(target, "h"); }
void CircuitGenerator::x(int target) { single(target, "x"); }
void CircuitGenerator::y(int target) { single(target, "y"); }
void CircuitGenerator::cx(int control, int target) { two(control, target, "cx"); }

// Implementacja generatora losowego
void CircuitGenerator::generate_random(int num_gates, double prob_two_qubit) {
    std::uniform_real_distribution<> dist_type(0.0, 1.0);
    std::uniform_int_distribution<> dist_qubit(0, num_qubits - 1);

    for (int i = 0; i < num_gates; ++i) {
        bool is_two_qubit = dist_type(rng) < prob_two_qubit;

        if (is_two_qubit && num_qubits > 1) {
            int c = dist_qubit(rng);
            int t = dist_qubit(rng);
            while (t == c) { t = dist_qubit(rng); }
            cx(c, t);
        } else {
            int q = dist_qubit(rng);
            double type = dist_type(rng);
            if (type < 0.33) h(q);
            else if (type < 0.66) x(q);
            else y(q);
        }
    }
}

// Pobieranie wyniku
std::vector<Gate> CircuitGenerator::get_circuit() const {
    return circuit;
}

// Czyszczenie
void CircuitGenerator::clear() {
    circuit.clear();
    next_id = 1;
}
