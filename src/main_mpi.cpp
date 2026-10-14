#include <mpi.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <string>
#include <cstring>
#include <algorithm>

// Deterministic 2D coordinate hash for decentralized grid seeding
static inline unsigned int hash_coord(unsigned int x, unsigned int y, unsigned int seed) {
    unsigned int a = x * 73856093u ^ y * 19349663u ^ seed * 83492791u;
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
}

int main(int argc, char* argv[]) {
    // 1. Initialize MPI
    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    #ifdef _OPENMP
    // For pure MPI benchmarks, default to 1 thread per rank to prevent CPU oversubscription
    if (std::getenv("OMP_NUM_THREADS") == nullptr) {
        omp_set_num_threads(1);
    }
    #endif

    // 2. Parse CLI Parameters
    int global_w = 1024;
    int global_h = 1024;
    int iterations = 100;
    std::string csv_filename = "benchmark_mpi.csv";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            if (world_rank == 0) {
                std::cout << "Usage: mpirun -np <P> " << argv[0] << " [width] [height] [iterations] [--csv filename]\n\n"
                          << "Distributed-Memory Conway's Game of Life (2D Cartesian MPI)\n"
                          << "  width       : Global grid width (default: 1024)\n"
                          << "  height      : Global grid height (default: 1024)\n"
                          << "  iterations  : Number of generations to benchmark (default: 100)\n"
                          << "  --csv <file>: Output CSV filepath (default: benchmark_mpi.csv)\n\n";
            }
            MPI_Finalize();
            return 0;
        } else if (arg == "--csv" && i + 1 < argc) {
            csv_filename = argv[++i];
        } else if (i == 1 && arg[0] != '-') {
            global_w = std::atoi(argv[1]);
            if (i + 1 < argc && argv[i+1][0] != '-') global_h = std::atoi(argv[++i]);
            if (i + 1 < argc && argv[i+1][0] != '-') iterations = std::atoi(argv[++i]);
        }
    }

    if (global_w <= 0) global_w = 1024;
    if (global_h <= 0) global_h = 1024;
    if (iterations <= 0) iterations = 100;

    // 3. Set up 2D Virtual Cartesian Topology (Toroidal Periodic Boundaries)
    int dims[2] = {0, 0};
    MPI_Dims_create(world_size, 2, dims); // dims[0] = n_ranks_y, dims[1] = n_ranks_x
    int periods[2] = {1, 1}; // Periodic in both dimensions (toroidal)
    MPI_Comm cart_comm;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cart_comm);

    int cart_rank;
    MPI_Comm_rank(cart_comm, &cart_rank);
    int coords[2];
    MPI_Cart_coords(cart_comm, cart_rank, 2, coords);

    int n_ranks_y  = dims[0];
    int n_ranks_x  = dims[1];
    int my_coord_y = coords[0];
    int my_coord_x = coords[1];

    // Find orthogonal neighbor ranks via Cartesian shift
    int rank_north, rank_south, rank_west, rank_east;
    MPI_Cart_shift(cart_comm, 0, 1, &rank_north, &rank_south); // Dimension 0: Y (rows)
    MPI_Cart_shift(cart_comm, 1, 1, &rank_west, &rank_east);   // Dimension 1: X (cols)

    // 4. Calculate Local Subdomain Dimensions with Remainder Balancing
    int base_h = global_h / n_ranks_y;
    int rem_h  = global_h % n_ranks_y;
    int local_h = base_h + (my_coord_y < rem_h ? 1 : 0);
    int global_start_y = my_coord_y * base_h + std::min(my_coord_y, rem_h);

    int base_w = global_w / n_ranks_x;
    int rem_w  = global_w % n_ranks_x;
    int local_w = base_w + (my_coord_x < rem_w ? 1 : 0);
    int global_start_x = my_coord_x * base_w + std::min(my_coord_x, rem_w);

    // Pitch: width including 2 ghost cell columns (1 left, 1 right)
    int pitch = local_w + 2;
    int total_cells_with_halos = pitch * (local_h + 2);

    std::vector<unsigned char> current_grid(total_cells_with_halos, 0);
    std::vector<unsigned char> next_grid(total_cells_with_halos, 0);

    #define IDX(x, y) ((y) * pitch + (x))

    // 5. Decentralized Deterministic Initialization (Zero Master Bottleneck)
    for (int y = 1; y <= local_h; ++y) {
        int gy = global_start_y + y - 1;
        for (int x = 1; x <= local_w; ++x) {
            int gx = global_start_x + x - 1;
            unsigned int h = hash_coord((unsigned int)gx, (unsigned int)gy, 12345u);
            current_grid[IDX(x, y)] = ((h % 100u) < 20u) ? 1 : 0;
        }
    }

    // 6. Define Derived Datatype for Strided Vertical Columns (Option A)
    // Height: local_h + 2 (includes corners populated by vertical exchange)
    MPI_Datatype col_type;
    MPI_Type_vector(local_h + 2, 1, pitch, MPI_UNSIGNED_CHAR, &col_type);
    MPI_Type_commit(&col_type);

    if (world_rank == 0) {
        std::cout << "======================================================================================\n"
                  << "            CONWAY'S GAME OF LIFE - DISTRIBUTED MPI (2D CARTESIAN)\n"
                  << "======================================================================================\n"
                  << " Total MPI Ranks     : " << world_size << " (" << n_ranks_x << " x " << n_ranks_y << " process grid)\n"
                  << " Global Grid Size    : " << global_w << " x " << global_h << " (" << ((long long)global_w * global_h / 1000000.0) << "M cells)\n"
                  << " Local Rank 0 Grid   : " << local_w << " x " << local_h << " (+ 1-cell halo ring)\n"
                  << " Decomposition Model : 2D Block with Non-Blocking Asynchronous Halo Overlap\n"
                  << " Boundary Topology   : Toroidal Periodic (MPI_Cart_create)\n"
                  << " Column Datatype     : Zero-Copy Strided Vector (MPI_Type_vector)\n"
                  << " Benchmark Steps     : " << iterations << " iterations (10 warmup steps)\n"
                  << "======================================================================================\n";
    }

    // Function performing one step with Two-Stage Halo Exchange & Latency Overlap
    auto step_simulation = [&]() {
        MPI_Request reqs_v[4];
        MPI_Request reqs_h[4];

        // --- STAGE 1: Vertical Halo Exchange (North / South) ---
        // Receive into ghost rows (Row 0 from North, Row local_h+1 from South)
        MPI_Irecv(&current_grid[IDX(0, 0)], pitch, MPI_UNSIGNED_CHAR, rank_north, 10, cart_comm, &reqs_v[0]);
        MPI_Irecv(&current_grid[IDX(0, local_h + 1)], pitch, MPI_UNSIGNED_CHAR, rank_south, 11, cart_comm, &reqs_v[1]);
        // Send boundary rows (Row local_h to South, Row 1 to North)
        MPI_Isend(&current_grid[IDX(0, local_h)], pitch, MPI_UNSIGNED_CHAR, rank_south, 10, cart_comm, &reqs_v[2]);
        MPI_Isend(&current_grid[IDX(0, 1)], pitch, MPI_UNSIGNED_CHAR, rank_north, 11, cart_comm, &reqs_v[3]);

        // --- OVERLAP COMPUTATION: Evolve the Inner Grid (Independent of Halos) ---
        // Cells in [2 .. local_w - 1] x [2 .. local_h - 1] do not touch halo cells!
        #pragma omp parallel for schedule(static)
        for (int y = 2; y <= local_h - 1; ++y) {
            int row_above = (y - 1) * pitch;
            int row_curr  = y * pitch;
            int row_below = (y + 1) * pitch;
            for (int x = 2; x <= local_w - 1; ++x) {
                int neighbors = current_grid[row_above + x - 1] + current_grid[row_above + x] + current_grid[row_above + x + 1]
                              + current_grid[row_curr  + x - 1]                               + current_grid[row_curr  + x + 1]
                              + current_grid[row_below + x - 1] + current_grid[row_below + x] + current_grid[row_below + x + 1];
                unsigned char c = current_grid[row_curr + x];
                next_grid[row_curr + x] = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
            }
        }

        // Complete Stage 1 (Ensures North/South halos are received before horizontal exchange)
        MPI_Waitall(4, reqs_v, MPI_STATUSES_IGNORE);

        // --- STAGE 2: Horizontal Halo Exchange (East / West) ---
        // Exchanging full columns (length local_h + 2) automatically routes corner halo data!
        MPI_Irecv(&current_grid[IDX(0, 0)], 1, col_type, rank_west, 20, cart_comm, &reqs_h[0]);
        MPI_Irecv(&current_grid[IDX(local_w + 1, 0)], 1, col_type, rank_east, 21, cart_comm, &reqs_h[1]);
        MPI_Isend(&current_grid[IDX(local_w, 0)], 1, col_type, rank_east, 20, cart_comm, &reqs_h[2]);
        MPI_Isend(&current_grid[IDX(1, 0)], 1, col_type, rank_west, 21, cart_comm, &reqs_h[3]);

        // Wait for East/West halos to arrive
        MPI_Waitall(4, reqs_h, MPI_STATUSES_IGNORE);

        // --- BOUNDARY COMPUTATION: Compute the 1-cell outer boundary ring ---
        // 1. Top border row (y = 1)
        {
            int row_above = 0;
            int row_curr  = 1 * pitch;
            int row_below = 2 * pitch;
            for (int x = 1; x <= local_w; ++x) {
                int neighbors = current_grid[row_above + x - 1] + current_grid[row_above + x] + current_grid[row_above + x + 1]
                              + current_grid[row_curr  + x - 1]                               + current_grid[row_curr  + x + 1]
                              + current_grid[row_below + x - 1] + current_grid[row_below + x] + current_grid[row_below + x + 1];
                unsigned char c = current_grid[row_curr + x];
                next_grid[row_curr + x] = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
            }
        }

        // 2. Bottom border row (y = local_h)
        if (local_h > 1) {
            int row_above = (local_h - 1) * pitch;
            int row_curr  = local_h * pitch;
            int row_below = (local_h + 1) * pitch;
            for (int x = 1; x <= local_w; ++x) {
                int neighbors = current_grid[row_above + x - 1] + current_grid[row_above + x] + current_grid[row_above + x + 1]
                              + current_grid[row_curr  + x - 1]                               + current_grid[row_curr  + x + 1]
                              + current_grid[row_below + x - 1] + current_grid[row_below + x] + current_grid[row_below + x + 1];
                unsigned char c = current_grid[row_curr + x];
                next_grid[row_curr + x] = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
            }
        }

        // 3. Left border col (x = 1, y from 2 to local_h - 1)
        for (int y = 2; y <= local_h - 1; ++y) {
            int row_above = (y - 1) * pitch;
            int row_curr  = y * pitch;
            int row_below = (y + 1) * pitch;
            int x = 1;
            int neighbors = current_grid[row_above + x - 1] + current_grid[row_above + x] + current_grid[row_above + x + 1]
                          + current_grid[row_curr  + x - 1]                               + current_grid[row_curr  + x + 1]
                          + current_grid[row_below + x - 1] + current_grid[row_below + x] + current_grid[row_below + x + 1];
            unsigned char c = current_grid[row_curr + x];
            next_grid[row_curr + x] = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
        }

        // 4. Right border col (x = local_w, y from 2 to local_h - 1)
        if (local_w > 1) {
            for (int y = 2; y <= local_h - 1; ++y) {
                int row_above = (y - 1) * pitch;
                int row_curr  = y * pitch;
                int row_below = (y + 1) * pitch;
                int x = local_w;
                int neighbors = current_grid[row_above + x - 1] + current_grid[row_above + x] + current_grid[row_above + x + 1]
                              + current_grid[row_curr  + x - 1]                               + current_grid[row_curr  + x + 1]
                              + current_grid[row_below + x - 1] + current_grid[row_below + x] + current_grid[row_below + x + 1];
                unsigned char c = current_grid[row_curr + x];
                next_grid[row_curr + x] = (neighbors == 3 || (c == 1 && neighbors == 2)) ? 1 : 0;
            }
        }

        // Swap state pointers
        std::swap(current_grid, next_grid);
    };

    // 7. Warmup Phase (10 iterations to stabilize network and cache)
    for (int step = 0; step < 10; ++step) {
        step_simulation();
    }

    // Synchronize all ranks before timing
    MPI_Barrier(cart_comm);

    // 8. Timed Benchmark Execution (100 iterations)
    double t_start = MPI_Wtime();
    for (int step = 0; step < iterations; ++step) {
        step_simulation();
    }
    double t_end = MPI_Wtime();
    double local_elapsed = t_end - t_start;

    // 9. Reduce Execution Time (HPC Metric: Max Time of Slowest Rank)
    double max_time = 0.0;
    double min_time = 0.0;
    double sum_time = 0.0;
    MPI_Reduce(&local_elapsed, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, cart_comm);
    MPI_Reduce(&local_elapsed, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0, cart_comm);
    MPI_Reduce(&local_elapsed, &sum_time, 1, MPI_DOUBLE, MPI_SUM, 0, cart_comm);

    // 10. Population Verification (Lazy Global Reduction at End)
    long long local_alive = 0;
    for (int y = 1; y <= local_h; ++y) {
        for (int x = 1; x <= local_w; ++x) {
            local_alive += current_grid[IDX(x, y)];
        }
    }
    long long global_alive = 0;
    MPI_Reduce(&local_alive, &global_alive, 1, MPI_LONG_LONG, MPI_SUM, 0, cart_comm);

    // 11. Report Summary and Export Results
    if (world_rank == 0) {
        double avg_time = sum_time / world_size;
        double step_ms = (max_time * 1000.0) / iterations;
        double gen_per_sec = 1000.0 / step_ms;
        double throughput_mcells = ((double)global_w * global_h) / (step_ms * 1000.0);
        double imbalance_pct = ((max_time - min_time) / max_time) * 100.0;

        std::cout << "\n======================================================================================\n"
                  << "                             MPI BENCHMARK RESULTS\n"
                  << "======================================================================================\n"
                  << " Benchmark Duration  : " << std::fixed << std::setprecision(4) << max_time << " seconds (" << iterations << " steps)\n"
                  << " Execution Time/Step : " << std::setprecision(3) << step_ms << " ms\n"
                  << " Generations / Sec   : " << std::setprecision(1) << gen_per_sec << " gen/s\n"
                  << " Throughput          : " << std::setprecision(1) << throughput_mcells << " Megacells / second\n"
                  << " Final Population    : " << global_alive << " live cells verified across all ranks\n"
                  << " Workload Imbalance  : " << std::setprecision(2) << imbalance_pct << "% (Min: " << min_time << "s, Max: " << max_time << "s, Avg: " << avg_time << "s)\n"
                  << "======================================================================================\n\n";

        // Export to CSV
        std::ofstream csv(csv_filename, std::ios::app);
        if (csv.is_open()) {
            csv.seekp(0, std::ios::end);
            if (csv.tellp() == 0) {
                csv << "Ranks,RanksX,RanksY,GridWidth,GridHeight,TotalCells,Iterations,StepTime_ms,GenPerSec,Throughput_MCellsSec,Imbalance_pct,FinalPopulation\n";
            }
            csv << world_size << ","
                << n_ranks_x << ","
                << n_ranks_y << ","
                << global_w << ","
                << global_h << ","
                << (long long)global_w * global_h << ","
                << iterations << ","
                << std::setprecision(4) << step_ms << ","
                << std::setprecision(2) << gen_per_sec << ","
                << std::setprecision(2) << throughput_mcells << ","
                << std::setprecision(2) << imbalance_pct << ","
                << global_alive << "\n";
            csv.close();
            std::cout << "[INFO] Appended benchmark row to: " << csv_filename << "\n";
        }
    }

    // Clean up
    MPI_Type_free(&col_type);
    MPI_Comm_free(&cart_comm);
    MPI_Finalize();
    return 0;
}
