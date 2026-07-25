# Quantum Circuit Transpilation Optimizer

![C++ Optimization](https://img.shields.io/badge/C++-Optimization-007acc?style=flat-square)
![Domain Quantum Computing](https://img.shields.io/badge/Domain-Quantum%20Computing-800080?style=flat-square)
![Status Master's Thesis](https://img.shields.io/badge/Status-Master's%20Thesis-ffd700?style=flat-square)

## Project Description
**Quantum Circuit Transpilation Optimizer** is a tool written in C++ and CUDA, designed to optimize quantum circuits by minimizing the cost of SWAP gates. The project was created for mapping of logical qubits to the physical topology of quantum processors. 

The software utilizes parallel computing techniques on multi-core processors and graphics cards, as well as evolutionary algorithm to solve the problem of routing logical qubits on a physical topology.

## Key Features
*   **Brute-force Search:** 
    *   Single-threaded version (Single-Core).
    *   Multi-threaded version using OpenMP.
*   **Evolutionary Optimization (Differential Evolution):** A metaheuristic algorithm that allows finding solutions for very large search spaces.
*   **GPU Acceleration (CUDA):** Optimization using graphics cards, utilizing constant memory (`__constant__`) and shared memory (`__shared__`) to reduce latency.
*   **Hybrid Processing (CPU + GPU):** Asynchronous, parallel workload distribution between the processor and graphics card to fully utilize hardware resources.
*   **Results Logging:** Automatic logging of statistics, execution times, and best configurations directly to `.scsv` files.

## System and Hardware Requirements
To compile and run the project, ensure you have:
*   A C++ compiler supporting the C++11 standard or newer.
*   **OpenMP** library support for CPU multi-threading.
*   **CUDA Toolkit** (nvcc) and a compatible NVIDIA graphics card, if you want to use GPU acceleration.

## Structure and Execution
The project consists of several main execution modules:

1.  **CPU Algorithms** (`CPUAlgorithm.cpp / .h`): Contains the logic for single-threaded, multi-threaded, and evolutionary approaches.
2.  **GPU Computations** (`runGPUCalculation`): Implementation in CUDA C++ that distributes blocks and threads to test permutations.
3.  **Hybrid Tasks**: Defining the task distribution ratio (e.g., `gpu_ratio = 1.0` or `0.5`) between the CPU and GPU using `std::future` and `std::async` objects.

**Note on data logging:** 
By default, the application saves results to the `data/` folder in the main project directory. Ensure that the appropriate directory structure exists before running the algorithms.
