#include "engine.h"
#include "engine_cuda.h"
#include <iostream>

#ifdef USE_CUDA
#include <cuda_runtime.h>
#endif

bool is_cuda_available() {
#ifdef USE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return (err == cudaSuccess && count > 0);
#else
    return false;
#endif
}

std::string get_cuda_device_name() {
#ifdef USE_CUDA
    int count = 0;
    if (cudaGetDeviceCount(&count) == cudaSuccess && count > 0) {
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
            return std::string(prop.name);
        }
    }
    return "NVIDIA CUDA Device (Not active)";
#else
    return "N/A (Compiled without CUDA toolkit)";
#endif
}

std::unique_ptr<SimulationEngine> create_simulation_engine(bool prefer_cuda, int width, int height) {
#ifdef USE_CUDA
    if (prefer_cuda && is_cuda_available()) {
        auto engine = std::make_unique<CUDAEngine>();
        if (engine->init(width, height)) {
            std::cout << "[INFO] Using Hardware Acceleration: " << engine->get_name() << "\n";
            return engine;
        }
        std::cerr << "[WARN] CUDA initialization failed. Falling back to multi-threaded CPU engine.\n";
    }
#endif

    auto engine = std::make_unique<CPUEngine>();
    engine->init(width, height);
    std::cout << "[INFO] Using Simulation Engine: " << engine->get_name() << "\n";
    return engine;
}
