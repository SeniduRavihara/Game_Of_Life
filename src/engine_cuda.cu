#ifdef USE_CUDA

#include "engine_cuda.h"
#include <cstdio>
#include <iostream>

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(err)); \
        } \
    } while (0)

// ============================================================================
// CUDA Kernels
// ============================================================================

__global__ void cuda_next_state_kernel(const unsigned char* __restrict__ current_state,
                                       unsigned char* __restrict__ next_state,
                                       int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int idx = y * width + x;
    int alive_neighbors = 0;

    #pragma unroll
    for (int dy = -1; dy <= 1; ++dy) {
        int ny = (y + dy + height) % height;
        int row_offset = ny * width;

        #pragma unroll
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = (x + dx + width) % width;
            alive_neighbors += current_state[row_offset + nx];
        }
    }

    unsigned char cell = current_state[idx];
    next_state[idx] = (alive_neighbors == 3 || (cell == 1 && alive_neighbors == 2)) ? 1 : 0;
}

__global__ void cuda_generate_pixels_kernel(const unsigned char* __restrict__ state,
                                            unsigned int* __restrict__ pixels,
                                            int width, int height,
                                            unsigned int alive_color,
                                            unsigned int dead_color) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int idx = y * width + x;
    pixels[idx] = (state[idx] != 0) ? alive_color : dead_color;
}

__device__ inline unsigned int gpu_hash(unsigned int a) {
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
}

__global__ void cuda_randomize_grid_kernel(unsigned char* state, int width, int height, int alive_prob, unsigned int seed) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int idx = y * width + x;
    unsigned int h = gpu_hash((unsigned int)idx ^ seed);
    state[idx] = ((h % 100) < (unsigned int)alive_prob) ? 1 : 0;
}

__global__ void cuda_draw_brush_kernel(unsigned char* state, int width, int height,
                                       int center_x, int center_y, int radius, unsigned char value) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int dx = x - center_x;
    int dy = y - center_y;
    if (dx * dx + dy * dy <= radius * radius) {
        state[y * width + x] = value;
    }
}

// Parallel block reduction kernel to count population
__global__ void cuda_count_alive_kernel(const unsigned char* state, int total_cells, int* block_sums) {
    __shared__ int s_data[256];
    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    s_data[tid] = (idx < total_cells) ? state[idx] : 0;
    __syncthreads();

    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_data[tid] += s_data[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        block_sums[blockIdx.x] = s_data[0];
    }
}

// ============================================================================
// CUDAEngine Implementation
// ============================================================================

CUDAEngine::CUDAEngine() {
    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
        device_name = prop.name;
    } else {
        device_name = "NVIDIA CUDA GPU";
    }
}

CUDAEngine::~CUDAEngine() {
    if (d_current) cudaFree(d_current);
    if (d_next)    cudaFree(d_next);
    if (d_pixels)  cudaFree(d_pixels);
    if (h_pixels)  cudaFreeHost(h_pixels);
}

bool CUDAEngine::init(int w, int h) {
    width = w;
    height = h;
    size_t grid_bytes = (size_t)width * height * sizeof(unsigned char);
    size_t pixel_bytes = (size_t)width * height * sizeof(unsigned int);

    if (d_current) cudaFree(d_current);
    if (d_next)    cudaFree(d_next);
    if (d_pixels)  cudaFree(d_pixels);
    if (h_pixels)  cudaFreeHost(h_pixels);

    CUDA_CHECK(cudaMalloc(&d_current, grid_bytes));
    CUDA_CHECK(cudaMalloc(&d_next, grid_bytes));
    CUDA_CHECK(cudaMalloc(&d_pixels, pixel_bytes));
    CUDA_CHECK(cudaMallocHost(&h_pixels, pixel_bytes));

    CUDA_CHECK(cudaMemset(d_current, 0, grid_bytes));
    CUDA_CHECK(cudaMemset(d_next, 0, grid_bytes));

    host_grid_buffer.resize(width * height, 0);
    population = 0;
    pop_dirty = true;
    return true;
}

void CUDAEngine::step() {
    dim3 block_dim(16, 16);
    dim3 grid_dim((width + block_dim.x - 1) / block_dim.x,
                  (height + block_dim.y - 1) / block_dim.y);

    cuda_next_state_kernel<<<grid_dim, block_dim>>>(d_current, d_next, width, height);
    std::swap(d_current, d_next);
    pop_dirty = true;
}

void CUDAEngine::clear() {
    size_t grid_bytes = (size_t)width * height * sizeof(unsigned char);
    CUDA_CHECK(cudaMemset(d_current, 0, grid_bytes));
    population = 0;
    pop_dirty = false;
}

void CUDAEngine::randomize(int alive_prob, unsigned int seed) {
    dim3 block_dim(16, 16);
    dim3 grid_dim((width + block_dim.x - 1) / block_dim.x,
                  (height + block_dim.y - 1) / block_dim.y);

    cuda_randomize_grid_kernel<<<grid_dim, block_dim>>>(d_current, width, height, alive_prob, seed);
    pop_dirty = true;
}

void CUDAEngine::set_cell(int x, int y, unsigned char value) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        size_t offset = (size_t)y * width + x;
        CUDA_CHECK(cudaMemcpy(d_current + offset, &value, sizeof(unsigned char), cudaMemcpyHostToDevice));
        pop_dirty = true;
    }
}

void CUDAEngine::set_brush(int cx, int cy, int radius, unsigned char value) {
    dim3 block_dim(16, 16);
    dim3 grid_dim((width + block_dim.x - 1) / block_dim.x,
                  (height + block_dim.y - 1) / block_dim.y);

    cuda_draw_brush_kernel<<<grid_dim, block_dim>>>(d_current, width, height, cx, cy, radius, value);
    pop_dirty = true;
}

void CUDAEngine::spawn_pattern(int cx, int cy, const Pattern& pattern) {
    // Read current host buffer, modify it, upload
    get_grid_state();
    for (const auto& pt : pattern.points) {
        int gx = (cx + pt.dx + width) % width;
        int gy = (cy + pt.dy + height) % height;
        host_grid_buffer[gy * width + gx] = 1;
    }
    set_grid_state(host_grid_buffer.data());
}

const unsigned int* CUDAEngine::get_pixels() {
    dim3 block_dim(16, 16);
    dim3 grid_dim((width + block_dim.x - 1) / block_dim.x,
                  (height + block_dim.y - 1) / block_dim.y);

    cuda_generate_pixels_kernel<<<grid_dim, block_dim>>>(d_current, d_pixels, width, height, COLOR_ALIVE, COLOR_DEAD);
    CUDA_CHECK(cudaDeviceSynchronize());

    size_t pixel_bytes = (size_t)width * height * sizeof(unsigned int);
    CUDA_CHECK(cudaMemcpy(h_pixels, d_pixels, pixel_bytes, cudaMemcpyDeviceToHost));
    return h_pixels;
}

int CUDAEngine::get_population() {
    if (pop_dirty) {
        int total = width * height;
        int threads_per_block = 256;
        int blocks = (total + threads_per_block - 1) / threads_per_block;

        int* d_block_sums = nullptr;
        CUDA_CHECK(cudaMalloc(&d_block_sums, blocks * sizeof(int)));
        cuda_count_alive_kernel<<<blocks, threads_per_block>>>(d_current, total, d_block_sums);

        std::vector<int> h_block_sums(blocks, 0);
        CUDA_CHECK(cudaMemcpy(h_block_sums.data(), d_block_sums, blocks * sizeof(int), cudaMemcpyDeviceToHost));
        cudaFree(d_block_sums);

        int total_pop = 0;
        for (int val : h_block_sums) total_pop += val;
        population = total_pop;
        pop_dirty = false;
    }
    return population;
}

std::string CUDAEngine::get_name() const {
    return "CUDA GPU (" + device_name + ")";
}

const unsigned char* CUDAEngine::get_grid_state() {
    size_t grid_bytes = (size_t)width * height * sizeof(unsigned char);
    CUDA_CHECK(cudaMemcpy(host_grid_buffer.data(), d_current, grid_bytes, cudaMemcpyDeviceToHost));
    return host_grid_buffer.data();
}

void CUDAEngine::set_grid_state(const unsigned char* host_state) {
    if (!host_state) return;
    size_t grid_bytes = (size_t)width * height * sizeof(unsigned char);
    CUDA_CHECK(cudaMemcpy(d_current, host_state, grid_bytes, cudaMemcpyHostToDevice));
    pop_dirty = true;
}

#endif // USE_CUDA
