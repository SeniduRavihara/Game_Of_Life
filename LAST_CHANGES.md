# Comprehensive Changelog: Changes Since Last Commit (`8275f0b`)

> **Repository**: `Game_Of_Life`  
> **Base Commit**: `8275f0b` (*"fix(ci): include benchmark.cpp in Windows and macOS build commands"*)  
> **Target Branch**: `main`  
> **Scope**: HPC Assignment Upgrade — OpenMP Thread Scaling, Headless CLI Benchmarking, 2D Cartesian Distributed MPI Engine, Scientific Visualization Suite, and Technical Assignment Deliverables.

---

## 1. Executive Summary

Since commit `8275f0b`, the codebase has evolved from a dual-engine (CPU/CUDA) desktop simulation into a dual-paradigm High Performance Computing (HPC) research platform. It supports both **Shared-Memory Parallelism (OpenMP)** with dynamic thread-level scaling and **Distributed-Memory Parallelism (MPI)** with 2D Cartesian block decomposition, asynchronous communication-computation overlap, and zero-copy derived vector datatypes.

### Key Metrics of this Update
- **Tracked Files Modified**: 11 files (`+784` lines, `-78` lines)
- **New Source & Script Files**: `src/main_mpi.cpp`, `scripts/plot_scaling.py`, `plan.md`
- **Output Artifacts**: 4 benchmark datasets (`.csv`), 6 high-resolution publication charts (`.png`)
- **Compilation Status**: Zero warnings across GCC/G++ (`-O3 -Wall -fopenmp`) and MPICXX (`-O3 -Wall -fopenmp`).

---

## 2. File Modification & Creation Inventory

| File Path | Type | Status | Description |
| :--- | :---: | :---: | :--- |
| `include/engine.h` | Header | Modified | Added virtual thread control methods and updated engine factory declaration |
| `src/engine_cpu.cpp` | Source | Modified | Added dynamic thread setter/getter, injected `num_threads` clauses into OpenMP loops |
| `src/engine_factory.cpp` | Source | Modified | Added thread and verbosity parameters to `create_simulation_engine()` |
| `include/benchmark.h` | Header | Modified | Added `ThreadScalingResult`, `ThreadBenchmarkSuite`, and HPC benchmark signatures |
| `src/benchmark.cpp` | Source | Modified | Implemented Strong/Weak scaling, CSV exporters, and formatted ASCII console printers |
| `src/main.cpp` | Source | Modified | Added headless CLI argument parser (`--bench-*`), decoupled from X11/GUI for clusters |
| `include/gui.h` | Header | Modified | Added `BenchmarkViewMode` enum, thread suite reference, and callback hook |
| `src/gui.cpp` | Source | Modified | Added `[THREAD SCALE]` and `[VIEW MODE]` buttons, rendered color-coded efficiency table |
| `src/main_mpi.cpp` | Source | **New** | Complete distributed-memory 2D Cartesian MPI engine with non-blocking halo overlap |
| `Makefile` | Build | Modified | Added `make mpi` target, auto-detection for `mpicxx`, and installation guide fallbacks |
| `.github/workflows/build-artifacts.yml` | CI/CD | Modified | Added `-fopenmp` compiler flag to MinGW Windows release build |
| `scripts/plot_scaling.py` | Script | **New** | Automated Python/Matplotlib tool generating 300 DPI Speedup, Efficiency, and Throughput plots |
| `README.md` | Docs | Modified | Added headless CLI benchmarks, Python visualization steps, and MPI usage instructions |
| `plan.md` | Docs | **New** | Full HPC engineering plan, theoretical formulas, comparison matrix, and 3-min video script |
| `benchmark_thread_scaling.csv` | Data | **New** | Measured OpenMP strong scaling results ($1, 2, 4, 6, 8, 12$ threads on $1024\times 1024$) |
| `benchmark_mpi.csv` | Data | **New** | Measured 2D Cartesian MPI scaling results ($1, 2, 4, 6$ ranks on $1024\times 1024$) |
| `benchmark_cpu_gpu.csv` | Data | **New** | Measured CPU vs CUDA GPU performance across 5 grid resolutions ($256^2 \to 4096^2$) |
| `benchmark_weak_scaling.csv` | Data | **New** | Weak scaling dataset tracking proportional workload expansion per thread |
| `scripts/*.png` | Image | **New** | 6 publication-ready charts (Speedup, Efficiency %, Throughput for both OpenMP and MPI) |

---

## 3. In-Depth Technical Changes by Component

### 3.1. Dynamic OpenMP Thread-Level Control
* **File**: `include/engine.h`
  - Added `virtual void set_num_threads(int threads)` (default no-op).
  - Added `virtual int get_num_threads() const` (default returning 1).
  - Added private member `int num_threads = 0;` to `CPUEngine`.
  - Updated `create_simulation_engine(bool prefer_cuda, int width, int height, int threads = 0, bool verbose = true)`.
* **File**: `src/engine_cpu.cpp`
  - Implemented `CPUEngine::set_num_threads(int threads)` to store thread limit (0 = max system threads).
  - Implemented `CPUEngine::get_num_threads()` querying `omp_get_max_threads()` dynamically when `num_threads == 0`.
  - Updated `CPUEngine::get_name()` to return dynamic string: `"CPU (OpenMP, N threads)"`.
  - Injected `num_threads(nthreads)` into:
    - `#pragma omp parallel for schedule(static) reduction(+:alive_count) num_threads(nthreads)` (in `step()`)
    - `#pragma omp parallel num_threads(nthreads)` (in `randomize()`)
    - `#pragma omp parallel for schedule(static) num_threads(nthreads)` (in `get_pixels()`)
    - `#pragma omp parallel for reduction(+:pop) schedule(static) num_threads(nthreads)` (in `get_population()`)
* **File**: `src/engine_factory.cpp`
  - Updated factory logic to configure `threads` into the newly created `CPUEngine`.
  - Silenced repeated `[INFO] Using Simulation Engine` logs during benchmark sweeps via `verbose = false`.

---

### 3.2. Precision HPC Benchmark Suite & Theoretical Metrics
* **File**: `include/benchmark.h`
  - Defined struct `ThreadScalingResult`:
    - `int threads`, `grid_w`, `grid_h`
    - `double step_ms`, `gen_per_sec`
    - `double speedup` ($S_p = T_1 / T_p$)
    - `double efficiency` ($E_p = \frac{S_p}{p} \times 100\%$)
    - `double throughput_mcells` ($\frac{W \times H}{T \times 1000}$)
    - `double amdahl_serial_frac` ($s = \frac{1/S_p - 1/p}{1 - 1/p}$)
  - Defined struct `ThreadBenchmarkSuite` protected by `std::mutex` for asynchronous worker threads.
  - Declared `run_strong_scaling()`, `run_weak_scaling()`, `export_thread_scaling_csv()`, `print_thread_scaling_console()`, and `print_cpu_gpu_console()`.
* **File**: `src/benchmark.cpp`
  - Implemented `BenchmarkRunner::run_strong_scaling()`: Sweeps thread counts with warm-up steps and microsecond timing.
  - Implemented `BenchmarkRunner::run_weak_scaling()`: Scales problem area ($W \times H$) proportionally with thread count.
  - Implemented `BenchmarkRunner::export_thread_scaling_csv()`: Exports CSV records.
  - Implemented `BenchmarkRunner::print_thread_scaling_console()`: Formats clean ASCII terminal tables with thread counts, step times, speedup, efficiency, throughput, and Amdahl's $s$.

---

### 3.3. Headless CLI Engine for HPC Clusters
* **File**: `src/main.cpp`
  - Added helper `get_scaling_thread_counts()`: Automatically detects physical and logical thread topologies (e.g. $[1, 2, 4, 6, 8, 12]$ on 6-core/12-thread architectures).
  - Added headless CLI argument flags allowing benchmark execution on cluster nodes without X11/SDL displays:
    - `--bench-threads [w] [h]`: Runs strong thread scaling on given grid size (default: $1024\times 1024$).
    - `--bench-weak [base_dim]`: Runs weak thread scaling (default base: $512$).
    - `--bench-cg`: Runs CPU vs CUDA GPU comparison across 5 resolution presets ($256^2 \to 4096^2$).
    - `--bench-all`: Executes the full test suite sequentially and exports CSV files.
    - `--threads <n>`, `-t <n>`: Locks CPU engine to $n$ threads for simulation.
    - `--csv <file>`: Sets custom CSV export path.
    - `--help`, `-h`: Displays formatted CLI manual.

---

### 3.4. In-Game Interactive Performance Dashboard (GUI)
* **File**: `include/gui.h`
  - Added enum `BenchmarkViewMode { CPU_VS_GPU, THREAD_SCALING }`.
  - Added `ThreadBenchmarkSuite thread_bench_suite;`.
  - Added `std::function<void()> on_run_thread_benchmark;`.
* **File**: `src/gui.cpp`
  - Updated Section 7 (Sidebar):
    - Paired `[CPU vs GPU]` and `[THREAD SCALE]` buttons side-by-side using `half_w`.
    - Added `[VIEW MODE]` button to toggle between comparison views without losing data.
    - Added `[CLOSE]` button.
  - Updated `draw_benchmark_overlay()`:
    - Added rendering path for OpenMP Thread Scaling table.
    - Implemented color-coded efficiency highlighting: Green ($\ge 80\%$), Amber ($50\%-79\%$), Blue ($<50\%$).
    - Added live progress status indicator (`[ N / M threads complete ]`).

---

### 3.5. Distributed-Memory 2D Cartesian MPI Engine
* **File**: `src/main_mpi.cpp` (Brand New Implementation)
  - **Topology**: Uses `MPI_Dims_create` and `MPI_Cart_create` with periodic boundaries (`periods = {1, 1}`).
  - **Neighbor Shifts**: Finds North, South, West, East ranks via `MPI_Cart_shift`.
  - **Domain Partitioning**: Handles dimension remainders evenly across rank coordinates.
  - **Two-Stage Halo Exchange**:
    - *Stage 1 (Vertical)*: Non-blocking exchange of North/South rows (`pitch` contiguous bytes).
    - *Stage 2 (Horizontal)*: Non-blocking exchange of East/West columns of height `local_h + 2` using `MPI_Type_vector`. This automatically transfers diagonal corners in 4 messages instead of 8.
  - **Latency Hiding (Overlap)**: Computes local inner cells $[2 \dots W-1] \times [2 \dots H-1]$ asynchronously while network packets are in-flight, synchronizes via `MPI_Waitall`, then computes outer boundary rows/columns.
  - **Decentralized Seeding**: Local deterministic coordinate hashing `hash_coord(gx, gy, seed)` eliminates rank-0 broadcast serialization.
  - **Safe Thread Cap**: Auto-caps OpenMP to 1 thread per rank if `OMP_NUM_THREADS` is unset, preventing multi-threaded oversubscription across ranks.
  - **Accurate HPC Timing**: Times 100 generations (after 10 warm-up) using `MPI_Wtime()`, computes critical-path maximum time via `MPI_Reduce(..., MPI_MAX)`, calculates workload imbalance %, and verifies global alive cell count via `MPI_Reduce(..., MPI_SUM)`.

---

### 3.6. Build System & Continuous Integration
* **File**: `Makefile`
  - Added `MPICXX ?= mpicxx`.
  - Added `MPIFLAGS = -std=c++14 -O3 -Wall -fopenmp`.
  - Added `mpi` target that auto-detects `$(MPICXX)` and produces `bin/game_of_life_mpi` (with symlink `game_of_life_mpi`).
  - Added helpful error handling instructing how to install `libopenmpi-dev` if missing.
  - Updated `clean` and `help` targets.
* **File**: `.github/workflows/build-artifacts.yml`
  - Added `-fopenmp` to the Windows MinGW compilation command so Windows binary releases include multi-threaded OpenMP acceleration.

---

### 3.7. Automated Scientific Visualization Suite
* **File**: `scripts/plot_scaling.py` (Brand New Implementation)
  - Automatically identifies whether an input CSV is OpenMP thread scaling (`benchmark_thread_scaling.csv`) or MPI distributed scaling (`benchmark_mpi.csv`).
  - Generates publication-ready 300-DPI charts:
    - `strong_scaling_speedup.png` / `mpi_scaling_speedup.png`: Measured speedup vs Ideal linear speedup vs Amdahl's Law model.
    - `parallel_efficiency.png` / `mpi_parallel_efficiency.png`: Parallel efficiency bars with 80% (green) and 50% (amber) threshold indicators.
    - `throughput_scaling.png` / `mpi_throughput_scaling.png`: Computational throughput in Million Cells/second.
  - Computes ASCII preview curves in terminal output.

---

### 3.8. HPC Assignment Deliverables & Documentation
* **File**: `plan.md`
  - Comprehensive HPC engineering plan.
  - Mathematical derivations for Speedup, Parallel Efficiency, Amdahl's Law, Gustafson's Law, and Throughput.
  - Technical architecture comparison matrix (Shared Memory OpenMP vs Distributed Memory MPI).
  - Complete **word-for-word 3-minute video presentation script** divided across 4 slides with exact timings.
* **File**: `README.md`
  - Documented CLI headless benchmarking commands.
  - Documented Python visualization script usage.
  - Documented distributed MPI compilation and execution commands.

---

## 4. Verification and Benchmark Results

All components have been compiled and verified on the local host machine (Intel Core i5-11400H, 6 physical cores, 12 logical threads):

### A. OpenMP Strong Scaling ($1024 \times 1024$ Grid, 100 Iterations)
| Threads ($p$) | Step Time | Speedup ($S_p$) | Efficiency ($E_p$) | Throughput | Amdahl Serial Fraction ($s$) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 2.42 ms | 1.00x | 100.0% | 432.7 MCells/s | Baseline |
| 2 | 1.15 ms | 2.10x | 104.9% | 908.1 MCells/s | 0.00% |
| 4 | 0.60 ms | 4.03x | 100.7% | 1,743.1 MCells/s | 0.00% |
| 6 | 0.67 ms | 3.62x | 60.3% | 1,565.0 MCells/s | 13.18% |
| 8 | 0.51 ms | 4.78x | 59.8% | 2,070.1 MCells/s | 9.60% |
| 12 | 0.50 ms | 4.84x | 40.3% | 2,092.9 MCells/s | 13.46% |

### B. Distributed 2D Cartesian MPI Scaling ($1024 \times 1024$ Grid, 100 Iterations)
| MPI Ranks ($P$) | Process Grid | Step Time | Speedup ($S_P$) | Efficiency ($E_P$) | Throughput | Workload Imbalance |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | $1 \times 1$ | 3.31 ms | 1.00x | 100.0% | 316.6 MCells/s | 0.00% |
| 2 | $1 \times 2$ | 1.70 ms | 1.94x | 97.2% | 615.5 MCells/s | 0.00% |
| 4 | $2 \times 2$ | 1.02 ms | 3.23x | 80.8% | 1,022.1 MCells/s | 0.00% |
| 6 | $2 \times 3$ | 0.95 ms | 3.47x | 57.8% | 1,098.8 MCells/s | 0.96% |
