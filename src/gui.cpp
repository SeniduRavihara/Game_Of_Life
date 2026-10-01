#include "gui.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

bool UIButton::handle_event(const SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
        int mx = e.motion.x;
        int my = e.motion.y;
        is_hovered = (mx >= rect.x && mx < rect.x + rect.w && my >= rect.y && my < rect.y + rect.h);
    } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            int mx = e.button.x;
            int my = e.button.y;
            if (mx >= rect.x && mx < rect.x + rect.w && my >= rect.y && my < rect.y + rect.h) {
                if (on_click) on_click();
                return true; // Click consumed
            }
        }
    }
    return false;
}

void UIButton::draw(SDL_Renderer* renderer, FontRenderer& font) {
    SDL_Color fill = UITheme::BTN_NORMAL;
    if (custom_color) {
        fill = override_color;
    } else if (is_active) {
        fill = UITheme::BTN_ACTIVE;
    } else if (is_danger) {
        fill = UITheme::BTN_DANGER;
    } else if (is_hovered) {
        fill = UITheme::BTN_HOVER;
    }

    // Button background
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);

    // Button border
    SDL_Color border = is_hovered ? UITheme::ACCENT_GREEN : UITheme::PANEL_BORDER;
    if (is_active) border = UITheme::TEXT_TITLE;
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);

    // Centered text
    int text_y = rect.y + (rect.h - 8) / 2;
    SDL_Color text_col = is_active ? UITheme::TEXT_TITLE : (is_hovered ? UITheme::TEXT_TITLE : UITheme::TEXT_NORMAL);
    font.draw_text_centered(text, rect.x + rect.w / 2, text_y, text_col, 1);
}

GUI::GUI() {
    patterns = get_available_patterns();
}

void GUI::init(int win_w, int win_h) {
    update_layout(win_w, win_h);
}

void GUI::update_layout(int win_w, int win_h) {
    window_width = win_w;
    window_height = win_h;

    const int header_h = 36;
    const int status_h = 28;
    const int sidebar_w = 280;

    header_rect  = { 0, 0, win_w, header_h };
    status_rect  = { 0, win_h - status_h, win_w, status_h };
    sidebar_rect = { win_w - sidebar_w, header_h, sidebar_w, win_h - header_h - status_h };
    canvas_rect  = { 0, header_h, win_w - sidebar_w, win_h - header_h - status_h };

    buttons.clear();
    section_labels.clear();
    idx_grid_sizes.clear();

    int sx = sidebar_rect.x + 14;
    int sy = sidebar_rect.y + 10;
    int full_w = sidebar_w - 28;
    int half_w = (full_w - 8) / 2;
    int third_w = (full_w - 12) / 3;

    // --- Section 1: Simulation Controls ---
    section_labels.push_back({ "--- SIMULATION CONTROLS ---", sy });
    sy += 14;

    buttons.emplace_back(sx, sy, full_w, 30, is_running ? "|| PAUSE SIMULATION" : "> PLAY SIMULATION", [this]() {
        is_running = !is_running;
        status_msg = is_running ? "Simulation running." : "Simulation paused.";
    });
    idx_play_pause = (int)buttons.size() - 1;
    buttons[idx_play_pause].custom_color = true;
    buttons[idx_play_pause].override_color = is_running ? UITheme::BTN_ACTIVE : UITheme::BTN_PAUSED;
    sy += 34;

    buttons.emplace_back(sx, sy, half_w, 24, "STEP (S)", [this]() {
        if (on_step_clicked) on_step_clicked();
        status_msg = "Stepped 1 generation.";
    });

    buttons.emplace_back(sx + half_w + 8, sy, half_w, 24, "CLEAR (C)", [this]() {
        if (on_clear_clicked) on_clear_clicked();
        status_msg = "Grid cleared.";
    });
    buttons.back().is_danger = true;
    sy += 28;

    buttons.emplace_back(sx, sy, full_w, 24, "RANDOMIZE (R)", [this]() {
        if (on_randomize_clicked) on_randomize_clicked();
        status_msg = "Grid randomized (20% alive).";
    });
    sy += 32;

    // --- Section 2: Speed Controls ---
    section_labels.push_back({ "--- EXECUTION SPEED ---", sy });
    sy += 14;

    buttons.emplace_back(sx, sy, third_w, 22, "<< SLOW", [this]() {
        delay_ms = std::min(200, delay_ms + 8);
        status_msg = "Speed set to " + std::to_string(delay_ms) + " ms delay.";
    });
    buttons.emplace_back(sx + third_w + 6, sy, third_w, 22, "60 FPS", [this]() {
        delay_ms = 16;
        status_msg = "Speed set to 60 FPS (16ms).";
    });
    buttons.emplace_back(sx + (third_w + 6) * 2, sy, third_w, 22, "FAST >>", [this]() {
        delay_ms = std::max(0, delay_ms - 8);
        status_msg = "Speed set to " + std::to_string(delay_ms) + " ms delay.";
    });
    sy += 26;

    buttons.emplace_back(sx, sy, full_w, 22, "MAX SPEED (0ms DELAY)", [this]() {
        delay_ms = 0;
        status_msg = "Max speed unlocked (no frame delay).";
    });
    sy += 30;

    // --- Section 3: Grid Resolution Presets ---
    section_labels.push_back({ "--- GRID RESOLUTION ---", sy });
    sy += 14;

    buttons.emplace_back(sx, sy, half_w, 22, "256x256", [this]() {
        if (on_change_grid_size) on_change_grid_size(256, 256);
    });
    idx_grid_sizes.push_back((int)buttons.size() - 1);

    buttons.emplace_back(sx + half_w + 8, sy, half_w, 22, "512x512", [this]() {
        if (on_change_grid_size) on_change_grid_size(512, 512);
    });
    idx_grid_sizes.push_back((int)buttons.size() - 1);
    sy += 26;

    buttons.emplace_back(sx, sy, half_w, 22, "1024x1024 (1M)", [this]() {
        if (on_change_grid_size) on_change_grid_size(1024, 1024);
    });
    idx_grid_sizes.push_back((int)buttons.size() - 1);

    buttons.emplace_back(sx + half_w + 8, sy, half_w, 22, "2048x2048 (4M)", [this]() {
        if (on_change_grid_size) on_change_grid_size(2048, 2048);
    });
    idx_grid_sizes.push_back((int)buttons.size() - 1);
    sy += 26;

    buttons.emplace_back(sx, sy, full_w, 22, "4096x4096 (16.8M ULTRA)", [this]() {
        if (on_change_grid_size) on_change_grid_size(4096, 4096);
    });
    idx_grid_sizes.push_back((int)buttons.size() - 1);
    sy += 26;

    buttons.emplace_back(sx, sy, full_w, 22, "1920x1080 (FHD 2M)", [this]() {
        if (on_change_grid_size) on_change_grid_size(1920, 1080);
    });
    idx_grid_sizes.push_back((int)buttons.size() - 1);
    sy += 30;

    // --- Section 4: Tools & Brush ---
    section_labels.push_back({ "--- TOOLS & BRUSH ---", sy });
    sy += 14;

    buttons.emplace_back(sx, sy, third_w, 24, "PEN", [this]() {
        current_tool = ToolMode::DRAW;
        status_msg = "Tool: Pen (Draw living cells).";
    });
    idx_draw = (int)buttons.size() - 1;

    buttons.emplace_back(sx + third_w + 6, sy, third_w, 24, "ERASE", [this]() {
        current_tool = ToolMode::ERASE;
        status_msg = "Tool: Erase cells.";
    });
    idx_erase = (int)buttons.size() - 1;

    buttons.emplace_back(sx + (third_w + 6) * 2, sy, third_w, 24, "STAMP", [this]() {
        current_tool = ToolMode::STAMP;
        status_msg = "Tool: Stamp pattern (Click on canvas to place).";
    });
    idx_stamp = (int)buttons.size() - 1;
    sy += 28;

    // Brush sizes
    buttons.emplace_back(sx, sy, third_w, 20, "Size 1", [this]() {
        brush_radius = 0;
        status_msg = "Brush size: 1x1.";
    });
    buttons.emplace_back(sx + third_w + 6, sy, third_w, 20, "Size 3", [this]() {
        brush_radius = 1;
        status_msg = "Brush size: 3x3.";
    });
    buttons.emplace_back(sx + (third_w + 6) * 2, sy, third_w, 20, "Size 5", [this]() {
        brush_radius = 2;
        status_msg = "Brush size: 5x5.";
    });
    sy += 28;

    // --- Section 5: Pattern Stamps ---
    section_labels.push_back({ "--- PATTERN STAMP LIBRARY ---", sy });
    sy += 14;

    for (size_t i = 0; i < patterns.size(); ++i) {
        int px = (i % 2 == 0) ? sx : sx + half_w + 8;
        int py = sy + (int)(i / 2) * 26;

        buttons.emplace_back(px, py, half_w, 22, patterns[i].name, [this, i]() {
            current_tool = ToolMode::STAMP;
            selected_pattern_idx = (int)i;
            status_msg = "Selected: " + patterns[i].name + ". Click canvas to place!";
        });
    }
    sy += ((int)(patterns.size() + 1) / 2) * 26 + 10;

    // --- Section 6: Engine Selection ---
    section_labels.push_back({ "--- COMPUTE ENGINE ---", sy });
    sy += 14;

    buttons.emplace_back(sx, sy, full_w, 26, "SWITCH ENGINE", [this]() {
        if (on_toggle_engine) on_toggle_engine();
    });
    idx_engine = (int)buttons.size() - 1;
    sy += 32;

    // --- Section 7: Camera & View ---
    section_labels.push_back({ "--- CAMERA / VIEW ---", sy });
    sy += 14;

    buttons.emplace_back(sx, sy, half_w, 22, "RESET VIEW", [this]() {
        zoom = 1.0f;
        pan_x = 0.0f;
        pan_y = 0.0f;
        status_msg = "View reset to 100% centered.";
    });
    buttons.emplace_back(sx + half_w + 8, sy, half_w, 22, "FIT CANVAS", [this]() {
        if (on_reset_view) on_reset_view();
        status_msg = "Fit grid to canvas view.";
    });
}

bool GUI::handle_event(const SDL_Event& e) {
    // 1. Check all UI Buttons in sidebar
    for (auto& btn : buttons) {
        if (btn.handle_event(e)) {
            return true;
        }
    }

    // 2. Track mouse position relative to canvas
    if (e.type == SDL_MOUSEMOTION) {
        int mx = e.motion.x;
        int my = e.motion.y;
        mouse_in_canvas = (mx >= canvas_rect.x && mx < canvas_rect.x + canvas_rect.w &&
                           my >= canvas_rect.y && my < canvas_rect.y + canvas_rect.h);

        if (is_panning) {
            pan_x += (mx - last_pan_mx);
            pan_y += (my - last_pan_my);
            last_pan_mx = mx;
            last_pan_my = my;
            return true;
        }
    }

    // 3. Middle click to Pan
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_MIDDLE) {
        if (mouse_in_canvas) {
            is_panning = true;
            last_pan_mx = e.button.x;
            last_pan_my = e.button.y;
            return true;
        }
    } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_MIDDLE) {
        is_panning = false;
    }

    // 4. Mouse wheel zoom
    if (e.type == SDL_MOUSEWHEEL && mouse_in_canvas) {
        float old_zoom = zoom;
        if (e.wheel.y > 0) {
            zoom = std::min(32.0f, zoom * 1.20f);
        } else if (e.wheel.y < 0) {
            zoom = std::max(0.1f, zoom / 1.20f);
        }

        // Adjust pan to zoom towards mouse position
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        float mouse_canvas_x = (float)(mx - canvas_rect.x);
        float mouse_canvas_y = (float)(my - canvas_rect.y);

        pan_x = mouse_canvas_x - (mouse_canvas_x - pan_x) * (zoom / old_zoom);
        pan_y = mouse_canvas_y - (mouse_canvas_y - pan_y) * (zoom / old_zoom);
        status_msg = "Zoom: " + std::to_string((int)(zoom * 100.0f)) + "%";
        return true;
    }

    return false;
}

void GUI::screen_to_grid(int screen_x, int screen_y, int grid_w, int grid_h, int& grid_x, int& grid_y) const {
    float local_x = (float)(screen_x - canvas_rect.x) - pan_x;
    float local_y = (float)(screen_y - canvas_rect.y) - pan_y;

    float cell_size = (float)canvas_rect.w / (float)grid_w * zoom;
    if (cell_size <= 0.001f) cell_size = 1.0f;

    grid_x = (int)(local_x / cell_size);
    grid_y = (int)(local_y / cell_size);
}

void GUI::grid_to_screen(int grid_x, int grid_y, int& screen_x, int& screen_y) const {
    float cell_size = (float)canvas_rect.w / 512.0f * zoom;
    screen_x = canvas_rect.x + (int)(pan_x + (float)grid_x * cell_size);
    screen_y = canvas_rect.y + (int)(pan_y + (float)grid_y * cell_size);
}

void GUI::draw(SDL_Renderer* renderer, FontRenderer& font, unsigned long long gen, int population, float fps, int grid_w, int grid_h) {
    // --- Update button active/highlight states ---
    if (idx_play_pause >= 0 && idx_play_pause < (int)buttons.size()) {
        buttons[idx_play_pause].text = is_running ? "|| PAUSE SIMULATION" : "> PLAY SIMULATION";
        buttons[idx_play_pause].override_color = is_running ? UITheme::BTN_ACTIVE : UITheme::BTN_PAUSED;
    }
    if (idx_draw >= 0 && idx_draw < (int)buttons.size()) {
        buttons[idx_draw].is_active = (current_tool == ToolMode::DRAW);
    }
    if (idx_erase >= 0 && idx_erase < (int)buttons.size()) {
        buttons[idx_erase].is_active = (current_tool == ToolMode::ERASE);
    }
    if (idx_stamp >= 0 && idx_stamp < (int)buttons.size()) {
        buttons[idx_stamp].is_active = (current_tool == ToolMode::STAMP);
    }
    for (size_t i = 0; i < idx_grid_sizes.size() && i < grid_size_presets.size(); ++i) {
        int idx = idx_grid_sizes[i];
        if (idx >= 0 && idx < (int)buttons.size()) {
            buttons[idx].is_active = (current_grid_w == grid_size_presets[i].first &&
                                      current_grid_h == grid_size_presets[i].second);
        }
    }

    // --- 1. Draw Header Bar ---
    SDL_SetRenderDrawColor(renderer, UITheme::HEADER_BG.r, UITheme::HEADER_BG.g, UITheme::HEADER_BG.b, 255);
    SDL_RenderFillRect(renderer, &header_rect);

    SDL_SetRenderDrawColor(renderer, UITheme::PANEL_BORDER.r, UITheme::PANEL_BORDER.g, UITheme::PANEL_BORDER.b, 255);
    SDL_RenderDrawLine(renderer, header_rect.x, header_rect.h - 1, header_rect.w, header_rect.h - 1);

    // Header Title
    font.draw_text("CONWAY'S GAME OF LIFE", 14, 11, UITheme::TEXT_TITLE, 2);

    // Engine Badge
    int badge_x = 340;
    SDL_Rect badge_rect = { badge_x, 7, 240, 22 };
    SDL_SetRenderDrawColor(renderer, 32, 40, 48, 255);
    SDL_RenderFillRect(renderer, &badge_rect);
    SDL_SetRenderDrawColor(renderer, cuda_capable ? UITheme::ACCENT_GREEN.r : UITheme::ACCENT_BLUE.r,
                                     cuda_capable ? UITheme::ACCENT_GREEN.g : UITheme::ACCENT_BLUE.g,
                                     cuda_capable ? UITheme::ACCENT_GREEN.b : UITheme::ACCENT_BLUE.b, 255);
    SDL_RenderDrawRect(renderer, &badge_rect);
    font.draw_text_centered(engine_name, badge_x + 120, 14, UITheme::TEXT_TITLE, 1);

    // Header Metrics (Gen, Pop, FPS, Grid)
    long long total_cells = (long long)grid_w * grid_h;
    std::ostringstream ss_metrics;
    ss_metrics << "GEN: " << gen
               << " | POP: " << population
               << " | FPS: " << std::fixed << std::setprecision(1) << fps
               << " | GRID: " << grid_w << "x" << grid_h;
    if (total_cells >= 1000000) {
        ss_metrics << " (" << std::fixed << std::setprecision(1) << ((double)total_cells / 1000000.0) << "M)";
    }
    font.draw_text(ss_metrics.str(), badge_x + 260, 14, UITheme::ACCENT_AMBER, 1);

    // --- 2. Draw Sidebar Panel ---
    SDL_SetRenderDrawColor(renderer, UITheme::PANEL_BG.r, UITheme::PANEL_BG.g, UITheme::PANEL_BG.b, 255);
    SDL_RenderFillRect(renderer, &sidebar_rect);

    SDL_SetRenderDrawColor(renderer, UITheme::PANEL_BORDER.r, UITheme::PANEL_BORDER.g, UITheme::PANEL_BORDER.b, 255);
    SDL_RenderDrawLine(renderer, sidebar_rect.x, sidebar_rect.y, sidebar_rect.x, sidebar_rect.y + sidebar_rect.h);

    // Dynamic Section Labels in Sidebar
    int label_x = sidebar_rect.x + 14;
    for (const auto& label : section_labels) {
        font.draw_text(label.title, label_x, label.y, UITheme::TEXT_MUTED, 1);
    }

    // Engine status description note
    std::string engine_desc = cuda_capable ? "CUDA Hardware Acceleration Active" : "Multi-Threaded CPU Engine (OpenMP)";
    int engine_note_y = sidebar_rect.y + 490;
    if (idx_engine >= 0 && idx_engine < (int)buttons.size()) {
        engine_note_y = buttons[idx_engine].rect.y + buttons[idx_engine].rect.h + 4;
    }
    font.draw_text(engine_desc, label_x, engine_note_y, UITheme::TEXT_MUTED, 1);

    // Keyboard Shortcuts Box at bottom of sidebar (if space permits)
    int help_y = sidebar_rect.y + sidebar_rect.h - 96;
    if (help_y > 540) {
        SDL_Rect help_rect = { label_x, help_y, sidebar_rect.w - 28, 88 };
        SDL_SetRenderDrawColor(renderer, 20, 20, 26, 255);
        SDL_RenderFillRect(renderer, &help_rect);
        SDL_SetRenderDrawColor(renderer, UITheme::PANEL_BORDER.r, UITheme::PANEL_BORDER.g, UITheme::PANEL_BORDER.b, 255);
        SDL_RenderDrawRect(renderer, &help_rect);

        font.draw_text("QUICK SHORTCUTS:", label_x + 8, help_y + 8, UITheme::ACCENT_AMBER, 1);
        font.draw_text("[Space] Play/Pause | [S] Step", label_x + 8, help_y + 22, UITheme::TEXT_NORMAL, 1);
        font.draw_text("[R] Randomize | [C] Clear Grid", label_x + 8, help_y + 36, UITheme::TEXT_NORMAL, 1);
        font.draw_text("[1-5] Quick Grid Presets", label_x + 8, help_y + 50, UITheme::ACCENT_GREEN, 1);
        font.draw_text("Right-Click: Erase | Wheel: Zoom", label_x + 8, help_y + 64, UITheme::TEXT_MUTED, 1);
        font.draw_text("Middle-Click & Drag: Pan View", label_x + 8, help_y + 76, UITheme::TEXT_MUTED, 1);
    }

    // Draw all sidebar buttons
    for (auto& btn : buttons) {
        btn.draw(renderer, font);
    }

    // --- 3. Draw Pattern Stamp Preview on Canvas ---
    if (mouse_in_canvas && current_tool == ToolMode::STAMP && selected_pattern_idx >= 0 && selected_pattern_idx < (int)patterns.size()) {
        const auto& pat = patterns[selected_pattern_idx];
        SDL_SetRenderDrawColor(renderer, 0, 255, 180, 180);
        float cell_pixel_size = (float)canvas_rect.w / (float)grid_w * zoom;
        for (const auto& pt : pat.points) {
            int px = canvas_rect.x + (int)(pan_x + (float)(mouse_grid_x + pt.dx) * cell_pixel_size);
            int py = canvas_rect.y + (int)(pan_y + (float)(mouse_grid_y + pt.dy) * cell_pixel_size);
            SDL_Rect r = { px, py, std::max(1, (int)cell_pixel_size), std::max(1, (int)cell_pixel_size) };
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // --- 4. Draw Status Bar (Bottom) ---
    SDL_SetRenderDrawColor(renderer, UITheme::STATUS_BG.r, UITheme::STATUS_BG.g, UITheme::STATUS_BG.b, 255);
    SDL_RenderFillRect(renderer, &status_rect);

    SDL_SetRenderDrawColor(renderer, UITheme::PANEL_BORDER.r, UITheme::PANEL_BORDER.g, UITheme::PANEL_BORDER.b, 255);
    SDL_RenderDrawLine(renderer, status_rect.x, status_rect.y, status_rect.w, status_rect.y);

    font.draw_text(status_msg, 14, status_rect.y + 10, UITheme::TEXT_NORMAL, 1);

    // Coordinate & Cell indicator on right side of status bar
    std::ostringstream ss_status_right;
    ss_status_right << "COORD: (" << mouse_grid_x << ", " << mouse_grid_y << ")"
                    << " | CELL: " << (mouse_cell_alive ? "ALIVE" : "DEAD")
                    << " | ZOOM: " << (int)(zoom * 100.0f) << "%";
    int right_text_w = font.get_text_width(ss_status_right.str(), 1);
    font.draw_text(ss_status_right.str(), window_width - right_text_w - 14, status_rect.y + 10, UITheme::TEXT_MUTED, 1);
}
