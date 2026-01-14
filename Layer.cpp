#include "Layer.h"

std::string Layer::toString() const {
    std::stringstream ss;
    ss << "=== WARSTWA " << id << " ===";
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

std::ostream& operator<<(std::ostream& os, const Layer& layer) {
    os << layer.toString();
    return os;
}

std::vector<Layer> Layer::fromCircuit(const std::vector<Gate>& circuit) {
    std::vector<Layer> layers;

    // Bufor na bramki, które wystąpiły ZANIM dany qubit wziął udział w bramce 2-kubitowej
    std::vector<Gate> initial_buffer;

    // Mapa: ID Qubita -> Indeks warstwy w wektorze 'layers'
    std::map<int, int> qubit_owner_layer;

    int layer_counter = 0;

    for (const auto& gate : circuit) {
        if (gate.type == TWO_QUBIT) {
            // 1. Tworzymy nową warstwę
            Layer new_layer(layer_counter); // Używamy konstruktora

            // 2. Sprawdzamy bufor startowy
            std::vector<Gate> remaining_buffer;
            for (const auto& buffered_gate : initial_buffer) {
                if (buffered_gate.target == gate.control || buffered_gate.target == gate.target) {
                    new_layer.gates.push_back(buffered_gate);
                } else {
                    remaining_buffer.push_back(buffered_gate);
                }
            }
            initial_buffer = remaining_buffer; // Aktualizacja bufora

            // 3. Dodajemy samą bramkę 2-kubitową
            new_layer.gates.push_back(gate);

            // 4. Zapisujemy warstwę
            layers.push_back(new_layer);

            // 5. Aktualizujemy mapę właścicieli
            qubit_owner_layer[gate.control] = layer_counter;
            qubit_owner_layer[gate.target]  = layer_counter;

            layer_counter++;
        }
        else { // SINGLE_QUBIT
            int q = gate.target;

            // Sprawdzamy, czy ten qubit ma już przypisaną warstwę
            if (qubit_owner_layer.count(q)) {
                // TAK: Dopisujemy do istniejącej warstwy
                int owner_id = qubit_owner_layer[q];

                // Zabezpieczenie (na wszelki wypadek, choć logika licznika powinna być spójna)
                if (owner_id < layers.size()) {
                    layers[owner_id].gates.push_back(gate);
                }
            } else {
                // NIE: Qubit jeszcze nie miał bramki 2-kubitowej -> idzie do bufora
                initial_buffer.push_back(gate);
            }
        }
    }

    // --- Sprzątanie (Orphans) ---
    if (!initial_buffer.empty()) {
        if (!layers.empty()) {
            // Doklej do ostatniej
            layers.back().gates.insert(
                layers.back().gates.end(),
                initial_buffer.begin(),
                initial_buffer.end()
            );
        } else {
            // Warstwa zerowa, jeśli brak bramek 2-kubitowych
            Layer l(0);
            l.gates = initial_buffer;
            layers.push_back(l);
        }
    }

    return layers;
}