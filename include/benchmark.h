#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <string>
#include <vector>
#include <mutex>
#include "engine.h"

// Standard CPU vs GPU benchmark result
struct BenchmarkResult {
    std::string engine_name;
    int grid_w, grid_h;
    double gen_per_sec;
    double step_ms;
    double speedup; // 0.0 for CPU, > 1.0 for GPU if faster
    double memory_mb;
};

// OpenMP thread scaling benchmark result (Strong and Weak scaling)
struct ThreadScalingResult {
    int threads;
    int grid_w, grid_h;
    double step_ms;
    double gen_per_sec;
    double speedup;            // T(1) / T(p)
    double efficiency;         // (Speedup / p) * 100.0%
    double throughput_mcells;  // (w * h) / (step_ms * 1000.0) -> Megacells/sec
    double amdahl_serial_frac; // Estimated empirical serial fraction s
};

struct BenchmarkSuite {
    std::vector<BenchmarkResult> results;
    bool is_running = false;
    bool is_done    = false;
    int  current_step = 0;
    int  total_steps  = 0;
    std::mutex mtx; // Protects access to results across threads
};

struct ThreadBenchmarkSuite {
    std::vector<ThreadScalingResult> results;
    bool is_running = false;
    bool is_done    = false;
    int  current_step = 0;
    int  total_steps  = 0;
    std::string title; // e.g. "Strong Scaling (1024x1024)"
    std::mutex mtx;
};

class BenchmarkRunner {
public:
    // CPU vs GPU Benchmark across grid sizes
    static void run(
        const std::vector<std::pair<int,int>>& grid_sizes,
        bool has_cuda,
        BenchmarkSuite* suite
    );

    // Strong Thread Scaling: sweep thread counts for a fixed grid size
    static void run_strong_scaling(
        int grid_w, int grid_h,
        const std::vector<int>& thread_counts,
        ThreadBenchmarkSuite* suite
    );

    // Weak Thread Scaling: scale grid resolution proportionally with thread count
    static void run_weak_scaling(
        int base_grid_dim,
        const std::vector<int>& thread_counts,
        ThreadBenchmarkSuite* suite
    );

    // CSV Export utilities
    static bool export_thread_scaling_csv(
        const std::string& filepath,
        const std::vector<ThreadScalingResult>& results,
        const std::string& mode_name = "Strong Scaling"
    );

    static bool export_cpu_gpu_csv(
        const std::string& filepath,
        const std::vector<BenchmarkResult>& results
    );

    // Formatted Terminal Console Printers (for Headless HPC Execution)
    static void print_thread_scaling_console(
        const std::vector<ThreadScalingResult>& results,
        const std::string& title = "OPENMP THREAD SCALING BENCHMARK"
    );

    static void print_cpu_gpu_console(
        const std::vector<BenchmarkResult>& results
    );

private:
    static BenchmarkResult time_engine(
        SimulationEngine* engine,
        int grid_w, int grid_h,
        int warmup_steps, int measure_steps
    );
    static double calc_memory_mb(int grid_w, int grid_h);
};

#endif // BENCHMARK_H
