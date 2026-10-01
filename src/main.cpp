#include <iostream>
#include <iomanip>
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>
#include <SDL2/SDL.h>

#include "engine.h"
#include "font8x8.h"
#include "gui.h"

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n"
              << "       Conway's Game of Life - Interactive GUI\n"
              << "========================================================\n";

    // 1. Grid Resolution (defaults to 512x512)
    int grid_w = 512;
    int grid_h = 512;
    if (argc >= 3) {
        grid_w = std::atoi(argv[1]);
        grid_h = std::atoi(argv[2]);
    }
    if (grid_w <= 0) grid_w = 512;
    if (grid_h <= 0) grid_h = 512;

    std::cout << "[INFO] Simulation Grid Size: " << grid_w << " x " << grid_h << "\n";

    // 2. Hardware Acceleration Check & Simulation Engine Initialization
    bool has_cuda = is_cuda_available();
    std::cout << "[INFO] CUDA GPU Acceleration: " << (has_cuda ? "Available" : "Not Detected") << "\n";

    std::unique_ptr<SimulationEngine> engine = create_simulation_engine(has_cuda, grid_w, grid_h);
    if (!engine) {
        std::cerr << "[ERROR] Could not initialize simulation engine!\n";
        return 1;
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
    gui.init(win_w, win_h);
    gui.cuda_capable = has_cuda;
    gui.engine_name = engine->is_gpu() ? "ENGINE: CUDA (GPU)" : "ENGINE: CPU (OpenMP)";

    // Connect GUI Callbacks
    unsigned long long generation = 0;

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
        gui.zoom = 1.0f;
        gui.pan_x = 0.0f;
        gui.pan_y = 0.0f;
    };

    // 6. Create SDL Streaming Texture for the Grid
    SDL_Texture* grid_texture = SDL_CreateTexture(
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
