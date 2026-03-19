#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "eui/graphics.h"

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

inline Color to_legacy_color(const eui::graphics::Color& color) {
    return Color{color.r, color.g, color.b, color.a};
}

inline eui::graphics::Color to_graphics_color(const Color& color) {
    return eui::graphics::Color{color.r, color.g, color.b, color.a};
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

inline Rect to_legacy_rect(const eui::graphics::Rect& rect) {
    return Rect{rect.x, rect.y, rect.w, rect.h};
}

inline eui::graphics::Rect to_graphics_rect(const Rect& rect) {
    return eui::graphics::Rect{rect.x, rect.y, rect.w, rect.h};
}

inline bool transform_3d_is_identity(const eui::graphics::Transform3D& transform) {
    return std::fabs(transform.translation_x) <= 1e-6f &&
           std::fabs(transform.translation_y) <= 1e-6f &&
           std::fabs(transform.translation_z) <= 1e-6f &&
           std::fabs(transform.scale_x - 1.0f) <= 1e-6f &&
           std::fabs(transform.scale_y - 1.0f) <= 1e-6f &&
           std::fabs(transform.scale_z - 1.0f) <= 1e-6f &&
           std::fabs(transform.rotation_x_deg) <= 1e-6f &&
           std::fabs(transform.rotation_y_deg) <= 1e-6f &&
           std::fabs(transform.rotation_z_deg) <= 1e-6f &&
           std::fabs(transform.origin_x) <= 1e-6f &&
           std::fabs(transform.origin_y) <= 1e-6f &&
           std::fabs(transform.origin_z) <= 1e-6f &&
           std::fabs(transform.perspective) <= 1e-6f;
}

struct ProjectedPoint {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

inline ProjectedPoint rotate_point_x(ProjectedPoint point, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return ProjectedPoint{
        point.x,
        point.y * c - point.z * s,
        point.y * s + point.z * c,
    };
}

inline ProjectedPoint rotate_point_y(ProjectedPoint point, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return ProjectedPoint{
        point.x * c + point.z * s,
        point.y,
        -point.x * s + point.z * c,
    };
}

inline ProjectedPoint rotate_point_z(ProjectedPoint point, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return ProjectedPoint{
        point.x * c - point.y * s,
        point.x * s + point.y * c,
        point.z,
    };
}

inline ProjectedPoint project_rect_point_3d(float x, float y, const Rect& rect,
                                            const eui::graphics::Transform3D& transform) {
    const float pivot_x = rect.x + transform.origin_x;
    const float pivot_y = rect.y + transform.origin_y;
    ProjectedPoint point{
        (x - pivot_x) * std::max(0.0f, transform.scale_x),
        (y - pivot_y) * std::max(0.0f, transform.scale_y),
        -transform.origin_z * std::max(0.0f, transform.scale_z),
    };

    point = rotate_point_x(point, transform.rotation_x_deg * 0.01745329252f);
    point = rotate_point_y(point, transform.rotation_y_deg * 0.01745329252f);
    point = rotate_point_z(point, transform.rotation_z_deg * 0.01745329252f);
    point.z += transform.translation_z;

    const float perspective = std::max(0.0f, transform.perspective);
    const float factor = perspective > 1e-4f ? (perspective / std::max(32.0f, perspective - point.z)) : 1.0f;

    return ProjectedPoint{
        pivot_x + transform.translation_x + point.x * factor,
        pivot_y + transform.translation_y + point.y * factor,
        point.z,
    };
}

inline Rect projected_rect_bounds(const Rect& rect, const eui::graphics::Transform3D& transform) {
    if (transform_3d_is_identity(transform)) {
        return rect;
    }

    const ProjectedPoint p0 = project_rect_point_3d(rect.x, rect.y, rect, transform);
    const ProjectedPoint p1 = project_rect_point_3d(rect.x + rect.w, rect.y, rect, transform);
    const ProjectedPoint p2 = project_rect_point_3d(rect.x + rect.w, rect.y + rect.h, rect, transform);
    const ProjectedPoint p3 = project_rect_point_3d(rect.x, rect.y + rect.h, rect, transform);
    const float min_x = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    const float min_y = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    const float max_x = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    const float max_y = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
    return Rect{
        min_x,
        min_y,
        std::max(0.0f, max_x - min_x),
        std::max(0.0f, max_y - min_y),
    };
}

inline Rect apply_rect_transform_2d(const Rect& rect, const eui::graphics::Transform2D& transform) {
    const float scale_x = std::max(0.0f, transform.scale_x);
    const float scale_y = std::max(0.0f, transform.scale_y);
    const float pivot_x = rect.x + transform.origin_x;
    const float pivot_y = rect.y + transform.origin_y;

    const float new_x = pivot_x + (rect.x - pivot_x) * scale_x + transform.translation_x;
    const float new_y = pivot_y + (rect.y - pivot_y) * scale_y + transform.translation_y;
    const float new_w = rect.w * scale_x;
    const float new_h = rect.h * scale_y;

    return Rect{new_x, new_y, std::max(0.0f, new_w), std::max(0.0f, new_h)};
}

inline Rect apply_rect_transform_3d_fallback(const Rect& rect, const eui::graphics::Transform3D& transform) {
    const float scale_x = std::max(0.0f, transform.scale_x);
    const float scale_y = std::max(0.0f, transform.scale_y);
    const float pivot_x = rect.x + transform.origin_x;
    const float pivot_y = rect.y + transform.origin_y;

    const float rot_x = transform.rotation_x_deg * 0.01745329252f;
    const float rot_y = transform.rotation_y_deg * 0.01745329252f;
    const float perspective = std::max(160.0f, transform.perspective);
    const float depth_scale = std::clamp(1.0f + transform.translation_z / perspective, 0.72f, 1.35f);
    const float tilt_scale_x = std::clamp(1.0f - std::abs(std::sin(rot_y)) * 0.32f, 0.58f, 1.0f);
    const float tilt_scale_y = std::clamp(1.0f - std::abs(std::sin(rot_x)) * 0.26f, 0.62f, 1.0f);
    const float final_scale_x = scale_x * depth_scale * tilt_scale_x;
    const float final_scale_y = scale_y * depth_scale * tilt_scale_y;

    const float tilt_offset_x = std::sin(rot_y) * rect.h * 0.16f + transform.translation_z * std::sin(rot_y) * 0.04f;
    const float tilt_offset_y = -std::sin(rot_x) * rect.w * 0.12f - transform.translation_z * std::sin(rot_x) * 0.03f;

    const float new_x = pivot_x + (rect.x - pivot_x) * final_scale_x + transform.translation_x + tilt_offset_x;
    const float new_y = pivot_y + (rect.y - pivot_y) * final_scale_y + transform.translation_y + tilt_offset_y;
    const float new_w = rect.w * final_scale_x;
    const float new_h = rect.h * final_scale_y;

    return Rect{new_x, new_y, std::max(0.0f, new_w), std::max(0.0f, new_h)};
}

struct SplitRects {
    Rect first{};
    Rect second{};
};

enum class FlexAlign {
    Top,
    Center,
    Bottom,
};

struct FlexLength {
    enum class Kind {
        Fixed,
        Flex,
        Content,
    };

    Kind kind{Kind::Flex};
    float value{1.0f};
    float max_value{1.0f};

    static FlexLength Fixed(float px) {
        return FlexLength{Kind::Fixed, std::max(0.0f, px), std::max(0.0f, px)};
    }

    static FlexLength Flex(float weight = 1.0f) {
        return FlexLength{Kind::Flex, std::max(0.0f, weight), std::max(0.0f, weight)};
    }

    static FlexLength Content(float min_width = 0.0f, float max_width = 1000000.0f) {
        const float min_w = std::max(0.0f, min_width);
        const float max_w = std::max(min_w, max_width);
        return FlexLength{Kind::Content, min_w, max_w};
    }
};

inline FlexLength Fixed(float px) {
    return FlexLength::Fixed(px);
}

inline FlexLength Flex(float weight = 1.0f) {
    return FlexLength::Flex(weight);
}

inline FlexLength Content(float min_width = 0.0f, float max_width = 1000000.0f) {
    return FlexLength::Content(min_width, max_width);
}

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
    BackdropBlur,
    Text,
    Chevron,
};

enum class TextAlign {
    Left,
    Center,
    Right,
};

struct DrawCommand {
    static constexpr std::uint32_t k_invalid_payload_index = std::numeric_limits<std::uint32_t>::max();
    CommandType type{CommandType::FilledRect};
    Rect rect{};
    Rect clip_rect{};
    Rect visible_rect{};
    Color color{};
    std::uint64_t payload_hash{0ull};
    std::uint32_t brush_payload_index{k_invalid_payload_index};
    std::uint32_t transform_payload_index{k_invalid_payload_index};
    std::uint32_t text_offset{0};
    std::uint32_t text_length{0};
    float font_size{13.0f};
    TextAlign align{TextAlign::Left};
    float radius{0.0f};
    float thickness{1.0f};
    float rotation{0.0f};
    float blur_radius{0.0f};
    float effect_alpha{1.0f};
    bool has_clip{false};
    std::uint64_t hash{0ull};
};

}  // namespace eui
