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

struct DemoState {
    bool expanded{true};
    bool glow{true};
    float gap{18.0f};
    float radius{24.0f};
    float blur{16.0f};
    float accent_mix{0.38f};
    float card_alpha{0.92f};
};

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
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

eui::Color color_from_hex(std::uint32_t hex, float alpha = 1.0f) {
    const float r = static_cast<float>((hex >> 16) & 0xffu) / 255.0f;
    const float g = static_cast<float>((hex >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>(hex & 0xffu) / 255.0f;
    return eui::rgba(r, g, b, alpha);
}

Rect inset_rect(const Rect& rect, float padding) {
    return Rect{
        rect.x + padding,
        rect.y + padding,
        std::max(0.0f, rect.w - padding * 2.0f),
        std::max(0.0f, rect.h - padding * 2.0f),
    };
}

float font_title(float scale) {
    return 22.0f * scale;
}

float font_heading(float scale) {
    return 16.0f * scale;
}

float font_body(float scale) {
    return 14.0f * scale;
}

float font_meta(float scale) {
    return 12.2f * scale;
}

float slide(UI& ui, std::string_view key, float target, float speed = 12.0f) {
    return ui.motion(key, target, speed);
}

float show(UI& ui, std::string_view key, bool visible, float enter_speed = 12.0f, float exit_speed = 16.0f) {
    return ui.presence(key, visible, enter_speed, exit_speed);
}

void draw_chip(UI& ui, const Rect& rect, std::string_view text, std::uint32_t fill, std::uint32_t stroke,
               std::uint32_t text_hex, float scale) {
    ui.shape().in(rect).radius(rect.h * 0.5f).fill(fill, 0.96f).stroke(stroke, 1.0f, 0.88f).draw();
    ui.text(text).in(rect).font(font_meta(scale)).center().color(text_hex, 0.98f).draw();
}

void draw_controls(UI& ui, DemoState& state, const Rect& rect, float scale) {
    ui.card("Minimal API")
        .in(rect)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(0x151B2B)
        .stroke(0x2A3245, 1.0f)
        .begin([&](auto& card) {
            ui.column(card.content())
                .tracks({
                    eui::quick::px(36.0f * scale),
                    eui::quick::px(40.0f * scale),
                    eui::quick::px(40.0f * scale),
                    eui::quick::px(40.0f * scale),
                    eui::quick::px(40.0f * scale),
                    eui::quick::px(40.0f * scale),
                    eui::quick::px(36.0f * scale),
                    eui::quick::fr(),
                })
                .gap(10.0f * scale)
                .begin([&](auto& rows) {
                    ui.row(rows.next())
                        .tracks({eui::quick::fr(), eui::quick::fr()})
                        .gap(10.0f * scale)
                        .begin([&](auto& buttons) {
                            auto expanded_button = ui.button(state.expanded ? "Collapse Card" : "Expand Card")
                                                       .in(buttons.next())
                                                       .height(36.0f * scale);
                            if (state.expanded) {
                                expanded_button.primary();
                            } else {
                                expanded_button.secondary();
                            }
                            if (expanded_button.draw()) {
                                state.expanded = !state.expanded;
                            }

                            auto glow_button = ui.button(state.glow ? "Glow On" : "Glow Off")
                                                   .in(buttons.next())
                                                   .height(36.0f * scale);
                            if (state.glow) {
                                glow_button.primary();
                            } else {
                                glow_button.ghost();
                            }
                            if (glow_button.draw()) {
                                state.glow = !state.glow;
                            }
                        });

                    ui.slider("Layout Gap", state.gap).in(rows.next()).range(10.0f, 32.0f).decimals(0).height(40.0f * scale).draw();
                    ui.slider("Corner Radius", state.radius).in(rows.next()).range(12.0f, 32.0f).decimals(0).height(40.0f * scale).draw();
                    ui.slider("Glow Blur", state.blur).in(rows.next()).range(0.0f, 24.0f).decimals(0).height(40.0f * scale).draw();
                    ui.slider("Accent Mix", state.accent_mix).in(rows.next()).range(0.0f, 1.0f).decimals(2).height(40.0f * scale).draw();
                    ui.slider("Card Alpha", state.card_alpha).in(rows.next()).range(0.30f, 1.0f).decimals(2).height(40.0f * scale).draw();
                    ui.readonly("Helpers", "slide(ui, key, target) + show(ui, key, visible)")
                        .in(rows.next())
                        .height(36.0f * scale)
                        .draw();

                    ui.text_area_readonly(
                            "Layout",
                            "view(rect).column()\n"
                            "row(body).tracks({ px(320), fr() })\n"
                            "zstack(stage).layers(4)\n"
                            "shape().in(rect).radius(...).blur(...)\n"
                            "button().in(slot) / slider().in(slot)")
                        .in(rows.next())
                        .height(180.0f * scale)
                        .draw();
                });
        });
}

void draw_preview(UI& ui, DemoState& state, const Rect& rect, float scale, double time_seconds) {
    const std::uint32_t accent = mix_hex(0x3B82F6, 0x8B5CF6, state.accent_mix);
    const std::uint32_t accent_soft = mix_hex(0x111827, accent, 0.28f);
    const float reveal = show(ui, "minimal/show-card", state.expanded);
    const float card_y = slide(ui, "minimal/card-y", state.expanded ? -12.0f * scale : 26.0f * scale, 10.0f);
    const float card_rotation = slide(ui, "minimal/card-rotation", state.expanded ? 5.0f : -6.0f, 9.0f);
    const float card_scale = slide(ui, "minimal/card-scale", state.expanded ? 1.0f : 0.92f, 10.0f);
    const float glow = slide(ui, "minimal/glow", state.glow ? state.blur * scale : 0.0f, 10.0f);
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(time_seconds * 1.8));
    const float orbit_x = static_cast<float>(std::cos(time_seconds * 1.4)) * 72.0f * scale * reveal;
    const float orbit_y = static_cast<float>(std::sin(time_seconds * 1.1)) * 40.0f * scale * reveal;
    const float card_alpha = clamp01(state.card_alpha);
    const float content_alpha = 0.62f + 0.36f * card_alpha;

    ui.card("Preview")
        .in(rect)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(0x151B2B)
        .stroke(0x2A3245, 1.0f)
        .begin([&](auto& card) {
            ui.column(card.content())
                .tracks({eui::quick::px(34.0f * scale), eui::quick::fr()})
                .gap(16.0f * scale)
                .begin([&](auto& rows) {
                    const Rect chip_row = rows.next();
                    const Rect stage = rows.next();

                    ui.row(chip_row)
                        .tracks({eui::quick::fr(), eui::quick::fr(), eui::quick::fr()})
                        .gap(10.0f * scale)
                        .begin([&](auto& chips) {
                            draw_chip(ui, chips.next(), "column()", 0x182033, accent, 0xE8F0FF, scale);
                            draw_chip(ui, chips.next(), "row()", 0x182033, accent, 0xE8F0FF, scale);
                            draw_chip(ui, chips.next(), "zstack()", 0x182033, accent, 0xE8F0FF, scale);
                        });

                    ui.zstack(stage)
                        .layers(4)
                        .begin([&](auto& layers) {
                            const Rect background = layers.next();
                            layers.next();
                            layers.next();
                            layers.next();

                            ui.shape()
                                .in(background)
                                .radius(state.radius * scale)
                                .gradient(0x0F172A, 0x111827, 1.0f)
                                .stroke(0x2C3650, 1.0f, 0.92f)
                                .draw();

                            const Rect halo{
                                background.x + background.w * 0.72f - 70.0f * scale + orbit_x,
                                background.y + background.h * 0.24f - 70.0f * scale + orbit_y,
                                140.0f * scale,
                                140.0f * scale,
                            };
                            ui.shape()
                                .in(halo)
                                .radius(999.0f)
                                .fill(accent, 0.16f + 0.08f * pulse)
                                .blur(glow * 0.7f, glow)
                                .draw();

                            const Rect glass_card{
                                background.x + background.w * 0.5f - 168.0f * scale,
                                background.y + background.h * 0.5f - 96.0f * scale + card_y,
                                336.0f * scale,
                                192.0f * scale,
                            };
                            ui.shape()
                                .in(glass_card)
                                .radius(state.radius * scale * 0.85f)
                                .gradient(accent_soft, mix_hex(0x1F2937, accent, 0.42f), card_alpha)
                                .stroke(0xFFFFFF, 1.0f, (0.08f + 0.12f * reveal) * card_alpha)
                                .shadow(0.0f, 18.0f * scale, 30.0f * scale, 0x000000, 0.22f * card_alpha, 6.0f * scale)
                                .blur(glow * 0.20f, glow)
                                .scale(card_scale)
                                .rotate(card_rotation)
                                .origin_center()
                                .draw();

                            const Rect title{
                                glass_card.x + 28.0f * scale,
                                glass_card.y + 28.0f * scale,
                                glass_card.w - 56.0f * scale,
                                24.0f * scale,
                            };
                            const Rect meta{
                                glass_card.x + 28.0f * scale,
                                glass_card.y + 56.0f * scale,
                                glass_card.w - 56.0f * scale,
                                18.0f * scale,
                            };
                            ui.text("Simple declarative layout")
                                .in(title)
                                .font(font_title(scale))
                                .center()
                                .color(0xF8FAFC, content_alpha)
                                .draw();
                            ui.text("shape().translate().scale().rotate().blur()")
                                .in(meta)
                                .font(font_meta(scale))
                                .center()
                                .color(0xCBD5E1, content_alpha * 0.94f)
                                .draw();

                            const Rect progress_track{
                                glass_card.x + 34.0f * scale,
                                glass_card.y + 98.0f * scale,
                                glass_card.w - 68.0f * scale,
                                16.0f * scale,
                            };
                            ui.shape().in(progress_track).radius(8.0f * scale).fill(0xFFFFFF, 0.10f * content_alpha).draw();
                            ui.shape()
                                .in(Rect{
                                    progress_track.x + 2.0f * scale,
                                    progress_track.y + 2.0f * scale,
                                    std::max(12.0f * scale, (progress_track.w - 4.0f * scale) * (0.30f + 0.46f * pulse)),
                                    progress_track.h - 4.0f * scale,
                                })
                                .radius(6.0f * scale)
                                .fill(accent, content_alpha)
                                .draw();

                            ui.row(Rect{
                                       glass_card.x + 28.0f * scale,
                                       glass_card.y + glass_card.h - 54.0f * scale,
                                       glass_card.w - 56.0f * scale,
                                       28.0f * scale,
                                   })
                                .tracks({eui::quick::fr(), eui::quick::fr(), eui::quick::fr()})
                                .gap(10.0f * scale)
                                .begin([&](auto& stat_chips) {
                                    draw_chip(ui, stat_chips.next(), "radius", 0x182033, accent, 0xE8F0FF, scale);
                                    draw_chip(ui, stat_chips.next(), "blur", 0x182033, accent, 0xE8F0FF, scale);
                                    draw_chip(ui, stat_chips.next(), "motion", 0x182033, accent, 0xE8F0FF, scale);
                                });
                        });
                });
        });
}

}  // namespace

int main() {
    DemoState state{};

    eui::app::AppOptions options{};
    options.title = "Minimal Quick Demo";
    options.width = 1180;
    options.height = 760;
    options.vsync = true;
    options.continuous_render = true;
    options.max_fps = 240.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const double time_seconds = ctx.input_state().time_seconds;

            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(color_from_hex(mix_hex(0x3B82F6, 0x8B5CF6, state.accent_mix)));

            UI ui(ctx);
            const float margin = 18.0f * scale;
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel("Minimal Quick Demo")
                .in(frame_rect)
                .title_font(font_heading(scale))
                .radius(26.0f * scale)
                .fill(0x0B1020)
                .gradient(0x111827, 0x0B1020)
                .stroke(0x293145, 1.0f)
                .shadow(0.0f, 14.0f * scale, 28.0f * scale, 0x000000, 0.22f)
                .begin([&](auto& root) {
                    ui.view(root.content())
                        .column()
                        .tracks({eui::quick::px(52.0f * scale), eui::quick::fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& rows) {
                            const Rect header = rows.next();
                            const Rect body = rows.next();

                            ui.text("Minimal Syntax: Layout + Shapes + Motion")
                                .in(Rect{header.x, header.y + 4.0f * scale, header.w, 24.0f * scale})
                                .font(font_title(scale))
                                .left()
                                .color(0xF8FAFC, 1.0f)
                                .draw();
                            ui.text("Left side uses slot-based layout. Right side shows shape/text drawing and slide()/show() helpers.")
                                .in(Rect{header.x, header.y + 28.0f * scale, header.w, 18.0f * scale})
                                .font(font_meta(scale))
                                .left()
                                .color(0x9FB0CC, 0.96f)
                                .draw();

                            ui.row(body)
                                .tracks({eui::quick::px(320.0f * scale), eui::quick::fr()})
                                .gap(state.gap * scale)
                                .begin([&](auto& cols) {
                                    draw_controls(ui, state, cols.next(), scale);
                                    draw_preview(ui, state, cols.next(), scale, time_seconds);
                                });
                        });
                });
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
