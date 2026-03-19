inline double sdl_now_seconds_precise() {
    const Uint64 freq = SDL_GetPerformanceFrequency();
    if (freq == 0u) {
        return 0.0;
    }
    return static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(freq);
}

template <typename BuildUiFn>
int run_with_sdl2(BuildUiFn&& build_ui, const AppOptions& options = {}) {
#ifdef _WIN32
    // Use SDL's DPI handling so SDL2 stays crisp while keeping the requested
    // window size in logical DPI-scaled points on Windows.
#ifdef SDL_HINT_WINDOWS_DPI_AWARENESS
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return EXIT_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#ifdef EUI_OPENGL_ES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);

    const Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    SDL_Window* window = SDL_CreateWindow(options.title,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          options.width,
                                          options.height,
                                          window_flags);
    if (window == nullptr) {
        std::cerr << "Failed to create SDL2 window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        std::cerr << "Failed to create SDL2 OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (SDL_GL_MakeCurrent(window, gl_context) != 0) {
        std::cerr << "Failed to make SDL2 OpenGL context current: " << SDL_GetError() << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GL_SetSwapInterval(options.vsync ? 1 : 0);
    SDL_StartTextInput();

    detail::RuntimeState runtime{};
    detail::modern_gl_set_proc_loader(+[](const char* name) -> void* {
        return SDL_GL_GetProcAddress(name);
    });
    detail::Renderer renderer(options);
    Context ui;
#if defined(_WIN32) && !defined(EUI_OPENGL_ES)
    const AppOptions::TextBackend resolved_text_backend = detail::Renderer::resolve_text_backend(options);
    TextMeasureBackend text_measure_backend = TextMeasureBackend::Approx;
    std::string text_measure_font_file{};
    std::string text_measure_icon_font_file{};
    if (resolved_text_backend == AppOptions::TextBackend::Win32) {
        text_measure_backend = TextMeasureBackend::Win32;
    } else if (resolved_text_backend == AppOptions::TextBackend::Stb) {
        text_measure_backend = TextMeasureBackend::Stb;
        text_measure_font_file = detail::StbFontRenderer::resolve_font_path_for_measure(
            options.text_font_file, options.text_font_family, false);
        text_measure_icon_font_file = detail::StbFontRenderer::resolve_font_path_for_measure(
            options.icon_font_file, options.icon_font_family, true);
    }
    ui.configure_text_measure(text_measure_backend,
                              options.text_font_family != nullptr ? options.text_font_family : "Segoe UI",
                              options.text_font_weight, text_measure_font_file,
                              options.icon_font_family != nullptr ? options.icon_font_family
                                                                  : "Font Awesome 7 Free Solid",
                              text_measure_icon_font_file, options.enable_icon_font_fallback);
#else
#if EUI_ENABLE_STB_TRUETYPE
    ui.configure_text_measure(TextMeasureBackend::Stb,
                              options.text_font_family != nullptr ? options.text_font_family : "Segoe UI",
                              options.text_font_weight,
                              detail::StbFontRenderer::resolve_font_path_for_measure(
                                  options.text_font_file, options.text_font_family, false),
                              options.icon_font_family != nullptr ? options.icon_font_family
                                                                  : "Font Awesome 7 Free Solid",
                              detail::StbFontRenderer::resolve_font_path_for_measure(
                                  options.icon_font_file, options.icon_font_family, true),
                              options.enable_icon_font_fallback);
#else
    ui.configure_text_measure(TextMeasureBackend::Approx,
                              options.text_font_family != nullptr ? options.text_font_family : "Segoe UI",
                              options.text_font_weight, {},
                              options.icon_font_family != nullptr ? options.icon_font_family
                                                                  : "Font Awesome 7 Free Solid",
                              {}, options.enable_icon_font_fallback);
#endif
#endif

    double previous_time = sdl_now_seconds_precise();
    double next_frame_deadline = previous_time;
    std::uint64_t frame_index = 0u;
    bool redraw_needed = true;

    while (!runtime.quit_requested) {
        if (options.continuous_render || redraw_needed) {
            if (options.max_fps > 0.0) {
                const double now_wait = sdl_now_seconds_precise();
                const double wait_s = next_frame_deadline - now_wait;
                if (wait_s > 0.0005) {
                    detail::pump_sdl_events(true, wait_s, window, runtime);
                } else {
                    detail::pump_sdl_events(false, 0.0, window, runtime);
                }
            } else {
                detail::pump_sdl_events(false, 0.0, window, runtime);
            }
        } else {
            detail::pump_sdl_events(true, std::max(0.001, options.idle_wait_seconds), window, runtime);
        }

        if (runtime.quit_requested) {
            break;
        }

        int framebuffer_w = 1;
        int framebuffer_h = 1;
        SDL_GL_GetDrawableSize(window, &framebuffer_w, &framebuffer_h);

        int window_w = 1;
        int window_h = 1;
        SDL_GetWindowSize(window, &window_w, &window_h);

        int mouse_window_x = 0;
        int mouse_window_y = 0;
        const Uint32 mouse_mask = SDL_GetMouseState(&mouse_window_x, &mouse_window_y);
        const double mouse_x = static_cast<double>(mouse_window_x);
        const double mouse_y = static_cast<double>(mouse_window_y);

        const float framebuffer_scale_x =
            window_w > 0 ? static_cast<float>(framebuffer_w) / static_cast<float>(window_w) : 1.0f;
        const float framebuffer_scale_y =
            window_h > 0 ? static_cast<float>(framebuffer_h) / static_cast<float>(window_h) : 1.0f;

        const float window_dpi_scale = detail::sdl_window_dpi_scale(window);
        const float dpi_scale_x = std::max(framebuffer_scale_x, std::max(0.5f, window_dpi_scale));
        const float dpi_scale_y = std::max(framebuffer_scale_y, std::max(0.5f, window_dpi_scale));

        const bool left_mouse_down = (mouse_mask & SDL_BUTTON_LMASK) != 0u;
        const bool right_mouse_down = (mouse_mask & SDL_BUTTON_RMASK) != 0u;

        const Uint8* key_state = SDL_GetKeyboardState(nullptr);
        const bool a_down = key_state != nullptr && key_state[SDL_SCANCODE_A] != 0u;
        const bool c_down = key_state != nullptr && key_state[SDL_SCANCODE_C] != 0u;
        const bool v_down = key_state != nullptr && key_state[SDL_SCANCODE_V] != 0u;
        const bool x_down = key_state != nullptr && key_state[SDL_SCANCODE_X] != 0u;
        const bool left_shift_down = key_state != nullptr && key_state[SDL_SCANCODE_LSHIFT] != 0u;
        const bool right_shift_down = key_state != nullptr && key_state[SDL_SCANCODE_RSHIFT] != 0u;
        const bool ctrl_down = detail::sdl_ctrl_down();

        InputState input{};
        input.mouse_x = static_cast<float>(mouse_x) * framebuffer_scale_x;
        input.mouse_y = static_cast<float>(mouse_y) * framebuffer_scale_y;
        input.mouse_wheel_y = static_cast<float>(runtime.scroll_y_accum);
        input.mouse_down = left_mouse_down;
        input.mouse_pressed = left_mouse_down && !runtime.prev_left_mouse;
        input.mouse_released = !left_mouse_down && runtime.prev_left_mouse;
        input.mouse_right_down = right_mouse_down;
        input.mouse_right_pressed = right_mouse_down && !runtime.prev_right_mouse;
        input.mouse_right_released = !right_mouse_down && runtime.prev_right_mouse;
        input.key_backspace = runtime.pending_backspace;
        input.key_delete = runtime.pending_delete;
        input.key_enter = runtime.pending_enter;
        input.key_escape = runtime.pending_escape;
        input.key_left = runtime.pending_left;
        input.key_right = runtime.pending_right;
        input.key_up = runtime.pending_up;
        input.key_down = runtime.pending_down;
        input.key_home = runtime.pending_home;
        input.key_end = runtime.pending_end;
        input.key_shift = left_shift_down || right_shift_down || detail::sdl_shift_down();
        input.key_select_all = ctrl_down && a_down && !runtime.prev_a_key;
        input.key_copy = ctrl_down && c_down && !runtime.prev_c_key;
        input.key_cut = ctrl_down && x_down && !runtime.prev_x_key;
        input.key_paste = ctrl_down && v_down && !runtime.prev_v_key;
        input.text_input = runtime.text_input;
        if (input.key_paste) {
            char* clipboard = SDL_GetClipboardText();
            if (clipboard != nullptr) {
                input.clipboard_text = clipboard;
                SDL_free(clipboard);
            } else {
                input.key_paste = false;
            }
        }

        const bool mouse_moved = !runtime.has_prev_mouse ||
                                 std::fabs(mouse_x - runtime.prev_mouse_x) > 0.5 ||
                                 std::fabs(mouse_y - runtime.prev_mouse_y) > 0.5;
        const bool framebuffer_changed =
            framebuffer_w != runtime.prev_framebuffer_w || framebuffer_h != runtime.prev_framebuffer_h;

        runtime.text_input.clear();
        runtime.scroll_y_accum = 0.0;
        runtime.pending_backspace = false;
        runtime.pending_delete = false;
        runtime.pending_enter = false;
        runtime.pending_escape = false;
        runtime.pending_left = false;
        runtime.pending_right = false;
        runtime.pending_up = false;
        runtime.pending_down = false;
        runtime.pending_home = false;
        runtime.pending_end = false;
        runtime.prev_left_mouse = left_mouse_down;
        runtime.prev_right_mouse = right_mouse_down;
        runtime.prev_a_key = a_down;
        runtime.prev_c_key = c_down;
        runtime.prev_v_key = v_down;
        runtime.prev_x_key = x_down;
        runtime.prev_mouse_x = mouse_x;
        runtime.prev_mouse_y = mouse_y;
        runtime.has_prev_mouse = true;
        runtime.prev_framebuffer_w = framebuffer_w;
        runtime.prev_framebuffer_h = framebuffer_h;

        const double now = sdl_now_seconds_precise();
        const float dt = static_cast<float>(now - previous_time);
        previous_time = now;
        input.time_seconds = now;

        const bool input_event =
            input.mouse_pressed || input.mouse_released || input.mouse_right_pressed ||
            input.mouse_right_released || input.key_backspace || input.key_delete || input.key_enter ||
            input.key_escape || input.key_left || input.key_right || input.key_up || input.key_down ||
            input.key_home || input.key_end ||
            input.key_select_all || input.key_copy || input.key_cut || input.key_paste ||
            std::fabs(input.mouse_wheel_y) > 1e-6f ||
            !input.text_input.empty();
        const bool render_this_frame =
            options.continuous_render || redraw_needed || framebuffer_changed || mouse_moved || input_event;
        if (!render_this_frame) {
            continue;
        }

        ui.begin_frame(static_cast<float>(framebuffer_w), static_cast<float>(framebuffer_h), input);
        bool request_next_frame = false;
        const float dpi_scale = std::max(1.0f, std::min(dpi_scale_x, dpi_scale_y));
        FrameContext frame_ctx{
            &ui,
            eui::runtime::NativeWindowHandle{eui::runtime::NativeWindowKind::SDL2, static_cast<void*>(window)},
            eui::runtime::FrameClock{frame_index, now, static_cast<double>(dt)},
            eui::runtime::WindowMetrics{
                window_w,
                window_h,
                framebuffer_w,
                framebuffer_h,
                dpi_scale_x,
                dpi_scale_y,
                dpi_scale,
            },
            &request_next_frame,
        };
        build_ui(frame_ctx);
        ++frame_index;
        if (ui.consume_repaint_request()) {
            request_next_frame = true;
        }
        ui.take_frame(runtime.curr_commands, runtime.curr_text_arena, runtime.curr_brush_payloads,
                      runtime.curr_transform_payloads);
        std::string clipboard_write_text;
        if (ui.consume_clipboard_write(clipboard_write_text)) {
            SDL_SetClipboardText(clipboard_write_text.c_str());
        }
        redraw_needed = request_next_frame;

        const Color bg = ui.theme().background;
        const auto& commands = runtime.curr_commands;
        const auto& text_arena = runtime.curr_text_arena;
        const auto& brush_payloads = runtime.curr_brush_payloads;
        const auto& transform_payloads = runtime.curr_transform_payloads;
        const std::uint64_t frame_hash = detail::hash_frame_payload(commands, bg);

        const bool hard_full_redraw = framebuffer_changed || !runtime.has_prev_frame;
        const bool cache_missing = !runtime.has_cache;
        const bool force_full_redraw = hard_full_redraw || cache_missing;
        bool has_dirty = false;
        runtime.dirty_regions.clear();
        if (!force_full_redraw && runtime.has_prev_frame && runtime.prev_frame_hash == frame_hash) {
            has_dirty = false;
        } else {
            has_dirty = detail::compute_dirty_regions(commands, runtime, bg, framebuffer_w, framebuffer_h,
                                                      hard_full_redraw, runtime.dirty_regions);
        }
        bool prefer_full_redraw = false;
        if (!hard_full_redraw && has_dirty) {
            float dirty_area = 0.0f;
            for (const Rect& dirty : runtime.dirty_regions) {
                dirty_area += std::max(0.0f, dirty.w) * std::max(0.0f, dirty.h);
            }
            const float framebuffer_area =
                std::max(1.0f, static_cast<float>(framebuffer_w) * static_cast<float>(framebuffer_h));
            prefer_full_redraw =
                runtime.dirty_regions.size() >= 6u || dirty_area >= framebuffer_area * 0.58f;
        }
        const bool use_full_redraw = force_full_redraw || prefer_full_redraw;

        if (!use_full_redraw && !has_dirty) {
            detail::update_prev_command_cache(runtime, commands);
            detail::trim_dirty_region_cache(runtime);
            detail::update_cache_lifecycle(runtime);
            runtime.prev_bg = bg;
            runtime.prev_frame_hash = frame_hash;
            runtime.has_prev_frame = true;
            if (options.max_fps > 0.0) {
                const double frame_interval = 1.0 / options.max_fps;
                next_frame_deadline = sdl_now_seconds_precise() + frame_interval;
            }
            continue;
        }

        if (use_full_redraw) {
            glDisable(GL_SCISSOR_TEST);
            glClearColor(bg.r, bg.g, bg.b, bg.a);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.render(commands, text_arena, brush_payloads, transform_payloads, framebuffer_w, framebuffer_h,
                            nullptr, frame_hash);
            if (!prefer_full_redraw || hard_full_redraw) {
                detail::ensure_cache_texture(runtime, framebuffer_w, framebuffer_h);
                detail::copy_full_to_cache(runtime, framebuffer_w, framebuffer_h);
            } else {
                runtime.has_cache = false;
            }
        } else {
            detail::draw_cache_texture(runtime, framebuffer_w, framebuffer_h);
            for (const Rect& dirty : runtime.dirty_regions) {
                const detail::IRect gl_dirty = detail::to_gl_rect(dirty, framebuffer_w, framebuffer_h);
                if (gl_dirty.w > 0 && gl_dirty.h > 0) {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(gl_dirty.x, gl_dirty.y, gl_dirty.w, gl_dirty.h);
                    glClearColor(bg.r, bg.g, bg.b, bg.a);
                    glClear(GL_COLOR_BUFFER_BIT);
                    renderer.render(commands, text_arena, brush_payloads, transform_payloads, framebuffer_w,
                                    framebuffer_h, &dirty, frame_hash);
                    detail::copy_region_to_cache(runtime, gl_dirty);
                }
            }
            glDisable(GL_SCISSOR_TEST);
        }

        detail::update_prev_command_cache(runtime, commands);
        detail::trim_dirty_region_cache(runtime);
        detail::update_cache_lifecycle(runtime);
        runtime.prev_bg = bg;
        runtime.prev_frame_hash = frame_hash;
        runtime.has_prev_frame = true;
        SDL_GL_SwapWindow(window);

        if (options.max_fps > 0.0) {
            const double frame_interval = 1.0 / options.max_fps;
            next_frame_deadline = sdl_now_seconds_precise() + frame_interval;
        }
    }

    SDL_StopTextInput();
    renderer.release_gl_resources();
    detail::release_cache_texture(runtime);
    detail::modern_gl_set_proc_loader(nullptr);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
