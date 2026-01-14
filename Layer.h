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
    std::vector<Gate> gates;

    Layer() : id(-1) {}
    Layer(int _id) : id(_id) {}

    std::string toString() const;

    friend std::ostream& operator<<(std::ostream& os, const Layer& layer);

    static std::vector<Layer> fromCircuit(const std::vector<Gate>& circuit);
};

#endif //LAYER_H
