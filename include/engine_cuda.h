#ifndef ENGINE_CUDA_H
#define ENGINE_CUDA_H

#ifdef USE_CUDA

#include "engine.h"
#include <cuda_runtime.h>

class CUDAEngine : public SimulationEngine {
private:
    unsigned char* d_current = nullptr;
    unsigned char* d_next = nullptr;
    unsigned int*  d_pixels = nullptr;
    unsigned int*  h_pixels = nullptr; // Pinned memory
    std::vector<unsigned char> host_grid_buffer;

    int population = 0;
    bool pop_dirty = true;
    std::string device_name;

public:
    CUDAEngine();
    ~CUDAEngine() override;

    bool init(int w, int h) override;
    void step() override;
    void clear() override;
    void randomize(int alive_prob, unsigned int seed) override;
    void set_cell(int x, int y, unsigned char value) override;
    void set_brush(int cx, int cy, int radius, unsigned char value) override;
    void spawn_pattern(int cx, int cy, const Pattern& pattern) override;

    const unsigned int* get_pixels() override;
    int get_population() override;
    std::string get_name() const override;
    bool is_gpu() const override { return true; }

    const unsigned char* get_grid_state() override;
    void set_grid_state(const unsigned char* host_state) override;
};

#endif // USE_CUDA

#endif // ENGINE_CUDA_H
