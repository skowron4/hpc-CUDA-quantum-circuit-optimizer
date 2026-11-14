# Quantum Circuit Transpilation Optimizer

![C++17](https://img.shields.io/badge/C%2B%2B17-Optimization-00599C) ![CUDA](https://img.shields.io/badge/CUDA_13.1-Parallel_Computing-76B900) ![OpenMP](https://img.shields.io/badge/OpenMP-Multithreading-blue) ![Domain](https://img.shields.io/badge/Domain-Quantum_Computing_%7C_HPC-purple) ![Platform](https://img.shields.io/badge/Platform-Windows_x86__64_%7C_SM__86-informational) ![Status](https://img.shields.io/badge/Status-Master's_Thesis-yellow)

## Abstract
Quantum Circuit Transpilation Optimizer is a C++17 and CUDA C++17 combinatorial search engine designed to minimize the insertion of `SWAP` gates in synthetic quantum circuits under a moving center-qubit routing model. The system partitions circuits into two-qubit gate groups, identifies non-overlapping commutative adjacent pairs ($k$), and explores the resulting $2^k$ permutation space. It provides four execution backends: single-threaded CPU exhaustive search, OpenMP multi-threaded exhaustive search, GPU-accelerated exhaustive search via CUDA, and a random-key Differential Evolution (`DE/rand/1/bin`) metaheuristic for high-dimensional spaces.

## System Architecture & Pipeline
1. **Circuit Synthesis (`CircuitGenerator`)**: Generates a flat `std::vector<Gate>` using `std::mt19937` for a parameterized number of qubits and target `CX` gates. At each step, single-qubit gates (`h`, `x`, `y`) are emitted with probability $p$, and two-qubit `CX` gates with probability $1 - p$.
2. **DAG Partitioning & Grouping (`Group::fromCircuit`)**: Aggregates gates into $G$ sequential `Group` structures (where $G$ equals the `CX` gate count). Each `Group` encapsulates one two-qubit `CX` gate and buffers preceding single-qubit operations acting on its target or control wires via an ownership map (`std::map<int, int>`).
3. **Commutative Pair Extraction (`Group::getGroupPermutation`)**: Scans adjacent groups $(i, i+1)$. If their `CX` gates share no qubits, the index $i$ is appended to `allowed_swaps` and $i+1$ is skipped. Because extracted pairs are disjoint, all swap operations commute, mapping the valid reordering space to a $k$-bit integer mask ($2^k$ states, where $k = \text{allowed\_swaps.size()}$).
4. **Search Space Exploration**: Evaluates candidate bitmasks across $[0, 2^k)$ using one of four solvers:
   - `solveSerial` / `solveParallel`: Exhaustive host-side evaluation.
   - `runGPUCalculation`: Exhaustive device-side evaluation across 1,507,328 threads (`5888` blocks x `256` threads) using a grid-stride loop.
   - `run_hybrid`: Asynchronous static range partitioning (`gpu_ratio`) between host OpenMP threads and the CUDA device via `std::async(std::launch::async)`.
   - `solveDE`: Heuristic search using `DE/rand/1/bin` with continuous weight vectors $\mathbf{w} \in [0, 1]^G$, where bit $i$ is enabled iff $w_{\text{swap}[i]} > w_{\text{swap}[i]+1}$.
5. **Cost Evaluation & Look-Ahead Routing (`calculateCost` / `optimizeGroupsFast`)**: Applies the candidate bitmask to an index permutation and traverses the groups sequentially while tracking `current_center` (initialized to qubit 0). If neither qubit of the active group matches `current_center`, the cost increments by one `SWAP`. The next center qubit is selected via a forward look-ahead scan identifying the first subsequent group that utilizes exclusively one of the current gate's two qubits (`uses_q1 ^ uses_q2`), defaulting to the control qubit. Time complexity per mask is $\mathcal{O}(k + G^2)$ in the worst case.
6. **Circuit Reconstruction & Telemetry**: Re-applies the globally optimal bitmask, injects explicit `SWAP` gate nodes into the sequence, prints an ASCII circuit representation, and logs execution times and minimum costs to `../wyniki_YYYYMMDD_HHMMSS.csv`.

## Engineering & Performance Highlights
- **Data Layout (AoS vs. SoA) & Constant Memory Caching**: On the host, circuit groups are stored as an Array of Structures (`std::vector<Group>`, 40 bytes per element in MSVC x64 Release). For GPU execution, the hot fields (qubit indices and swap positions) are projected into a Structure of Arrays (SoA) of 8-bit integers (`int8_t`) and uploaded to CUDA `__constant__` memory (`c_initial_q1[160]`, `c_initial_q2[160]`, `c_allowed_swaps[125]`), consuming 445 bytes in `cmem[3]` for broadcast caching across warps.
- **Register Pressure & Local Memory Trade-offs**: Compiled for `sm_86` (`nvcc 13.1 -O3`), `optimizeGroupsFast` allocates 255 registers per thread with 0 register spills. Because per-thread state arrays `local_q1[160]` and `local_q2[160]` are indexed dynamically at runtime during permutation and look-ahead, the compiler places them in a 320-byte local memory stack frame (cached in L1/L2). At 255 registers x 256 threads (65,280 registers/block out of 65,536 per SM on `sm_86`), theoretical occupancy is bounded to 1 block (8 warps, 16.7%) per SM.
- **Two-Stage Shared Memory Reduction**: Each thread maintains a register-local minimum across its grid-stride iterations. Block-level reduction is performed in 3,072 bytes of `__shared__` memory (`int[256]` + `unsigned long long[256]`) via a power-of-two tree reduction (`s >>= 1` with `__syncthreads()`), emitting 5,888 block minima. A secondary kernel (`findGlobalBest<<<1, 256>>>`, 12 registers/thread) reduces the block outputs to a single global optimum, avoiding global atomic contention.
- **Bitwise Intrinsics & Zero-Allocation Hot Loops**: The CPU exhaustive solvers pre-allocate a single `std::vector<int>` index buffer per OpenMP thread and reset it via `std::copy` on each iteration, performing zero heap allocations inside the $2^k$ loop. Active swap bits are decoded in $\mathcal{O}(\text{popcount}(\text{mask}))$ steps using `mask &= mask - 1` paired with compiler intrinsics (`_BitScanForward` on MSVC, `__builtin_ctz` on GCC/Clang).
- **Differential Evolution Complexity Reduction**: For circuits where $2^k$ is intractable for exhaustive enumeration, `solveDE` (`pop_size = 100`, `max_gen = 500`, $F = 0.8$, $CR = 0.9$) bounds the evaluation budget to 50,100 cost calls—evaluating 0.30% of the $2^{24}$ space for $G = 80$ and enabling search over $G = 200$ ($k = 53$, $2^{53} \approx 9.01 \times 10^{15}$ states).

## Build & Usage

### Prerequisites
- CMake >= 3.20 and Ninja build tool
- MSVC (`cl.exe` >= 19.44 / Visual Studio 2022) or compatible C++17 host compiler with OpenMP support
- NVIDIA CUDA Toolkit (>= 13.1 recommended) and a CUDA-capable GPU (`CUDA_ARCHITECTURES` is set to `native`)

### Compilation
Execute from an `x64 Native Tools Command Prompt for VS 2022` (ensuring both `cl.exe` and `nvcc` are in `PATH`):

```bash
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target run_cpu run_gpu run_hybrid
```

To inspect PTX register allocation, constant memory usage, and stack frame metrics for `sm_86`:

```bash
nvcc -std=c++17 -O3 -arch=sm_86 -Xptxas -v -c GPUKernel.cu -o GPUKernel.obj
```

### Execution
Run the binaries from inside the build directory (note: `run_cpu` exports CSV telemetry to `../wyniki_<timestamp>.csv` relative to the working directory):

```bash
cd cmake-build-release

# Run CPU benchmark suite (Differential Evolution / OpenMP / Single-Core)
./run_cpu

# Run GPU exhaustive search (G=125, k=36)
./run_gpu

# Run Hybrid asynchronous CPU + GPU solver (G=160, k=43)
./run_hybrid
```
