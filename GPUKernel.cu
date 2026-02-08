#include <cuda_runtime.h>
#include <vector>
#include <iostream>
#include <climits>
#include "CPUAlgorithm.h"

// --- STREFA KONFIGURACJI ---
#define MAX_GATES 125
#define MAX_SWAPS 125
// RTX 3070 ma 46 SM. Ustawiamy bloki jako wielokrotność SM dla równego obłożenia.
// 46 SM * 64 bloki/SM = 2944. Zwiększamy bufor, by ukryć latencję pamięci.
#define BLOCKS_NUM 46 * 128
#define THREADS_PER_BLOCK 256

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }

inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true) {
    if (code != cudaSuccess) {
        fprintf(stderr,"CUDA Error: %s %s %d\n", cudaGetErrorString(code), file, line);
        if (abort) exit(code);
    }
}

// Używamy __constant__ dla stałych tablic - ultraszybki dostęp broadcast
__constant__ int8_t c_allowed_swaps[MAX_SWAPS];
__constant__ int8_t c_initial_q1[MAX_GATES];
__constant__ int8_t c_initial_q2[MAX_GATES];

__global__ void optimizeGroupsFast(
    int num_group,
    int num_allowed_swaps,
    int initial_center_qubit,
    int* results_cost,
    unsigned long long* best_results_indices,
    unsigned long long start_offset,
    unsigned long long end_offset
) {
    // Redukcja na poziomie bloku
    __shared__ int s_min_cost[THREADS_PER_BLOCK];
    __shared__ unsigned long long s_best_idx[THREADS_PER_BLOCK];

    int t_id = threadIdx.x;

    // Inicjalizacja lokalnych minimów
    int local_min_cost = INT_MAX;
    unsigned long long local_best_config = 0;

    unsigned long long global_tid = (unsigned long long)blockIdx.x * blockDim.x + t_id;
    unsigned long long stride = (unsigned long long)gridDim.x * blockDim.x;

    // Przesunięcie startowe dla tego wątku
    unsigned long long current_mask = start_offset + global_tid;

    // --- GŁÓWNA PĘTLA OBLICZENIOWA ---
    // Grid-Stride Loop
    while (current_mask < end_offset) {

        // Kopia do rejestrów (używamy int8_t by oszczędzać rejestry)
        int8_t local_q1[MAX_GATES];
        int8_t local_q2[MAX_GATES];

        // Kopiowanie z pamięci __constant__ (bardzo szybkie)
        #pragma unroll
        for(int k=0; k<MAX_GATES; ++k){ // Kompilator zoptymalizuje to jeśli num_group < MAX_GATES
            if (k < num_group) {
                local_q1[k] = c_initial_q1[k];
                local_q2[k] = c_initial_q2[k];
            }
        }

        // Aplikowanie swapów
        // Unrollowanie tej pętli pomaga, jeśli num_allowed_swaps jest małe
        for (int i = 0; i < num_allowed_swaps; ++i) {
            if ((current_mask >> i) & 1) {
                // Odczyt z Constant Cache
                int idx = c_allowed_swaps[i];

                // Swap logic
                // Używamy zmiennych tymczasowych w rejestrach
                int8_t t1 = local_q1[idx];
                int8_t t2 = local_q2[idx];
                local_q1[idx] = local_q1[idx+1];
                local_q2[idx] = local_q2[idx+1];
                local_q1[idx+1] = t1;
                local_q2[idx+1] = t2;
            }
        }

        // Obliczanie kosztu
        int cost = 0;
        int current_center = initial_center_qubit;

        for (int i = 0; i < num_group; ++i) {
            int q1 = local_q1[i];
            int q2 = local_q2[i];

            if (q1 != current_center && q2 != current_center) {
                int next_target = -1;

                // Look-ahead optimization
                for (int j = i + 1; j < num_group; ++j) {
                    int nq1 = local_q1[j];
                    int nq2 = local_q2[j];

                    bool uses_q1 = (nq1 == q1 || nq2 == q1);
                    bool uses_q2 = (nq1 == q2 || nq2 == q2);

                    // Logika XOR-opodobna, jeśli znaleźliśmy użycie jednego, ale nie drugiego -> to jest target
                    if (uses_q1 ^ uses_q2) {
                        next_target = uses_q1 ? q1 : q2;
                        break; // Znaleziono, przerywamy pętlę wewnętrzną
                    }
                }

                current_center = (next_target != -1) ? next_target : q1;
                cost++;
            }
            // Wczesne wyjście (opcjonalne, może powodować rozbieżność wątków, testować ostrożnie)
            // if (cost >= local_min_cost) break;
        }

        if (cost < local_min_cost) {
            local_min_cost = cost;
            local_best_config = current_mask;
        }

        current_mask += stride;
    }

    // Zapis do Shared Memory dla redukcji
    s_min_cost[t_id] = local_min_cost;
    s_best_idx[t_id] = local_best_config;
    __syncthreads();

    // Redukcja w Shared Memory (Warp Shuffle byłoby szybsze, ale to jest bezpieczniejsze i czytelne)
    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (t_id < s) {
            if (s_min_cost[t_id + s] < s_min_cost[t_id]) {
                s_min_cost[t_id] = s_min_cost[t_id + s];
                s_best_idx[t_id] = s_best_idx[t_id + s];
            }
        }
        __syncthreads();
    }

    // Zapis wyniku bloku do pamięci globalnej
    if (t_id == 0) {
        results_cost[blockIdx.x] = s_min_cost[0];
        best_results_indices[blockIdx.x] = s_best_idx[0];
    }
}

__global__ void findGlobalBest(
        const int *__restrict__ block_costs,
        const unsigned long long *__restrict__ block_indices,
        int num_blocks,
        int *final_cost,
        unsigned long long *final_index
) {
    // Prosta redukcja wyników z bloków
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

    // Używamy atomics dla uproszczenia przy jednym bloku końcowym lub shared mem reduce
    // Tutaj klasyczna redukcja w shared mem
    __shared__ int s_min[256];
    __shared__ unsigned long long s_idx[256];

    s_min[t_id] = local_min;
    s_idx[t_id] = local_idx;
    __syncthreads();

    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (t_id < s && s_min[t_id + s] < s_min[t_id]) {
            s_min[t_id] = s_min[t_id + s];
            s_idx[t_id] = s_idx[t_id + s];
        }
        __syncthreads();
    }

    if (t_id == 0) {
        *final_cost = s_min[0];
        *final_index = s_idx[0];
    }
}

CPUAlgorithm::Result runGPUCalculation(const CPUAlgorithm::OptimizationData& data, long long start_range, long long end_range) {
    int num_groups = data.initial_groups.size();
    int num_swaps = data.allowed_swaps.size();

    // Przygotowanie danych do __constant__ memory
    std::vector<int8_t> h_q1(num_groups), h_q2(num_groups);
    std::vector<int8_t> h_allowed(num_swaps);

    for(int i=0; i<num_groups; ++i) {
        h_q1[i] = (int8_t)data.initial_groups[i].control_two_qubit_gate;
        h_q2[i] = (int8_t)data.initial_groups[i].target_two_qubit_gate;
    }
    for(int i=0; i<num_swaps; ++i) {
        h_allowed[i] = (int8_t)data.allowed_swaps[i];
    }

    // Kopiowanie do symboli __constant__
    gpuErrchk(cudaMemcpyToSymbol(c_initial_q1, h_q1.data(), num_groups * sizeof(int8_t)));
    gpuErrchk(cudaMemcpyToSymbol(c_initial_q2, h_q2.data(), num_groups * sizeof(int8_t)));
    gpuErrchk(cudaMemcpyToSymbol(c_allowed_swaps, h_allowed.data(), num_swaps * sizeof(int8_t)));

    int blocks = BLOCKS_NUM;
    int threads = THREADS_PER_BLOCK;

    int *d_res_cost, *d_final_cost;
    unsigned long long *d_res_idx, *d_final_idx;

    // Alokujemy tylko pamięć na wyniki (dane wejściowe są w __constant__)
    cudaMalloc(&d_res_cost, blocks * sizeof(int));
    cudaMalloc(&d_res_idx, blocks * sizeof(unsigned long long));
    cudaMalloc(&d_final_cost, sizeof(int));
    cudaMalloc(&d_final_idx, sizeof(unsigned long long));

    int init_max = INT_MAX;
    cudaMemcpy(d_final_cost, &init_max, sizeof(int), cudaMemcpyHostToDevice);

    std::cout << "[GPU RTX 3070] Start obliczen na zakresie: " << start_range << " -> " << end_range << "\n";
    std::cout << "[Info] Blocks: " << blocks << ", Threads: " << threads << ", Using Constant Memory.\n";

    optimizeGroupsFast<<<blocks, threads>>>(
        num_groups, num_swaps, 0,
        d_res_cost, d_res_idx,
        start_range, end_range
    );
    gpuErrchk(cudaGetLastError());

    // Faza redukcji globalnej (jeden blok)
    findGlobalBest<<<1, 256>>>(d_res_cost, d_res_idx, blocks, d_final_cost, d_final_idx);
    gpuErrchk(cudaDeviceSynchronize());

    CPUAlgorithm::Result res;
    cudaMemcpy(&res.min_cost, d_final_cost, sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(&res.best_config, d_final_idx, sizeof(unsigned long long), cudaMemcpyDeviceToHost);

    cudaFree(d_res_cost); cudaFree(d_res_idx); cudaFree(d_final_cost); cudaFree(d_final_idx);

    return res;
}