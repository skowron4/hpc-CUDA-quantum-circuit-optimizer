cmake_minimum_required(VERSION 3.31)
project(obliczenia CUDA)

set(CMAKE_CUDA_STANDARD 17)

add_executable(obliczenia main.cu)

set_target_properties(obliczenia PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)
