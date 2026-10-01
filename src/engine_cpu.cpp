#include "engine.h"
#include <cstring>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

bool CPUEngine::init(int w, int h) {
    width = w;
    height = h;
    int total = width * height;
    current_state.assign(total, 0);
    next_state.assign(total, 0);
    pixels.assign(total, COLOR_DEAD);
    population = 0;
    pop_dirty = true;
    return true;
}

#if defined(__clang__)
#define UNROLL_3 _Pragma("unroll 3")
#elif defined(__GNUC__)
#define UNROLL_3 _Pragma("GCC unroll 3")
#else
#define UNROLL_3
#endif

void CPUEngine::step() {
    #ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(static)
    #endif
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int alive_neighbors = 0;

            UNROLL_3
            for (int dy = -1; dy <= 1; ++dy) {
                int ny = (y + dy + height) % height;
                int row_offset = ny * width;

                UNROLL_3
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = (x + dx + width) % width;
                    alive_neighbors += current_state[row_offset + nx];
                }
            }

            int idx = y * width + x;
            unsigned char cell = current_state[idx];
            next_state[idx] = (alive_neighbors == 3 || (cell == 1 && alive_neighbors == 2)) ? 1 : 0;
        }
    }

    std::swap(current_state, next_state);
    pop_dirty = true;
}

void CPUEngine::clear() {
    std::fill(current_state.begin(), current_state.end(), 0);
    population = 0;
    pop_dirty = false;
}

void CPUEngine::randomize(int alive_prob, unsigned int seed) {
    #ifdef _OPENMP
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        unsigned int local_seed = seed + tid * 1013904223u;
        #pragma omp for schedule(static)
        for (int i = 0; i < width * height; ++i) {
            local_seed = local_seed * 1664525u + 1013904223u;
            current_state[i] = ((local_seed % 100) < (unsigned int)alive_prob) ? 1 : 0;
        }
    }
    #else
    srand(seed);
    for (int i = 0; i < width * height; ++i) {
        current_state[i] = ((rand() % 100) < alive_prob) ? 1 : 0;
    }
    #endif
    pop_dirty = true;
}

void CPUEngine::set_cell(int x, int y, unsigned char value) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        current_state[y * width + x] = value;
        pop_dirty = true;
    }
}

void CPUEngine::set_brush(int cx, int cy, int radius, unsigned char value) {
    int min_y = std::max(0, cy - radius);
    int max_y = std::min(height - 1, cy + radius);
    int min_x = std::max(0, cx - radius);
    int max_x = std::min(width - 1, cx + radius);

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius) {
                current_state[y * width + x] = value;
            }
        }
    }
    pop_dirty = true;
}

void CPUEngine::spawn_pattern(int cx, int cy, const Pattern& pattern) {
    for (const auto& pt : pattern.points) {
        int gx = (cx + pt.dx + width) % width;
        int gy = (cy + pt.dy + height) % height;
        current_state[gy * width + gx] = 1;
    }
    pop_dirty = true;
}

const unsigned int* CPUEngine::get_pixels() {
    #ifdef _OPENMP
    #pragma omp parallel for schedule(static)
    #endif
    for (int i = 0; i < width * height; ++i) {
        pixels[i] = (current_state[i] != 0) ? COLOR_ALIVE : COLOR_DEAD;
    }
    return pixels.data();
}

int CPUEngine::get_population() {
    if (pop_dirty) {
        int pop = 0;
        #ifdef _OPENMP
        #pragma omp parallel for reduction(+:pop) schedule(static)
        #endif
        for (int i = 0; i < width * height; ++i) {
            pop += current_state[i];
        }
        population = pop;
        pop_dirty = false;
    }
    return population;
}

void CPUEngine::set_grid_state(const unsigned char* host_state) {
    if (!host_state) return;
    std::memcpy(current_state.data(), host_state, (size_t)width * height * sizeof(unsigned char));
    pop_dirty = true;
}
