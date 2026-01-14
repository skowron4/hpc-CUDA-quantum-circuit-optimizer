#include <iostream>
#include <vector>
#include "CircuitGenerator.h"
#include "Group.h"
#include <cuda_runtime.h>
#include <algorithm>


#define MAX_GATES 20
#define MAX_SWAPS 16
#define NUM_QUBITS 5


struct GPUGroup {
    int id;
    int q1;
    int q2;
};


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
const GPUGroup* __restrict__ initial_groups,
    int num_group,
    const int* __restrict__ allowed_swap_indices,
    int num_allowed_swaps,
    int initial_center_qubit,
    int* results_cost
) {
// --- SHARED MEMORY ---
    __shared__ GPUGroup s_initial_group[MAX_GATES];
    __shared__ int      s_allowed_swaps[MAX_SWAPS];


    int tid = threadIdx.x;

    // Ładowanie grup do pamięci współdzielonej
    for (int i = tid; i < num_group; i += blockDim.x) {
        s_initial_group[i] = initial_groups[i];
    }
    // Ładowanie indeksów zamian
    for (int i = tid; i < num_allowed_swaps; i += blockDim.x) {
        s_allowed_swaps[i] = allowed_swap_indices[i];
    }

    // BARDZO WAŻNE: Bariera synchronizacyjna.
    // Musimy czekać, aż wszystkie wątki skończą ładować dane do shared memory,
    // zanim ktokolwiek zacznie ich używać.
    __syncthreads();

    // --- LOGIKA PERMUTACJI (Unikalna dla wątku) ---
    int config_mask = blockIdx.x * blockDim.x + threadIdx.x;
    if (config_mask >= (1 << num_allowed_swaps)) return;

    // Kopiujemy z Shared do Rejestrów
    GPUGroup local_groups[MAX_GATES];
    for(int i = 0; i < num_group; ++i) {
        local_groups[i] = s_initial_group[i];
    }


    // Aplikacja zamian (Reordering warstw)
    for (int i = 0; i < num_allowed_swaps; ++i) {
        if ((config_mask >> i) & 1) {
            int idx = s_allowed_swaps[i];
            if (idx < num_group - 1) {
                // Swapujemy całe struktury GPUGroup
                GPUGroup temp = local_groups[idx];
                local_groups[idx] = local_groups[idx + 1];
                local_groups[idx + 1] = temp;
            }
        }
    }

    // --- SYMULACJA KOSZTU ---
    int cost = 0;
    int current_center = initial_center_qubit;

    for (int i = 0; i < num_group; ++i) {
        // Mapowanie pól ze struktury Group na logikę q1/q2
        int q1 = local_groups[i].q1;
        int q2 = local_groups[i].q2;

        // Sprawdzamy, czy obecny centralny kubit pasuje do bramki
        bool q1_is_center = (q1 == current_center);
        bool q2_is_center = (q2 == current_center);

        if (q1_is_center || q2_is_center) continue;// Wszystko OK, bramka wykonalna (koszt CNOT/Gate pomijamy, liczymy tylko narzut SWAP)
        else {
            // Żaden z kubitów bramki nie jest w centrum. Musimy wykonać SWAP.
            // Koszt wstawienia nowego kubitu do centrum.
            cost++;

            // LOGIKA HEURYSTYCZNA (Look-ahead):
            // Musimy wstawić 'q1' albo 'q2' do centrum. Który wybrać?
            // Patrzymy na NASTĘPNĄ bramkę (i+1).

            int next_target = -1; // Brak preferencji
            bool end = false;

            for (int j = i + 1; !end && j < num_group; ++j) {
                int next_q1 = local_groups[j].q1;
                int next_q2 = local_groups[j].q2;

                // Sprawdzamy, czy przyszła bramka korzysta z naszych kandydatów (q1 lub q2)
                bool uses_q1 = (next_q1 == q1 || next_q2 == q1);
                bool uses_q2 = (next_q1 == q2 || next_q2 == q2);

                // Logika decyzji:
                if (uses_q1 && !uses_q2) {
                    // 'q1' jest potrzebny wcześniej niż 'q2' -> wybieramy 'q1'
                    next_target = q1;
                    end = true;
                }
                if (uses_q2 && !uses_q1) {
                    // 'q2' jest potrzebny wcześniej niż 'q1' -> wybieramy 'q2'
                    next_target = q2;
                    end = true;
                }
            }

            // Decyzja
            if (next_target != -1) current_center = next_target;
            else current_center = q1;
        }
    }

    results_cost[config_mask] = cost;
}

// ----------------------------------------------------------------------
// HOST (CPU)
// ----------------------------------------------------------------------

int main() {
    CircuitGenerator gen(NUM_QUBITS);
    // gen.single(0, "s1");
    // gen.single(1, "s2");
    // gen.two(0, 1, "d1");
    // gen.two(1,2, "d2");
    // gen.single(0, "s3");

    gen.generate_random(MAX_GATES);

    std::vector<Gate> my_circuit = gen.get_circuit();

    std::vector<Group> host_group = Group::fromCircuit(my_circuit);

    std::vector<int> h_allowed_swaps = Group::getGroupPermutation(host_group);

    std::cout << "Wygenerowano " << host_group.size() << " warstw:\n";
    for (const auto& group : host_group) {
        std::cout << group << std::endl;
    }

    for (const auto& perm : h_allowed_swaps) {
        std::cout << perm << std::endl;
    }

    int num_groups = (int) host_group.size();
    int num_swaps = (int) h_allowed_swaps.size();
    int num_configs = 1 << num_swaps;

    // 3. KONWERSJA: Group (CPU) -> GPUGroup (GPU Friendly)
    std::vector<GPUGroup> h_gpu_groups;
    h_gpu_groups.reserve(num_groups);

    for (const auto& group : host_group) {
        GPUGroup gl;
        gl.id = group.id;
        gl.q1 = group.control_two_qubit_gate; // Mapowanie pól
        gl.q2 = group.target_two_qubit_gate;  // Mapowanie pól
        h_gpu_groups.push_back(gl);
    }

    // 4. Alokacja na GPU
    GPUGroup* d_groups;
    int* d_allowed_swaps;
    int* d_results;

    gpuErrchk(cudaMalloc(&d_groups, num_groups * sizeof(GPUGroup)));
    gpuErrchk(cudaMalloc(&d_allowed_swaps, num_swaps * sizeof(int)));
    gpuErrchk(cudaMalloc(&d_results, num_configs * sizeof(int)));

    // Kopiujemy PRZEKONWERTOWANE struktury
    gpuErrchk(cudaMemcpy(d_groups, h_gpu_groups.data(), num_groups * sizeof(GPUGroup), cudaMemcpyHostToDevice));
    gpuErrchk(cudaMemcpy(d_allowed_swaps, h_allowed_swaps.data(), num_swaps * sizeof(int), cudaMemcpyHostToDevice));

    // 5. Uruchomienie
    int threadsPerBlock = 256;
    int blocksPerGrid = (num_configs + threadsPerBlock - 1) / threadsPerBlock;
    int initial_center_qubit = 0;

    std::cout << "Start kernela dla " << num_groups << " warstw..." << std::endl;

    optimizeGroupsShared<<<blocksPerGrid, threadsPerBlock>>>(
            d_groups,
            num_groups,
            d_allowed_swaps,
            num_swaps,
            initial_center_qubit,
            d_results
    );
    gpuErrchk(cudaDeviceSynchronize());

    // 6. Wyniki
    std::vector<int> h_results(num_configs);
    gpuErrchk(cudaMemcpy(h_results.data(), d_results, num_configs * sizeof(int), cudaMemcpyDeviceToHost));

    int best_config = -1;
    int min_cost = 999999;

    for (int i = 0; i < num_configs; ++i) {
        if (h_results[i] < min_cost) {
            min_cost = h_results[i];
            best_config = i;
        }
    }

    std::cout << "Najlepsza konfiguracja: " << best_config << " Koszt: " << min_cost << std::endl;

    // Odtworzenie najlepszego ułożenia (na CPU, żeby zobaczyć co wyszło)
    std::cout << "Kolejnosc ID warstw w najlepszym rozwiazaniu: ";
    std::vector<GPUGroup> final_layers = h_gpu_groups;

    // Aplikujemy zamiany lokalnie na CPU żeby wypisać wynik
    for (int i = 0; i < num_swaps; ++i) {
        if ((best_config >> i) & 1) {
            int idx = h_allowed_swaps[i];
            if (idx < num_groups - 1)
                std::swap(final_layers[idx], final_layers[idx+1]);
        }
    }
    for(const auto& l : final_layers) std::cout << l.id << " ";
    std::cout << std::endl;

    cudaFree(d_groups);
    cudaFree(d_allowed_swaps);
    cudaFree(d_results);

    return 0;
}