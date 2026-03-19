#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include "EUI.h"
#include "eui/graphics.h"

namespace eui::quick::detail {

inline float average_corner_radius(const eui::graphics::CornerRadius& radius) {
    return std::max(0.0f, (radius.top_left + radius.top_right + radius.bottom_right + radius.bottom_left) * 0.25f);
}

inline bool brush_primary_color(const eui::graphics::Brush& brush, eui::graphics::Color& out_color) {
    switch (brush.kind) {
        case eui::graphics::BrushKind::solid:
            out_color = brush.solid;
            return true;
        case eui::graphics::BrushKind::linear_gradient:
            if (brush.linear.stop_count > 0u) {
                out_color = brush.linear.stops[0].color;
                return true;
            }
            return false;
        case eui::graphics::BrushKind::radial_gradient:
            if (brush.radial.stop_count > 0u) {
                out_color = brush.radial.stops[0].color;
                return true;
            }
            return false;
        case eui::graphics::BrushKind::none:
        default:
            return false;
    }
}

inline eui::graphics::Transform3D combine_rect_transforms(const eui::graphics::Transform2D& transform_2d,
                                                          const eui::graphics::Transform3D& transform_3d) {
    eui::graphics::Transform3D combined = transform_3d;
    combined.translation_x += transform_2d.translation_x;
    combined.translation_y += transform_2d.translation_y;
    combined.scale_x *= std::max(0.0f, transform_2d.scale_x);
    combined.scale_y *= std::max(0.0f, transform_2d.scale_y);
    combined.rotation_z_deg += transform_2d.rotation_deg;

    if (std::fabs(combined.origin_x) <= 1e-6f) {
        combined.origin_x = transform_2d.origin_x;
    }
    if (std::fabs(combined.origin_y) <= 1e-6f) {
        combined.origin_y = transform_2d.origin_y;
    }

    return combined;
}

inline eui::Rect resolve_rect(const eui::graphics::RectanglePrimitive& primitive) {
    const eui::Rect rect = eui::to_legacy_rect(primitive.rect);
    const eui::graphics::Transform3D combined =
        combine_rect_transforms(primitive.transform_2d, primitive.transform_3d);
    return eui::projected_rect_bounds(rect, combined);
}

inline eui::Rect resolve_rect(const eui::graphics::ImagePrimitive& primitive) {
    eui::Rect rect = eui::to_legacy_rect(primitive.rect);
    rect = eui::apply_rect_transform_2d(rect, primitive.transform_2d);
    rect = eui::apply_rect_transform_3d_fallback(rect, primitive.transform_3d);
    return rect;
}

inline eui::Rect resolve_rect(const eui::graphics::IconPrimitive& primitive) {
    eui::Rect rect = eui::to_legacy_rect(primitive.rect);
    rect = eui::apply_rect_transform_2d(rect, primitive.transform_2d);
    rect = eui::apply_rect_transform_3d_fallback(rect, primitive.transform_3d);
    return rect;
}

inline void paint_shadow_approx(eui::Context& ui, const eui::Rect& rect, float radius,
                                const eui::graphics::Shadow& shadow, float opacity) {
    const float blur = std::max(0.0f, shadow.blur_radius);
    if (blur <= 0.0f && shadow.spread <= 0.0f) {
        return;
    }

    const int layers = std::clamp(static_cast<int>(blur / 4.5f) + 6, 6, 16);
    const eui::Color base = eui::to_legacy_color(shadow.color);
    for (int i = layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(layers);
        const float grow = std::max(0.0f, shadow.spread + blur * (0.10f + t * 0.82f));
        const eui::Rect layer{
            rect.x + shadow.offset_x - grow,
            rect.y + shadow.offset_y - grow,
            rect.w + grow * 2.0f,
            rect.h + grow * 2.0f,
        };
        const eui::Color layer_color = eui::rgba(
            base.r,
            base.g,
            base.b,
            std::clamp(base.a * opacity * (0.54f / layers) * (1.08f - 0.38f * t), 0.0f, 1.0f));
        ui.paint_filled_rect(layer, layer_color, std::max(0.0f, radius + grow * 0.56f));
    }
}

inline void paint_fill_brush(eui::Context& ui, const eui::Rect& rect, float radius,
                             const eui::graphics::Brush& brush, float opacity) {
    eui::graphics::Brush scaled = brush;
    switch (scaled.kind) {
        case eui::graphics::BrushKind::solid:
            scaled.solid.a = std::clamp(scaled.solid.a * opacity, 0.0f, 1.0f);
            break;
        case eui::graphics::BrushKind::linear_gradient:
            for (std::size_t i = 0; i < scaled.linear.stop_count && i < scaled.linear.stops.size(); ++i) {
                scaled.linear.stops[i].color.a = std::clamp(scaled.linear.stops[i].color.a * opacity, 0.0f, 1.0f);
            }
            break;
        case eui::graphics::BrushKind::radial_gradient:
            for (std::size_t i = 0; i < scaled.radial.stop_count && i < scaled.radial.stops.size(); ++i) {
                scaled.radial.stops[i].color.a = std::clamp(scaled.radial.stops[i].color.a * opacity, 0.0f, 1.0f);
            }
            break;
        case eui::graphics::BrushKind::none:
        default:
            break;
    }
    if (scaled.kind == eui::graphics::BrushKind::none) {
        return;
    }
    ui.paint_filled_rect(rect, scaled, radius);
}

inline void paint_rectangle(eui::Context& ui, const eui::graphics::RectanglePrimitive& primitive) {
    const eui::Rect base_rect = eui::to_legacy_rect(primitive.rect);
    const eui::graphics::Transform3D combined =
        combine_rect_transforms(primitive.transform_2d, primitive.transform_3d);
    const eui::Rect rect = eui::projected_rect_bounds(base_rect, combined);
    const float radius = average_corner_radius(primitive.radius);
    const float opacity = std::clamp(primitive.opacity, 0.0f, 1.0f);
    const bool has_transform_3d = !eui::transform_3d_is_identity(combined);
    const float blur_radius = std::max(primitive.blur.radius, primitive.blur.backdrop_radius);

    const bool has_clip = primitive.clip.mode == eui::graphics::ClipMode::bounds;
    if (has_clip) {
        ui.push_clip(eui::to_legacy_rect(primitive.clip.rect));
    }

    paint_shadow_approx(ui, rect, radius, primitive.shadow, opacity);
    if (blur_radius > 0.0f) {
        if (has_transform_3d) {
            ui.paint_backdrop_blur(base_rect, radius, blur_radius, opacity, &combined);
        } else {
            ui.paint_backdrop_blur(base_rect, radius, blur_radius, opacity);
        }
    }
    if (has_transform_3d) {
        eui::graphics::Brush scaled_fill = primitive.fill;
        switch (scaled_fill.kind) {
            case eui::graphics::BrushKind::solid:
                scaled_fill.solid.a = std::clamp(scaled_fill.solid.a * opacity, 0.0f, 1.0f);
                break;
            case eui::graphics::BrushKind::linear_gradient:
                for (std::size_t i = 0; i < scaled_fill.linear.stop_count && i < scaled_fill.linear.stops.size(); ++i) {
                    scaled_fill.linear.stops[i].color.a =
                        std::clamp(scaled_fill.linear.stops[i].color.a * opacity, 0.0f, 1.0f);
                }
                break;
            case eui::graphics::BrushKind::radial_gradient:
                for (std::size_t i = 0; i < scaled_fill.radial.stop_count && i < scaled_fill.radial.stops.size(); ++i) {
                    scaled_fill.radial.stops[i].color.a =
                        std::clamp(scaled_fill.radial.stops[i].color.a * opacity, 0.0f, 1.0f);
                }
                break;
            case eui::graphics::BrushKind::none:
            default:
                break;
        }
        if (scaled_fill.kind != eui::graphics::BrushKind::none) {
            ui.paint_filled_rect(base_rect, scaled_fill, radius, &combined);
        }
    } else {
        paint_fill_brush(ui, base_rect, radius, primitive.fill, opacity);
    }

    if (primitive.stroke.width > 0.0f) {
        eui::graphics::Color stroke_color{};
        if (brush_primary_color(primitive.stroke.brush, stroke_color)) {
            const eui::Color legacy = eui::rgba(
                stroke_color.r,
                stroke_color.g,
                stroke_color.b,
                std::clamp(stroke_color.a * opacity, 0.0f, 1.0f));
            if (has_transform_3d) {
                ui.paint_outline_rect(base_rect, legacy, radius, primitive.stroke.width, &combined);
            } else {
                ui.paint_outline_rect(base_rect, legacy, radius, primitive.stroke.width);
            }
        }
    }

    if (has_clip) {
        ui.pop_clip();
    }
}

inline std::string utf8_from_codepoint(std::uint32_t codepoint) {
    std::string out;
    if (codepoint <= 0x7Fu) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | ((codepoint >> 6u) & 0x1Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | ((codepoint >> 12u) & 0x0Fu)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | ((codepoint >> 18u) & 0x07u)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    }
    return out;
}

inline void paint_icon(eui::Context& ui, const eui::graphics::IconPrimitive& primitive) {
    const eui::Rect rect = resolve_rect(primitive);
    eui::graphics::Color tint{};
    if (!brush_primary_color(primitive.tint, tint)) {
        tint = eui::graphics::Color{1.0f, 1.0f, 1.0f, 1.0f};
    }
    const eui::Color color = eui::rgba(tint.r, tint.g, tint.b, std::clamp(tint.a * primitive.opacity, 0.0f, 1.0f));
    const std::string glyph = utf8_from_codepoint(primitive.glyph);

    const bool has_clip = primitive.clip.mode == eui::graphics::ClipMode::bounds;
    const eui::Rect clip_rect = eui::to_legacy_rect(primitive.clip.rect);
    if (has_clip) {
        ui.push_clip(clip_rect);
    }
    ui.paint_text(glyph, rect, color, std::max(8.0f, rect.h * 0.72f), eui::TextAlign::Center, has_clip ? &clip_rect : nullptr);
    if (has_clip) {
        ui.pop_clip();
    }
}

inline std::string image_placeholder_label(std::string_view source) {
    if (source.empty()) {
        return "image";
    }
    const std::size_t slash = source.find_last_of("/\\");
    if (slash == std::string_view::npos) {
        return std::string(source);
    }
    return std::string(source.substr(slash + 1u));
}

inline std::uint64_t image_placeholder_hash(std::string_view text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (char ch : text) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

inline bool image_placeholder_parse_dimension(std::string_view source, std::size_t& index, int& out_value) {
    if (index >= source.size() || source[index] < '0' || source[index] > '9') {
        return false;
    }
    int value = 0;
    while (index < source.size() && source[index] >= '0' && source[index] <= '9') {
        value = value * 10 + static_cast<int>(source[index] - '0');
        if (value > 16384) {
            return false;
        }
        ++index;
    }
    out_value = value;
    return value > 0;
}

inline eui::graphics::Size image_placeholder_intrinsic_size(std::string_view source) {
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] < '0' || source[i] > '9') {
            continue;
        }
        std::size_t cursor = i;
        int width = 0;
        if (!image_placeholder_parse_dimension(source, cursor, width)) {
            continue;
        }
        if (cursor >= source.size() || (source[cursor] != 'x' && source[cursor] != 'X')) {
            continue;
        }
        ++cursor;
        int height = 0;
        if (!image_placeholder_parse_dimension(source, cursor, height)) {
            continue;
        }
        if (width >= 8 && height >= 8) {
            return eui::graphics::Size{static_cast<float>(width), static_cast<float>(height)};
        }
    }

    return eui::graphics::Size{320.0f, 200.0f};
}

inline const char* image_fit_name(eui::graphics::ImageFit fit) {
    switch (fit) {
        case eui::graphics::ImageFit::fill:
            return "fill";
        case eui::graphics::ImageFit::contain:
            return "contain";
        case eui::graphics::ImageFit::cover:
            return "cover";
        case eui::graphics::ImageFit::stretch:
            return "stretch";
        case eui::graphics::ImageFit::center:
            return "center";
        default:
            return "image";
    }
}

inline eui::Rect resolve_image_fit_rect(const eui::Rect& frame, const eui::graphics::Size& intrinsic,
                                        eui::graphics::ImageFit fit) {
    const float src_w = std::max(1.0f, intrinsic.w);
    const float src_h = std::max(1.0f, intrinsic.h);
    switch (fit) {
        case eui::graphics::ImageFit::fill:
        case eui::graphics::ImageFit::stretch:
            return frame;
        case eui::graphics::ImageFit::center: {
            const float w = src_w;
            const float h = src_h;
            return eui::Rect{
                frame.x + (frame.w - w) * 0.5f,
                frame.y + (frame.h - h) * 0.5f,
                w,
                h,
            };
        }
        case eui::graphics::ImageFit::contain:
        case eui::graphics::ImageFit::cover:
        default: {
            const float scale_x = frame.w / src_w;
            const float scale_y = frame.h / src_h;
            const float scale =
                (fit == eui::graphics::ImageFit::contain) ? std::min(scale_x, scale_y) : std::max(scale_x, scale_y);
            const float w = src_w * scale;
            const float h = src_h * scale;
            return eui::Rect{
                frame.x + (frame.w - w) * 0.5f,
                frame.y + (frame.h - h) * 0.5f,
                w,
                h,
            };
        }
    }
}

inline void paint_image_placeholder_tiles(eui::Context& ui, const eui::Rect& rect, std::uint64_t seed, float opacity) {
    if (rect.w <= 1.0f || rect.h <= 1.0f) {
        return;
    }

    const float tile = std::clamp(std::min(rect.w, rect.h) / 5.5f, 12.0f, 32.0f);
    const int cols = std::max(1, static_cast<int>(std::ceil(rect.w / tile)));
    const int rows = std::max(1, static_cast<int>(std::ceil(rect.h / tile)));
    const float hue_a = static_cast<float>((seed >> 8u) & 0xffu) / 255.0f;
    const float hue_b = static_cast<float>((seed >> 24u) & 0xffu) / 255.0f;
    const eui::Color color_a = eui::rgba(0.18f + hue_a * 0.28f, 0.28f + hue_b * 0.32f, 0.42f + hue_a * 0.20f,
                                         std::clamp(opacity * 0.92f, 0.0f, 1.0f));
    const eui::Color color_b = eui::rgba(0.10f + hue_b * 0.22f, 0.16f + hue_a * 0.24f, 0.24f + hue_b * 0.26f,
                                         std::clamp(opacity * 0.92f, 0.0f, 1.0f));

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const float x = rect.x + static_cast<float>(col) * tile;
            const float y = rect.y + static_cast<float>(row) * tile;
            const float w = std::min(tile, rect.x + rect.w - x);
            const float h = std::min(tile, rect.y + rect.h - y);
            if (w <= 0.0f || h <= 0.0f) {
                continue;
            }
            ui.paint_filled_rect(
                eui::Rect{x, y, w, h},
                ((row + col) & 1) == 0 ? color_a : color_b,
                0.0f);
        }
    }
}

inline void paint_image_placeholder_guides(eui::Context& ui, const eui::Rect& rect, float opacity) {
    const eui::Color guide = eui::rgba(0.94f, 0.97f, 1.0f, std::clamp(opacity * 0.18f, 0.0f, 1.0f));
    ui.paint_outline_rect(rect, eui::rgba(1.0f, 1.0f, 1.0f, std::clamp(opacity * 0.28f, 0.0f, 1.0f)), 0.0f, 1.0f);
    ui.paint_filled_rect(eui::Rect{rect.x + rect.w * 0.5f - 0.5f, rect.y, 1.0f, rect.h}, guide, 0.0f);
    ui.paint_filled_rect(eui::Rect{rect.x, rect.y + rect.h * 0.5f - 0.5f, rect.w, 1.0f}, guide, 0.0f);
}

inline bool image_visible_rect(const eui::Rect& lhs, const eui::Rect& rhs, eui::Rect& out) {
    const float x0 = std::max(lhs.x, rhs.x);
    const float y0 = std::max(lhs.y, rhs.y);
    const float x1 = std::min(lhs.x + lhs.w, rhs.x + rhs.w);
    const float y1 = std::min(lhs.y + lhs.h, rhs.y + rhs.h);
    if (x1 <= x0 || y1 <= y0) {
        out = eui::Rect{};
        return false;
    }
    out = eui::Rect{x0, y0, x1 - x0, y1 - y0};
    return true;
}

inline void paint_image(eui::Context& ui, const eui::graphics::ImagePrimitive& primitive) {
    const eui::Rect frame_rect = resolve_rect(primitive);
    const eui::graphics::Size intrinsic = image_placeholder_intrinsic_size(primitive.source);
    const eui::Rect content_rect = resolve_image_fit_rect(frame_rect, intrinsic, primitive.fit);
    const float radius = average_corner_radius(primitive.radius);
    const float opacity = std::clamp(primitive.opacity, 0.0f, 1.0f);
    const std::uint64_t seed = image_placeholder_hash(primitive.source.empty() ? std::string_view("image") : primitive.source);
    const eui::Color fill = eui::rgba(0.08f, 0.11f, 0.15f, opacity);
    const eui::Color outline = eui::rgba(0.24f, 0.31f, 0.40f, opacity);
    const eui::Color text = eui::rgba(0.82f, 0.88f, 0.94f, opacity);
    const eui::Color label_bg = eui::rgba(0.03f, 0.05f, 0.08f, std::clamp(opacity * 0.72f, 0.0f, 1.0f));

    const bool has_clip = primitive.clip.mode == eui::graphics::ClipMode::bounds;
    const eui::Rect clip_rect = eui::to_legacy_rect(primitive.clip.rect);
    if (has_clip) {
        ui.push_clip(clip_rect);
    }

    ui.paint_filled_rect(frame_rect, fill, radius);
    ui.push_clip(frame_rect);
    eui::Rect visible_content{};
    if (image_visible_rect(content_rect, frame_rect, visible_content)) {
        paint_image_placeholder_tiles(ui, visible_content, seed, opacity);
    }
    paint_image_placeholder_guides(ui, content_rect, opacity);
    ui.pop_clip();

    ui.paint_outline_rect(frame_rect, outline, radius, 1.0f);

    const std::string label = image_placeholder_label(primitive.source);
    const std::string mode_label = std::string(image_fit_name(primitive.fit)) + "  " +
                                   std::to_string(static_cast<int>(intrinsic.w)) + "x" +
                                   std::to_string(static_cast<int>(intrinsic.h));
    const float label_h = std::clamp(frame_rect.h * 0.20f, 18.0f, 28.0f);
    const eui::Rect footer{
        frame_rect.x,
        frame_rect.y + frame_rect.h - label_h,
        frame_rect.w,
        label_h,
    };
    ui.paint_filled_rect(footer, label_bg, 0.0f);
    ui.paint_text(label, eui::Rect{footer.x + 8.0f, footer.y, std::max(0.0f, footer.w - 16.0f), footer.h}, text,
                  std::max(10.0f, footer.h * 0.52f), eui::TextAlign::Left, has_clip ? &clip_rect : nullptr);
    ui.paint_text(mode_label,
                  eui::Rect{footer.x + 8.0f, footer.y, std::max(0.0f, footer.w - 16.0f), footer.h},
                  eui::rgba(text.r, text.g, text.b, std::clamp(opacity * 0.74f, 0.0f, 1.0f)),
                  std::max(9.0f, footer.h * 0.46f), eui::TextAlign::Right, has_clip ? &clip_rect : nullptr);

    if (has_clip) {
        ui.pop_clip();
    }
}

}  // namespace eui::quick::detail
