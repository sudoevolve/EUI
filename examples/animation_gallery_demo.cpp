#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace {

using eui::Rect;
using eui::quick::UI;

struct AnimationGalleryState {
    int selected_demo{0};
};

struct DemoDescriptor {
    const char* title;
    const char* summary;
    const char* api_label;
};

struct Point2 {
    float x{0.0f};
    float y{0.0f};
};

static constexpr std::array<DemoDescriptor, 6> kDemos{{
    {"Translate", "Pure 2D translation on one axis", "translate() + TimelineClip"},
    {"Scale", "Uniform grow / shrink around the center", "scale() + ease_in_out"},
    {"Rotate", "Continuous angle animation around origin", "rotate() + linear loop"},
    {"Arc Motion", "Move on a curved arc, not a straight line", "curve path + ease_out"},
    {"Bezier Path", "Follow a cubic Bezier trajectory", "Bezier path + custom cubic"},
    {"Transform Mix", "Translation + scale + rotation together", "translate + scale + rotate"},
}};

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float lerp(float from, float to, float t) {
    return from + (to - from) * clamp01(t);
}

Rect inset_rect(const Rect& rect, float padding_x, float padding_y) {
    return Rect{
        rect.x + padding_x,
        rect.y + padding_y,
        std::max(0.0f, rect.w - padding_x * 2.0f),
        std::max(0.0f, rect.h - padding_y * 2.0f),
    };
}

Rect inset_rect(const Rect& rect, float padding) {
    return inset_rect(rect, padding, padding);
}

std::uint32_t mix_hex(std::uint32_t lhs, std::uint32_t rhs, float t) {
    const float clamped = clamp01(t);
    const auto mix_channel = [&](int shift) {
        const float a = static_cast<float>((lhs >> shift) & 0xffu);
        const float b = static_cast<float>((rhs >> shift) & 0xffu);
        return static_cast<std::uint32_t>(std::lround(a + (b - a) * clamped));
    };
    return (mix_channel(16) << 16) | (mix_channel(8) << 8) | mix_channel(0);
}

bool hovered(const eui::InputState& input, const Rect& rect) {
    return rect.contains(input.mouse_x, input.mouse_y);
}

bool clicked(const eui::InputState& input, const Rect& rect) {
    return hovered(input, rect) && input.mouse_pressed;
}

void draw_fill(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).fill(hex, alpha).draw();
}

void draw_stroke(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float width = 1.0f, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).no_fill().stroke(hex, width, alpha).draw();
}

void draw_text_left(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex,
                    float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).align(eui::TextAlign::Left).draw();
}

void draw_text_center(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex,
                      float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).align(eui::TextAlign::Center).draw();
}

float loop_progress(double time_seconds, float duration_seconds) {
    const float safe_duration = std::max(1e-4f, duration_seconds);
    float phase = std::fmod(static_cast<float>(time_seconds), safe_duration);
    if (phase < 0.0f) {
        phase += safe_duration;
    }
    return phase / safe_duration;
}

float timeline_ping_pong(UI& ui, double time_seconds, float duration_seconds, eui::animation::EasingPreset preset) {
    eui::animation::TimelineClip clip{};
    clip.scalar.from = 0.0f;
    clip.scalar.to = 1.0f;
    clip.duration_seconds = std::max(1e-4f, duration_seconds);
    clip.easing = eui::animation::preset_bezier(preset);

    const float loop = loop_progress(time_seconds, clip.duration_seconds * 2.0f) * clip.duration_seconds * 2.0f;
    if (loop <= clip.duration_seconds) {
        return ui.animate(clip, loop);
    }
    return 1.0f - ui.animate(clip, loop - clip.duration_seconds);
}

float timeline_ping_pong(UI& ui, double time_seconds, float duration_seconds, const eui::animation::CubicBezier& bezier) {
    eui::animation::TimelineClip clip{};
    clip.scalar.from = 0.0f;
    clip.scalar.to = 1.0f;
    clip.duration_seconds = std::max(1e-4f, duration_seconds);
    clip.easing = bezier;

    const float loop = loop_progress(time_seconds, clip.duration_seconds * 2.0f) * clip.duration_seconds * 2.0f;
    if (loop <= clip.duration_seconds) {
        return ui.animate(clip, loop);
    }
    return 1.0f - ui.animate(clip, loop - clip.duration_seconds);
}

Point2 cubic_bezier_point(const Point2& p0, const Point2& p1, const Point2& p2, const Point2& p3, float t) {
    t = clamp01(t);
    return Point2{
        eui::animation::cubic_bezier_component(p0.x, p1.x, p2.x, p3.x, t),
        eui::animation::cubic_bezier_component(p0.y, p1.y, p2.y, p3.y, t),
    };
}

void draw_stage_background(UI& ui, const Rect& rect, float scale) {
    draw_fill(ui, rect, 0x09131F, 22.0f * scale, 1.0f);
    draw_stroke(ui, rect, 0x22344A, 22.0f * scale, 1.0f, 0.92f);

    const float gap = 34.0f * scale;
    for (float x = rect.x + gap; x < rect.x + rect.w; x += gap) {
        ui.shape().in(Rect{x, rect.y, 1.0f, rect.h}).fill(0x203247, 0.46f).draw();
    }
    for (float y = rect.y + gap; y < rect.y + rect.h; y += gap) {
        ui.shape().in(Rect{rect.x, y, rect.w, 1.0f}).fill(0x203247, 0.46f).draw();
    }
}

void draw_actor(UI& ui, const Rect& rect, float scale) {
    ui.shape()
        .in(rect)
        .radius(18.0f * scale)
        .gradient(0x60A5FA, 0x2563EB)
        .stroke(0xDBECFF, 1.0f, 0.92f)
        .shadow(0.0f, 8.0f * scale, 20.0f * scale, 0x020617, 0.12f)
        .draw();
    ui.shape()
        .in(inset_rect(rect, 10.0f * scale))
        .radius(10.0f * scale)
        .fill(0xEFF6FF, 0.18f)
        .draw();
}

void draw_translate_demo(UI& ui, const Rect& rect, float scale, double time_seconds) {
    draw_stage_background(ui, rect, scale);

    const Rect track{rect.x + 48.0f * scale, rect.y + rect.h * 0.5f - 3.0f * scale, rect.w - 96.0f * scale, 6.0f * scale};
    draw_fill(ui, track, 0x203247, 3.0f * scale, 0.86f);

    const Rect start_dot{track.x - 8.0f * scale, track.y - 8.0f * scale, 16.0f * scale, 16.0f * scale};
    const Rect end_dot{track.x + track.w - 8.0f * scale, track.y - 8.0f * scale, 16.0f * scale, 16.0f * scale};
    draw_fill(ui, start_dot, 0x7DD3FC, 8.0f * scale, 1.0f);
    draw_fill(ui, end_dot, 0x38BDF8, 8.0f * scale, 1.0f);

    const float travel = track.w - 72.0f * scale;
    const float t = timeline_ping_pong(ui, time_seconds, 1.35f, eui::animation::EasingPreset::ease_in_out);
    draw_actor(ui, Rect{track.x + travel * t, rect.y + rect.h * 0.5f - 36.0f * scale, 72.0f * scale, 72.0f * scale}, scale);
}

void draw_scale_demo(UI& ui, const Rect& rect, float scale, double time_seconds) {
    draw_stage_background(ui, rect, scale);

    const float t = timeline_ping_pong(ui, time_seconds, 1.55f, eui::animation::EasingPreset::ease_in_out);
    const float actor_scale = lerp(0.62f, 1.28f, t);
    const Rect actor{rect.x + rect.w * 0.5f - 72.0f * scale, rect.y + rect.h * 0.5f - 72.0f * scale, 144.0f * scale, 144.0f * scale};

    ui.shape()
        .in(actor)
        .radius(30.0f * scale)
        .gradient(0x60A5FA, 0x2563EB)
        .stroke(0xDBECFF, 1.0f, 0.92f)
        .shadow(0.0f, 12.0f * scale, 24.0f * scale, 0x020617, 0.14f)
        .scale(actor_scale)
        .origin_center()
        .draw();

    ui.shape()
        .in(Rect{actor.x + 28.0f * scale, actor.y + 28.0f * scale, actor.w - 56.0f * scale, actor.h - 56.0f * scale})
        .radius(18.0f * scale)
        .fill(0xEFF6FF, 0.18f)
        .scale(actor_scale)
        .origin_center()
        .draw();
}

void draw_rotate_demo(UI& ui, const Rect& rect, float scale, double time_seconds) {
    draw_stage_background(ui, rect, scale);

    const float t = loop_progress(time_seconds, 2.40f);
    const float angle = 360.0f * t;
    const Rect dial{rect.x + rect.w * 0.5f - 98.0f * scale, rect.y + rect.h * 0.5f - 98.0f * scale, 196.0f * scale, 196.0f * scale};
    const Rect hand{dial.x + dial.w * 0.5f - 10.0f * scale, dial.y + 26.0f * scale, 20.0f * scale, dial.h - 52.0f * scale};
    const Rect hand_tip{dial.x + dial.w * 0.5f - 18.0f * scale, dial.y + 18.0f * scale, 36.0f * scale, 36.0f * scale};
    const Rect hub{dial.x + dial.w * 0.5f - 18.0f * scale, dial.y + dial.h * 0.5f - 18.0f * scale, 36.0f * scale, 36.0f * scale};

    draw_stroke(ui, dial, 0x33506F, dial.w * 0.5f, 2.0f * scale, 0.84f);
    draw_fill(ui, inset_rect(dial, 10.0f * scale), 0x0D1726, (dial.w - 20.0f * scale) * 0.5f, 0.96f);

    ui.shape()
        .in(hand)
        .radius(hand.w * 0.5f)
        .gradient(0x93C5FD, 0x2563EB)
        .stroke(0xEFF6FF, 1.0f, 0.92f)
        .shadow(0.0f, 8.0f * scale, 18.0f * scale, 0x020617, 0.14f)
        .rotate(angle)
        .origin_center()
        .draw();
    ui.shape()
        .in(hand_tip)
        .radius(hand_tip.w * 0.5f)
        .fill(0xF8FBFF, 0.96f)
        .rotate(angle)
        .origin_center()
        .draw();
    draw_fill(ui, hub, 0xF8FBFF, hub.w * 0.5f, 0.98f);
    draw_fill(ui, inset_rect(hub, 8.0f * scale), 0x2563EB, (hub.w - 16.0f * scale) * 0.5f, 0.96f);
}

void draw_arc_motion_demo(UI& ui, const Rect& rect, float scale, double time_seconds) {
    draw_stage_background(ui, rect, scale);

    const float t = timeline_ping_pong(ui, time_seconds, 1.75f, eui::animation::EasingPreset::ease_out);
    const float left = rect.x + 52.0f * scale;
    const float right = rect.x + rect.w - 52.0f * scale;
    const float base_y = rect.y + rect.h * 0.70f;
    const float height = rect.h * 0.34f;

    for (int i = 0; i <= 30; ++i) {
        const float sample = static_cast<float>(i) / 30.0f;
        const float x = lerp(left, right, sample);
        const float y = base_y - std::sin(sample * 3.14159265f) * height;
        draw_fill(ui, Rect{x - 3.0f * scale, y - 3.0f * scale, 6.0f * scale, 6.0f * scale}, 0x335C85, 3.0f * scale, 0.70f);
    }

    const float x = lerp(left, right, t);
    const float y = base_y - std::sin(t * 3.14159265f) * height;
    draw_actor(ui, Rect{x - 36.0f * scale, y - 36.0f * scale, 72.0f * scale, 72.0f * scale}, scale);
}

void draw_bezier_demo(UI& ui, const Rect& rect, float scale, double time_seconds) {
    draw_stage_background(ui, rect, scale);

    const Point2 p0{rect.x + 58.0f * scale, rect.y + rect.h - 74.0f * scale};
    const Point2 p1{rect.x + rect.w * 0.30f, rect.y + 38.0f * scale};
    const Point2 p2{rect.x + rect.w * 0.68f, rect.y + rect.h - 42.0f * scale};
    const Point2 p3{rect.x + rect.w - 58.0f * scale, rect.y + 74.0f * scale};

    for (int i = 0; i <= 40; ++i) {
        const float sample = static_cast<float>(i) / 40.0f;
        const Point2 point = cubic_bezier_point(p0, p1, p2, p3, sample);
        draw_fill(ui, Rect{point.x - 3.0f * scale, point.y - 3.0f * scale, 6.0f * scale, 6.0f * scale},
                  0x3F5F86, 3.0f * scale, sample < 0.5f ? 0.72f : 0.90f);
    }

    draw_fill(ui, Rect{p0.x - 6.0f * scale, p0.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, 0x7DD3FC, 6.0f * scale, 1.0f);
    draw_fill(ui, Rect{p1.x - 6.0f * scale, p1.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, 0x334155, 6.0f * scale, 0.92f);
    draw_fill(ui, Rect{p2.x - 6.0f * scale, p2.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, 0x334155, 6.0f * scale, 0.92f);
    draw_fill(ui, Rect{p3.x - 6.0f * scale, p3.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, 0x38BDF8, 6.0f * scale, 1.0f);

    const eui::animation::CubicBezier velocity{0.20f, 0.0f, 0.10f, 1.0f};
    const float t = timeline_ping_pong(ui, time_seconds, 2.10f, velocity);
    const Point2 actor = cubic_bezier_point(p0, p1, p2, p3, t);
    draw_actor(ui, Rect{actor.x - 34.0f * scale, actor.y - 34.0f * scale, 68.0f * scale, 68.0f * scale}, scale);
}

void draw_transform_mix_demo(UI& ui, const Rect& rect, float scale, double time_seconds) {
    draw_stage_background(ui, rect, scale);

    const float translate_t = timeline_ping_pong(ui, time_seconds, 1.65f, eui::animation::EasingPreset::ease_in_out);
    const float scale_t = timeline_ping_pong(ui, time_seconds + 0.28, 1.65f, eui::animation::EasingPreset::spring_soft);
    const float rotate_t = loop_progress(time_seconds, 2.60f);

    const float dx = lerp(-rect.w * 0.18f, rect.w * 0.18f, translate_t);
    const float actor_scale = lerp(0.72f, 1.18f, scale_t);
    const float angle = lerp(-28.0f, 332.0f, rotate_t);
    const Rect actor{rect.x + rect.w * 0.5f - 78.0f * scale, rect.y + rect.h * 0.5f - 78.0f * scale, 156.0f * scale, 156.0f * scale};

    ui.shape()
        .in(actor)
        .radius(32.0f * scale)
        .gradient(0x60A5FA, 0x2563EB)
        .stroke(0xDBECFF, 1.0f, 0.92f)
        .shadow(0.0f, 12.0f * scale, 26.0f * scale, 0x020617, 0.15f)
        .translate(dx, 0.0f)
        .scale(actor_scale)
        .rotate(angle)
        .origin_center()
        .draw();

    ui.shape()
        .in(Rect{actor.x + 24.0f * scale, actor.y + 24.0f * scale, actor.w - 48.0f * scale, actor.h - 48.0f * scale})
        .radius(22.0f * scale)
        .fill(0xEFF6FF, 0.18f)
        .translate(dx, 0.0f)
        .scale(actor_scale)
        .rotate(angle)
        .origin_center()
        .draw();
}

void draw_demo_scene(UI& ui, int demo_index, const Rect& rect, float scale, double time_seconds) {
    switch (demo_index) {
        case 0:
            draw_translate_demo(ui, rect, scale, time_seconds);
            break;
        case 1:
            draw_scale_demo(ui, rect, scale, time_seconds);
            break;
        case 2:
            draw_rotate_demo(ui, rect, scale, time_seconds);
            break;
        case 3:
            draw_arc_motion_demo(ui, rect, scale, time_seconds);
            break;
        case 4:
            draw_bezier_demo(ui, rect, scale, time_seconds);
            break;
        case 5:
        default:
            draw_transform_mix_demo(ui, rect, scale, time_seconds);
            break;
    }
}

void draw_sidebar(UI& ui, const eui::InputState& input, AnimationGalleryState& state, const Rect& rect, float scale) {
    draw_fill(ui, rect, 0x07111C, 26.0f * scale, 1.0f);
    draw_stroke(ui, rect, 0x20324A, 26.0f * scale, 1.0f, 1.0f);

    const Rect inner = inset_rect(rect, 18.0f * scale);
    draw_text_left(ui, "Animation Lab", Rect{inner.x, inner.y, inner.w, 22.0f * scale}, 18.0f * scale, 0xF5FAFF, 1.0f);
    draw_text_left(ui, "Click a basic motion on the left. The right panel isolates that animation only.",
                   Rect{inner.x, inner.y + 26.0f * scale, inner.w, 30.0f * scale}, 10.8f * scale, 0x8EA0BA, 0.96f);

    const float row_h = 72.0f * scale;
    const float list_y = inner.y + 74.0f * scale;
    const float indicator_y_target = list_y + static_cast<float>(state.selected_demo) * (row_h + 8.0f * scale);
    const std::uint64_t indicator_id = ui.id("animation-demo/sidebar-indicator");
    if (ui.motion_value(indicator_id, -1.0f) < 0.0f) {
        ui.reset_motion(indicator_id, indicator_y_target);
    }
    const float indicator_y = ui.motion(indicator_id, indicator_y_target, 18.0f);
    draw_fill(ui, Rect{inner.x, indicator_y, inner.w, row_h}, 0x0E1A2A, 18.0f * scale, 0.96f);
    draw_stroke(ui, Rect{inner.x, indicator_y, inner.w, row_h}, 0x4C8FFF, 18.0f * scale, 1.0f, 0.72f);
    draw_fill(ui, Rect{inner.x, indicator_y, 4.0f * scale, row_h}, 0x60A5FA, 2.0f * scale, 0.98f);

    for (std::size_t i = 0; i < kDemos.size(); ++i) {
        const Rect row{
            inner.x,
            list_y + static_cast<float>(i) * (row_h + 8.0f * scale),
            inner.w,
            row_h,
        };
        const bool is_hovered = hovered(input, row);
        const bool is_selected = state.selected_demo == static_cast<int>(i);
        const float hover_mix =
            ui.presence(ui.id("animation-demo/sidebar-hover", static_cast<std::uint64_t>(i + 1u)), is_hovered, 18.0f, 12.0f);

        if (!is_selected && is_hovered) {
            draw_fill(ui, row, 0x0D1725, 18.0f * scale, 0.92f * hover_mix);
            draw_stroke(ui, row, 0x273852, 18.0f * scale, 1.0f, 0.88f * hover_mix);
        }
        if (clicked(input, row)) {
            state.selected_demo = static_cast<int>(i);
        }

        draw_text_left(ui, kDemos[i].title,
                       Rect{row.x + 18.0f * scale, row.y + 14.0f * scale, row.w - 36.0f * scale, 18.0f * scale},
                       13.0f * scale, is_selected ? 0xF5FAFF : 0xD3E6FF, 0.98f);
        draw_text_left(ui, kDemos[i].summary,
                       Rect{row.x + 18.0f * scale, row.y + 36.0f * scale, row.w - 36.0f * scale, 16.0f * scale},
                       10.4f * scale, is_selected ? 0x9FC7FF : 0x8EA0BA, 0.94f);
    }
}

void draw_stage(UI& ui, int demo_index, const Rect& rect, float scale, double time_seconds) {
    const DemoDescriptor& demo = kDemos[static_cast<std::size_t>(std::clamp(demo_index, 0, static_cast<int>(kDemos.size() - 1)))];

    draw_fill(ui, rect, 0x07111C, 26.0f * scale, 1.0f);
    draw_stroke(ui, rect, 0x20324A, 26.0f * scale, 1.0f, 1.0f);

    const Rect inner = inset_rect(rect, 22.0f * scale);
    const Rect title_rect{inner.x, inner.y, inner.w, 24.0f * scale};
    const Rect summary_rect{inner.x, inner.y + 26.0f * scale, inner.w, 18.0f * scale};
    const Rect chip_rect{inner.x, inner.y + 56.0f * scale, 224.0f * scale, 30.0f * scale};
    const Rect stage_rect{inner.x, inner.y + 102.0f * scale, inner.w, inner.h - 102.0f * scale};

    draw_text_left(ui, demo.title, title_rect, 22.0f * scale, 0xF5FAFF, 1.0f);
    draw_text_left(ui, demo.summary, summary_rect, 11.0f * scale, 0x8EA0BA, 0.96f);
    draw_fill(ui, chip_rect, 0x0F1B2A, chip_rect.h * 0.5f, 0.96f);
    draw_stroke(ui, chip_rect, 0x26364E, chip_rect.h * 0.5f, 1.0f, 0.90f);
    draw_text_center(ui, demo.api_label, chip_rect, 11.0f * scale, 0xB9D7FF, 0.98f);

    for (std::size_t i = 0; i < kDemos.size(); ++i) {
        const float mix =
            ui.presence(ui.id("animation-demo/view", static_cast<std::uint64_t>(i + 1u)), demo_index == static_cast<int>(i), 11.0f, 13.0f);
        if (mix <= 0.01f) {
            continue;
        }
        const float alpha = mix * mix * (3.0f - 2.0f * mix);
        ui.ctx().set_global_alpha(alpha);
        ui.clip(stage_rect, [&] {
            const Rect scene_rect{stage_rect.x + (1.0f - alpha) * 18.0f * scale, stage_rect.y, stage_rect.w, stage_rect.h};
            draw_demo_scene(ui, static_cast<int>(i), scene_rect, scale, time_seconds);
        });
        ui.ctx().set_global_alpha(1.0f);
    }
}

}  // namespace

int main() {
    AnimationGalleryState state{};

    eui::app::AppOptions options{};
    options.title = "EUI Animation Gallery";
    options.width = 1420;
    options.height = 860;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 144.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const auto& input = ctx.input_state();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const double time_seconds = input.time_seconds;

            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(eui::rgba(0.36f, 0.67f, 0.98f, 1.0f));
            ctx.set_corner_radius(14.0f * scale);

            UI ui(ctx);
            const float margin = 18.0f * scale;
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel("Animation gallery")
                .in(frame_rect)
                .padding(18.0f * scale)
                .radius(30.0f * scale)
                .gradient(0x060D17, 0x0C1625)
                .stroke(0x1F3147, 1.0f)
                .shadow(0.0f, 14.0f * scale, 28.0f * scale, 0x020617, 0.18f)
                .begin([&](auto& root) {
                    const Rect header = root.dock_top(68.0f * scale, 16.0f * scale);
                    draw_fill(ui, header, 0x08111D, 22.0f * scale, 0.96f);
                    draw_stroke(ui, header, 0x20324A, 22.0f * scale, 1.0f, 1.0f);
                    draw_text_left(ui, "Animation Capability Showcase",
                                   Rect{header.x + 22.0f * scale, header.y + 14.0f * scale, header.w - 44.0f * scale, 20.0f * scale},
                                   18.0f * scale, 0xF5FAFF, 1.0f);
                    draw_text_left(ui, "Left list switches a single base animation. Right stage isolates that one capability only.",
                                   Rect{header.x + 22.0f * scale, header.y + 36.0f * scale, header.w - 44.0f * scale, 16.0f * scale},
                                   11.0f * scale, 0x90A4BE, 0.96f);

                    const auto split = root.split_h_ratio(root.content(), 0.26f, 18.0f * scale);
                    draw_sidebar(ui, input, state, split.first, scale);
                    draw_stage(ui, state.selected_demo, split.second, scale, time_seconds);
                });

            frame.request_next_frame();
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
