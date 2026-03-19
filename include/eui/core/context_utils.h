#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>
#include <vector>

#include "eui/core/foundation.h"

namespace eui::detail {

inline std::uint64_t context_hash_sv(std::string_view text) {
    std::uint64_t value = 1469598103934665603ull;
    for (char ch : text) {
        value ^= static_cast<std::uint8_t>(ch);
        value *= 1099511628211ull;
    }
    return value;
}

inline std::uint64_t context_hash_mix(std::uint64_t hash, std::uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ull;
    return hash;
}

inline std::uint32_t context_float_bits(float value) {
    std::uint32_t out = 0u;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

inline std::uint64_t context_hash_rect(const Rect& rect) {
    std::uint64_t hash = 1469598103934665603ull;
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(rect.x)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(rect.y)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(rect.w)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(rect.h)));
    return hash;
}

inline std::uint64_t context_hash_color(const Color& color) {
    std::uint64_t hash = 1469598103934665603ull;
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(color.r)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(color.g)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(color.b)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(color.a)));
    return hash;
}

inline std::uint64_t context_hash_graphics_color(const eui::graphics::Color& color) {
    return context_hash_color(to_legacy_color(color));
}

inline std::uint64_t context_hash_brush(const eui::graphics::Brush& brush) {
    std::uint64_t hash = 1469598103934665603ull;
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(brush.kind));
    switch (brush.kind) {
        case eui::graphics::BrushKind::solid:
            hash = context_hash_mix(hash, context_hash_graphics_color(brush.solid));
            break;
        case eui::graphics::BrushKind::linear_gradient:
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(brush.linear.start.x)));
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(brush.linear.start.y)));
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(brush.linear.end.x)));
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(brush.linear.end.y)));
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(brush.linear.stop_count));
            for (std::size_t i = 0; i < brush.linear.stop_count && i < brush.linear.stops.size(); ++i) {
                hash = context_hash_mix(
                    hash, static_cast<std::uint64_t>(context_float_bits(brush.linear.stops[i].position)));
                hash = context_hash_mix(hash, context_hash_graphics_color(brush.linear.stops[i].color));
            }
            break;
        case eui::graphics::BrushKind::radial_gradient:
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(brush.radial.center.x)));
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(brush.radial.center.y)));
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(brush.radial.radius)));
            hash = context_hash_mix(hash, static_cast<std::uint64_t>(brush.radial.stop_count));
            for (std::size_t i = 0; i < brush.radial.stop_count && i < brush.radial.stops.size(); ++i) {
                hash = context_hash_mix(
                    hash, static_cast<std::uint64_t>(context_float_bits(brush.radial.stops[i].position)));
                hash = context_hash_mix(hash, context_hash_graphics_color(brush.radial.stops[i].color));
            }
            break;
        case eui::graphics::BrushKind::none:
        default:
            break;
    }
    return hash;
}

inline std::uint64_t context_hash_transform_3d(const eui::graphics::Transform3D& transform) {
    std::uint64_t hash = 1469598103934665603ull;
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.translation_x)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.translation_y)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.translation_z)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.scale_x)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.scale_y)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.scale_z)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.rotation_x_deg)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.rotation_y_deg)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.rotation_z_deg)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.origin_x)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.origin_y)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.origin_z)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(transform.perspective)));
    return hash;
}

inline std::uint64_t context_hash_command_base(const DrawCommand& cmd) {
    std::uint64_t hash = 1469598103934665603ull;
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(cmd.type));
    hash = context_hash_mix(hash, context_hash_rect(cmd.rect));
    hash = context_hash_mix(hash, context_hash_color(cmd.color));
    hash = context_hash_mix(hash, cmd.payload_hash);
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(cmd.font_size)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(cmd.align));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(cmd.radius)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(cmd.thickness)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(cmd.rotation)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(cmd.blur_radius)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(context_float_bits(cmd.effect_alpha)));
    hash = context_hash_mix(hash, static_cast<std::uint64_t>(cmd.has_clip ? 1u : 0u));
    if (cmd.has_clip) {
        hash = context_hash_mix(hash, context_hash_rect(cmd.clip_rect));
    }
    return hash;
}

template <typename T>
inline void context_retain_vector_hysteresis(std::vector<T>& storage, std::size_t desired_capacity,
                                             std::uint32_t& underuse_counter,
                                             std::uint32_t trigger_frames = 90u) {
    storage.clear();
    if (storage.capacity() < desired_capacity) {
        storage.reserve(desired_capacity);
        underuse_counter = 0u;
        return;
    }

    const bool oversized =
        desired_capacity == 0u ? storage.capacity() > 0u : storage.capacity() > desired_capacity * 2u;
    if (!oversized) {
        underuse_counter = 0u;
        return;
    }

    underuse_counter += 1u;
    if (underuse_counter < trigger_frames) {
        return;
    }

    std::vector<T> trimmed{};
    if (desired_capacity > 0u) {
        trimmed.reserve(desired_capacity);
    }
    storage.swap(trimmed);
    underuse_counter = 0u;
}

template <typename T>
inline void context_trim_live_vector_hysteresis(std::vector<T>& storage, std::size_t desired_capacity,
                                                std::uint32_t& underuse_counter,
                                                std::uint32_t trigger_frames = 90u) {
    if (storage.capacity() < desired_capacity) {
        storage.reserve(desired_capacity);
        underuse_counter = 0u;
        return;
    }

    const std::size_t min_capacity = std::max(desired_capacity, storage.size());
    const bool oversized = min_capacity == 0u ? storage.capacity() > 0u : storage.capacity() > min_capacity * 2u;
    if (!oversized) {
        underuse_counter = 0u;
        return;
    }

    underuse_counter += 1u;
    if (underuse_counter < trigger_frames) {
        return;
    }

    std::vector<T> trimmed{};
    if (min_capacity > 0u) {
        trimmed.reserve(min_capacity);
    }
    trimmed.insert(trimmed.end(), std::make_move_iterator(storage.begin()), std::make_move_iterator(storage.end()));
    storage.swap(trimmed);
    underuse_counter = 0u;
}

inline bool context_intersect_rects(const Rect& lhs, const Rect& rhs, Rect& out) {
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

inline Rect context_expand_rect(const Rect& rect, float dx, float dy) {
    return Rect{
        rect.x - dx,
        rect.y - dy,
        rect.w + dx * 2.0f,
        rect.h + dy * 2.0f,
    };
}

inline Rect context_translate_rect(const Rect& rect, float dx, float dy) {
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

inline Rect context_scale_rect_from_center(const Rect& rect, float scale_x, float scale_y) {
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

inline Color context_scale_alpha(const Color& color, float factor) {
    return Color{
        color.r,
        color.g,
        color.b,
        std::clamp(color.a * factor, 0.0f, 1.0f),
    };
}

inline eui::graphics::Color context_scale_alpha(const eui::graphics::Color& color, float factor) {
    return eui::graphics::Color{
        color.r,
        color.g,
        color.b,
        std::clamp(color.a * factor, 0.0f, 1.0f),
    };
}

inline eui::graphics::Brush context_scale_alpha(const eui::graphics::Brush& brush, float factor) {
    eui::graphics::Brush scaled = brush;
    switch (scaled.kind) {
        case eui::graphics::BrushKind::solid:
            scaled.solid = context_scale_alpha(scaled.solid, factor);
            break;
        case eui::graphics::BrushKind::linear_gradient:
            for (std::size_t i = 0; i < scaled.linear.stop_count && i < scaled.linear.stops.size(); ++i) {
                scaled.linear.stops[i].color = context_scale_alpha(scaled.linear.stops[i].color, factor);
            }
            break;
        case eui::graphics::BrushKind::radial_gradient:
            for (std::size_t i = 0; i < scaled.radial.stop_count && i < scaled.radial.stops.size(); ++i) {
                scaled.radial.stops[i].color = context_scale_alpha(scaled.radial.stops[i].color, factor);
            }
            break;
        case eui::graphics::BrushKind::none:
        default:
            break;
    }
    return scaled;
}

}  // namespace eui::detail
