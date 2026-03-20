#ifndef EUI_H_
#define EUI_H_

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <GLFW/glfw3.h>
#ifdef _WIN32
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#endif

#include <array>
#include <iostream>
#include <limits>
#include <memory>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_ALPHA
#define GL_ALPHA 0x1906
#endif

#ifndef EUI_ENABLE_STB_TRUETYPE
#if defined(__has_include)
#if __has_include("stb_truetype.h")
#define EUI_ENABLE_STB_TRUETYPE 1
#else
#define EUI_ENABLE_STB_TRUETYPE 0
#endif
#else
#define EUI_ENABLE_STB_TRUETYPE 0
#endif
#endif

#if EUI_ENABLE_STB_TRUETYPE
#ifndef EUI_STB_TRUETYPE_INCLUDED
#define EUI_STB_TRUETYPE_INCLUDED
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#ifndef STBTT_STATIC
#define STBTT_STATIC
#define EUI_UNDEF_STBTT_STATIC
#endif
#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define EUI_UNDEF_STBTT_IMPLEMENTATION
#endif
#include "stb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#ifdef EUI_UNDEF_STBTT_IMPLEMENTATION
#undef STB_TRUETYPE_IMPLEMENTATION
#undef EUI_UNDEF_STBTT_IMPLEMENTATION
#endif
#ifdef EUI_UNDEF_STBTT_STATIC
#undef STBTT_STATIC
#undef EUI_UNDEF_STBTT_STATIC
#endif
#endif
#endif
#endif

namespace eui {

struct Color {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};
};

inline Color rgba(float r, float g, float b, float a = 1.0f) {
    return Color{r, g, b, a};
}

inline Color mix(const Color& lhs, const Color& rhs, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{
        lhs.r + (rhs.r - lhs.r) * t,
        lhs.g + (rhs.g - lhs.g) * t,
        lhs.b + (rhs.b - lhs.b) * t,
        lhs.a + (rhs.a - lhs.a) * t,
    };
}

inline constexpr float k_icon_visual_scale = 0.86f;

inline float srgb_to_linear(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    if (value <= 0.04045f) {
        return value / 12.92f;
    }
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

inline float color_luminance(const Color& color) {
    const float r = srgb_to_linear(color.r);
    const float g = srgb_to_linear(color.g);
    const float b = srgb_to_linear(color.b);
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

inline Color brighten_primary_for_dark_mode(const Color& primary) {
    Color tuned{
        std::clamp(primary.r, 0.0f, 1.0f),
        std::clamp(primary.g, 0.0f, 1.0f),
        std::clamp(primary.b, 0.0f, 1.0f),
        std::clamp(primary.a, 0.0f, 1.0f),
    };
    const float luminance = color_luminance(tuned);
    const float target_luminance = 0.24f;
    if (luminance >= target_luminance) {
        return tuned;
    }

    const float denom = std::max(1e-6f, 1.0f - luminance);
    const float lift = std::clamp((target_luminance - luminance) / denom, 0.0f, 0.72f);
    const Color white = rgba(1.0f, 1.0f, 1.0f, tuned.a);
    tuned = mix(tuned, white, lift);
    // Keep some original chroma to avoid turning into gray.
    tuned = mix(tuned, primary, 0.14f);
    tuned.r = std::clamp(tuned.r, 0.0f, 1.0f);
    tuned.g = std::clamp(tuned.g, 0.0f, 1.0f);
    tuned.b = std::clamp(tuned.b, 0.0f, 1.0f);
    tuned.a = std::clamp(primary.a, 0.0f, 1.0f);
    return tuned;
}

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};

    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

enum class ThemeMode {
    Light,
    Dark,
};

enum class ButtonStyle {
    Primary,
    Secondary,
    Ghost,
};

enum class TextMeasureBackend {
    Approx,
    Win32,
    Stb,
};

struct Theme {
    Color background{};
    Color panel{};
    Color panel_border{};
    Color text{};
    Color muted_text{};
    Color primary{};
    Color primary_text{};
    Color secondary{};
    Color secondary_hover{};
    Color secondary_active{};
    Color track{};
    Color track_fill{};
    Color outline{};
    Color input_bg{};
    Color input_border{};
    Color focus_ring{};
    float radius{8.0f};
};

inline Theme make_theme(ThemeMode mode, const Color& primary) {
    Theme theme{};
    theme.primary = (mode == ThemeMode::Dark) ? brighten_primary_for_dark_mode(primary) : primary;
    theme.radius = 8.0f;

    if (mode == ThemeMode::Dark) {
        theme.background = rgba(0.05f, 0.07f, 0.10f, 1.0f);
        theme.panel = rgba(0.09f, 0.12f, 0.16f, 1.0f);
        theme.panel_border = rgba(0.18f, 0.23f, 0.30f, 1.0f);
        theme.text = rgba(0.92f, 0.95f, 0.98f, 1.0f);
        theme.muted_text = rgba(0.63f, 0.70f, 0.79f, 1.0f);
        theme.primary_text = rgba(0.06f, 0.10f, 0.17f, 1.0f);
        theme.secondary = rgba(0.15f, 0.20f, 0.27f, 1.0f);
        theme.secondary_hover = mix(theme.secondary, theme.primary, 0.18f);
        theme.secondary_active = mix(theme.secondary, theme.primary, 0.32f);
        theme.track = rgba(0.18f, 0.23f, 0.31f, 1.0f);
        theme.track_fill = theme.primary;
        theme.outline = rgba(0.25f, 0.31f, 0.40f, 1.0f);
        theme.input_bg = rgba(0.08f, 0.11f, 0.15f, 1.0f);
        theme.input_border = mix(rgba(0.26f, 0.33f, 0.42f, 1.0f), theme.primary, 0.20f);
        theme.focus_ring = mix(theme.primary, rgba(1.0f, 1.0f, 1.0f, 1.0f), 0.18f);
    } else {
        theme.background = rgba(0.96f, 0.97f, 0.99f, 1.0f);
        theme.panel = rgba(1.0f, 1.0f, 1.0f, 1.0f);
        theme.panel_border = rgba(0.84f, 0.88f, 0.93f, 1.0f);
        theme.text = rgba(0.11f, 0.15f, 0.22f, 1.0f);
        theme.muted_text = rgba(0.41f, 0.47f, 0.58f, 1.0f);
        theme.primary_text = rgba(0.96f, 0.98f, 1.0f, 1.0f);
        theme.secondary = rgba(0.92f, 0.94f, 0.97f, 1.0f);
        theme.secondary_hover = mix(theme.secondary, theme.primary, 0.12f);
        theme.secondary_active = mix(theme.secondary, theme.primary, 0.24f);
        theme.track = rgba(0.90f, 0.92f, 0.96f, 1.0f);
        theme.track_fill = theme.primary;
        theme.outline = rgba(0.80f, 0.85f, 0.92f, 1.0f);
        theme.input_bg = rgba(1.0f, 1.0f, 1.0f, 1.0f);
        theme.input_border = mix(rgba(0.79f, 0.84f, 0.91f, 1.0f), theme.primary, 0.28f);
        theme.focus_ring = mix(theme.primary, rgba(1.0f, 1.0f, 1.0f, 1.0f), 0.10f);
    }
    return theme;
}

struct InputState {
    float mouse_x{0.0f};
    float mouse_y{0.0f};
    float mouse_wheel_y{0.0f};
    bool mouse_down{false};
    bool mouse_pressed{false};
    bool mouse_released{false};

    bool mouse_right_down{false};
    bool mouse_right_pressed{false};
    bool mouse_right_released{false};

    bool key_backspace{false};
    bool key_delete{false};
    bool key_enter{false};
    bool key_escape{false};
    bool key_left{false};
    bool key_right{false};
    bool key_up{false};
    bool key_down{false};
    bool key_home{false};
    bool key_end{false};
    bool key_select_all{false};
    bool key_copy{false};
    bool key_cut{false};
    bool key_paste{false};
    bool key_shift{false};
    std::string text_input{};
    std::string clipboard_text{};
    double time_seconds{0.0};
};

enum class CommandType {
    FilledRect,
    RectOutline,
    Text,
    Chevron,
};

enum class TextAlign {
    Left,
    Center,
    Right,
};

struct DrawCommand {
    CommandType type{CommandType::FilledRect};
    Rect rect{};
    Rect clip_rect{};
    Color color{};
    std::uint32_t text_offset{0};
    std::uint32_t text_length{0};
    float font_size{13.0f};
    TextAlign align{TextAlign::Left};
    float radius{0.0f};
    float thickness{1.0f};
    float rotation{0.0f};
    bool has_clip{false};
    std::uint64_t hash{0ull};
};

class Context {
#if EUI_ENABLE_STB_TRUETYPE
    struct StbMeasureFont;
#endif
    struct TextAreaState {
        float scroll{0.0f};
        float preferred_x{-1.0f};
    };

    struct ScrollAreaState {
        float scroll{0.0f};
        float velocity{0.0f};
        float content_height{0.0f};
    };

    enum class ScrollInputKind {
        None,
        ScrollArea,
        TextArea,
    };

    struct ScrollInputTarget {
        ScrollInputKind kind{ScrollInputKind::None};
        std::uint64_t id{0u};
        int depth{-1};
        bool on_thumb{false};
    };

    struct ScrollInputRegion {
        ScrollInputKind kind{ScrollInputKind::None};
        std::uint64_t id{0u};
        Rect body{};
        Rect thumb{};
        bool has_thumb{false};
        bool can_scroll{false};
        int depth{-1};
    };

public:
    Context() {
        commands_.reserve(1024);
        scope_stack_.reserve(24);
        text_arena_.reserve(16 * 1024);
        input_buffer_.reserve(64);
        theme_.radius = corner_radius_;
    }

    ~Context() {
        release_text_measure_cache();
        release_stb_measure_cache();
    }

    void set_theme_mode(ThemeMode mode) {
        if (theme_mode_ == mode) {
            return;
        }
        theme_mode_ = mode;
        refresh_theme();
    }

    ThemeMode theme_mode() const {
        return theme_mode_;
    }

    void set_primary_color(const Color& color) {
        primary_color_ = color;
        refresh_theme();
    }

    void set_corner_radius(float radius) {
        corner_radius_ = std::clamp(radius, 0.0f, 28.0f);
        theme_.radius = corner_radius_;
    }

    void set_global_alpha(float alpha) {
        global_alpha_ = std::clamp(alpha, 0.0f, 1.0f);
    }

    float corner_radius() const {
        return corner_radius_;
    }

    const Theme& theme() const {
        return theme_;
    }

    void configure_text_measure(TextMeasureBackend backend, std::string_view family, int weight,
                                std::string_view font_file = {}, std::string_view icon_family = {},
                                std::string_view icon_file = {}, bool enable_icon_fallback = true) {
        const std::string resolved_family =
            family.empty() ? std::string("Segoe UI") : std::string(family);
        const int resolved_weight = std::clamp(weight, 100, 900);
        const std::string resolved_file = std::string(font_file);
        const std::string resolved_icon_family =
            icon_family.empty() ? std::string("Font Awesome 7 Free Solid") : std::string(icon_family);
        const std::string resolved_icon_file = std::string(icon_file);

        const bool backend_changed = text_measure_backend_ != backend;
        const bool family_changed = text_measure_font_family_ != resolved_family;
        const bool weight_changed = text_measure_font_weight_ != resolved_weight;
        const bool file_changed = text_measure_font_file_ != resolved_file;
        const bool icon_family_changed = text_measure_icon_font_family_ != resolved_icon_family;
        const bool icon_file_changed = text_measure_icon_font_file_ != resolved_icon_file;
        const bool icon_fallback_changed = text_measure_enable_icon_fallback_ != enable_icon_fallback;

        text_measure_backend_ = backend;
        text_measure_font_family_ = resolved_family;
        text_measure_font_weight_ = resolved_weight;
        text_measure_font_file_ = resolved_file;
        text_measure_icon_font_family_ = resolved_icon_family;
        text_measure_icon_font_file_ = resolved_icon_file;
        text_measure_enable_icon_fallback_ = enable_icon_fallback;

        if (backend_changed || family_changed || weight_changed || icon_family_changed || icon_fallback_changed) {
            release_text_measure_cache();
        }
        if (backend_changed || file_changed || icon_file_changed || icon_fallback_changed) {
            release_stb_measure_cache();
        }
    }

    void begin_frame(float width, float height, const InputState& input) {
        frame_width_ = std::max(1.0f, width);
        frame_height_ = std::max(1.0f, height);
        input_ = input;
        repaint_requested_ = false;
        global_alpha_ = 1.0f;
        motion_frame_id_ += 1u;
        if ((motion_frame_id_ & 63u) == 0u) {
            prune_motion_states();
        }
        if (has_prev_input_time_) {
            const double dt = input_.time_seconds - prev_input_time_;
            ui_dt_ = std::clamp(static_cast<float>(dt), 1.0f / 240.0f, 0.10f);
        } else {
            ui_dt_ = 1.0f / 60.0f;
            has_prev_input_time_ = true;
        }
        prev_input_time_ = input_.time_seconds;
        commands_.clear();
        text_arena_.clear();
        flush_row();
        scope_stack_.clear();
        clip_stack_.clear();
        waterfall_ = WaterfallState{};
        prev_scroll_input_regions_.swap(scroll_input_regions_);
        scroll_input_regions_.clear();
        if (std::fabs(input_.mouse_wheel_y) > 1e-6f) {
            wheel_target_ =
                resolve_scroll_input_target(prev_scroll_input_regions_, input_.mouse_x, input_.mouse_y, true);
        } else {
            wheel_target_ = ScrollInputTarget{};
        }
        if (input_.mouse_pressed) {
            press_target_ =
                resolve_scroll_input_target(prev_scroll_input_regions_, input_.mouse_x, input_.mouse_y, false);
        } else {
            press_target_ = ScrollInputTarget{};
        }

        content_x_ = 16.0f;
        content_width_ = frame_width_ - 32.0f;
        cursor_y_ = 16.0f;
        panel_id_seed_ = 1469598103934665603ull;
    }

    const std::vector<DrawCommand>& end_frame() {
        finalize_frame_state();
        return commands_;
    }

    void take_frame(std::vector<DrawCommand>& out_commands, std::vector<char>& out_text_arena) {
        finalize_frame_state();
        out_commands.clear();
        out_text_arena.clear();
        out_commands.swap(commands_);
        out_text_arena.swap(text_arena_);
        const std::size_t cmd_cap = out_commands.capacity();
        const std::size_t text_cap = out_text_arena.capacity();
        commands_.clear();
        text_arena_.clear();
        if (commands_.capacity() < cmd_cap) {
            commands_.reserve(cmd_cap);
        }
        if (text_arena_.capacity() < text_cap) {
            text_arena_.reserve(text_cap);
        }
    }

    const std::vector<char>& text_arena() const {
        return text_arena_;
    }

    const std::vector<DrawCommand>& commands() const {
        return commands_;
    }

private:
    void finalize_frame_state() {
        flush_row();
        scope_stack_.clear();
        clip_stack_.clear();
        waterfall_ = WaterfallState{};
        active_slider_id_ = input_.mouse_down ? active_slider_id_ : 0;
        active_textarea_scroll_id_ = input_.mouse_down ? active_textarea_scroll_id_ : 0;
        if (!input_.mouse_down) {
            active_scroll_drag_id_ = 0;
            active_scrollbar_drag_id_ = 0;
        }
    }

    ScrollInputTarget resolve_scroll_input_target(const std::vector<ScrollInputRegion>& regions, float mouse_x,
                                                  float mouse_y, bool require_scrollable) const {
        ScrollInputTarget best{};
        for (const ScrollInputRegion& region : regions) {
            if (require_scrollable && !region.can_scroll) {
                continue;
            }
            const bool on_thumb = region.has_thumb && region.thumb.contains(mouse_x, mouse_y);
            const bool in_body = region.body.contains(mouse_x, mouse_y);
            if (!on_thumb && !in_body) {
                continue;
            }
            if (best.kind == ScrollInputKind::None || region.depth > best.depth || region.depth == best.depth) {
                best.kind = region.kind;
                best.id = region.id;
                best.depth = region.depth;
                best.on_thumb = on_thumb;
            }
        }
        return best;
    }

public:
    void begin_panel(std::string_view title, float x, float y, float width, float padding = 14.0f,
                     float radius = 10.0f) {
        flush_row();
        scope_stack_.clear();
        waterfall_ = WaterfallState{};

        const float panel_w = std::max(140.0f, width);
        const float panel_h = std::max(120.0f, frame_height_ - y - 20.0f);
        panel_rect_ = Rect{x, y, panel_w, panel_h};
        panel_id_seed_ = hash_sv(title);

        if (radius >= 0.0f) {
            add_filled_rect(panel_rect_, theme_.panel, radius);
            add_outline_rect(panel_rect_, theme_.panel_border, radius);
        }

        content_x_ = x + padding;
        content_width_ = std::max(20.0f, panel_w - 2.0f * padding);
        cursor_y_ = y + padding;
        if (!title.empty()) {
            const float title_font = std::clamp(theme_.radius * 1.4f, 14.0f, 24.0f);
            const float title_h = title_font + 2.0f;
            add_text(title, Rect{content_x_, cursor_y_, content_width_, title_h}, theme_.text, title_font,
                     TextAlign::Left);
            cursor_y_ += title_h + 6.0f;
        }
    }

    void end_panel() {
        flush_row();
        scope_stack_.clear();
        waterfall_ = WaterfallState{};
    }

    void begin_card(std::string_view title, float height = 0.0f, float padding = 14.0f,
                    float radius = -1.0f) {
        padding = std::clamp(padding, 6.0f, 28.0f);
        const float min_height = std::max(42.0f, height);
        const float card_radius = (radius < 0.0f) ? std::max(0.0f, theme_.radius * 0.5f) : radius;
        bool in_waterfall = false;
        int column_index = -1;
        Rect card{};
        if (waterfall_.active && !row_.active) {
            in_waterfall = true;
            float best_y = waterfall_.column_y.empty() ? cursor_y_ : waterfall_.column_y[0];
            int best_idx = 0;
            for (int i = 1; i < waterfall_.columns; ++i) {
                const float y = waterfall_.column_y[static_cast<std::size_t>(i)];
                if (y < best_y) {
                    best_y = y;
                    best_idx = i;
                }
            }
            column_index = best_idx;
            const float x =
                content_x_ + static_cast<float>(best_idx) * (waterfall_.item_width + waterfall_.gap);
            card = Rect{x, best_y, waterfall_.item_width, min_height};
        } else {
            card = next_rect(min_height);
        }
        const std::size_t fill_cmd_index = commands_.size();
        add_filled_rect(card, theme_.panel, card_radius);
        const std::size_t outline_cmd_index = commands_.size();
        add_outline_rect(card, theme_.panel_border, card_radius);
        const bool had_outer_row = row_.active;
        const RowState outer_row = row_;

        scope_stack_.push_back(ScopeState{
            ScopeKind::Card,
            content_x_,
            content_width_,
            0.0f,
            fill_cmd_index,
            outline_cmd_index,
            card.y,
            min_height,
            padding,
            had_outer_row,
            outer_row,
            in_waterfall,
            column_index,
        });

        content_x_ = card.x + padding;
        content_width_ = std::max(10.0f, card.w - 2.0f * padding);
        cursor_y_ = card.y + padding;
        row_ = RowState{};

        if (!title.empty()) {
            const float title_font = std::clamp(theme_.radius * 1.4f, 14.0f, 24.0f);
            const float title_h = title_font + 2.0f;
            add_text(title, Rect{content_x_, cursor_y_, content_width_, title_h}, theme_.text, title_font,
                     TextAlign::Left);
            cursor_y_ += title_h + 6.0f;
        }
    }

    void end_card() {
        flush_row();
        if (scope_stack_.empty()) {
            return;
        }
        restore_scope();
    }

    void label(std::string_view text, float font_size = 13.0f, bool muted = false, float height = 0.0f) {
        const float resolved_height = std::max(font_size + 6.0f, height);
        const Rect rect = next_rect(resolved_height);
        add_text(text, Rect{rect.x, rect.y, rect.w, resolved_height}, muted ? theme_.muted_text : theme_.text,
                 font_size, TextAlign::Left);
    }

    void spacer(float height = 8.0f) {
        flush_row();
        cursor_y_ += std::max(0.0f, height);
    }

    void row_skip(int columns = 1, float min_height = 0.0f) {
        if (!row_.active) {
            return;
        }
        const int skip = std::max(0, columns);
        if (skip == 0) {
            return;
        }
        row_.max_height = std::max(row_.max_height, std::max(0.0f, min_height));
        row_.index += skip;
        if (row_.index >= row_.columns) {
            flush_row();
        }
    }

    void row_flex_spacer(int keep_trailing_columns = 1, float min_height = 0.0f) {
        if (!row_.active) {
            return;
        }
        const int keep = std::max(0, keep_trailing_columns);
        const int remaining = row_.columns - row_.index - keep;
        if (remaining > 0) {
            row_skip(remaining, min_height);
        }
    }

    void set_next_item_span(int columns) {
        if (!row_.active) {
            return;
        }
        row_.next_span = std::max(1, columns);
    }

    bool button(std::string_view label, ButtonStyle style = ButtonStyle::Secondary, float height = 34.0f,
                float text_scale = 1.0f) {
        const Rect rect = next_rect(height);
        const bool hovered = rect.contains(input_.mouse_x, input_.mouse_y);
        const bool held = hovered && input_.mouse_down;
        std::string_view draw_label = label;
        const float resolved_text_scale = std::clamp(text_scale, 0.6f, 2.0f);
        const bool force_left_align = !draw_label.empty() && draw_label.front() == '\t';
        if (force_left_align) {
            draw_label.remove_prefix(1);
        }
        bool icon_like = false;
        if (!draw_label.empty() && draw_label.size() <= 4u) {
            icon_like = true;
            for (char ch : draw_label) {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if (uch < 0x80u && std::isalnum(uch)) {
                    icon_like = false;
                    break;
                }
            }
        }
        const float text_size = icon_like
                                    ? std::clamp(rect.h * 0.72f * k_icon_visual_scale * resolved_text_scale, 13.0f,
                                                 36.0f)
                                    : std::clamp(rect.h * 0.38f * resolved_text_scale, 12.0f, 34.0f);

        // For labels like "<icon>  TEXT", draw icon/text separately to keep visual vertical alignment.
        bool icon_text_combo = false;
        std::string_view icon_part{};
        std::string_view text_part{};
        if (!draw_label.empty() && static_cast<unsigned char>(draw_label.front()) >= 0x80u) {
            std::size_t split = draw_label.find("  ");
            if (split == std::string_view::npos) {
                split = draw_label.find(' ');
            }
            if (split != std::string_view::npos) {
                std::size_t text_start = split;
                while (text_start < draw_label.size() && draw_label[text_start] == ' ') {
                    text_start += 1u;
                }
                if (split > 0u && text_start < draw_label.size()) {
                    icon_part = draw_label.substr(0u, split);
                    text_part = draw_label.substr(text_start);
                    icon_text_combo = true;
                }
            }
        }

        Color fill = theme_.secondary;
        Color text_color = theme_.text;
        if (style == ButtonStyle::Primary) {
            fill = theme_.primary;
            text_color = theme_.primary_text;
        } else if (style == ButtonStyle::Ghost) {
            fill = hovered ? mix(theme_.secondary, theme_.panel, 0.5f) : theme_.panel;
            text_color = theme_.text;
        }

        if (held) {
            fill = mix(fill, theme_.secondary_active, 0.35f);
        } else if (hovered && style != ButtonStyle::Ghost) {
            fill = mix(fill, theme_.secondary_hover, 0.30f);
        }

        MotionState& motion =
            update_motion_state(motion_id(draw_label, rect, 0x1a2b3c4d5e6f7001ull), hovered, held);
        const float hover_v = snap_visual_motion(motion.hover);
        const float press_v = snap_visual_motion(motion.press, 1.0f / 36.0f);
        const float visual_scale = 1.0f - press_v * 0.018f;
        Rect visual_rect = scale_rect_from_center(rect, visual_scale, visual_scale);
        visual_rect = translate_rect(visual_rect, 0.0f, press_v * 0.4f);
        const Color outline_color = mix(style == ButtonStyle::Primary ? theme_.primary : theme_.outline,
                                        theme_.focus_ring, hover_v * 0.26f + press_v * 0.12f);
        if (style == ButtonStyle::Primary) {
            fill = mix(fill, theme_.panel, hover_v * 0.12f);
            add_soft_glow(visual_rect, theme_.primary, theme_.radius, hover_v * 0.54f + press_v * 0.14f,
                          6.5f);
        } else {
            fill = mix(fill, theme_.panel, hover_v * 0.05f);
            add_soft_glow(visual_rect, mix(theme_.primary, theme_.secondary, 0.52f), theme_.radius,
                          hover_v * 0.12f, 5.0f);
        }

        add_filled_rect(visual_rect, fill, theme_.radius);
        add_outline_rect(visual_rect, outline_color, theme_.radius, 1.0f + hover_v * 0.14f);
        if (icon_text_combo) {
            const float pad = force_left_align ? std::clamp(visual_rect.h * 0.24f, 9.0f, 14.0f)
                                               : std::clamp(visual_rect.h * 0.24f, 8.0f, 14.0f);
            const float icon_size =
                force_left_align
                    ? std::clamp(visual_rect.h * 0.46f * k_icon_visual_scale * resolved_text_scale, 10.0f, 24.0f)
                    : std::clamp(visual_rect.h * 0.60f * k_icon_visual_scale * resolved_text_scale, 12.0f, 36.0f);
            const float icon_w = force_left_align ? std::max(icon_size + 2.0f, visual_rect.h * 0.44f)
                                                  : std::max(14.0f, visual_rect.h - pad * 0.2f);
            const Rect icon_rect{
                visual_rect.x + pad,
                visual_rect.y,
                icon_w,
                visual_rect.h,
            };
            const float gap = force_left_align ? std::max(6.0f, pad * 0.58f) : std::max(4.0f, pad * 0.45f);
            const float text_x = icon_rect.x + icon_rect.w + gap;
            const Rect text_rect{
                text_x,
                visual_rect.y,
                std::max(0.0f, visual_rect.x + visual_rect.w - text_x - pad),
                visual_rect.h,
            };
            const float combo_text_size =
                force_left_align ? std::clamp(visual_rect.h * 0.37f * resolved_text_scale, 12.0f, 30.0f)
                                 : std::clamp(visual_rect.h * 0.35f * resolved_text_scale, 11.0f, 30.0f);
            add_text(icon_part, icon_rect, text_color, icon_size, TextAlign::Center);
            add_text(text_part, text_rect, text_color, combo_text_size, TextAlign::Left);
        } else {
            if (force_left_align) {
                const float pad = std::clamp(visual_rect.h * 0.24f, 8.0f, 14.0f);
                add_text(draw_label,
                         Rect{visual_rect.x + pad, visual_rect.y,
                              std::max(0.0f, visual_rect.w - pad * 1.5f), visual_rect.h},
                         text_color, text_size, TextAlign::Left);
            } else {
                add_text(draw_label, Rect{visual_rect.x, visual_rect.y, visual_rect.w, visual_rect.h}, text_color,
                         text_size, TextAlign::Center);
            }
        }

        return hovered && input_.mouse_pressed;
    }

    bool tab(std::string_view label, bool selected, float height = 30.0f) {
        const Rect rect = next_rect(height);
        const bool hovered = rect.contains(input_.mouse_x, input_.mouse_y);
        const bool held = hovered && input_.mouse_down;
        const float text_size = std::clamp(rect.h * 0.42f, 13.0f, 26.0f);
        MotionState& motion =
            update_motion_state(motion_id(label, rect, 0x1a2b3c4d5e6f7002ull), hovered, held, false, selected);
        const float hover_v = snap_visual_motion(motion.hover);
        const float press_v = snap_visual_motion(motion.press, 1.0f / 36.0f);
        const float active_v = snap_visual_motion(motion.active, 1.0f / 40.0f);
        Color fill = mix(theme_.secondary, mix(theme_.primary, theme_.panel, 0.72f), active_v);
        fill = mix(fill, selected ? theme_.primary : theme_.secondary_hover, hover_v * (selected ? 0.16f : 0.28f));
        fill = mix(fill, theme_.secondary_active, press_v * (selected ? 0.18f : 0.32f));
        const float tab_radius = std::max(0.0f, theme_.radius - 2.0f);
        const float visual_scale = 1.0f + active_v * 0.004f - press_v * 0.014f;
        Rect visual_rect = scale_rect_from_center(rect, visual_scale, visual_scale);
        visual_rect = translate_rect(visual_rect, 0.0f, press_v * 0.3f);
        add_soft_glow(visual_rect, theme_.primary, tab_radius, active_v * 0.34f + hover_v * 0.06f, 5.0f);
        add_filled_rect(visual_rect, fill, tab_radius);
        add_outline_rect(visual_rect,
                         mix(mix(theme_.outline, theme_.panel, 0.6f), theme_.primary,
                             active_v * 0.78f + hover_v * 0.18f),
                         tab_radius, 1.0f + active_v * 0.36f + hover_v * 0.06f);
        add_text(label, Rect{visual_rect.x, visual_rect.y, visual_rect.w, visual_rect.h},
                 mix(theme_.muted_text, theme_.text, 0.38f + active_v * 0.62f), text_size, TextAlign::Center);

        return hovered && input_.mouse_pressed;
    }

    bool slider_float(std::string_view label, float& value, float min_value, float max_value, int decimals = -1,
                      float height = 40.0f) {
        if (max_value < min_value) {
            std::swap(max_value, min_value);
        }

        const int value_decimals = resolve_decimals(min_value, max_value, decimals);
        const std::uint64_t id = id_for(label) ^ 0x62e2ac4d9d45f7b1ull;
        const Rect rect = next_rect(height);
        const float radius = theme_.radius;
        const float label_font = std::clamp(rect.h * 0.36f, 13.0f, 24.0f);
        const float value_font = std::max(12.0f, label_font - 0.5f);
        const float value_padding = std::clamp(rect.h * 0.15f, 6.0f, 12.0f);
        const float value_box_w = std::clamp(rect.h * 1.8f, 64.0f, 128.0f);
        const Rect value_box{
            rect.x + rect.w - value_box_w - value_padding,
            rect.y + value_padding,
            value_box_w,
            rect.h - value_padding * 2.0f,
        };

        const bool hovered = rect.contains(input_.mouse_x, input_.mouse_y);
        const bool value_hovered = value_box.contains(input_.mouse_x, input_.mouse_y);

        if (input_.mouse_right_pressed && value_hovered) {
            start_text_input(id, format_float(value, value_decimals), true);
        }

        bool changed = false;
        if (is_text_input_active(id)) {
            update_mouse_selection(value_box, value_font, true, value_padding, value_hovered);
            consume_numeric_typing(min_value < 0.0f, true);
            clamp_live_numeric_buffer(min_value, max_value, value_decimals);
            if (input_.key_escape) {
                stop_text_input();
            } else if (input_.key_enter || (input_.mouse_pressed && !value_hovered)) {
                changed = commit_text_input(value, min_value, max_value) || changed;
            }
        }

        if (hovered && input_.mouse_pressed && !value_hovered && !is_text_input_active(id)) {
            active_slider_id_ = id;
        }

        if (active_slider_id_ == id) {
            if (input_.mouse_down) {
                float t = (input_.mouse_x - rect.x) / rect.w;
                t = std::clamp(t, 0.0f, 1.0f);
                const float new_value = min_value + (max_value - min_value) * t;
                if (std::fabs(new_value - value) > 1e-6f) {
                    value = new_value;
                    changed = true;
                }
            }
            if (!input_.mouse_down || input_.mouse_released) {
                active_slider_id_ = 0;
            }
        }

        const float range = std::max(1e-6f, max_value - min_value);
        const float t = std::clamp((value - min_value) / range, 0.0f, 1.0f);
        const float inner_x = rect.x + 1.0f;
        const float inner_y = rect.y + 1.0f;
        const float inner_w = std::max(0.0f, rect.w - 2.0f);
        const float inner_h = std::max(0.0f, rect.h - 2.0f);
        const float thumb_w = std::clamp(rect.h * 0.24f, 4.0f, 10.0f);
        const float thumb_center_x = inner_x + inner_w * t;
        const float thumb_x = std::clamp(thumb_center_x - thumb_w * 0.5f, inner_x,
                                         inner_x + std::max(0.0f, inner_w - thumb_w));
        float fill_right = inner_x + inner_w * t;
        if (t > 1e-4f) {
            fill_right = std::min(inner_x + inner_w, fill_right + thumb_w * 0.25f);
        }
        if (t >= 0.9999f) {
            fill_right = inner_x + inner_w;
        }
        const Rect fill{
            inner_x,
            inner_y,
            std::max(0.0f, fill_right - inner_x),
            inner_h,
        };
        const Rect thumb{
            thumb_x,
            inner_y,
            thumb_w,
            inner_h,
        };
        const bool thumb_hovered = thumb.contains(input_.mouse_x, input_.mouse_y);
        MotionState& slider_motion =
            update_motion_state(motion_id(id, 0x1a2b3c4d5e6f7003ull), hovered, active_slider_id_ == id && input_.mouse_down,
                                false, active_slider_id_ == id);
        MotionState& thumb_motion =
            update_motion_state(motion_id(id, 0x1a2b3c4d5e6f7004ull), thumb_hovered,
                                active_slider_id_ == id && input_.mouse_down, false, active_slider_id_ == id);
        const float slider_hover_v = snap_visual_motion(slider_motion.hover);
        const float slider_active_v = snap_visual_motion(slider_motion.active, 1.0f / 40.0f);
        const float thumb_hover_v = snap_visual_motion(thumb_motion.hover);
        const float thumb_active_v = snap_visual_motion(thumb_motion.active, 1.0f / 36.0f);
        add_filled_rect(rect, mix(theme_.secondary, theme_.panel, slider_hover_v * 0.05f), radius);
        if (fill.w > 0.0f && fill.h > 0.0f) {
            const float fill_radius = std::max(
                0.0f, std::min(std::min(radius - 1.0f, fill.h * 0.5f), fill.w * 0.5f));
            add_filled_rect(fill, mix(theme_.primary, theme_.secondary, 0.75f), fill_radius);
        }
        add_outline_rect(rect,
                         mix(theme_.outline, theme_.primary, slider_hover_v * 0.44f + slider_active_v * 0.16f),
                         radius, 1.0f + slider_hover_v * 0.08f);
        const float thumb_radius = std::max(0.0f, std::min(theme_.radius - 1.0f, std::min(thumb.w, thumb.h) * 0.5f));
        Color thumb_color = theme_.primary;
        if (active_slider_id_ == id) {
            thumb_color = mix(theme_.primary, theme_.panel, 0.18f);
        } else if (thumb_hovered) {
            thumb_color = mix(theme_.primary, theme_.panel, 0.10f);
        }
        const float thumb_scale = 1.0f + thumb_active_v * 0.05f;
        Rect visual_thumb = scale_rect_from_center(thumb, thumb_scale, thumb_scale);
        add_soft_glow(visual_thumb, theme_.primary, thumb_radius, thumb_hover_v * 0.14f + thumb_active_v * 0.18f,
                      3.6f);
        add_filled_rect(visual_thumb, thumb_color, thumb_radius);

        add_text(label,
                 Rect{rect.x + value_padding, rect.y,
                      rect.w - value_box.w - value_padding * 2.0f, rect.h},
                 theme_.text, label_font, TextAlign::Left);

        const bool editing = is_text_input_active(id);
        draw_input_chrome(motion_id(id, 0x1a2b3c4d5e6f7005ull), value_box, value_hovered || editing, editing,
                          editing ? theme_.input_bg : mix(theme_.input_bg, theme_.secondary, 0.25f),
                          theme_.radius - 2.0f, editing ? 1.2f : 1.0f);

        if (editing) {
            draw_text_input_content(value_box, value_font, true, value_padding, theme_.text,
                                    mix(theme_.primary, theme_.input_bg, 0.55f));
        } else {
            std::string value_text = format_float(value, value_decimals);
            if (value_text.empty()) {
                value_text = "0";
            }
            add_text(value_text,
                     Rect{value_box.x + value_padding, value_box.y,
                          value_box.w - value_padding * 2.0f, value_box.h},
                     theme_.muted_text, value_font, TextAlign::Right);
        }

        return changed;
    }

    bool input_float(std::string_view label, float& value, float min_value, float max_value, int decimals = 2,
                     float height = 34.0f) {
        if (max_value < min_value) {
            std::swap(max_value, min_value);
        }

        const Rect rect = next_rect(height);
        const float label_font = std::clamp(rect.h * 0.40f, 13.0f, 24.0f);
        const float value_font = std::max(12.0f, label_font - 0.5f);
        const float input_padding = std::clamp(rect.h * 0.18f, 6.0f, 12.0f);
        const Rect label_rect{
            rect.x,
            rect.y,
            rect.w * 0.46f,
            rect.h,
        };
        const Rect input_rect{
            rect.x + rect.w * 0.50f,
            rect.y + input_padding * 0.5f,
            rect.w * 0.50f,
            rect.h - input_padding,
        };
        const std::uint64_t id = id_for(label) ^ 0x95b6a4cb4123be3full;

        add_text(label, label_rect, theme_.text, label_font, TextAlign::Left);

        const bool hovered = input_rect.contains(input_.mouse_x, input_.mouse_y);
        if (hovered && input_.mouse_pressed) {
            start_text_input(id, format_float(value, std::max(0, decimals)), true);
        }

        bool changed = false;
        if (is_text_input_active(id)) {
            update_mouse_selection(input_rect, value_font, true, input_padding, hovered);
            consume_numeric_typing(min_value < 0.0f, true);
            clamp_live_numeric_buffer(min_value, max_value, std::max(0, decimals));
            if (input_.key_escape) {
                stop_text_input();
            } else if (input_.key_enter || (input_.mouse_pressed && !hovered)) {
                changed = commit_text_input(value, min_value, max_value) || changed;
            }
        }

        const bool editing = is_text_input_active(id);
        draw_input_chrome(motion_id(id, 0x1a2b3c4d5e6f7006ull), input_rect, hovered || editing, editing,
                          theme_.input_bg, theme_.radius - 2.0f, editing ? 1.2f : 1.0f);

        if (editing) {
            draw_text_input_content(input_rect, value_font, true, input_padding, theme_.text,
                                    mix(theme_.primary, theme_.input_bg, 0.55f));
        } else {
            std::string value_text = format_float(value, std::max(0, decimals));
            if (value_text.empty()) {
                value_text = "0";
            }
            draw_static_input_content(input_rect, value_text, value_font, true, input_padding, theme_.text);
        }

        return changed;
    }

    bool input_text(std::string_view label, std::string& value, float height = 34.0f,
                    std::string_view placeholder = {}, bool align_right = false) {
        const std::string before = value;
        const Rect rect = next_rect(height);
        const float label_font = std::clamp(rect.h * 0.40f, 13.0f, 24.0f);
        const float value_font = std::max(12.0f, label_font - 0.5f);
        const float input_padding = std::clamp(rect.h * 0.18f, 6.0f, 12.0f);
        const bool has_label = !label.empty();
        const Rect label_rect{
            rect.x,
            rect.y,
            has_label ? rect.w * 0.34f : 0.0f,
            rect.h,
        };
        const Rect input_rect = has_label
                                    ? Rect{
                                          rect.x + rect.w * 0.36f,
                                          rect.y + input_padding * 0.5f,
                                          rect.w * 0.64f,
                                          rect.h - input_padding,
                                      }
                                    : Rect{
                                          rect.x,
                                          rect.y + input_padding * 0.5f,
                                          rect.w,
                                          rect.h - input_padding,
                                      };
        std::uint64_t id = id_for(label) ^ 0x8a9de541c17f42e9ull;
        id = hash_mix(id, hash_rect(input_rect));

        if (has_label) {
            add_text(label, label_rect, theme_.text, label_font, TextAlign::Left);
        }

        const bool hovered = input_rect.contains(input_.mouse_x, input_.mouse_y);
        if (hovered && input_.mouse_pressed) {
            start_text_input(id, value, false);
        }

        bool editing = is_text_input_active(id);
        if (editing) {
            update_mouse_selection(input_rect, value_font, align_right, input_padding, hovered);
            consume_plain_typing(false);
            if (input_buffer_.size() > 256u) {
                input_buffer_.resize(256u);
                ensure_edit_state_bounds();
            }
            value = input_buffer_;
            if (input_.key_escape) {
                stop_text_input();
                editing = false;
            } else if (input_.key_enter || (input_.mouse_pressed && !hovered)) {
                value = input_buffer_;
                stop_text_input();
                editing = false;
            }
        }

        draw_input_chrome(motion_id(id, 0x1a2b3c4d5e6f7007ull), input_rect, hovered || editing, editing,
                          theme_.input_bg, theme_.radius - 2.0f, editing ? 1.2f : 1.0f);

        if (editing) {
            draw_text_input_content(input_rect, value_font, align_right, input_padding, theme_.text,
                                    mix(theme_.primary, theme_.input_bg, 0.55f));
        } else if (value.empty() && !placeholder.empty()) {
            draw_static_input_content(input_rect, placeholder, value_font, align_right, input_padding,
                                      theme_.muted_text);
        } else {
            draw_static_input_content(input_rect, value, value_font, align_right, input_padding, theme_.text);
        }

        return value != before;
    }

    void input_readonly(std::string_view label, std::string_view value, float height = 34.0f,
                        bool align_right = false, float value_font_scale = 1.0f, bool muted = true) {
        const Rect rect = next_rect(height);
        const float label_font = std::clamp(rect.h * 0.40f, 13.0f, 24.0f);
        const float base_value_font = std::clamp(rect.h * 0.44f, 12.0f, 56.0f);
        const float scale = std::clamp(value_font_scale, 0.5f, 2.2f);
        const float value_font = std::clamp(base_value_font * scale, 12.0f, 72.0f);
        const float input_padding = std::clamp(rect.h * 0.18f, 6.0f, 12.0f);
        const bool has_label = !label.empty();
        const Rect label_rect{
            rect.x,
            rect.y,
            has_label ? rect.w * 0.34f : 0.0f,
            rect.h,
        };
        const Rect input_rect = has_label
                                    ? Rect{
                                          rect.x + rect.w * 0.36f,
                                          rect.y + input_padding * 0.5f,
                                          rect.w * 0.64f,
                                          rect.h - input_padding,
                                      }
                                    : Rect{
                                          rect.x,
                                          rect.y + input_padding * 0.5f,
                                          rect.w,
                                          rect.h - input_padding,
                                      };

        if (has_label) {
            add_text(label, label_rect, theme_.text, label_font, TextAlign::Left);
        }
        const bool hovered = input_rect.contains(input_.mouse_x, input_.mouse_y);
        draw_input_chrome(motion_id(label, input_rect, 0x1a2b3c4d5e6f7008ull), input_rect, hovered, false,
                          mix(theme_.input_bg, theme_.secondary, 0.08f), theme_.radius - 2.0f, 1.0f);
        draw_static_input_content(input_rect, value, value_font, align_right, input_padding,
                                  muted ? theme_.muted_text : theme_.text);
    }

    struct ScrollAreaOptions {
        float padding{6.0f};
        float scrollbar_width{7.0f};
        float min_thumb_height{18.0f};
        float wheel_step{30.0f};
        float drag_sensitivity{1.0f};
        float inertia_friction{14.0f};
        float bounce_strength{28.0f};
        float bounce_damping{18.0f};
        float overscroll_limit{20.0f};
        bool enable_drag{true};
        bool show_scrollbar{true};
        bool draw_background{true};
    };

    bool begin_scroll_area(std::string_view label, float height) {
        return begin_scroll_area(label, height, ScrollAreaOptions{});
    }

    bool begin_scroll_area(std::string_view label, float height, const ScrollAreaOptions& options) {
        const Rect outer = next_rect(std::max(40.0f, height));
        const float pad = std::clamp(options.padding, 2.0f, 24.0f);
        const bool show_scrollbar = options.show_scrollbar;
        const float scrollbar_w = show_scrollbar ? std::clamp(options.scrollbar_width, 4.0f, 20.0f) : 0.0f;
        const float lane_gap = show_scrollbar ? std::max(3.0f, pad * 0.45f) : 0.0f;
        const Rect viewport{
            outer.x + pad,
            outer.y + pad,
            std::max(10.0f, outer.w - pad * 2.0f - scrollbar_w - lane_gap),
            std::max(10.0f, outer.h - pad * 2.0f),
        };
        const Rect track{
            viewport.x + viewport.w + lane_gap,
            viewport.y,
            scrollbar_w,
            viewport.h,
        };

        const std::uint64_t id = id_for(label) ^ 0x3cd71be2458fe12bull;
        ScrollAreaState& state = scroll_area_state_[id];
        const float content_h = std::max(viewport.h, state.content_height);
        const float max_scroll = std::max(0.0f, content_h - viewport.h);
        const bool hovered_outer = outer.contains(input_.mouse_x, input_.mouse_y);
        const bool hovered_view = viewport.contains(input_.mouse_x, input_.mouse_y);

        const float before_scroll = state.scroll;
        const float before_velocity = state.velocity;
        bool needs_animation = false;

        const int input_depth = static_cast<int>(scope_stack_.size());
        const bool wheel_targeted =
            wheel_target_.kind == ScrollInputKind::ScrollArea && wheel_target_.id == id;
        if (wheel_targeted && hovered_outer && max_scroll > 1e-4f && std::fabs(input_.mouse_wheel_y) > 1e-6f) {
            const float wheel_delta = input_.mouse_wheel_y * std::max(12.0f, options.wheel_step);
            state.scroll -= wheel_delta;
            const float dt = std::max(1e-4f, ui_dt_);
            const float impulse_velocity = (-wheel_delta / dt) * 0.10f;
            state.velocity = state.velocity * 0.30f + impulse_velocity;
            needs_animation = true;
        }

        const float thumb_h = max_scroll > 0.0f ? std::max(std::max(12.0f, options.min_thumb_height),
                                                           viewport.h * (viewport.h / std::max(viewport.h + 1.0f, content_h)))
                                                : viewport.h;
        const float thumb_travel = std::max(0.0f, track.h - thumb_h);
        const float clamped_scroll = std::clamp(state.scroll, 0.0f, max_scroll);
        const float thumb_y = track.y + (max_scroll > 0.0f ? (clamped_scroll / max_scroll) * thumb_travel : 0.0f);
        const Rect thumb{
            track.x,
            thumb_y,
            track.w,
            thumb_h,
        };
        const bool hovered_thumb =
            show_scrollbar && max_scroll > 0.0f && thumb.contains(input_.mouse_x, input_.mouse_y);

        scroll_input_regions_.push_back(ScrollInputRegion{
            ScrollInputKind::ScrollArea,
            id,
            outer,
            thumb,
            show_scrollbar && max_scroll > 0.0f,
            max_scroll > 1e-4f,
            input_depth,
        });

        const bool press_targeted =
            press_target_.kind == ScrollInputKind::ScrollArea && press_target_.id == id;
        if (input_.mouse_pressed && press_targeted) {
            if (press_target_.on_thumb && hovered_thumb) {
                active_scrollbar_drag_id_ = id;
                active_scrollbar_drag_offset_ = input_.mouse_y - thumb.y;
                active_scroll_drag_id_ = 0u;
                state.velocity = 0.0f;
            } else if (options.enable_drag && hovered_view) {
                active_scroll_drag_id_ = id;
                active_scroll_drag_last_y_ = input_.mouse_y;
                active_scrollbar_drag_id_ = 0u;
                state.velocity = 0.0f;
            }
        }

        if (active_scrollbar_drag_id_ == id) {
            if (input_.mouse_down && max_scroll > 0.0f) {
                const float new_thumb_y =
                    std::clamp(input_.mouse_y - active_scrollbar_drag_offset_, track.y, track.y + thumb_travel);
                const float t = thumb_travel > 1e-5f ? (new_thumb_y - track.y) / thumb_travel : 0.0f;
                state.scroll = t * max_scroll;
                state.velocity = 0.0f;
                needs_animation = true;
            } else if (!input_.mouse_down) {
                active_scrollbar_drag_id_ = 0u;
            }
        }

        if (active_scroll_drag_id_ == id) {
            if (input_.mouse_down) {
                const float dy = input_.mouse_y - active_scroll_drag_last_y_;
                active_scroll_drag_last_y_ = input_.mouse_y;
                const float delta = -dy * std::max(0.1f, options.drag_sensitivity);
                state.scroll += delta;
                const float dt = std::max(1e-4f, ui_dt_);
                const float inst_velocity = delta / dt;
                state.velocity = state.velocity * 0.60f + inst_velocity * 0.40f;
                needs_animation = true;
            } else {
                active_scroll_drag_id_ = 0u;
            }
        }

        const bool dragging =
            active_scroll_drag_id_ == id || active_scrollbar_drag_id_ == id;
        const float overscroll =
            max_scroll > 1e-4f ? std::max(0.0f, options.overscroll_limit) : 0.0f;
        if (!dragging) {
            const float friction = std::clamp(options.inertia_friction, 0.0f, 80.0f);
            const float spring = std::max(0.0f, options.bounce_strength);
            const float damping = std::max(0.0f, options.bounce_damping);
            const bool out_of_bounds = state.scroll < 0.0f || state.scroll > max_scroll;
            if (out_of_bounds) {
                const float target = state.scroll < 0.0f ? 0.0f : max_scroll;
                const float displacement = target - state.scroll;
                state.velocity += displacement * spring * ui_dt_;
                state.velocity *= std::exp(-damping * ui_dt_);
            } else {
                state.velocity *= std::exp(-friction * ui_dt_);
            }
            state.scroll += state.velocity * ui_dt_;
            if (std::fabs(state.velocity) < 6.0f && !out_of_bounds) {
                state.velocity = 0.0f;
            }
            if (out_of_bounds) {
                const float target = state.scroll < 0.0f ? 0.0f : max_scroll;
                if (std::fabs(target - state.scroll) < 0.25f && std::fabs(state.velocity) < 8.0f) {
                    state.scroll = target;
                    state.velocity = 0.0f;
                }
            }
        }
        if (max_scroll <= 1e-4f) {
            state.scroll = 0.0f;
            state.velocity = 0.0f;
        }
        state.scroll = std::clamp(state.scroll, -overscroll, max_scroll + overscroll);

        const bool scroll_changed = std::fabs(state.scroll - before_scroll) > 0.05f;
        const bool velocity_changed = std::fabs(state.velocity - before_velocity) > 5.0f;
        const bool still_animating =
            std::fabs(state.velocity) > 6.0f ||
            state.scroll < -0.25f || state.scroll > max_scroll + 0.25f;
        if (scroll_changed || velocity_changed || needs_animation || still_animating) {
            repaint_requested_ = true;
        }

        MotionState& thumb_motion =
            update_motion_state(motion_id(id, 0x1a2b3c4d5e6f700cull), hovered_thumb,
                                active_scrollbar_drag_id_ == id && input_.mouse_down, false,
                                active_scrollbar_drag_id_ == id);
        if (options.draw_background) {
            draw_input_chrome(motion_id(id, 0x1a2b3c4d5e6f700dull), outer, hovered_outer,
                              active_scroll_drag_id_ == id || active_scrollbar_drag_id_ == id,
                              mix(theme_.input_bg, theme_.secondary, 0.10f), theme_.radius - 2.0f, 1.0f);
        }

        if (show_scrollbar) {
            const float draw_scroll = std::clamp(state.scroll, 0.0f, max_scroll);
            const float draw_thumb_y =
                track.y + (max_scroll > 0.0f ? (draw_scroll / max_scroll) * thumb_travel : 0.0f);
            const Rect draw_thumb{
                track.x,
                draw_thumb_y,
                track.w,
                thumb_h,
            };
            const bool has_scroll = max_scroll > 1e-4f;
            const Color track_color = mix(theme_.secondary, theme_.panel, 0.45f);
            const Color thumb_color = has_scroll
                                          ? ((hovered_thumb || active_scrollbar_drag_id_ == id)
                                                 ? theme_.primary
                                                 : mix(theme_.primary, theme_.panel, 0.40f))
                                          : mix(theme_.outline, theme_.panel, 0.55f);
            add_filled_rect(track, track_color, std::max(0.0f, std::min(track.w, track.h) * 0.5f));
            const float thumb_hover_v = snap_visual_motion(thumb_motion.hover);
            const float thumb_active_v = snap_visual_motion(thumb_motion.active, 1.0f / 36.0f);
            const float thumb_scale = 1.0f + thumb_active_v * 0.05f;
            const Rect visual_thumb = scale_rect_from_center(draw_thumb, 1.0f, thumb_scale);
            add_soft_glow(visual_thumb, theme_.primary, std::max(0.0f, std::min(draw_thumb.w, draw_thumb.h) * 0.5f),
                          thumb_hover_v * 0.12f + thumb_active_v * 0.18f, 3.2f);
            add_filled_rect(visual_thumb, thumb_color, std::max(0.0f, std::min(visual_thumb.w, visual_thumb.h) * 0.5f));
        }

        const bool had_outer_row = row_.active;
        const RowState outer_row = row_;
        scope_stack_.push_back(ScopeState{
            ScopeKind::ScrollArea,
            content_x_,
            content_width_,
            0.0f,
            0u,
            0u,
            outer.y,
            outer.h,
            0.0f,
            had_outer_row,
            outer_row,
            false,
            -1,
            id,
            viewport,
            viewport.y - state.scroll,
            true,
        });

        push_clip_rect(viewport);
        content_x_ = viewport.x;
        content_width_ = viewport.w;
        cursor_y_ = viewport.y - state.scroll;
        row_ = RowState{};
        return true;
    }

    void end_scroll_area() {
        flush_row();
        if (scope_stack_.empty()) {
            return;
        }
        if (scope_stack_.back().kind != ScopeKind::ScrollArea) {
            return;
        }
        restore_scope();
    }

    bool text_area(std::string_view label, std::string& text, float height = 170.0f) {
        const std::string original_text = text;
        const Rect rect = next_rect(std::max(96.0f, height));
        const float label_font = std::clamp(rect.h * 0.12f, 12.0f, 18.0f);
        const float text_font = std::clamp(rect.h * 0.13f, 13.0f, 22.0f);
        const float outer_pad = std::clamp(rect.h * 0.04f, 6.0f, 12.0f);
        const float line_h = text_font + 5.0f;

        const Rect label_rect{
            rect.x + outer_pad,
            rect.y + outer_pad,
            rect.w - outer_pad * 2.0f,
            label_font,
        };
        const Rect box_rect{
            rect.x + outer_pad,
            label_rect.y + label_rect.h + 6.0f,
            rect.w - outer_pad * 2.0f,
            rect.h - (label_rect.h + outer_pad + 12.0f),
        };
        const float radius = std::max(2.0f, theme_.radius - 2.0f);
        const float scrollbar_w = 8.0f;
        const float text_pad = std::clamp(rect.h * 0.03f, 6.0f, 10.0f);
        const float content_w = std::max(24.0f, box_rect.w - text_pad * 2.0f - 2.0f);
        const Rect content_clip{
            box_rect.x + text_pad,
            box_rect.y + text_pad,
            content_w,
            std::max(24.0f, box_rect.h - text_pad * 2.0f),
        };

        const std::uint64_t id = id_for(label) ^ 0x5f8d37aa44c2e391ull;
        TextAreaState& state = text_area_state_[id];
        const bool hovered_box = box_rect.contains(input_.mouse_x, input_.mouse_y);
        const int input_depth = static_cast<int>(scope_stack_.size());
        bool editing = is_text_input_active(id);
        bool follow_caret = false;

        if (input_.mouse_pressed) {
            if (hovered_box) {
                if (!editing) {
                    start_text_input(id, text, false);
                }
                editing = is_text_input_active(id);
            } else if (editing) {
                text = input_buffer_;
                stop_text_input();
                editing = false;
            }
        }

        if (editing) {
            if (input_.key_escape) {
                stop_text_input();
                editing = false;
            } else {
                follow_caret = input_.key_home || input_.key_end || input_.key_left || input_.key_right ||
                               input_.key_backspace || input_.key_delete || input_.key_enter ||
                               input_.key_paste || !input_.text_input.empty();
                consume_plain_typing(true);
                text = input_buffer_;
            }
        }

        const std::string_view text_view = editing ? std::string_view(input_buffer_) : std::string_view(text);

        struct WrappedLine {
            std::size_t start{0u};
            std::size_t len{0u};
            std::vector<std::size_t> boundaries{};
            std::vector<float> offsets{};
        };
        std::vector<WrappedLine> lines;
        lines.reserve(64);
        auto push_wrapped_line = [&](std::size_t start, std::size_t end,
                                     const std::vector<std::size_t>& boundaries,
                                     const std::vector<float>& offsets) {
            WrappedLine line{};
            line.start = start;
            line.len = end - start;
            line.boundaries = boundaries;
            line.offsets = offsets;
            if (line.boundaries.empty()) {
                line.boundaries.push_back(start);
                line.offsets.push_back(0.0f);
            }
            if (line.boundaries.back() != end) {
                line.boundaries.push_back(end);
                line.offsets.push_back(line.offsets.empty() ? 0.0f : line.offsets.back());
            }
            lines.push_back(std::move(line));
        };
        std::size_t line_start = 0u;
        std::size_t index = 0u;
        float line_width = 0.0f;
        std::vector<std::size_t> line_boundaries{line_start};
        std::vector<float> line_offsets{0.0f};
        while (index < text_view.size()) {
            const unsigned char ch = static_cast<unsigned char>(text_view[index]);
            if (ch == '\r') {
                index += 1u;
                continue;
            }
            if (ch == '\n') {
                push_wrapped_line(line_start, index, line_boundaries, line_offsets);
                index += 1u;
                line_start = index;
                line_width = 0.0f;
                line_boundaries.assign(1u, line_start);
                line_offsets.assign(1u, 0.0f);
                continue;
            }

            std::size_t next = index;
            std::uint32_t cp = 0u;
            decode_utf8_at(text_view, index, text_view.size(), cp, next);
            const float advance = codepoint_advance(text_view, index, next, cp, text_font);
            if (line_width > 0.0f && line_width + advance > content_w) {
                push_wrapped_line(line_start, index, line_boundaries, line_offsets);
                line_start = index;
                line_width = 0.0f;
                line_boundaries.assign(1u, line_start);
                line_offsets.assign(1u, 0.0f);
                continue;
            }
            index = next;
            line_width += advance;
            line_boundaries.push_back(index);
            line_offsets.push_back(line_width);
        }
        push_wrapped_line(line_start, text_view.size(), line_boundaries, line_offsets);
        if (lines.empty()) {
            lines.push_back(WrappedLine{0u, 0u, std::vector<std::size_t>{0u}, std::vector<float>{0.0f}});
        }

        const float viewport_h = content_clip.h;
        const float total_h = static_cast<float>(lines.size()) * line_h;
        const float max_scroll = std::max(0.0f, total_h - viewport_h);
        const Rect track{
            box_rect.x + box_rect.w - text_pad - scrollbar_w,
            box_rect.y + text_pad,
            scrollbar_w,
            viewport_h,
        };
        const float thumb_h = max_scroll > 0.0f
                                  ? std::max(18.0f, viewport_h * (viewport_h / std::max(viewport_h + 1.0f, total_h)))
                                  : viewport_h;
        const float thumb_travel = std::max(0.0f, track.h - thumb_h);
        const float thumb_y =
            track.y + (max_scroll > 0.0f ? (state.scroll / max_scroll) * thumb_travel : 0.0f);
        const Rect thumb{track.x, thumb_y, track.w, thumb_h};
        const bool hovered_thumb = thumb.contains(input_.mouse_x, input_.mouse_y);

        const bool wheel_targeted =
            wheel_target_.kind == ScrollInputKind::TextArea && wheel_target_.id == id;
        if (wheel_targeted && hovered_box && max_scroll > 1e-4f && std::fabs(input_.mouse_wheel_y) > 1e-6f) {
            state.scroll = std::clamp(state.scroll - input_.mouse_wheel_y * line_h * 2.0f, 0.0f, max_scroll);
        } else {
            state.scroll = std::clamp(state.scroll, 0.0f, max_scroll);
        }

        auto boundary_index_for_byte = [&](const WrappedLine& line, std::size_t byte_pos) -> std::size_t {
            if (line.boundaries.empty()) {
                return 0u;
            }
            const auto it = std::lower_bound(line.boundaries.begin(), line.boundaries.end(), byte_pos);
            if (it == line.boundaries.end()) {
                return line.boundaries.size() - 1u;
            }
            return static_cast<std::size_t>(it - line.boundaries.begin());
        };

        auto line_x_for_byte = [&](const WrappedLine& line, std::size_t byte_pos) -> float {
            if (line.offsets.empty()) {
                return 0.0f;
            }
            return line.offsets[boundary_index_for_byte(line, byte_pos)];
        };

        auto caret_from_point = [&](float mouse_x, float mouse_y) -> std::size_t {
            if (lines.empty()) {
                return 0u;
            }
            const float local_y = std::clamp(mouse_y - content_clip.y + state.scroll, 0.0f, total_h);
            std::size_t line_index =
                static_cast<std::size_t>(std::floor(local_y / line_h));
            line_index = std::min(line_index, lines.size() - 1u);
            const WrappedLine& line = lines[line_index];
            const float rel_x = std::max(0.0f, mouse_x - content_clip.x);
            if (line.boundaries.size() <= 1u) {
                return line.start;
            }
            for (std::size_t i = 1u; i < line.boundaries.size(); ++i) {
                const float left_x = line.offsets[i - 1u];
                const float right_x = line.offsets[i];
                if (rel_x < left_x + (right_x - left_x) * 0.5f) {
                    return line.boundaries[i - 1u];
                }
                if (rel_x < right_x) {
                    return line.boundaries[i];
                }
            }
            return line.boundaries.back();
        };

        auto resolve_caret_visual = [&](std::size_t caret_pos) -> std::pair<std::size_t, std::size_t> {
            if (lines.empty()) {
                return {0u, 0u};
            }
            const std::size_t clamped_caret = std::min(caret_pos, text_view.size());
            for (std::size_t i = 0; i < lines.size(); ++i) {
                const std::size_t line_start_idx = lines[i].start;
                const std::size_t line_end_idx = line_start_idx + lines[i].len;
                const bool has_next = i + 1u < lines.size();
                const std::size_t next_start = has_next ? lines[i + 1u].start : line_end_idx;

                if (clamped_caret < line_start_idx) {
                    return {i, 0u};
                }
                if (clamped_caret < line_end_idx) {
                    return {i, boundary_index_for_byte(lines[i], clamped_caret)};
                }
                if (clamped_caret == line_end_idx) {
                    // Wrapped lines share a boundary; prefer next line start to avoid end/start jitter.
                    if (has_next && next_start == line_end_idx) {
                        return {i + 1u, 0u};
                    }
                    return {i, boundary_index_for_byte(lines[i], clamped_caret)};
                }
                if (has_next && clamped_caret < next_start) {
                    return {i, lines[i].boundaries.empty() ? 0u : lines[i].boundaries.size() - 1u};
                }
            }
            return {lines.size() - 1u, lines.back().boundaries.empty() ? 0u : lines.back().boundaries.size() - 1u};
        };

        auto caret_x_in_line = [&](std::size_t line_index, std::size_t caret_boundary) -> float {
            if (lines.empty()) {
                return 0.0f;
            }
            line_index = std::min(line_index, lines.size() - 1u);
            const WrappedLine& line = lines[line_index];
            if (line.offsets.empty()) {
                return 0.0f;
            }
            caret_boundary = std::min(caret_boundary, line.offsets.size() - 1u);
            return line.offsets[caret_boundary];
        };

        auto caret_for_line_x = [&](std::size_t line_index, float target_x) -> std::size_t {
            if (lines.empty()) {
                return 0u;
            }
            line_index = std::min(line_index, lines.size() - 1u);
            const WrappedLine& line = lines[line_index];
            if (target_x <= 0.0f) {
                return line.start;
            }
            if (line.boundaries.size() <= 1u) {
                return line.start;
            }
            for (std::size_t i = 1u; i < line.boundaries.size(); ++i) {
                const float left_x = line.offsets[i - 1u];
                const float right_x = line.offsets[i];
                if (target_x < left_x + (right_x - left_x) * 0.5f) {
                    return line.boundaries[i - 1u];
                }
                if (target_x < right_x) {
                    return line.boundaries[i];
                }
            }
            return line.boundaries.back();
        };

        if (editing) {
            const bool reset_preferred_x =
                input_.mouse_pressed || input_.key_left || input_.key_right || input_.key_home || input_.key_end ||
                input_.key_backspace || input_.key_delete || input_.key_enter || input_.key_paste ||
                !input_.text_input.empty();
            if (reset_preferred_x) {
                state.preferred_x = -1.0f;
            }

            if (input_.mouse_pressed && content_clip.contains(input_.mouse_x, input_.mouse_y) &&
                !hovered_thumb) {
                const std::size_t caret = caret_from_point(input_.mouse_x, input_.mouse_y);
                caret_pos_ = caret;
                selection_start_ = caret;
                selection_end_ = caret;
                drag_anchor_ = caret;
                drag_selecting_ = true;
                state.preferred_x = -1.0f;
                follow_caret = true;
            }
            if (drag_selecting_ && input_.mouse_down) {
                const float clamped_x = std::clamp(input_.mouse_x, content_clip.x, content_clip.x + content_clip.w);
                const float clamped_y = std::clamp(input_.mouse_y, content_clip.y, content_clip.y + content_clip.h);
                const std::size_t caret = caret_from_point(clamped_x, clamped_y);
                caret_pos_ = caret;
                selection_start_ = drag_anchor_;
                selection_end_ = caret;
                state.preferred_x = -1.0f;
                follow_caret = true;
            }
            if (input_.mouse_released) {
                drag_selecting_ = false;
            }

            if ((input_.key_up || input_.key_down) && !lines.empty()) {
                const auto caret_visual = resolve_caret_visual(caret_pos_);
                const std::size_t caret_line = caret_visual.first;
                const std::size_t caret_byte = caret_visual.second;
                if (state.preferred_x < 0.0f) {
                    state.preferred_x = caret_x_in_line(caret_line, caret_byte);
                }

                std::size_t target_line = caret_line;
                if (input_.key_up && caret_line > 0u) {
                    target_line = caret_line - 1u;
                } else if (input_.key_down && caret_line + 1u < lines.size()) {
                    target_line = caret_line + 1u;
                }

                if (target_line != caret_line) {
                    set_caret(caret_for_line_x(target_line, state.preferred_x), input_.key_shift);
                }
                follow_caret = true;
            }

            if (follow_caret) {
                const auto caret_visual = resolve_caret_visual(caret_pos_);
                const std::size_t caret_line = caret_visual.first;
                const float caret_top = static_cast<float>(caret_line) * line_h;
                if (caret_top < state.scroll) {
                    state.scroll = caret_top;
                } else if (caret_top + line_h > state.scroll + viewport_h) {
                    state.scroll = caret_top + line_h - viewport_h;
                }
            }
            state.scroll = std::clamp(state.scroll, 0.0f, max_scroll);
        }

        add_text(label, label_rect, theme_.muted_text, label_font, TextAlign::Left);
        draw_input_chrome(motion_id(id, 0x1a2b3c4d5e6f700eull), box_rect, hovered_box || editing, editing,
                          mix(theme_.input_bg, theme_.secondary, 0.08f), radius, editing ? 1.1f : 1.0f);

        if (input_.mouse_pressed && press_target_.kind == ScrollInputKind::TextArea && press_target_.id == id &&
            press_target_.on_thumb && hovered_thumb) {
            active_textarea_scroll_id_ = id;
            active_textarea_drag_offset_ = input_.mouse_y - thumb.y;
            active_scroll_drag_id_ = 0u;
            active_scrollbar_drag_id_ = 0u;
        }
        if (active_textarea_scroll_id_ == id) {
            if (input_.mouse_down && max_scroll > 0.0f) {
                const float new_thumb_y =
                    std::clamp(input_.mouse_y - active_textarea_drag_offset_, track.y, track.y + thumb_travel);
                const float t = thumb_travel > 1e-5f ? (new_thumb_y - track.y) / thumb_travel : 0.0f;
                state.scroll = std::clamp(t * max_scroll, 0.0f, max_scroll);
            } else if (!input_.mouse_down) {
                active_textarea_scroll_id_ = 0u;
            }
        }

        MotionState& thumb_motion =
            update_motion_state(motion_id(id, 0x1a2b3c4d5e6f700full), hovered_thumb,
                                active_textarea_scroll_id_ == id && input_.mouse_down, false,
                                active_textarea_scroll_id_ == id);
        const float thumb_hover_v = snap_visual_motion(thumb_motion.hover);
        const float thumb_active_v = snap_visual_motion(thumb_motion.active, 1.0f / 36.0f);
        add_filled_rect(track, mix(theme_.secondary, theme_.panel, 0.45f), 3.0f);
        const Rect visual_thumb =
            scale_rect_from_center(thumb, 1.0f, 1.0f + thumb_active_v * 0.05f);
        add_soft_glow(visual_thumb, theme_.primary, 3.0f, thumb_hover_v * 0.12f + thumb_active_v * 0.18f, 3.2f);
        add_filled_rect(visual_thumb, hovered_thumb ? theme_.primary : mix(theme_.primary, theme_.panel, 0.40f), 3.0f);

        const float line_offset = std::fmod(state.scroll, line_h);
        std::size_t first_line = static_cast<std::size_t>(std::floor(state.scroll / line_h));
        first_line = std::min(first_line, lines.size() - 1u);
        const Color selection_color = mix(theme_.primary, theme_.input_bg, 0.55f);

        if (editing && has_selection()) {
            const std::size_t sel_l = selection_left();
            const std::size_t sel_r = selection_right();
            const float selection_pad_x = std::max(1.0f, text_font * 0.08f);
            float y = content_clip.y - line_offset;
            for (std::size_t i = first_line; i < lines.size(); ++i) {
                if (y > content_clip.y + content_clip.h) {
                    break;
                }
                const WrappedLine& line = lines[i];
                const std::size_t line_start_idx = line.start;
                const std::size_t line_end_idx = line.start + line.len;
                const std::size_t hi_l = std::max(sel_l, line_start_idx);
                const std::size_t hi_r = std::min(sel_r, line_end_idx);
                if (hi_r > hi_l && y + line_h >= content_clip.y) {
                    const float before_w = line_x_for_byte(line, hi_l);
                    const float selected_w = line_x_for_byte(line, hi_r) - before_w;
                    Rect sel_rect{
                        content_clip.x + before_w - selection_pad_x,
                        y,
                        std::max(0.0f, selected_w) + selection_pad_x * 2.0f,
                        std::max(2.0f, line_h),
                    };
                    Rect clipped{};
                    if (intersect_rects(sel_rect, content_clip, clipped)) {
                        add_filled_rect(clipped, selection_color, 2.0f);
                    }
                }
                y += line_h;
            }
        }

        float y = content_clip.y - line_offset;
        for (std::size_t i = first_line; i < lines.size(); ++i) {
            if (y > content_clip.y + content_clip.h) {
                break;
            }
            if (y + line_h >= content_clip.y) {
                const WrappedLine& line = lines[i];
                add_text(text_view.substr(line.start, line.len), Rect{content_clip.x, y, content_w, line_h},
                         theme_.text, text_font, TextAlign::Left, &content_clip);
            }
            y += line_h;
        }

        if (editing && should_show_caret()) {
            const auto caret_visual = resolve_caret_visual(caret_pos_);
            const std::size_t caret_line = caret_visual.first;
            const std::size_t caret_boundary = caret_visual.second;
            const float caret_y = content_clip.y + static_cast<float>(caret_line) * line_h - state.scroll;
            const float caret_w = caret_x_in_line(caret_line, caret_boundary);
            float caret_x = content_clip.x + caret_w;
            const float caret_h = std::max(2.0f, line_h - 2.0f);
            caret_x = std::clamp(caret_x, content_clip.x, content_clip.x + content_clip.w - 1.0f);
            Rect caret_rect{
                caret_x,
                caret_y + 1.0f,
                std::max(1.4f, text_font * 0.08f),
                caret_h,
            };
            Rect clipped{};
            if (intersect_rects(caret_rect, content_clip, clipped)) {
                add_filled_rect(clipped, theme_.text, 0.0f);
            }
        }

        return text != original_text;
    }

    void text_area_readonly(std::string_view label, std::string_view text, float height = 170.0f) {
        const Rect rect = next_rect(std::max(96.0f, height));
        const float label_font = std::clamp(rect.h * 0.12f, 12.0f, 18.0f);
        const float text_font = std::clamp(rect.h * 0.13f, 13.0f, 20.0f);
        const float outer_pad = std::clamp(rect.h * 0.04f, 6.0f, 12.0f);
        const float line_h = text_font + 5.0f;

        const Rect label_rect{
            rect.x + outer_pad,
            rect.y + outer_pad,
            rect.w - outer_pad * 2.0f,
            label_font,
        };
        const Rect box_rect{
            rect.x + outer_pad,
            label_rect.y + label_rect.h + 6.0f,
            rect.w - outer_pad * 2.0f,
            rect.h - (label_rect.h + outer_pad + 12.0f),
        };
        const float radius = std::max(2.0f, theme_.radius - 2.0f);
        const float scrollbar_w = 8.0f;
        const float text_pad = std::clamp(rect.h * 0.03f, 6.0f, 10.0f);
        const float content_w = std::max(24.0f, box_rect.w - text_pad * 2.0f - 2.0f);
        const Rect content_clip{
            box_rect.x + text_pad,
            box_rect.y + text_pad,
            content_w,
            std::max(24.0f, box_rect.h - text_pad * 2.0f),
        };

        std::vector<std::pair<std::size_t, std::size_t>> lines;
        lines.reserve(64);
        std::size_t line_start = 0u;
        std::size_t index = 0u;
        float line_width = 0.0f;
        while (index < text.size()) {
            const unsigned char ch = static_cast<unsigned char>(text[index]);
            if (ch == '\r') {
                index += 1u;
                continue;
            }
            if (ch == '\n') {
                lines.emplace_back(line_start, index - line_start);
                index += 1u;
                line_start = index;
                line_width = 0.0f;
                continue;
            }

            std::size_t next = index;
            std::uint32_t cp = 0u;
            decode_utf8_at(text, index, text.size(), cp, next);
            const float advance = codepoint_advance(text, index, next, cp, text_font);
            if (line_width > 0.0f && line_width + advance > content_w) {
                lines.emplace_back(line_start, index - line_start);
                line_start = index;
                line_width = 0.0f;
                continue;
            }
            index = next;
            line_width += advance;
        }
        lines.emplace_back(line_start, text.size() - line_start);
        if (lines.empty()) {
            lines.emplace_back(0u, 0u);
        }

        const float viewport_h = std::max(24.0f, box_rect.h - text_pad * 2.0f);
        const float total_h = static_cast<float>(lines.size()) * line_h;
        const float max_scroll = std::max(0.0f, total_h - viewport_h);

        const std::uint64_t id = id_for(label) ^ 0x5f8d37aa44c2e391ull;
        TextAreaState& state = text_area_state_[id];
        const bool hovered_box = box_rect.contains(input_.mouse_x, input_.mouse_y);
        const int input_depth = static_cast<int>(scope_stack_.size());
        const bool wheel_targeted =
            wheel_target_.kind == ScrollInputKind::TextArea && wheel_target_.id == id;
        if (wheel_targeted && hovered_box && max_scroll > 1e-4f && std::fabs(input_.mouse_wheel_y) > 1e-6f) {
            state.scroll = std::clamp(state.scroll - input_.mouse_wheel_y * line_h * 2.0f, 0.0f, max_scroll);
        } else {
            state.scroll = std::clamp(state.scroll, 0.0f, max_scroll);
        }

        add_text(label, label_rect, theme_.muted_text, label_font, TextAlign::Left);
        draw_input_chrome(motion_id(id, 0x1a2b3c4d5e6f7010ull), box_rect, hovered_box, false,
                          mix(theme_.input_bg, theme_.secondary, 0.08f), radius, 1.0f);

        const Rect track{
            box_rect.x + box_rect.w - text_pad - scrollbar_w,
            box_rect.y + text_pad,
            scrollbar_w,
            viewport_h,
        };
        const float thumb_h = max_scroll > 0.0f
                                  ? std::max(18.0f, viewport_h * (viewport_h / std::max(viewport_h + 1.0f, total_h)))
                                  : viewport_h;
        const float thumb_travel = std::max(0.0f, track.h - thumb_h);
        const float thumb_y =
            track.y + (max_scroll > 0.0f ? (state.scroll / max_scroll) * thumb_travel : 0.0f);
        const Rect thumb{track.x, thumb_y, track.w, thumb_h};
        const bool hovered_thumb = thumb.contains(input_.mouse_x, input_.mouse_y);

        scroll_input_regions_.push_back(ScrollInputRegion{
            ScrollInputKind::TextArea,
            id,
            box_rect,
            thumb,
            max_scroll > 0.0f,
            max_scroll > 1e-4f,
            input_depth,
        });
        if (input_.mouse_pressed && press_target_.kind == ScrollInputKind::TextArea && press_target_.id == id &&
            press_target_.on_thumb && hovered_thumb) {
            active_textarea_scroll_id_ = id;
            active_textarea_drag_offset_ = input_.mouse_y - thumb.y;
            active_scroll_drag_id_ = 0u;
            active_scrollbar_drag_id_ = 0u;
        }
        if (active_textarea_scroll_id_ == id) {
            if (input_.mouse_down && max_scroll > 0.0f) {
                const float new_thumb_y =
                    std::clamp(input_.mouse_y - active_textarea_drag_offset_, track.y, track.y + thumb_travel);
                const float t = thumb_travel > 1e-5f ? (new_thumb_y - track.y) / thumb_travel : 0.0f;
                state.scroll = std::clamp(t * max_scroll, 0.0f, max_scroll);
            } else if (!input_.mouse_down) {
                active_textarea_scroll_id_ = 0u;
            }
        }

        MotionState& thumb_motion =
            update_motion_state(motion_id(id, 0x1a2b3c4d5e6f7011ull), hovered_thumb,
                                active_textarea_scroll_id_ == id && input_.mouse_down, false,
                                active_textarea_scroll_id_ == id);
        const float thumb_hover_v = snap_visual_motion(thumb_motion.hover);
        const float thumb_active_v = snap_visual_motion(thumb_motion.active, 1.0f / 36.0f);
        add_filled_rect(track, mix(theme_.secondary, theme_.panel, 0.45f), 3.0f);
        const Rect visual_thumb =
            scale_rect_from_center(thumb, 1.0f, 1.0f + thumb_active_v * 0.05f);
        add_soft_glow(visual_thumb, theme_.primary, 3.0f, thumb_hover_v * 0.12f + thumb_active_v * 0.18f, 3.2f);
        add_filled_rect(visual_thumb, hovered_thumb ? theme_.primary : mix(theme_.primary, theme_.panel, 0.40f), 3.0f);

        const float line_offset = std::fmod(state.scroll, line_h);
        std::size_t first_line = static_cast<std::size_t>(std::floor(state.scroll / line_h));
        float y = box_rect.y + text_pad - line_offset;
        for (std::size_t i = first_line; i < lines.size(); ++i) {
            if (y > box_rect.y + box_rect.h - text_pad) {
                break;
            }
            const auto [start, len] = lines[i];
            if (y + line_h >= box_rect.y + text_pad) {
                add_text(text.substr(start, len),
                         Rect{content_clip.x, y, content_w, line_h},
                         theme_.text, text_font, TextAlign::Left, &content_clip);
            }
            y += line_h;
        }
    }

    void progress(std::string_view label, float ratio, float height = 10.0f) {
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        const float label_h = std::clamp(height * 1.6f, 14.0f, 26.0f);
        const float text_gap = std::max(8.0f, label_h + 4.0f);
        const Rect rect = next_rect(height + text_gap);
        const float animated_ratio = update_motion_value(motion_id(label, rect, 0x1a2b3c4d5e6f7009ull), ratio, 9.0f);

        add_text(label, Rect{rect.x, rect.y, rect.w * 0.7f, label_h}, theme_.muted_text, label_h,
                 TextAlign::Left);

        char pct_text[32];
        std::snprintf(pct_text, sizeof(pct_text), "%.0f%%", ratio * 100.0f);
        add_text(pct_text, Rect{rect.x + rect.w * 0.7f, rect.y, rect.w * 0.3f, label_h}, theme_.text, label_h,
                 TextAlign::Right);

        const Rect track{rect.x, rect.y + text_gap, rect.w, std::max(4.0f, height)};
        const Rect fill{
            track.x + 1.0f,
            track.y + 1.0f,
            std::max(0.0f, track.w * animated_ratio - 2.0f),
            std::max(0.0f, track.h - 2.0f),
        };
        const float track_radius = track.h * 0.5f;
        add_filled_rect(track, theme_.track, track_radius);
        if (fill.w > 0.0f && fill.h > 0.0f) {
            const float fill_radius = std::max(
                0.0f, std::min(std::min(track_radius - 1.0f, fill.h * 0.5f), fill.w * 0.5f));
            add_soft_glow(fill, theme_.track_fill, fill_radius, std::min(1.0f, animated_ratio) * 0.22f, 4.5f);
            add_filled_rect(fill, theme_.track_fill, fill_radius);
        }
    }

    bool begin_dropdown(std::string_view label, bool& open, float body_height = 104.0f,
                        float padding = 10.0f) {
        const float header_height = std::max(34.0f, padding * 3.0f);
        const Rect header = next_rect(header_height);
        const bool hovered = header.contains(input_.mouse_x, input_.mouse_y);
        const std::uint64_t reveal_id = motion_id(label, header, 0x1a2b3c4d5e6f7013ull);
        if (hovered && input_.mouse_pressed) {
            open = !open;
            if (open) {
                MotionState& reveal_state = touch_motion_state(reveal_id);
                reveal_state.value = 0.0f;
                reveal_state.value_initialized = true;
            }
        }
        MotionState& motion =
            update_motion_state(motion_id(label, header, 0x1a2b3c4d5e6f700aull), hovered, hovered && input_.mouse_down,
                                false, open);
        Color fill = mix(theme_.panel, theme_.secondary_hover, motion.hover * 0.32f + motion.active * 0.08f);
        const Rect visual_header = header;
        add_soft_glow(visual_header, theme_.primary, theme_.radius, motion.active * 0.26f + motion.hover * 0.10f, 5.0f);
        add_filled_rect(visual_header, fill, theme_.radius);
        add_outline_rect(visual_header, mix(theme_.outline, theme_.focus_ring, motion.hover * 0.20f + motion.active * 0.18f),
                         theme_.radius, 1.0f + motion.hover * 0.10f);
        const float header_font = std::clamp(header.h * 0.38f, 13.0f, 24.0f);
        const float header_pad = std::clamp(header.h * 0.28f, 10.0f, 22.0f);
        const float indicator_size = std::clamp(header.h * 0.34f, 10.0f, 18.0f);
        add_text(label,
                 Rect{visual_header.x + header_pad, visual_header.y,
                      visual_header.w - header_pad * 2.0f - indicator_size - 6.0f, visual_header.h},
                 theme_.text, header_font, TextAlign::Left);
        const float reveal = std::clamp(update_motion_value(reveal_id, open ? 1.0f : 0.0f, open ? 12.0f : 16.0f),
                                        0.0f, 1.0f);
        const float reveal_alpha = 1.0f - (1.0f - reveal) * (1.0f - reveal);
        const Rect indicator_rect{
            visual_header.x + visual_header.w - header_pad - indicator_size,
            visual_header.y + (visual_header.h - indicator_size) * 0.5f,
            indicator_size,
            indicator_size,
        };
        add_chevron(indicator_rect, mix(theme_.muted_text, theme_.text, motion.active * 0.20f + motion.hover * 0.10f),
                    reveal * (3.1415926535f * 0.5f), std::clamp(header.h * 0.065f, 1.4f, 2.4f));
        if (!open) {
            return false;
        }

        flush_row();

        body_height = std::max(36.0f, body_height);
        padding = std::clamp(padding, 4.0f, 24.0f);

        const Rect body{content_x_, cursor_y_, content_width_, body_height};
        const float body_alpha = std::clamp(0.16f + reveal_alpha * 0.84f, 0.0f, 1.0f);
        const std::size_t fill_cmd_index = commands_.size();
        const GlowCommandRange body_glow =
            add_soft_glow_tracked(body, theme_.primary, theme_.radius, motion.active * 0.10f + reveal_alpha * 0.14f,
                                  6.0f);
        add_filled_rect(body,
                        scale_alpha(mix(theme_.panel, theme_.secondary, 0.35f), body_alpha),
                        theme_.radius);
        const std::size_t outline_cmd_index = commands_.size();
        add_outline_rect(body,
                         scale_alpha(mix(theme_.outline, theme_.focus_ring, motion.active * 0.16f),
                                     0.18f + reveal_alpha * 0.82f),
                         theme_.radius, 1.0f + motion.active * 0.12f);
        const bool had_outer_row = row_.active;
        const RowState outer_row = row_;
        const std::size_t content_cmd_begin = commands_.size();

        scope_stack_.push_back(ScopeState{
            ScopeKind::DropdownBody,
            content_x_,
            content_width_,
            0.0f,
            fill_cmd_index,
            outline_cmd_index,
            body.y,
            body_height,
            padding,
            had_outer_row,
            outer_row,
            false,
            -1,
            0u,
            Rect{},
            0.0f,
            false,
            reveal,
            body_glow.outer_cmd_index,
            body_glow.inner_cmd_index,
            body_glow.outer_spread,
            body_glow.inner_spread,
            content_cmd_begin,
            true,
        });
        content_x_ = body.x + padding;
        content_width_ = std::max(10.0f, body.w - 2.0f * padding);
        cursor_y_ = body.y + padding;
        row_ = RowState{};

        return true;
    }

    void end_dropdown() {
        flush_row();
        if (scope_stack_.empty()) {
            return;
        }
        restore_scope();
    }

    void begin_row(int columns, float gap = 8.0f) {
        flush_row();
        row_.active = true;
        row_.columns = std::max(1, columns);
        row_.index = 0;
        row_.next_span = 1;
        row_.gap = std::max(0.0f, gap);
        row_.y = cursor_y_;
        row_.max_height = 0.0f;
    }

    void begin_columns(int columns, float gap = 8.0f) {
        begin_row(columns, gap);
    }

    void begin_waterfall(int columns, float gap = 8.0f) {
        flush_row();
        waterfall_.active = true;
        waterfall_.columns = std::max(1, columns);
        waterfall_.gap = std::max(0.0f, gap);
        waterfall_.start_y = cursor_y_;
        const float total_gap = waterfall_.gap * static_cast<float>(waterfall_.columns - 1);
        waterfall_.item_width =
            std::max(10.0f, (content_width_ - total_gap) / static_cast<float>(waterfall_.columns));
        waterfall_.column_y.assign(static_cast<std::size_t>(waterfall_.columns), cursor_y_);
    }

    void end_row() {
        flush_row();
    }

    void end_columns() {
        end_row();
    }

    void end_waterfall() {
        if (!waterfall_.active) {
            return;
        }
        float max_y = waterfall_.start_y;
        for (float y : waterfall_.column_y) {
            max_y = std::max(max_y, y);
        }
        cursor_y_ = max_y;
        waterfall_ = WaterfallState{};
    }

    bool consume_clipboard_write(std::string& out_text) {
        if (!clipboard_write_pending_) {
            return false;
        }
        out_text = clipboard_write_text_;
        clipboard_write_text_.clear();
        clipboard_write_pending_ = false;
        return true;
    }

    bool consume_repaint_request() {
        const bool requested = repaint_requested_;
        repaint_requested_ = false;
        return requested;
    }

private:
    void refresh_theme() {
        theme_ = make_theme(theme_mode_, primary_color_);
        theme_.radius = corner_radius_;
    }

    enum class ScopeKind {
        Card,
        DropdownBody,
        ScrollArea,
    };

    struct RowState {
        bool active{false};
        int columns{1};
        int index{0};
        int next_span{1};
        float gap{8.0f};
        float y{0.0f};
        float max_height{0.0f};
    };

    struct WaterfallState {
        bool active{false};
        int columns{1};
        float gap{8.0f};
        float start_y{0.0f};
        float item_width{0.0f};
        std::vector<float> column_y{};
    };

    struct MotionState {
        float hover{0.0f};
        float press{0.0f};
        float focus{0.0f};
        float active{0.0f};
        float value{0.0f};
        bool initialized{false};
        bool value_initialized{false};
        std::uint64_t last_touched_frame{0u};
    };

    static constexpr std::size_t k_invalid_command_index = static_cast<std::size_t>(-1);

    struct GlowCommandRange {
        std::size_t outer_cmd_index{k_invalid_command_index};
        std::size_t inner_cmd_index{k_invalid_command_index};
        float outer_spread{0.0f};
        float inner_spread{0.0f};
    };

    struct ScopeState {
        ScopeKind kind{ScopeKind::Card};
        float content_x{0.0f};
        float content_width{0.0f};
        float cursor_y_after{0.0f};
        std::size_t fill_cmd_index{0};
        std::size_t outline_cmd_index{0};
        float top_y{0.0f};
        float min_height{0.0f};
        float padding{0.0f};
        bool had_outer_row{false};
        RowState outer_row{};
        bool in_waterfall{false};
        int column_index{-1};
        std::uint64_t scroll_state_id{0u};
        Rect scroll_viewport{};
        float scroll_content_origin_y{0.0f};
        bool pushed_clip{false};
        float reveal{1.0f};
        std::size_t glow_outer_cmd_index{k_invalid_command_index};
        std::size_t glow_inner_cmd_index{k_invalid_command_index};
        float glow_outer_spread{0.0f};
        float glow_inner_spread{0.0f};
        std::size_t content_cmd_begin{0u};
        bool show_content{true};
    };

    static std::uint64_t hash_sv(std::string_view text) {
        std::uint64_t value = 1469598103934665603ull;
        for (char ch : text) {
            value ^= static_cast<std::uint8_t>(ch);
            value *= 1099511628211ull;
        }
        return value;
    }

    static std::uint64_t hash_mix(std::uint64_t hash, std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
        return hash;
    }

    static std::uint32_t float_bits(float value) {
        std::uint32_t out = 0u;
        std::memcpy(&out, &value, sizeof(out));
        return out;
    }

    static std::uint64_t hash_rect(const Rect& rect) {
        std::uint64_t hash = 1469598103934665603ull;
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(rect.x)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(rect.y)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(rect.w)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(rect.h)));
        return hash;
    }

    static std::uint64_t hash_color(const Color& color) {
        std::uint64_t hash = 1469598103934665603ull;
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(color.r)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(color.g)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(color.b)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(color.a)));
        return hash;
    }

    static std::uint64_t hash_command_base(const DrawCommand& cmd) {
        std::uint64_t hash = 1469598103934665603ull;
        hash = hash_mix(hash, static_cast<std::uint64_t>(cmd.type));
        hash = hash_mix(hash, hash_rect(cmd.rect));
        hash = hash_mix(hash, hash_color(cmd.color));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(cmd.font_size)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(cmd.align));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(cmd.radius)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(cmd.thickness)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(float_bits(cmd.rotation)));
        hash = hash_mix(hash, static_cast<std::uint64_t>(cmd.has_clip ? 1u : 0u));
        if (cmd.has_clip) {
            hash = hash_mix(hash, hash_rect(cmd.clip_rect));
        }
        return hash;
    }

    std::uint64_t hash_command(const DrawCommand& cmd) const {
        std::uint64_t hash = hash_command_base(cmd);
        if (cmd.type == CommandType::Text &&
            static_cast<std::size_t>(cmd.text_offset) + static_cast<std::size_t>(cmd.text_length) <= text_arena_.size()) {
            const std::string_view text(text_arena_.data() + cmd.text_offset, cmd.text_length);
            hash = hash_mix(hash, hash_sv(text));
            hash = hash_mix(hash, static_cast<std::uint64_t>(cmd.text_length));
        }
        return hash;
    }

    std::uint64_t id_for(std::string_view label) const {
        const std::uint64_t key = hash_sv(label);
        return panel_id_seed_ ^ (key + 0x9e3779b97f4a7c15ull + (panel_id_seed_ << 6u) +
                                 (panel_id_seed_ >> 2u));
    }

    static bool parse_float_text(const std::string& text, float& out_value) {
        if (text.empty() || text == "-" || text == "." || text == "-.") {
            return false;
        }
        char* end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || (end != nullptr && *end != '\0') || !std::isfinite(parsed)) {
            return false;
        }
        out_value = parsed;
        return true;
    }

    static int resolve_decimals(float min_value, float max_value, int requested) {
        if (requested >= 0) {
            return std::clamp(requested, 0, 4);
        }
        const float span = std::fabs(max_value - min_value);
        if (span <= 1.0f) {
            return 2;
        }
        if (span <= 10.0f) {
            return 1;
        }
        return 0;
    }

    static std::string format_float(float value, int decimals) {
        decimals = std::clamp(decimals, 0, 4);
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
        return std::string(buffer);
    }

    static bool intersect_rects(const Rect& lhs, const Rect& rhs, Rect& out) {
        const float x1 = std::max(lhs.x, rhs.x);
        const float y1 = std::max(lhs.y, rhs.y);
        const float x2 = std::min(lhs.x + lhs.w, rhs.x + rhs.w);
        const float y2 = std::min(lhs.y + lhs.h, rhs.y + rhs.h);
        if (x2 <= x1 || y2 <= y1) {
            out = Rect{};
            return false;
        }
        out = Rect{x1, y1, x2 - x1, y2 - y1};
        return true;
    }

    static Rect expand_rect(const Rect& rect, float dx, float dy) {
        return Rect{
            rect.x - dx,
            rect.y - dy,
            rect.w + dx * 2.0f,
            rect.h + dy * 2.0f,
        };
    }

    static Rect translate_rect(const Rect& rect, float dx, float dy) {
        if (std::fabs(dx) < 0.15f && std::fabs(dy) < 0.15f) {
            return rect;
        }
        return Rect{
            rect.x + dx,
            rect.y + dy,
            rect.w,
            rect.h,
        };
    }

    static Rect scale_rect_from_center(const Rect& rect, float scale_x, float scale_y) {
        const float clamped_scale_x = std::max(0.1f, scale_x);
        const float clamped_scale_y = std::max(0.1f, scale_y);
        if (std::fabs(clamped_scale_x - 1.0f) < 0.0035f && std::fabs(clamped_scale_y - 1.0f) < 0.0035f) {
            return rect;
        }
        const float new_w = rect.w * clamped_scale_x;
        const float new_h = rect.h * clamped_scale_y;
        return Rect{
            rect.x + (rect.w - new_w) * 0.5f,
            rect.y + (rect.h - new_h) * 0.5f,
            new_w,
            new_h,
        };
    }

    static Color scale_alpha(const Color& color, float factor) {
        return Color{
            color.r,
            color.g,
            color.b,
            std::clamp(color.a * factor, 0.0f, 1.0f),
        };
    }

    static float snap_visual_motion(float value, float step = 1.0f / 48.0f) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        if (step <= 1e-6f) {
            return clamped;
        }
        if (clamped <= step * 0.5f) {
            return 0.0f;
        }
        if (clamped >= 1.0f - step * 0.5f) {
            return 1.0f;
        }
        return std::round(clamped / step) * step;
    }

    static bool near_value(float lhs, float rhs = 0.0f, float epsilon = 1e-3f) {
        return std::fabs(lhs - rhs) <= epsilon;
    }

    std::uint64_t motion_id(std::string_view label, const Rect& rect, std::uint64_t salt) const {
        return hash_mix(hash_mix(id_for(label), hash_rect(rect)), salt);
    }

    std::uint64_t motion_id(std::uint64_t base, std::uint64_t salt) const {
        return hash_mix(base, salt);
    }

    MotionState& touch_motion_state(std::uint64_t id) {
        MotionState& state = motion_states_[id];
        state.last_touched_frame = motion_frame_id_;
        return state;
    }

    float animate_motion_channel(float current, float target, float speed) {
        if (near_value(current, target, 1.5e-3f)) {
            return target;
        }
        const float blend = 1.0f - std::exp(-std::max(0.0f, speed) * ui_dt_);
        const float next = current + (target - current) * blend;
        if (!near_value(next, current, 3e-4f) || !near_value(next, target, 2.0e-3f)) {
            repaint_requested_ = true;
        }
        return near_value(next, target, 1.5e-3f) ? target : next;
    }

    MotionState& update_motion_state(std::uint64_t id, bool hovered, bool pressed, bool focused = false,
                                     bool active = false) {
        MotionState& state = touch_motion_state(id);
        const float hover_target = hovered ? 1.0f : 0.0f;
        const float press_target = pressed ? 1.0f : 0.0f;
        const float focus_target = focused ? 1.0f : 0.0f;
        const float active_target = active ? 1.0f : 0.0f;
        if (!state.initialized) {
            state.hover = hover_target;
            state.press = press_target;
            state.focus = focus_target;
            state.active = active_target;
            state.initialized = true;
            return state;
        }
        state.hover = animate_motion_channel(state.hover, hover_target, hovered ? 18.0f : 12.0f);
        state.press = animate_motion_channel(state.press, press_target, pressed ? 28.0f : 18.0f);
        state.focus = animate_motion_channel(state.focus, focus_target, focused ? 16.0f : 11.0f);
        state.active = animate_motion_channel(state.active, active_target, active ? 14.0f : 10.0f);
        return state;
    }

    float update_motion_value(std::uint64_t id, float target, float speed = 10.0f) {
        MotionState& state = touch_motion_state(id);
        if (!state.value_initialized) {
            state.value = target;
            state.value_initialized = true;
            return target;
        }
        state.value = animate_motion_channel(state.value, target, speed);
        return state.value;
    }

    void prune_motion_states() {
        for (auto it = motion_states_.begin(); it != motion_states_.end();) {
            const std::uint64_t age = motion_frame_id_ - it->second.last_touched_frame;
            const bool settled = near_value(it->second.hover) && near_value(it->second.press) &&
                                 near_value(it->second.focus) && near_value(it->second.active) &&
                                 (!it->second.value_initialized || near_value(it->second.value));
            if (age > 300u || (age > 96u && settled)) {
                it = motion_states_.erase(it);
            } else {
                ++it;
            }
        }
    }

    GlowCommandRange add_soft_glow_tracked(const Rect& rect, const Color& color, float radius, float intensity,
                                           float spread = 8.0f) {
        GlowCommandRange range{};
        const float glow = std::clamp(intensity, 0.0f, 1.0f);
        const float area = std::max(0.0f, rect.w) * std::max(0.0f, rect.h);
        const float inner_alpha = 0.08f * glow * color.a;
        const float outer_alpha = 0.035f * glow * color.a;
        if (glow <= 1e-3f || color.a <= 1e-3f || (inner_alpha < 0.010f && outer_alpha < 0.006f)) {
            return range;
        }
        const bool allow_outer = outer_alpha >= 0.010f && area < 22000.0f && spread >= 4.0f;
        range.inner_spread = spread * (0.30f + glow * 0.28f);
        range.outer_spread = spread * (0.72f + glow * 0.48f);
        if (allow_outer) {
            range.outer_cmd_index = commands_.size();
            add_filled_rect(expand_rect(rect, range.outer_spread, range.outer_spread),
                            scale_alpha(color, 0.030f * glow), radius + range.outer_spread);
        }
        range.inner_cmd_index = commands_.size();
        add_filled_rect(expand_rect(rect, range.inner_spread, range.inner_spread),
                        scale_alpha(color, 0.074f * glow), radius + range.inner_spread);
        return range;
    }

    void add_soft_glow(const Rect& rect, const Color& color, float radius, float intensity, float spread = 8.0f) {
        (void)add_soft_glow_tracked(rect, color, radius, intensity, spread);
    }

    void draw_input_chrome(std::uint64_t id, const Rect& rect, bool hovered, bool focused, const Color& base_fill,
                           float radius, float base_thickness = 1.0f) {
        MotionState& motion = update_motion_state(id, hovered, false, focused, focused);
        const float hover_v = snap_visual_motion(motion.hover, 1.0f / 40.0f);
        const float focus_v = snap_visual_motion(motion.focus, 1.0f / 40.0f);
        const float glow = focus_v * 0.78f + hover_v * 0.06f;
        if (glow > 0.045f) {
            add_soft_glow(rect, theme_.primary, radius, glow, 7.0f);
        }
        const Color fill =
            mix(base_fill, mix(theme_.secondary, theme_.primary, 0.14f), hover_v * 0.08f + focus_v * 0.10f);
        const Color border =
            mix(theme_.input_border, theme_.focus_ring, focus_v * 0.92f + hover_v * 0.24f);
        const float thickness = base_thickness + focus_v * 0.80f + hover_v * 0.06f;
        add_filled_rect(rect, fill, radius);
        add_outline_rect(rect, border, radius, thickness);
    }

    bool resolve_effective_clip(const Rect* requested_clip, Rect& out_clip, bool& has_clip) const {
        has_clip = false;
        if (requested_clip != nullptr) {
            out_clip = *requested_clip;
            has_clip = true;
        }
        if (!clip_stack_.empty()) {
            if (has_clip) {
                Rect merged{};
                if (!intersect_rects(out_clip, clip_stack_.back(), merged)) {
                    return false;
                }
                out_clip = merged;
            } else {
                out_clip = clip_stack_.back();
                has_clip = true;
            }
        }
        return true;
    }

    bool apply_clip_to_command(DrawCommand& cmd, const Rect* requested_clip = nullptr) const {
        Rect effective_clip{};
        bool has_clip = false;
        if (!resolve_effective_clip(requested_clip, effective_clip, has_clip)) {
            return false;
        }
        if (!has_clip) {
            return true;
        }
        Rect visible{};
        if (!intersect_rects(cmd.rect, effective_clip, visible)) {
            return false;
        }
        cmd.has_clip = true;
        cmd.clip_rect = effective_clip;
        return true;
    }

    void push_clip_rect(const Rect& rect) {
        Rect clipped = rect;
        if (!clip_stack_.empty()) {
            Rect merged{};
            if (intersect_rects(clipped, clip_stack_.back(), merged)) {
                clipped = merged;
            } else {
                clipped = Rect{};
            }
        }
        clip_stack_.push_back(clipped);
    }

    void pop_clip_rect() {
        if (!clip_stack_.empty()) {
            clip_stack_.pop_back();
        }
    }

    static std::size_t utf8_next_index(std::string_view text, std::size_t index) {
        if (index >= text.size()) {
            return text.size();
        }
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80u) {
            return index + 1u;
        }
        if ((lead >> 5u) == 0x6u && index + 1u < text.size()) {
            const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
            if ((b1 & 0xC0u) == 0x80u) {
                return index + 2u;
            }
        }
        if ((lead >> 4u) == 0xEu && index + 2u < text.size()) {
            const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
            const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
                return index + 3u;
            }
        }
        if ((lead >> 3u) == 0x1Eu && index + 3u < text.size()) {
            const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
            const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
            const unsigned char b3 = static_cast<unsigned char>(text[index + 3u]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
                return index + 4u;
            }
        }
        return index + 1u;
    }

    static std::size_t utf8_prev_index(std::string_view text, std::size_t index) {
        if (index == 0u || text.empty()) {
            return 0u;
        }
        std::size_t i = std::min(index, text.size());
        i -= 1u;
        while (i > 0u) {
            const unsigned char ch = static_cast<unsigned char>(text[i]);
            if ((ch & 0xC0u) != 0x80u) {
                break;
            }
            i -= 1u;
        }
        return i;
    }

    static bool is_private_use_codepoint_measure(std::uint32_t cp) {
        return cp >= 0xE000u && cp <= 0xF8FFu;
    }

#if defined(_WIN32) && defined(EUI_ENABLE_GLFW_OPENGL_BACKEND)
    static std::wstring utf8_to_wide_measure(std::string_view text) {
        if (text.empty()) {
            return {};
        }
        const int needed =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (needed <= 0) {
            std::wstring fallback;
            fallback.reserve(text.size());
            for (unsigned char ch : text) {
                fallback.push_back(static_cast<wchar_t>(ch));
            }
            return fallback;
        }
        std::wstring wide(static_cast<std::size_t>(needed), L'\0');
        const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                                static_cast<int>(text.size()), wide.data(), needed);
        if (written <= 0) {
            return {};
        }
        return wide;
    }

    HDC ensure_text_measure_hdc() const {
        if (text_measure_hdc_ == nullptr) {
            text_measure_hdc_ = CreateCompatibleDC(nullptr);
        }
        return text_measure_hdc_;
    }

    static int measure_codepoint_to_utf16(std::uint32_t cp, wchar_t out[2]) {
        if (cp <= 0xFFFFu) {
            out[0] = static_cast<wchar_t>(cp);
            out[1] = L'\0';
            return 1;
        }
        out[0] = L'\0';
        out[1] = L'\0';
        return 0;
    }

    HFONT ensure_text_measure_font(bool icon_font, int px) const {
        const int clamped_px = std::max(8, px);
        const std::string& family_name = icon_font ? text_measure_icon_font_family_ : text_measure_font_family_;
        const int font_weight = icon_font ? 400 : text_measure_font_weight_;
        const std::string key =
            std::string(icon_font ? "icon:" : "text:") + family_name + "#" + std::to_string(font_weight) + "#" +
            std::to_string(clamped_px);
        auto it = text_measure_fonts_.find(key);
        if (it != text_measure_fonts_.end()) {
            return it->second;
        }

        std::wstring family = utf8_to_wide_measure(family_name);
        if (family.empty()) {
            family = icon_font ? L"Font Awesome 7 Free Solid" : L"Segoe UI";
        }
        HFONT font = CreateFontW(-clamped_px, 0, 0, 0, font_weight, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, family.c_str());
        if (font == nullptr) {
            font = CreateFontW(-clamped_px, 0, 0, 0, font_weight, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, icon_font ? L"Segoe UI Symbol" : L"Segoe UI");
        }
        if (font == nullptr) {
            font = CreateFontW(-clamped_px, 0, 0, 0, font_weight, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        }
        text_measure_fonts_[key] = font;
        return font;
    }
#endif

    void release_text_measure_cache() const {
#if defined(_WIN32) && defined(EUI_ENABLE_GLFW_OPENGL_BACKEND)
        for (auto& pair : text_measure_fonts_) {
            if (pair.second != nullptr) {
                DeleteObject(pair.second);
            }
        }
        text_measure_fonts_.clear();
        if (text_measure_hdc_ != nullptr) {
            DeleteDC(text_measure_hdc_);
            text_measure_hdc_ = nullptr;
        }
#endif
    }

    void release_stb_measure_cache() const {
#if EUI_ENABLE_STB_TRUETYPE
        stb_measure_fonts_.clear();
#endif
    }

#if defined(_WIN32) && defined(EUI_ENABLE_GLFW_OPENGL_BACKEND)
    bool measure_font_has_glyph(HDC hdc, HFONT font, std::uint32_t cp) const {
        if (hdc == nullptr || font == nullptr) {
            return false;
        }
        wchar_t wide[2] = {L'\0', L'\0'};
        const int wide_len = measure_codepoint_to_utf16(cp, wide);
        if (wide_len <= 0) {
            return false;
        }
        HGDIOBJ old = SelectObject(hdc, font);
        WORD glyph_index = 0xFFFFu;
        const DWORD glyph_query = GetGlyphIndicesW(hdc, wide, wide_len, &glyph_index, GGI_MARK_NONEXISTING_GLYPHS);
        if (old != nullptr) {
            SelectObject(hdc, old);
        }
        return glyph_query != GDI_ERROR && glyph_index != 0xFFFFu;
    }
#endif

    float measure_text_width_runtime(std::uint32_t cp, float font_size) const {
#if defined(_WIN32) && defined(EUI_ENABLE_GLFW_OPENGL_BACKEND)
        HDC hdc = ensure_text_measure_hdc();
        if (hdc == nullptr) {
            return 0.0f;
        }

        const int text_font_px = std::max(8, static_cast<int>(std::round(font_size * 1.20f)));
        const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
        HFONT text_font = ensure_text_measure_font(false, text_font_px);
        HFONT icon_font = text_measure_enable_icon_fallback_ ? ensure_text_measure_font(true, icon_font_px) : nullptr;

        HFONT measure_font = text_font;
        const bool prefer_icon =
            text_measure_enable_icon_fallback_ && icon_font != nullptr && is_private_use_codepoint_measure(cp);
        if (prefer_icon && measure_font_has_glyph(hdc, icon_font, cp)) {
            measure_font = icon_font;
        } else if (text_font == nullptr || !measure_font_has_glyph(hdc, text_font, cp)) {
            if (text_measure_enable_icon_fallback_ && icon_font != nullptr &&
                measure_font_has_glyph(hdc, icon_font, cp)) {
                measure_font = icon_font;
            }
        }

        if (measure_font == nullptr) {
            return 0.0f;
        }

        wchar_t wide[2] = {L'\0', L'\0'};
        const int wide_len = measure_codepoint_to_utf16(cp, wide);
        if (wide_len <= 0) {
            return 0.0f;
        }

        HGDIOBJ old = SelectObject(hdc, measure_font);
        SIZE size{};
        if (GetTextExtentPoint32W(hdc, wide, wide_len, &size) == FALSE) {
            size.cx = 0;
        }
        if (old != nullptr) {
            SelectObject(hdc, old);
        }
        return std::max(0.0f, static_cast<float>(size.cx));
#else
        (void)cp;
        (void)font_size;
        return 0.0f;
#endif
    }

    static int quantize_text_measure_font_px(int px) {
        int clamped = std::max(8, px);
        if (clamped <= 32) {
            clamped = ((clamped + 1) / 2) * 2;
        } else if (clamped <= 72) {
            clamped = ((clamped + 2) / 4) * 4;
        } else {
            clamped = ((clamped + 4) / 8) * 8;
        }
        return std::clamp(clamped, 8, 200);
    }

    const StbMeasureFont* ensure_stb_measure_font(bool icon_font, int px) const {
#if EUI_ENABLE_STB_TRUETYPE
        const std::string& font_file = icon_font ? text_measure_icon_font_file_ : text_measure_font_file_;
        if (font_file.empty()) {
            return nullptr;
        }

        const int quantized_px = quantize_text_measure_font_px(px);
        const std::string key =
            std::string(icon_font ? "icon:" : "text:") + font_file + "#" + std::to_string(quantized_px);

        auto it = stb_measure_fonts_.find(key);
        if (it == stb_measure_fonts_.end()) {
            StbMeasureFont font{};
            font.data = read_file_bytes(font_file);
            if (!font.data.empty() && stbtt_InitFont(&font.info, font.data.data(), 0) != 0) {
                font.scale = stbtt_ScaleForPixelHeight(&font.info, static_cast<float>(quantized_px));
                font.valid = font.scale > 0.0f;
            }
            it = stb_measure_fonts_.emplace(key, std::move(font)).first;
        }
        return it->second.valid ? &it->second : nullptr;
#else
        (void)icon_font;
        (void)px;
        return nullptr;
#endif
    }

    float measure_text_width_stb(std::uint32_t cp, float font_size) const {
#if EUI_ENABLE_STB_TRUETYPE
        const int text_font_px = std::max(8, static_cast<int>(std::round(font_size * 1.20f)));
        const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
        const StbMeasureFont* text_font = ensure_stb_measure_font(false, text_font_px);
        const StbMeasureFont* icon_font =
            text_measure_enable_icon_fallback_ ? ensure_stb_measure_font(true, icon_font_px) : nullptr;

        auto glyph_index_for = [&](const StbMeasureFont* font) -> int {
            if (font == nullptr || !font->valid) {
                return 0;
            }
            return stbtt_FindGlyphIndex(&font->info, static_cast<int>(cp));
        };

        const StbMeasureFont* measure_font = text_font;
        int glyph_index = glyph_index_for(text_font);
        const bool prefer_icon =
            text_measure_enable_icon_fallback_ && icon_font != nullptr && is_private_use_codepoint_measure(cp);
        if (prefer_icon) {
            const int icon_glyph_index = glyph_index_for(icon_font);
            if (icon_glyph_index != 0) {
                measure_font = icon_font;
                glyph_index = icon_glyph_index;
            }
        } else if ((measure_font == nullptr || glyph_index == 0) && text_measure_enable_icon_fallback_) {
            const int icon_glyph_index = glyph_index_for(icon_font);
            if (icon_font != nullptr && icon_glyph_index != 0) {
                measure_font = icon_font;
                glyph_index = icon_glyph_index;
            }
        }

        if (measure_font == nullptr || !measure_font->valid) {
            return 0.0f;
        }
        if (glyph_index == 0 && cp != 0u) {
            glyph_index = stbtt_FindGlyphIndex(&measure_font->info, static_cast<int>('?'));
        }
        if (glyph_index == 0) {
            return 0.0f;
        }
        int advance = 0;
        int lsb = 0;
        stbtt_GetGlyphHMetrics(&measure_font->info, glyph_index, &advance, &lsb);
        (void)lsb;
        return std::max(0.0f, static_cast<float>(advance) * measure_font->scale);
#else
        (void)cp;
        (void)font_size;
        return 0.0f;
#endif
    }

    static std::vector<unsigned char> read_file_bytes(const std::string& path) {
        if (path.empty()) {
            return {};
        }
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            return {};
        }
        ifs.seekg(0, std::ios::end);
        const std::streamoff size = ifs.tellg();
        if (size <= 0) {
            return {};
        }
        ifs.seekg(0, std::ios::beg);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        ifs.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!ifs) {
            return {};
        }
        return bytes;
    }

    static float approx_char_width(float font_size) {
        return std::max(5.0f, font_size * 0.58f);
    }

    static bool decode_utf8_at(std::string_view text, std::size_t index, std::size_t limit, std::uint32_t& cp,
                               std::size_t& next) {
        cp = 0u;
        next = index;
        if (index >= text.size() || index >= limit) {
            return false;
        }

        const unsigned char lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80u) {
            cp = static_cast<std::uint32_t>(lead);
            next = index + 1u;
            return true;
        }

        if ((lead >> 5u) == 0x6u && index + 1u < limit) {
            const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
            if ((b1 & 0xC0u) == 0x80u) {
                cp = (static_cast<std::uint32_t>(lead & 0x1Fu) << 6u) |
                     static_cast<std::uint32_t>(b1 & 0x3Fu);
                next = index + 2u;
                return true;
            }
        }
        if ((lead >> 4u) == 0xEu && index + 2u < limit) {
            const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
            const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
                cp = (static_cast<std::uint32_t>(lead & 0x0Fu) << 12u) |
                     (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6u) |
                     static_cast<std::uint32_t>(b2 & 0x3Fu);
                next = index + 3u;
                return true;
            }
        }
        if ((lead >> 3u) == 0x1Eu && index + 3u < limit) {
            const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
            const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
            const unsigned char b3 = static_cast<unsigned char>(text[index + 3u]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
                cp = (static_cast<std::uint32_t>(lead & 0x07u) << 18u) |
                     (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12u) |
                     (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6u) |
                     static_cast<std::uint32_t>(b3 & 0x3Fu);
                next = index + 4u;
                return true;
            }
        }

        cp = static_cast<std::uint32_t>(lead);
        next = index + 1u;
        return false;
    }

    static float approx_codepoint_advance(std::uint32_t cp, float font_size) {
        constexpr float k_measure_correction = 0.92f;
        const float base = std::max(5.0f, font_size);
        if (cp <= 0x7Fu) {
            const char ch = static_cast<char>(cp);
            float factor = 0.52f;
            if (ch == ' ') {
                factor = 0.30f;
            } else if (std::strchr(".,:;'`!|", ch) != nullptr) {
                factor = 0.28f;
            } else if (std::strchr("()[]{}ilIjtfr", ch) != nullptr) {
                factor = 0.36f;
            } else if (std::strchr("mwMW@#%&", ch) != nullptr) {
                factor = 0.78f;
            } else if (ch >= '0' && ch <= '9') {
                factor = 0.52f;
            } else if (ch >= 'A' && ch <= 'Z') {
                factor = 0.58f;
            } else if (ch >= 'a' && ch <= 'z') {
                factor = 0.50f;
            } else {
                factor = 0.50f;
            }
            return std::max(3.0f, base * factor * k_measure_correction);
        }

        if ((cp >= 0x4E00u && cp <= 0x9FFFu) || (cp >= 0x3400u && cp <= 0x4DBFu) ||
            (cp >= 0xAC00u && cp <= 0xD7AFu) || (cp >= 0x3040u && cp <= 0x30FFu)) {
            return std::max(6.0f, base * 1.0f * k_measure_correction);
        }
        if (cp >= 0x1F300u && cp <= 0x1FAFFu) {
            return std::max(6.0f, base * 1.05f * k_measure_correction);
        }
        return approx_char_width(font_size) * k_measure_correction;
    }

    float codepoint_advance(std::string_view text, std::size_t start, std::size_t end, std::uint32_t cp,
                            float font_size) const {
        if (end <= start || start >= text.size()) {
            return 0.0f;
        }
        end = std::min(end, text.size());
        float measured = 0.0f;
        if (text_measure_backend_ == TextMeasureBackend::Win32) {
            measured = measure_text_width_runtime(cp, font_size);
        } else if (text_measure_backend_ == TextMeasureBackend::Stb) {
            measured = measure_text_width_stb(cp, font_size);
        }
        if (measured <= 0.0f) {
            measured = approx_codepoint_advance(cp, font_size);
        }
        return std::max(1.0f, measured);
    }

    float approx_text_width_until(std::string_view text, std::size_t byte_index, float font_size) const {
        const std::size_t target = std::min(byte_index, text.size());
        std::size_t index = 0u;
        float width = 0.0f;
        while (index < target) {
            std::size_t next = index;
            std::uint32_t cp = 0u;
            decode_utf8_at(text, index, target, cp, next);
            width += codepoint_advance(text, index, next, cp, font_size);
            index = next;
        }
        return width;
    }

    float resolve_input_origin_x(const Rect& input_rect, float font_size, bool align_right, float padding,
                                 std::string_view text, std::size_t caret_pos, bool follow_caret) {
        const float text_w = approx_text_width_until(text, text.size(), font_size);
        const float min_x = input_rect.x + padding;
        const float content_w = std::max(0.0f, input_rect.w - padding * 2.0f);
        if (content_w <= 0.0f) {
            input_scroll_x_ = 0.0f;
            return min_x;
        }

        if (align_right && text_w <= content_w) {
            input_scroll_x_ = 0.0f;
            return min_x + (content_w - text_w);
        }

        const float max_scroll = std::max(0.0f, text_w - content_w);
        if (follow_caret) {
            const float caret_w = approx_text_width_until(text, std::min(caret_pos, text.size()), font_size);
            if (caret_w < input_scroll_x_) {
                input_scroll_x_ = caret_w;
            }
            const float visible_w = std::max(1.0f, content_w - 1.0f);
            if (caret_w > input_scroll_x_ + visible_w) {
                input_scroll_x_ = caret_w - visible_w;
            }
        } else if (align_right && input_scroll_x_ <= 1e-4f && max_scroll > 0.0f) {
            // Right-aligned numeric edit fields start by showing the tail.
            input_scroll_x_ = max_scroll;
        }

        input_scroll_x_ = std::clamp(input_scroll_x_, 0.0f, max_scroll);
        return min_x - input_scroll_x_;
    }

    std::size_t caret_from_mouse_x(float origin_x, float font_size, std::string_view text, float mouse_x,
                                   float content_left, float content_right) const {
        if (mouse_x <= content_left) {
            return 0u;
        }
        if (mouse_x >= content_right - 1.0f) {
            return text.size();
        }
        const float rel_x = mouse_x - origin_x;
        if (rel_x <= 0.0f) {
            return 0u;
        }
        float pen_x = 0.0f;
        std::size_t index = 0u;
        while (index < text.size()) {
            std::size_t next = index;
            std::uint32_t cp = 0u;
            decode_utf8_at(text, index, text.size(), cp, next);
            const float advance = codepoint_advance(text, index, next, cp, font_size);
            if (rel_x < pen_x + advance * 0.5f) {
                return index;
            }
            pen_x += advance;
            if (rel_x < pen_x) {
                return next;
            }
            index = next;
        }
        return text.size();
    }

    bool has_selection() const {
        return selection_start_ != selection_end_;
    }

    std::size_t selection_left() const {
        return std::min(selection_start_, selection_end_);
    }

    std::size_t selection_right() const {
        return std::max(selection_start_, selection_end_);
    }

    void clear_selection() {
        selection_start_ = caret_pos_;
        selection_end_ = caret_pos_;
    }

    void request_clipboard_write(std::string text) {
        clipboard_write_text_ = std::move(text);
        clipboard_write_pending_ = true;
    }

    void ensure_edit_state_bounds() {
        const std::size_t size = input_buffer_.size();
        caret_pos_ = std::min(caret_pos_, size);
        selection_start_ = std::min(selection_start_, size);
        selection_end_ = std::min(selection_end_, size);
    }

    void set_caret(std::size_t pos, bool extend_selection) {
        const std::size_t clamped = std::min(pos, input_buffer_.size());
        if (extend_selection) {
            if (!has_selection()) {
                selection_start_ = caret_pos_;
            }
            caret_pos_ = clamped;
            selection_end_ = caret_pos_;
            return;
        }
        caret_pos_ = clamped;
        clear_selection();
    }

    void select_all_text() {
        caret_pos_ = input_buffer_.size();
        selection_start_ = 0u;
        selection_end_ = caret_pos_;
    }

    void erase_selection() {
        if (!has_selection()) {
            return;
        }
        const std::size_t left = selection_left();
        const std::size_t right = selection_right();
        if (right > left && right <= input_buffer_.size()) {
            input_buffer_.erase(left, right - left);
            caret_pos_ = left;
        }
        clear_selection();
    }

    void insert_at_caret(char ch) {
        erase_selection();
        input_buffer_.insert(input_buffer_.begin() + static_cast<std::ptrdiff_t>(caret_pos_), ch);
        caret_pos_ += 1u;
        clear_selection();
    }

    void insert_text_at_caret(std::string_view text) {
        if (text.empty()) {
            return;
        }
        erase_selection();
        input_buffer_.insert(caret_pos_, text.data(), text.size());
        caret_pos_ += text.size();
        clear_selection();
    }

    static std::string sanitize_paste_text(std::string_view text, bool multiline) {
        std::string out;
        out.reserve(text.size());
        for (std::size_t i = 0u; i < text.size(); ++i) {
            const char ch = text[i];
            if (ch == '\r') {
                if (multiline) {
                    out.push_back('\n');
                    if (i + 1u < text.size() && text[i + 1u] == '\n') {
                        i += 1u;
                    }
                }
                continue;
            }
            if (ch == '\n') {
                if (multiline) {
                    out.push_back('\n');
                }
                continue;
            }
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (uch < 0x20u && ch != '\t') {
                continue;
            }
            out.push_back(ch);
        }
        return out;
    }

    bool is_numeric_char_allowed(char ch, bool allow_negative, bool allow_decimal) const {
        if (ch >= '0' && ch <= '9') {
            return true;
        }
        if (allow_decimal && ch == '.') {
            return input_buffer_.find('.') == std::string::npos;
        }
        if (allow_negative && ch == '-') {
            return input_buffer_.find('-') == std::string::npos && caret_pos_ == 0u;
        }
        return false;
    }

    void insert_filtered_numeric_text(std::string_view text, bool allow_negative, bool allow_decimal) {
        for (char ch : text) {
            if (!is_numeric_char_allowed(ch, allow_negative, allow_decimal)) {
                continue;
            }

            if (ch == '.' && (input_buffer_.empty() || input_buffer_ == "-")) {
                if (input_buffer_ == "-") {
                    if (caret_pos_ == 0u) {
                        caret_pos_ = 1u;
                    }
                    insert_at_caret('0');
                } else if (input_buffer_.empty()) {
                    insert_at_caret('0');
                }
            }
            insert_at_caret(ch);
        }
    }

    void delete_backward() {
        if (has_selection()) {
            erase_selection();
            return;
        }
        if (caret_pos_ == 0u || input_buffer_.empty()) {
            return;
        }
        const std::size_t prev = utf8_prev_index(input_buffer_, caret_pos_);
        input_buffer_.erase(prev, caret_pos_ - prev);
        caret_pos_ = prev;
        clear_selection();
    }

    void delete_forward() {
        if (has_selection()) {
            erase_selection();
            return;
        }
        if (caret_pos_ >= input_buffer_.size()) {
            return;
        }
        const std::size_t next = utf8_next_index(input_buffer_, caret_pos_);
        input_buffer_.erase(caret_pos_, next - caret_pos_);
        clear_selection();
    }

    void move_caret_left(bool extend_selection) {
        if (!extend_selection && has_selection()) {
            set_caret(selection_left(), false);
            return;
        }
        if (caret_pos_ == 0u) {
            if (!extend_selection) {
                clear_selection();
            }
            return;
        }
        set_caret(utf8_prev_index(input_buffer_, caret_pos_), extend_selection);
    }

    void move_caret_right(bool extend_selection) {
        if (!extend_selection && has_selection()) {
            set_caret(selection_right(), false);
            return;
        }
        if (caret_pos_ >= input_buffer_.size()) {
            if (!extend_selection) {
                clear_selection();
            }
            return;
        }
        set_caret(utf8_next_index(input_buffer_, caret_pos_), extend_selection);
    }

    void move_caret_home(bool extend_selection) {
        set_caret(0u, extend_selection);
    }

    void move_caret_end(bool extend_selection) {
        set_caret(input_buffer_.size(), extend_selection);
    }

    void start_text_input(std::uint64_t id, const std::string& initial_value, bool select_all = true) {
        if (active_input_id_ == id) {
            return;
        }
        active_input_id_ = id;
        input_buffer_ = initial_value;
        caret_pos_ = input_buffer_.size();
        selection_start_ = caret_pos_;
        selection_end_ = caret_pos_;
        if (select_all) {
            select_all_text();
        }
        drag_selecting_ = false;
        input_scroll_x_ = 0.0f;
    }

    bool is_text_input_active(std::uint64_t id) const {
        return active_input_id_ == id;
    }

    void stop_text_input() {
        active_input_id_ = 0u;
        input_buffer_.clear();
        caret_pos_ = 0u;
        selection_start_ = 0u;
        selection_end_ = 0u;
        drag_selecting_ = false;
        input_scroll_x_ = 0.0f;
    }

    void update_mouse_selection(const Rect& input_rect, float font_size, bool align_right, float padding,
                                bool hovered) {
        const float origin_x =
            resolve_input_origin_x(input_rect, font_size, align_right, padding, input_buffer_, caret_pos_, false);
        const float content_left = input_rect.x + padding;
        const float content_right = input_rect.x + input_rect.w - padding;
        if (hovered && input_.mouse_pressed) {
            const std::size_t caret = caret_from_mouse_x(origin_x, font_size, input_buffer_, input_.mouse_x,
                                                         content_left, content_right);
            caret_pos_ = caret;
            selection_start_ = caret;
            selection_end_ = caret;
            drag_anchor_ = caret;
            drag_selecting_ = true;
        }

        if (drag_selecting_ && input_.mouse_down) {
            const std::size_t caret = caret_from_mouse_x(origin_x, font_size, input_buffer_, input_.mouse_x,
                                                         content_left, content_right);
            caret_pos_ = caret;
            selection_start_ = drag_anchor_;
            selection_end_ = caret;
        }

        if (input_.mouse_released) {
            drag_selecting_ = false;
        }
    }

    void consume_numeric_typing(bool allow_negative, bool allow_decimal) {
        const bool extend_selection = input_.key_shift;

        if (input_.key_select_all) {
            select_all_text();
        }
        if (input_.key_copy && has_selection()) {
            request_clipboard_write(input_buffer_.substr(selection_left(), selection_right() - selection_left()));
        }
        if (input_.key_cut && has_selection()) {
            request_clipboard_write(input_buffer_.substr(selection_left(), selection_right() - selection_left()));
            erase_selection();
        }
        if (input_.key_paste) {
            erase_selection();
            insert_filtered_numeric_text(input_.clipboard_text, allow_negative, allow_decimal);
        }

        if (input_.key_home) {
            move_caret_home(extend_selection);
        }
        if (input_.key_end) {
            move_caret_end(extend_selection);
        }
        if (input_.key_left) {
            move_caret_left(extend_selection);
        }
        if (input_.key_right) {
            move_caret_right(extend_selection);
        }
        if (input_.key_backspace) {
            delete_backward();
        }
        if (input_.key_delete) {
            delete_forward();
        }

        if (!input_.text_input.empty()) {
            erase_selection();
            for (char ch : input_.text_input) {
                if (is_numeric_char_allowed(ch, allow_negative, allow_decimal)) {
                    if (ch == '.' && (input_buffer_.empty() || input_buffer_ == "-")) {
                        if (input_buffer_ == "-") {
                            insert_at_caret('0');
                        } else if (input_buffer_.empty()) {
                            insert_at_caret('0');
                        }
                    }
                    insert_at_caret(ch);
                }
            }
        }
        ensure_edit_state_bounds();
    }

    void consume_plain_typing(bool multiline) {
        const bool extend_selection = input_.key_shift;

        if (input_.key_select_all) {
            select_all_text();
        }
        if (input_.key_copy && has_selection()) {
            request_clipboard_write(input_buffer_.substr(selection_left(), selection_right() - selection_left()));
        }
        if (input_.key_cut && has_selection()) {
            request_clipboard_write(input_buffer_.substr(selection_left(), selection_right() - selection_left()));
            erase_selection();
        }
        if (input_.key_paste) {
            const std::string pasted = sanitize_paste_text(input_.clipboard_text, multiline);
            if (!pasted.empty()) {
                insert_text_at_caret(pasted);
            }
        }

        if (input_.key_home) {
            move_caret_home(extend_selection);
        }
        if (input_.key_end) {
            move_caret_end(extend_selection);
        }
        if (input_.key_left) {
            move_caret_left(extend_selection);
        }
        if (input_.key_right) {
            move_caret_right(extend_selection);
        }
        if (input_.key_backspace) {
            delete_backward();
        }
        if (input_.key_delete) {
            delete_forward();
        }

        if (multiline && input_.key_enter) {
            insert_at_caret('\n');
        }

        if (!input_.text_input.empty()) {
            insert_text_at_caret(sanitize_paste_text(input_.text_input, multiline));
        }
        ensure_edit_state_bounds();
    }

    bool commit_text_input(float& value, float min_value, float max_value) {
        float parsed = value;
        const bool valid = parse_float_text(input_buffer_, parsed);
        if (valid) {
            parsed = std::clamp(parsed, min_value, max_value);
        }
        stop_text_input();
        if (!valid) {
            return false;
        }
        if (std::fabs(parsed - value) <= 1e-6f) {
            return false;
        }
        value = parsed;
        return true;
    }

    void clamp_live_numeric_buffer(float min_value, float max_value, int decimals) {
        if (input_buffer_.empty() || input_buffer_ == "-" || input_buffer_ == "." ||
            input_buffer_ == "-.") {
            return;
        }

        if (input_buffer_.size() > 24u) {
            input_buffer_.resize(24u);
        }

        decimals = std::clamp(decimals, 0, 6);
        const std::size_t dot_pos = input_buffer_.find('.');
        if (dot_pos != std::string::npos && decimals >= 0) {
            const std::size_t max_len = dot_pos + 1u + static_cast<std::size_t>(decimals);
            if (input_buffer_.size() > max_len) {
                input_buffer_.resize(max_len);
            }
        }

        float parsed = 0.0f;
        if (!parse_float_text(input_buffer_, parsed)) {
            return;
        }

        const float clamped = std::clamp(parsed, min_value, max_value);
        if (std::fabs(clamped - parsed) > 1e-6f) {
            input_buffer_ = format_float(clamped, decimals);
            caret_pos_ = input_buffer_.size();
            clear_selection();
        }
        ensure_edit_state_bounds();
    }

    bool should_show_caret() const {
        return true;
    }

    void draw_static_input_content(const Rect& input_rect, std::string_view text, float font_size, bool align_right,
                                   float padding, const Color& text_color) {
        const Rect content_clip{
            input_rect.x + padding,
            input_rect.y + 2.0f,
            std::max(0.0f, input_rect.w - padding * 2.0f),
            std::max(0.0f, input_rect.h - 4.0f),
        };
        const float text_w = approx_text_width_until(text, text.size(), font_size);
        float origin_x = content_clip.x;
        if (align_right) {
            origin_x += content_clip.w - text_w;
        }
        add_text(text,
                 Rect{origin_x, input_rect.y, std::max(content_clip.w, text_w), input_rect.h},
                 text_color, font_size, TextAlign::Left, &content_clip);
    }

    void draw_text_input_content(const Rect& input_rect, float font_size, bool align_right, float padding,
                                 const Color& text_color, const Color& selection_color) {
        const std::string_view text_view = std::string_view(input_buffer_);
        const float origin_x =
            resolve_input_origin_x(input_rect, font_size, align_right, padding, text_view, caret_pos_, true);
        const float text_w = approx_text_width_until(text_view, text_view.size(), font_size);
        const Rect content_clip{
            input_rect.x + padding,
            input_rect.y + 2.0f,
            std::max(0.0f, input_rect.w - padding * 2.0f),
            std::max(0.0f, input_rect.h - 4.0f),
        };

        if (has_selection()) {
            const float sel_l = approx_text_width_until(text_view, selection_left(), font_size);
            const float sel_r = approx_text_width_until(text_view, selection_right(), font_size);
            const float selection_pad_x = std::max(1.0f, font_size * 0.08f);
            const Rect selection_rect{
                origin_x + sel_l - selection_pad_x,
                input_rect.y + 2.0f,
                std::max(0.0f, sel_r - sel_l) + selection_pad_x * 2.0f,
                std::max(2.0f, input_rect.h - 4.0f),
            };
            if (selection_rect.w > 0.0f) {
                Rect clipped{};
                if (intersect_rects(selection_rect, content_clip, clipped)) {
                    add_filled_rect(clipped, selection_color, 2.0f);
                }
            }
        }

        add_text(text_view, Rect{origin_x, input_rect.y, std::max(content_clip.w, text_w), input_rect.h},
                 text_color, font_size, TextAlign::Left, &content_clip);

        if (should_show_caret()) {
            const float caret_x = approx_text_width_until(text_view, caret_pos_, font_size);
            const Rect caret_rect{
                origin_x + caret_x,
                input_rect.y + 4.0f,
                1.3f,
                std::max(2.0f, input_rect.h - 8.0f),
            };
            Rect clipped{};
            if (intersect_rects(caret_rect, content_clip, clipped)) {
                add_filled_rect(clipped, text_color, 0.0f);
            }
        }
    }

    void restore_scope() {
        const ScopeState scope = scope_stack_.back();
        scope_stack_.pop_back();

        if (scope.kind == ScopeKind::Card || scope.kind == ScopeKind::DropdownBody) {
            if (scope.pushed_clip) {
                pop_clip_rect();
            }
            const float needed_height = std::max(0.0f, (cursor_y_ - scope.top_y) + scope.padding);
            const float final_height = std::max(scope.min_height, needed_height);
            const float display_height = final_height;
            const bool animate_dropdown = scope.kind == ScopeKind::DropdownBody;
            const float reveal = animate_dropdown ? std::clamp(scope.reveal, 0.0f, 1.0f) : 1.0f;
            const float reveal_alpha =
                animate_dropdown ? (1.0f - (1.0f - reveal) * (1.0f - reveal)) : 1.0f;
            const float shell_offset_y = animate_dropdown ? (1.0f - reveal) * 8.0f : 0.0f;
            const float content_offset_y = animate_dropdown ? (1.0f - reveal) * 10.0f : 0.0f;
            const float content_alpha = animate_dropdown ? std::clamp(0.08f + reveal_alpha * 0.92f, 0.0f, 1.0f)
                                                         : 1.0f;
            if (scope.fill_cmd_index < commands_.size()) {
                commands_[scope.fill_cmd_index].rect.y = scope.top_y + shell_offset_y;
                commands_[scope.fill_cmd_index].rect.h = display_height;
                commands_[scope.fill_cmd_index].hash = hash_command(commands_[scope.fill_cmd_index]);
            }
            if (scope.outline_cmd_index < commands_.size()) {
                commands_[scope.outline_cmd_index].rect.y = scope.top_y + shell_offset_y;
                commands_[scope.outline_cmd_index].rect.h = display_height;
                commands_[scope.outline_cmd_index].hash = hash_command(commands_[scope.outline_cmd_index]);
            }
            if (scope.glow_outer_cmd_index != k_invalid_command_index && scope.glow_outer_cmd_index < commands_.size()) {
                commands_[scope.glow_outer_cmd_index].rect.y =
                    scope.top_y + shell_offset_y - scope.glow_outer_spread;
                commands_[scope.glow_outer_cmd_index].rect.h = display_height + scope.glow_outer_spread * 2.0f;
                commands_[scope.glow_outer_cmd_index].hash = hash_command(commands_[scope.glow_outer_cmd_index]);
            }
            if (scope.glow_inner_cmd_index != k_invalid_command_index && scope.glow_inner_cmd_index < commands_.size()) {
                commands_[scope.glow_inner_cmd_index].rect.y =
                    scope.top_y + shell_offset_y - scope.glow_inner_spread;
                commands_[scope.glow_inner_cmd_index].rect.h = display_height + scope.glow_inner_spread * 2.0f;
                commands_[scope.glow_inner_cmd_index].hash = hash_command(commands_[scope.glow_inner_cmd_index]);
            }
            if (animate_dropdown && (content_offset_y > 1e-3f || content_alpha < 0.999f) &&
                scope.content_cmd_begin < commands_.size()) {
                for (std::size_t i = scope.content_cmd_begin; i < commands_.size(); ++i) {
                    DrawCommand& cmd = commands_[i];
                    cmd.rect.y += content_offset_y;
                    cmd.color = scale_alpha(cmd.color, content_alpha);
                    if (cmd.has_clip) {
                        cmd.clip_rect.y += content_offset_y;
                    }
                    cmd.hash = hash_command(cmd);
                }
            }
            content_x_ = scope.content_x;
            content_width_ = scope.content_width;
            if (scope.in_waterfall && waterfall_.active && scope.column_index >= 0 &&
                scope.column_index < static_cast<int>(waterfall_.column_y.size())) {
                waterfall_.column_y[static_cast<std::size_t>(scope.column_index)] =
                    scope.top_y + display_height + item_spacing_;
                cursor_y_ = waterfall_.start_y;
            } else if (scope.had_outer_row) {
                row_ = scope.outer_row;
                row_.max_height = std::max(row_.max_height, display_height);
                cursor_y_ = row_.y;
                if (row_.index >= row_.columns) {
                    flush_row();
                }
            } else {
                cursor_y_ = scope.top_y + display_height + item_spacing_;
            }
            return;
        }

        if (scope.kind == ScopeKind::ScrollArea) {
            if (scope.pushed_clip) {
                pop_clip_rect();
            }

            auto it = scroll_area_state_.find(scope.scroll_state_id);
            if (it != scroll_area_state_.end()) {
                const float measured_height =
                    std::max(0.0f, cursor_y_ - scope.scroll_content_origin_y - item_spacing_);
                it->second.content_height = std::max(scope.scroll_viewport.h, measured_height);
                const float max_scroll =
                    std::max(0.0f, it->second.content_height - scope.scroll_viewport.h);
                const float hard_bound = std::max(24.0f, scope.scroll_viewport.h);
                it->second.scroll = std::clamp(it->second.scroll, -hard_bound, max_scroll + hard_bound);
            }

            content_x_ = scope.content_x;
            content_width_ = scope.content_width;
            if (scope.had_outer_row) {
                row_ = scope.outer_row;
                row_.max_height = std::max(row_.max_height, scope.min_height);
                cursor_y_ = row_.y;
                if (row_.index >= row_.columns) {
                    flush_row();
                }
            } else {
                cursor_y_ = scope.top_y + scope.min_height;
            }
            return;
        }

        content_x_ = scope.content_x;
        content_width_ = scope.content_width;
        cursor_y_ = scope.cursor_y_after;
    }

    void flush_row() {
        if (!row_.active) {
            return;
        }
        if (row_.index > 0) {
            cursor_y_ = row_.y + row_.max_height + item_spacing_;
        } else {
            cursor_y_ = row_.y;
        }
        row_ = RowState{};
    }

    Rect next_rect(float height) {
        height = std::max(2.0f, height);

        if (!row_.active) {
            Rect rect{content_x_, cursor_y_, content_width_, height};
            cursor_y_ += height + item_spacing_;
            return rect;
        }

        if (row_.index >= row_.columns) {
            row_.y += row_.max_height + item_spacing_;
            row_.index = 0;
            row_.max_height = 0.0f;
        }

        const float total_gap = row_.gap * static_cast<float>(row_.columns - 1);
        const float item_width =
            std::max(10.0f, (content_width_ - total_gap) / static_cast<float>(row_.columns));
        const int remaining_columns = std::max(1, row_.columns - row_.index);
        const int span = std::clamp(row_.next_span, 1, remaining_columns);
        row_.next_span = 1;
        Rect rect{
            content_x_ + static_cast<float>(row_.index) * (item_width + row_.gap),
            row_.y,
            item_width * static_cast<float>(span) + row_.gap * static_cast<float>(span - 1),
            height,
        };

        row_.max_height = std::max(row_.max_height, height);
        row_.index += span;

        return rect;
    }

    void add_filled_rect(const Rect& rect, const Color& color, float radius = 0.0f) {
        DrawCommand cmd;
        cmd.type = CommandType::FilledRect;
        cmd.rect = rect;
        cmd.color = scale_alpha(color, global_alpha_);
        if (cmd.color.a <= 1e-4f) {
            return;
        }
        cmd.radius = std::max(0.0f, radius);
        if (!apply_clip_to_command(cmd, nullptr)) {
            return;
        }
        cmd.hash = hash_command_base(cmd);
        commands_.push_back(std::move(cmd));
    }

    void add_outline_rect(const Rect& rect, const Color& color, float radius = 0.0f,
                          float thickness = 1.0f) {
        DrawCommand cmd;
        cmd.type = CommandType::RectOutline;
        cmd.rect = rect;
        cmd.color = scale_alpha(color, global_alpha_);
        if (cmd.color.a <= 1e-4f) {
            return;
        }
        cmd.radius = std::max(0.0f, radius);
        cmd.thickness = std::max(1.0f, thickness);
        if (!apply_clip_to_command(cmd, nullptr)) {
            return;
        }
        cmd.hash = hash_command_base(cmd);
        commands_.push_back(std::move(cmd));
    }

    void add_text(std::string_view text, const Rect& rect, const Color& color, float font_size,
                  TextAlign align, const Rect* clip_rect = nullptr) {
        DrawCommand cmd;
        cmd.type = CommandType::Text;
        cmd.rect = rect;
        cmd.color = scale_alpha(color, global_alpha_);
        if (cmd.color.a <= 1e-4f || text.empty()) {
            return;
        }
        cmd.font_size = std::max(8.0f, font_size);
        cmd.align = align;
        if (!apply_clip_to_command(cmd, clip_rect)) {
            return;
        }

        const std::uint32_t offset = static_cast<std::uint32_t>(text_arena_.size());
        const std::uint32_t length = static_cast<std::uint32_t>(text.size());
        text_arena_.insert(text_arena_.end(), text.begin(), text.end());
        cmd.text_offset = offset;
        cmd.text_length = length;
        cmd.hash = hash_mix(hash_command_base(cmd), hash_sv(text));
        cmd.hash = hash_mix(cmd.hash, static_cast<std::uint64_t>(length));
        commands_.push_back(std::move(cmd));
    }

    void add_chevron(const Rect& rect, const Color& color, float rotation, float thickness = 1.8f) {
        DrawCommand cmd;
        cmd.type = CommandType::Chevron;
        cmd.rect = rect;
        cmd.color = scale_alpha(color, global_alpha_);
        if (cmd.color.a <= 1e-4f) {
            return;
        }
        cmd.rotation = rotation;
        cmd.thickness = std::max(1.0f, thickness);
        if (!apply_clip_to_command(cmd, nullptr)) {
            return;
        }
        cmd.hash = hash_command_base(cmd);
        commands_.push_back(std::move(cmd));
    }

    float frame_width_{1280.0f};
    float frame_height_{720.0f};
    InputState input_{};

    ThemeMode theme_mode_{ThemeMode::Dark};
    Color primary_color_{0.25f, 0.55f, 1.0f, 1.0f};
    float corner_radius_{8.0f};
    Theme theme_{make_theme(theme_mode_, primary_color_)};

    std::vector<DrawCommand> commands_{};
    std::vector<ScopeState> scope_stack_{};
    std::vector<char> text_arena_{};

    RowState row_{};
    WaterfallState waterfall_{};
    float content_x_{16.0f};
    float content_width_{800.0f};
    float cursor_y_{16.0f};
    float item_spacing_{8.0f};

    std::uint64_t panel_id_seed_{1469598103934665603ull};
    std::uint64_t active_slider_id_{0};
    std::uint64_t active_input_id_{0};
    std::uint64_t active_textarea_scroll_id_{0};
    std::uint64_t active_scroll_drag_id_{0};
    std::uint64_t active_scrollbar_drag_id_{0};
    std::string input_buffer_{};
    float active_textarea_drag_offset_{0.0f};
    float active_scroll_drag_last_y_{0.0f};
    float active_scrollbar_drag_offset_{0.0f};
    double prev_input_time_{0.0};
    bool has_prev_input_time_{false};
    float ui_dt_{1.0f / 60.0f};
    std::size_t caret_pos_{0u};
    std::size_t selection_start_{0u};
    std::size_t selection_end_{0u};
    std::size_t drag_anchor_{0u};
    float input_scroll_x_{0.0f};
    bool drag_selecting_{false};
#if defined(_WIN32) && defined(EUI_ENABLE_GLFW_OPENGL_BACKEND)
    mutable HDC text_measure_hdc_{nullptr};
    mutable std::unordered_map<std::string, HFONT> text_measure_fonts_{};
#endif
    TextMeasureBackend text_measure_backend_{TextMeasureBackend::Approx};
    std::string text_measure_font_family_{"Segoe UI"};
    int text_measure_font_weight_{600};
    std::string text_measure_font_file_{};
    std::string text_measure_icon_font_family_{"Font Awesome 7 Free Solid"};
    std::string text_measure_icon_font_file_{};
    bool text_measure_enable_icon_fallback_{true};
#if EUI_ENABLE_STB_TRUETYPE
    struct StbMeasureFont {
        std::vector<unsigned char> data{};
        stbtt_fontinfo info{};
        float scale{0.0f};
        bool valid{false};
    };
    mutable std::unordered_map<std::string, StbMeasureFont> stb_measure_fonts_{};
#endif
    bool repaint_requested_{false};
    float global_alpha_{1.0f};
    bool clipboard_write_pending_{false};
    std::string clipboard_write_text_{};
    std::unordered_map<std::uint64_t, TextAreaState> text_area_state_{};
    std::unordered_map<std::uint64_t, ScrollAreaState> scroll_area_state_{};
    std::vector<ScrollInputRegion> prev_scroll_input_regions_{};
    std::vector<ScrollInputRegion> scroll_input_regions_{};
    ScrollInputTarget wheel_target_{};
    ScrollInputTarget press_target_{};
    std::unordered_map<std::uint64_t, MotionState> motion_states_{};
    std::uint64_t motion_frame_id_{0u};
    std::vector<Rect> clip_stack_{};
    Rect panel_rect_{};
};

#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND

namespace demo {

struct AppOptions {
    enum class TextBackend {
        Auto,
        Stb,
        Win32,
    };

    int width{1150};
    int height{820};
    const char* title{"EUI Demo"};
    bool vsync{true};
    bool continuous_render{false};
    double idle_wait_seconds{0.25};
    double max_fps{60.0};
    const char* text_font_family{"Segoe UI"};
    int text_font_weight{600};
    const char* text_font_file{nullptr};
    const char* icon_font_family{"Font Awesome 7 Free Solid"};
    const char* icon_font_file{"include/Font Awesome 7 Free-Solid-900.otf"};
    bool enable_icon_font_fallback{true};
    TextBackend text_backend{TextBackend::Auto};
};

struct FrameContext {
    Context& ui;
    GLFWwindow* window{nullptr};
    float dt{0.0f};
    int framebuffer_w{1};
    int framebuffer_h{1};
    int window_w{1};
    int window_h{1};
    float dpi_scale_x{1.0f};
    float dpi_scale_y{1.0f};
    float dpi_scale{1.0f};
    bool* repaint_flag{nullptr};

    void request_next_frame() const {
        if (repaint_flag != nullptr) {
            *repaint_flag = true;
        }
    }
};

namespace detail {

using Glyph = std::array<std::uint8_t, 7>;
using Point = std::array<float, 2>;
using TextureId = unsigned int;

struct RuntimeState {
    std::string text_input{};
    double scroll_y_accum{0.0};
    bool pending_backspace{false};
    bool pending_delete{false};
    bool pending_enter{false};
    bool pending_escape{false};
    bool pending_left{false};
    bool pending_right{false};
    bool pending_up{false};
    bool pending_down{false};
    bool pending_home{false};
    bool pending_end{false};
    bool prev_left_mouse{false};
    bool prev_right_mouse{false};
    bool prev_a_key{false};
    bool prev_c_key{false};
    bool prev_v_key{false};
    bool prev_x_key{false};
    double prev_mouse_x{0.0};
    double prev_mouse_y{0.0};
    bool has_prev_mouse{false};
    int prev_framebuffer_w{0};
    int prev_framebuffer_h{0};

    std::vector<DrawCommand> curr_commands{};
    std::vector<char> curr_text_arena{};
    std::vector<Rect> dirty_regions{};
    std::vector<DrawCommand> prev_commands{};
    std::vector<char> prev_text_arena{};
    Color prev_bg{};
    std::uint64_t prev_frame_hash{0ull};
    bool has_prev_frame{false};

    TextureId cache_texture{0};
    int cache_w{0};
    int cache_h{0};
    bool has_cache{false};
};

inline void text_input_callback(GLFWwindow* window, unsigned int codepoint) {
    RuntimeState* state = static_cast<RuntimeState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }
    if (codepoint < 32u && codepoint != '\t') {
        return;
    }
    if (codepoint <= 0x7Fu) {
        state->text_input.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFu) {
        state->text_input.push_back(static_cast<char>(0xC0u | ((codepoint >> 6u) & 0x1Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0xFFFFu) {
        state->text_input.push_back(static_cast<char>(0xE0u | ((codepoint >> 12u) & 0x0Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0x10FFFFu) {
        state->text_input.push_back(static_cast<char>(0xF0u | ((codepoint >> 18u) & 0x07u)));
        state->text_input.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    }
}

inline void key_input_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }
    RuntimeState* state = static_cast<RuntimeState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }

    switch (key) {
        case GLFW_KEY_BACKSPACE:
            state->pending_backspace = true;
            break;
        case GLFW_KEY_DELETE:
            state->pending_delete = true;
            break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            state->pending_enter = true;
            break;
        case GLFW_KEY_ESCAPE:
            state->pending_escape = true;
            break;
        case GLFW_KEY_LEFT:
            state->pending_left = true;
            break;
        case GLFW_KEY_RIGHT:
            state->pending_right = true;
            break;
        case GLFW_KEY_UP:
            state->pending_up = true;
            break;
        case GLFW_KEY_DOWN:
            state->pending_down = true;
            break;
        case GLFW_KEY_HOME:
            state->pending_home = true;
            break;
        case GLFW_KEY_END:
            state->pending_end = true;
            break;
        default:
            break;
    }
}

inline void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    RuntimeState* state = static_cast<RuntimeState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }
    state->scroll_y_accum += yoffset;
}

inline bool float_eq(float lhs, float rhs, float eps = 1e-5f) {
    return std::fabs(lhs - rhs) <= eps;
}

inline bool color_eq(const Color& lhs, const Color& rhs, float eps = 1e-4f) {
    return float_eq(lhs.r, rhs.r, eps) && float_eq(lhs.g, rhs.g, eps) &&
           float_eq(lhs.b, rhs.b, eps) && float_eq(lhs.a, rhs.a, eps);
}

inline bool rect_eq(const Rect& lhs, const Rect& rhs, float eps = 0.01f) {
    return float_eq(lhs.x, rhs.x, eps) && float_eq(lhs.y, rhs.y, eps) &&
           float_eq(lhs.w, rhs.w, eps) && float_eq(lhs.h, rhs.h, eps);
}

inline bool rect_intersects(const Rect& lhs, const Rect& rhs) {
    if (lhs.w <= 0.0f || lhs.h <= 0.0f || rhs.w <= 0.0f || rhs.h <= 0.0f) {
        return false;
    }
    return lhs.x < rhs.x + rhs.w && lhs.x + lhs.w > rhs.x && lhs.y < rhs.y + rhs.h &&
           lhs.y + lhs.h > rhs.y;
}

inline bool rect_intersection(const Rect& lhs, const Rect& rhs, Rect& out) {
    const float x1 = std::max(lhs.x, rhs.x);
    const float y1 = std::max(lhs.y, rhs.y);
    const float x2 = std::min(lhs.x + lhs.w, rhs.x + rhs.w);
    const float y2 = std::min(lhs.y + lhs.h, rhs.y + rhs.h);
    if (x2 <= x1 || y2 <= y1) {
        out = Rect{};
        return false;
    }
    out = Rect{x1, y1, x2 - x1, y2 - y1};
    return true;
}

inline Rect rect_union(const Rect& lhs, const Rect& rhs) {
    const float x1 = std::min(lhs.x, rhs.x);
    const float y1 = std::min(lhs.y, rhs.y);
    const float x2 = std::max(lhs.x + lhs.w, rhs.x + rhs.w);
    const float y2 = std::max(lhs.y + lhs.h, rhs.y + rhs.h);
    return Rect{x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1)};
}

inline bool command_payload_equal(const DrawCommand& lhs, const DrawCommand& rhs,
                                  const std::vector<char>& lhs_arena,
                                  const std::vector<char>& rhs_arena) {
    (void)lhs_arena;
    (void)rhs_arena;
    return lhs.hash == rhs.hash;
}

inline Rect command_visible_rect(const DrawCommand& cmd) {
    if (!cmd.has_clip) {
        return cmd.rect;
    }
    Rect visible{};
    if (!rect_intersection(cmd.rect, cmd.clip_rect, visible)) {
        return Rect{};
    }
    return visible;
}

inline Rect expanded_and_clamped(const Rect& rect, int width, int height, float expand_px = 2.0f) {
    Rect out{
        rect.x - expand_px,
        rect.y - expand_px,
        rect.w + expand_px * 2.0f,
        rect.h + expand_px * 2.0f,
    };
    out.x = std::clamp(out.x, 0.0f, static_cast<float>(width));
    out.y = std::clamp(out.y, 0.0f, static_cast<float>(height));
    const float right = std::clamp(out.x + out.w, 0.0f, static_cast<float>(width));
    const float bottom = std::clamp(out.y + out.h, 0.0f, static_cast<float>(height));
    out.w = std::max(0.0f, right - out.x);
    out.h = std::max(0.0f, bottom - out.y);
    return out;
}

inline std::uint64_t hash_frame_payload(const std::vector<DrawCommand>& commands, const Color& bg) {
    std::uint64_t hash = 1469598103934665603ull;
    auto mix = [](std::uint64_t h, std::uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
        return h;
    };
    auto float_to_u32 = [](float value) {
        std::uint32_t out = 0u;
        std::memcpy(&out, &value, sizeof(out));
        return out;
    };

    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.r)));
    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.g)));
    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.b)));
    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.a)));
    hash = mix(hash, static_cast<std::uint64_t>(commands.size()));
    for (const DrawCommand& cmd : commands) {
        hash = mix(hash, cmd.hash);
    }
    return hash;
}

inline void append_dirty_rect(const Rect& rect, int width, int height, std::vector<Rect>& out_regions,
                              std::size_t max_regions = 16u) {
    Rect region = expanded_and_clamped(rect, width, height);
    if (region.w <= 0.0f || region.h <= 0.0f) {
        return;
    }

    for (Rect& existing : out_regions) {
        if (rect_intersects(existing, region)) {
            existing = rect_union(existing, region);
            return;
        }
    }

    if (out_regions.size() < max_regions) {
        out_regions.push_back(region);
        return;
    }

    // Region budget exceeded: collapse to one bounding dirty area to cap CPU overhead.
    Rect merged = out_regions.front();
    for (std::size_t i = 1; i < out_regions.size(); ++i) {
        merged = rect_union(merged, out_regions[i]);
    }
    merged = rect_union(merged, region);
    out_regions.clear();
    out_regions.push_back(expanded_and_clamped(merged, width, height));
}

inline void merge_dirty_overlaps(std::vector<Rect>& regions, int width, int height) {
    if (regions.size() < 2u) {
        return;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t i = 0; i < regions.size() && !changed; ++i) {
            for (std::size_t j = i + 1; j < regions.size(); ++j) {
                if (rect_intersects(regions[i], regions[j])) {
                    regions[i] = expanded_and_clamped(rect_union(regions[i], regions[j]), width, height, 0.0f);
                    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(j));
                    changed = true;
                    break;
                }
            }
        }
    }
}

inline bool compute_dirty_regions(const std::vector<DrawCommand>& commands,
                                  const std::vector<char>& text_arena, const RuntimeState& runtime,
                                  const Color& bg, int width, int height, bool force_full,
                                  std::vector<Rect>& out_regions) {
    (void)text_arena;
    out_regions.clear();
    if (force_full || !runtime.has_prev_frame || !color_eq(bg, runtime.prev_bg)) {
        out_regions.push_back(Rect{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        return true;
    }

    const std::size_t max_count = std::max(commands.size(), runtime.prev_commands.size());
    for (std::size_t i = 0; i < max_count; ++i) {
        const bool has_curr = i < commands.size();
        const bool has_prev = i < runtime.prev_commands.size();

        if (has_curr && has_prev &&
            command_payload_equal(commands[i], runtime.prev_commands[i], text_arena, runtime.prev_text_arena)) {
            continue;
        }

        if (has_curr) {
            append_dirty_rect(command_visible_rect(commands[i]), width, height, out_regions);
        }
        if (has_prev) {
            append_dirty_rect(command_visible_rect(runtime.prev_commands[i]), width, height, out_regions);
        }
    }

    if (out_regions.empty()) {
        return false;
    }

    merge_dirty_overlaps(out_regions, width, height);
    return true;
}

struct IRect {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

inline IRect to_gl_rect(const Rect& rect, int framebuffer_w, int framebuffer_h) {
    int x = static_cast<int>(std::floor(rect.x));
    int y_top = static_cast<int>(std::floor(rect.y));
    int w = static_cast<int>(std::ceil(rect.w));
    int h = static_cast<int>(std::ceil(rect.h));

    x = std::clamp(x, 0, framebuffer_w);
    y_top = std::clamp(y_top, 0, framebuffer_h);
    w = std::clamp(w, 0, framebuffer_w - x);
    h = std::clamp(h, 0, framebuffer_h - y_top);
    int y = framebuffer_h - (y_top + h);
    y = std::clamp(y, 0, framebuffer_h);
    return IRect{x, y, w, h};
}

inline void ensure_cache_texture(RuntimeState& runtime, int width, int height) {
    if (runtime.cache_texture == 0u) {
        glGenTextures(1, &runtime.cache_texture);
    }
    if (runtime.cache_w == width && runtime.cache_h == height && runtime.has_cache) {
        return;
    }
    runtime.cache_w = width;
    runtime.cache_h = height;
    runtime.has_cache = false;

    glBindTexture(GL_TEXTURE_2D, runtime.cache_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

inline void copy_full_to_cache(RuntimeState& runtime, int width, int height) {
    if (runtime.cache_texture == 0u) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, runtime.cache_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    runtime.has_cache = true;
}

inline void copy_region_to_cache(RuntimeState& runtime, const IRect& gl_rect) {
    if (runtime.cache_texture == 0u || gl_rect.w <= 0 || gl_rect.h <= 0) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, runtime.cache_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, gl_rect.x, gl_rect.y, gl_rect.x, gl_rect.y, gl_rect.w, gl_rect.h);
    runtime.has_cache = true;
}

inline void draw_cache_texture(const RuntimeState& runtime, int width, int height) {
    if (!runtime.has_cache || runtime.cache_texture == 0u) {
        return;
    }
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, runtime.cache_texture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(static_cast<float>(width), 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(static_cast<float>(width), static_cast<float>(height));
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, static_cast<float>(height));
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

inline bool font_file_exists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::ifstream ifs(path, std::ios::binary);
    return static_cast<bool>(ifs);
}

inline std::string resolve_bundled_icon_font_file() {
    static constexpr const char* kBundledFont = "Font Awesome 7 Free-Solid-900.otf";
    static constexpr const char* kCandidates[] = {
        "include/Font Awesome 7 Free-Solid-900.otf",
        "./include/Font Awesome 7 Free-Solid-900.otf",
        "../include/Font Awesome 7 Free-Solid-900.otf",
        "../../include/Font Awesome 7 Free-Solid-900.otf",
    };
    for (const char* candidate : kCandidates) {
        if (candidate != nullptr && font_file_exists(candidate)) {
            return std::string(candidate);
        }
    }
    (void)kBundledFont;
    return {};
}

inline std::string resolve_icon_font_file_path(const char* explicit_file) {
    if (explicit_file != nullptr && explicit_file[0] != '\0') {
        const std::string explicit_path = explicit_file;
        if (font_file_exists(explicit_path)) {
            return explicit_path;
        }
    }
    const std::string bundled = resolve_bundled_icon_font_file();
    if (!bundled.empty()) {
        return bundled;
    }
    if (explicit_file != nullptr && explicit_file[0] != '\0') {
        return std::string(explicit_file);
    }
    return {};
}

#ifndef _WIN32
inline bool utf8_next(std::string_view text, std::size_t& index, std::uint32_t& codepoint) {
    if (index >= text.size()) {
        return false;
    }
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80u) {
        codepoint = lead;
        index += 1u;
        return true;
    }
    if ((lead >> 5u) == 0x6u && index + 1u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        if ((b1 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x1Fu) << 6u) |
                        static_cast<std::uint32_t>(b1 & 0x3Fu);
            index += 2u;
            return true;
        }
    }
    if ((lead >> 4u) == 0xEu && index + 2u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x0Fu) << 12u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b2 & 0x3Fu);
            index += 3u;
            return true;
        }
    }
    if ((lead >> 3u) == 0x1Eu && index + 3u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        const unsigned char b3 = static_cast<unsigned char>(text[index + 3u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x07u) << 18u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12u) |
                        (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b3 & 0x3Fu);
            index += 4u;
            return true;
        }
    }
    codepoint = static_cast<std::uint32_t>(lead);
    index += 1u;
    return true;
}
#endif

#ifdef _WIN32
inline std::wstring utf8_to_wide(std::string_view text) {
    if (text.empty()) {
        return std::wstring{};
    }
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) {
        std::wstring fallback;
        fallback.reserve(text.size());
        for (char ch : text) {
            fallback.push_back(static_cast<unsigned char>(ch));
        }
        return fallback;
    }
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(),
                        count);
    return out;
}

inline bool utf8_next(std::string_view text, std::size_t& index, std::uint32_t& codepoint) {
    if (index >= text.size()) {
        return false;
    }
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80u) {
        codepoint = lead;
        index += 1u;
        return true;
    }
    if ((lead >> 5u) == 0x6u && index + 1u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        if ((b1 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x1Fu) << 6u) |
                        static_cast<std::uint32_t>(b1 & 0x3Fu);
            index += 2u;
            return true;
        }
    }
    if ((lead >> 4u) == 0xEu && index + 2u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x0Fu) << 12u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b2 & 0x3Fu);
            index += 3u;
            return true;
        }
    }
    if ((lead >> 3u) == 0x1Eu && index + 3u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        const unsigned char b3 = static_cast<unsigned char>(text[index + 3u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x07u) << 18u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12u) |
                        (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b3 & 0x3Fu);
            index += 4u;
            return true;
        }
    }
    codepoint = static_cast<std::uint32_t>(lead);
    index += 1u;
    return true;
}

class Win32FontRenderer {
public:
    explicit Win32FontRenderer(const AppOptions& options)
        : text_family_(utf8_to_wide(options.text_font_family != nullptr ? options.text_font_family : "Segoe UI")),
          text_font_weight_(std::clamp(options.text_font_weight, 100, 900)),
          icon_family_(utf8_to_wide(options.icon_font_family != nullptr ? options.icon_font_family
                                                                       : "Font Awesome 7 Free Solid")),
          text_font_file_(utf8_to_wide(options.text_font_file != nullptr ? options.text_font_file : "")),
          icon_font_file_(utf8_to_wide(resolve_icon_font_file_path(options.icon_font_file))),
          enable_icon_fallback_(options.enable_icon_font_fallback) {}

    ~Win32FontRenderer() {
        if (wglGetCurrentContext() != nullptr) {
            release_gl_resources();
        }
        for (auto& pair : fonts_) {
            if (pair.second.handle != nullptr) {
                DeleteObject(pair.second.handle);
                pair.second.handle = nullptr;
            }
        }
        for (const std::wstring& path : loaded_private_fonts_) {
            RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
        }
    }

    Win32FontRenderer(const Win32FontRenderer&) = delete;
    Win32FontRenderer& operator=(const Win32FontRenderer&) = delete;

    void release_gl_resources() const {
        for (auto& pair : fonts_) {
            auto& glyphs = pair.second.glyphs;
            for (auto& glyph_pair : glyphs) {
                GlyphData& glyph = glyph_pair.second;
                if (glyph.list_id != 0u) {
                    glDeleteLists(glyph.list_id, 1);
                    glyph.list_id = 0u;
                }
            }
            glyphs.clear();
        }
    }

    bool draw_text(const DrawCommand& cmd, std::string_view text) const {
        if (text.empty()) {
            return true;
        }

        HDC hdc = wglGetCurrentDC();
        if (hdc == nullptr) {
            return false;
        }

        ensure_private_fonts_loaded();

        // Outline path appears visually smaller than legacy bitmap; lift render px for parity.
        const int text_font_px = std::max(8, static_cast<int>(std::round(cmd.font_size * 1.2f)));
        const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
        FontInstance* text_font = ensure_font(false, text_font_px, hdc);
        if (text_font == nullptr) {
            return false;
        }
        FontInstance* icon_font = enable_icon_fallback_ ? ensure_font(true, icon_font_px, hdc) : nullptr;

        std::vector<GlyphData> glyphs;
        glyphs.reserve(text.size());
        float width = 0.0f;
        float max_above_baseline = 0.0f;
        float max_below_baseline = 0.0f;

        std::size_t index = 0u;
        while (index < text.size()) {
            std::uint32_t cp = 0u;
            if (!utf8_next(text, index, cp)) {
                break;
            }
            const GlyphData* glyph = resolve_glyph(cp, text_font, icon_font, hdc);
            if (glyph == nullptr || !glyph->valid) {
                continue;
            }
            glyphs.push_back(*glyph);
            width += glyph->advance;
            max_above_baseline = std::max(max_above_baseline, glyph->ascent);
            max_below_baseline =
                std::max(max_below_baseline, std::max(0.0f, glyph->line_height - glyph->ascent));
        }

        if (glyphs.empty()) {
            return false;
        }

        float x = cmd.rect.x;
        if (cmd.align == TextAlign::Center) {
            x += std::max(0.0f, (cmd.rect.w - width) * 0.5f);
        } else if (cmd.align == TextAlign::Right) {
            x += std::max(0.0f, cmd.rect.w - width);
        }
        const float resolved_text_h =
            std::max(1.0f, max_above_baseline + max_below_baseline);
        const float baseline_y =
            cmd.rect.y + std::max(0.0f, (cmd.rect.h - resolved_text_h) * 0.5f) + max_above_baseline;

        glColor4f(cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        float pen_x = x;
        for (const GlyphData& glyph : glyphs) {
            if (glyph.list_id != 0u) {
                if (glyph.is_bitmap) {
                    glRasterPos2f(pen_x, baseline_y);
                    glCallList(glyph.list_id);
                } else {
                    glPushMatrix();
                    glTranslatef(pen_x, baseline_y, 0.0f);
                    // wgl outline glyph coordinates are y-up; UI projection here is y-down.
                    const float scale = std::max(1.0f, glyph.outline_scale);
                    glScalef(scale, -scale, 1.0f);
                    glCallList(glyph.list_id);
                    glPopMatrix();
                }
            }
            pen_x += glyph.advance;
        }
        return true;
    }

private:
    struct GlyphData {
        unsigned int list_id{0u};
        float advance{0.0f};
        float ascent{0.0f};
        float line_height{0.0f};
        float outline_scale{1.0f};
        bool is_bitmap{false};
        bool valid{false};
    };

    struct FontInstance {
        HFONT handle{nullptr};
        int ascent{0};
        int line_height{0};
        int pixel_size{0};
        std::unordered_map<std::uint32_t, GlyphData> glyphs{};
    };

    static int to_utf16(std::uint32_t cp, wchar_t out[2]) {
        if (cp <= 0xFFFFu) {
            out[0] = static_cast<wchar_t>(cp);
            return 1;
        }
        out[0] = L'\0';
        out[1] = L'\0';
        return 0;
    }

    static bool is_private_use_codepoint(std::uint32_t cp) {
        return cp >= 0xE000u && cp <= 0xF8FFu;
    }

    static bool is_known_icon_face(const std::wstring& face_name) {
        return face_name == L"Segoe Fluent Icons" || face_name == L"Segoe MDL2 Assets" ||
               face_name == L"Segoe UI Symbol";
    }

    static bool wide_iequals(const std::wstring& lhs, const std::wstring& rhs) {
        if (lhs.empty() || rhs.empty()) {
            return false;
        }
        return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
    }

    std::string font_key(bool icon_font, int px) const {
        return std::string(icon_font ? "icon:" : "text:") + std::to_string(px);
    }

    void ensure_private_fonts_loaded() const {
        if (private_fonts_loaded_) {
            return;
        }
        private_fonts_loaded_ = true;

        auto load_private = [&](const std::wstring& path) {
            if (path.empty()) {
                return;
            }
            const int added = AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
            if (added > 0) {
                loaded_private_fonts_.push_back(path);
            }
        };
        load_private(text_font_file_);
        load_private(icon_font_file_);
    }

    FontInstance* ensure_font(bool icon_font, int px, HDC hdc) const {
        const std::string key = font_key(icon_font, px);
        auto it = fonts_.find(key);
        if (it != fonts_.end()) {
            return &it->second;
        }

        auto create_font_instance =
            [&](const std::wstring& family, int font_weight, DWORD charset, bool* out_is_known_icon_face,
                std::wstring* out_resolved_face) -> FontInstance {
            FontInstance instance{};
            if (family.empty()) {
                if (out_is_known_icon_face != nullptr) {
                    *out_is_known_icon_face = false;
                }
                if (out_resolved_face != nullptr) {
                    out_resolved_face->clear();
                }
                return instance;
            }

            HFONT font = CreateFontW(-px, 0, 0, 0, std::clamp(font_weight, 100, 900), FALSE, FALSE, FALSE, charset, OUT_TT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     family.c_str());
            if (font == nullptr) {
                if (out_is_known_icon_face != nullptr) {
                    *out_is_known_icon_face = false;
                }
                if (out_resolved_face != nullptr) {
                    out_resolved_face->clear();
                }
                return instance;
            }

            HGDIOBJ old = SelectObject(hdc, font);
            TEXTMETRICW tm{};
            GetTextMetricsW(hdc, &tm);
            wchar_t face_name[LF_FACESIZE]{};
            GetTextFaceW(hdc, LF_FACESIZE, face_name);
            SelectObject(hdc, old);

            if (out_is_known_icon_face != nullptr) {
                *out_is_known_icon_face = is_known_icon_face(face_name);
            }
            if (out_resolved_face != nullptr) {
                *out_resolved_face = face_name;
            }

            instance.handle = font;
            instance.ascent = std::max(1, static_cast<int>(tm.tmAscent));
            instance.line_height = std::max(1, static_cast<int>(tm.tmHeight));
            instance.pixel_size = std::max(1, px);
            return instance;
        };

        FontInstance instance{};
        if (icon_font) {
            std::vector<std::wstring> candidates;
            candidates.reserve(4);
            auto add_unique = [&](const std::wstring& family) {
                if (family.empty()) {
                    return;
                }
                for (const std::wstring& existing : candidates) {
                    if (existing == family) {
                        return;
                    }
                }
                candidates.push_back(family);
            };

            add_unique(icon_family_);
            add_unique(L"Font Awesome 7 Free Solid");
            add_unique(L"Font Awesome 7 Free");
            add_unique(L"Segoe Fluent Icons");
            add_unique(L"Segoe MDL2 Assets");
            add_unique(L"Segoe UI Symbol");

            for (const std::wstring& family : candidates) {
                bool known_icon_face = false;
                std::wstring resolved_face{};
                FontInstance candidate =
                    create_font_instance(family, FW_NORMAL, DEFAULT_CHARSET, &known_icon_face, &resolved_face);
                if (candidate.handle == nullptr) {
                    continue;
                }

                const bool exact_family_match = wide_iequals(resolved_face, family);
                const bool usable_icon_face = known_icon_face || exact_family_match;
                if (usable_icon_face) {
                    instance = std::move(candidate);
                    break;
                }
                DeleteObject(candidate.handle);
            }
        } else {
            instance = create_font_instance(text_family_, text_font_weight_, DEFAULT_CHARSET, nullptr, nullptr);
        }

        if (instance.handle == nullptr) {
            return nullptr;
        }

        auto [inserted_it, _] = fonts_.emplace(key, std::move(instance));
        return &inserted_it->second;
    }

    GlyphData* ensure_glyph(FontInstance* font, std::uint32_t cp, HDC hdc) const {
        if (font == nullptr) {
            return nullptr;
        }

        auto it = font->glyphs.find(cp);
        if (it != font->glyphs.end()) {
            return &it->second;
        }

        GlyphData glyph{};
        wchar_t wide[2] = {L'\0', L'\0'};
        const int wide_len = to_utf16(cp, wide);
        if (wide_len == 1) {
            HGDIOBJ old = SelectObject(hdc, font->handle);
            bool glyph_available = true;
            if (is_private_use_codepoint(cp)) {
                WORD glyph_index = 0xFFFFu;
                const DWORD glyph_query =
                    GetGlyphIndicesW(hdc, wide, 1, &glyph_index, GGI_MARK_NONEXISTING_GLYPHS);
                glyph_available = glyph_query != GDI_ERROR && glyph_index != 0xFFFFu;
            }

            if (glyph_available) {
                const unsigned int list_id = glGenLists(1);
                bool built = false;
                const DWORD glyph_code = static_cast<DWORD>(wide[0]);
                if (list_id != 0u) {
                    GLYPHMETRICSFLOAT gm{};
                    if (wglUseFontOutlinesW(hdc, glyph_code, 1u, list_id, 0.0f, 0.0f,
                                            WGL_FONT_POLYGONS, &gm) != FALSE) {
                        const bool outline_metrics_ok =
                            gm.gmfBlackBoxX > 0.0f && gm.gmfBlackBoxY > 0.0f;
                        const float outline_scale = static_cast<float>(std::max(1, font->pixel_size));
                        glyph.advance = std::max(0.0f, gm.gmfCellIncX * outline_scale);
                        if (glyph.advance <= 0.0f) {
                            SIZE size{};
                            GetTextExtentPoint32W(hdc, wide, 1, &size);
                            glyph.advance = std::max(0.0f, static_cast<float>(size.cx));
                        }
                        if (outline_metrics_ok && glyph.advance >= 0.25f) {
                            const float above = std::max(0.0f, gm.gmfptGlyphOrigin.y * outline_scale);
                            const float below =
                                std::max(0.0f, (gm.gmfBlackBoxY - gm.gmfptGlyphOrigin.y) * outline_scale);
                            glyph.ascent = above;
                            glyph.line_height = std::max(1.0f, above + below);
                            if (glyph.line_height < 1.0f || glyph.ascent < 0.0f) {
                                glyph.ascent = static_cast<float>(std::max(1, font->ascent));
                                glyph.line_height = static_cast<float>(std::max(1, font->line_height));
                            }
                            glyph.outline_scale = outline_scale;
                            glyph.list_id = list_id;
                            glyph.is_bitmap = false;
                            glyph.valid = true;
                            built = true;
                        }
                    }
                }
                if (!built && list_id != 0u && wglUseFontBitmapsW(hdc, glyph_code, 1u, list_id) != FALSE) {
                    SIZE size{};
                    GetTextExtentPoint32W(hdc, wide, 1, &size);
                    glyph.advance = std::max(0.0f, static_cast<float>(size.cx));
                    glyph.ascent = static_cast<float>(std::max(1, font->ascent));
                    glyph.line_height = static_cast<float>(std::max(1, font->line_height));
                    glyph.outline_scale = 1.0f;
                    glyph.list_id = list_id;
                    glyph.is_bitmap = true;
                    glyph.valid = true;
                    built = true;
                }

                if (!built && list_id != 0u) {
                    glDeleteLists(list_id, 1);
                }
            }
            SelectObject(hdc, old);
        }

        auto [inserted_it, _] = font->glyphs.emplace(cp, glyph);
        return &inserted_it->second;
    }

    const GlyphData* resolve_glyph(std::uint32_t cp, FontInstance* text_font, FontInstance* icon_font,
                                   HDC hdc) const {
        auto try_font = [&](FontInstance* font) -> GlyphData* {
            GlyphData* glyph = ensure_glyph(font, cp, hdc);
            if (glyph != nullptr && glyph->valid) {
                return glyph;
            }
            return nullptr;
        };

        const bool prefer_icon_font = enable_icon_fallback_ && icon_font != nullptr && is_private_use_codepoint(cp);
        if (prefer_icon_font) {
            if (GlyphData* icon_glyph = try_font(icon_font)) {
                return icon_glyph;
            }
            return nullptr;
        } else {
            if (GlyphData* text_glyph = try_font(text_font)) {
                return text_glyph;
            }
            if (enable_icon_fallback_ && icon_font != nullptr) {
                if (GlyphData* icon_glyph = try_font(icon_font)) {
                    return icon_glyph;
                }
            }
        }

        GlyphData* fallback_glyph = ensure_glyph(text_font, static_cast<std::uint32_t>('?'), hdc);
        if (fallback_glyph != nullptr && fallback_glyph->valid) {
            return fallback_glyph;
        }
        return nullptr;
    }

    std::wstring text_family_{};
    int text_font_weight_{FW_NORMAL};
    std::wstring icon_family_{};
    std::wstring text_font_file_{};
    std::wstring icon_font_file_{};
    bool enable_icon_fallback_{true};

    mutable bool private_fonts_loaded_{false};
    mutable std::unordered_map<std::string, FontInstance> fonts_{};
    mutable std::vector<std::wstring> loaded_private_fonts_{};
};
#endif

#if EUI_ENABLE_STB_TRUETYPE
class StbFontRenderer {
public:
    static std::string resolve_font_path_for_measure(const char* explicit_file, const char* family, bool icon_font) {
        return resolve_font_path(explicit_file, family, icon_font);
    }

    explicit StbFontRenderer(const AppOptions& options)
        : text_font_path_(resolve_font_path(options.text_font_file, options.text_font_family, false)),
          icon_font_path_(resolve_font_path(options.icon_font_file, options.icon_font_family, true)),
          enable_icon_fallback_(options.enable_icon_font_fallback) {}

    void release_gl_resources() const {
        for (auto& pair : faces_) {
            FontFace& face = pair.second;
            for (AtlasPage& page : face.atlas_pages) {
                if (page.texture_id != 0u) {
                    glDeleteTextures(1, &page.texture_id);
                    page.texture_id = 0u;
                }
            }
            face.atlas_pages.clear();
            face.glyphs.clear();
            face.data_blob.reset();
        }
        faces_.clear();
        font_blob_cache_.clear();
        face_use_tick_ = 1u;
    }

    bool draw_text(const DrawCommand& cmd, std::string_view text) const {
        if (text.empty()) {
            return true;
        }

        const int text_font_px = std::max(8, static_cast<int>(std::round(cmd.font_size * 1.20f)));
        const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
        FontFace* text_face = ensure_face(false, text_font_px);
        FontFace* icon_face = nullptr;
        auto ensure_icon_face = [&]() -> FontFace* {
            if (!enable_icon_fallback_) {
                return nullptr;
            }
            if (icon_face == nullptr) {
                icon_face = ensure_face(true, icon_font_px);
            }
            return icon_face;
        };
        if (text_face == nullptr) {
            if (!enable_icon_fallback_ || ensure_icon_face() == nullptr) {
                return false;
            }
        }

        struct DrawGlyph {
            const GlyphData* glyph{nullptr};
            float pen_x{0.0f};
        };

        std::vector<DrawGlyph> draw_glyphs;
        draw_glyphs.reserve(text.size());
        float width = 0.0f;
        float max_above = 0.0f;
        float max_below = 0.0f;

        auto fallback_glyph = [&](FontFace* face) -> const GlyphData* {
            if (face == nullptr) {
                return nullptr;
            }
            GlyphData* fallback = ensure_glyph(face, static_cast<std::uint32_t>('?'));
            if (fallback != nullptr && fallback->valid) {
                return fallback;
            }
            return nullptr;
        };

        auto pick_glyph = [&](std::uint32_t cp) -> const GlyphData* {
            auto try_face = [&](FontFace* face) -> const GlyphData* {
                if (face == nullptr) {
                    return nullptr;
                }
                GlyphData* glyph = ensure_glyph(face, cp);
                if (glyph != nullptr && glyph->valid) {
                    return glyph;
                }
                return nullptr;
            };

            const bool private_use = cp >= 0xE000u && cp <= 0xF8FFu;
            if (private_use && enable_icon_fallback_) {
                if (const GlyphData* icon = try_face(ensure_icon_face())) {
                    return icon;
                }
                return fallback_glyph(text_face);
            }
            if (const GlyphData* regular = try_face(text_face)) {
                return regular;
            }
            if (enable_icon_fallback_) {
                if (const GlyphData* icon = try_face(ensure_icon_face())) {
                    return icon;
                }
            }
            if (const GlyphData* fallback = fallback_glyph(text_face)) {
                return fallback;
            }
            return fallback_glyph(ensure_icon_face());
        };

        std::size_t index = 0u;
        while (index < text.size()) {
            std::uint32_t cp = 0u;
            if (!utf8_next(text, index, cp)) {
                break;
            }
            const GlyphData* glyph = pick_glyph(cp);
            if (glyph == nullptr) {
                continue;
            }

            draw_glyphs.push_back(DrawGlyph{glyph, width});
            width += std::max(0.0f, glyph->advance);
            max_above = std::max(max_above, std::max(0.0f, -glyph->yoff));
            max_below = std::max(max_below, std::max(0.0f, glyph->h + glyph->yoff));
        }

        if (draw_glyphs.empty()) {
            return false;
        }

        float x = cmd.rect.x;
        if (cmd.align == TextAlign::Center) {
            x += std::max(0.0f, (cmd.rect.w - width) * 0.5f);
        } else if (cmd.align == TextAlign::Right) {
            x += std::max(0.0f, cmd.rect.w - width);
        }

        if (max_above + max_below < 1.0f) {
            if (text_face != nullptr && text_face->line_height > 1.0f) {
                max_above = std::max(max_above, text_face->ascent);
                max_below = std::max(max_below, text_face->line_height - text_face->ascent);
            } else if (icon_face != nullptr && icon_face->line_height > 1.0f) {
                max_above = std::max(max_above, icon_face->ascent);
                max_below = std::max(max_below, icon_face->line_height - icon_face->ascent);
            } else {
                max_above = std::max(max_above, cmd.font_size * 0.72f);
                max_below = std::max(max_below, cmd.font_size * 0.28f);
            }
        }

        const float text_h = std::max(1.0f, max_above + max_below);
        const float baseline_y = cmd.rect.y + std::max(0.0f, (cmd.rect.h - text_h) * 0.5f) + max_above;

        glEnable(GL_TEXTURE_2D);
        glColor4f(cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a);
        unsigned int bound_texture = 0u;
        for (const DrawGlyph& draw : draw_glyphs) {
            const GlyphData& glyph = *draw.glyph;
            if (!glyph.has_bitmap || glyph.texture_id == 0u || glyph.w <= 0.0f || glyph.h <= 0.0f) {
                continue;
            }
            const float x0 = x + draw.pen_x + glyph.xoff;
            const float y0 = baseline_y + glyph.yoff;
            const float x1 = x0 + glyph.w;
            const float y1 = y0 + glyph.h;

            if (bound_texture != glyph.texture_id) {
                glBindTexture(GL_TEXTURE_2D, glyph.texture_id);
                bound_texture = glyph.texture_id;
            }
            glBegin(GL_QUADS);
            glTexCoord2f(glyph.u0, glyph.v0);
            glVertex2f(x0, y0);
            glTexCoord2f(glyph.u1, glyph.v0);
            glVertex2f(x1, y0);
            glTexCoord2f(glyph.u1, glyph.v1);
            glVertex2f(x1, y1);
            glTexCoord2f(glyph.u0, glyph.v1);
            glVertex2f(x0, y1);
            glEnd();
        }
        if (bound_texture != 0u) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glDisable(GL_TEXTURE_2D);
        return true;
    }

private:
    static constexpr int k_atlas_padding = 1;
    static constexpr int k_max_atlas_pages = 3;
    static constexpr int k_tiny_atlas_size = 256;
    static constexpr int k_small_atlas_size = 512;
    static constexpr int k_large_atlas_size = 1024;
    static constexpr std::size_t k_max_face_cache = 20u;

    struct AtlasSlot {
        bool occupied{false};
        std::uint32_t codepoint{0u};
        std::uint64_t last_used{0u};
    };

    struct AtlasPage {
        unsigned int texture_id{0u};
        int width{0};
        int height{0};
        int cell_size{0};
        int columns{0};
        int rows{0};
        std::vector<AtlasSlot> slots{};
    };

    struct GlyphData {
        unsigned int texture_id{0u};
        std::uint16_t page_index{0u};
        std::uint16_t slot_index{0u};
        float advance{0.0f};
        float xoff{0.0f};
        float yoff{0.0f};
        float w{0.0f};
        float h{0.0f};
        float u0{0.0f};
        float v0{0.0f};
        float u1{0.0f};
        float v1{0.0f};
        bool has_bitmap{false};
        bool valid{false};
    };

    struct FontFace {
        std::shared_ptr<std::vector<unsigned char>> data_blob{};
        stbtt_fontinfo info{};
        float scale{0.0f};
        float ascent{0.0f};
        float line_height{0.0f};
        int atlas_size{k_small_atlas_size};
        int cell_size{64};
        int max_pages{k_max_atlas_pages};
        std::uint64_t lru_tick{1u};
        std::uint64_t last_used{0u};
        bool valid{false};
        std::unordered_map<std::uint32_t, GlyphData> glyphs{};
        std::vector<AtlasPage> atlas_pages{};
    };

    struct SlotHandle {
        AtlasPage* page{nullptr};
        std::size_t page_index{0u};
        std::size_t slot_index{0u};
        int x{0};
        int y{0};
        bool valid{false};
    };

    static int next_power_of_two(int value) {
        int result = 1;
        while (result < value && result < 4096) {
            result <<= 1;
        }
        return std::max(1, result);
    }

    static int quantize_font_px(int px) {
        int clamped = std::max(8, px);
        if (clamped <= 32) {
            clamped = ((clamped + 1) / 2) * 2;
        } else if (clamped <= 72) {
            clamped = ((clamped + 2) / 4) * 4;
        } else {
            clamped = ((clamped + 4) / 8) * 8;
        }
        return std::clamp(clamped, 8, 200);
    }

    static bool file_exists(const std::string& path) {
        if (path.empty()) {
            return false;
        }
        std::ifstream ifs(path, std::ios::binary);
        return static_cast<bool>(ifs);
    }

    static std::vector<unsigned char> read_file(const std::string& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            return {};
        }
        ifs.seekg(0, std::ios::end);
        const std::streamoff size = ifs.tellg();
        if (size <= 0) {
            return {};
        }
        ifs.seekg(0, std::ios::beg);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        ifs.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!ifs) {
            return {};
        }
        return bytes;
    }

    std::shared_ptr<std::vector<unsigned char>> load_font_blob(const std::string& path) const {
        if (path.empty()) {
            return {};
        }
        auto it = font_blob_cache_.find(path);
        if (it != font_blob_cache_.end()) {
            return it->second;
        }
        auto data = std::make_shared<std::vector<unsigned char>>(read_file(path));
        if (data->empty()) {
            return {};
        }
        font_blob_cache_[path] = data;
        return data;
    }

    static void release_face_gl_resources(FontFace& face) {
        for (AtlasPage& page : face.atlas_pages) {
            if (page.texture_id != 0u) {
                glDeleteTextures(1, &page.texture_id);
                page.texture_id = 0u;
            }
        }
        face.atlas_pages.clear();
        face.glyphs.clear();
        face.data_blob.reset();
    }

    void prune_face_cache_if_needed(const std::string& preserve_key) const {
        while (faces_.size() > k_max_face_cache) {
            auto victim = faces_.end();
            for (auto it = faces_.begin(); it != faces_.end(); ++it) {
                if (it->first == preserve_key) {
                    continue;
                }
                if (victim == faces_.end() || it->second.last_used < victim->second.last_used) {
                    victim = it;
                }
            }
            if (victim == faces_.end()) {
                break;
            }
            release_face_gl_resources(victim->second);
            faces_.erase(victim);
        }
    }

    static std::string to_lower_ascii(std::string_view text) {
        std::string out;
        out.reserve(text.size());
        for (char ch : text) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return out;
    }

    static std::string join_path(const std::string& base, const char* file_name) {
        if (base.empty() || file_name == nullptr || *file_name == '\0') {
            return {};
        }
        const char last = base.back();
        if (last == '/' || last == '\\') {
            return base + file_name;
        }
#ifdef _WIN32
        return base + "\\" + file_name;
#else
        return base + "/" + file_name;
#endif
    }

    static std::string resolve_font_path(const char* explicit_file, const char* family, bool icon_font) {
        if (icon_font) {
            const std::string icon_path = resolve_icon_font_file_path(explicit_file);
            if (!icon_path.empty() && file_exists(icon_path)) {
                return icon_path;
            }
        } else if (explicit_file != nullptr && explicit_file[0] != '\0') {
            const std::string explicit_path = explicit_file;
            if (file_exists(explicit_path)) {
                return explicit_path;
            }
        }

        std::vector<std::string> candidates;
        const std::string family_lower = to_lower_ascii(family != nullptr ? std::string_view(family) : "");

#ifdef _WIN32
        std::string windows_dir = "C:\\Windows";
#if defined(_MSC_VER)
        char* env_value = nullptr;
        std::size_t env_len = 0u;
        if (_dupenv_s(&env_value, &env_len, "WINDIR") == 0 && env_value != nullptr) {
            if (env_value[0] != '\0') {
                windows_dir = env_value;
            }
        }
        std::free(env_value);
#else
        if (const char* env = std::getenv("WINDIR")) {
            if (env[0] != '\0') {
                windows_dir = env;
            }
        }
#endif
        const std::string fonts_dir = join_path(windows_dir, "Fonts");
        auto add_win = [&](const char* file_name) {
            const std::string full = join_path(fonts_dir, file_name);
            if (!full.empty()) {
                candidates.push_back(full);
            }
        };

        if (icon_font) {
            if (family_lower.find("fluent") != std::string::npos) {
                add_win("segfluenticons.ttf");
            }
            if (family_lower.find("mdl2") != std::string::npos) {
                add_win("segmdl2.ttf");
            }
            if (family_lower.find("symbol") != std::string::npos) {
                add_win("seguisym.ttf");
            }
            add_win("segmdl2.ttf");
            add_win("segfluenticons.ttf");
            add_win("seguisym.ttf");
        } else {
            if (family_lower.find("segoe") != std::string::npos) {
                add_win("segoeui.ttf");
            }
            if (family_lower.find("arial") != std::string::npos) {
                add_win("arial.ttf");
            }
            add_win("segoeui.ttf");
            add_win("arial.ttf");
        }
#else
        auto add_unix = [&](const char* path) {
            candidates.emplace_back(path);
        };

        if (icon_font) {
            if (family_lower.find("emoji") != std::string::npos) {
                add_unix("/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf");
            }
            add_unix("/usr/share/fonts/truetype/noto/NotoSansSymbols2-Regular.ttf");
            add_unix("/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf");
            add_unix("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        } else {
            if (family_lower.find("noto") != std::string::npos) {
                add_unix("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
            }
            add_unix("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
            add_unix("/usr/share/fonts/dejavu/DejaVuSans.ttf");
            add_unix("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
        }
#endif

        for (const std::string& path : candidates) {
            if (file_exists(path)) {
                return path;
            }
        }
        return {};
    }

    static std::size_t slot_count_for(const AtlasPage& page) {
        return static_cast<std::size_t>(std::max(0, page.columns) * std::max(0, page.rows));
    }

    bool create_atlas_page(FontFace* face) const {
        if (face == nullptr || static_cast<int>(face->atlas_pages.size()) >= face->max_pages) {
            return false;
        }

        AtlasPage page{};
        page.width = std::max(128, face->atlas_size);
        page.height = std::max(128, face->atlas_size);
        page.cell_size = std::max(8, face->cell_size);
        page.columns = std::max(1, page.width / page.cell_size);
        page.rows = std::max(1, page.height / page.cell_size);
        page.slots.assign(slot_count_for(page), AtlasSlot{});

        glGenTextures(1, &page.texture_id);
        if (page.texture_id == 0u) {
            return false;
        }

        GLint prev_unpack = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, page.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        std::vector<unsigned char> zero(static_cast<std::size_t>(page.width * page.height), 0u);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, page.width, page.height, 0, GL_ALPHA, GL_UNSIGNED_BYTE,
                     zero.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack);

        face->atlas_pages.push_back(std::move(page));
        return true;
    }

    void touch_slot(FontFace* face, const GlyphData& glyph) const {
        if (face == nullptr || !glyph.has_bitmap) {
            return;
        }
        const std::size_t page_index = static_cast<std::size_t>(glyph.page_index);
        if (page_index >= face->atlas_pages.size()) {
            return;
        }
        AtlasPage& page = face->atlas_pages[page_index];
        const std::size_t slot_index = static_cast<std::size_t>(glyph.slot_index);
        if (slot_index >= page.slots.size()) {
            return;
        }
        AtlasSlot& slot = page.slots[slot_index];
        if (!slot.occupied) {
            return;
        }
        slot.last_used = ++face->lru_tick;
    }

    SlotHandle acquire_slot(FontFace* face, std::uint32_t cp) const {
        if (face == nullptr) {
            return {};
        }

        auto make_handle = [&](AtlasPage& page, std::size_t page_index, std::size_t slot_index) -> SlotHandle {
            AtlasSlot& slot = page.slots[slot_index];
            slot.occupied = true;
            slot.codepoint = cp;
            slot.last_used = ++face->lru_tick;
            const int col = static_cast<int>(slot_index % static_cast<std::size_t>(std::max(1, page.columns)));
            const int row = static_cast<int>(slot_index / static_cast<std::size_t>(std::max(1, page.columns)));
            return SlotHandle{
                &page,
                page_index,
                slot_index,
                col * page.cell_size,
                row * page.cell_size,
                true,
            };
        };

        for (std::size_t page_index = 0; page_index < face->atlas_pages.size(); ++page_index) {
            AtlasPage& page = face->atlas_pages[page_index];
            for (std::size_t slot_index = 0; slot_index < page.slots.size(); ++slot_index) {
                if (!page.slots[slot_index].occupied) {
                    return make_handle(page, page_index, slot_index);
                }
            }
        }

        if (static_cast<int>(face->atlas_pages.size()) < face->max_pages && create_atlas_page(face)) {
            AtlasPage& page = face->atlas_pages.back();
            if (!page.slots.empty()) {
                return make_handle(page, face->atlas_pages.size() - 1u, 0u);
            }
        }

        std::uint64_t oldest_tick = std::numeric_limits<std::uint64_t>::max();
        std::size_t oldest_page_index = 0u;
        std::size_t oldest_slot_index = 0u;
        bool found = false;
        for (std::size_t page_index = 0; page_index < face->atlas_pages.size(); ++page_index) {
            AtlasPage& page = face->atlas_pages[page_index];
            for (std::size_t slot_index = 0; slot_index < page.slots.size(); ++slot_index) {
                const AtlasSlot& slot = page.slots[slot_index];
                if (!slot.occupied) {
                    continue;
                }
                if (!found || slot.last_used < oldest_tick) {
                    found = true;
                    oldest_tick = slot.last_used;
                    oldest_page_index = page_index;
                    oldest_slot_index = slot_index;
                }
            }
        }
        if (!found) {
            return {};
        }

        AtlasPage& page = face->atlas_pages[oldest_page_index];
        AtlasSlot& slot = page.slots[oldest_slot_index];
        if (slot.occupied) {
            face->glyphs.erase(slot.codepoint);
        }
        return make_handle(page, oldest_page_index, oldest_slot_index);
    }

    FontFace* ensure_face(bool icon_font, int px) const {
        const int quantized_px = quantize_font_px(px);
        const std::string key = std::string(icon_font ? "icon:" : "text:") + std::to_string(quantized_px);
        auto it = faces_.find(key);
        if (it != faces_.end()) {
            it->second.last_used = ++face_use_tick_;
            return it->second.valid ? &it->second : nullptr;
        }

        FontFace face{};
        const std::string& path = icon_font ? icon_font_path_ : text_font_path_;
        face.data_blob = load_font_blob(path);
        if (face.data_blob != nullptr && !face.data_blob->empty()) {
            const int offset = stbtt_GetFontOffsetForIndex(face.data_blob->data(), 0);
            if (offset >= 0 && stbtt_InitFont(&face.info, face.data_blob->data(), offset) != 0) {
                face.scale = stbtt_ScaleForPixelHeight(&face.info, static_cast<float>(quantized_px));
                int ascent = 0;
                int descent = 0;
                int line_gap = 0;
                stbtt_GetFontVMetrics(&face.info, &ascent, &descent, &line_gap);
                face.ascent = static_cast<float>(ascent) * face.scale;
                face.line_height = static_cast<float>(ascent - descent + line_gap) * face.scale;
                face.cell_size = std::clamp(next_power_of_two(std::max(20, quantized_px + quantized_px / 2)), 24, 128);
                if (face.cell_size <= 32) {
                    face.atlas_size = k_tiny_atlas_size;
                    face.max_pages = 2;
                } else if (face.cell_size <= 64) {
                    face.atlas_size = k_small_atlas_size;
                    face.max_pages = 2;
                } else {
                    face.atlas_size = k_large_atlas_size;
                    face.max_pages = k_max_atlas_pages;
                }
                face.lru_tick = 1u;
                face.last_used = ++face_use_tick_;
                face.valid = face.scale > 0.0f;
            }
        }

        auto [inserted, _] = faces_.emplace(key, std::move(face));
        return inserted->second.valid ? &inserted->second : nullptr;
    }

    GlyphData* ensure_glyph(FontFace* face, std::uint32_t cp) const {
        if (face == nullptr) {
            return nullptr;
        }
        auto it = face->glyphs.find(cp);
        if (it != face->glyphs.end()) {
            touch_slot(face, it->second);
            return &it->second;
        }

        GlyphData glyph{};
        const int glyph_index = stbtt_FindGlyphIndex(&face->info, static_cast<int>(cp));
        if (glyph_index == 0 && cp != 0u) {
            auto [inserted, _] = face->glyphs.emplace(cp, glyph);
            return &inserted->second;
        }

        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&face->info, static_cast<int>(cp), &advance, &lsb);
        glyph.advance = std::max(0.0f, static_cast<float>(advance) * face->scale);

        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetCodepointBitmapBox(&face->info, static_cast<int>(cp), face->scale, face->scale, &x0, &y0, &x1, &y1);
        const int w = std::max(0, x1 - x0);
        const int h = std::max(0, y1 - y0);
        glyph.xoff = static_cast<float>(x0);
        glyph.yoff = static_cast<float>(y0);
        glyph.w = static_cast<float>(w);
        glyph.h = static_cast<float>(h);
        glyph.valid = true;

        if (w > 0 && h > 0) {
            std::vector<unsigned char> bitmap(static_cast<std::size_t>(w * h), 0u);
            stbtt_MakeCodepointBitmap(&face->info, bitmap.data(), w, h, w, face->scale, face->scale,
                                      static_cast<int>(cp));

            int pad = k_atlas_padding;
            int upload_w = w + pad * 2;
            int upload_h = h + pad * 2;
            if (upload_w > face->cell_size || upload_h > face->cell_size) {
                pad = 0;
                upload_w = w;
                upload_h = h;
            }

            if (upload_w <= face->cell_size && upload_h <= face->cell_size) {
                const SlotHandle slot = acquire_slot(face, cp);
                if (slot.valid && slot.page != nullptr && slot.page->texture_id != 0u) {
                    const unsigned char* upload_data = bitmap.data();
                    std::vector<unsigned char> padded_bitmap{};
                    if (pad > 0) {
                        padded_bitmap.assign(static_cast<std::size_t>(upload_w * upload_h), 0u);
                        for (int row = 0; row < h; ++row) {
                            std::memcpy(&padded_bitmap[static_cast<std::size_t>((row + pad) * upload_w + pad)],
                                        &bitmap[static_cast<std::size_t>(row * w)], static_cast<std::size_t>(w));
                        }
                        upload_data = padded_bitmap.data();
                    }

                    GLint prev_unpack = 4;
                    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glBindTexture(GL_TEXTURE_2D, slot.page->texture_id);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, slot.x, slot.y, upload_w, upload_h, GL_ALPHA,
                                    GL_UNSIGNED_BYTE, upload_data);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack);

                    const float inv_w = 1.0f / static_cast<float>(slot.page->width);
                    const float inv_h = 1.0f / static_cast<float>(slot.page->height);
                    const int uv_x0 = slot.x + pad;
                    const int uv_y0 = slot.y + pad;
                    const int uv_x1 = uv_x0 + w;
                    const int uv_y1 = uv_y0 + h;

                    glyph.texture_id = slot.page->texture_id;
                    glyph.page_index = static_cast<std::uint16_t>(slot.page_index);
                    glyph.slot_index = static_cast<std::uint16_t>(slot.slot_index);
                    glyph.u0 = static_cast<float>(uv_x0) * inv_w;
                    glyph.v0 = static_cast<float>(uv_y0) * inv_h;
                    glyph.u1 = static_cast<float>(uv_x1) * inv_w;
                    glyph.v1 = static_cast<float>(uv_y1) * inv_h;
                    glyph.has_bitmap = true;
                }
            }
        }

        auto [inserted, _] = face->glyphs.emplace(cp, std::move(glyph));
        return &inserted->second;
    }

    std::string text_font_path_{};
    std::string icon_font_path_{};
    bool enable_icon_fallback_{true};
    mutable std::unordered_map<std::string, FontFace> faces_{};
    mutable std::unordered_map<std::string, std::shared_ptr<std::vector<unsigned char>>> font_blob_cache_{};
    mutable std::uint64_t face_use_tick_{1u};
};
#else
class StbFontRenderer {
public:
    static std::string resolve_font_path_for_measure(const char*, const char*, bool) {
        return {};
    }
    explicit StbFontRenderer(const AppOptions&) {}
    void release_gl_resources() const {}
    bool draw_text(const DrawCommand&, std::string_view) const {
        return false;
    }
};
#endif

inline const Glyph& glyph_for(char ch) {
    static const Glyph kUnknown = {0x1E, 0x11, 0x02, 0x04, 0x04, 0x00, 0x04};
    static const Glyph kSpace = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const Glyph kDot = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    static const Glyph kMinus = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    static const Glyph kGreater = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10};
    static const Glyph kPipe = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const Glyph kPercent = {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
    static const Glyph kSlash = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};

    static const Glyph k0 = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const Glyph k1 = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const Glyph k2 = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const Glyph k3 = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    static const Glyph k4 = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const Glyph k5 = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
    static const Glyph k6 = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const Glyph k7 = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const Glyph k8 = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const Glyph k9 = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};

    static const Glyph kA = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const Glyph kB = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const Glyph kC = {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F};
    static const Glyph kD = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const Glyph kE = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const Glyph kF = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const Glyph kG = {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F};
    static const Glyph kH = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const Glyph kI = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const Glyph kJ = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    static const Glyph kK = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const Glyph kL = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const Glyph kM = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const Glyph kN = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const Glyph kO = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const Glyph kP = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const Glyph kQ = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    static const Glyph kR = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const Glyph kS = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const Glyph kT = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const Glyph kU = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const Glyph kV = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    static const Glyph kW = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    static const Glyph kX = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    static const Glyph kY = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    static const Glyph kZ = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};

    switch (ch) {
        case ' ':
            return kSpace;
        case '.':
            return kDot;
        case '-':
            return kMinus;
        case '>':
            return kGreater;
        case '|':
            return kPipe;
        case '%':
            return kPercent;
        case '/':
            return kSlash;
        case '0':
            return k0;
        case '1':
            return k1;
        case '2':
            return k2;
        case '3':
            return k3;
        case '4':
            return k4;
        case '5':
            return k5;
        case '6':
            return k6;
        case '7':
            return k7;
        case '8':
            return k8;
        case '9':
            return k9;
        case 'A':
            return kA;
        case 'B':
            return kB;
        case 'C':
            return kC;
        case 'D':
            return kD;
        case 'E':
            return kE;
        case 'F':
            return kF;
        case 'G':
            return kG;
        case 'H':
            return kH;
        case 'I':
            return kI;
        case 'J':
            return kJ;
        case 'K':
            return kK;
        case 'L':
            return kL;
        case 'M':
            return kM;
        case 'N':
            return kN;
        case 'O':
            return kO;
        case 'P':
            return kP;
        case 'Q':
            return kQ;
        case 'R':
            return kR;
        case 'S':
            return kS;
        case 'T':
            return kT;
        case 'U':
            return kU;
        case 'V':
            return kV;
        case 'W':
            return kW;
        case 'X':
            return kX;
        case 'Y':
            return kY;
        case 'Z':
            return kZ;
        default:
            return kUnknown;
    }
}

inline void gl_set_color(const Color& color) {
    glColor4f(color.r, color.g, color.b, color.a);
}

inline int build_rounded_points(const Rect& rect, float radius, Point* out_points, int max_points) {
    if (max_points < 4) {
        return 0;
    }

    radius = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    if (radius <= 0.0f) {
        out_points[0] = Point{rect.x, rect.y};
        out_points[1] = Point{rect.x + rect.w, rect.y};
        out_points[2] = Point{rect.x + rect.w, rect.y + rect.h};
        out_points[3] = Point{rect.x, rect.y + rect.h};
        return 4;
    }

    const float left = rect.x;
    const float right = rect.x + rect.w;
    const float top = rect.y;
    const float bottom = rect.y + rect.h;
    const int steps = std::clamp(static_cast<int>(radius * 0.65f), 3, 10);
    const float kPi = 3.1415926f;

    int count = 0;
    auto push_point = [&](float x, float y) {
        if (count < max_points) {
            out_points[count++] = Point{x, y};
        }
    };
    auto add_arc = [&](float cx, float cy, float start_angle, float end_angle, bool include_first) {
        for (int i = 0; i <= steps; ++i) {
            if (i == 0 && !include_first) {
                continue;
            }
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float angle = start_angle + (end_angle - start_angle) * t;
            push_point(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        }
    };

    add_arc(left + radius, top + radius, kPi, 1.5f * kPi, true);
    add_arc(right - radius, top + radius, 1.5f * kPi, 2.0f * kPi, false);
    add_arc(right - radius, bottom - radius, 0.0f, 0.5f * kPi, false);
    add_arc(left + radius, bottom - radius, 0.5f * kPi, kPi, false);
    return count;
}

inline void draw_filled_rect(const Rect& rect, float radius) {
    if (radius <= 0.0f) {
        glBegin(GL_QUADS);
        glVertex2f(rect.x, rect.y);
        glVertex2f(rect.x + rect.w, rect.y);
        glVertex2f(rect.x + rect.w, rect.y + rect.h);
        glVertex2f(rect.x, rect.y + rect.h);
        glEnd();
        return;
    }

    std::array<Point, 64> points{};
    const int count = build_rounded_points(rect, radius, points.data(), static_cast<int>(points.size()));
    if (count < 3) {
        return;
    }

    float center_x = 0.0f;
    float center_y = 0.0f;
    for (int i = 0; i < count; ++i) {
        center_x += points[static_cast<std::size_t>(i)][0];
        center_y += points[static_cast<std::size_t>(i)][1];
    }
    center_x /= static_cast<float>(count);
    center_y /= static_cast<float>(count);

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center_x, center_y);
    for (int i = 0; i < count; ++i) {
        const Point& p = points[static_cast<std::size_t>(i)];
        glVertex2f(p[0], p[1]);
    }
    glVertex2f(points[0][0], points[0][1]);
    glEnd();
}

inline void draw_outline_rect(const Rect& rect, float radius, float thickness) {
    if (radius <= 0.0f) {
        glLineWidth(std::max(1.0f, thickness));
        glBegin(GL_LINE_LOOP);
        glVertex2f(rect.x, rect.y);
        glVertex2f(rect.x + rect.w, rect.y);
        glVertex2f(rect.x + rect.w, rect.y + rect.h);
        glVertex2f(rect.x, rect.y + rect.h);
        glEnd();
        glLineWidth(1.0f);
        return;
    }

    std::array<Point, 64> points{};
    const int count = build_rounded_points(rect, radius, points.data(), static_cast<int>(points.size()));
    if (count < 3) {
        return;
    }

    glLineWidth(std::max(1.0f, thickness));
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < count; ++i) {
        const Point& p = points[static_cast<std::size_t>(i)];
        glVertex2f(p[0], p[1]);
    }
    glEnd();
    glLineWidth(1.0f);
}

inline float text_width(std::size_t glyph_count, float scale) {
    if (glyph_count == 0u) {
        return 0.0f;
    }
    const float advance = 6.0f * scale;
    return advance * static_cast<float>(glyph_count) - scale;
}

inline void draw_text_bitmap(const DrawCommand& cmd, std::string_view text) {
    if (text.empty()) {
        return;
    }

    const float scale = std::max(1.0f, cmd.font_size / 8.0f);
    const float advance = 6.0f * scale;
    const float width = text_width(text.size(), scale);
    const float glyph_h = 7.0f * scale;

    float x = cmd.rect.x;
    if (cmd.align == TextAlign::Center) {
        x += std::max(0.0f, (cmd.rect.w - width) * 0.5f);
    } else if (cmd.align == TextAlign::Right) {
        x += std::max(0.0f, cmd.rect.w - width);
    }
    const float y = cmd.rect.y + std::max(0.0f, (cmd.rect.h - glyph_h) * 0.5f);

    struct BitmapVertex {
        float x;
        float y;
        float r;
        float g;
        float b;
        float a;
    };
    std::vector<BitmapVertex> verts;
    verts.reserve(text.size() * 7u * 5u * 4u);
    auto push_v = [&](float px, float py) {
        verts.push_back(BitmapVertex{px, py, cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a});
    };

    float pen_x = x;
    for (char raw_ch : text) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(raw_ch)));
        if (ch == ' ') {
            pen_x += advance;
            continue;
        }

        const Glyph& glyph = glyph_for(ch);
        for (std::size_t row = 0; row < glyph.size(); ++row) {
            const std::uint8_t bits = glyph[row];
            for (int col = 0; col < 5; ++col) {
                const std::uint8_t mask = static_cast<std::uint8_t>(1u << (4 - col));
                if ((bits & mask) == 0u) {
                    continue;
                }
                const float px = pen_x + static_cast<float>(col) * scale;
                const float py = y + static_cast<float>(row) * scale;
                push_v(px, py);
                push_v(px + scale, py);
                push_v(px + scale, py + scale);
                push_v(px, py + scale);
            }
        }
        pen_x += advance;
    }
    if (!verts.empty()) {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(2, GL_FLOAT, sizeof(BitmapVertex), &verts[0].x);
        glColorPointer(4, GL_FLOAT, sizeof(BitmapVertex), &verts[0].r);
        glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(verts.size()));
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
}

class Renderer {
public:
    explicit Renderer(const AppOptions& options = {})
#ifdef _WIN32
        : text_backend_(resolve_text_backend(options))
#else
        : stb_font_renderer_(std::make_unique<StbFontRenderer>(options))
#endif
    {
#ifdef _WIN32
        if (text_backend_ == AppOptions::TextBackend::Win32) {
            win32_font_renderer_ = std::make_unique<Win32FontRenderer>(options);
        } else {
            stb_font_renderer_ = std::make_unique<StbFontRenderer>(options);
        }
#endif
    }

    void release_gl_resources() const {
#ifdef _WIN32
        if (win32_font_renderer_ != nullptr) {
            win32_font_renderer_->release_gl_resources();
        }
#endif
        if (stb_font_renderer_ != nullptr) {
            stb_font_renderer_->release_gl_resources();
        }
    }

    void render(const std::vector<DrawCommand>& commands, const std::vector<char>& text_arena, int width,
                int height, const Rect* clip_rect = nullptr) const {
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const bool has_outer_clip = clip_rect != nullptr;
        const IRect outer_gl_clip = has_outer_clip ? to_gl_rect(*clip_rect, width, height) : IRect{};
        const bool outer_clip_valid = !has_outer_clip || (outer_gl_clip.w > 0 && outer_gl_clip.h > 0);
        if (!outer_clip_valid) {
            return;
        }

        auto irect_equal = [](const IRect& lhs, const IRect& rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.w == rhs.w && lhs.h == rhs.h;
        };

        bool scissor_enabled = false;
        IRect active_scissor{};
        auto apply_scissor = [&](const IRect* desired) {
            if (desired == nullptr) {
                if (scissor_enabled) {
                    glDisable(GL_SCISSOR_TEST);
                    scissor_enabled = false;
                }
                return;
            }
            if (desired->w <= 0 || desired->h <= 0) {
                if (scissor_enabled) {
                    glDisable(GL_SCISSOR_TEST);
                    scissor_enabled = false;
                }
                return;
            }
            if (!scissor_enabled) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(desired->x, desired->y, desired->w, desired->h);
                active_scissor = *desired;
                scissor_enabled = true;
                return;
            }
            if (!irect_equal(active_scissor, *desired)) {
                glScissor(desired->x, desired->y, desired->w, desired->h);
                active_scissor = *desired;
            }
        };

        visible_indices_.clear();
        if (clip_rect == nullptr || commands.size() < 96u) {
            visible_indices_.reserve(commands.size());
            for (std::size_t i = 0; i < commands.size(); ++i) {
                const Rect visible_rect = command_visible_rect(commands[i]);
                if (clip_rect == nullptr || rect_intersects(visible_rect, *clip_rect)) {
                    visible_indices_.push_back(i);
                }
            }
        } else {
            const int tile_px = 128;
            const int cols = std::max(1, (width + tile_px - 1) / tile_px);
            const int rows = std::max(1, (height + tile_px - 1) / tile_px);
            const std::size_t bucket_count = static_cast<std::size_t>(cols * rows);
            if (spatial_buckets_.size() != bucket_count) {
                spatial_buckets_.assign(bucket_count, std::vector<std::uint32_t>{});
            } else {
                for (std::vector<std::uint32_t>& bucket : spatial_buckets_) {
                    bucket.clear();
                }
            }

            for (std::size_t i = 0; i < commands.size(); ++i) {
                const Rect rect = command_visible_rect(commands[i]);
                if (rect.w <= 0.0f || rect.h <= 0.0f || !rect_intersects(rect, *clip_rect)) {
                    continue;
                }

                const int x0 = std::clamp(static_cast<int>(std::floor(rect.x / tile_px)), 0, cols - 1);
                const int y0 = std::clamp(static_cast<int>(std::floor(rect.y / tile_px)), 0, rows - 1);
                const int x1 = std::clamp(static_cast<int>(std::floor((rect.x + rect.w) / tile_px)), 0, cols - 1);
                const int y1 = std::clamp(static_cast<int>(std::floor((rect.y + rect.h) / tile_px)), 0, rows - 1);
                for (int y = y0; y <= y1; ++y) {
                    for (int x = x0; x <= x1; ++x) {
                        spatial_buckets_[static_cast<std::size_t>(y * cols + x)].push_back(
                            static_cast<std::uint32_t>(i));
                    }
                }
            }

            if (spatial_marks_.size() < commands.size()) {
                spatial_marks_.assign(commands.size(), 0u);
                spatial_mark_id_ = 1u;
            }
            spatial_mark_id_ += 1u;
            if (spatial_mark_id_ == 0u) {
                std::fill(spatial_marks_.begin(), spatial_marks_.end(), 0u);
                spatial_mark_id_ = 1u;
            }

            visible_indices_.reserve(commands.size() / 4u + 8u);
            const int clip_x0 = std::clamp(static_cast<int>(std::floor(clip_rect->x / tile_px)), 0, cols - 1);
            const int clip_y0 = std::clamp(static_cast<int>(std::floor(clip_rect->y / tile_px)), 0, rows - 1);
            const int clip_x1 =
                std::clamp(static_cast<int>(std::floor((clip_rect->x + clip_rect->w) / tile_px)), 0, cols - 1);
            const int clip_y1 =
                std::clamp(static_cast<int>(std::floor((clip_rect->y + clip_rect->h) / tile_px)), 0, rows - 1);
            for (int y = clip_y0; y <= clip_y1; ++y) {
                for (int x = clip_x0; x <= clip_x1; ++x) {
                    const std::vector<std::uint32_t>& bucket =
                        spatial_buckets_[static_cast<std::size_t>(y * cols + x)];
                    for (std::uint32_t idx : bucket) {
                        if (spatial_marks_[idx] == spatial_mark_id_) {
                            continue;
                        }
                        spatial_marks_[idx] = spatial_mark_id_;
                        visible_indices_.push_back(static_cast<std::size_t>(idx));
                    }
                }
            }
            std::sort(visible_indices_.begin(), visible_indices_.end());
        }

        apply_scissor(nullptr);

        struct BatchVertex {
            float x;
            float y;
            float r;
            float g;
            float b;
            float a;
        };
        enum class PendingBatch {
            None,
            Filled,
            Outline,
        };
        PendingBatch pending_batch = PendingBatch::None;
        float pending_outline_thickness = 1.0f;
        std::vector<BatchVertex> filled_batch{};
        std::vector<BatchVertex> outline_batch{};
        filled_batch.reserve(1024);
        outline_batch.reserve(1024);

        auto push_colored_vertex = [](std::vector<BatchVertex>& batch, float x, float y, const Color& color) {
            batch.push_back(BatchVertex{x, y, color.r, color.g, color.b, color.a});
        };

        auto append_filled_rect = [&](const DrawCommand& cmd) {
            if (cmd.radius <= 0.0f) {
                const float x0 = cmd.rect.x;
                const float y0 = cmd.rect.y;
                const float x1 = cmd.rect.x + cmd.rect.w;
                const float y1 = cmd.rect.y + cmd.rect.h;
                push_colored_vertex(filled_batch, x0, y0, cmd.color);
                push_colored_vertex(filled_batch, x1, y0, cmd.color);
                push_colored_vertex(filled_batch, x1, y1, cmd.color);
                push_colored_vertex(filled_batch, x0, y0, cmd.color);
                push_colored_vertex(filled_batch, x1, y1, cmd.color);
                push_colored_vertex(filled_batch, x0, y1, cmd.color);
                return;
            }

            std::array<Point, 64> points{};
            const int count = build_rounded_points(cmd.rect, cmd.radius, points.data(),
                                                   static_cast<int>(points.size()));
            if (count < 3) {
                return;
            }
            float center_x = 0.0f;
            float center_y = 0.0f;
            for (int i = 0; i < count; ++i) {
                center_x += points[static_cast<std::size_t>(i)][0];
                center_y += points[static_cast<std::size_t>(i)][1];
            }
            center_x /= static_cast<float>(count);
            center_y /= static_cast<float>(count);

            for (int i = 0; i < count; ++i) {
                const int next = (i + 1) % count;
                push_colored_vertex(filled_batch, center_x, center_y, cmd.color);
                push_colored_vertex(filled_batch, points[static_cast<std::size_t>(i)][0],
                                    points[static_cast<std::size_t>(i)][1], cmd.color);
                push_colored_vertex(filled_batch, points[static_cast<std::size_t>(next)][0],
                                    points[static_cast<std::size_t>(next)][1], cmd.color);
            }
        };

        auto append_outline_rect = [&](const DrawCommand& cmd) {
            if (cmd.radius <= 0.0f) {
                const float x0 = cmd.rect.x;
                const float y0 = cmd.rect.y;
                const float x1 = cmd.rect.x + cmd.rect.w;
                const float y1 = cmd.rect.y + cmd.rect.h;
                push_colored_vertex(outline_batch, x0, y0, cmd.color);
                push_colored_vertex(outline_batch, x1, y0, cmd.color);
                push_colored_vertex(outline_batch, x1, y0, cmd.color);
                push_colored_vertex(outline_batch, x1, y1, cmd.color);
                push_colored_vertex(outline_batch, x1, y1, cmd.color);
                push_colored_vertex(outline_batch, x0, y1, cmd.color);
                push_colored_vertex(outline_batch, x0, y1, cmd.color);
                push_colored_vertex(outline_batch, x0, y0, cmd.color);
                return;
            }

            std::array<Point, 64> points{};
            const int count = build_rounded_points(cmd.rect, cmd.radius, points.data(),
                                                   static_cast<int>(points.size()));
            if (count < 3) {
                return;
            }
            for (int i = 0; i < count; ++i) {
                const int next = (i + 1) % count;
                push_colored_vertex(outline_batch, points[static_cast<std::size_t>(i)][0],
                                    points[static_cast<std::size_t>(i)][1], cmd.color);
                push_colored_vertex(outline_batch, points[static_cast<std::size_t>(next)][0],
                                    points[static_cast<std::size_t>(next)][1], cmd.color);
            }
        };

        auto append_chevron = [&](const DrawCommand& cmd) {
            const float cx = cmd.rect.x + cmd.rect.w * 0.5f;
            const float cy = cmd.rect.y + cmd.rect.h * 0.5f;
            const float half_w = std::max(2.0f, cmd.rect.w * 0.34f);
            const float half_h = std::max(2.0f, cmd.rect.h * 0.28f);
            const float cos_a = std::cos(cmd.rotation);
            const float sin_a = std::sin(cmd.rotation);
            auto rotate_point = [&](float x, float y) -> Point {
                return Point{
                    cx + x * cos_a - y * sin_a,
                    cy + x * sin_a + y * cos_a,
                };
            };
            const Point p0 = rotate_point(-half_w, -half_h);
            const Point p1 = rotate_point(half_w, 0.0f);
            const Point p2 = rotate_point(-half_w, half_h);
            push_colored_vertex(outline_batch, p0[0], p0[1], cmd.color);
            push_colored_vertex(outline_batch, p1[0], p1[1], cmd.color);
            push_colored_vertex(outline_batch, p1[0], p1[1], cmd.color);
            push_colored_vertex(outline_batch, p2[0], p2[1], cmd.color);
        };

        auto flush_filled_batch = [&]() {
            if (filled_batch.empty()) {
                return;
            }
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_COLOR_ARRAY);
            glVertexPointer(2, GL_FLOAT, sizeof(BatchVertex), &filled_batch[0].x);
            glColorPointer(4, GL_FLOAT, sizeof(BatchVertex), &filled_batch[0].r);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(filled_batch.size()));
            glDisableClientState(GL_COLOR_ARRAY);
            glDisableClientState(GL_VERTEX_ARRAY);
            filled_batch.clear();
        };

        auto flush_outline_batch = [&]() {
            if (outline_batch.empty()) {
                return;
            }
            glLineWidth(std::max(1.0f, pending_outline_thickness));
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_COLOR_ARRAY);
            glVertexPointer(2, GL_FLOAT, sizeof(BatchVertex), &outline_batch[0].x);
            glColorPointer(4, GL_FLOAT, sizeof(BatchVertex), &outline_batch[0].r);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(outline_batch.size()));
            glDisableClientState(GL_COLOR_ARRAY);
            glDisableClientState(GL_VERTEX_ARRAY);
            glLineWidth(1.0f);
            outline_batch.clear();
        };

        auto flush_pending_batch = [&]() {
            if (pending_batch == PendingBatch::Filled) {
                flush_filled_batch();
            } else if (pending_batch == PendingBatch::Outline) {
                flush_outline_batch();
            }
            pending_batch = PendingBatch::None;
        };

        auto scissor_matches = [&](const IRect* desired) {
            if (desired == nullptr) {
                return !scissor_enabled;
            }
            if (!scissor_enabled) {
                return false;
            }
            return irect_equal(active_scissor, *desired);
        };

        for (std::size_t draw_idx : visible_indices_) {
            const DrawCommand& cmd = commands[draw_idx];
            const Rect visible_rect = command_visible_rect(cmd);
            if (clip_rect != nullptr && !rect_intersects(visible_rect, *clip_rect)) {
                continue;
            }
            IRect cmd_gl_clip{};
            const IRect* desired_clip = nullptr;
            if (cmd.has_clip) {
                Rect effective_clip = cmd.clip_rect;
                if (has_outer_clip) {
                    Rect clipped{};
                    if (!rect_intersection(effective_clip, *clip_rect, clipped)) {
                        continue;
                    }
                    effective_clip = clipped;
                }
                cmd_gl_clip = to_gl_rect(effective_clip, width, height);
                if (cmd_gl_clip.w <= 0 || cmd_gl_clip.h <= 0) {
                    continue;
                }
                desired_clip = &cmd_gl_clip;
            } else if (has_outer_clip) {
                desired_clip = &outer_gl_clip;
            }

            switch (cmd.type) {
                case CommandType::FilledRect:
                    if (!scissor_matches(desired_clip)) {
                        flush_pending_batch();
                        apply_scissor(desired_clip);
                    }
                    if (pending_batch != PendingBatch::Filled) {
                        flush_pending_batch();
                        pending_batch = PendingBatch::Filled;
                    }
                    append_filled_rect(cmd);
                    break;
                case CommandType::RectOutline:
                    if (!scissor_matches(desired_clip)) {
                        flush_pending_batch();
                        apply_scissor(desired_clip);
                    }
                    if (pending_batch != PendingBatch::Outline ||
                        !float_eq(pending_outline_thickness, std::max(1.0f, cmd.thickness), 1e-3f)) {
                        flush_pending_batch();
                        pending_batch = PendingBatch::Outline;
                        pending_outline_thickness = std::max(1.0f, cmd.thickness);
                    }
                    append_outline_rect(cmd);
                    break;
                case CommandType::Chevron:
                    if (!scissor_matches(desired_clip)) {
                        flush_pending_batch();
                        apply_scissor(desired_clip);
                    }
                    if (pending_batch != PendingBatch::Outline ||
                        !float_eq(pending_outline_thickness, std::max(1.0f, cmd.thickness), 1e-3f)) {
                        flush_pending_batch();
                        pending_batch = PendingBatch::Outline;
                        pending_outline_thickness = std::max(1.0f, cmd.thickness);
                    }
                    append_chevron(cmd);
                    break;
                case CommandType::Text: {
                    flush_pending_batch();
                    apply_scissor(desired_clip);
                    std::string_view text{};
                    const std::size_t offset = static_cast<std::size_t>(cmd.text_offset);
                    const std::size_t length = static_cast<std::size_t>(cmd.text_length);
                    if (offset + length <= text_arena.size()) {
                        text = std::string_view(text_arena.data() + offset, length);
                    }

#ifdef _WIN32
                    bool rendered = false;
                    if (stb_font_renderer_ != nullptr) {
                        rendered = stb_font_renderer_->draw_text(cmd, text);
                    }
                    if (!rendered && win32_font_renderer_ != nullptr) {
                        rendered = win32_font_renderer_->draw_text(cmd, text);
                    }
                    if (!rendered) {
                        draw_text_bitmap(cmd, text);
                    }
#else
                    if (stb_font_renderer_ == nullptr || !stb_font_renderer_->draw_text(cmd, text)) {
                        draw_text_bitmap(cmd, text);
                    }
#endif
                    break;
                }
            }
        }

        flush_pending_batch();
        apply_scissor(nullptr);
    }

#ifdef _WIN32
    static AppOptions::TextBackend resolve_text_backend(const AppOptions& options) {
        using TextBackend = AppOptions::TextBackend;
        if (options.text_backend == TextBackend::Win32) {
            return TextBackend::Win32;
        }
        if (options.text_backend == TextBackend::Stb) {
#if EUI_ENABLE_STB_TRUETYPE
            return TextBackend::Stb;
#else
            return TextBackend::Win32;
#endif
        }
#if EUI_ENABLE_STB_TRUETYPE
        return TextBackend::Stb;
#else
        return TextBackend::Win32;
#endif
    }
#endif

#ifdef _WIN32
private:
    AppOptions::TextBackend text_backend_{AppOptions::TextBackend::Auto};
    mutable std::unique_ptr<Win32FontRenderer> win32_font_renderer_{};
#endif
private:
    mutable std::unique_ptr<StbFontRenderer> stb_font_renderer_{};
    mutable std::vector<std::vector<std::uint32_t>> spatial_buckets_{};
    mutable std::vector<std::uint32_t> spatial_marks_{};
    mutable std::vector<std::size_t> visible_indices_{};
    mutable std::uint32_t spatial_mark_id_{1u};
};

}  // namespace detail

template <typename BuildUiFn>
int run(BuildUiFn&& build_ui, const AppOptions& options = {}) {
#ifdef _WIN32
    // Best-effort: opt into system/per-monitor DPI awareness before creating the GLFW window.
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
        auto set_context = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_context != nullptr) {
            const HANDLE kPerMonitorAwareV2 = reinterpret_cast<HANDLE>(-4);
            const HANDLE kPerMonitorAware = reinterpret_cast<HANDLE>(-3);
            if (set_context(kPerMonitorAwareV2) == FALSE) {
                set_context(kPerMonitorAware);
            }
        } else {
            using SetProcessDPIAwareFn = BOOL(WINAPI*)();
            auto set_legacy = reinterpret_cast<SetProcessDPIAwareFn>(GetProcAddress(user32, "SetProcessDPIAware"));
            if (set_legacy != nullptr) {
                set_legacy();
            }
        }
    }
#endif

    if (glfwInit() == 0) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#ifdef GLFW_SCALE_TO_MONITOR
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(options.width, options.height, options.title, nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(options.vsync ? 1 : 0);

    detail::RuntimeState runtime{};
    glfwSetWindowUserPointer(window, &runtime);
    glfwSetCharCallback(window, detail::text_input_callback);
    glfwSetKeyCallback(window, detail::key_input_callback);
    glfwSetScrollCallback(window, detail::scroll_callback);

    detail::Renderer renderer(options);
    Context ui;
#ifdef _WIN32
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
    double previous_time = glfwGetTime();
    double next_frame_deadline = previous_time;
    bool redraw_needed = true;

    while (glfwWindowShouldClose(window) == 0) {
        if (options.continuous_render || redraw_needed) {
            if (options.max_fps > 0.0) {
                const double now_wait = glfwGetTime();
                const double wait_s = next_frame_deadline - now_wait;
                if (wait_s > 0.0005) {
                    glfwWaitEventsTimeout(wait_s);
                } else {
                    glfwPollEvents();
                }
            } else {
                glfwPollEvents();
            }
        } else {
            glfwWaitEventsTimeout(std::max(0.001, options.idle_wait_seconds));
        }

        int framebuffer_w = 1;
        int framebuffer_h = 1;
        glfwGetFramebufferSize(window, &framebuffer_w, &framebuffer_h);

        int window_w = 1;
        int window_h = 1;
        glfwGetWindowSize(window, &window_w, &window_h);

        double mouse_x = 0.0;
        double mouse_y = 0.0;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        const float framebuffer_scale_x =
            window_w > 0 ? static_cast<float>(framebuffer_w) / static_cast<float>(window_w) : 1.0f;
        const float framebuffer_scale_y =
            window_h > 0 ? static_cast<float>(framebuffer_h) / static_cast<float>(window_h) : 1.0f;

        float content_scale_x = 1.0f;
        float content_scale_y = 1.0f;
#if defined(GLFW_VERSION_MAJOR) && ((GLFW_VERSION_MAJOR > 3) || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
        glfwGetWindowContentScale(window, &content_scale_x, &content_scale_y);
#endif

        float window_dpi_scale = 1.0f;
#ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(window);
        if (hwnd != nullptr) {
            HMODULE user32_module = GetModuleHandleW(L"user32.dll");
            if (user32_module != nullptr) {
                using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
                auto get_dpi_for_window =
                    reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32_module, "GetDpiForWindow"));
                if (get_dpi_for_window != nullptr) {
                    const UINT dpi = get_dpi_for_window(hwnd);
                    if (dpi > 0u) {
                        window_dpi_scale = static_cast<float>(dpi) / 96.0f;
                    }
                } else {
                    HDC dc = GetDC(hwnd);
                    if (dc != nullptr) {
                        const int dpi_x = GetDeviceCaps(dc, LOGPIXELSX);
                        if (dpi_x > 0) {
                            window_dpi_scale = static_cast<float>(dpi_x) / 96.0f;
                        }
                        ReleaseDC(hwnd, dc);
                    }
                }
            }
        }
#endif

        const float dpi_scale_x =
            std::max(framebuffer_scale_x, std::max(std::max(0.5f, content_scale_x), window_dpi_scale));
        const float dpi_scale_y =
            std::max(framebuffer_scale_y, std::max(std::max(0.5f, content_scale_y), window_dpi_scale));

        const bool left_mouse_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool right_mouse_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        const bool a_down = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        const bool c_down = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
        const bool v_down = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
        const bool x_down = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;
        const bool left_shift_down = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        const bool right_shift_down = glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        const bool ctrl_down = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
                               (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

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
        input.key_shift = left_shift_down || right_shift_down;
        input.key_select_all = ctrl_down && a_down && !runtime.prev_a_key;
        input.key_copy = ctrl_down && c_down && !runtime.prev_c_key;
        input.key_cut = ctrl_down && x_down && !runtime.prev_x_key;
        input.key_paste = ctrl_down && v_down && !runtime.prev_v_key;
        input.text_input = runtime.text_input;
        if (input.key_paste) {
            const char* clipboard = glfwGetClipboardString(window);
            if (clipboard != nullptr) {
                input.clipboard_text = clipboard;
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

        const double now = glfwGetTime();
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
            ui,
            window,
            dt,
            framebuffer_w,
            framebuffer_h,
            window_w,
            window_h,
            dpi_scale_x,
            dpi_scale_y,
            dpi_scale,
            &request_next_frame,
        };
        build_ui(frame_ctx);
        if (ui.consume_repaint_request()) {
            request_next_frame = true;
        }
        ui.take_frame(runtime.curr_commands, runtime.curr_text_arena);
        std::string clipboard_write_text;
        if (ui.consume_clipboard_write(clipboard_write_text)) {
            glfwSetClipboardString(window, clipboard_write_text.c_str());
        }
        redraw_needed = request_next_frame;

        const Color bg = ui.theme().background;
        const auto& commands = runtime.curr_commands;
        const auto& text_arena = runtime.curr_text_arena;
        detail::ensure_cache_texture(runtime, framebuffer_w, framebuffer_h);
        const std::uint64_t frame_hash = detail::hash_frame_payload(commands, bg);

        const bool force_full_redraw =
            framebuffer_changed || !runtime.has_prev_frame || !runtime.has_cache;
        bool has_dirty = false;
        runtime.dirty_regions.clear();
        if (!force_full_redraw && runtime.has_prev_frame && runtime.prev_frame_hash == frame_hash) {
            has_dirty = false;
        } else {
            has_dirty =
                detail::compute_dirty_regions(commands, text_arena, runtime, bg, framebuffer_w, framebuffer_h,
                                              force_full_redraw, runtime.dirty_regions);
        }
        bool prefer_full_redraw = false;
        if (!force_full_redraw && has_dirty) {
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
            runtime.prev_commands.swap(runtime.curr_commands);
            runtime.prev_text_arena.swap(runtime.curr_text_arena);
            runtime.prev_bg = bg;
            runtime.prev_frame_hash = frame_hash;
            runtime.has_prev_frame = true;
            if (options.max_fps > 0.0) {
                const double frame_interval = 1.0 / options.max_fps;
                next_frame_deadline = glfwGetTime() + frame_interval;
            }
            continue;
        }

        if (use_full_redraw) {
            glDisable(GL_SCISSOR_TEST);
            glClearColor(bg.r, bg.g, bg.b, bg.a);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.render(commands, text_arena, framebuffer_w, framebuffer_h, nullptr);
            detail::copy_full_to_cache(runtime, framebuffer_w, framebuffer_h);
        } else {
            detail::draw_cache_texture(runtime, framebuffer_w, framebuffer_h);
            for (const Rect& dirty : runtime.dirty_regions) {
                const detail::IRect gl_dirty = detail::to_gl_rect(dirty, framebuffer_w, framebuffer_h);
                if (gl_dirty.w > 0 && gl_dirty.h > 0) {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(gl_dirty.x, gl_dirty.y, gl_dirty.w, gl_dirty.h);
                    glClearColor(bg.r, bg.g, bg.b, bg.a);
                    glClear(GL_COLOR_BUFFER_BIT);
                    renderer.render(commands, text_arena, framebuffer_w, framebuffer_h, &dirty);
                    detail::copy_region_to_cache(runtime, gl_dirty);
                }
            }
            glDisable(GL_SCISSOR_TEST);
        }

        runtime.prev_commands.swap(runtime.curr_commands);
        runtime.prev_text_arena.swap(runtime.curr_text_arena);
        runtime.prev_bg = bg;
        runtime.prev_frame_hash = frame_hash;
        runtime.has_prev_frame = true;
        glfwSwapBuffers(window);

        if (options.max_fps > 0.0) {
            const double frame_interval = 1.0 / options.max_fps;
            next_frame_deadline = glfwGetTime() + frame_interval;
        }
    }

    renderer.release_gl_resources();
    if (runtime.cache_texture != 0u) {
        glDeleteTextures(1, &runtime.cache_texture);
        runtime.cache_texture = 0u;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

}  // namespace demo

#endif  // EUI_ENABLE_GLFW_OPENGL_BACKEND

}  // namespace eui

#endif  // EUI_H_
