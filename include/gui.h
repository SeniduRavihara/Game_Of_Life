#ifndef GUI_H
#define GUI_H

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
#include <string>
#include <vector>
#include <functional>
#include "font8x8.h"
#include "patterns.h"

namespace UITheme {
    const SDL_Color BG_DARK        = { 18,  18,  22, 255 }; // #121216
    const SDL_Color PANEL_BG       = { 26,  26,  34, 255 }; // #1A1A22
    const SDL_Color PANEL_BORDER   = { 44,  44,  58, 255 }; // #2C2C3A
    const SDL_Color HEADER_BG      = { 22,  22,  28, 255 }; // #16161C
    const SDL_Color STATUS_BG      = { 15,  15,  18, 255 }; // #0F0F12

    const SDL_Color BTN_NORMAL     = { 36,  36,  48, 255 }; // #242430
    const SDL_Color BTN_HOVER      = { 52,  52,  70, 255 }; // #343446
    const SDL_Color BTN_ACTIVE     = { 0,  180, 100, 255 }; // Green active
    const SDL_Color BTN_PAUSED     = { 220, 130,  20, 255 }; // Amber/Orange
    const SDL_Color BTN_DANGER     = { 180,  40,  40, 255 }; // Red

    const SDL_Color TEXT_TITLE     = { 255, 255, 255, 255 };
    const SDL_Color TEXT_NORMAL    = { 220, 225, 235, 255 };
    const SDL_Color TEXT_MUTED     = { 140, 145, 160, 255 };
    const SDL_Color ACCENT_GREEN   = { 0,  255, 127, 255 };
    const SDL_Color ACCENT_BLUE    = { 70, 160, 255, 255 };
    const SDL_Color ACCENT_AMBER   = { 255, 180,  40, 255 };
}

enum class ToolMode {
    DRAW,
    ERASE,
    STAMP
};

class UIButton {
public:
    SDL_Rect rect;
    std::string text;
    std::function<void()> on_click;
    bool is_hovered = false;
    bool is_active = false;
    bool is_danger = false;
    bool custom_color = false;
    SDL_Color override_color = {0,0,0,0};

    UIButton(int x, int y, int w, int h, const std::string& label, std::function<void()> callback = nullptr)
        : rect{x, y, w, h}, text(label), on_click(callback) {}

    bool handle_event(const SDL_Event& e);
    void draw(SDL_Renderer* renderer, FontRenderer& font);
};

class GUI {
public:
    int window_width = 1280;
    int window_height = 768;

    // Layout areas
    SDL_Rect header_rect;
    SDL_Rect sidebar_rect;
    SDL_Rect canvas_rect;
    SDL_Rect status_rect;

    // Simulation state references
    bool is_running = true;
    int delay_ms = 16;
    ToolMode current_tool = ToolMode::DRAW;
    int brush_radius = 1;
    int selected_pattern_idx = 0;
    std::vector<Pattern> patterns;

    // UI Buttons
    std::vector<UIButton> buttons;
    int idx_play_pause = -1;
    int idx_draw = -1;
    int idx_erase = -1;
    int idx_stamp = -1;
    int idx_engine = -1;
    int engine_note_y = 0;
    std::vector<int> idx_brush_sizes;
    std::vector<int> idx_grid_sizes;
    std::vector<std::pair<int, int>> grid_size_presets = {
        {256, 256},
        {512, 512},
        {1024, 1024},
        {2048, 2048},
        {4096, 4096},
        {1920, 1080}
    };

    struct SectionLabel {
        std::string title;
        int y;
    };
    std::vector<SectionLabel> section_labels;

    // Status message
    std::string status_msg = "Ready. Left-click canvas to draw, drag to paint.";
    int mouse_grid_x = 0;
    int mouse_grid_y = 0;
    bool mouse_in_canvas = false;
    bool mouse_cell_alive = false;

    // Camera / Viewport
    float zoom = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    bool is_panning = false;
    int last_pan_mx = 0;
    int last_pan_my = 0;

    // Engine info
    std::string engine_name = "CPU (OpenMP)";
    bool cuda_capable = false;

    int current_grid_w = 512;
    int current_grid_h = 512;

    // Callbacks for main loop
    std::function<void()> on_step_clicked;
    std::function<void()> on_clear_clicked;
    std::function<void()> on_randomize_clicked;
    std::function<void()> on_toggle_engine;
    std::function<void()> on_reset_view;
    std::function<void()> on_fit_canvas;
    std::function<void(int new_w, int new_h)> on_change_grid_size;

    GUI();
    void init(int win_w, int win_h);
    void update_layout(int win_w, int win_h);

    void fit_to_canvas(int grid_w, int grid_h);
    void reset_view(int grid_w, int grid_h);

    bool handle_event(const SDL_Event& e);
    void draw(SDL_Renderer* renderer, FontRenderer& font, unsigned long long gen, int population, float fps, int grid_w, int grid_h);

    // Coordinate conversion
    void screen_to_grid(int screen_x, int screen_y, int grid_w, int grid_h, int& grid_x, int& grid_y) const;
    void grid_to_screen(int grid_x, int grid_y, int& screen_x, int& screen_y) const;
};

#endif // GUI_H
