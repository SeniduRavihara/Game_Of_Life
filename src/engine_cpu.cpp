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

void CPUEngine::step() {
    int alive_count = 0;

    #ifdef _OPENMP
    #pragma omp parallel for schedule(static) reduction(+:alive_count)
    #endif
    for (int y = 0; y < height; ++y) {
        int row_above = (y == 0 ? height - 1 : y - 1) * width;
        int row_curr  = y * width;
        int row_below = (y == height - 1 ? 0 : y + 1) * width;

        const unsigned char* p_above = &current_state[row_above];
        const unsigned char* p_curr  = &current_state[row_curr];
        const unsigned char* p_below = &current_state[row_below];
        unsigned char* p_next        = &next_state[row_curr];

        // Left border (x = 0)
        {
            int neighbors = p_above[width - 1] + p_above[0] + p_above[1]
                          + p_curr[width - 1]               + p_curr[1]
                          + p_below[width - 1] + p_below[0] + p_below[1];
            unsigned char c = p_curr[0];
            unsigned char next_c = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
            p_next[0] = next_c;
            alive_count += next_c;
        }

        // Fast inner loop: 0 modulos, sequential cache access, auto-vectorizable
        #if defined(__GNUC__) || defined(__clang__)
        #pragma GCC ivdep
        #endif
        for (int x = 1; x < width - 1; ++x) {
            int neighbors = p_above[x - 1] + p_above[x] + p_above[x + 1]
                          + p_curr[x - 1]               + p_curr[x + 1]
                          + p_below[x - 1] + p_below[x] + p_below[x + 1];
            unsigned char c = p_curr[x];
            unsigned char next_c = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
            p_next[x] = next_c;
            alive_count += next_c;
        }

        // Right border (x = width - 1)
        {
            int x = width - 1;
            int neighbors = p_above[x - 1] + p_above[x] + p_above[0]
                          + p_curr[x - 1]               + p_curr[0]
                          + p_below[x - 1] + p_below[x] + p_below[0];
            unsigned char c = p_curr[x];
            unsigned char next_c = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
            p_next[x] = next_c;
            alive_count += next_c;
        }
    }

    std::swap(current_state, next_state);
    population = alive_count;
    pop_dirty = false;
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
