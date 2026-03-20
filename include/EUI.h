#ifndef EUI_H_
#define EUI_H_

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "eui/core.h"
#include "eui/app.h"
#include "eui/animation.h"
#include "eui/graphics.h"
#include "eui/renderer.h"
#include "eui/runtime.h"
#include "eui/core/foundation.h"
#include "eui/core/context_state.h"
#include "eui/core/context_utils.h"

#if defined(EUI_PLATFORM_SDL) && !defined(EUI_PLATFORM_SDL2)
#define EUI_PLATFORM_SDL2 1
#endif

#if defined(EUI_BACKEND_VK) && !defined(EUI_BACKEND_VULKAN)
#define EUI_BACKEND_VULKAN 1
#endif

#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND
#ifndef EUI_BACKEND_OPENGL
#define EUI_BACKEND_OPENGL 1
#endif
#ifndef EUI_PLATFORM_GLFW
#define EUI_PLATFORM_GLFW 1
#endif
#endif

#ifdef EUI_ENABLE_SDL2_OPENGL_BACKEND
#ifndef EUI_BACKEND_OPENGL
#define EUI_BACKEND_OPENGL 1
#endif
#ifndef EUI_PLATFORM_SDL2
#define EUI_PLATFORM_SDL2 1
#endif
#endif

#if defined(EUI_PLATFORM_GLFW) && defined(EUI_PLATFORM_SDL2)
#error "EUI only supports one platform macro at a time. Pick EUI_PLATFORM_GLFW or EUI_PLATFORM_SDL2."
#endif

#if defined(EUI_BACKEND_OPENGL) && defined(EUI_BACKEND_VULKAN)
#error "EUI only supports one render backend macro at a time."
#endif

#if (defined(EUI_PLATFORM_GLFW) || defined(EUI_PLATFORM_SDL2)) && \
    !defined(EUI_BACKEND_OPENGL) && !defined(EUI_BACKEND_VULKAN)
#define EUI_BACKEND_OPENGL 1
#endif

#ifdef EUI_BACKEND_VULKAN
#error "EUI_BACKEND_VULKAN is not implemented yet."
#endif

#if defined(EUI_BACKEND_OPENGL) && !defined(EUI_PLATFORM_GLFW) && !defined(EUI_PLATFORM_SDL2) && \
    !defined(EUI_ENABLE_GLFW_OPENGL_BACKEND) && !defined(EUI_ENABLE_SDL2_OPENGL_BACKEND)
#error "EUI_BACKEND_OPENGL requires EUI_PLATFORM_GLFW or EUI_PLATFORM_SDL2."
#endif

#if defined(EUI_BACKEND_OPENGL) && defined(EUI_PLATFORM_GLFW) && !defined(EUI_ENABLE_GLFW_OPENGL_BACKEND)
#define EUI_ENABLE_GLFW_OPENGL_BACKEND 1
#endif

#if defined(EUI_BACKEND_OPENGL) && defined(EUI_PLATFORM_SDL2) && !defined(EUI_ENABLE_SDL2_OPENGL_BACKEND)
#define EUI_ENABLE_SDL2_OPENGL_BACKEND 1
#endif

#if defined(EUI_ENABLE_GLFW_OPENGL_BACKEND) || defined(EUI_ENABLE_SDL2_OPENGL_BACKEND)
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND
#ifdef EUI_OPENGL_ES
#ifndef GLFW_INCLUDE_ES2
#define GLFW_INCLUDE_ES2
#endif
#endif
#include <GLFW/glfw3.h>
#ifdef _WIN32
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#endif
#endif

#ifdef EUI_ENABLE_SDL2_OPENGL_BACKEND
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define EUI_UNDEF_SDL_MAIN_HANDLED
#endif
#if defined(__has_include)
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#ifdef EUI_OPENGL_ES
#include <SDL2/SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif
#elif __has_include(<SDL.h>)
#include <SDL.h>
#ifdef EUI_OPENGL_ES
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif
#else
#error "SDL2 headers were not found but EUI_ENABLE_SDL2_OPENGL_BACKEND is defined."
#endif
#else
#include <SDL.h>
#ifdef EUI_OPENGL_ES
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif
#endif
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
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
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

#ifdef EUI_UNDEF_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef EUI_UNDEF_SDL_MAIN_HANDLED
#endif
#endif

#include "eui/core/context_text_measure.h"

namespace eui {

namespace quick::detail {
struct ContextAccess;
}

class Context {
    friend struct quick::detail::ContextAccess;
public:
    Context() {
        commands_.reserve(1024);
        scope_stack_.reserve(24);
        text_arena_.reserve(16 * 1024);
        brush_payloads_.reserve(128);
        transform_payloads_.reserve(64);
        brush_payload_lookup_.reserve(128);
        transform_payload_lookup_.reserve(64);
        brush_payload_ref_counts_.reserve(128);
        input_buffer_.reserve(64);
        theme_.radius = corner_radius_;
    }

    ~Context() {
        detail::context_release_text_measure_cache(text_measure_);
        detail::context_release_stb_measure_cache(text_measure_);
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
        detail::context_configure_text_measure(text_measure_, backend, family, weight,
                                               font_file, icon_family, icon_file, enable_icon_fallback);
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
            prune_persistent_states();
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
        brush_payloads_.clear();
        transform_payloads_.clear();
        brush_payload_lookup_.clear();
        transform_payload_lookup_.clear();
        brush_payload_ref_counts_.clear();
        flush_row();
        flush_flex_row();
        scope_stack_.clear();
        layout_rect_stack_.clear();
        clip_stack_.clear();
        waterfall_ = WaterfallState{};
        flex_row_ = FlexRowState{};

        content_x_ = 16.0f;
        content_width_ = frame_width_ - 32.0f;
        cursor_y_ = 16.0f;
        layout_rect_ = Rect{content_x_, cursor_y_, content_width_, frame_height_ - 32.0f};
        last_item_rect_ = Rect{};
        panel_id_seed_ = 1469598103934665603ull;
    }

    const std::vector<DrawCommand>& end_frame() {
        finalize_frame_state();
        return commands_;
    }

    void take_frame(std::vector<DrawCommand>& out_commands, std::vector<char>& out_text_arena,
                    std::vector<eui::graphics::Brush>& out_brush_payloads,
                    std::vector<eui::graphics::Transform3D>& out_transform_payloads) {
        finalize_frame_state();
        out_commands.clear();
        out_text_arena.clear();
        out_brush_payloads.clear();
        out_transform_payloads.clear();
        out_commands.swap(commands_);
        out_text_arena.swap(text_arena_);
        out_brush_payloads.swap(brush_payloads_);
        out_transform_payloads.swap(transform_payloads_);
        brush_payload_lookup_.clear();
        transform_payload_lookup_.clear();
        brush_payload_ref_counts_.clear();
        const std::size_t next_cmd_cap =
            std::max<std::size_t>(1024u, out_commands.size() + out_commands.size() / 2u + 32u);
        const std::size_t next_text_cap =
            std::max<std::size_t>(16u * 1024u, out_text_arena.size() + out_text_arena.size() / 2u + 256u);
        const std::size_t next_brush_cap =
            std::max<std::size_t>(128u, out_brush_payloads.size() + out_brush_payloads.size() / 2u + 8u);
        const std::size_t next_transform_cap =
            std::max<std::size_t>(64u, out_transform_payloads.size() + out_transform_payloads.size() / 2u + 8u);
        detail::context_retain_vector_hysteresis(commands_, next_cmd_cap, commands_trim_frames_, 120u);
        detail::context_retain_vector_hysteresis(text_arena_, next_text_cap, text_arena_trim_frames_, 120u);
        detail::context_retain_vector_hysteresis(brush_payloads_, next_brush_cap, brush_payload_trim_frames_, 120u);
        detail::context_retain_vector_hysteresis(transform_payloads_, next_transform_cap,
                                                 transform_payload_trim_frames_, 120u);
        detail::context_retain_vector_hysteresis(brush_payload_lookup_, next_brush_cap,
                                                 brush_payload_lookup_trim_frames_, 120u);
        detail::context_retain_vector_hysteresis(transform_payload_lookup_, next_transform_cap,
                                                 transform_payload_lookup_trim_frames_, 120u);
        detail::context_retain_vector_hysteresis(brush_payload_ref_counts_, next_brush_cap,
                                                 brush_payload_ref_counts_trim_frames_, 120u);
    }

    const std::vector<char>& text_arena() const {
        return text_arena_;
    }

    const std::vector<DrawCommand>& commands() const {
        return commands_;
    }

    const InputState& input_state() const {
        return input_;
    }

    Rect content_rect() const {
        return layout_rect_;
    }

    Rect last_item_rect() const {
        return last_item_rect_;
    }

    float measure_text(std::string_view text, float font_size) const {
        return approx_text_width_until(text, text.size(), std::max(8.0f, font_size));
    }

private:
    struct PayloadLookupEntry {
        std::uint64_t hash{0ull};
        std::uint32_t index{DrawCommand::k_invalid_payload_index};
    };

    void internal_push_layout_rect(const Rect& rect) {
        flush_row();
        flush_flex_row();
        layout_rect_stack_.push_back(LayoutRectState{
            layout_rect_,
            cursor_y_,
            last_item_rect_,
            row_,
            flex_row_,
            waterfall_,
        });
        layout_rect_ = Rect{rect.x, rect.y, std::max(1.0f, rect.w), std::max(1.0f, rect.h)};
        content_x_ = layout_rect_.x;
        content_width_ = layout_rect_.w;
        cursor_y_ = layout_rect_.y;
        row_ = RowState{};
        flex_row_ = FlexRowState{};
        waterfall_ = WaterfallState{};
    }

    void internal_pop_layout_rect() {
        flush_row();
        flush_flex_row();
        if (layout_rect_stack_.empty()) {
            return;
        }
        const LayoutRectState state = layout_rect_stack_.back();
        layout_rect_stack_.pop_back();
        layout_rect_ = state.layout_rect;
        content_x_ = layout_rect_.x;
        content_width_ = layout_rect_.w;
        cursor_y_ = state.cursor_y;
        last_item_rect_ = state.last_item_rect;
        row_ = state.row;
        flex_row_ = state.flex_row;
        waterfall_ = state.waterfall;
    }

    void internal_begin_stack(const Rect& rect) {
        internal_push_layout_rect(rect);
    }

    void internal_end_stack() {
        internal_pop_layout_rect();
    }

    Rect internal_dock_left(float width, float gap = 0.0f) {
        flush_row();
        flush_flex_row();
        const float used_w = std::clamp(width, 0.0f, layout_rect_.w);
        const Rect out{layout_rect_.x, layout_rect_.y, used_w, layout_rect_.h};
        const float consumed = std::min(layout_rect_.w, used_w + std::max(0.0f, gap));
        layout_rect_.x += consumed;
        layout_rect_.w = std::max(0.0f, layout_rect_.w - consumed);
        content_x_ = layout_rect_.x;
        content_width_ = layout_rect_.w;
        cursor_y_ = layout_rect_.y;
        last_item_rect_ = out;
        return out;
    }

    Rect internal_dock_right(float width, float gap = 0.0f) {
        flush_row();
        flush_flex_row();
        const float used_w = std::clamp(width, 0.0f, layout_rect_.w);
        const Rect out{layout_rect_.x + layout_rect_.w - used_w, layout_rect_.y, used_w, layout_rect_.h};
        const float consumed = std::min(layout_rect_.w, used_w + std::max(0.0f, gap));
        layout_rect_.w = std::max(0.0f, layout_rect_.w - consumed);
        content_x_ = layout_rect_.x;
        content_width_ = layout_rect_.w;
        cursor_y_ = layout_rect_.y;
        last_item_rect_ = out;
        return out;
    }

    Rect internal_dock_top(float height, float gap = 0.0f) {
        flush_row();
        flush_flex_row();
        const float used_h = std::clamp(height, 0.0f, layout_rect_.h);
        const Rect out{layout_rect_.x, layout_rect_.y, layout_rect_.w, used_h};
        const float consumed = std::min(layout_rect_.h, used_h + std::max(0.0f, gap));
        layout_rect_.y += consumed;
        layout_rect_.h = std::max(0.0f, layout_rect_.h - consumed);
        content_x_ = layout_rect_.x;
        content_width_ = layout_rect_.w;
        cursor_y_ = layout_rect_.y;
        last_item_rect_ = out;
        return out;
    }

    Rect internal_dock_bottom(float height, float gap = 0.0f) {
        flush_row();
        flush_flex_row();
        const float used_h = std::clamp(height, 0.0f, layout_rect_.h);
        const Rect out{layout_rect_.x, layout_rect_.y + layout_rect_.h - used_h, layout_rect_.w, used_h};
        const float consumed = std::min(layout_rect_.h, used_h + std::max(0.0f, gap));
        layout_rect_.h = std::max(0.0f, layout_rect_.h - consumed);
        content_x_ = layout_rect_.x;
        content_width_ = layout_rect_.w;
        cursor_y_ = layout_rect_.y;
        last_item_rect_ = out;
        return out;
    }

    SplitRects internal_split_h(const Rect& rect, float first_width, float gap = 0.0f) const {
        const float safe_gap = std::max(0.0f, gap);
        const float clamped_first = std::clamp(first_width, 0.0f, std::max(0.0f, rect.w - safe_gap));
        return SplitRects{
            Rect{rect.x, rect.y, clamped_first, rect.h},
            Rect{rect.x + clamped_first + safe_gap, rect.y, std::max(0.0f, rect.w - clamped_first - safe_gap), rect.h},
        };
    }

    SplitRects internal_split_h(float first_width, float gap = 0.0f) const {
        return internal_split_h(layout_rect_, first_width, gap);
    }

    SplitRects internal_split_v(const Rect& rect, float first_height, float gap = 0.0f) const {
        const float safe_gap = std::max(0.0f, gap);
        const float clamped_first = std::clamp(first_height, 0.0f, std::max(0.0f, rect.h - safe_gap));
        return SplitRects{
            Rect{rect.x, rect.y, rect.w, clamped_first},
            Rect{rect.x, rect.y + clamped_first + safe_gap, rect.w, std::max(0.0f, rect.h - clamped_first - safe_gap)},
        };
    }

    SplitRects internal_split_v(float first_height, float gap = 0.0f) const {
        return internal_split_v(layout_rect_, first_height, gap);
    }

    void internal_begin_flex_row(std::initializer_list<FlexLength> items, float gap = 8.0f,
                                 FlexAlign align = FlexAlign::Top) {
        internal_begin_flex_row(std::vector<FlexLength>(items), gap, align);
    }

    void internal_begin_flex_row(const std::vector<FlexLength>& items, float gap = 8.0f,
                                 FlexAlign align = FlexAlign::Top) {
        flush_row();
        flush_flex_row();
        flex_row_.active = true;
        flex_row_.items = items;
        flex_row_.widths.assign(items.size(), 0.0f);
        flex_row_.heights.assign(items.size(), 0.0f);
        flex_row_.item_rects.assign(items.size(), Rect{});
        flex_row_.cmd_begin.assign(items.size(), k_invalid_command_index);
        flex_row_.cmd_end.assign(items.size(), k_invalid_command_index);
        flex_row_.index = 0;
        flex_row_.gap = std::max(0.0f, gap);
        flex_row_.y = cursor_y_;
        flex_row_.row_height = 0.0f;
        flex_row_.align = align;
    }

    void internal_end_flex_row() {
        flush_flex_row();
    }

public:
    void paint_filled_rect(const Rect& rect, const Color& color, float radius = 0.0f) {
        add_filled_rect(rect, color, radius);
    }

    void paint_filled_rect(const Rect& rect, const eui::graphics::Brush& brush, float radius = 0.0f,
                           const eui::graphics::Transform3D* transform_3d = nullptr) {
        add_filled_rect(rect, brush, radius, transform_3d);
    }

    void paint_outline_rect(const Rect& rect, const Color& color, float radius = 0.0f,
                            float thickness = 1.0f) {
        add_outline_rect(rect, color, radius, thickness);
    }

    void paint_outline_rect(const Rect& rect, const Color& color, float radius, float thickness,
                            const eui::graphics::Transform3D* transform_3d) {
        add_outline_rect(rect, color, radius, thickness, transform_3d);
    }

    void paint_backdrop_blur(const Rect& rect, float radius, float blur_radius, float opacity = 1.0f,
                             const eui::graphics::Transform3D* transform_3d = nullptr) {
        add_backdrop_blur(rect, radius, blur_radius, opacity, transform_3d);
    }

    void paint_text(std::string_view text, const Rect& rect, const Color& color, float font_size = 13.0f,
                    TextAlign align = TextAlign::Left, const Rect* clip_rect = nullptr) {
        add_text(text, rect, color, font_size, align, clip_rect);
    }

    void paint_chevron(const Rect& rect, const Color& color, float rotation, float thickness = 1.8f) {
        add_chevron(rect, color, rotation, thickness);
    }

    void push_clip(const Rect& rect) {
        push_clip_rect(rect);
    }

    void pop_clip() {
        pop_clip_rect();
    }

private:
    void finalize_frame_state() {
        flush_row();
        flush_flex_row();
        scope_stack_.clear();
        layout_rect_stack_.clear();
        clip_stack_.clear();
        waterfall_ = WaterfallState{};
        flex_row_ = FlexRowState{};
        active_slider_id_ = input_.mouse_down ? active_slider_id_ : 0;
        active_textarea_scroll_id_ = input_.mouse_down ? active_textarea_scroll_id_ : 0;
        if (!input_.mouse_down) {
            active_scroll_drag_id_ = 0;
            active_scrollbar_drag_id_ = 0;
        }
    }

    void internal_label(std::string_view text, float font_size = 13.0f, bool muted = false, float height = 0.0f) {
        const float resolved_height = std::max(font_size + 6.0f, height);
        const float preferred_width = measure_text(text, font_size) + font_size * 0.6f;
        const Rect rect = next_rect(resolved_height, preferred_width);
        add_text(text, Rect{rect.x, rect.y, rect.w, resolved_height}, muted ? theme_.muted_text : theme_.text,
                 font_size, TextAlign::Left);
    }

    void internal_spacer(float height = 8.0f) {
        flush_row();
        flush_flex_row();
        cursor_y_ += std::max(0.0f, height);
    }

    bool internal_button(std::string_view label, ButtonStyle style = ButtonStyle::Secondary, float height = 34.0f,
                         float text_scale = 1.0f) {
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
        const float probe_height = std::max(2.0f, height);
        const float probe_text_size = icon_like
                                          ? std::clamp(probe_height * 0.72f * k_icon_visual_scale * resolved_text_scale,
                                                       13.0f, 36.0f)
                                          : std::clamp(probe_height * 0.38f * resolved_text_scale, 12.0f, 34.0f);
        const float probe_pad = std::clamp(probe_height * 0.24f, 8.0f, 14.0f);
        const float preferred_width =
            std::max(probe_height,
                     measure_text(draw_label, probe_text_size) + probe_pad * (force_left_align ? 2.8f : 2.2f));
        const Rect rect = next_rect(height, preferred_width);
        const bool hovered = rect.contains(input_.mouse_x, input_.mouse_y);
        const bool held = hovered && input_.mouse_down;
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

    bool internal_tab(std::string_view label, bool selected, float height = 30.0f) {
        const float probe_text_size = std::clamp(std::max(2.0f, height) * 0.42f, 13.0f, 26.0f);
        const float preferred_width = measure_text(label, probe_text_size) + std::max(18.0f, height * 0.9f);
        const Rect rect = next_rect(height, preferred_width);
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

    bool internal_slider_float(std::string_view label, float& value, float min_value, float max_value,
                               int decimals = -1, float height = 40.0f) {
        if (max_value < min_value) {
            std::swap(max_value, min_value);
        }

        const int value_decimals = resolve_decimals(min_value, max_value, decimals);
        const std::uint64_t id = id_for(label) ^ 0x62e2ac4d9d45f7b1ull;
        const float probe_label_font = std::clamp(std::max(2.0f, height) * 0.36f, 13.0f, 24.0f);
        const float probe_value_box_w = std::clamp(std::max(2.0f, height) * 1.8f, 64.0f, 128.0f);
        const float preferred_width =
            measure_text(label, probe_label_font) + probe_value_box_w + std::max(24.0f, height * 1.1f);
        const Rect rect = next_rect(height, preferred_width);
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

    bool internal_input_float(std::string_view label, float& value, float min_value, float max_value,
                              int decimals = 2, float height = 34.0f) {
        if (max_value < min_value) {
            std::swap(max_value, min_value);
        }

        const float probe_label_font = std::clamp(std::max(2.0f, height) * 0.40f, 13.0f, 24.0f);
        const float probe_value_font = std::max(12.0f, probe_label_font - 0.5f);
        const std::string probe_value_text = format_float(value, std::max(0, decimals));
        const float preferred_width = measure_text(label, probe_label_font) +
                                      measure_text(probe_value_text.empty() ? std::string_view("0")
                                                                            : std::string_view(probe_value_text),
                                                   probe_value_font) +
                                      std::max(44.0f, height * 2.2f);
        const Rect rect = next_rect(height, preferred_width);
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

    bool internal_input_text(std::string_view label, std::string& value, float height = 34.0f,
                             std::string_view placeholder = {}, bool align_right = false,
                             float leading_padding = 0.0f) {
        const std::string before = value;
        const float probe_label_font = std::clamp(std::max(2.0f, height) * 0.40f, 13.0f, 24.0f);
        const float probe_value_font = std::max(12.0f, probe_label_font - 0.5f);
        const std::string_view probe_value = value.empty() ? placeholder : std::string_view(value);
        const float preferred_width = measure_text(label, probe_label_font) +
                                      std::max(120.0f, measure_text(probe_value, probe_value_font) + height * 2.0f);
        const Rect rect = next_rect(height, preferred_width);
        const float label_font = std::clamp(rect.h * 0.40f, 13.0f, 24.0f);
        const float value_font = std::max(12.0f, label_font - 0.5f);
        const float input_padding = std::clamp(rect.h * 0.18f, 6.0f, 12.0f);
        const float content_padding = input_padding + std::max(0.0f, leading_padding);
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
            update_mouse_selection(input_rect, value_font, align_right, content_padding, hovered);
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
            draw_text_input_content(input_rect, value_font, align_right, content_padding, theme_.text,
                                    mix(theme_.primary, theme_.input_bg, 0.55f));
        } else if (value.empty() && !placeholder.empty()) {
            draw_static_input_content(input_rect, placeholder, value_font, align_right, content_padding,
                                      theme_.muted_text);
        } else {
            draw_static_input_content(input_rect, value, value_font, align_right, content_padding, theme_.text);
        }

        return value != before;
    }

    void internal_input_readonly(std::string_view label, std::string_view value, float height = 34.0f,
                                 bool align_right = false, float value_font_scale = 1.0f, bool muted = true) {
        const float probe_label_font = std::clamp(std::max(2.0f, height) * 0.40f, 13.0f, 24.0f);
        const float probe_base_value_font = std::clamp(std::max(2.0f, height) * 0.44f, 12.0f, 56.0f);
        const float probe_scale = std::clamp(value_font_scale, 0.5f, 2.2f);
        const float probe_value_font = std::clamp(probe_base_value_font * probe_scale, 12.0f, 72.0f);
        const float preferred_width =
            measure_text(label, probe_label_font) + std::max(96.0f, measure_text(value, probe_value_font) + height * 1.8f);
        const Rect rect = next_rect(height, preferred_width);
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

    struct InternalScrollAreaOptions {
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

    bool internal_begin_scroll_area(std::string_view label, float height) {
        return internal_begin_scroll_area(label, height, InternalScrollAreaOptions{});
    }

    bool internal_begin_scroll_area(std::string_view label, float height, const InternalScrollAreaOptions& options) {
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
        ScrollAreaState& state = touch_scroll_area_state(id);
        const float content_h = std::max(viewport.h, state.content_height);
        const float max_scroll = std::max(0.0f, content_h - viewport.h);
        const bool hovered_outer = outer.contains(input_.mouse_x, input_.mouse_y);
        const bool hovered_view = viewport.contains(input_.mouse_x, input_.mouse_y);

        const float before_scroll = state.scroll;
        const float before_velocity = state.velocity;
        bool needs_animation = false;

        if (hovered_view && std::fabs(input_.mouse_wheel_y) > 1e-6f) {
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

        if (input_.mouse_pressed) {
            if (hovered_thumb) {
                active_scrollbar_drag_id_ = id;
                active_scrollbar_drag_offset_ = input_.mouse_y - thumb.y;
                state.velocity = 0.0f;
            } else if (options.enable_drag && hovered_view) {
                active_scroll_drag_id_ = id;
                active_scroll_drag_last_y_ = input_.mouse_y;
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
        const bool had_outer_flex_row = flex_row_.active;
        const FlexRowState outer_flex_row = flex_row_;
        const WaterfallState outer_waterfall = waterfall_;
        ScopeState scope{};
        scope.kind = ScopeKind::ScrollArea;
        scope.content_x = content_x_;
        scope.content_width = content_width_;
        scope.layout_rect = layout_rect_;
        scope.top_y = outer.y;
        scope.min_height = outer.h;
        scope.had_outer_row = had_outer_row;
        scope.outer_row = outer_row;
        scope.had_outer_flex_row = had_outer_flex_row;
        scope.outer_flex_row = outer_flex_row;
        scope.outer_waterfall = outer_waterfall;
        scope.fixed_rect = outer;
        scope.lock_shell_rect = true;
        scope.scroll_state_id = id;
        scope.scroll_viewport = viewport;
        scope.scroll_content_origin_y = viewport.y - state.scroll;
        scope.pushed_clip = true;
        scope_stack_.push_back(scope);

        push_clip_rect(viewport);
        layout_rect_ = viewport;
        content_x_ = viewport.x;
        content_width_ = viewport.w;
        cursor_y_ = viewport.y - state.scroll;
        row_ = RowState{};
        flex_row_ = FlexRowState{};
        waterfall_ = WaterfallState{};
        return true;
    }

    void internal_end_scroll_area() {
        flush_row();
        flush_flex_row();
        if (scope_stack_.empty()) {
            return;
        }
        if (scope_stack_.back().kind != ScopeKind::ScrollArea) {
            return;
        }
        restore_scope();
    }

    bool internal_text_area(std::string_view label, std::string& text, float height = 170.0f) {
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
        TextAreaState& state = touch_text_area_state(id);
        const bool hovered_box = box_rect.contains(input_.mouse_x, input_.mouse_y);
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
            detail::context_decode_utf8_at(text_view, index, text_view.size(), cp, next);
            const float advance = detail::context_measure_codepoint_advance(text_measure_, cp, text_font);
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

        if (hovered_box && std::fabs(input_.mouse_wheel_y) > 1e-6f) {
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

        if (input_.mouse_pressed && hovered_thumb) {
            active_textarea_scroll_id_ = id;
            active_textarea_drag_offset_ = input_.mouse_y - thumb.y;
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

    void internal_text_area_readonly(std::string_view label, std::string_view text, float height = 170.0f) {
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
            detail::context_decode_utf8_at(text, index, text.size(), cp, next);
            const float advance = detail::context_measure_codepoint_advance(text_measure_, cp, text_font);
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
        TextAreaState& state = touch_text_area_state(id);
        const bool hovered_box = box_rect.contains(input_.mouse_x, input_.mouse_y);
        if (hovered_box && std::fabs(input_.mouse_wheel_y) > 1e-6f) {
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
        if (input_.mouse_pressed && hovered_thumb) {
            active_textarea_scroll_id_ = id;
            active_textarea_drag_offset_ = input_.mouse_y - thumb.y;
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

    void internal_progress(std::string_view label, float ratio, float height = 10.0f) {
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        const float label_h = std::clamp(height * 1.6f, 14.0f, 26.0f);
        const float text_gap = std::max(8.0f, label_h + 4.0f);
        const float preferred_width = measure_text(label, label_h) + measure_text("100%", label_h) + label_h * 2.6f;
        const Rect rect = next_rect(height + text_gap, preferred_width);
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

    bool internal_begin_dropdown(std::string_view label, bool& open, float body_height = 104.0f,
                                 float padding = 10.0f) {
        const float header_height = std::max(34.0f, padding * 3.0f);
        const float probe_header_font = std::clamp(header_height * 0.38f, 13.0f, 24.0f);
        const float probe_header_pad = std::clamp(header_height * 0.28f, 10.0f, 22.0f);
        const float probe_indicator_size = std::clamp(header_height * 0.34f, 10.0f, 18.0f);
        const float preferred_width =
            measure_text(label, probe_header_font) + probe_header_pad * 2.0f + probe_indicator_size + 12.0f;
        const Rect header = next_rect(header_height, preferred_width);
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
        const bool had_outer_flex_row = flex_row_.active;
        const FlexRowState outer_flex_row = flex_row_;
        const WaterfallState outer_waterfall = waterfall_;
        const std::size_t content_cmd_begin = commands_.size();
        ScopeState scope{};
        scope.kind = ScopeKind::DropdownBody;
        scope.content_x = content_x_;
        scope.content_width = content_width_;
        scope.layout_rect = layout_rect_;
        scope.fill_cmd_index = fill_cmd_index;
        scope.outline_cmd_index = outline_cmd_index;
        scope.top_y = body.y;
        scope.min_height = body_height;
        scope.padding = padding;
        scope.had_outer_row = had_outer_row;
        scope.outer_row = outer_row;
        scope.had_outer_flex_row = had_outer_flex_row;
        scope.outer_flex_row = outer_flex_row;
        scope.outer_waterfall = outer_waterfall;
        scope.fixed_rect = body;
        scope.lock_shell_rect = true;
        scope.reveal = reveal;
        scope.glow_outer_cmd_index = body_glow.outer_cmd_index;
        scope.glow_inner_cmd_index = body_glow.inner_cmd_index;
        scope.glow_outer_spread = body_glow.outer_spread;
        scope.glow_inner_spread = body_glow.inner_spread;
        scope.content_cmd_begin = content_cmd_begin;
        scope.show_content = true;
        scope_stack_.push_back(scope);
        layout_rect_ = Rect{
            body.x + padding,
            body.y + padding,
            std::max(10.0f, body.w - 2.0f * padding),
            std::max(10.0f, body.h - 2.0f * padding),
        };
        content_x_ = body.x + padding;
        content_width_ = std::max(10.0f, body.w - 2.0f * padding);
        cursor_y_ = body.y + padding;
        row_ = RowState{};
        flex_row_ = FlexRowState{};
        waterfall_ = WaterfallState{};

        return true;
    }

    void internal_end_dropdown() {
        flush_row();
        flush_flex_row();
        if (scope_stack_.empty()) {
            return;
        }
        restore_scope();
    }

public:
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

    std::uint64_t motion_key(std::string_view label) const {
        return id_for(label);
    }

    std::uint64_t motion_key(std::string_view label, std::uint64_t salt) const {
        return hash_mix(id_for(label), salt);
    }

    std::uint64_t motion_key(std::uint64_t base, std::uint64_t salt) const {
        return hash_mix(base, salt);
    }

    float motion(std::uint64_t id, float target, float speed = 10.0f) {
        return update_motion_value(id, target, speed);
    }

    float motion(std::uint64_t id, float target, float rise_speed, float fall_speed) {
        return update_motion_value(id, target, rise_speed, fall_speed);
    }

    float presence(std::uint64_t id, bool visible, float enter_speed = 12.0f, float exit_speed = 16.0f) {
        return update_motion_value(id, visible ? 1.0f : 0.0f, enter_speed, exit_speed);
    }

    void reset_motion(std::uint64_t id, float value = 0.0f) {
        MotionState& state = touch_motion_state(id);
        state.value = value;
        state.value_initialized = true;
    }

    float motion_value(std::uint64_t id, float fallback = 0.0f) const {
        const auto it = motion_states_.find(id);
        if (it == motion_states_.end() || !it->second.value_initialized) {
            return fallback;
        }
        return it->second.value;
    }

private:
    void refresh_theme() {
        theme_ = make_theme(theme_mode_, primary_color_);
        theme_.radius = corner_radius_;
    }

    using ScopeKind = detail::ContextScopeKind;
    using RowState = detail::ContextRowState;
    using WaterfallState = detail::ContextWaterfallState;
    using FlexRowState = detail::ContextFlexRowState;
    using LayoutRectState = detail::ContextLayoutRectState;
    using TextAreaState = detail::ContextTextAreaState;
    using ScrollAreaState = detail::ContextScrollAreaState;
    using MotionState = detail::ContextMotionState;
    using TextMeasureState = detail::ContextTextMeasureState;
    static constexpr std::size_t k_invalid_command_index = detail::k_context_invalid_command_index;
    using GlowCommandRange = detail::ContextGlowCommandRange;
    using ScopeState = detail::ContextScopeState;

    static std::uint64_t hash_sv(std::string_view text) {
        return detail::context_hash_sv(text);
    }

    static std::uint64_t hash_mix(std::uint64_t hash, std::uint64_t value) {
        return detail::context_hash_mix(hash, value);
    }

    static std::uint32_t float_bits(float value) {
        return detail::context_float_bits(value);
    }

    static std::uint64_t hash_rect(const Rect& rect) {
        return detail::context_hash_rect(rect);
    }

    static std::uint64_t hash_color(const Color& color) {
        return detail::context_hash_color(color);
    }

    static std::uint64_t hash_graphics_color(const eui::graphics::Color& color) {
        return detail::context_hash_graphics_color(color);
    }

    static std::uint64_t hash_brush(const eui::graphics::Brush& brush) {
        return detail::context_hash_brush(brush);
    }

    static std::uint64_t hash_transform_3d(const eui::graphics::Transform3D& transform) {
        return detail::context_hash_transform_3d(transform);
    }

    static std::uint64_t hash_command_base(const DrawCommand& cmd) {
        return detail::context_hash_command_base(cmd);
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

    static bool float_bits_equal(float lhs, float rhs) {
        return detail::context_float_bits(lhs) == detail::context_float_bits(rhs);
    }

    static bool graphics_color_equal(const eui::graphics::Color& lhs, const eui::graphics::Color& rhs) {
        return float_bits_equal(lhs.r, rhs.r) && float_bits_equal(lhs.g, rhs.g) && float_bits_equal(lhs.b, rhs.b) &&
               float_bits_equal(lhs.a, rhs.a);
    }

    static bool graphics_point_equal(const eui::graphics::Point& lhs, const eui::graphics::Point& rhs) {
        return float_bits_equal(lhs.x, rhs.x) && float_bits_equal(lhs.y, rhs.y);
    }

    static bool graphics_color_stop_equal(const eui::graphics::ColorStop& lhs, const eui::graphics::ColorStop& rhs) {
        return float_bits_equal(lhs.position, rhs.position) && graphics_color_equal(lhs.color, rhs.color);
    }

    static bool brush_payload_equal(const eui::graphics::Brush& lhs, const eui::graphics::Brush& rhs) {
        if (lhs.kind != rhs.kind) {
            return false;
        }
        switch (lhs.kind) {
            case eui::graphics::BrushKind::solid:
                return graphics_color_equal(lhs.solid, rhs.solid);
            case eui::graphics::BrushKind::linear_gradient:
                if (!graphics_point_equal(lhs.linear.start, rhs.linear.start) ||
                    !graphics_point_equal(lhs.linear.end, rhs.linear.end) ||
                    lhs.linear.stop_count != rhs.linear.stop_count) {
                    return false;
                }
                for (std::size_t i = 0; i < lhs.linear.stop_count && i < lhs.linear.stops.size(); ++i) {
                    if (!graphics_color_stop_equal(lhs.linear.stops[i], rhs.linear.stops[i])) {
                        return false;
                    }
                }
                return true;
            case eui::graphics::BrushKind::radial_gradient:
                if (!graphics_point_equal(lhs.radial.center, rhs.radial.center) ||
                    !float_bits_equal(lhs.radial.radius, rhs.radial.radius) ||
                    lhs.radial.stop_count != rhs.radial.stop_count) {
                    return false;
                }
                for (std::size_t i = 0; i < lhs.radial.stop_count && i < lhs.radial.stops.size(); ++i) {
                    if (!graphics_color_stop_equal(lhs.radial.stops[i], rhs.radial.stops[i])) {
                        return false;
                    }
                }
                return true;
            case eui::graphics::BrushKind::none:
            default:
                return true;
        }
    }

    static bool transform_payload_equal(const eui::graphics::Transform3D& lhs,
                                        const eui::graphics::Transform3D& rhs) {
        return float_bits_equal(lhs.translation_x, rhs.translation_x) &&
               float_bits_equal(lhs.translation_y, rhs.translation_y) &&
               float_bits_equal(lhs.translation_z, rhs.translation_z) && float_bits_equal(lhs.scale_x, rhs.scale_x) &&
               float_bits_equal(lhs.scale_y, rhs.scale_y) && float_bits_equal(lhs.scale_z, rhs.scale_z) &&
               float_bits_equal(lhs.rotation_x_deg, rhs.rotation_x_deg) &&
               float_bits_equal(lhs.rotation_y_deg, rhs.rotation_y_deg) &&
               float_bits_equal(lhs.rotation_z_deg, rhs.rotation_z_deg) &&
               float_bits_equal(lhs.origin_x, rhs.origin_x) && float_bits_equal(lhs.origin_y, rhs.origin_y) &&
               float_bits_equal(lhs.origin_z, rhs.origin_z) &&
               float_bits_equal(lhs.perspective, rhs.perspective);
    }

    std::uint32_t store_brush_payload(const eui::graphics::Brush& brush) {
        const std::uint64_t hash = hash_brush(brush);
        for (const PayloadLookupEntry& entry : brush_payload_lookup_) {
            if (entry.hash != hash || static_cast<std::size_t>(entry.index) >= brush_payloads_.size()) {
                continue;
            }
            if (!brush_payload_equal(brush_payloads_[entry.index], brush)) {
                continue;
            }
            if (static_cast<std::size_t>(entry.index) < brush_payload_ref_counts_.size()) {
                brush_payload_ref_counts_[entry.index] += 1u;
            }
            return entry.index;
        }
        const std::uint32_t index = static_cast<std::uint32_t>(brush_payloads_.size());
        brush_payloads_.push_back(brush);
        brush_payload_lookup_.push_back(PayloadLookupEntry{hash, index});
        brush_payload_ref_counts_.push_back(1u);
        return index;
    }

    std::uint32_t store_transform_payload(const eui::graphics::Transform3D& transform) {
        const std::uint64_t hash = hash_transform_3d(transform);
        for (const PayloadLookupEntry& entry : transform_payload_lookup_) {
            if (entry.hash != hash || static_cast<std::size_t>(entry.index) >= transform_payloads_.size()) {
                continue;
            }
            if (transform_payload_equal(transform_payloads_[entry.index], transform)) {
                return entry.index;
            }
        }
        const std::uint32_t index = static_cast<std::uint32_t>(transform_payloads_.size());
        transform_payloads_.push_back(transform);
        transform_payload_lookup_.push_back(PayloadLookupEntry{hash, index});
        return index;
    }

    const eui::graphics::Brush* command_brush_payload(const DrawCommand& cmd) const {
        if (cmd.brush_payload_index == DrawCommand::k_invalid_payload_index ||
            static_cast<std::size_t>(cmd.brush_payload_index) >= brush_payloads_.size()) {
            return nullptr;
        }
        return &brush_payloads_[cmd.brush_payload_index];
    }

    const eui::graphics::Transform3D& command_transform(const DrawCommand& cmd) const {
        static const eui::graphics::Transform3D identity{};
        if (cmd.transform_payload_index == DrawCommand::k_invalid_payload_index ||
            static_cast<std::size_t>(cmd.transform_payload_index) >= transform_payloads_.size()) {
            return identity;
        }
        return transform_payloads_[cmd.transform_payload_index];
    }

    void release_command_brush_payload(DrawCommand& cmd) {
        if (cmd.brush_payload_index == DrawCommand::k_invalid_payload_index ||
            static_cast<std::size_t>(cmd.brush_payload_index) >= brush_payload_ref_counts_.size()) {
            cmd.brush_payload_index = DrawCommand::k_invalid_payload_index;
            return;
        }
        if (brush_payload_ref_counts_[cmd.brush_payload_index] > 0u) {
            brush_payload_ref_counts_[cmd.brush_payload_index] -= 1u;
        }
        cmd.brush_payload_index = DrawCommand::k_invalid_payload_index;
    }

    void rebuild_command_payload_hash(DrawCommand& cmd) const {
        std::uint64_t payload_hash = 0ull;
        if (const eui::graphics::Brush* brush = command_brush_payload(cmd)) {
            payload_hash = hash_brush(*brush);
        }
        if (cmd.transform_payload_index != DrawCommand::k_invalid_payload_index) {
            payload_hash = hash_mix(payload_hash, hash_transform_3d(command_transform(cmd)));
        }
        cmd.payload_hash = payload_hash;
    }

    void update_command_visible_rect(DrawCommand& cmd) const {
        Rect bounds = projected_rect_bounds(cmd.rect, command_transform(cmd));
        if (!cmd.has_clip) {
            cmd.visible_rect = bounds;
            return;
        }
        Rect visible{};
        if (!intersect_rects(bounds, cmd.clip_rect, visible)) {
            cmd.visible_rect = Rect{};
            return;
        }
        cmd.visible_rect = visible;
    }

    void refresh_command_metadata(DrawCommand& cmd, bool refresh_payload_hash = false) const {
        if (refresh_payload_hash) {
            rebuild_command_payload_hash(cmd);
        }
        update_command_visible_rect(cmd);
        cmd.hash = hash_command(cmd);
    }

    void assign_command_brush_payload(DrawCommand& cmd, const eui::graphics::Brush& brush) {
        release_command_brush_payload(cmd);
        if (brush.kind == eui::graphics::BrushKind::linear_gradient ||
            brush.kind == eui::graphics::BrushKind::radial_gradient) {
            cmd.brush_payload_index = store_brush_payload(brush);
        }
        rebuild_command_payload_hash(cmd);
    }

    void assign_command_transform_payload(DrawCommand& cmd, const eui::graphics::Transform3D* transform_3d) {
        cmd.transform_payload_index = DrawCommand::k_invalid_payload_index;
        if (transform_3d != nullptr && !transform_3d_is_identity(*transform_3d)) {
            cmd.transform_payload_index = store_transform_payload(*transform_3d);
        }
        rebuild_command_payload_hash(cmd);
    }

    void scale_command_brush_payload_alpha(DrawCommand& cmd, float alpha) {
        if (cmd.brush_payload_index == DrawCommand::k_invalid_payload_index ||
            static_cast<std::size_t>(cmd.brush_payload_index) >= brush_payloads_.size()) {
            return;
        }
        const std::uint32_t current_index = cmd.brush_payload_index;
        const eui::graphics::Brush scaled = scale_alpha(brush_payloads_[current_index], alpha);
        if (brush_payload_equal(brush_payloads_[current_index], scaled)) {
            rebuild_command_payload_hash(cmd);
            return;
        }
        if (static_cast<std::size_t>(current_index) < brush_payload_ref_counts_.size() &&
            brush_payload_ref_counts_[current_index] > 1u) {
            brush_payload_ref_counts_[current_index] -= 1u;
            cmd.brush_payload_index = store_brush_payload(scaled);
            rebuild_command_payload_hash(cmd);
            return;
        }
        brush_payloads_[current_index] = scaled;
        const std::uint64_t scaled_hash = hash_brush(scaled);
        for (PayloadLookupEntry& entry : brush_payload_lookup_) {
            if (entry.index == current_index) {
                entry.hash = scaled_hash;
            }
        }
        rebuild_command_payload_hash(cmd);
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
        return detail::context_intersect_rects(lhs, rhs, out);
    }

    static Rect expand_rect(const Rect& rect, float dx, float dy) {
        return detail::context_expand_rect(rect, dx, dy);
    }

    static Rect translate_rect(const Rect& rect, float dx, float dy) {
        return detail::context_translate_rect(rect, dx, dy);
    }

    static Rect scale_rect_from_center(const Rect& rect, float scale_x, float scale_y) {
        return detail::context_scale_rect_from_center(rect, scale_x, scale_y);
    }

    static Color scale_alpha(const Color& color, float factor) {
        return detail::context_scale_alpha(color, factor);
    }

    static eui::graphics::Color scale_alpha(const eui::graphics::Color& color, float factor) {
        return detail::context_scale_alpha(color, factor);
    }

    static eui::graphics::Brush scale_alpha(const eui::graphics::Brush& brush, float factor) {
        return detail::context_scale_alpha(brush, factor);
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

    TextAreaState& touch_text_area_state(std::uint64_t id) {
        TextAreaState& state = text_area_state_[id];
        state.last_touched_frame = motion_frame_id_;
        return state;
    }

    ScrollAreaState& touch_scroll_area_state(std::uint64_t id) {
        ScrollAreaState& state = scroll_area_state_[id];
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

    float update_motion_value(std::uint64_t id, float target, float rise_speed, float fall_speed) {
        MotionState& state = touch_motion_state(id);
        if (!state.value_initialized) {
            state.value = target;
            state.value_initialized = true;
            return target;
        }
        const float speed = (target >= state.value) ? rise_speed : fall_speed;
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

    void prune_persistent_states() {
        for (auto it = text_area_state_.begin(); it != text_area_state_.end();) {
            const std::uint64_t age = motion_frame_id_ - it->second.last_touched_frame;
            const bool active =
                active_input_id_ == it->first || active_textarea_scroll_id_ == it->first;
            const bool settled = near_value(it->second.scroll) && it->second.preferred_x < 0.0f;
            if (!active && (age > 1800u || (age > 240u && settled))) {
                it = text_area_state_.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = scroll_area_state_.begin(); it != scroll_area_state_.end();) {
            const std::uint64_t age = motion_frame_id_ - it->second.last_touched_frame;
            const bool active =
                active_scroll_drag_id_ == it->first || active_scrollbar_drag_id_ == it->first;
            const bool settled = near_value(it->second.scroll) && near_value(it->second.velocity) &&
                                 near_value(it->second.content_height);
            if (!active && (age > 1800u || (age > 360u && settled))) {
                it = scroll_area_state_.erase(it);
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

    float approx_text_width_until(std::string_view text, std::size_t byte_index, float font_size) const {
        return detail::context_text_width_until(text_measure_, text, byte_index, font_size);
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
            detail::context_decode_utf8_at(text, index, text.size(), cp, next);
            const float advance = detail::context_measure_codepoint_advance(text_measure_, cp, font_size);
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

    void shift_command_range_y(std::size_t begin_index, std::size_t end_index, float dy) {
        if (begin_index == k_invalid_command_index || begin_index >= commands_.size() || std::fabs(dy) < 1e-3f) {
            return;
        }
        end_index = std::min(end_index, commands_.size());
        if (end_index <= begin_index) {
            return;
        }
        for (std::size_t i = begin_index; i < end_index; ++i) {
            DrawCommand& cmd = commands_[i];
            cmd.rect.y += dy;
            if (cmd.has_clip) {
                cmd.clip_rect.y += dy;
            }
            refresh_command_metadata(cmd, false);
        }
    }

    void finalize_last_flex_item_command_range() {
        if (!flex_row_.active || flex_row_.items.empty() || flex_row_.index <= 0) {
            return;
        }
        const std::size_t item_index = static_cast<std::size_t>(flex_row_.index - 1);
        if (item_index >= flex_row_.cmd_begin.size() || item_index >= flex_row_.cmd_end.size()) {
            return;
        }
        if (flex_row_.cmd_begin[item_index] == k_invalid_command_index) {
            flex_row_.cmd_begin[item_index] = commands_.size();
        }
        flex_row_.cmd_end[item_index] = commands_.size();
    }

    void register_flex_item_height(float height) {
        if (!flex_row_.active || flex_row_.items.empty() || flex_row_.index <= 0) {
            return;
        }
        const std::size_t item_index = static_cast<std::size_t>(flex_row_.index - 1);
        if (item_index >= flex_row_.heights.size() || item_index >= flex_row_.item_rects.size()) {
            return;
        }
        const float resolved_height = std::max(0.0f, height);
        flex_row_.heights[item_index] = std::max(flex_row_.heights[item_index], resolved_height);
        flex_row_.item_rects[item_index].h = std::max(flex_row_.item_rects[item_index].h, resolved_height);
        flex_row_.row_height = std::max(flex_row_.row_height, flex_row_.item_rects[item_index].h);
    }

    void flush_flex_row() {
        if (!flex_row_.active) {
            return;
        }

        finalize_last_flex_item_command_range();
        const float row_height = std::max(0.0f, flex_row_.row_height);
        for (std::size_t i = 0; i < flex_row_.item_rects.size(); ++i) {
            Rect& item_rect = flex_row_.item_rects[i];
            const float item_height = std::max(item_rect.h, i < flex_row_.heights.size() ? flex_row_.heights[i] : 0.0f);
            if (item_height <= 0.0f) {
                continue;
            }

            float offset_y = 0.0f;
            if (flex_row_.align == FlexAlign::Center) {
                offset_y = (row_height - item_height) * 0.5f;
            } else if (flex_row_.align == FlexAlign::Bottom) {
                offset_y = row_height - item_height;
            }
            if (std::fabs(offset_y) > 1e-3f) {
                item_rect.y += offset_y;
                const std::size_t begin_index =
                    i < flex_row_.cmd_begin.size() ? flex_row_.cmd_begin[i] : k_invalid_command_index;
                const std::size_t end_index =
                    i < flex_row_.cmd_end.size() && flex_row_.cmd_end[i] != k_invalid_command_index
                        ? flex_row_.cmd_end[i]
                        : commands_.size();
                shift_command_range_y(begin_index, end_index, offset_y);
            }
        }

        cursor_y_ = flex_row_.index > 0 ? (flex_row_.y + row_height + item_spacing_) : flex_row_.y;
        flex_row_ = FlexRowState{};
    }

    void restore_scope() {
        const ScopeState scope = scope_stack_.back();
        scope_stack_.pop_back();

        layout_rect_ = scope.layout_rect;
        content_x_ = scope.content_x;
        content_width_ = scope.content_width;
        row_ = RowState{};
        flex_row_ = FlexRowState{};
        waterfall_ = scope.outer_waterfall;

        if (scope.kind == ScopeKind::Card || scope.kind == ScopeKind::DropdownBody) {
            if (scope.pushed_clip) {
                pop_clip_rect();
            }

            const float needed_height = std::max(0.0f, (cursor_y_ - scope.top_y) + scope.padding);
            const float final_height = scope.lock_shell_rect && scope.fixed_rect.h > 0.0f
                                           ? scope.fixed_rect.h
                                           : std::max(scope.min_height, needed_height);
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
                refresh_command_metadata(commands_[scope.fill_cmd_index], false);
            }
            if (scope.outline_cmd_index < commands_.size()) {
                commands_[scope.outline_cmd_index].rect.y = scope.top_y + shell_offset_y;
                commands_[scope.outline_cmd_index].rect.h = display_height;
                refresh_command_metadata(commands_[scope.outline_cmd_index], false);
            }
            if (scope.glow_outer_cmd_index != k_invalid_command_index && scope.glow_outer_cmd_index < commands_.size()) {
                commands_[scope.glow_outer_cmd_index].rect.y =
                    scope.top_y + shell_offset_y - scope.glow_outer_spread;
                commands_[scope.glow_outer_cmd_index].rect.h = display_height + scope.glow_outer_spread * 2.0f;
                refresh_command_metadata(commands_[scope.glow_outer_cmd_index], false);
            }
            if (scope.glow_inner_cmd_index != k_invalid_command_index && scope.glow_inner_cmd_index < commands_.size()) {
                commands_[scope.glow_inner_cmd_index].rect.y =
                    scope.top_y + shell_offset_y - scope.glow_inner_spread;
                commands_[scope.glow_inner_cmd_index].rect.h = display_height + scope.glow_inner_spread * 2.0f;
                refresh_command_metadata(commands_[scope.glow_inner_cmd_index], false);
            }
            if (animate_dropdown && (content_offset_y > 1e-3f || content_alpha < 0.999f) &&
                scope.content_cmd_begin < commands_.size()) {
                for (std::size_t i = scope.content_cmd_begin; i < commands_.size(); ++i) {
                    DrawCommand& cmd = commands_[i];
                    cmd.rect.y += content_offset_y;
                    cmd.color = scale_alpha(cmd.color, content_alpha);
                    scale_command_brush_payload_alpha(cmd, content_alpha);
                    cmd.effect_alpha = std::clamp(cmd.effect_alpha * content_alpha, 0.0f, 1.0f);
                    if (cmd.has_clip) {
                        cmd.clip_rect.y += content_offset_y;
                    }
                    refresh_command_metadata(cmd, false);
                }
            }

            last_item_rect_ =
                Rect{scope.fixed_rect.x, scope.top_y + shell_offset_y, scope.fixed_rect.w, display_height};
            if (scope.in_waterfall && waterfall_.active && scope.column_index >= 0 &&
                scope.column_index < static_cast<int>(waterfall_.column_y.size())) {
                waterfall_.column_y[static_cast<std::size_t>(scope.column_index)] =
                    scope.top_y + display_height + item_spacing_;
                cursor_y_ = waterfall_.start_y;
            } else if (scope.had_outer_flex_row) {
                flex_row_ = scope.outer_flex_row;
                register_flex_item_height(display_height);
                cursor_y_ = flex_row_.y;
                if (flex_row_.index >= static_cast<int>(flex_row_.items.size())) {
                    flush_flex_row();
                }
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

            last_item_rect_ = scope.fixed_rect;
            if (scope.had_outer_flex_row) {
                flex_row_ = scope.outer_flex_row;
                register_flex_item_height(scope.fixed_rect.h);
                cursor_y_ = flex_row_.y;
                if (flex_row_.index >= static_cast<int>(flex_row_.items.size())) {
                    flush_flex_row();
                }
            } else if (scope.had_outer_row) {
                row_ = scope.outer_row;
                row_.max_height = std::max(row_.max_height, scope.fixed_rect.h);
                cursor_y_ = row_.y;
                if (row_.index >= row_.columns) {
                    flush_row();
                }
            } else {
                cursor_y_ = scope.top_y + scope.fixed_rect.h + item_spacing_;
            }
            return;
        }

        last_item_rect_ = scope.fixed_rect;
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

    Rect next_rect(float height, float preferred_width) {
        height = std::max(2.0f, height);
        preferred_width = std::max(0.0f, preferred_width);

        if (flex_row_.active) {
            finalize_last_flex_item_command_range();
            if (flex_row_.index >= static_cast<int>(flex_row_.items.size())) {
                flush_flex_row();
            }
            if (flex_row_.active && !flex_row_.items.empty()) {
                const std::size_t item_index = static_cast<std::size_t>(flex_row_.index);
                float consumed_width = 0.0f;
                for (std::size_t i = 0; i < item_index && i < flex_row_.widths.size(); ++i) {
                    consumed_width += flex_row_.widths[i];
                }

                const float remaining_gaps =
                    flex_row_.gap * static_cast<float>(std::max<int>(0, static_cast<int>(flex_row_.items.size()) -
                                                                            static_cast<int>(item_index) - 1));
                const float remaining_space =
                    std::max(0.0f, content_width_ - consumed_width -
                                        flex_row_.gap * static_cast<float>(item_index) - remaining_gaps);

                float following_nonflex_min = 0.0f;
                float remaining_flex_weight = 0.0f;
                for (std::size_t i = item_index; i < flex_row_.items.size(); ++i) {
                    const FlexLength& length = flex_row_.items[i];
                    if (length.kind == FlexLength::Kind::Flex) {
                        remaining_flex_weight += std::max(0.0f, length.value);
                    } else if (i > item_index) {
                        following_nonflex_min += length.kind == FlexLength::Kind::Fixed ? std::max(0.0f, length.value)
                                                                                        : std::max(0.0f, length.value);
                    }
                }

                const FlexLength& length = flex_row_.items[item_index];
                const float available_for_current = std::max(0.0f, remaining_space - following_nonflex_min);
                float width = 0.0f;
                if (length.kind == FlexLength::Kind::Fixed) {
                    width = std::min(std::max(0.0f, length.value), available_for_current);
                } else if (length.kind == FlexLength::Kind::Content) {
                    const float desired = std::clamp(preferred_width > 0.0f ? preferred_width : length.value,
                                                     std::max(0.0f, length.value), std::max(length.value, length.max_value));
                    width = std::min(desired, available_for_current);
                } else if (remaining_flex_weight > 1e-4f) {
                    width = std::max(0.0f, remaining_space - following_nonflex_min) *
                            (std::max(0.0f, length.value) / remaining_flex_weight);
                }

                const Rect rect{
                    content_x_ + consumed_width + flex_row_.gap * static_cast<float>(item_index),
                    flex_row_.y,
                    std::max(0.0f, width),
                    height,
                };
                if (item_index < flex_row_.widths.size()) {
                    flex_row_.widths[item_index] = rect.w;
                }
                if (item_index < flex_row_.heights.size()) {
                    flex_row_.heights[item_index] = height;
                }
                if (item_index < flex_row_.item_rects.size()) {
                    flex_row_.item_rects[item_index] = rect;
                }
                if (item_index < flex_row_.cmd_begin.size()) {
                    flex_row_.cmd_begin[item_index] = commands_.size();
                }
                if (item_index < flex_row_.cmd_end.size()) {
                    flex_row_.cmd_end[item_index] = k_invalid_command_index;
                }
                flex_row_.row_height = std::max(flex_row_.row_height, height);
                flex_row_.index += 1;
                cursor_y_ = flex_row_.y;
                last_item_rect_ = rect;
                return rect;
            }
        }

        if (!row_.active) {
            Rect rect{content_x_, cursor_y_, content_width_, height};
            cursor_y_ += height + item_spacing_;
            last_item_rect_ = rect;
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
        last_item_rect_ = rect;
        return rect;
    }

    Rect next_rect(float height) {
        return next_rect(height, 0.0f);
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
        assign_command_brush_payload(cmd, eui::graphics::Brush{});
        if (!apply_clip_to_command(cmd, nullptr)) {
            return;
        }
        refresh_command_metadata(cmd, false);
        commands_.push_back(std::move(cmd));
    }

    void add_filled_rect(const Rect& rect, const eui::graphics::Brush& brush, float radius = 0.0f,
                         const eui::graphics::Transform3D* transform_3d = nullptr) {
        DrawCommand cmd;
        cmd.type = CommandType::FilledRect;
        cmd.rect = rect;
        const eui::graphics::Brush scaled_brush = scale_alpha(brush, global_alpha_);
        bool has_visible_alpha = false;

        switch (scaled_brush.kind) {
            case eui::graphics::BrushKind::solid:
                cmd.color = to_legacy_color(scaled_brush.solid);
                has_visible_alpha = scaled_brush.solid.a > 1e-4f;
                break;
            case eui::graphics::BrushKind::linear_gradient:
                if (scaled_brush.linear.stop_count > 0u) {
                    cmd.color = to_legacy_color(scaled_brush.linear.stops[0].color);
                }
                for (std::size_t i = 0; i < scaled_brush.linear.stop_count && i < scaled_brush.linear.stops.size();
                     ++i) {
                    has_visible_alpha = has_visible_alpha || scaled_brush.linear.stops[i].color.a > 1e-4f;
                }
                break;
            case eui::graphics::BrushKind::radial_gradient:
                if (scaled_brush.radial.stop_count > 0u) {
                    cmd.color = to_legacy_color(scaled_brush.radial.stops[0].color);
                }
                for (std::size_t i = 0; i < scaled_brush.radial.stop_count && i < scaled_brush.radial.stops.size();
                     ++i) {
                    has_visible_alpha = has_visible_alpha || scaled_brush.radial.stops[i].color.a > 1e-4f;
                }
                break;
            case eui::graphics::BrushKind::none:
            default:
                break;
        }

        if (!has_visible_alpha) {
            return;
        }
        cmd.radius = std::max(0.0f, radius);
        assign_command_brush_payload(cmd, scaled_brush);
        assign_command_transform_payload(cmd, transform_3d);
        if (!apply_clip_to_command(cmd, nullptr)) {
            return;
        }
        refresh_command_metadata(cmd, false);
        commands_.push_back(std::move(cmd));
    }

    void add_outline_rect(const Rect& rect, const Color& color, float radius = 0.0f,
                          float thickness = 1.0f) {
        add_outline_rect(rect, color, radius, thickness, nullptr);
    }

    void add_outline_rect(const Rect& rect, const Color& color, float radius,
                          float thickness, const eui::graphics::Transform3D* transform_3d) {
        DrawCommand cmd;
        cmd.type = CommandType::RectOutline;
        cmd.rect = rect;
        cmd.color = scale_alpha(color, global_alpha_);
        if (cmd.color.a <= 1e-4f) {
            return;
        }
        cmd.radius = std::max(0.0f, radius);
        cmd.thickness = std::max(1.0f, thickness);
        assign_command_brush_payload(cmd, eui::graphics::Brush{});
        assign_command_transform_payload(cmd, transform_3d);
        if (!apply_clip_to_command(cmd, nullptr)) {
            return;
        }
        refresh_command_metadata(cmd, false);
        commands_.push_back(std::move(cmd));
    }

    void add_backdrop_blur(const Rect& rect, float radius, float blur_radius, float opacity,
                           const eui::graphics::Transform3D* transform_3d) {
        const float effective_radius = std::max(0.0f, blur_radius);
        const float effective_alpha = std::clamp(opacity * global_alpha_, 0.0f, 1.0f);
        if (effective_radius <= 1e-4f || effective_alpha <= 1e-4f) {
            return;
        }

        DrawCommand cmd;
        cmd.type = CommandType::BackdropBlur;
        cmd.rect = rect;
        cmd.radius = std::max(0.0f, radius);
        cmd.blur_radius = effective_radius;
        cmd.effect_alpha = effective_alpha;
        assign_command_transform_payload(cmd, transform_3d);
        if (!apply_clip_to_command(cmd, nullptr)) {
            return;
        }
        refresh_command_metadata(cmd, false);
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
        refresh_command_metadata(cmd, false);
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
        refresh_command_metadata(cmd, false);
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
    std::vector<eui::graphics::Brush> brush_payloads_{};
    std::vector<eui::graphics::Transform3D> transform_payloads_{};
    std::vector<PayloadLookupEntry> brush_payload_lookup_{};
    std::vector<PayloadLookupEntry> transform_payload_lookup_{};
    std::vector<std::uint32_t> brush_payload_ref_counts_{};
    std::vector<LayoutRectState> layout_rect_stack_{};

    RowState row_{};
    FlexRowState flex_row_{};
    WaterfallState waterfall_{};
    Rect layout_rect_{16.0f, 16.0f, 800.0f, 600.0f};
    Rect last_item_rect_{};
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
    mutable TextMeasureState text_measure_{};
    bool repaint_requested_{false};
    float global_alpha_{1.0f};
    bool clipboard_write_pending_{false};
    std::string clipboard_write_text_{};
    std::unordered_map<std::uint64_t, TextAreaState> text_area_state_{};
    std::unordered_map<std::uint64_t, ScrollAreaState> scroll_area_state_{};
    std::unordered_map<std::uint64_t, MotionState> motion_states_{};
    std::uint64_t motion_frame_id_{0u};
    std::uint32_t commands_trim_frames_{0u};
    std::uint32_t text_arena_trim_frames_{0u};
    std::uint32_t brush_payload_trim_frames_{0u};
    std::uint32_t transform_payload_trim_frames_{0u};
    std::uint32_t brush_payload_lookup_trim_frames_{0u};
    std::uint32_t transform_payload_lookup_trim_frames_{0u};
    std::uint32_t brush_payload_ref_counts_trim_frames_{0u};
    std::vector<Rect> clip_stack_{};
    Rect panel_rect_{};
};

#if defined(EUI_ENABLE_GLFW_OPENGL_BACKEND) || defined(EUI_ENABLE_SDL2_OPENGL_BACKEND)

namespace app {

namespace detail {
#include "eui/app/detail/runtime_state_detail.inl"
#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND
#include "eui/app/detail/glfw_runtime_detail.inl"
#endif
#ifdef EUI_ENABLE_SDL2_OPENGL_BACKEND
#include "eui/app/detail/sdl2_runtime_detail.inl"
#endif
#include "eui/app/detail/modern_gl_detail.inl"
#include "eui/app/detail/dirty_cache_detail.inl"

#include "eui/app/detail/text_support_detail.inl"
#include "eui/app/detail/font_renderers_detail.inl"
#include "eui/app/detail/opengl_renderer_detail.inl"
}  // namespace detail

#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND
#include "eui/app/detail/glfw_run_loop_detail.inl"
#endif
#ifdef EUI_ENABLE_SDL2_OPENGL_BACKEND
#include "eui/app/detail/sdl2_run_loop_detail.inl"
#endif
#include "eui/app/detail/app_entry_detail.inl"
}  // namespace app

#endif  // defined(EUI_ENABLE_GLFW_OPENGL_BACKEND) || defined(EUI_ENABLE_SDL2_OPENGL_BACKEND)

}  // namespace eui

#include "eui/quick.h"

#endif  // EUI_H_
