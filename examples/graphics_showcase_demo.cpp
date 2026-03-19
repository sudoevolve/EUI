#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_SDL2
#include "EUI.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace {

using eui::Rect;
using eui::quick::UI;

struct GraphicsDemoState {
    float blur_radius{24.0f};
    float blur_strength{0.90f};
    float blur_frost{0.08f};
    float blur_cell{38.0f};

    float shadow_offset_x{10.0f};
    float shadow_offset_y{14.0f};
    float shadow_blur{28.0f};
    float shadow_opacity{0.30f};
    float shadow_spread{8.0f};

    float tilt_depth{16.0f};
    float tilt_limit_x{18.0f};
    float tilt_limit_y{14.0f};
    float tilt_smooth{12.0f};
    float tilt_perspective{240.0f};
};

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

Rect inset_rect(const Rect& rect, float padding) {
    return Rect{
        rect.x + padding,
        rect.y + padding,
        std::max(0.0f, rect.w - padding * 2.0f),
        std::max(0.0f, rect.h - padding * 2.0f),
    };
}

void draw_grid_pattern(UI& ui, const Rect& clip_rect, float cell, float alpha, float shift_x = 0.0f, float shift_y = 0.0f) {
    static constexpr std::uint32_t colors[6] = {0xEF4444, 0xF59E0B, 0x10B981, 0x3B82F6, 0x8B5CF6, 0xEC4899};
    const float safe_cell = std::max(18.0f, cell);
    const float gap = std::max(3.0f, safe_cell * 0.14f);
    const float radius = std::max(5.0f, safe_cell * 0.22f);
    const float tile_size = std::max(8.0f, safe_cell - gap);
    const int rows = std::max(1, static_cast<int>(std::ceil((clip_rect.h + safe_cell * 2.0f) / safe_cell)));
    const int cols = std::max(1, static_cast<int>(std::ceil((clip_rect.w + safe_cell * 2.0f) / safe_cell)));
    const float start_x = clip_rect.x - safe_cell + shift_x;
    const float start_y = clip_rect.y - safe_cell + shift_y;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const Rect cell_rect{
                start_x + static_cast<float>(col) * safe_cell,
                start_y + static_cast<float>(row) * safe_cell,
                tile_size,
                tile_size,
            };
            const float cell_alpha = std::clamp(alpha * (((row + col) % 2 == 0) ? 0.96f : 0.68f), 0.0f, 1.0f);
            ui.shape()
                .in(cell_rect)
                .radius(radius)
                .fill(colors[(row + col) % 6], cell_alpha)
                .draw();
        }
    }
}

void draw_grid_lines(UI& ui, const Rect& clip_rect, float step, std::uint32_t hex, float alpha, float thickness = 1.0f) {
    const float safe_step = std::max(12.0f, step);
    const float line = std::max(1.0f, thickness);
    const int cols = std::max(1, static_cast<int>(std::ceil(clip_rect.w / safe_step)) + 1);
    const int rows = std::max(1, static_cast<int>(std::ceil(clip_rect.h / safe_step)) + 1);
    for (int col = 0; col <= cols; ++col) {
        const float x = clip_rect.x + static_cast<float>(col) * safe_step;
        ui.shape()
            .in(Rect{x, clip_rect.y, line, clip_rect.h})
            .fill(hex, alpha)
            .draw();
    }
    for (int row = 0; row <= rows; ++row) {
        const float y = clip_rect.y + static_cast<float>(row) * safe_step;
        ui.shape()
            .in(Rect{clip_rect.x, y, clip_rect.w, line})
            .fill(hex, alpha)
            .draw();
    }
}

void draw_soft_shadow_layers(UI& ui, const Rect& rect, float radius, float offset_x, float offset_y, float blur,
                             float opacity, float spread, float scale) {
    const float dx = offset_x * scale;
    const float dy = offset_y * scale;
    const float blur_px = blur * scale;
    const float spread_px = spread * scale;
    const int layers = std::clamp(static_cast<int>(blur * 0.38f) + 8, 8, 18);

    for (int i = layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(layers);
        const float grow = spread_px + blur_px * (0.08f + t * 0.82f);
        const float alpha = std::clamp(opacity * (0.58f / static_cast<float>(layers)) * (1.08f - 0.46f * t), 0.0f, 1.0f);
        ui.shape()
            .in(Rect{
                rect.x + dx - grow,
                rect.y + dy - grow,
                rect.w + grow * 2.0f,
                rect.h + grow * 2.0f,
            })
            .radius(radius + grow * 0.52f)
            .fill(0x020617, alpha)
            .draw();
    }
}

template <typename Fn>
void draw_slider_group(UI& ui, const Rect& rect, Fn&& draw_controls) {
    ui.scope(rect, [&](auto&) {
        draw_controls();
    });
}

void draw_blur_demo(UI& ui, GraphicsDemoState& state, const Rect& rect, float scale) {
    const auto dp = [scale](float value) { return value * scale; };
    const float cell = dp(state.blur_cell);

    ui.card("Blur")
        .in(rect)
        .radius(dp(28.0f))
        .fill(0x07101A)
        .stroke(0x203248, 1.0f)
        .begin([&](auto& card) {
            const Rect stage = card.dock_top(dp(260.0f), dp(16.0f));
            const Rect controls = card.content();
            const Rect left = Rect{stage.x, stage.y, (stage.w - dp(12.0f)) * 0.5f, stage.h};
            const Rect right = Rect{left.x + left.w + dp(12.0f), stage.y, stage.w - left.w - dp(12.0f), stage.h};
            const Rect left_inner = inset_rect(left, dp(12.0f));
            const Rect right_inner = inset_rect(right, dp(12.0f));
            const Rect left_clip = inset_rect(left_inner, dp(4.0f));
            const Rect right_clip = inset_rect(right_inner, dp(4.0f));

            ui.shape()
                .in(left)
                .radius(dp(24.0f))
                .fill(0x0A1320)
                .stroke(0x31455F, 1.0f, 0.44f)
                .draw();

            ui.shape()
                .in(left_inner)
                .radius(dp(22.0f))
                .fill(0x0B1320)
                .draw();

            ui.clip(left_clip, [&] {
                draw_grid_pattern(ui, left_clip, cell, 1.0f);
                draw_grid_lines(ui, left_clip, cell, 0xFFFFFF, 0.08f, dp(1.0f));
            });

            ui.shape()
                .in(left_inner)
                .radius(dp(22.0f))
                .no_fill()
                .stroke(0xFFFFFF, 1.0f, 0.82f)
                .draw();

            ui.shape()
                .in(right)
                .radius(dp(22.0f))
                .fill(0x0A1320)
                .stroke(0x31455F, 1.0f, 0.44f)
                .draw();

            ui.shape()
                .in(right_inner)
                .radius(dp(22.0f))
                .fill(0x0B1320)
                .draw();

            ui.clip(right_clip, [&] {
                draw_grid_pattern(ui, right_clip, cell, 0.92f);
                draw_grid_lines(ui, right_clip, cell, 0xFFFFFF, 0.08f, dp(1.0f));
            });

            const float effective_blur = dp(std::max(0.0f, state.blur_radius) * clamp01(state.blur_strength));
            ui.shape()
                .in(right_inner)
                .radius(dp(22.0f))
                .blur(effective_blur, effective_blur)
                .fill(0xFFFFFF, state.blur_frost)
                .stroke(0xFFFFFF, 1.0f, 0.86f)
                .draw();

            draw_slider_group(ui, controls, [&] {
                ui.slider("Blur Radius", state.blur_radius).range(0.0f, 42.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Blur Strength", state.blur_strength).range(0.0f, 1.0f).decimals(2).height(dp(34.0f)).draw();
                ui.slider("Blur Frost", state.blur_frost).range(0.0f, 0.20f).decimals(2).height(dp(34.0f)).draw();
                ui.slider("Grid Cell", state.blur_cell).range(24.0f, 52.0f).decimals(0).height(dp(34.0f)).draw();
            });
        });
}

void draw_shadow_demo(UI& ui, GraphicsDemoState& state, const Rect& rect, float scale) {
    const auto dp = [scale](float value) { return value * scale; };

    ui.card("Shadow")
        .in(rect)
        .radius(dp(28.0f))
        .fill(0x07101A)
        .stroke(0x203248, 1.0f)
        .begin([&](auto& card) {
            const Rect stage = card.dock_top(dp(260.0f), dp(16.0f));
            const Rect controls = card.content();
            const Rect flat{
                stage.x + dp(28.0f),
                stage.y + dp(92.0f),
                dp(118.0f),
                dp(84.0f),
            };
            const Rect plate{
                stage.x + stage.w - dp(28.0f) - dp(118.0f),
                stage.y + dp(92.0f),
                dp(118.0f),
                dp(84.0f),
            };

            ui.shape()
                .in(stage)
                .radius(dp(24.0f))
                .fill(0xE3E8EF)
                .draw();

            ui.shape()
                .in(flat)
                .radius(dp(22.0f))
                .fill(0xFFFFFF, 0.98f)
                .stroke(0xCBD5E1, 1.0f)
                .draw();

            draw_soft_shadow_layers(ui, plate, dp(24.0f), state.shadow_offset_x, state.shadow_offset_y, state.shadow_blur,
                                    state.shadow_opacity, state.shadow_spread, scale);

            ui.shape()
                .in(plate)
                .radius(dp(24.0f))
                .fill(0xFFFFFF, 0.98f)
                .stroke(0xCBD5E1, 1.0f)
                .draw();

            draw_slider_group(ui, controls, [&] {
                ui.slider("Shadow Offset X", state.shadow_offset_x).range(0.0f, 24.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Shadow Offset Y", state.shadow_offset_y).range(0.0f, 28.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Shadow Blur", state.shadow_blur).range(0.0f, 40.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Shadow Opacity", state.shadow_opacity).range(0.0f, 0.60f).decimals(2).height(dp(34.0f)).draw();
                ui.slider("Shadow Spread", state.shadow_spread).range(0.0f, 18.0f).decimals(0).height(dp(34.0f)).draw();
            });
        });
}

void draw_transform_demo(UI& ui, GraphicsDemoState& state, const Rect& rect, float scale) {
    const auto dp = [scale](float value) { return value * scale; };
    const auto& input = ui.ctx().input_state();

    ui.card("3D Transform")
        .in(rect)
        .radius(dp(28.0f))
        .fill(0x07101A)
        .stroke(0x203248, 1.0f)
        .begin([&](auto& card) {
            const Rect stage = card.dock_top(dp(260.0f), dp(16.0f));
            const Rect controls = card.content();
            const Rect interactive = ui.anchor().in(stage).center_x(0.0f).center_y(0.0f).size(dp(256.0f), dp(160.0f)).resolve();
            const float display_x_limit = state.tilt_limit_x;
            const float display_y_limit = state.tilt_limit_y;

            const bool hovered = interactive.contains(input.mouse_x, input.mouse_y);
            const float local_u =
                hovered ? clamp01((input.mouse_x - interactive.x) / std::max(1.0f, interactive.w)) : 0.5f;
            const float local_v =
                hovered ? clamp01((input.mouse_y - interactive.y) / std::max(1.0f, interactive.h)) : 0.5f;
            const float target_tilt_x = hovered ? (local_u - 0.5f) * display_x_limit * 2.0f : 0.0f;
            const float target_tilt_y = hovered ? (0.5f - local_v) * display_y_limit * 2.0f : 0.0f;
            const float tilt_x =
                ui.motion("graphics-demo/tilt-x", target_tilt_x, hovered ? state.tilt_smooth : state.tilt_smooth * 0.7f);
            const float tilt_y =
                ui.motion("graphics-demo/tilt-y", target_tilt_y, hovered ? state.tilt_smooth : state.tilt_smooth * 0.7f);
            const float card_rotate_x = -tilt_y;
            const float card_rotate_y = -tilt_x;

            ui.shape()
                .in(stage)
                .radius(dp(24.0f))
                .fill(0x0A1320)
                .stroke(0x31455F, 1.0f, 0.34f)
                .draw();

            ui.shape()
                .in(interactive)
                .radius(dp(24.0f))
                .gradient(0x334155, 0x0F172A)
                .stroke(0x93C5FD, 1.2f, 0.56f)
                .translate_3d(0.0f, 0.0f, dp(state.tilt_depth))
                .rotate_x(card_rotate_x)
                .rotate_y(card_rotate_y)
                .perspective(dp(state.tilt_perspective))
                .origin_center()
                .draw();

            const Rect grid_clip = inset_rect(interactive, dp(18.0f));
            const float plane_origin_x = interactive.w * 0.5f;
            const float plane_origin_y = interactive.h * 0.5f;
            const int cols = 5;
            const int rows = 3;
            const float line_w = dp(1.5f);
            for (int i = 1; i < cols; ++i) {
                const float x = grid_clip.x + grid_clip.w * static_cast<float>(i) / static_cast<float>(cols);
                const Rect line_rect{x - line_w * 0.5f, grid_clip.y, line_w, grid_clip.h};
                ui.shape()
                    .in(line_rect)
                    .fill(0xBFDBFE, 0.22f)
                    .translate_3d(0.0f, 0.0f, dp(state.tilt_depth))
                    .rotate_x(card_rotate_x)
                    .rotate_y(card_rotate_y)
                    .perspective(dp(state.tilt_perspective))
                    .origin_3d(plane_origin_x - (line_rect.x - interactive.x),
                               plane_origin_y - (line_rect.y - interactive.y),
                               0.0f)
                    .draw();
            }
            for (int i = 1; i < rows; ++i) {
                const float y = grid_clip.y + grid_clip.h * static_cast<float>(i) / static_cast<float>(rows);
                const Rect line_rect{grid_clip.x, y - line_w * 0.5f, grid_clip.w, line_w};
                ui.shape()
                    .in(line_rect)
                    .fill(0xBFDBFE, 0.22f)
                    .translate_3d(0.0f, 0.0f, dp(state.tilt_depth))
                    .rotate_x(card_rotate_x)
                    .rotate_y(card_rotate_y)
                    .perspective(dp(state.tilt_perspective))
                    .origin_3d(plane_origin_x - (line_rect.x - interactive.x),
                               plane_origin_y - (line_rect.y - interactive.y),
                               0.0f)
                    .draw();
            }

            draw_slider_group(ui, controls, [&] {
                ui.slider("Tilt Depth", state.tilt_depth).range(0.0f, 28.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Tilt X Range", state.tilt_limit_x).range(4.0f, 26.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Tilt Y Range", state.tilt_limit_y).range(4.0f, 22.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Tilt Smooth", state.tilt_smooth).range(4.0f, 24.0f).decimals(0).height(dp(34.0f)).draw();
                ui.slider("Perspective", state.tilt_perspective).range(140.0f, 360.0f).decimals(0).height(dp(34.0f)).draw();
            });
        });
}

}  // namespace

int main() {
    GraphicsDemoState state{};

    eui::app::AppOptions options{};
    options.title = "EUI Graphics Showcase";
    options.width = 1320;
    options.height = 760;
    options.vsync = true;
    options.continuous_render = true;
    options.max_fps = 240.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";
    options.enable_icon_font_fallback = true;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const auto dp = [scale](float value) { return value * scale; };

            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(eui::rgba(0.28f, 0.72f, 0.98f, 1.0f));
            ctx.set_corner_radius(dp(14.0f));

            UI ui(ctx);
            const float margin = dp(20.0f);
            const Rect frame_rect{
                margin,
                margin,
                std::max(dp(980.0f), static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(dp(620.0f), static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel("Graphics showcase")
                .in(frame_rect)
                .padding(dp(18.0f))
                .radius(dp(32.0f))
                .gradient(0x060D17, 0x0B1524)
                .stroke(0x21364C, 1.0f)
                .shadow(0.0f, dp(18.0f), dp(34.0f), 0x020617, 0.20f, 0.0f)
                .begin([&](auto& root) {
                    const Rect header = root.dock_top(dp(66.0f), dp(16.0f));
                    ui.card()
                        .in(header)
                        .radius(dp(22.0f))
                        .gradient(0x07101A, 0x102032)
                        .stroke(0x22374D, 1.0f)
                        .begin([&](auto& card) {
                            const Rect icon_rect = ui.anchor().in(card.content()).left(0.0f).center_y(0.0f).size(dp(28.0f), dp(28.0f)).resolve();
                            ui.glyph(0xF53Fu).in(icon_rect).tint(0x38BDF8).draw();
                            Rect text_rect = card.content();
                            text_rect.x += dp(42.0f);
                            text_rect.w = std::max(0.0f, text_rect.w - dp(42.0f));
                            ui.text("Blur / shadow / tilt")
                                .in(text_rect)
                                .font(dp(16.0f))
                                .color(0xE6F2FF)
                                .draw();
                        });

                    const auto first = root.split_h_ratio(root.content(), 0.333f, dp(16.0f));
                    const auto second = root.split_h_ratio(first.second, 0.50f, dp(16.0f));

                    draw_blur_demo(ui, state, first.first, scale);
                    draw_shadow_demo(ui, state, second.first, scale);
                    draw_transform_demo(ui, state, second.second, scale);
                });
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
