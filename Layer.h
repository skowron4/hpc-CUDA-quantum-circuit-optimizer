#ifndef LAYER_H
#define LAYER_H

#pragma once
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include "Gate.h"

struct Layer {
    int id;
    int target_two_qubit_gate;
    int control_two_qubit_gate;
    std::vector<Gate> gates;

    Layer(int _id, int _target_two_qubit_gate, int _control_two_qubit_gate) : id(_id), target_two_qubit_gate(_target_two_qubit_gate), control_two_qubit_gate(_control_two_qubit_gate) {}

    std::string toString() const;

    friend std::ostream& operator<<(std::ostream& os, const Layer& layer);

    static std::vector<Layer> fromCircuit(const std::vector<Gate>& circuit);

    static std::vector<int> getLayerPermutation(const std::vector<Layer>& layers);

};

#endif //LAYER_H
