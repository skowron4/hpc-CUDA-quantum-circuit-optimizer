cmake_minimum_required(VERSION 3.31)

project(obliczenia LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(obliczenia
        main.cu
        CircuitGenerator.cpp
        CircuitGenerator.h
        Gate.h
        Layer.h
        Layer.cpp
)

set_target_properties(obliczenia PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)
