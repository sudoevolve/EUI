#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <string_view>

namespace {

using eui::Rect;
using eui::quick::UI;

void draw_stage_block(UI& ui, const Rect& rect, float scale, std::string_view title, std::string_view rule,
                      std::uint32_t fill, std::uint32_t stroke) {
    const auto dp = [scale](float value) { return value * scale; };

    ui.card()
        .in(rect)
        .radius(dp(18.0f))
        .fill(fill)
        .stroke(stroke, 1.0f)
        .begin([&](auto& card) {
            ui.text(title)
                .in(card.dock_top(dp(20.0f), dp(6.0f)))
                .font(dp(15.0f))
                .color(ui.theme().text)
                .draw();
            ui.text(rule)
                .in(card.content())
                .font(dp(12.0f))
                .color(ui.theme().muted_text)
                .draw();
        });
}

void draw_anchor_stage(UI& ui, const Rect& rect, float scale) {
    const auto dp = [scale](float value) { return value * scale; };

    ui.card("ANCHOR LAYOUT")
        .in(rect)
        .radius(dp(26.0f))
        .gradient(0x0F1A28, 0x132132)
        .stroke(0x29425B, 1.0f)
        .begin([&](auto& stage) {
            const Rect host = stage.content();

            const Rect top_bar =
                ui.anchor().in(host).top(dp(16.0f)).left(dp(16.0f)).right(dp(16.0f)).height(dp(68.0f)).resolve();
            const Rect left_panel =
                ui.anchor().in(host).left(dp(16.0f)).top(dp(100.0f)).bottom(dp(92.0f)).width(dp(188.0f)).resolve();
            const Rect right_panel =
                ui.anchor().in(host).right(dp(16.0f)).top(dp(100.0f)).bottom(dp(92.0f)).width(dp(188.0f)).resolve();
            const Rect center_card =
                ui.anchor().in(host).center_x(0.0f).center_y(0.0f).size(dp(150.0f), dp(90.0f)).resolve();
            const Rect bottom_bar =
                ui.anchor().in(host).left(dp(220.0f)).right(dp(220.0f)).bottom(dp(16.0f)).height(dp(60.0f)).resolve();

            draw_stage_block(ui, top_bar, scale, "Top Bar", "top + left + right + height", 0x18314A, 0x315978);
            draw_stage_block(ui, left_panel, scale, "Left Panel", "left + top + bottom + width", 0x17303D, 0x2D586A);
            draw_stage_block(ui, right_panel, scale, "Right Panel", "right + top + bottom + width", 0x17303D, 0x2D586A);
            draw_stage_block(ui, center_card, scale, "Center Card", "center_x + center_y + size", 0x162432, 0x30485D);
            draw_stage_block(ui, bottom_bar, scale, "Bottom Bar", "bottom + left + right + height", 0x2A1D33, 0x66466E);
        });
}

void draw_layout_stage(UI& ui, const Rect& rect, float scale) {
    const auto dp = [scale](float value) { return value * scale; };

    ui.card("LAYOUT")
        .in(rect)
        .radius(dp(26.0f))
        .gradient(0x101A28, 0x132032)
        .stroke(0x29425B, 1.0f)
        .begin([&](auto& stage) {
            const Rect host = stage.content();

            ui.scope(host, [&](auto& scope) {
                const Rect header = scope.dock_top(dp(68.0f), dp(12.0f));
                const Rect sidebar = scope.dock_left(dp(150.0f), dp(12.0f));
                const Rect footer = scope.dock_bottom(dp(60.0f), dp(12.0f));
                const auto split = ui.split_h_ratio(scope.content(), 0.62f, dp(12.0f));

                draw_stage_block(ui, header, scale, "Header", "dock_top()", 0x18314A, 0x315978);
                draw_stage_block(ui, sidebar, scale, "Sidebar", "dock_left()", 0x17303D, 0x2D586A);
                draw_stage_block(ui, footer, scale, "Footer", "dock_bottom()", 0x2A1D33, 0x66466E);
                draw_stage_block(ui, split.first, scale, "Main", "remaining content()", 0x162432, 0x30485D);
                draw_stage_block(ui, split.second, scale, "Inspector", "split_h_ratio()", 0x182B22, 0x35614C);
            });
        });
}

}  // namespace

int main() {
    eui::app::AppOptions options{};
    options.title = "EUI Anchor + Layout Demo";
    options.width = 1360;
    options.height = 720;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 120.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();

            const float scale = std::max(1.0f, metrics.dpi_scale);
            const auto dp = [scale](float value) { return value * scale; };

            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(eui::rgba(0.30f, 0.74f, 0.77f, 1.0f));
            ctx.set_corner_radius(dp(12.0f));

            UI ui(ctx);
            const float margin = dp(18.0f);
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel()
                .in(frame_rect)
                .padding(dp(16.0f))
                .radius(dp(28.0f))
                .gradient(0x09121B, 0x0C1620)
                .stroke(0x1D2F42, 1.0f)
                .begin([&](auto& root) {
                    const auto halves = root.split_h_ratio(root.content(), 0.50f, dp(14.0f));
                    draw_anchor_stage(ui, halves.first, scale);
                    draw_layout_stage(ui, halves.second, scale);
                });
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
