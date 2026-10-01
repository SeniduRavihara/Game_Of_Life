#ifndef ENGINE_H
#define ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "patterns.h"

// Color scheme for ARGB 32-bit pixel buffers
static const unsigned int COLOR_ALIVE = 0xFF00FF7F; // Vibrant Spring Green
static const unsigned int COLOR_DEAD  = 0xFF141418; // Sleek Dark Slate

class SimulationEngine {
public:
    virtual ~SimulationEngine() {}

    virtual bool init(int w, int h) = 0;
    virtual void step() = 0;
    virtual void clear() = 0;
    virtual void randomize(int alive_prob, unsigned int seed) = 0;
    virtual void set_cell(int x, int y, unsigned char value) = 0;
    virtual void set_brush(int cx, int cy, int radius, unsigned char value) = 0;
    virtual void spawn_pattern(int cx, int cy, const Pattern& pattern) = 0;

    virtual const unsigned int* get_pixels() = 0;
    virtual int get_population() = 0;
    virtual std::string get_name() const = 0;
    virtual bool is_gpu() const = 0;

    // Grid state export / import for seamless engine switching
    virtual const unsigned char* get_grid_state() = 0;
    virtual void set_grid_state(const unsigned char* host_state) = 0;

    int get_width() const { return width; }
    int get_height() const { return height; }

protected:
    int width = 0;
    int height = 0;
};

// ============================================================================
// Multi-Threaded CPU Engine (OpenMP)
// ============================================================================
class CPUEngine : public SimulationEngine {
private:
    std::vector<unsigned char> current_state;
    std::vector<unsigned char> next_state;
    std::vector<unsigned int> pixels;
    int population = 0;
    bool pop_dirty = true;

public:
    CPUEngine() {}
    ~CPUEngine() override {}

    bool init(int w, int h) override;
    void step() override;
    void clear() override;
    void randomize(int alive_prob, unsigned int seed) override;
    void set_cell(int x, int y, unsigned char value) override;
    void set_brush(int cx, int cy, int radius, unsigned char value) override;
    void spawn_pattern(int cx, int cy, const Pattern& pattern) override;

    const unsigned int* get_pixels() override;
    int get_population() override;
    std::string get_name() const override { return "CPU (OpenMP Multi-Threaded)"; }
    bool is_gpu() const override { return false; }

    const unsigned char* get_grid_state() override { return current_state.data(); }
    void set_grid_state(const unsigned char* host_state) override;
};

// Check if CUDA support is compiled in and available on the machine
bool is_cuda_available();
std::string get_cuda_device_name();

// Factory to create simulation engine
std::unique_ptr<SimulationEngine> create_simulation_engine(bool prefer_cuda, int width, int height);

#endif // ENGINE_H
