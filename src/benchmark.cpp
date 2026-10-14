#include "benchmark.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <cmath>
#include <algorithm>

static const int WARMUP_STEPS  = 10;
static const int MEASURE_STEPS = 100;

double BenchmarkRunner::calc_memory_mb(int w, int h) {
    // Current state, next state, and pixel buffer (ARGB = 4 bytes)
    size_t bytes = (size_t)w * h * (1 + 1 + 4);
    return bytes / (1024.0 * 1024.0);
}

BenchmarkResult BenchmarkRunner::time_engine(
    SimulationEngine* engine,
    int grid_w, int grid_h,
    int warmup_steps, int measure_steps)
{
    engine->init(grid_w, grid_h);
    engine->randomize(20, 12345); // deterministic seed for fairness

    // Warm-up
    for (int i = 0; i < warmup_steps; i++) {
        engine->step();
    }

    // Measure
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < measure_steps; i++) {
        engine->step();
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double step_ms  = total_ms / measure_steps;
    double gen_per_sec = 1000.0 / step_ms;

    BenchmarkResult res;
    res.engine_name = engine->get_name();
    res.grid_w = grid_w;
    res.grid_h = grid_h;
    res.step_ms = step_ms;
    res.gen_per_sec = gen_per_sec;
    res.speedup = 0.0;
    res.memory_mb = calc_memory_mb(grid_w, grid_h);

    return res;
}

void BenchmarkRunner::run(const std::vector<std::pair<int,int>>& grid_sizes, bool has_cuda, BenchmarkSuite* suite) {
    if (!suite) return;

    {
        std::lock_guard<std::mutex> lock(suite->mtx);
        suite->results.clear();
        suite->total_steps = grid_sizes.size() * (has_cuda ? 2 : 1);
        suite->current_step = 0;
        suite->is_running = true;
        suite->is_done = false;
    }

    for (const auto& size : grid_sizes) {
        int w = size.first;
        int h = size.second;

        // CPU Benchmark
        auto cpu_engine = create_simulation_engine(false, w, h, 0, false);
        BenchmarkResult cpu_res = time_engine(cpu_engine.get(), w, h, WARMUP_STEPS, MEASURE_STEPS);
        cpu_res.engine_name = "CPU"; // Standardize name for display

        {
            std::lock_guard<std::mutex> lock(suite->mtx);
            suite->results.push_back(cpu_res);
            suite->current_step++;
        }

        // GPU Benchmark
        if (has_cuda) {
            auto gpu_engine = create_simulation_engine(true, w, h, 0, false);
            BenchmarkResult gpu_res = time_engine(gpu_engine.get(), w, h, WARMUP_STEPS, MEASURE_STEPS);
            gpu_res.engine_name = "CUDA GPU";

            // Calculate speedup
            if (cpu_res.gen_per_sec > 0) {
                gpu_res.speedup = gpu_res.gen_per_sec / cpu_res.gen_per_sec;
            }

            {
                std::lock_guard<std::mutex> lock(suite->mtx);
                suite->results.push_back(gpu_res);
                suite->current_step++;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(suite->mtx);
        suite->is_running = false;
        suite->is_done = true;
    }
}

void BenchmarkRunner::run_strong_scaling(
    int grid_w, int grid_h,
    const std::vector<int>& thread_counts,
    ThreadBenchmarkSuite* suite)
{
    if (!suite) return;

    {
        std::lock_guard<std::mutex> lock(suite->mtx);
        suite->results.clear();
        suite->total_steps = (int)thread_counts.size();
        suite->current_step = 0;
        suite->is_running = true;
        suite->is_done = false;
        suite->title = "Strong Scaling (" + std::to_string(grid_w) + "x" + std::to_string(grid_h) + ")";
    }

    double t1_baseline_ms = 0.0;
    int measure_steps = (grid_w >= 2048) ? 40 : 100;

    for (int threads : thread_counts) {
        auto engine = create_simulation_engine(false, grid_w, grid_h, threads, false);

        BenchmarkResult b_res = time_engine(engine.get(), grid_w, grid_h, WARMUP_STEPS, measure_steps);

        if (threads == 1 || t1_baseline_ms <= 0.0) {
            t1_baseline_ms = b_res.step_ms;
        }

        double speedup = (b_res.step_ms > 0.0) ? (t1_baseline_ms / b_res.step_ms) : 1.0;
        double efficiency = (threads > 0) ? (speedup / (double)threads) * 100.0 : 100.0;
        double throughput = ((double)grid_w * grid_h) / (b_res.step_ms * 1000.0); // MCells/sec

        // Estimate Amdahl's serial fraction s = (1/S - 1/p) / (1 - 1/p)
        double amdahl_s = 0.0;
        if (threads > 1 && speedup > 0.0) {
            amdahl_s = ( (1.0 / speedup) - (1.0 / (double)threads) ) / (1.0 - (1.0 / (double)threads));
            if (amdahl_s < 0.0) amdahl_s = 0.0;
            if (amdahl_s > 1.0) amdahl_s = 1.0;
        }

        ThreadScalingResult ts;
        ts.threads = threads;
        ts.grid_w = grid_w;
        ts.grid_h = grid_h;
        ts.step_ms = b_res.step_ms;
        ts.gen_per_sec = b_res.gen_per_sec;
        ts.speedup = speedup;
        ts.efficiency = efficiency;
        ts.throughput_mcells = throughput;
        ts.amdahl_serial_frac = amdahl_s;

        {
            std::lock_guard<std::mutex> lock(suite->mtx);
            suite->results.push_back(ts);
            suite->current_step++;
        }
    }

    {
        std::lock_guard<std::mutex> lock(suite->mtx);
        suite->is_running = false;
        suite->is_done = true;
    }
}

void BenchmarkRunner::run_weak_scaling(
    int base_grid_dim,
    const std::vector<int>& thread_counts,
    ThreadBenchmarkSuite* suite)
{
    if (!suite) return;

    {
        std::lock_guard<std::mutex> lock(suite->mtx);
        suite->results.clear();
        suite->total_steps = (int)thread_counts.size();
        suite->current_step = 0;
        suite->is_running = true;
        suite->is_done = false;
        suite->title = "Weak Scaling (Base " + std::to_string(base_grid_dim) + "x" + std::to_string(base_grid_dim) + "/thread)";
    }

    double base_cells = (double)base_grid_dim * base_grid_dim;
    double t1_baseline_ms = 0.0;

    for (int threads : thread_counts) {
        // Proportionally scale problem size with thread count
        double target_cells = base_cells * (double)threads;
        int dim = (int)std::round(std::sqrt(target_cells));
        if (dim % 2 != 0) dim++; // ensure even dimensions

        auto engine = create_simulation_engine(false, dim, dim, threads, false);

        int measure_steps = (dim >= 2048) ? 40 : 80;
        BenchmarkResult b_res = time_engine(engine.get(), dim, dim, WARMUP_STEPS, measure_steps);

        if (threads == 1 || t1_baseline_ms <= 0.0) {
            t1_baseline_ms = b_res.step_ms;
        }

        // Weak scaling efficiency: E = T(1) / T(p) * 100%
        double efficiency = (b_res.step_ms > 0.0) ? (t1_baseline_ms / b_res.step_ms) * 100.0 : 100.0;
        // Weak scaling speedup: S = p * (T(1) / T(p))
        double speedup = ((double)threads * efficiency) / 100.0;
        double throughput = ((double)dim * dim) / (b_res.step_ms * 1000.0); // MCells/sec

        ThreadScalingResult ts;
        ts.threads = threads;
        ts.grid_w = dim;
        ts.grid_h = dim;
        ts.step_ms = b_res.step_ms;
        ts.gen_per_sec = b_res.gen_per_sec;
        ts.speedup = speedup;
        ts.efficiency = efficiency;
        ts.throughput_mcells = throughput;
        ts.amdahl_serial_frac = 0.0;

        {
            std::lock_guard<std::mutex> lock(suite->mtx);
            suite->results.push_back(ts);
            suite->current_step++;
        }
    }

    {
        std::lock_guard<std::mutex> lock(suite->mtx);
        suite->is_running = false;
        suite->is_done = true;
    }
}

bool BenchmarkRunner::export_thread_scaling_csv(
    const std::string& filepath,
    const std::vector<ThreadScalingResult>& results,
    const std::string& mode_name)
{
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open file for CSV export: " << filepath << "\n";
        return false;
    }

    file << "# OpenMP Thread Scaling Benchmark Results\n";
    file << "# Mode: " << mode_name << "\n";
    file << "Threads,GridWidth,GridHeight,TotalCells,StepTime_ms,GenPerSec,Speedup,Efficiency_pct,Throughput_MCellsSec,Amdahl_SerialFrac\n";

    for (const auto& r : results) {
        long long total_cells = (long long)r.grid_w * r.grid_h;
        file << r.threads << ","
             << r.grid_w << ","
             << r.grid_h << ","
             << total_cells << ","
             << std::fixed << std::setprecision(4) << r.step_ms << ","
             << std::setprecision(2) << r.gen_per_sec << ","
             << std::setprecision(3) << r.speedup << ","
             << std::setprecision(1) << r.efficiency << ","
             << std::setprecision(2) << r.throughput_mcells << ","
             << std::setprecision(4) << r.amdahl_serial_frac << "\n";
    }

    file.close();
    std::cout << "[INFO] Successfully exported thread scaling benchmark to: " << filepath << "\n";
    return true;
}

bool BenchmarkRunner::export_cpu_gpu_csv(
    const std::string& filepath,
    const std::vector<BenchmarkResult>& results)
{
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open file for CSV export: " << filepath << "\n";
        return false;
    }

    file << "Engine,GridWidth,GridHeight,TotalCells,StepTime_ms,GenPerSec,Speedup,Memory_MB\n";
    for (const auto& r : results) {
        long long total_cells = (long long)r.grid_w * r.grid_h;
        file << "\"" << r.engine_name << "\","
             << r.grid_w << ","
             << r.grid_h << ","
             << total_cells << ","
             << std::fixed << std::setprecision(4) << r.step_ms << ","
             << std::setprecision(2) << r.gen_per_sec << ","
             << std::setprecision(2) << r.speedup << ","
             << std::setprecision(2) << r.memory_mb << "\n";
    }

    file.close();
    std::cout << "[INFO] Successfully exported CPU vs GPU benchmark to: " << filepath << "\n";
    return true;
}

void BenchmarkRunner::print_thread_scaling_console(
    const std::vector<ThreadScalingResult>& results,
    const std::string& title)
{
    std::cout << "\n"
              << "===================================================================================================\n"
              << " " << title << "\n"
              << "===================================================================================================\n"
              << " THREADS | GRID RESOLUTION | STEP TIME  |  GEN/SEC   | SPEEDUP | EFFICIENCY | THROUGHPUT | AMDAHL 's'\n"
              << "---------+-----------------+------------+------------+---------+------------+------------+-----------\n";

    for (const auto& r : results) {
        std::string grid_str = std::to_string(r.grid_w) + "x" + std::to_string(r.grid_h);
        char buf[160];
        std::string amdahl_str = (r.threads == 1) ? "    -    " : "";
        if (r.threads > 1) {
            char a_buf[32];
            snprintf(a_buf, sizeof(a_buf), "%7.2f%%", r.amdahl_serial_frac * 100.0);
            amdahl_str = a_buf;
        }

        snprintf(buf, sizeof(buf),
            "   %3d   |   %11s   | %7.2f ms | %8.1f/s | %6.2fx  |   %6.1f%%   | %7.1f MC/s | %9s",
            r.threads, grid_str.c_str(), r.step_ms, r.gen_per_sec, r.speedup, r.efficiency, r.throughput_mcells, amdahl_str.c_str());

        std::cout << buf << "\n";
    }

    std::cout << "===================================================================================================\n\n";
}

void BenchmarkRunner::print_cpu_gpu_console(
    const std::vector<BenchmarkResult>& results)
{
    std::cout << "\n"
              << "======================================================================================\n"
              << "                           CPU vs CUDA GPU BENCHMARK\n"
              << "======================================================================================\n"
              << " ENGINE         | GRID RESOLUTION | STEP TIME  |  GEN/SEC   | SPEEDUP | MEMORY USAGE\n"
              << "----------------+-----------------+------------+------------+---------+---------------\n";

    for (const auto& r : results) {
        std::string grid_str = std::to_string(r.grid_w) + "x" + std::to_string(r.grid_h);
        char buf[140];
        std::string speedup_str = (r.speedup > 0) ? "" : "   -   ";
        if (r.speedup > 0) {
            char s_buf[32];
            snprintf(s_buf, sizeof(s_buf), "%5.1fx ", r.speedup);
            speedup_str = s_buf;
        }

        snprintf(buf, sizeof(buf),
            " %-14s |   %11s   | %7.2f ms | %8.1f/s | %7s | %7.1f MB",
            r.engine_name.c_str(), grid_str.c_str(), r.step_ms, r.gen_per_sec, speedup_str.c_str(), r.memory_mb);

        std::cout << buf << "\n";
    }

    std::cout << "======================================================================================\n\n";
}

