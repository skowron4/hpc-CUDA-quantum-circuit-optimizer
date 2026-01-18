#include <iostream>
#include <vector>
#include "CircuitGenerator.h"
#include "Group.h"
#include <cuda_runtime.h>
#include <algorithm>
#include <fstream>
#include <chrono>


#define MAX_GATES 90
#define MAX_SWAPS 45
#define NUM_QUBITS 5


#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true) {
    if (code != cudaSuccess) {
        fprintf(stderr,"CUDA Error: %s %s %d\n", cudaGetErrorString(code), file, line);
        if (abort) exit(code);
    }
}
// ----------------------------------------------------------------------
// KERNEL CUDA
// ----------------------------------------------------------------------
__global__ void optimizeGroupsShared(
    const int* __restrict__ q1_array,
    const int* __restrict__ q2_array,
    int num_group,
    const int* __restrict__ allowed_swap_indices,
    int num_allowed_swaps,
    int initial_center_qubit,
    int* results_cost,
    unsigned long long* best_results_indices,
    unsigned long long total_configs_to_check
) {
// --- SHARED MEMORY ---
    __shared__ int s_min_cost[256];
    __shared__ unsigned long long s_best_idx[256];
    __shared__ int s_q1[MAX_GATES];
    __shared__ int s_q2[MAX_GATES];
    __shared__ int s_allowed_swaps[MAX_SWAPS];

    int t_id = threadIdx.x;
    s_min_cost[t_id] = INT_MAX;
    s_best_idx[t_id] = 0;
    unsigned long long id = (unsigned long long) blockIdx.x * blockDim.x + threadIdx.x;
    unsigned long long stride = (unsigned long long)gridDim.x * blockDim.x;
    int local_min_cost = INT_MAX;
    unsigned long long local_best_config = 0;

    // Ładowanie grup do pamięci współdzielonej
    for (int i = t_id; i < num_group; i += blockDim.x) {
        s_q1[i] = q1_array[i];
        s_q2[i] = q2_array[i];
    }
    // Ładowanie indeksów zamian
    for (int i = t_id; i < num_allowed_swaps; i += blockDim.x) s_allowed_swaps[i] = allowed_swap_indices[i];

    // BARDZO WAŻNE: Bariera synchronizacyjna.
    // Musimy czekać, aż wszystkie wątki skończą ładować dane do shared memory,
    // zanim ktokolwiek zacznie ich używać.
    __syncthreads();

    // --- SYMULACJA KOSZTU ---
    int cost = INT_MAX;

    for (unsigned long long config_mask = id; config_mask < total_configs_to_check; config_mask += stride) {
        int local_q1[MAX_GATES];
        int local_q2[MAX_GATES];

        for (int i = 0; i < num_group; ++i) {
            local_q1[i] = s_q1[i];
            local_q2[i] = s_q2[i];
        }

        for (int i = 0; i < num_allowed_swaps; ++i) {
            if ((config_mask >> i) & 1) {
                int idx = s_allowed_swaps[i];
                if (idx < num_group - 1) {
                    int temp_q1 = local_q1[idx];
                    int temp_q2 = local_q2[idx];

                    local_q1[idx] = local_q1[idx + 1];
                    local_q2[idx] = local_q2[idx + 1];

                    local_q1[idx + 1] = temp_q1;
                    local_q2[idx + 1] = temp_q2;
                }
            }
        }

        cost = 0;
        int current_center = initial_center_qubit;
        int q1, q2;
        bool q1_is_center, q2_is_center;

        for (int i = 0; i < num_group; ++i) {
            q1 = local_q1[i];
            q2 = local_q2[i];
            q1_is_center = (q1 != current_center);
            q2_is_center = (q2 != current_center);

            if (q1_is_center && q2_is_center) {
                int next_target = -1;
                bool end = false;

                int next_q1, next_q2;
                bool uses_q1, uses_q2;

                for (int j = i + 1; !end && j < num_group; ++j) {
                    next_q1 = local_q1[j];
                    next_q2 = local_q2[j];
                    uses_q1 = (next_q1 == q1 || next_q2 == q1);
                    uses_q2 = (next_q1 == q2 || next_q2 == q2);

                    if (uses_q1 && !uses_q2) {
                        next_target = q1;
                        end = true;
                    }
                    if (uses_q2 && !uses_q1) {
                        next_target = q2;
                        end = true;
                    }
                }

                current_center = (next_target != -1) ? next_target : q1;
                cost++;
            }
        }

        if (cost < local_min_cost) {
            local_min_cost = cost;
            local_best_config = config_mask;
        }
    }
    s_min_cost[t_id] = local_min_cost;
    s_best_idx[t_id] = local_best_config;

    __syncthreads();

    for (unsigned int s = blockDim.x >> 1; s > 0; s >>= 1) {
        if (t_id < s) {
            if (s_min_cost[t_id + s] < s_min_cost[t_id]) {
                s_min_cost[t_id] = s_min_cost[t_id + s];
                s_best_idx[t_id] = s_best_idx[t_id + s];
            }
        }
        __syncthreads();
    }

    if (t_id == 0) {
        results_cost[blockIdx.x] = s_min_cost[t_id];
        best_results_indices[blockIdx.x] = s_best_idx[t_id];
    }
}

void printWithSwaps(const std::vector<Gate>& circuit, int num_qubits) {
    if (circuit.empty()) return;

    std::vector<std::stringstream> lines(num_qubits);
    for (int i = 0; i < num_qubits; ++i) lines[i] << "q" << i << ": ";
    int max_len;

    for (const auto& gate : circuit) {
        max_len = 0;
        for (const auto& ss : lines)
            if (ss.str().length() > max_len) max_len = ss.str().length();

        for (auto& ss : lines)
            while ((int)ss.str().length() < max_len) ss << "-";

        if (gate.type == SINGLE_QUBIT) {
            for (int i = 0; i < num_qubits; ++i) {
                if (i == gate.target) {
                    std::string g_name = gate.name;
                    if(g_name.length() > 3) g_name = g_name.substr(0, 3);
                    std::transform(g_name.begin(), g_name.end(), g_name.begin(), ::toupper);
                    lines[i] << "-[" << g_name << "]-";
                } else {
                    lines[i] << "-----";
                }
            }
        }
        else if (gate.type == TWO_QUBIT) {
            int c = gate.control;
            int t = gate.target;
            int min_q = std::min(c, t);
            int max_q = std::max(c, t);
            bool is_swap = (gate.name == "SWAP");

            for (int i = 0; i < num_qubits; ++i) {
                if (i > min_q && i < max_q) {
                    lines[i] << "--|--";
                }
                else if (is_swap && (i == c || i == t)) {
                    lines[i] << "--x--"; // SWAP
                }
                else if (!is_swap && i == c) {
                    lines[i] << "--o--"; // Control
                }
                else if (!is_swap && i == t) {
                    lines[i] << "-(+)-"; // Target
                }
                else {
                    lines[i] << "-----";
                }
            }
        }
    }

    std::cout << "\n=== FINALNY OBWOD Z BRAMKAMI SWAP ===\n";
    for (int i = 0; i < num_qubits; ++i) {
        std::cout << lines[i].str() << "\n";
    }
    std::cout << "=====================================\n";
}

__global__ void findGlobalBest(
        const int *__restrict__ block_costs,
        const unsigned long long *__restrict__ block_indices,
        int num_blocks,
        int *final_cost,
        unsigned long long *final_index
) {
    __shared__ int s_min_cost[1024];
    __shared__ unsigned long long s_best_idx[1024];

    int t_id = threadIdx.x;
    int local_min = INT_MAX;
    unsigned long long local_idx = 0;

    for (int i = t_id; i < num_blocks; i += blockDim.x) {
        int val = block_costs[i];
        if (val < local_min) {
            local_min = val;
            local_idx = block_indices[i];
        }
    }

    s_min_cost[t_id] = local_min;
    s_best_idx[t_id] = local_idx;
    __syncthreads();

    for (unsigned int s = blockDim.x >> 1; s > 0; s >>= 1) {
        if (t_id < s && s_min_cost[t_id + s] < s_min_cost[t_id]) {
            s_min_cost[t_id] = s_min_cost[t_id + s];
            s_best_idx[t_id] = s_best_idx[t_id + s];
        }
        __syncthreads();
    }

    if (t_id == 0) {
        *final_cost = s_min_cost[0];
        *final_index = s_best_idx[0];
    }
}

// ----------------------------------------------------------------------
// HOST (CPU)
// ----------------------------------------------------------------------

int main() {
    CircuitGenerator gen(NUM_QUBITS, 24);
    gen.generate_random(MAX_GATES, 0.5);

    std::vector<Gate> my_circuit = gen.get_circuit();
    std::vector<Group> host_group = Group::fromCircuit(my_circuit);
    std::vector<int> h_allowed_swaps = Group::getGroupPermutation(host_group);

    gen.print();
    Group::printGroups(host_group, NUM_QUBITS);
    const int hard_limit_swaps = 60;

    if (h_allowed_swaps.size() > hard_limit_swaps) {
        std::cout << "!!! UWAGA: Zbyt duzo kombinacji (>" << hard_limit_swaps << ") !!!\n";
        exit(1);
    }

    std::cout << "Dozwolone permutacje grup ("<< h_allowed_swaps.size() << "):\n";
    for (const auto &perm: h_allowed_swaps) std::cout << perm << " ";
    std::cout << "\n";

    int num_groups = host_group.size();
    int num_swaps = h_allowed_swaps.size();

    unsigned long long num_configs = 1ULL << num_swaps;

    // 3. KONWERSJA: Group (CPU) -> GPUGroup (GPU Friendly)
    std::vector<int> h_q1(num_groups);
    std::vector<int> h_q2(num_groups);

    for(int i=0; i<num_groups; ++i) {
        h_q1[i] = host_group[i].control_two_qubit_gate;
        h_q2[i] = host_group[i].target_two_qubit_gate;
    }

    // 4. Alokacja na GPU
    int *d_allowed_swaps, *d_results_cost, *d_q1, *d_q2, *d_final_cost;
    unsigned long long* d_result_indices, *d_final_index;
    unsigned int threads_per_block = 256, blocks_per_grid = 81920;

    gpuErrchk(cudaMalloc(&d_q1, num_groups * sizeof(int)))
    gpuErrchk(cudaMalloc(&d_q2, num_groups * sizeof(int)))
    gpuErrchk(cudaMalloc(&d_allowed_swaps, num_swaps * sizeof(int)))

    // Bufory pośrednie (wyniki z bloków)
    gpuErrchk(cudaMalloc(&d_results_cost, blocks_per_grid * sizeof(int)))
    gpuErrchk(cudaMalloc(&d_result_indices, blocks_per_grid * sizeof(unsigned long long)))

    // Bufory końcowe (1 int, 1 long long)
    gpuErrchk(cudaMalloc(&d_final_cost, sizeof(int)))
    gpuErrchk(cudaMalloc(&d_final_index, sizeof(unsigned long long)))

    // Kopiowanie danych (SoA)
    gpuErrchk(cudaMemcpy(d_q1, h_q1.data(), num_groups * sizeof(int), cudaMemcpyHostToDevice))
    gpuErrchk(cudaMemcpy(d_q2, h_q2.data(), num_groups * sizeof(int), cudaMemcpyHostToDevice))
    gpuErrchk(cudaMemcpy(d_allowed_swaps, h_allowed_swaps.data(), num_swaps * sizeof(int), cudaMemcpyHostToDevice))

    // 5. Uruchomienie
    int initial_center_qubit = 0;

    std::cout << "Start kernela dla " << num_groups << " warstw..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    optimizeGroupsShared<<<blocks_per_grid, threads_per_block>>>(
            d_q1,
            d_q2,
            num_groups,
            d_allowed_swaps,
            num_swaps,
            initial_center_qubit,
            d_results_cost,
            d_result_indices,
            num_configs
    );
    findGlobalBest<<<1, 1024>>>(
            d_results_cost, d_result_indices, blocks_per_grid,
            d_final_cost, d_final_index
    );

    gpuErrchk(cudaDeviceSynchronize())
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    gpuErrchk(cudaPeekAtLastError())

    std::cout << "Czas obliczen: " << elapsed.count() << "s\n";
    // 6. Wyniki
    unsigned long long best_config = 0;
    int min_cost = INT_MAX;

    gpuErrchk(cudaMemcpy(&min_cost, d_final_cost, sizeof(int), cudaMemcpyDeviceToHost))
    gpuErrchk(cudaMemcpy(&best_config, d_final_index, sizeof(unsigned long long), cudaMemcpyDeviceToHost))

    std::cout << "Najlepsza konfiguracja  (ID): " << best_config << " Koszt SWAPow: " << min_cost << "\n";

    cudaFree(d_q1);
    cudaFree(d_q2);
    cudaFree(d_allowed_swaps);
    cudaFree(d_results_cost);
    cudaFree(d_result_indices);
    cudaFree(d_final_cost);
    cudaFree(d_final_index);

    std::vector<Group> final_host_groups = host_group;

    for (int i = 0; i < num_swaps; ++i) {
        if ((best_config >> i) & 1) {
            int idx = h_allowed_swaps[i];
            if (idx < num_groups - 1)
                std::swap(final_host_groups[idx], final_host_groups[idx + 1]);
        }
    }

    std::cout << "Kolejnosc ID warstw: ";
    for(const auto& g : final_host_groups) std::cout << g.id << " ";
    std::cout << "\n";

    std::vector<Gate> full_circuit_with_swaps;
    int current_center = initial_center_qubit;
    bool needed_swap;
    int chosen_swap_target;
    int q1, q2;

    for (int i = 0; i < num_groups; ++i) {
        q1 = final_host_groups[i].control_two_qubit_gate;
        q2 = final_host_groups[i].target_two_qubit_gate;

        needed_swap = false;
        chosen_swap_target = -1;

        if (!(q1 == current_center || q2 == current_center)) {
            needed_swap = true;

            int next_target = -1;
            bool end = false;

            for (int j = i + 1; !end && j < num_groups; ++j) {
                int next_q1 = final_host_groups[j].control_two_qubit_gate;
                int next_q2 = final_host_groups[j].target_two_qubit_gate;

                bool uses_q1 = (next_q1 == q1 || next_q2 == q1);
                bool uses_q2 = (next_q1 == q2 || next_q2 == q2);

                if (uses_q1 && !uses_q2) { next_target = q1; end = true; }
                if (uses_q2 && !uses_q1) { next_target = q2; end = true; }
            }

            if (next_target != -1) chosen_swap_target = next_target;
            else chosen_swap_target = q1;
        }

        if (needed_swap) {
            Gate swap_gate;
            swap_gate.type = TWO_QUBIT;
            swap_gate.name = "SWAP";
            swap_gate.control = current_center;
            swap_gate.target = chosen_swap_target;

            full_circuit_with_swaps.push_back(swap_gate);

            current_center = chosen_swap_target;
        }

        full_circuit_with_swaps.insert(
                full_circuit_with_swaps.end(),
                final_host_groups[i].gates.begin(),
                final_host_groups[i].gates.end()
        );
    }

    printWithSwaps(full_circuit_with_swaps, NUM_QUBITS);
    return 0;
}