#ifndef CIRCUITGENERATOR_H
#define CIRCUITGENERATOR_H

#pragma once
#include <vector>
#include <string>
#include <random>
#include "Gate.h"

class CircuitGenerator {
private:
    int num_qubits;
    int next_id;
    std::vector<Gate> circuit;
    std::mt19937 rng;

public:
    CircuitGenerator(int qubits);
    CircuitGenerator(int qubits, int seed);



    void single(int target, std::string name = "u3");
    void two(int control, int target, std::string name = "cx");


    void h(int target);
    void x(int target);
    void y(int target);
    void cx(int control, int target);


    void generate_random(int target_two_qubit_count, double prob_single_qubit);


    std::vector<Gate> get_circuit() const;
    void clear();

    void print() const;
};



#endif //CIRCUITGENERATOR_H
