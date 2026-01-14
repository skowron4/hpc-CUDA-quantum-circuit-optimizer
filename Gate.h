#ifndef GATE_H
#define GATE_H

#pragma once
#include <iostream>
#include <string>

enum GateType { SINGLE_QUBIT, TWO_QUBIT };

struct Gate {
    int id;
    std::string name;
    GateType type;
    int target;
    int control; // -1 for SINGLE_QUBIT
};

inline std::ostream& operator<<(std::ostream& os, const Gate& g) {
    os << "Gate { id: " << g.id
       << ", name: '" << g.name << "'";

    if (g.type == SINGLE_QUBIT) {
        os << ", type: SINGLE, target: q" << g.target;
    } else {
        os << ", type: TWO, control: q" << g.control
           << " -> target: q" << g.target;
    }

    os << " }";
    return os;
}
#endif //GATE_H
