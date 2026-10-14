#include <iostream>
#include <iomanip>
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>
#if defined(__has_include)
  #if __has_include(<SDL2/SDL.h>)
    #include <SDL2/SDL.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
  #else
    #include <SDL2/SDL.h>
  #endif
#else
  #include <SDL2/SDL.h>
#endif

#include "engine.h"
#include "font8x8.h"
#include "gui.h"
#include "benchmark.h"
#include <thread>
#include <mutex>
#ifdef _OPENMP
#include <omp.h>
#endif

static std::vector<int> get_scaling_thread_counts() {
    int max_t = 1;
    #ifdef _OPENMP
    max_t = omp_get_max_threads();
    #endif
    std::vector<int> t_counts;
    for (int t = 1; t <= max_t; t *= 2) {
        t_counts.push_back(t);
    }
    // Include 6 for 6-core 12-thread architectures
    if (max_t == 12 && std::find(t_counts.begin(), t_counts.end(), 6) == t_counts.end()) {
        t_counts.push_back(6);
    }
    if (std::find(t_counts.begin(), t_counts.end(), max_t) == t_counts.end()) {
        t_counts.push_back(max_t);
    }
    std::sort(t_counts.begin(), t_counts.end());
    return t_counts;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n"
              << "       Conway's Game of Life - HPC Accelerated\n"
              << "========================================================\n";

    // 1. Parse Command-Line Options
    int grid_w = 512;
    int grid_h = 512;
    int user_thread_limit = 0;
    bool run_bench_threads_cli = false;
    bool run_bench_weak_cli = false;
    bool run_bench_cg_cli = false;
    bool run_bench_all_cli = false;
    std::string custom_csv_file = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options] [width] [height]\n\n"
                      << "Simulation Options:\n"
                      << "  [width] [height]           Initial grid resolution (default: 512 512)\n"
                      << "  --threads, -t <n>          Lock CPU engine to <n> OpenMP threads\n\n"
                      << "HPC Benchmarking Options (Headless Mode - No GUI / X11 Required):\n"
                      << "  --bench-threads [w] [h]    Run OpenMP strong thread scaling benchmark (default: 1024x1024)\n"
                      << "  --bench-weak [base_dim]    Run OpenMP weak thread scaling benchmark (default: 512)\n"
                      << "  --bench-cg                 Run CPU vs CUDA GPU benchmark across resolution presets\n"
                      << "  --bench-all                Run CPU vs GPU + Strong + Weak scaling benchmarks\n"
                      << "  --csv <filename>           Custom CSV export filepath\n"
                      << "  --help, -h                 Display this help menu\n\n";
            return 0;
        } else if (arg == "--bench-threads" || arg == "--bench-strong") {
            run_bench_threads_cli = true;
            if (i + 2 < argc && argv[i+1][0] != '-' && argv[i+2][0] != '-') {
                grid_w = std::atoi(argv[++i]);
                grid_h = std::atoi(argv[++i]);
            }
        } else if (arg == "--bench-weak") {
            run_bench_weak_cli = true;
            if (i + 1 < argc && argv[i+1][0] != '-') {
                grid_w = std::atoi(argv[++i]);
            }
        } else if (arg == "--bench-cg") {
            run_bench_cg_cli = true;
        } else if (arg == "--bench-all") {
            run_bench_all_cli = true;
        } else if (arg == "--csv") {
            if (i + 1 < argc) custom_csv_file = argv[++i];
        } else if (arg == "--threads" || arg == "-t") {
            if (i + 1 < argc) user_thread_limit = std::atoi(argv[++i]);
        } else if (i == 1 && argc >= 3 && argv[1][0] != '-' && argv[2][0] != '-') {
            grid_w = std::atoi(argv[1]);
            grid_h = std::atoi(argv[2]);
            ++i;
        }
    }

    if (grid_w <= 0) grid_w = 512;
    if (grid_h <= 0) grid_h = 512;

    bool has_cuda = is_cuda_available();
    std::vector<int> thread_counts = get_scaling_thread_counts();

    // 2. Headless CLI Benchmarking Mode (Runs on HPC Clusters without X11)
    if (run_bench_all_cli || run_bench_threads_cli || run_bench_weak_cli || run_bench_cg_cli) {
        std::cout << "[HPC BENCHMARK MODE] Running without GUI display...\n";
        std::cout << "[INFO] CUDA GPU Acceleration: " << (has_cuda ? "Available" : "Not Detected") << "\n";
        #ifdef _OPENMP
        std::cout << "[INFO] OpenMP Hardware Threads Detected: " << omp_get_max_threads() << "\n";
        #else
        std::cout << "[INFO] OpenMP: Not compiled in (Serial CPU)\n";
        #endif

        if (run_bench_all_cli || run_bench_cg_cli) {
            std::cout << "\n>>> Starting CPU vs GPU Benchmark across standard resolutions...\n";
            std::vector<std::pair<int,int>> sizes = {
                {256, 256}, {512, 512}, {1024, 1024}, {2048, 2048}, {4096, 4096}
            };
            BenchmarkSuite suite;
            BenchmarkRunner::run(sizes, has_cuda, &suite);
            BenchmarkRunner::print_cpu_gpu_console(suite.results);
            std::string csv_path = custom_csv_file.empty() ? "benchmark_cpu_gpu.csv" : custom_csv_file;
            BenchmarkRunner::export_cpu_gpu_csv(csv_path, suite.results);
        }

        if (run_bench_all_cli || run_bench_threads_cli) {
            std::cout << "\n>>> Starting OpenMP Strong Thread Scaling Benchmark (" << grid_w << "x" << grid_h << ")...\n";
            ThreadBenchmarkSuite suite;
            BenchmarkRunner::run_strong_scaling(grid_w, grid_h, thread_counts, &suite);
            BenchmarkRunner::print_thread_scaling_console(suite.results, "OPENMP STRONG THREAD SCALING (" + std::to_string(grid_w) + "x" + std::to_string(grid_h) + ")");
            std::string csv_path = custom_csv_file.empty() ? "benchmark_thread_scaling.csv" : custom_csv_file;
            BenchmarkRunner::export_thread_scaling_csv(csv_path, suite.results, "Strong Scaling");
        }

        if (run_bench_all_cli || run_bench_weak_cli) {
            int base_dim = (grid_w > 0) ? grid_w : 512;
            std::cout << "\n>>> Starting OpenMP Weak Thread Scaling Benchmark (Base: " << base_dim << "x" << base_dim << " per thread)...\n";
            ThreadBenchmarkSuite suite;
            BenchmarkRunner::run_weak_scaling(base_dim, thread_counts, &suite);
            BenchmarkRunner::print_thread_scaling_console(suite.results, "OPENMP WEAK THREAD SCALING (Base: " + std::to_string(base_dim) + "x" + std::to_string(base_dim) + "/thread)");
            std::string csv_path = custom_csv_file.empty() ? "benchmark_weak_scaling.csv" : custom_csv_file;
            BenchmarkRunner::export_thread_scaling_csv(csv_path, suite.results, "Weak Scaling");
        }

        std::cout << "[SUCCESS] All requested benchmarks completed cleanly.\n";
        return 0;
    }

    // 3. Interactive Graphical Mode
    std::cout << "[INFO] Simulation Grid Size: " << grid_w << " x " << grid_h << "\n";
    std::cout << "[INFO] CUDA GPU Acceleration: " << (has_cuda ? "Available" : "Not Detected") << "\n";

    std::unique_ptr<SimulationEngine> engine = create_simulation_engine(has_cuda, grid_w, grid_h);
    if (!engine) {
        std::cerr << "[ERROR] Could not initialize simulation engine!\n";
        return 1;
    }

    if (user_thread_limit > 0) {
        engine->set_num_threads(user_thread_limit);
        std::cout << "[INFO] CPU Thread count locked to: " << user_thread_limit << "\n";
    }

    // Seed initial state
    engine->randomize(20, (unsigned int)time(nullptr));

    // 3. Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[ERROR] SDL Initialization failed: " << SDL_GetError() << "\n";
        return 1;
    }

    int win_w = 1280;
    int win_h = 768;

    SDL_Window* window = SDL_CreateWindow(
        "Conway's Game of Life [Hardware Accelerated]",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "[ERROR] Window creation failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        // Fallback to software renderer if hardware acceleration is unavailable
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::cerr << "[ERROR] Renderer creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 4. Initialize Font Renderer
    FontRenderer font;
    if (!font.init(renderer)) {
        std::cerr << "[ERROR] Failed to initialize built-in font atlas!\n";
    }

    // 5. Initialize GUI & Layout
    GUI gui;
    gui.current_grid_w = grid_w;
    gui.current_grid_h = grid_h;
    gui.init(win_w, win_h);
    gui.cuda_capable = has_cuda;
    gui.engine_name = engine->is_gpu() ? "ENGINE: CUDA (GPU)" : "ENGINE: CPU (OpenMP)";

    // Forward declare grid_texture pointer
    SDL_Texture* grid_texture = nullptr;

    // Connect GUI Callbacks
    unsigned long long generation = 0;

    gui.on_change_grid_size = [&](int new_w, int new_h) {
        if (new_w == grid_w && new_h == grid_h) return;

        std::cout << "[INFO] Resizing simulation grid: " << grid_w << "x" << grid_h
                  << " -> " << new_w << "x" << new_h << "\n";

        grid_w = new_w;
        grid_h = new_h;
        gui.current_grid_w = grid_w;
        gui.current_grid_h = grid_h;

        engine->init(grid_w, grid_h);
        engine->randomize(20, (unsigned int)time(nullptr));
        generation = 0;

        if (grid_texture) {
            SDL_DestroyTexture(grid_texture);
        }
        grid_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            grid_w, grid_h
        );

        gui.fit_to_canvas(grid_w, grid_h);

        long long total_cells = (long long)grid_w * grid_h;
        std::ostringstream ss;
        ss << "Grid resized: " << grid_w << "x" << grid_h << " (";
        if (total_cells >= 1000000) {
            ss << std::fixed << std::setprecision(1) << ((double)total_cells / 1000000.0) << "M cells)! Wheel: zoom, Drag: pan.";
        } else {
            ss << (total_cells / 1000) << "K cells)! Wheel: zoom, Drag: pan.";
        }
        gui.status_msg = ss.str();
    };

    gui.on_step_clicked = [&]() {
        engine->step();
        generation++;
    };

    gui.on_clear_clicked = [&]() {
        engine->clear();
        generation = 0;
    };

    gui.on_randomize_clicked = [&]() {
        engine->randomize(20, (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count());
        generation = 0;
    };

    gui.on_toggle_engine = [&]() {
        if (!is_cuda_available()) {
            gui.status_msg = "No CUDA GPU detected. Running multi-threaded CPU engine.";
            return;
        }

        bool switch_to_gpu = !engine->is_gpu();
        std::cout << "[INFO] Switching compute engine to: " << (switch_to_gpu ? "CUDA GPU" : "CPU") << "\n";

        // Preserve current state across switch
        const unsigned char* old_state = engine->get_grid_state();
        std::vector<unsigned char> state_backup(old_state, old_state + (size_t)grid_w * grid_h);

        engine = create_simulation_engine(switch_to_gpu, grid_w, grid_h);
        engine->set_grid_state(state_backup.data());

        gui.engine_name = engine->is_gpu() ? "ENGINE: CUDA (GPU)" : "ENGINE: CPU (OpenMP)";
        gui.status_msg = "Switched engine to: " + engine->get_name();
    };

    gui.on_reset_view = [&]() {
        gui.reset_view(grid_w, grid_h);
        gui.status_msg = "View reset to 100% scale.";
    };

    gui.on_fit_canvas = [&]() {
        gui.fit_to_canvas(grid_w, grid_h);
        gui.status_msg = "Fit entire grid inside canvas.";
    };

    gui.on_run_benchmark = [&]() {
        bool was_running = gui.is_running;
        gui.is_running = false; // Pause during benchmark

        std::vector<std::pair<int,int>> sizes = {
            {256, 256}, {512, 512}, {1024, 1024}, {2048, 2048}, {4096, 4096}
        };

        std::thread([&, was_running, sizes]() {
            BenchmarkRunner::run(sizes, has_cuda, &gui.bench_suite);
            BenchmarkRunner::export_cpu_gpu_csv("benchmark_cpu_gpu.csv", gui.bench_suite.results);
            gui.is_running = was_running;
            gui.status_msg = "CPU vs GPU benchmark complete! Saved to benchmark_cpu_gpu.csv";
        }).detach();
    };

    gui.on_run_thread_benchmark = [&]() {
        bool was_running = gui.is_running;
        gui.is_running = false; // Pause during benchmark

        int bench_w = grid_w;
        int bench_h = grid_h;
        std::vector<int> t_counts = thread_counts;

        std::thread([&, was_running, bench_w, bench_h, t_counts]() {
            BenchmarkRunner::run_strong_scaling(bench_w, bench_h, t_counts, &gui.thread_bench_suite);
            BenchmarkRunner::export_thread_scaling_csv("benchmark_thread_scaling.csv", gui.thread_bench_suite.results, "Strong Scaling");
            gui.is_running = was_running;
            gui.status_msg = "Thread scaling complete! Saved to benchmark_thread_scaling.csv";
        }).detach();
    };

    // 6. Create SDL Streaming Texture for the Grid
    grid_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        grid_w, grid_h
    );
    if (!grid_texture) {
        std::cerr << "[ERROR] Texture creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 7. Simulation & Event Loop
    bool quit = false;
    bool mouse_drawing = false;
    bool mouse_erasing = false;

    auto last_fps_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    float current_fps = 60.0f;

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
                break;
            }

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                win_w = e.window.data1;
                win_h = e.window.data2;
                gui.update_layout(win_w, win_h);
                continue;
            }

            // Let GUI process the event first
            if (gui.handle_event(e)) {
                continue;
            }

            // Keyboard Shortcuts
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_SPACE:
                        gui.is_running = !gui.is_running;
                        gui.status_msg = gui.is_running ? "Simulation running." : "Simulation paused.";
                        break;
                    case SDLK_s:
                        if (!gui.is_running) {
                            engine->step();
                            generation++;
                            gui.status_msg = "Stepped 1 generation.";
                        }
                        break;
                    case SDLK_r:
                        engine->randomize(20, (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count());
                        generation = 0;
                        gui.status_msg = "Grid randomized.";
                        break;
                    case SDLK_c:
                        engine->clear();
                        generation = 0;
                        gui.status_msg = "Grid cleared.";
                        break;
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                    case SDLK_UP:
                        gui.delay_ms = std::max(0, gui.delay_ms - 4);
                        gui.status_msg = "Speed set to " + std::to_string(gui.delay_ms) + " ms delay.";
                        break;
                    case SDLK_MINUS:
                    case SDLK_DOWN:
                        gui.delay_ms = std::min(200, gui.delay_ms + 4);
                        gui.status_msg = "Speed set to " + std::to_string(gui.delay_ms) + " ms delay.";
                        break;
                    case SDLK_1:
                        if (gui.on_change_grid_size) gui.on_change_grid_size(256, 256);
                        break;
                    case SDLK_2:
                        if (gui.on_change_grid_size) gui.on_change_grid_size(512, 512);
                        break;
                    case SDLK_3:
                        if (gui.on_change_grid_size) gui.on_change_grid_size(1024, 1024);
                        break;
                    case SDLK_4:
                        if (gui.on_change_grid_size) gui.on_change_grid_size(2048, 2048);
                        break;
                    case SDLK_5:
                        if (gui.on_change_grid_size) gui.on_change_grid_size(4096, 4096);
                        break;
                    case SDLK_6:
                        if (gui.on_change_grid_size) gui.on_change_grid_size(1920, 1080);
                        break;
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        quit = true;
                        break;
                    default:
                        break;
                }
            }

            // Mouse Canvas Interaction
            if (gui.mouse_in_canvas) {
                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        mouse_drawing = true;
                        int gx, gy;
                        gui.screen_to_grid(e.button.x, e.button.y, grid_w, grid_h, gx, gy);
                        if (gui.current_tool == ToolMode::DRAW) {
                            engine->set_brush(gx, gy, gui.brush_radius, 1);
                        } else if (gui.current_tool == ToolMode::ERASE) {
                            engine->set_brush(gx, gy, gui.brush_radius, 0);
                        } else if (gui.current_tool == ToolMode::STAMP) {
                            engine->spawn_pattern(gx, gy, gui.patterns[gui.selected_pattern_idx]);
                        }
                    } else if (e.button.button == SDL_BUTTON_RIGHT) {
                        mouse_erasing = true;
                        int gx, gy;
                        gui.screen_to_grid(e.button.x, e.button.y, grid_w, grid_h, gx, gy);
                        engine->set_brush(gx, gy, gui.brush_radius, 0);
                    }
                } else if (e.type == SDL_MOUSEBUTTONUP) {
                    if (e.button.button == SDL_BUTTON_LEFT) mouse_drawing = false;
                    if (e.button.button == SDL_BUTTON_RIGHT) mouse_erasing = false;
                } else if (e.type == SDL_MOUSEMOTION) {
                    int gx, gy;
                    gui.screen_to_grid(e.motion.x, e.motion.y, grid_w, grid_h, gx, gy);
                    gui.mouse_grid_x = gx;
                    gui.mouse_grid_y = gy;

                    // Query hover cell state
                    const unsigned char* grid_ptr = engine->get_grid_state();
                    if (grid_ptr && gx >= 0 && gx < grid_w && gy >= 0 && gy < grid_h) {
                        gui.mouse_cell_alive = (grid_ptr[gy * grid_w + gx] != 0);
                    }

                    if (mouse_drawing) {
                        if (gui.current_tool == ToolMode::DRAW) {
                            engine->set_brush(gx, gy, gui.brush_radius, 1);
                        } else if (gui.current_tool == ToolMode::ERASE) {
                            engine->set_brush(gx, gy, gui.brush_radius, 0);
                        }
                    } else if (mouse_erasing) {
                        engine->set_brush(gx, gy, gui.brush_radius, 0);
                    }
                }
            }
        }

        // Advance simulation
        if (gui.is_running) {
            engine->step();
            generation++;
        }

        // Transfer grid pixels to SDL texture
        const unsigned int* pixel_buffer = engine->get_pixels();
        SDL_UpdateTexture(grid_texture, nullptr, pixel_buffer, grid_w * sizeof(unsigned int));

        // Render Canvas & Grid
        SDL_SetRenderDrawColor(renderer, UITheme::BG_DARK.r, UITheme::BG_DARK.g, UITheme::BG_DARK.b, 255);
        SDL_RenderClear(renderer);

        // Clip grid rendering to the canvas area so it doesn't bleed into sidebar or header
        SDL_RenderSetClipRect(renderer, &gui.canvas_rect);

        float cell_pixel_size = (float)gui.canvas_rect.w / (float)grid_w * gui.zoom;
        SDL_Rect grid_dest = {
            gui.canvas_rect.x + (int)gui.pan_x,
            gui.canvas_rect.y + (int)gui.pan_y,
            std::max(1, (int)((float)grid_w * cell_pixel_size)),
            std::max(1, (int)((float)grid_h * cell_pixel_size))
        };
        SDL_RenderCopy(renderer, grid_texture, nullptr, &grid_dest);

        // Reset clip rect before rendering GUI
        SDL_RenderSetClipRect(renderer, nullptr);

        // Calculate Population & FPS
        int population = engine->get_population();

        frame_count++;
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - last_fps_time;
        if (elapsed.count() >= 0.5f) {
            current_fps = (float)frame_count / elapsed.count();
            frame_count = 0;
            last_fps_time = now;
        }

        // Draw GUI overlay (Header, Sidebar, Controls, Status Bar)
        gui.draw(renderer, font, generation, population, current_fps, grid_w, grid_h);

        SDL_RenderPresent(renderer);

        if (gui.delay_ms > 0) {
            SDL_Delay(gui.delay_ms);
        }
    }

    // Cleanup
    SDL_DestroyTexture(grid_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "[INFO] Application closed cleanly. Generations computed: " << generation << "\n";
    return 0;
}
