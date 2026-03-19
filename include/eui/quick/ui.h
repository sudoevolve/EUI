#pragma once

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "eui/quick/detail/anchor_spec.h"
#include "eui/quick/detail/primitive_painter.h"

namespace eui::quick {

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

namespace detail {

struct ContextAccess {
    static const Theme& theme(const Context& ctx) {
        return ctx.theme();
    }

    static Rect content_rect(const Context& ctx) {
        return ctx.content_rect();
    }

    static Rect last_item_rect(const Context& ctx) {
        return ctx.last_item_rect();
    }

    static float measure_text(const Context& ctx, std::string_view text, float font_size) {
        return ctx.measure_text(text, font_size);
    }

    static void push_layout_rect(Context& ctx, const Rect& rect) {
        ctx.internal_push_layout_rect(rect);
    }

    static void pop_layout_rect(Context& ctx) {
        ctx.internal_pop_layout_rect();
    }

    static void begin_stack(Context& ctx, const Rect& rect) {
        ctx.internal_begin_stack(rect);
    }

    static void push_clip(Context& ctx, const Rect& rect) {
        ctx.push_clip(rect);
    }

    static void pop_clip(Context& ctx) {
        ctx.pop_clip();
    }

    static Rect dock_left(Context& ctx, float width, float gap = 0.0f) {
        return ctx.internal_dock_left(width, gap);
    }

    static Rect dock_right(Context& ctx, float width, float gap = 0.0f) {
        return ctx.internal_dock_right(width, gap);
    }

    static Rect dock_top(Context& ctx, float height, float gap = 0.0f) {
        return ctx.internal_dock_top(height, gap);
    }

    static Rect dock_bottom(Context& ctx, float height, float gap = 0.0f) {
        return ctx.internal_dock_bottom(height, gap);
    }

    static SplitRects split_h(const Context& ctx, float first_width, float gap = 0.0f) {
        return ctx.internal_split_h(first_width, gap);
    }

    static SplitRects split_h(const Context& ctx, const Rect& rect, float first_width, float gap = 0.0f) {
        return ctx.internal_split_h(rect, first_width, gap);
    }

    static SplitRects split_v(const Context& ctx, float first_height, float gap = 0.0f) {
        return ctx.internal_split_v(first_height, gap);
    }

    static SplitRects split_v(const Context& ctx, const Rect& rect, float first_height, float gap = 0.0f) {
        return ctx.internal_split_v(rect, first_height, gap);
    }

    static void begin_flex_row(Context& ctx, const std::vector<FlexLength>& items, float gap, FlexAlign align) {
        ctx.internal_begin_flex_row(items, gap, align);
    }

    static void end_flex_row(Context& ctx) {
        ctx.internal_end_flex_row();
    }

    static void label(Context& ctx, std::string_view text, float font_size, bool muted, float height) {
        ctx.internal_label(text, font_size, muted, height);
    }

    static void spacer(Context& ctx, float height) {
        ctx.internal_spacer(height);
    }

    static bool button(Context& ctx, std::string_view label, ButtonStyle style, float height, float text_scale) {
        return ctx.internal_button(label, style, height, text_scale);
    }

    static bool tab(Context& ctx, std::string_view label, bool selected, float height) {
        return ctx.internal_tab(label, selected, height);
    }

    static bool slider_float(Context& ctx, std::string_view label, float& value, float min_value, float max_value,
                             int decimals, float height) {
        return ctx.internal_slider_float(label, value, min_value, max_value, decimals, height);
    }

    static bool input_float(Context& ctx, std::string_view label, float& value, float min_value, float max_value,
                            int decimals, float height) {
        return ctx.internal_input_float(label, value, min_value, max_value, decimals, height);
    }

    static bool input_text(Context& ctx, std::string_view label, std::string& value, float height,
                           std::string_view placeholder, bool align_right) {
        return ctx.internal_input_text(label, value, height, placeholder, align_right);
    }

    static void input_readonly(Context& ctx, std::string_view label, std::string_view value, float height,
                               bool align_right, float value_scale, bool muted) {
        ctx.internal_input_readonly(label, value, height, align_right, value_scale, muted);
    }

    static bool begin_scroll_area(Context& ctx, std::string_view label, float height,
                                  const ScrollAreaOptions& options) {
        Context::InternalScrollAreaOptions legacy{};
        legacy.padding = options.padding;
        legacy.scrollbar_width = options.scrollbar_width;
        legacy.min_thumb_height = options.min_thumb_height;
        legacy.wheel_step = options.wheel_step;
        legacy.drag_sensitivity = options.drag_sensitivity;
        legacy.inertia_friction = options.inertia_friction;
        legacy.bounce_strength = options.bounce_strength;
        legacy.bounce_damping = options.bounce_damping;
        legacy.overscroll_limit = options.overscroll_limit;
        legacy.enable_drag = options.enable_drag;
        legacy.show_scrollbar = options.show_scrollbar;
        legacy.draw_background = options.draw_background;
        return ctx.internal_begin_scroll_area(label, height, legacy);
    }

    static void end_scroll_area(Context& ctx) {
        ctx.internal_end_scroll_area();
    }

    static bool text_area(Context& ctx, std::string_view label, std::string& text, float height) {
        return ctx.internal_text_area(label, text, height);
    }

    static void text_area_readonly(Context& ctx, std::string_view label, std::string_view text, float height) {
        ctx.internal_text_area_readonly(label, text, height);
    }

    static void progress(Context& ctx, std::string_view label, float ratio, float height) {
        ctx.internal_progress(label, ratio, height);
    }

    static bool begin_dropdown(Context& ctx, std::string_view label, bool& open, float body_height, float padding) {
        return ctx.internal_begin_dropdown(label, open, body_height, padding);
    }

    static void end_dropdown(Context& ctx) {
        ctx.internal_end_dropdown();
    }
};

}  // namespace detail

class UI;
class RegionScope;
class ClipScope;
class FlexRowScope;
class DropdownScope;
class ScrollAreaScope;
class AnchorBuilder;
class RectangleBuilder;
class TextBuilder;
class ShapeBuilder;
class GlyphBuilder;
class ImageBuilder;
class SurfaceBuilder;
class LabelBuilder;
class ButtonBuilder;
class TabBuilder;
class SliderFloatBuilder;
class InputBuilder;
class FloatInputBuilder;
class ReadonlyBuilder;
class ProgressBuilder;
class TextAreaBuilder;
class TextAreaReadonlyBuilder;
class DropdownBuilder;
class ScrollAreaBuilder;
class MetricBuilder;
class RowBuilder;

inline Color rgb(std::uint32_t hex, float alpha = 1.0f) {
    const float inv = 1.0f / 255.0f;
    return rgba(
        static_cast<float>((hex >> 16u) & 0xffu) * inv,
        static_cast<float>((hex >> 8u) & 0xffu) * inv,
        static_cast<float>(hex & 0xffu) * inv,
        std::clamp(alpha, 0.0f, 1.0f));
}

inline Color with_alpha(const Color& color, float alpha) {
    return rgba(color.r, color.g, color.b, std::clamp(color.a * alpha, 0.0f, 1.0f));
}

inline Rect make_rect(float x, float y, float w, float h) {
    return Rect{x, y, std::max(0.0f, w), std::max(0.0f, h)};
}

inline Rect inset(const Rect& rect, float padding_x, float padding_y) {
    const float px = std::max(0.0f, padding_x);
    const float py = std::max(0.0f, padding_y);
    return make_rect(
        rect.x + px,
        rect.y + py,
        std::max(0.0f, rect.w - px * 2.0f),
        std::max(0.0f, rect.h - py * 2.0f));
}

inline Rect inset(const Rect& rect, float padding) {
    return inset(rect, padding, padding);
}

inline Rect translate(const Rect& rect, float dx, float dy) {
    return Rect{rect.x + dx, rect.y + dy, rect.w, rect.h};
}

inline bool has_area(const Rect& rect) {
    return rect.w > 0.0f && rect.h > 0.0f;
}

inline float ease_out_cubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

inline detail::ResolvedRect to_resolved_rect(const Rect& rect) {
    return detail::ResolvedRect{rect.x, rect.y, rect.w, rect.h};
}

inline Rect from_resolved_rect(const detail::ResolvedRect& rect) {
    return Rect{rect.x, rect.y, rect.w, rect.h};
}

namespace gfx {

inline eui::graphics::Color color(std::uint32_t hex, float alpha = 1.0f) {
    const float inv = 1.0f / 255.0f;
    return eui::graphics::Color{
        static_cast<float>((hex >> 16u) & 0xffu) * inv,
        static_cast<float>((hex >> 8u) & 0xffu) * inv,
        static_cast<float>(hex & 0xffu) * inv,
        std::clamp(alpha, 0.0f, 1.0f),
    };
}

inline eui::graphics::Color color(const Color& legacy, float alpha = 1.0f) {
    return eui::graphics::Color{
        legacy.r,
        legacy.g,
        legacy.b,
        std::clamp(legacy.a * alpha, 0.0f, 1.0f),
    };
}

inline eui::graphics::Brush solid(const eui::graphics::Color& color) {
    eui::graphics::Brush brush{};
    brush.kind = eui::graphics::BrushKind::solid;
    brush.solid = color;
    return brush;
}

inline eui::graphics::Brush solid(const Color& color, float alpha = 1.0f) {
    return solid(gfx::color(color, alpha));
}

inline eui::graphics::Brush solid(std::uint32_t hex, float alpha = 1.0f) {
    return solid(gfx::color(hex, alpha));
}

inline eui::graphics::Brush vertical_gradient(const eui::graphics::Color& top, const eui::graphics::Color& bottom) {
    eui::graphics::Brush brush{};
    brush.kind = eui::graphics::BrushKind::linear_gradient;
    brush.linear.start = eui::graphics::Point{0.0f, 0.0f};
    brush.linear.end = eui::graphics::Point{0.0f, 1.0f};
    brush.linear.stops[0] = eui::graphics::ColorStop{0.0f, top};
    brush.linear.stops[1] = eui::graphics::ColorStop{1.0f, bottom};
    brush.linear.stop_count = 2u;
    return brush;
}

inline eui::graphics::Brush vertical_gradient(const Color& top, const Color& bottom, float alpha = 1.0f) {
    return vertical_gradient(gfx::color(top, alpha), gfx::color(bottom, alpha));
}

inline eui::graphics::Brush vertical_gradient(std::uint32_t top, std::uint32_t bottom, float alpha = 1.0f) {
    return vertical_gradient(gfx::color(top, alpha), gfx::color(bottom, alpha));
}

inline eui::graphics::Brush radial_gradient(const eui::graphics::Color& inner, const eui::graphics::Color& outer,
                                            float radius = 0.75f) {
    eui::graphics::Brush brush{};
    brush.kind = eui::graphics::BrushKind::radial_gradient;
    brush.radial.center = eui::graphics::Point{0.5f, 0.5f};
    brush.radial.radius = std::max(0.0f, radius);
    brush.radial.stops[0] = eui::graphics::ColorStop{0.0f, inner};
    brush.radial.stops[1] = eui::graphics::ColorStop{1.0f, outer};
    brush.radial.stop_count = 2u;
    return brush;
}

inline eui::graphics::Brush radial_gradient(const Color& inner, const Color& outer, float radius = 0.75f,
                                            float alpha = 1.0f) {
    return radial_gradient(gfx::color(inner, alpha), gfx::color(outer, alpha), radius);
}

inline eui::graphics::Brush radial_gradient(std::uint32_t inner, std::uint32_t outer, float radius = 0.75f,
                                            float alpha = 1.0f) {
    return radial_gradient(gfx::color(inner, alpha), gfx::color(outer, alpha), radius);
}

inline eui::graphics::Stroke stroke(const eui::graphics::Brush& brush, float width = 1.0f) {
    eui::graphics::Stroke outline{};
    outline.width = std::max(0.0f, width);
    outline.brush = brush;
    return outline;
}

inline eui::graphics::Stroke stroke(const Color& color, float width = 1.0f, float alpha = 1.0f) {
    return stroke(solid(color, alpha), width);
}

inline eui::graphics::Stroke stroke(std::uint32_t hex, float width = 1.0f, float alpha = 1.0f) {
    return stroke(solid(hex, alpha), width);
}

inline eui::graphics::CornerRadius radius(float uniform) {
    const float safe = std::max(0.0f, uniform);
    return eui::graphics::CornerRadius{safe, safe, safe, safe};
}

inline eui::graphics::CornerRadius radius(float top_left, float top_right, float bottom_right, float bottom_left) {
    return eui::graphics::CornerRadius{
        std::max(0.0f, top_left),
        std::max(0.0f, top_right),
        std::max(0.0f, bottom_right),
        std::max(0.0f, bottom_left),
    };
}

inline eui::graphics::ClipRect clip(const Rect& rect) {
    return eui::graphics::ClipRect{eui::to_graphics_rect(rect), eui::graphics::ClipMode::bounds};
}

}  // namespace gfx

struct Placement {
    bool has_rect{false};
    Rect rect{};

    bool has_pos{false};
    float x{0.0f};
    float y{0.0f};

    bool has_size{false};
    float w{0.0f};
    float h{0.0f};

    bool use_parent{false};

    bool use_after_last{false};
    bool use_below_last{false};
    float offset_x{0.0f};
    float offset_y{0.0f};
};

struct ShadowStyle {
    bool enabled{false};
    float offset_x{0.0f};
    float offset_y{0.0f};
    float blur{0.0f};
    float spread{0.0f};
    Color color{};
};

struct StrokeStyle {
    bool enabled{false};
    float width{1.0f};
    Color color{};
};

struct GradientStyle {
    bool enabled{false};
    Color top{};
    Color bottom{};
};

class UI {
public:
    explicit UI(Context& ctx)
        : ctx_(ctx), last_rect_(detail::ContextAccess::last_item_rect(ctx)) {}

    Context& ctx() {
        return ctx_;
    }

    const Context& ctx() const {
        return ctx_;
    }

    const Theme& theme() const {
        return detail::ContextAccess::theme(ctx_);
    }

    Rect content() const {
        return detail::ContextAccess::content_rect(ctx_);
    }

    Rect content_rect() const {
        return detail::ContextAccess::content_rect(ctx_);
    }

    Rect last() const {
        return last_rect_;
    }

    float measure_text(std::string_view text, float font_size) const {
        return detail::ContextAccess::measure_text(ctx_, text, font_size);
    }

    void remember(const Rect& rect) {
        last_rect_ = rect;
    }

    FlexLength fixed(float px) const {
        return eui::Fixed(px);
    }

    FlexLength flex(float weight = 1.0f) const {
        return eui::Flex(weight);
    }

    FlexLength content(float min_width, float max_width) const {
        return eui::Content(min_width, max_width);
    }

    SplitRects split_h(const Rect& rect, float first_width, float gap = 0.0f) const {
        return detail::ContextAccess::split_h(ctx_, rect, first_width, gap);
    }

    SplitRects split_h(float first_width, float gap = 0.0f) const {
        return detail::ContextAccess::split_h(ctx_, first_width, gap);
    }

    SplitRects split_v(const Rect& rect, float first_height, float gap = 0.0f) const {
        return detail::ContextAccess::split_v(ctx_, rect, first_height, gap);
    }

    SplitRects split_v(float first_height, float gap = 0.0f) const {
        return detail::ContextAccess::split_v(ctx_, first_height, gap);
    }

    SplitRects split_h_ratio(const Rect& rect, float first_ratio, float gap = 0.0f) const {
        const float safe_gap = std::max(0.0f, gap);
        const float usable = std::max(0.0f, rect.w - safe_gap);
        return detail::ContextAccess::split_h(ctx_, rect, usable * std::clamp(first_ratio, 0.0f, 1.0f), gap);
    }

    SplitRects split_v_ratio(const Rect& rect, float first_ratio, float gap = 0.0f) const {
        const float safe_gap = std::max(0.0f, gap);
        const float usable = std::max(0.0f, rect.h - safe_gap);
        return detail::ContextAccess::split_v(ctx_, rect, usable * std::clamp(first_ratio, 0.0f, 1.0f), gap);
    }

    RectangleBuilder rectangle();
    RectangleBuilder rect();
    TextBuilder text(std::string_view text);
    TextBuilder icon(std::string_view text);
    ShapeBuilder shape();
    GlyphBuilder glyph(std::uint32_t codepoint);
    ImageBuilder image(std::string_view source);
    SurfaceBuilder panel(std::string_view title = {});
    SurfaceBuilder card(std::string_view title = {});
    LabelBuilder label(std::string_view text);
    ButtonBuilder button(std::string_view label);
    TabBuilder tab(std::string_view label, bool selected = false);
    SliderFloatBuilder slider(std::string_view label, float& value);
    InputBuilder input(std::string_view label, std::string& value);
    InputBuilder input(std::string& value);
    FloatInputBuilder input_float(std::string_view label, float& value);
    ReadonlyBuilder readonly(std::string_view label, std::string_view value);
    ProgressBuilder progress(std::string_view label, float ratio);
    TextAreaBuilder text_area(std::string_view label, std::string& text);
    TextAreaReadonlyBuilder text_area_readonly(std::string_view label, std::string_view text);
    DropdownBuilder dropdown(std::string_view label, bool& open);
    ScrollAreaBuilder scroll_area(std::string_view label);
    MetricBuilder metric(std::string_view label, std::string_view value);
    RowBuilder row();
    AnchorBuilder anchor();

    Rect paint(const eui::graphics::RectanglePrimitive& primitive) {
        const Rect rect = detail::resolve_rect(primitive);
        detail::paint_rectangle(ctx_, primitive);
        remember(rect);
        return rect;
    }

    Rect paint(const eui::graphics::IconPrimitive& primitive) {
        const Rect rect = detail::resolve_rect(primitive);
        detail::paint_icon(ctx_, primitive);
        remember(rect);
        return rect;
    }

    Rect paint(const eui::graphics::ImagePrimitive& primitive) {
        const Rect rect = detail::resolve_rect(primitive);
        detail::paint_image(ctx_, primitive);
        remember(rect);
        return rect;
    }

    Rect resolve_anchor(const detail::AnchorRect& anchor) const {
        return resolve_anchor(anchor, content_rect());
    }

    Rect resolve_anchor(const detail::AnchorRect& anchor, const Rect& reference) const {
        return from_resolved_rect(detail::resolve_anchor_rect(anchor, to_resolved_rect(reference)));
    }

    float ease(eui::animation::EasingPreset preset, float progress) const {
        return eui::animation::ease(preset, progress);
    }

    float ease(const eui::animation::CubicBezier& bezier, float progress) const {
        return eui::animation::sample_bezier_y(bezier, progress);
    }

    float animate(const eui::animation::TimelineClip& clip, float elapsed_seconds) const {
        return eui::animation::animate_scalar(clip, elapsed_seconds);
    }

    std::uint64_t id(std::string_view label) const {
        return ctx_.motion_key(label);
    }

    std::uint64_t id(std::string_view label, std::uint64_t salt) const {
        return ctx_.motion_key(label, salt);
    }

    std::uint64_t id(std::uint64_t base, std::uint64_t salt) const {
        return ctx_.motion_key(base, salt);
    }

    float motion(std::uint64_t id, float target, float speed = 10.0f) {
        return ctx_.motion(id, target, speed);
    }

    float motion(std::uint64_t id, float target, float rise_speed, float fall_speed) {
        return ctx_.motion(id, target, rise_speed, fall_speed);
    }

    float motion(std::string_view label, float target, float speed = 10.0f) {
        return ctx_.motion(id(label), target, speed);
    }

    float motion(std::string_view label, float target, float rise_speed, float fall_speed) {
        return ctx_.motion(id(label), target, rise_speed, fall_speed);
    }

    float presence(std::uint64_t id, bool visible, float enter_speed = 12.0f, float exit_speed = 16.0f) {
        return ctx_.presence(id, visible, enter_speed, exit_speed);
    }

    float presence(std::string_view label, bool visible, float enter_speed = 12.0f, float exit_speed = 16.0f) {
        return ctx_.presence(id(label), visible, enter_speed, exit_speed);
    }

    void reset_motion(std::uint64_t id, float value = 0.0f) {
        ctx_.reset_motion(id, value);
    }

    void reset_motion(std::string_view label, float value = 0.0f) {
        ctx_.reset_motion(id(label), value);
    }

    float motion_value(std::uint64_t id, float fallback = 0.0f) const {
        return ctx_.motion_value(id, fallback);
    }

    float motion_value(std::string_view label, float fallback = 0.0f) const {
        return ctx_.motion_value(id(label), fallback);
    }

    RegionScope scope(const Rect& rect);
    template <typename Fn>
    void scope(const Rect& rect, Fn&& fn);

    RegionScope stack(const Rect& rect);
    template <typename Fn>
    void stack(const Rect& rect, Fn&& fn);

    ClipScope clip(const Rect& rect);
    template <typename Fn>
    void clip(const Rect& rect, Fn&& fn);

    void spacer(float height = 8.0f) {
        detail::ContextAccess::spacer(ctx_, height);
    }

private:
    Context& ctx_;
    Rect last_rect_{};
};

class RegionScope {
public:
    RegionScope() = default;

    RegionScope(UI* ui, const Rect& shell_rect)
        : ui_(ui), shell_rect_(shell_rect), active_(ui != nullptr) {}

    RegionScope(const RegionScope&) = delete;
    RegionScope& operator=(const RegionScope&) = delete;

    RegionScope(RegionScope&& other) noexcept
        : ui_(other.ui_), shell_rect_(other.shell_rect_), active_(other.active_) {
        other.ui_ = nullptr;
        other.active_ = false;
    }

    RegionScope& operator=(RegionScope&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        end();
        ui_ = other.ui_;
        shell_rect_ = other.shell_rect_;
        active_ = other.active_;
        other.ui_ = nullptr;
        other.active_ = false;
        return *this;
    }

    ~RegionScope() {
        end();
    }

    void end() {
        if (!active_ || ui_ == nullptr) {
            return;
        }
        detail::ContextAccess::pop_layout_rect(ui_->ctx());
        ui_->remember(shell_rect_);
        active_ = false;
    }

    bool active() const {
        return active_;
    }

    Rect shell() const {
        return shell_rect_;
    }

    Rect content() const {
        return (ui_ != nullptr) ? ui_->ctx().content_rect() : Rect{};
    }

    Rect last() const {
        return (ui_ != nullptr) ? ui_->last() : Rect{};
    }

    Rect dock_left(float width, float gap = 0.0f) {
        if (ui_ == nullptr) {
            return Rect{};
        }
        const Rect rect = detail::ContextAccess::dock_left(ui_->ctx(), width, gap);
        ui_->remember(rect);
        return rect;
    }

    Rect dock_right(float width, float gap = 0.0f) {
        if (ui_ == nullptr) {
            return Rect{};
        }
        const Rect rect = detail::ContextAccess::dock_right(ui_->ctx(), width, gap);
        ui_->remember(rect);
        return rect;
    }

    Rect dock_top(float height, float gap = 0.0f) {
        if (ui_ == nullptr) {
            return Rect{};
        }
        const Rect rect = detail::ContextAccess::dock_top(ui_->ctx(), height, gap);
        ui_->remember(rect);
        return rect;
    }

    Rect dock_bottom(float height, float gap = 0.0f) {
        if (ui_ == nullptr) {
            return Rect{};
        }
        const Rect rect = detail::ContextAccess::dock_bottom(ui_->ctx(), height, gap);
        ui_->remember(rect);
        return rect;
    }

    SplitRects split_h(float first_width, float gap = 0.0f) const {
        return (ui_ != nullptr) ? detail::ContextAccess::split_h(ui_->ctx(), first_width, gap) : SplitRects{};
    }

    SplitRects split_v(float first_height, float gap = 0.0f) const {
        return (ui_ != nullptr) ? detail::ContextAccess::split_v(ui_->ctx(), first_height, gap) : SplitRects{};
    }

    SplitRects split_h(const Rect& rect, float first_width, float gap = 0.0f) const {
        return (ui_ != nullptr) ? detail::ContextAccess::split_h(ui_->ctx(), rect, first_width, gap) : SplitRects{};
    }

    SplitRects split_v(const Rect& rect, float first_height, float gap = 0.0f) const {
        return (ui_ != nullptr) ? detail::ContextAccess::split_v(ui_->ctx(), rect, first_height, gap) : SplitRects{};
    }

    SplitRects split_h_ratio(float first_ratio, float gap = 0.0f) const {
        return split_h_ratio(content(), first_ratio, gap);
    }

    SplitRects split_v_ratio(float first_ratio, float gap = 0.0f) const {
        return split_v_ratio(content(), first_ratio, gap);
    }

    SplitRects split_h_ratio(const Rect& rect, float first_ratio, float gap = 0.0f) const {
        return (ui_ != nullptr) ? ui_->split_h_ratio(rect, first_ratio, gap) : SplitRects{};
    }

    SplitRects split_v_ratio(const Rect& rect, float first_ratio, float gap = 0.0f) const {
        return (ui_ != nullptr) ? ui_->split_v_ratio(rect, first_ratio, gap) : SplitRects{};
    }

private:
    UI* ui_{nullptr};
    Rect shell_rect_{};
    bool active_{false};
};

class ClipScope {
public:
    ClipScope() = default;

    explicit ClipScope(Context* ctx)
        : ctx_(ctx), active_(ctx != nullptr) {}

    ClipScope(const ClipScope&) = delete;
    ClipScope& operator=(const ClipScope&) = delete;

    ClipScope(ClipScope&& other) noexcept
        : ctx_(other.ctx_), active_(other.active_) {
        other.ctx_ = nullptr;
        other.active_ = false;
    }

    ClipScope& operator=(ClipScope&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        end();
        ctx_ = other.ctx_;
        active_ = other.active_;
        other.ctx_ = nullptr;
        other.active_ = false;
        return *this;
    }

    ~ClipScope() {
        end();
    }

    void end() {
        if (!active_ || ctx_ == nullptr) {
            return;
        }
        detail::ContextAccess::pop_clip(*ctx_);
        active_ = false;
    }

private:
    Context* ctx_{nullptr};
    bool active_{false};
};

class FlexRowScope {
public:
    FlexRowScope() = default;

    explicit FlexRowScope(UI* ui)
        : ui_(ui), active_(ui != nullptr) {}

    FlexRowScope(const FlexRowScope&) = delete;
    FlexRowScope& operator=(const FlexRowScope&) = delete;

    FlexRowScope(FlexRowScope&& other) noexcept
        : ui_(other.ui_), active_(other.active_) {
        other.ui_ = nullptr;
        other.active_ = false;
    }

    FlexRowScope& operator=(FlexRowScope&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        end();
        ui_ = other.ui_;
        active_ = other.active_;
        other.ui_ = nullptr;
        other.active_ = false;
        return *this;
    }

    ~FlexRowScope() {
        end();
    }

    void end() {
        if (!active_ || ui_ == nullptr) {
            return;
        }
        detail::ContextAccess::end_flex_row(ui_->ctx());
        ui_->remember(detail::ContextAccess::last_item_rect(ui_->ctx()));
        active_ = false;
    }

private:
    UI* ui_{nullptr};
    bool active_{false};
};

class DropdownScope {
public:
    DropdownScope() = default;

    explicit DropdownScope(UI* ui)
        : ui_(ui), active_(ui != nullptr) {}

    DropdownScope(const DropdownScope&) = delete;
    DropdownScope& operator=(const DropdownScope&) = delete;

    DropdownScope(DropdownScope&& other) noexcept
        : ui_(other.ui_), active_(other.active_) {
        other.ui_ = nullptr;
        other.active_ = false;
    }

    DropdownScope& operator=(DropdownScope&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        end();
        ui_ = other.ui_;
        active_ = other.active_;
        other.ui_ = nullptr;
        other.active_ = false;
        return *this;
    }

    ~DropdownScope() {
        end();
    }

    void end() {
        if (!active_ || ui_ == nullptr) {
            return;
        }
        detail::ContextAccess::end_dropdown(ui_->ctx());
        ui_->remember(detail::ContextAccess::last_item_rect(ui_->ctx()));
        active_ = false;
    }

    bool active() const {
        return active_;
    }

    Rect content() const {
        return (ui_ != nullptr) ? ui_->content_rect() : Rect{};
    }

    Rect last() const {
        return (ui_ != nullptr) ? ui_->last() : Rect{};
    }

private:
    UI* ui_{nullptr};
    bool active_{false};
};

class ScrollAreaScope {
public:
    ScrollAreaScope() = default;

    explicit ScrollAreaScope(UI* ui)
        : ui_(ui), active_(ui != nullptr) {}

    ScrollAreaScope(const ScrollAreaScope&) = delete;
    ScrollAreaScope& operator=(const ScrollAreaScope&) = delete;

    ScrollAreaScope(ScrollAreaScope&& other) noexcept
        : ui_(other.ui_), active_(other.active_) {
        other.ui_ = nullptr;
        other.active_ = false;
    }

    ScrollAreaScope& operator=(ScrollAreaScope&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        end();
        ui_ = other.ui_;
        active_ = other.active_;
        other.ui_ = nullptr;
        other.active_ = false;
        return *this;
    }

    ~ScrollAreaScope() {
        end();
    }

    void end() {
        if (!active_ || ui_ == nullptr) {
            return;
        }
        detail::ContextAccess::end_scroll_area(ui_->ctx());
        ui_->remember(detail::ContextAccess::last_item_rect(ui_->ctx()));
        active_ = false;
    }

    bool active() const {
        return active_;
    }

    Rect content() const {
        return (ui_ != nullptr) ? ui_->content_rect() : Rect{};
    }

    Rect last() const {
        return (ui_ != nullptr) ? ui_->last() : Rect{};
    }

private:
    UI* ui_{nullptr};
    bool active_{false};
};

class AnchorBuilder {
public:
    explicit AnchorBuilder(UI& ui)
        : ui_(ui) {}

    AnchorBuilder& in(const Rect& rect) {
        reference_ = rect;
        has_reference_ = true;
        spec_.reference = detail::AnchorReference::parent;
        return *this;
    }

    AnchorBuilder& to_last() {
        reference_ = ui_.last();
        has_reference_ = true;
        spec_.reference = detail::AnchorReference::last_item;
        return *this;
    }

    AnchorBuilder& left(float px) {
        spec_.left = detail::AnchorValue::Px(px);
        return *this;
    }

    AnchorBuilder& right(float px) {
        spec_.right = detail::AnchorValue::Px(px);
        return *this;
    }

    AnchorBuilder& top(float px) {
        spec_.top = detail::AnchorValue::Px(px);
        return *this;
    }

    AnchorBuilder& bottom(float px) {
        spec_.bottom = detail::AnchorValue::Px(px);
        return *this;
    }

    AnchorBuilder& center_x(float offset = 0.0f) {
        spec_.center_x = detail::AnchorValue::Px(offset);
        return *this;
    }

    AnchorBuilder& center_y(float offset = 0.0f) {
        spec_.center_y = detail::AnchorValue::Px(offset);
        return *this;
    }

    AnchorBuilder& width(float px) {
        spec_.width = detail::AnchorValue::Px(px);
        return *this;
    }

    AnchorBuilder& height(float px) {
        spec_.height = detail::AnchorValue::Px(px);
        return *this;
    }

    AnchorBuilder& size(float w, float h) {
        return width(w).height(h);
    }

    AnchorBuilder& left_percent(float percent) {
        spec_.left = detail::AnchorValue::Percent(percent);
        return *this;
    }

    AnchorBuilder& right_percent(float percent) {
        spec_.right = detail::AnchorValue::Percent(percent);
        return *this;
    }

    AnchorBuilder& top_percent(float percent) {
        spec_.top = detail::AnchorValue::Percent(percent);
        return *this;
    }

    AnchorBuilder& bottom_percent(float percent) {
        spec_.bottom = detail::AnchorValue::Percent(percent);
        return *this;
    }

    AnchorBuilder& width_percent(float percent) {
        spec_.width = detail::AnchorValue::Percent(percent);
        return *this;
    }

    AnchorBuilder& height_percent(float percent) {
        spec_.height = detail::AnchorValue::Percent(percent);
        return *this;
    }

    AnchorBuilder& center_x_percent(float percent) {
        spec_.center_x = detail::AnchorValue::Percent(percent);
        return *this;
    }

    AnchorBuilder& center_y_percent(float percent) {
        spec_.center_y = detail::AnchorValue::Percent(percent);
        return *this;
    }

    Rect resolve() const {
        return ui_.resolve_anchor(spec_, has_reference_ ? reference_ : ui_.content_rect());
    }

private:
    UI& ui_;
    detail::AnchorRect spec_{};
    Rect reference_{};
    bool has_reference_{false};
};

template <typename Derived>
class PositionedBuilderBase {
public:
    explicit PositionedBuilderBase(UI& ui)
        : ui_(ui) {}

    Derived& in(const Rect& rect) {
        placement_.has_rect = true;
        placement_.rect = make_rect(rect.x, rect.y, rect.w, rect.h);
        return self();
    }

    Derived& at(float x, float y) {
        placement_.has_pos = true;
        placement_.x = x;
        placement_.y = y;
        return self();
    }

    Derived& size(float w, float h) {
        placement_.has_size = true;
        placement_.w = std::max(0.0f, w);
        placement_.h = std::max(0.0f, h);
        return self();
    }

    Derived& fill_parent() {
        placement_.use_parent = true;
        return self();
    }

    Derived& after_last(float dx = 0.0f, float dy = 0.0f) {
        placement_.use_after_last = true;
        placement_.use_below_last = false;
        placement_.offset_x = dx;
        placement_.offset_y = dy;
        return self();
    }

    Derived& below_last(float dx = 0.0f, float dy = 0.0f) {
        placement_.use_below_last = true;
        placement_.use_after_last = false;
        placement_.offset_x = dx;
        placement_.offset_y = dy;
        return self();
    }

protected:
    Rect resolve_rect(float default_w = 0.0f, float default_h = 0.0f) const {
        if (placement_.has_rect) {
            return placement_.rect;
        }
        if (placement_.use_parent) {
            return ui_.content();
        }

        const float resolved_w = placement_.has_size ? placement_.w : default_w;
        const float resolved_h = placement_.has_size ? placement_.h : default_h;

        if (placement_.use_after_last) {
            const Rect last = ui_.last();
            return make_rect(last.x + last.w + placement_.offset_x, last.y + placement_.offset_y, resolved_w, resolved_h);
        }
        if (placement_.use_below_last) {
            const Rect last = ui_.last();
            return make_rect(last.x + placement_.offset_x, last.y + last.h + placement_.offset_y, resolved_w, resolved_h);
        }
        if (placement_.has_pos) {
            const Rect parent = ui_.content();
            return make_rect(
                placement_.x,
                placement_.y,
                resolved_w > 0.0f ? resolved_w : parent.w,
                resolved_h > 0.0f ? resolved_h : parent.h);
        }
        if (resolved_w > 0.0f || resolved_h > 0.0f) {
            const Rect parent = ui_.content();
            return make_rect(parent.x, parent.y, resolved_w > 0.0f ? resolved_w : parent.w,
                             resolved_h > 0.0f ? resolved_h : parent.h);
        }
        return ui_.content();
    }

    UI& ui_;
    Placement placement_{};

private:
    Derived& self() {
        return static_cast<Derived&>(*this);
    }
};

template <typename Derived, typename Primitive>
class PrimitiveBuilderBase : public PositionedBuilderBase<Derived> {
public:
    explicit PrimitiveBuilderBase(UI& ui)
        : PositionedBuilderBase<Derived>(ui) {}

    Derived& opacity(float opacity) {
        primitive_.opacity = std::clamp(opacity, 0.0f, 1.0f);
        return self();
    }

    Derived& alpha(float alpha) {
        return opacity(alpha);
    }

    Derived& clip(const Rect& rect) {
        primitive_.clip = gfx::clip(rect);
        return self();
    }

    Derived& no_clip() {
        primitive_.clip = eui::graphics::ClipRect{};
        return self();
    }

    Derived& translate(float dx, float dy) {
        primitive_.transform_2d.translation_x = dx;
        primitive_.transform_2d.translation_y = dy;
        return self();
    }

    Derived& scale(float uniform) {
        return scale(uniform, uniform);
    }

    Derived& scale(float sx, float sy) {
        primitive_.transform_2d.scale_x = std::max(0.0f, sx);
        primitive_.transform_2d.scale_y = std::max(0.0f, sy);
        return self();
    }

    Derived& rotate(float degrees) {
        primitive_.transform_2d.rotation_deg = degrees;
        return self();
    }

    Derived& origin(float x, float y) {
        primitive_.transform_2d.origin_x = x;
        primitive_.transform_2d.origin_y = y;
        primitive_.transform_3d.origin_x = x;
        primitive_.transform_3d.origin_y = y;
        use_percent_origin_ = false;
        return self();
    }

    Derived& origin_percent(float x, float y) {
        origin_percent_x_ = x;
        origin_percent_y_ = y;
        use_percent_origin_ = true;
        return self();
    }

    Derived& origin_center() {
        return origin_percent(0.5f, 0.5f);
    }

    Derived& translate_3d(float x, float y, float z) {
        primitive_.transform_3d.translation_x = x;
        primitive_.transform_3d.translation_y = y;
        primitive_.transform_3d.translation_z = z;
        return self();
    }

    Derived& scale_3d(float x, float y, float z) {
        primitive_.transform_3d.scale_x = std::max(0.0f, x);
        primitive_.transform_3d.scale_y = std::max(0.0f, y);
        primitive_.transform_3d.scale_z = std::max(0.0f, z);
        return self();
    }

    Derived& rotate_x(float degrees) {
        primitive_.transform_3d.rotation_x_deg = degrees;
        return self();
    }

    Derived& rotate_y(float degrees) {
        primitive_.transform_3d.rotation_y_deg = degrees;
        return self();
    }

    Derived& rotate_z(float degrees) {
        primitive_.transform_3d.rotation_z_deg = degrees;
        return self();
    }

    Derived& perspective(float amount) {
        primitive_.transform_3d.perspective = std::max(0.0f, amount);
        return self();
    }

    Derived& origin_3d(float x, float y, float z) {
        primitive_.transform_3d.origin_x = x;
        primitive_.transform_3d.origin_y = y;
        primitive_.transform_3d.origin_z = z;
        use_percent_origin_ = false;
        return self();
    }

protected:
    Primitive& primitive() {
        return primitive_;
    }

    const Primitive& primitive() const {
        return primitive_;
    }

    Rect resolve_primitive_rect(float default_w = 0.0f, float default_h = 0.0f) {
        const Rect rect = this->resolve_rect(default_w, default_h);
        primitive_.rect = eui::to_graphics_rect(rect);
        if (use_percent_origin_) {
            const float ox = rect.w * origin_percent_x_;
            const float oy = rect.h * origin_percent_y_;
            primitive_.transform_2d.origin_x = ox;
            primitive_.transform_2d.origin_y = oy;
            primitive_.transform_3d.origin_x = ox;
            primitive_.transform_3d.origin_y = oy;
        }
        return rect;
    }

private:
    Derived& self() {
        return static_cast<Derived&>(*this);
    }

    Primitive primitive_{};
    bool use_percent_origin_{false};
    float origin_percent_x_{0.0f};
    float origin_percent_y_{0.0f};
};

inline void paint_shadow(Context& ctx, const Rect& rect, float radius, const ShadowStyle& shadow, float alpha) {
    if (!shadow.enabled || !has_area(rect)) {
        return;
    }

    const float blur = std::max(0.0f, shadow.blur);
    const float spread = std::max(0.0f, shadow.spread);
    const int layers = std::clamp(static_cast<int>(blur / 8.0f) + 2, 2, 6);

    for (int i = layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(layers);
        const float grow = spread + blur * t * 0.55f;
        const Rect layer = make_rect(
            rect.x + shadow.offset_x - grow,
            rect.y + shadow.offset_y - grow,
            rect.w + grow * 2.0f,
            rect.h + grow * 2.0f);
        const float layer_alpha = alpha * (0.16f / static_cast<float>(layers)) * (1.12f - 0.72f * t);
        ctx.paint_filled_rect(layer, with_alpha(shadow.color, layer_alpha), std::max(0.0f, radius + grow * 0.45f));
    }
}

inline void paint_gradient(Context& ctx, const Rect& rect, float radius, const GradientStyle& gradient, float alpha) {
    if (!gradient.enabled || !has_area(rect)) {
        return;
    }
    ctx.paint_filled_rect(rect, gfx::vertical_gradient(gradient.top, gradient.bottom, alpha), radius);
}

class RectangleBuilder : public PositionedBuilderBase<RectangleBuilder> {
public:
    explicit RectangleBuilder(UI& ui)
        : PositionedBuilderBase<RectangleBuilder>(ui),
          fill_(ui.theme().secondary),
          text_color_(ui.theme().text) {}

    RectangleBuilder& radius(float radius) {
        radius_ = std::max(0.0f, radius);
        return *this;
    }

    RectangleBuilder& fill(const Color& color) {
        fill_ = color;
        has_fill_ = true;
        gradient_.enabled = false;
        return *this;
    }

    RectangleBuilder& fill(std::uint32_t hex, float alpha = 1.0f) {
        return fill(rgb(hex, alpha));
    }

    RectangleBuilder& gradient(const Color& top, const Color& bottom) {
        gradient_.enabled = true;
        gradient_.top = top;
        gradient_.bottom = bottom;
        has_fill_ = false;
        return *this;
    }

    RectangleBuilder& gradient(std::uint32_t top, std::uint32_t bottom, float alpha = 1.0f) {
        return gradient(rgb(top, alpha), rgb(bottom, alpha));
    }

    RectangleBuilder& alpha(float alpha) {
        alpha_ = std::clamp(alpha, 0.0f, 1.0f);
        return *this;
    }

    RectangleBuilder& stroke(const Color& color, float width = 1.0f) {
        stroke_.enabled = true;
        stroke_.color = color;
        stroke_.width = std::max(0.0f, width);
        return *this;
    }

    RectangleBuilder& stroke(std::uint32_t hex, float width = 1.0f, float alpha = 1.0f) {
        return stroke(rgb(hex, alpha), width);
    }

    RectangleBuilder& shadow(float offset_x, float offset_y, float blur, const Color& color, float spread = 0.0f) {
        shadow_.enabled = true;
        shadow_.offset_x = offset_x;
        shadow_.offset_y = offset_y;
        shadow_.blur = blur;
        shadow_.spread = spread;
        shadow_.color = color;
        return *this;
    }

    RectangleBuilder& shadow(float offset_x, float offset_y, float blur, std::uint32_t hex, float alpha = 0.18f,
                             float spread = 0.0f) {
        return shadow(offset_x, offset_y, blur, rgb(hex, alpha), spread);
    }

    RectangleBuilder& blur(float radius) {
        blur_radius_ = std::max(0.0f, radius);
        return *this;
    }

    RectangleBuilder& text(std::string_view text) {
        text_ = std::string(text);
        has_text_ = !text_.empty();
        return *this;
    }

    RectangleBuilder& font(float font_size) {
        text_font_ = std::max(8.0f, font_size);
        return *this;
    }

    RectangleBuilder& text_color(const Color& color) {
        text_color_ = color;
        return *this;
    }

    RectangleBuilder& text_color(std::uint32_t hex, float alpha = 1.0f) {
        return text_color(rgb(hex, alpha));
    }

    RectangleBuilder& align(TextAlign align) {
        text_align_ = align;
        return *this;
    }

    RectangleBuilder& center() {
        text_align_ = TextAlign::Center;
        return *this;
    }

    RectangleBuilder& text_padding(float padding) {
        text_padding_ = std::max(0.0f, padding);
        return *this;
    }

    Rect draw() {
        const Rect rect = resolve_rect();
        Context& ctx = ui_.ctx();

        if (blur_radius_ > 0.0f && !shadow_.enabled) {
            shadow(0.0f, 8.0f, blur_radius_, with_alpha(fill_, 0.18f), blur_radius_ * 0.15f);
        }
        paint_shadow(ctx, rect, radius_, shadow_, alpha_);

        if (gradient_.enabled) {
            paint_gradient(ctx, rect, radius_, gradient_, alpha_);
        } else if (has_fill_) {
            ctx.paint_filled_rect(rect, with_alpha(fill_, alpha_), radius_);
        }

        if (stroke_.enabled && stroke_.width > 0.0f) {
            ctx.paint_outline_rect(rect, with_alpha(stroke_.color, alpha_), radius_, stroke_.width);
        }

        if (has_text_) {
            const Rect inner = inset(rect, text_padding_);
            ctx.paint_text(text_, inner, with_alpha(text_color_, alpha_), text_font_, text_align_, &rect);
        }

        ui_.remember(rect);
        return rect;
    }

private:
    bool has_fill_{true};
    Color fill_{};
    GradientStyle gradient_{};
    StrokeStyle stroke_{};
    ShadowStyle shadow_{};
    float radius_{0.0f};
    float alpha_{1.0f};
    float blur_radius_{0.0f};
    bool has_text_{false};
    std::string text_{};
    float text_font_{14.0f};
    float text_padding_{12.0f};
    Color text_color_{};
    TextAlign text_align_{TextAlign::Center};
};

class TextBuilder : public PositionedBuilderBase<TextBuilder> {
public:
    TextBuilder(UI& ui, std::string_view text)
        : PositionedBuilderBase<TextBuilder>(ui),
          text_(text),
          color_(ui.theme().text) {}

    TextBuilder& font(float font_size) {
        font_size_ = std::max(8.0f, font_size);
        return *this;
    }

    TextBuilder& color(const Color& color) {
        color_ = color;
        return *this;
    }

    TextBuilder& color(std::uint32_t hex, float alpha = 1.0f) {
        return color(rgb(hex, alpha));
    }

    TextBuilder& alpha(float alpha) {
        alpha_ = std::clamp(alpha, 0.0f, 1.0f);
        return *this;
    }

    TextBuilder& align(TextAlign align) {
        align_ = align;
        return *this;
    }

    TextBuilder& left() {
        align_ = TextAlign::Left;
        return *this;
    }

    TextBuilder& center() {
        align_ = TextAlign::Center;
        return *this;
    }

    TextBuilder& right() {
        align_ = TextAlign::Right;
        return *this;
    }

    Rect draw() {
        const float default_w = ui_.measure_text(text_, font_size_) + font_size_;
        const float default_h = font_size_ + 6.0f;
        const Rect rect = resolve_rect(default_w, default_h);
        ui_.ctx().paint_text(text_, rect, with_alpha(color_, alpha_), font_size_, align_, &rect);
        ui_.remember(rect);
        return rect;
    }

private:
    std::string text_{};
    float font_size_{13.0f};
    Color color_{};
    float alpha_{1.0f};
    TextAlign align_{TextAlign::Left};
};

class ShapeBuilder : public PrimitiveBuilderBase<ShapeBuilder, eui::graphics::RectanglePrimitive> {
public:
    explicit ShapeBuilder(UI& ui)
        : PrimitiveBuilderBase<ShapeBuilder, eui::graphics::RectanglePrimitive>(ui) {
        primitive().fill = gfx::solid(ui.theme().secondary);
    }

    ShapeBuilder& radius(float uniform) {
        primitive().radius = gfx::radius(uniform);
        return *this;
    }

    ShapeBuilder& radius(float top_left, float top_right, float bottom_right, float bottom_left) {
        primitive().radius = gfx::radius(top_left, top_right, bottom_right, bottom_left);
        return *this;
    }

    ShapeBuilder& fill(const eui::graphics::Brush& brush) {
        primitive().fill = brush;
        return *this;
    }

    ShapeBuilder& fill(const Color& color) {
        primitive().fill = gfx::solid(color);
        return *this;
    }

    ShapeBuilder& fill(std::uint32_t hex, float alpha = 1.0f) {
        primitive().fill = gfx::solid(hex, alpha);
        return *this;
    }

    ShapeBuilder& gradient(const eui::graphics::Color& top, const eui::graphics::Color& bottom) {
        primitive().fill = gfx::vertical_gradient(top, bottom);
        return *this;
    }

    ShapeBuilder& gradient(const Color& top, const Color& bottom, float alpha = 1.0f) {
        primitive().fill = gfx::vertical_gradient(top, bottom, alpha);
        return *this;
    }

    ShapeBuilder& gradient(std::uint32_t top, std::uint32_t bottom, float alpha = 1.0f) {
        primitive().fill = gfx::vertical_gradient(top, bottom, alpha);
        return *this;
    }

    ShapeBuilder& radial_gradient(const eui::graphics::Color& inner, const eui::graphics::Color& outer,
                                  float radius = 0.75f) {
        primitive().fill = gfx::radial_gradient(inner, outer, radius);
        return *this;
    }

    ShapeBuilder& radial_gradient(const Color& inner, const Color& outer, float radius = 0.75f,
                                  float alpha = 1.0f) {
        primitive().fill = gfx::radial_gradient(inner, outer, radius, alpha);
        return *this;
    }

    ShapeBuilder& radial_gradient(std::uint32_t inner, std::uint32_t outer, float radius = 0.75f,
                                  float alpha = 1.0f) {
        primitive().fill = gfx::radial_gradient(inner, outer, radius, alpha);
        return *this;
    }

    ShapeBuilder& no_fill() {
        primitive().fill = eui::graphics::Brush{};
        return *this;
    }

    ShapeBuilder& stroke(const eui::graphics::Stroke& stroke) {
        primitive().stroke = stroke;
        return *this;
    }

    ShapeBuilder& stroke(const eui::graphics::Brush& brush, float width = 1.0f) {
        primitive().stroke = gfx::stroke(brush, width);
        return *this;
    }

    ShapeBuilder& stroke(const Color& color, float width = 1.0f, float alpha = 1.0f) {
        primitive().stroke = gfx::stroke(color, width, alpha);
        return *this;
    }

    ShapeBuilder& stroke(std::uint32_t hex, float width = 1.0f, float alpha = 1.0f) {
        primitive().stroke = gfx::stroke(hex, width, alpha);
        return *this;
    }

    ShapeBuilder& no_stroke() {
        primitive().stroke = eui::graphics::Stroke{};
        return *this;
    }

    ShapeBuilder& shadow(float offset_x, float offset_y, float blur, const eui::graphics::Color& color,
                         float spread = 0.0f) {
        primitive().shadow = eui::graphics::Shadow{
            offset_x,
            offset_y,
            std::max(0.0f, blur),
            std::max(0.0f, spread),
            color,
        };
        return *this;
    }

    ShapeBuilder& shadow(float offset_x, float offset_y, float blur, const Color& color, float alpha = 1.0f,
                         float spread = 0.0f) {
        return shadow(offset_x, offset_y, blur, gfx::color(color, alpha), spread);
    }

    ShapeBuilder& shadow(float offset_x, float offset_y, float blur, std::uint32_t hex, float alpha = 0.18f,
                         float spread = 0.0f) {
        return shadow(offset_x, offset_y, blur, gfx::color(hex, alpha), spread);
    }

    ShapeBuilder& no_shadow() {
        primitive().shadow = eui::graphics::Shadow{};
        return *this;
    }

    ShapeBuilder& blur(float radius, float backdrop_radius = 0.0f) {
        primitive().blur.radius = std::max(0.0f, radius);
        primitive().blur.backdrop_radius = std::max(0.0f, backdrop_radius);
        return *this;
    }

    Rect draw() {
        resolve_primitive_rect();
        return this->ui_.paint(primitive());
    }
};

class GlyphBuilder : public PrimitiveBuilderBase<GlyphBuilder, eui::graphics::IconPrimitive> {
public:
    GlyphBuilder(UI& ui, std::uint32_t codepoint)
        : PrimitiveBuilderBase<GlyphBuilder, eui::graphics::IconPrimitive>(ui),
          font_family_() {
        primitive().glyph = codepoint;
        primitive().tint = gfx::solid(ui.theme().text);
    }

    GlyphBuilder& codepoint(std::uint32_t codepoint) {
        primitive().glyph = codepoint;
        return *this;
    }

    GlyphBuilder& font_family(std::string_view family) {
        font_family_ = std::string(family);
        return *this;
    }

    GlyphBuilder& tint(const eui::graphics::Brush& brush) {
        primitive().tint = brush;
        return *this;
    }

    GlyphBuilder& tint(const Color& color) {
        primitive().tint = gfx::solid(color);
        return *this;
    }

    GlyphBuilder& tint(std::uint32_t hex, float alpha = 1.0f) {
        primitive().tint = gfx::solid(hex, alpha);
        return *this;
    }

    Rect draw() {
        resolve_primitive_rect(18.0f, 18.0f);
        primitive().font_family = font_family_;
        return this->ui_.paint(primitive());
    }

private:
    std::string font_family_{};
};

class ImageBuilder : public PrimitiveBuilderBase<ImageBuilder, eui::graphics::ImagePrimitive> {
public:
    ImageBuilder(UI& ui, std::string_view source)
        : PrimitiveBuilderBase<ImageBuilder, eui::graphics::ImagePrimitive>(ui),
          source_(source) {}

    ImageBuilder& source(std::string_view source) {
        source_ = std::string(source);
        return *this;
    }

    ImageBuilder& radius(float uniform) {
        primitive().radius = gfx::radius(uniform);
        return *this;
    }

    ImageBuilder& radius(float top_left, float top_right, float bottom_right, float bottom_left) {
        primitive().radius = gfx::radius(top_left, top_right, bottom_right, bottom_left);
        return *this;
    }

    ImageBuilder& fit(eui::graphics::ImageFit fit_mode) {
        primitive().fit = fit_mode;
        return *this;
    }

    ImageBuilder& fill_mode() {
        primitive().fit = eui::graphics::ImageFit::fill;
        return *this;
    }

    ImageBuilder& contain() {
        primitive().fit = eui::graphics::ImageFit::contain;
        return *this;
    }

    ImageBuilder& cover() {
        primitive().fit = eui::graphics::ImageFit::cover;
        return *this;
    }

    ImageBuilder& stretch() {
        primitive().fit = eui::graphics::ImageFit::stretch;
        return *this;
    }

    ImageBuilder& center() {
        primitive().fit = eui::graphics::ImageFit::center;
        return *this;
    }

    Rect draw() {
        resolve_primitive_rect(72.0f, 72.0f);
        primitive().source = source_;
        return this->ui_.paint(primitive());
    }

private:
    std::string source_{};
};

enum class SurfaceTone {
    panel,
    card,
};

class SurfaceBuilder : public PositionedBuilderBase<SurfaceBuilder> {
public:
    SurfaceBuilder(UI& ui, SurfaceTone tone, std::string_view title)
        : PositionedBuilderBase<SurfaceBuilder>(ui),
          tone_(tone),
          title_(title),
          fill_(tone == SurfaceTone::panel ? ui.theme().panel : mix(ui.theme().panel, ui.theme().secondary, 0.18f)),
          stroke_color_(tone == SurfaceTone::panel ? ui.theme().panel_border
                                                   : mix(ui.theme().panel_border, ui.theme().primary, 0.05f)),
          title_color_(ui.theme().text) {
        radius_ = (tone == SurfaceTone::panel) ? 24.0f : std::max(12.0f, ui.theme().radius + 4.0f);
        padding_ = (tone == SurfaceTone::panel) ? 18.0f : 16.0f;
        stroke_.enabled = true;
        stroke_.width = 1.0f;
        stroke_.color = stroke_color_;
        shadow_.enabled = true;
        shadow_.offset_x = 0.0f;
        shadow_.offset_y = 10.0f;
        shadow_.blur = (tone == SurfaceTone::panel) ? 26.0f : 18.0f;
        shadow_.spread = 0.0f;
        shadow_.color = rgb(0x020617, tone == SurfaceTone::panel ? 0.18f : 0.12f);
    }

    SurfaceBuilder& padding(float padding) {
        padding_ = std::max(0.0f, padding);
        return *this;
    }

    SurfaceBuilder& radius(float radius) {
        radius_ = std::max(0.0f, radius);
        return *this;
    }

    SurfaceBuilder& fill(const Color& color) {
        fill_ = color;
        has_fill_ = true;
        gradient_.enabled = false;
        return *this;
    }

    SurfaceBuilder& fill(std::uint32_t hex, float alpha = 1.0f) {
        return fill(rgb(hex, alpha));
    }

    SurfaceBuilder& gradient(const Color& top, const Color& bottom) {
        gradient_.enabled = true;
        gradient_.top = top;
        gradient_.bottom = bottom;
        has_fill_ = false;
        return *this;
    }

    SurfaceBuilder& gradient(std::uint32_t top, std::uint32_t bottom, float alpha = 1.0f) {
        return gradient(rgb(top, alpha), rgb(bottom, alpha));
    }

    SurfaceBuilder& alpha(float alpha) {
        alpha_ = std::clamp(alpha, 0.0f, 1.0f);
        return *this;
    }

    SurfaceBuilder& stroke(const Color& color, float width = 1.0f) {
        stroke_.enabled = true;
        stroke_.width = std::max(0.0f, width);
        stroke_.color = color;
        return *this;
    }

    SurfaceBuilder& stroke(std::uint32_t hex, float width = 1.0f, float alpha = 1.0f) {
        return stroke(rgb(hex, alpha), width);
    }

    SurfaceBuilder& no_stroke() {
        stroke_.enabled = false;
        return *this;
    }

    SurfaceBuilder& shadow(float offset_x, float offset_y, float blur, const Color& color, float spread = 0.0f) {
        shadow_.enabled = true;
        shadow_.offset_x = offset_x;
        shadow_.offset_y = offset_y;
        shadow_.blur = blur;
        shadow_.spread = spread;
        shadow_.color = color;
        return *this;
    }

    SurfaceBuilder& shadow(float offset_x, float offset_y, float blur, std::uint32_t hex, float alpha = 0.18f,
                           float spread = 0.0f) {
        return shadow(offset_x, offset_y, blur, rgb(hex, alpha), spread);
    }

    SurfaceBuilder& title(std::string_view title) {
        title_ = std::string(title);
        return *this;
    }

    SurfaceBuilder& title_font(float font_size) {
        title_font_ = std::max(8.0f, font_size);
        return *this;
    }

    SurfaceBuilder& title_color(const Color& color) {
        title_color_ = color;
        return *this;
    }

    SurfaceBuilder& title_color(std::uint32_t hex, float alpha = 1.0f) {
        return title_color(rgb(hex, alpha));
    }

    SurfaceBuilder& title_gap(float gap) {
        title_gap_ = std::max(0.0f, gap);
        return *this;
    }

    RegionScope begin() {
        const Rect shell = resolve_rect();
        Context& ctx = ui_.ctx();

        paint_shadow(ctx, shell, radius_, shadow_, alpha_);
        if (gradient_.enabled) {
            paint_gradient(ctx, shell, radius_, gradient_, alpha_);
        } else if (has_fill_) {
            ctx.paint_filled_rect(shell, with_alpha(fill_, alpha_), radius_);
        }
        if (stroke_.enabled && stroke_.width > 0.0f) {
            ctx.paint_outline_rect(shell, with_alpha(stroke_.color, alpha_), radius_, stroke_.width);
        }

        Rect inner = inset(shell, padding_);
        if (!title_.empty()) {
            const float title_font =
                (title_font_ > 0.0f) ? title_font_ : std::clamp(radius_ * 0.56f + 6.0f, 14.0f, 24.0f);
            const float title_height = title_font + 4.0f;
            const Rect title_rect = make_rect(inner.x, inner.y, inner.w, title_height);
            ctx.paint_text(title_, title_rect, with_alpha(title_color_, alpha_), title_font, TextAlign::Left, &shell);
            inner.y += title_height + title_gap_;
            inner.h = std::max(0.0f, inner.h - title_height - title_gap_);
        }

        detail::ContextAccess::push_layout_rect(ctx, inner);
        ui_.remember(shell);
        return RegionScope(&ui_, shell);
    }

    template <typename Fn>
    void begin(Fn&& fn) {
        auto scope = begin();
        static_cast<void>(scope);
        fn(scope);
    }

private:
    SurfaceTone tone_{SurfaceTone::panel};
    std::string title_{};
    bool has_fill_{true};
    Color fill_{};
    GradientStyle gradient_{};
    StrokeStyle stroke_{};
    ShadowStyle shadow_{};
    Color stroke_color_{};
    float padding_{16.0f};
    float radius_{18.0f};
    float alpha_{1.0f};
    float title_font_{0.0f};
    float title_gap_{8.0f};
    Color title_color_{};
};

class LabelBuilder {
public:
    LabelBuilder(UI& ui, std::string_view text)
        : ui_(ui), text_(text) {}

    LabelBuilder& font(float font_size) {
        font_size_ = std::max(8.0f, font_size);
        return *this;
    }

    LabelBuilder& muted(bool muted = true) {
        muted_ = muted;
        return *this;
    }

    LabelBuilder& height(float height) {
        height_ = std::max(0.0f, height);
        return *this;
    }

    void draw() {
        detail::ContextAccess::label(ui_.ctx(), text_, font_size_, muted_, height_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
    }

private:
    UI& ui_;
    std::string text_{};
    float font_size_{13.0f};
    bool muted_{false};
    float height_{0.0f};
};

class ButtonBuilder {
public:
    ButtonBuilder(UI& ui, std::string_view label)
        : ui_(ui), label_(label) {}

    ButtonBuilder& style(ButtonStyle style) {
        style_ = style;
        return *this;
    }

    ButtonBuilder& primary() {
        style_ = ButtonStyle::Primary;
        return *this;
    }

    ButtonBuilder& secondary() {
        style_ = ButtonStyle::Secondary;
        return *this;
    }

    ButtonBuilder& ghost() {
        style_ = ButtonStyle::Ghost;
        return *this;
    }

    ButtonBuilder& height(float height) {
        height_ = std::max(18.0f, height);
        return *this;
    }

    ButtonBuilder& text_scale(float text_scale) {
        text_scale_ = std::clamp(text_scale, 0.6f, 2.0f);
        return *this;
    }

    bool draw() {
        const bool clicked = detail::ContextAccess::button(ui_.ctx(), label_, style_, height_, text_scale_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return clicked;
    }

private:
    UI& ui_;
    std::string label_{};
    ButtonStyle style_{ButtonStyle::Secondary};
    float height_{34.0f};
    float text_scale_{1.0f};
};

class TabBuilder {
public:
    TabBuilder(UI& ui, std::string_view label, bool selected = false)
        : ui_(ui), label_(label), selected_(selected) {}

    TabBuilder& selected(bool selected = true) {
        selected_ = selected;
        return *this;
    }

    TabBuilder& height(float height) {
        height_ = std::max(18.0f, height);
        return *this;
    }

    bool draw() {
        const bool clicked = detail::ContextAccess::tab(ui_.ctx(), label_, selected_, height_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return clicked;
    }

private:
    UI& ui_;
    std::string label_{};
    bool selected_{false};
    float height_{30.0f};
};

class SliderFloatBuilder {
public:
    SliderFloatBuilder(UI& ui, std::string_view label, float& value)
        : ui_(ui), label_(label), value_(value) {}

    SliderFloatBuilder& range(float min_value, float max_value) {
        min_value_ = min_value;
        max_value_ = max_value;
        return *this;
    }

    SliderFloatBuilder& min(float min_value) {
        min_value_ = min_value;
        return *this;
    }

    SliderFloatBuilder& max(float max_value) {
        max_value_ = max_value;
        return *this;
    }

    SliderFloatBuilder& decimals(int decimals) {
        decimals_ = decimals;
        return *this;
    }

    SliderFloatBuilder& height(float height) {
        height_ = std::max(18.0f, height);
        return *this;
    }

    bool draw() {
        const bool changed = detail::ContextAccess::slider_float(
            ui_.ctx(), label_, value_, min_value_, max_value_, decimals_, height_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return changed;
    }

private:
    UI& ui_;
    std::string label_{};
    float& value_;
    float min_value_{0.0f};
    float max_value_{1.0f};
    int decimals_{-1};
    float height_{40.0f};
};

class InputBuilder {
public:
    InputBuilder(UI& ui, std::string_view label, std::string& value)
        : ui_(ui), label_(label), value_(value) {}

    InputBuilder& label(std::string_view label) {
        label_ = std::string(label);
        return *this;
    }

    InputBuilder& placeholder(std::string_view placeholder) {
        placeholder_ = std::string(placeholder);
        return *this;
    }

    InputBuilder& height(float height) {
        height_ = std::max(18.0f, height);
        return *this;
    }

    InputBuilder& align_right(bool align_right = true) {
        align_right_ = align_right;
        return *this;
    }

    bool draw() {
        const bool changed =
            detail::ContextAccess::input_text(ui_.ctx(), label_, value_, height_, placeholder_, align_right_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return changed;
    }

private:
    UI& ui_;
    std::string label_{};
    std::string& value_;
    std::string placeholder_{};
    float height_{34.0f};
    bool align_right_{false};
};

class FloatInputBuilder {
public:
    FloatInputBuilder(UI& ui, std::string_view label, float& value)
        : ui_(ui), label_(label), value_(value) {}

    FloatInputBuilder& range(float min_value, float max_value) {
        min_value_ = min_value;
        max_value_ = max_value;
        return *this;
    }

    FloatInputBuilder& min(float min_value) {
        min_value_ = min_value;
        return *this;
    }

    FloatInputBuilder& max(float max_value) {
        max_value_ = max_value;
        return *this;
    }

    FloatInputBuilder& decimals(int decimals) {
        decimals_ = decimals;
        return *this;
    }

    FloatInputBuilder& height(float height) {
        height_ = std::max(18.0f, height);
        return *this;
    }

    bool draw() {
        const bool changed = detail::ContextAccess::input_float(
            ui_.ctx(), label_, value_, min_value_, max_value_, decimals_, height_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return changed;
    }

private:
    UI& ui_;
    std::string label_{};
    float& value_;
    float min_value_{0.0f};
    float max_value_{1.0f};
    int decimals_{2};
    float height_{34.0f};
};

class ReadonlyBuilder {
public:
    ReadonlyBuilder(UI& ui, std::string_view label, std::string_view value)
        : ui_(ui), label_(label), value_(value) {}

    ReadonlyBuilder& height(float height) {
        height_ = std::max(18.0f, height);
        return *this;
    }

    ReadonlyBuilder& align_right(bool align_right = true) {
        align_right_ = align_right;
        return *this;
    }

    ReadonlyBuilder& value_scale(float value_scale) {
        value_scale_ = std::clamp(value_scale, 0.5f, 2.2f);
        return *this;
    }

    ReadonlyBuilder& muted(bool muted = true) {
        muted_ = muted;
        return *this;
    }

    void draw() {
        detail::ContextAccess::input_readonly(ui_.ctx(), label_, value_, height_, align_right_, value_scale_, muted_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
    }

private:
    UI& ui_;
    std::string label_{};
    std::string value_{};
    float height_{34.0f};
    bool align_right_{false};
    float value_scale_{1.0f};
    bool muted_{true};
};

class ProgressBuilder {
public:
    ProgressBuilder(UI& ui, std::string_view label, float ratio)
        : ui_(ui), label_(label), ratio_(ratio) {}

    ProgressBuilder& height(float height) {
        height_ = std::max(4.0f, height);
        return *this;
    }

    ProgressBuilder& ratio(float ratio) {
        ratio_ = ratio;
        return *this;
    }

    void draw() {
        detail::ContextAccess::progress(ui_.ctx(), label_, std::clamp(ratio_, 0.0f, 1.0f), height_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
    }

private:
    UI& ui_;
    std::string label_{};
    float ratio_{0.0f};
    float height_{10.0f};
};

class TextAreaBuilder {
public:
    TextAreaBuilder(UI& ui, std::string_view label, std::string& text)
        : ui_(ui), label_(label), text_(text) {}

    TextAreaBuilder& height(float height) {
        height_ = std::max(48.0f, height);
        return *this;
    }

    bool draw() {
        const bool changed = detail::ContextAccess::text_area(ui_.ctx(), label_, text_, height_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return changed;
    }

private:
    UI& ui_;
    std::string label_{};
    std::string& text_;
    float height_{170.0f};
};

class TextAreaReadonlyBuilder {
public:
    TextAreaReadonlyBuilder(UI& ui, std::string_view label, std::string_view text)
        : ui_(ui), label_(label), text_(text) {}

    TextAreaReadonlyBuilder& height(float height) {
        height_ = std::max(48.0f, height);
        return *this;
    }

    void draw() {
        detail::ContextAccess::text_area_readonly(ui_.ctx(), label_, text_, height_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
    }

private:
    UI& ui_;
    std::string label_{};
    std::string text_{};
    float height_{170.0f};
};

class DropdownBuilder {
public:
    DropdownBuilder(UI& ui, std::string_view label, bool& open)
        : ui_(ui), label_(label), open_(open) {}

    DropdownBuilder& body_height(float height) {
        body_height_ = std::max(36.0f, height);
        return *this;
    }

    DropdownBuilder& padding(float padding) {
        padding_ = std::max(0.0f, padding);
        return *this;
    }

    DropdownScope begin() {
        const bool active =
            detail::ContextAccess::begin_dropdown(ui_.ctx(), label_, open_, body_height_, padding_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return active ? DropdownScope(&ui_) : DropdownScope();
    }

    template <typename Fn>
    void begin(Fn&& fn) {
        auto scope = begin();
        if (scope.active()) {
            fn(scope);
        }
    }

private:
    UI& ui_;
    std::string label_{};
    bool& open_;
    float body_height_{104.0f};
    float padding_{10.0f};
};

class ScrollAreaBuilder {
public:
    ScrollAreaBuilder(UI& ui, std::string_view label)
        : ui_(ui), label_(label) {}

    ScrollAreaBuilder& height(float height) {
        height_ = std::max(40.0f, height);
        return *this;
    }

    ScrollAreaBuilder& padding(float padding) {
        options_.padding = padding;
        return *this;
    }

    ScrollAreaBuilder& scrollbar_width(float width) {
        options_.scrollbar_width = width;
        return *this;
    }

    ScrollAreaBuilder& min_thumb_height(float height) {
        options_.min_thumb_height = height;
        return *this;
    }

    ScrollAreaBuilder& wheel_step(float step) {
        options_.wheel_step = step;
        return *this;
    }

    ScrollAreaBuilder& drag_sensitivity(float value) {
        options_.drag_sensitivity = value;
        return *this;
    }

    ScrollAreaBuilder& inertia_friction(float value) {
        options_.inertia_friction = value;
        return *this;
    }

    ScrollAreaBuilder& bounce_strength(float value) {
        options_.bounce_strength = value;
        return *this;
    }

    ScrollAreaBuilder& bounce_damping(float value) {
        options_.bounce_damping = value;
        return *this;
    }

    ScrollAreaBuilder& overscroll_limit(float value) {
        options_.overscroll_limit = value;
        return *this;
    }

    ScrollAreaBuilder& enable_drag(bool enabled = true) {
        options_.enable_drag = enabled;
        return *this;
    }

    ScrollAreaBuilder& show_scrollbar(bool show = true) {
        options_.show_scrollbar = show;
        return *this;
    }

    ScrollAreaBuilder& draw_background(bool draw = true) {
        options_.draw_background = draw;
        return *this;
    }

    ScrollAreaScope begin() {
        const bool active = detail::ContextAccess::begin_scroll_area(ui_.ctx(), label_, height_, options_);
        ui_.remember(detail::ContextAccess::last_item_rect(ui_.ctx()));
        return active ? ScrollAreaScope(&ui_) : ScrollAreaScope();
    }

    template <typename Fn>
    void begin(Fn&& fn) {
        auto scope = begin();
        if (scope.active()) {
            fn(scope);
        }
    }

private:
    UI& ui_;
    std::string label_{};
    ScrollAreaOptions options_{};
    float height_{120.0f};
};

class MetricBuilder : public PositionedBuilderBase<MetricBuilder> {
public:
    MetricBuilder(UI& ui, std::string_view label, std::string_view value)
        : PositionedBuilderBase<MetricBuilder>(ui),
          label_(label),
          value_(value),
          fill_(mix(ui.theme().panel, ui.theme().secondary, 0.28f)),
          label_color_(ui.theme().muted_text),
          value_color_(ui.theme().text),
          caption_color_(ui.theme().muted_text),
          tag_fill_(mix(ui.theme().primary, ui.theme().panel, 0.24f)),
          tag_color_(ui.theme().primary_text) {
        stroke_.enabled = true;
        stroke_.width = 1.0f;
        stroke_.color = mix(ui.theme().panel_border, ui.theme().primary, 0.06f);
        shadow_.enabled = true;
        shadow_.offset_x = 0.0f;
        shadow_.offset_y = 8.0f;
        shadow_.blur = 16.0f;
        shadow_.spread = 0.0f;
        shadow_.color = rgb(0x020617, 0.10f);
    }

    MetricBuilder& radius(float radius) {
        radius_ = std::max(0.0f, radius);
        return *this;
    }

    MetricBuilder& fill(const Color& color) {
        fill_ = color;
        has_fill_ = true;
        gradient_.enabled = false;
        return *this;
    }

    MetricBuilder& fill(std::uint32_t hex, float alpha = 1.0f) {
        return fill(rgb(hex, alpha));
    }

    MetricBuilder& gradient(const Color& top, const Color& bottom) {
        gradient_.enabled = true;
        gradient_.top = top;
        gradient_.bottom = bottom;
        has_fill_ = false;
        return *this;
    }

    MetricBuilder& gradient(std::uint32_t top, std::uint32_t bottom, float alpha = 1.0f) {
        return gradient(rgb(top, alpha), rgb(bottom, alpha));
    }

    MetricBuilder& stroke(const Color& color, float width = 1.0f) {
        stroke_.enabled = true;
        stroke_.color = color;
        stroke_.width = std::max(0.0f, width);
        return *this;
    }

    MetricBuilder& stroke(std::uint32_t hex, float width = 1.0f, float alpha = 1.0f) {
        return stroke(rgb(hex, alpha), width);
    }

    MetricBuilder& shadow(float offset_x, float offset_y, float blur, const Color& color, float spread = 0.0f) {
        shadow_.enabled = true;
        shadow_.offset_x = offset_x;
        shadow_.offset_y = offset_y;
        shadow_.blur = blur;
        shadow_.spread = spread;
        shadow_.color = color;
        return *this;
    }

    MetricBuilder& shadow(float offset_x, float offset_y, float blur, std::uint32_t hex, float alpha = 0.14f,
                          float spread = 0.0f) {
        return shadow(offset_x, offset_y, blur, rgb(hex, alpha), spread);
    }

    MetricBuilder& label_color(const Color& color) {
        label_color_ = color;
        return *this;
    }

    MetricBuilder& value_color(const Color& color) {
        value_color_ = color;
        return *this;
    }

    MetricBuilder& caption(std::string_view caption) {
        caption_ = std::string(caption);
        return *this;
    }

    MetricBuilder& caption_color(const Color& color) {
        caption_color_ = color;
        return *this;
    }

    MetricBuilder& tag(std::string_view tag) {
        tag_ = std::string(tag);
        return *this;
    }

    MetricBuilder& tag_fill(const Color& color) {
        tag_fill_ = color;
        return *this;
    }

    MetricBuilder& tag_fill(std::uint32_t hex, float alpha = 1.0f) {
        return tag_fill(rgb(hex, alpha));
    }

    MetricBuilder& tag_color(const Color& color) {
        tag_color_ = color;
        return *this;
    }

    MetricBuilder& value_font(float font_size) {
        value_font_ = std::max(10.0f, font_size);
        return *this;
    }

    MetricBuilder& label_font(float font_size) {
        label_font_ = std::max(8.0f, font_size);
        return *this;
    }

    Rect draw() {
        const Rect rect = resolve_rect(220.0f, 92.0f);
        Context& ctx = ui_.ctx();

        paint_shadow(ctx, rect, radius_, shadow_, 1.0f);
        if (gradient_.enabled) {
            paint_gradient(ctx, rect, radius_, gradient_, 1.0f);
        } else if (has_fill_) {
            ctx.paint_filled_rect(rect, fill_, radius_);
        }

        if (stroke_.enabled && stroke_.width > 0.0f) {
            ctx.paint_outline_rect(rect, stroke_.color, radius_, stroke_.width);
        }

        const Rect inner = inset(rect, 14.0f);
        const float label_font = (label_font_ > 0.0f) ? label_font_ : 12.0f;
        const float value_font = (value_font_ > 0.0f) ? value_font_ : std::clamp(rect.h * 0.30f, 16.0f, 28.0f);

        if (!tag_.empty()) {
            const float chip_font = 11.0f;
            const float chip_w = ui_.measure_text(tag_, chip_font) + 20.0f;
            const Rect chip_rect = make_rect(inner.x + inner.w - chip_w, inner.y, chip_w, 22.0f);
            ctx.paint_filled_rect(chip_rect, tag_fill_, 11.0f);
            ctx.paint_text(tag_, chip_rect, tag_color_, chip_font, TextAlign::Center, &rect);
        }

        const Rect label_rect = make_rect(inner.x, inner.y, inner.w, label_font + 4.0f);
        const Rect value_rect = make_rect(inner.x, inner.y + 24.0f, inner.w, value_font + 8.0f);
        ctx.paint_text(label_, label_rect, label_color_, label_font, TextAlign::Left, &rect);
        ctx.paint_text(value_, value_rect, value_color_, value_font, TextAlign::Left, &rect);

        if (!caption_.empty()) {
            const Rect caption_rect = make_rect(inner.x, rect.y + rect.h - 26.0f, inner.w, 16.0f);
            ctx.paint_text(caption_, caption_rect, caption_color_, 11.0f, TextAlign::Left, &rect);
        }

        ui_.remember(rect);
        return rect;
    }

private:
    std::string label_{};
    std::string value_{};
    std::string caption_{};
    std::string tag_{};
    bool has_fill_{true};
    Color fill_{};
    GradientStyle gradient_{};
    StrokeStyle stroke_{};
    ShadowStyle shadow_{};
    float radius_{18.0f};
    float value_font_{0.0f};
    float label_font_{0.0f};
    Color label_color_{};
    Color value_color_{};
    Color caption_color_{};
    Color tag_fill_{};
    Color tag_color_{};
};

class RowBuilder {
public:
    explicit RowBuilder(UI& ui)
        : ui_(ui) {}

    RowBuilder& items(std::initializer_list<FlexLength> items) {
        items_.assign(items.begin(), items.end());
        return *this;
    }

    RowBuilder& items(const std::vector<FlexLength>& items) {
        items_ = items;
        return *this;
    }

    RowBuilder& gap(float gap) {
        gap_ = std::max(0.0f, gap);
        return *this;
    }

    RowBuilder& align(FlexAlign align) {
        align_ = align;
        return *this;
    }

    RowBuilder& align_top() {
        align_ = FlexAlign::Top;
        return *this;
    }

    RowBuilder& align_center() {
        align_ = FlexAlign::Center;
        return *this;
    }

    RowBuilder& align_bottom() {
        align_ = FlexAlign::Bottom;
        return *this;
    }

    FlexRowScope begin() {
        if (items_.empty()) {
            items_.push_back(ui_.flex(1.0f));
        }
        detail::ContextAccess::begin_flex_row(ui_.ctx(), items_, gap_, align_);
        return FlexRowScope(&ui_);
    }

    template <typename Fn>
    void begin(Fn&& fn) {
        auto scope = begin();
        static_cast<void>(scope);
        fn();
    }

private:
    UI& ui_;
    std::vector<FlexLength> items_{};
    float gap_{8.0f};
    FlexAlign align_{FlexAlign::Top};
};

inline RectangleBuilder UI::rectangle() {
    return RectangleBuilder(*this);
}

inline RectangleBuilder UI::rect() {
    return rectangle();
}

inline TextBuilder UI::text(std::string_view text) {
    return TextBuilder(*this, text);
}

inline TextBuilder UI::icon(std::string_view text) {
    TextBuilder builder(*this, text);
    builder.center();
    return builder;
}

inline ShapeBuilder UI::shape() {
    return ShapeBuilder(*this);
}

inline GlyphBuilder UI::glyph(std::uint32_t codepoint) {
    return GlyphBuilder(*this, codepoint);
}

inline ImageBuilder UI::image(std::string_view source) {
    return ImageBuilder(*this, source);
}

inline SurfaceBuilder UI::panel(std::string_view title) {
    return SurfaceBuilder(*this, SurfaceTone::panel, title);
}

inline SurfaceBuilder UI::card(std::string_view title) {
    return SurfaceBuilder(*this, SurfaceTone::card, title);
}

inline LabelBuilder UI::label(std::string_view text) {
    return LabelBuilder(*this, text);
}

inline ButtonBuilder UI::button(std::string_view label) {
    return ButtonBuilder(*this, label);
}

inline TabBuilder UI::tab(std::string_view label, bool selected) {
    return TabBuilder(*this, label, selected);
}

inline SliderFloatBuilder UI::slider(std::string_view label, float& value) {
    return SliderFloatBuilder(*this, label, value);
}

inline InputBuilder UI::input(std::string_view label, std::string& value) {
    return InputBuilder(*this, label, value);
}

inline InputBuilder UI::input(std::string& value) {
    return InputBuilder(*this, "", value);
}

inline FloatInputBuilder UI::input_float(std::string_view label, float& value) {
    return FloatInputBuilder(*this, label, value);
}

inline ReadonlyBuilder UI::readonly(std::string_view label, std::string_view value) {
    return ReadonlyBuilder(*this, label, value);
}

inline ProgressBuilder UI::progress(std::string_view label, float ratio) {
    return ProgressBuilder(*this, label, ratio);
}

inline TextAreaBuilder UI::text_area(std::string_view label, std::string& text) {
    return TextAreaBuilder(*this, label, text);
}

inline TextAreaReadonlyBuilder UI::text_area_readonly(std::string_view label, std::string_view text) {
    return TextAreaReadonlyBuilder(*this, label, text);
}

inline DropdownBuilder UI::dropdown(std::string_view label, bool& open) {
    return DropdownBuilder(*this, label, open);
}

inline ScrollAreaBuilder UI::scroll_area(std::string_view label) {
    return ScrollAreaBuilder(*this, label);
}

inline MetricBuilder UI::metric(std::string_view label, std::string_view value) {
    return MetricBuilder(*this, label, value);
}

inline RowBuilder UI::row() {
    return RowBuilder(*this);
}

inline AnchorBuilder UI::anchor() {
    return AnchorBuilder(*this);
}

inline RegionScope UI::scope(const Rect& rect) {
    const Rect safe = make_rect(rect.x, rect.y, rect.w, rect.h);
    detail::ContextAccess::push_layout_rect(ctx_, safe);
    remember(safe);
    return RegionScope(this, safe);
}

template <typename Fn>
inline void UI::scope(const Rect& rect, Fn&& fn) {
    auto scope_guard = scope(rect);
    static_cast<void>(scope_guard);
    fn(scope_guard);
}

inline RegionScope UI::stack(const Rect& rect) {
    const Rect safe = make_rect(rect.x, rect.y, rect.w, rect.h);
    detail::ContextAccess::begin_stack(ctx_, safe);
    remember(safe);
    return RegionScope(this, safe);
}

template <typename Fn>
inline void UI::stack(const Rect& rect, Fn&& fn) {
    auto stack_guard = stack(rect);
    static_cast<void>(stack_guard);
    fn(stack_guard);
}

inline ClipScope UI::clip(const Rect& rect) {
    const Rect safe = make_rect(rect.x, rect.y, rect.w, rect.h);
    detail::ContextAccess::push_clip(ctx_, safe);
    return ClipScope(&ctx_);
}

template <typename Fn>
inline void UI::clip(const Rect& rect, Fn&& fn) {
    auto clip_guard = clip(rect);
    static_cast<void>(clip_guard);
    fn();
}

}  // namespace eui::quick
