cmake_minimum_required(VERSION 3.20)

project(obliczenia LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)
find_package(CUDAToolkit REQUIRED)

# --- FIX: Ręczne znajdowanie NVTX (zamiast polegać na CUDA::nvToolsExt) ---
find_library(NVTX_LIB
        NAMES nvToolsExt64_1 nvToolsExt
        HINTS ${CMAKE_CUDA_IMPLICIT_LINK_DIRECTORIES}
        $ENV{CUDA_PATH}/lib/x64
        DOC "Biblioteka NVIDIA Tools Extension"
)

if (NOT NVTX_LIB)
    message(WARNING "Nie znaleziono biblioteki NVTX (nvToolsExt). Profilowanie może nie działać.")
else()
    message(STATUS "Znaleziono NVTX: ${NVTX_LIB}")
endif()

set(COMMON_SOURCES
        CircuitGenerator.cpp
        CircuitGenerator.h
        Gate.h
        Group.cpp
        Group.h
        CPUAlgorithm.cpp
        CPUAlgorithm.h
)

# ==========================================
# 1. Target CPU
# ==========================================
add_executable(run_cpu
        main2.cpp
        ${COMMON_SOURCES}
)
target_link_libraries(run_cpu PRIVATE OpenMP::OpenMP_CXX)

# Linkujemy zmienną ${NVTX_LIB} zamiast targetu CUDA::nvToolsExt
if(NVTX_LIB)
    target_link_libraries(run_cpu PRIVATE ${NVTX_LIB})
endif()


# ==========================================
# 2. Target GPU
# ==========================================
add_executable(run_gpu
        main.cu
        ${COMMON_SOURCES}
)
set_target_properties(run_gpu PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES "native"
)
target_link_libraries(run_gpu PRIVATE OpenMP::OpenMP_CXX)

if(NVTX_LIB)
    target_link_libraries(run_gpu PRIVATE ${NVTX_LIB})
endif()


# ==========================================
# 3. Target Hybrid
# ==========================================
add_executable(run_hybrid
        main3.cu
        GPUKernel.cu
        ${COMMON_SOURCES}
)
set_target_properties(run_hybrid PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES "native"
)
target_link_libraries(run_hybrid PRIVATE OpenMP::OpenMP_CXX)

if(NVTX_LIB)
    target_link_libraries(run_hybrid PRIVATE ${NVTX_LIB})
endif()