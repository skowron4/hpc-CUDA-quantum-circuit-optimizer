#ifndef LAYER_H
#define LAYER_H

#pragma once
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include "Gate.h"

struct Group {
    int id;
    int target_two_qubit_gate;
    int control_two_qubit_gate;
    std::vector<Gate> gates;

    Group(int _id, int _target_two_qubit_gate, int _control_two_qubit_gate) : id(_id), target_two_qubit_gate(_target_two_qubit_gate), control_two_qubit_gate(_control_two_qubit_gate) {}

    std::string toString() const;

    friend std::ostream& operator<<(std::ostream& os, const Group& group);

    static std::vector<Group> fromCircuit(const std::vector<Gate>& circuit);

    static std::vector<int> getGroupPermutation(const std::vector<Group>& groups);

};

#endif //LAYER_H
