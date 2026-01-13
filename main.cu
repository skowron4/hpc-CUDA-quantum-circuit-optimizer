#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cuda_runtime.h>
#include <map>

// --- DEFINICJE I STRUKTURY DANYCH ---

#define NUM_QUBITS 5
#define CENTER_QUBIT 0
#define MAX_GATES_PER_LAYER 3 // Ograniczenie dla brute force (6! = 720 wątków to mało dla GPU, ale wystarczy dla demo)

enum GateType { SINGLE_QUBIT, TWO_QUBIT };
struct Gate {
    int id;
    GateType type;
    int control; // Dla Single to jest target
    int target;  // Używane tylko dla TWO_QUBIT
};

struct Layer {
    int num_gates;
    Gate gates[MAX_GATES_PER_LAYER];
};

struct Layer2 {
    int id;
    std::vector<Gate> gates;
};

// Struktura do przesyłania wyników z GPU
struct PermutationResult {
    int cost;           // Koszt SWAPów
    int permutation[MAX_GATES_PER_LAYER]; // Indeksy w kolejności
};

// --- 1. GENERATOR OBWODU (CPU) ---

std::vector<Gate> generate_circuit(int num_gates) {
    std::vector<Gate> circuit;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> type_dist(0, 1);
    std::uniform_int_distribution<> qubit_dist(0, NUM_QUBITS - 1);

    for (int i = 0; i < num_gates; ++i) {
        Gate g;
        g.id = i;
        g.type = (type_dist(gen) == 0) ? SINGLE_QUBIT : TWO_QUBIT;
        g.target = qubit_dist(gen);

        if (g.type == TWO_QUBIT) {
            do {
                g.control = qubit_dist(gen);
            } while (g.target == g.control);
        } else {
            g.control = -1;
        }
        circuit.push_back(g);
    }
    return circuit;
}

// --- 2. PREPROCESSING (CPU) ---
// Grupuje bramki w warstwy. W prostym modelu grupujemy po N bramek.
// W zaawansowanym modelu grupowalibyśmy bramki komutujące.
std::vector<Layer> preprocess_layers(const std::vector<Gate>& circuit, int layer_size) {
    std::vector<Layer> layers;
    Layer current_layer;
    current_layer.num_gates = 0;

    for (const auto& gate : circuit) {
        current_layer.gates[current_layer.num_gates++] = gate;
        if (current_layer.num_gates >= layer_size || current_layer.num_gates >= MAX_GATES_PER_LAYER) {
            layers.push_back(current_layer);
            current_layer.num_gates = 0;
        }
    }
    if (current_layer.num_gates > 0) {
        layers.push_back(current_layer);
    }
    return layers;
}


std::vector<Layer2> preprocess_layers_ownership(const std::vector<Gate>& circuit) {
    std::vector<Layer2> layers;

    // Bufor na bramki, które wystąpiły ZANIM dany qubit wziął udział w bramce 2-kubitowej
    std::vector<Gate> initial_buffer;

    // Mapa: ID Qubita -> Wskaźnik (indeks) do warstwy, która jest jego "właścicielem"
    // Jeśli qubit nie ma wpisu w mapie, znaczy że jeszcze nie miał bramki 2-kubitowej.
    std::map<int, int> qubit_owner_layer;

    int layer_counter = 0;

    for (const auto& gate : circuit) {
        if (gate.type == TWO_QUBIT) {
            // 1. Tworzymy nową warstwę dla bramki dwu-kubitowej
            Layer2 new_layer;
            new_layer.id = layer_counter;

            // 2. Sprawdzamy bufor startowy (czy są tam jakieś zaległe bramki dla tych qubitów)
            std::vector<Gate> remaining_buffer;
            for (const auto& buffered_gate : initial_buffer) {
                if (buffered_gate.target == gate.control || buffered_gate.target == gate.target) {
                    new_layer.gates.push_back(buffered_gate);
                } else {
                    remaining_buffer.push_back(buffered_gate);
                }
            }
            initial_buffer = remaining_buffer; // Aktualizujemy bufor

            // 3. Dodajemy samą bramkę 2-kubitową
            new_layer.gates.push_back(gate);

            // 4. Zapisujemy warstwę
            layers.push_back(new_layer);

            // 5. Aktualizujemy mapę właścicieli
            // Od teraz każda kolejna bramka single na tych qubitach trafi do TEJ warstwy
            // (dopóki nie powstanie nowsza warstwa na tych samych qubitach)
            qubit_owner_layer[gate.control] = layer_counter;
            qubit_owner_layer[gate.target]  = layer_counter;

            layer_counter++;
        }
        else { // SINGLE_QUBIT
            int q = gate.target;

            // Sprawdzamy, czy ten qubit ma już przypisaną warstwę
            if (qubit_owner_layer.count(q)) {
                // TAK: Dopisujemy bramkę do instniejącej warstwy (nawet wstecz!)
                int owner_id = qubit_owner_layer[q];
                // Uwaga: owner_id to indeks w wektorze layers (zakładając że id rosną sekwencyjnie 0,1,2...)
                layers[owner_id].gates.push_back(gate);
            } else {
                // NIE: Qubit jeszcze nie miał bramki 2-kubitowej -> idzie do bufora
                initial_buffer.push_back(gate);
            }
        }
    }

    // --- Sprzątanie ---
    // Jeśli zostały jakieś bramki w buforze (qubity, które NIGDY nie miały bramki 2-kubitowej),
    // zgodnie z zasadą "dołącz do ostatniej lub pierwszej", doklejamy je.
    if (!initial_buffer.empty()) {
        if (!layers.empty()) {
            // Doklej do ostatniej
            layers.back().gates.insert(
                layers.back().gates.end(),
                initial_buffer.begin(),
                initial_buffer.end()
            );
        } else {
            // Jeśli nie było żadnych bramek 2Q, stwórz warstwę "zerową"
            Layer2 l;
            l.id = 0;
            l.gates = initial_buffer;
            layers.push_back(l);
        }
    }

    return layers;
}


void print_layers(const std::vector<Layer2>& layers) {
    std::cout << "\nREZULTAT GRUPOWANIA:\n";
    std::cout << "========================================\n";

    if (layers.empty()) {
        std::cout << "Obwód jest pusty lub brak warstw.\n";
        return;
    }

    for (const auto& layer : layers) {
        std::cout << "WARSTWA " << layer.id << ":\n";
        std::cout << "----------------------------------------\n";

        for (const auto& gate : layer.gates) {
            std::cout << "  -> BRAMKA [" << gate.type << "] (id:" << gate.id << ")";

            // Wyrównanie tekstu dla estetyki
            if (gate.type == SINGLE_QUBIT) {
                std::cout << "\t Typ: SINGLE \t Target: q" << gate.target;
            } else {
                std::cout << "\t Typ: TWO    \t Control: q" << gate.control
                          << " -> Target: q" << gate.target;
            }
            std::cout << std::endl;
        }
        std::cout << "\n"; // Pusta linia między warstwami
    }
    std::cout << "========================================\n";
}


// --- POMOCNICZE FUNKCJE GPU ---

__device__ void swap_ptr(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Funkcja obliczająca silnię (do określenia liczby permutacji)
__device__ int factorial(int n) {
    int res = 1;
    for (int i = 2; i <= n; i++) res *= i;
    return res;
}

// Generowanie k-tej permutacji leksykograficznej (Algorytm Lehmera lub podobny dla indeksowania wątków)
__device__ void get_permutation(int n, int k, int* p) {
    int elements[MAX_GATES_PER_LAYER];
    for(int i=0; i<n; i++) elements[i] = i;

    for (int i = 0; i < n; i++) {
        int f = factorial(n - 1 - i);
        int index = k / f;
        k %= f;
        p[i] = elements[index];

        // Przesunięcie tablicy
        for (int j = index; j < n - 1 - i; j++) {
            elements[j] = elements[j + 1];
        }
    }
}

// Sprawdzenie czy fizyczne kubity są połączone (Gwiazda: 0 jest połączone z 1,2,3,4)
__device__ bool are_connected(int p1, int p2) {
    if (p1 == CENTER_QUBIT || p2 == CENTER_QUBIT) return true;
    return false;
}

// --- 3. KERNEL CUDA (BRUTE FORCE PERMUTACJI) ---

__global__ void evaluate_permutations(
        Layer layer,
        int* current_logical_to_physical,
        PermutationResult* results
) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int num_perms = factorial(layer.num_gates);

    if (tid >= num_perms) return;

    // 1. Ustal kolejność bramek dla tego wątku
    int perm_indices[MAX_GATES_PER_LAYER];
    get_permutation(layer.num_gates, tid, perm_indices);

    // Kopiujemy wynik permutacji do struktury wyjściowej
    for(int i=0; i<layer.num_gates; i++) results[tid].permutation[i] = perm_indices[i];

    // 2. Symulacja kosztu
    // Kopiujemy lokalnie mapowanie, bo będziemy symulować zmiany (SWAPy)
    int map_l2p[NUM_QUBITS];
    for(int i=0; i<NUM_QUBITS; i++) map_l2p[i] = current_logical_to_physical[i];

    int cost = 0;

    // Iterujemy po bramkach w wylosowanej kolejności
    for (int i = 0; i < layer.num_gates; i++) {
        int gate_idx = perm_indices[i];
        Gate g = layer.gates[gate_idx];

        if (g.type == SINGLE_QUBIT) {
            // Bramki 1-kubitowe są "darmowe" w kwestii routingu
            continue;
        }

        int phys_c = map_l2p[g.control];
        int phys_t = map_l2p[g.target];

        // Jeśli są połączone w architekturze gwiazdy
        if (are_connected(phys_c, phys_t)) {
            cost += 0; // Można wykonać TWO_QUBIT
        } else {
            // Jeśli nie są połączone (oba są na ramionach gwiazdy),
            // musimy zrobić SWAP jednego z nich do centrum.
            // Koszt SWAP to zazwyczaj 3 * TWO_QUBIT, ale tu liczymy jako 1 jednostkę logiczną "operacji swap".
            cost += 1;

            // Symulacja: "Greedy" - przesuwamy control do centrum (jeśli centrum nie jest zajęte przez target)
            // W topologii gwiazdy, jeśli phys_c i phys_t != 0, to jeden z nich musi wejść na 0.
            // Sprawdzamy kto jest na 0.

            // Znajdź logiczny kubit, który jest teraz na fizycznym 0
            int logical_at_center = -1;
            for(int k=0; k<NUM_QUBITS; k++) if(map_l2p[k] == CENTER_QUBIT) logical_at_center = k;

            // Swapujemy g.control z tym co jest w centrum
            // Aktualizacja mapy
            map_l2p[logical_at_center] = phys_c; // Ten z centrum idzie na miejsce control
            map_l2p[g.control] = CENTER_QUBIT;   // Control idzie do centrum
        }
    }

    results[tid].cost = cost;
}

// --- 4. INTERPRETER I MAIN (CPU) ---

void print_gate(const Gate& g) {
    if (g.type == SINGLE_QUBIT) std::cout << "U3(" << g.target << ")";
    else std::cout << "TWO_QUBIT(" << g.control << ", " << g.target << ")";
}

int main() {
    // A. Inicjalizacja
    int num_gates = 10;
    std::cout << "[CPU] Generowanie obwodu z " << num_gates << " bramkami...\n";
    auto circuit = generate_circuit(num_gates);

    // Wyświetl obwód wejściowy
    std::cout << "Obwod wejsciowy: ";
    for(const auto& g : circuit) { print_gate(g); std::cout << " "; }
    std::cout << "\n\n";

    // B. Preprocessing
    int layer_size = 3; // Grupy po 3 bramki dla testu
    auto layers = preprocess_layers(circuit, layer_size);
    std::cout << "[CPU] Podzielono na " << layers.size() << " warstw (chunks).\n";

    auto layers2 = preprocess_layers_ownership(circuit);
    print_layers(layers2);

    // Początkowe mapowanie: logiczny i -> fizyczny i
    int logical_to_physical[NUM_QUBITS];
    for(int i=0; i<NUM_QUBITS; i++) logical_to_physical[i] = i;

    // Alokacja pamięci na GPU dla mapowania (stały rozmiar)
    int* d_map;
    cudaMalloc(&d_map, NUM_QUBITS * sizeof(int));

    std::cout << "\n[START PRZETWARZANIA]\n";
    std::cout << "------------------------------------------------\n";

    // C. Pętla Główna (Layer by Layer)
    for (int i = 0; i < layers.size(); ++i) {
        Layer& layer = layers[i];
        int num_perms = 1;
        for(int k=1; k<=layer.num_gates; k++) num_perms *= k;

        // Przygotowanie danych na GPU
        cudaMemcpy(d_map, logical_to_physical, NUM_QUBITS * sizeof(int), cudaMemcpyHostToDevice);

        PermutationResult* d_results;
        cudaMalloc(&d_results, num_perms * sizeof(PermutationResult));

        // Uruchomienie Kernela
        // 1 blok, liczba wątków = liczba permutacji
        evaluate_permutations<<<1, num_perms>>>(layer, d_map, d_results);
        cudaDeviceSynchronize();

        // Pobranie wyników
        std::vector<PermutationResult> host_results(num_perms);
        cudaMemcpy(host_results.data(), d_results, num_perms * sizeof(PermutationResult), cudaMemcpyDeviceToHost);

        // Wybór najlepszego rozwiązania (CPU)
        int best_idx = 0;
        int min_cost = 999999;

        for (int r = 0; r < num_perms; r++) {
            if (host_results[r].cost < min_cost) {
                min_cost = host_results[r].cost;
                best_idx = r;
            }
        }

        // D. Interpretacja i Wypisanie
        PermutationResult best = host_results[best_idx];
        std::cout << "Warstwa " << i << ": Najlepsza permutacja kosztuje " << min_cost << " SWAPow.\n";
        std::cout << "  Kolejnosc wykonywania: ";

        // Symulacja wykonania najlepszej permutacji w celu aktualizacji mapy na CPU i wypisania wyniku
        for (int k = 0; k < layer.num_gates; k++) {
            int gate_idx = best.permutation[k];
            Gate g = layer.gates[gate_idx];

            if (g.type == SINGLE_QUBIT) {
                std::cout << "[B" << g.id << ":SINGLE_QUBIT q" << g.control << "] ";
            } else {
                int phys_c = logical_to_physical[g.control];
                int phys_t = logical_to_physical[g.target];

                // Sprawdź czy potrzebny SWAP (logika identyczna jak w kernelu)
                if (phys_c != CENTER_QUBIT && phys_t != CENTER_QUBIT) {
                    // Potrzebny SWAP
                    int logical_at_center = -1;
                    for(int q=0; q<NUM_QUBITS; q++) if(logical_to_physical[q] == CENTER_QUBIT) logical_at_center = q;

                    // Wykonaj SWAP w mapowaniu
                    std::cout << ">>SWAP(L" << logical_at_center << ", L" << g.control << ")>> ";

                    logical_to_physical[logical_at_center] = phys_c;
                    logical_to_physical[g.control] = CENTER_QUBIT;
                }
                std::cout << "[B" << g.id << ":TWO_QUBIT q" << g.control << ",q" << g.target << "] ";
            }
        }
        std::cout << "\n";

        cudaFree(d_results);
    }

    std::cout << "------------------------------------------------\n";
    std::cout << "Koniec mapowania.\n";

    cudaFree(d_map);
    return 0;
}