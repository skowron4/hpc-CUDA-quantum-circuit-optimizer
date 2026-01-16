cmake_minimum_required(VERSION 3.20)

project(obliczenia LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

set(COMMON_SOURCES
        CircuitGenerator.cpp
        CircuitGenerator.h
        Gate.h
        Group.cpp
        Group.h
        CPUAlgorithm.cpp
        CPUAlgorithm.h
)

add_executable(run_cpu
        main2.cpp
        ${COMMON_SOURCES}
)

# Tutaj włączamy obsługę OpenMP dla pliku wykonywalnego CPU
target_link_libraries(run_cpu PRIVATE OpenMP::OpenMP_CXX)

add_executable(run_gpu
        main.cu
        ${COMMON_SOURCES}
)

set_target_properties(run_gpu PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES "native"
)

target_link_libraries(run_gpu PRIVATE OpenMP::OpenMP_CXX)