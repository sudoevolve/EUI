#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

using eui::Rect;
using eui::quick::UI;
using eui::quick::fr;
using eui::quick::px;

constexpr float kPi = 3.14159265358979323846f;

enum class OpenMenu : int {
    None = 0,
    QuickActions = 1,
    TrendRange = 2,
    ActivityRange = 3,
};

struct Palette {
    std::uint32_t background{};
    std::uint32_t shell{};
    std::uint32_t shell_alt{};
    std::uint32_t panel{};
    std::uint32_t panel_alt{};
    std::uint32_t panel_tint{};
    std::uint32_t border{};
    std::uint32_t text{};
    std::uint32_t muted{};
    std::uint32_t soft{};
    std::uint32_t accent{};
    std::uint32_t accent_warm{};
    std::uint32_t accent_soft{};
    std::uint32_t accent_cool{};
    std::uint32_t success{};
    std::uint32_t warning{};
    std::uint32_t shadow{};
};

struct QuickAction {
    std::uint32_t icon;
    const char* label;
    const char* caption;
};

struct TaskCard {
    const char* title;
    const char* subtitle;
    const char* amount;
    float progress;
    std::uint32_t accent;
};

struct DayValue {
    const char* label;
    float value;
};

struct StockPoint {
    float value;
};

struct DashboardState {
    int utility_selection{1};
    int balance_mode{0};
    int assistant_chip{0};
    int active_quick_action{0};
    int trend_range{1};
    int activity_range{1};
    int selected_task{1};
    int selected_rating{4};
    int hovered_trend{-1};
    int hovered_stock{-1};
    bool mic_active{false};
    OpenMenu open_menu{OpenMenu::None};
    std::string search_text{"Budget insights"};
    std::string activity_search{"Find tasks, contacts, reports"};
    Rect quick_action_button_rect{};
    Rect trend_dropdown_rect{};
    Rect activity_dropdown_rect{};
    Rect overlay_block_rect{};
    bool overlay_block_active{false};
};

static constexpr std::array<std::uint32_t, 3> kUtilityIcons{{0xF03Au, 0xF201u, 0xF0E8u}};
static constexpr std::array<const char*, 3> kAssistantChips{{"Transfers", "Budgeting", "Analytics"}};
static constexpr std::array<const char*, 3> kTrendRanges{{"2024", "2025", "2026"}};
static constexpr std::array<const char*, 3> kActivityRanges{{"Today", "This week", "This month"}};
static constexpr std::array<QuickAction, 3> kQuickActions{{
    {0xF067u, "New card", "Create a fresh finance card"},
    {0xF0D6u, "Pay vendors", "Open payout workflow"},
    {0xF201u, "Forecast", "Generate next month forecast"},
}};
static constexpr std::array<TaskCard, 3> kTasks{{
    {"Payroll review", "Needs final approval", "$18,240", 0.74f, 0xF29A54},
    {"Treasury sync", "Liquidity model refreshed", "$42,880", 0.92f, 0x6E9EFF},
    {"Board packet", "Quarterly deck in progress", "$6,430", 0.58f, 0x71C5A7},
}};
static constexpr std::array<DayValue, 7> kTrendValues{{
    {"Mon", 18.0f},
    {"Tue", 36.0f},
    {"Wed", 28.0f},
    {"Thu", 52.0f},
    {"Fri", 44.0f},
    {"Sat", 69.0f},
    {"Sun", 58.0f},
}};
static constexpr std::array<StockPoint, 9> kStockValues{{
    {26.0f}, {28.0f}, {31.0f}, {35.0f}, {33.0f}, {38.0f}, {42.0f}, {40.0f}, {47.0f},
}};

Palette make_palette() {
    return Palette{
        0xEEF1F6,
        0xF9FAFD,
        0xF2F5FB,
        0xFFFFFF,
        0xF5F7FC,
        0xFFF6EF,
        0xDDE4EF,
        0x1C2333,
        0x7D8798,
        0xA4AFC0,
        0xF29A54,
        0xF3B26A,
        0xFDEAD7,
        0x7FA6FF,
        0x72C5A1,
        0xF0B94E,
        0x0F172A,
    };
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
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

std::uint32_t lighten_hex(std::uint32_t hex, float amount) {
    return mix_hex(hex, 0xFFFFFF, amount);
}

Rect inset_rect(const Rect& rect, float padding) {
    return Rect{
        rect.x + padding,
        rect.y + padding,
        std::max(0.0f, rect.w - padding * 2.0f),
        std::max(0.0f, rect.h - padding * 2.0f),
    };
}

float font_display(float scale) {
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

float font_hero(float scale) {
    return 33.0f * scale;
}

bool pointer_captured_by_overlay(const DashboardState& state, const eui::InputState& input) {
    return state.overlay_block_active && state.overlay_block_rect.contains(input.mouse_x, input.mouse_y);
}

void set_overlay_block(DashboardState& state, const Rect& rect) {
    state.overlay_block_rect = rect;
    state.overlay_block_active = rect.w > 0.0f && rect.h > 0.0f;
}

bool hovered(const eui::InputState& input, const Rect& rect) {
    return rect.contains(input.mouse_x, input.mouse_y);
}

bool clicked(const eui::InputState& input, const Rect& rect) {
    return input.mouse_pressed && hovered(input, rect);
}

bool interactive_hovered(const DashboardState& state, const eui::InputState& input, const Rect& rect) {
    return !pointer_captured_by_overlay(state, input) && hovered(input, rect);
}

bool interactive_clicked(const DashboardState& state, const eui::InputState& input, const Rect& rect) {
    return input.mouse_pressed && interactive_hovered(state, input, rect);
}

void draw_fill(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).fill(hex, alpha).draw();
}

void draw_stroke(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float width = 1.0f, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).no_fill().stroke(hex, width, alpha).draw();
}

void draw_text_left(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex, float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).draw();
}

void draw_text_center(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex, float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).center().draw();
}

void draw_icon(UI& ui, std::uint32_t codepoint, const Rect& rect, std::uint32_t hex, float alpha = 1.0f) {
    ui.glyph(codepoint).in(rect).tint(hex, alpha).draw();
}

void draw_soft_glow(UI& ui, const Rect& rect, std::uint32_t inner, std::uint32_t outer, float radius = 0.84f, float alpha = 1.0f) {
    ui.shape().in(rect).radius(std::max(rect.w, rect.h) * 0.5f).radial_gradient(inner, outer, radius, alpha).draw();
}

void paint_panel(UI& ui, const Rect& rect, float radius, float scale, const Palette& palette,
                 float hover_mix = 0.0f, float alpha = 1.0f, bool glass = false) {
    const std::uint32_t top = mix_hex(palette.panel, glass ? palette.panel_tint : 0xFFFFFF, hover_mix * 0.18f);
    const std::uint32_t bottom = mix_hex(palette.panel_alt, glass ? palette.accent_soft : 0xFFFFFF, hover_mix * 0.08f);
    auto shape = ui.shape()
                     .in(rect)
                     .radius(radius)
                     .gradient(top, bottom, alpha)
                     .stroke(mix_hex(palette.border, palette.accent_soft, hover_mix * 0.24f), 1.0f, alpha)
                     .shadow(0.0f, 16.0f * scale, 32.0f * scale, palette.shadow, (0.09f + hover_mix * 0.04f) * alpha);
    if (glass) {
        shape.blur(16.0f * scale, 24.0f * scale);
    }
    shape.draw();
}

Rect lifted_rect(UI& ui, std::uint64_t id, const Rect& rect, bool hovered_state, float scale) {
    const float lift = smoothstep01(ui.presence(id, hovered_state, 16.0f, 18.0f)) * 6.0f * scale;
    return Rect{rect.x, rect.y - lift, rect.w, rect.h};
}

template <std::size_t N>
void draw_dropdown_control(UI& ui, const eui::InputState& input, DashboardState& state,
                           const Palette& palette, Rect button, OpenMenu menu_id, int& selected_index,
                           const std::array<const char*, N>& labels, float scale) {
    if (menu_id == OpenMenu::TrendRange) {
        state.trend_dropdown_rect = button;
    } else if (menu_id == OpenMenu::ActivityRange) {
        state.activity_dropdown_rect = button;
    }
    const bool button_hovered = interactive_hovered(state, input, button);
    const std::uint64_t menu_motion_id = ui.id("financial/dropdown", static_cast<std::uint64_t>(menu_id));
    if (interactive_clicked(state, input, button)) {
        if (state.open_menu != menu_id) {
            ui.reset_motion(menu_motion_id, 0.0f);
        }
        state.open_menu = (state.open_menu == menu_id) ? OpenMenu::None : menu_id;
    }

    ui.presence(menu_motion_id, state.open_menu == menu_id, 18.0f, 16.0f);
    const float radius = button.h * 0.5f;
    ui.shape()
        .in(button)
        .radius(radius)
        .fill(0xF5F7FC, button_hovered ? 1.0f : 0.96f)
        .stroke(button_hovered ? palette.accent : palette.border, 1.0f, button_hovered ? 0.40f : 1.0f)
        .draw();
    draw_text_left(ui, labels[static_cast<std::size_t>(selected_index)],
                   Rect{button.x + 14.0f * scale, button.y + 10.0f * scale, button.w - 40.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.text, 0.96f);
    draw_icon(ui, 0xF078u,
              Rect{button.x + button.w - 22.0f * scale, button.y + (button.h - 12.0f * scale) * 0.5f, 12.0f * scale, 12.0f * scale},
              palette.text, 0.82f);
}

void draw_avatar(UI& ui, const Rect& rect, float scale) {
    draw_fill(ui, rect, 0xFFFFFF, rect.w * 0.5f, 1.0f);
    const Rect inner = inset_rect(rect, 2.0f * scale);
    ui.shape().in(inner).radius(inner.w * 0.5f).gradient(0xFFDAB7, 0xF5A6AC).draw();
    draw_fill(ui, Rect{inner.x + inner.w * 0.18f, inner.y + inner.h * 0.58f, inner.w * 0.64f, inner.h * 0.34f}, 0x7D95C0, inner.h * 0.18f, 0.98f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.44f, inner.y + inner.h * 0.47f, inner.w * 0.12f, inner.h * 0.10f}, 0xF5CCB1, inner.w * 0.06f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.30f, inner.y + inner.h * 0.20f, inner.w * 0.40f, inner.h * 0.40f}, 0xF5CCB1, inner.w * 0.20f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.20f, inner.y + inner.h * 0.08f, inner.w * 0.60f, inner.h * 0.32f}, 0x7A4F46, inner.w * 0.28f, 0.98f);
}

void draw_search_field(UI& ui, const Rect& rect, float scale, std::string& value) {
    ui.scope(rect, [&](auto&) {
        ui.input("", value).height(rect.h).padding_left(24.0f * scale).placeholder("Search...").draw();
    });
    draw_icon(ui, 0xF002u,
              Rect{rect.x + 10.0f * scale, rect.y + (rect.h - 13.0f * scale) * 0.5f, 13.0f * scale, 13.0f * scale},
              0x8691A3, 0.85f);
}

void draw_header(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                 float scale, const Palette& palette) {
    ui.row(rect)
        .tracks({px(280.0f * scale), fr(), px(388.0f * scale)})
        .gap(18.0f * scale)
        .begin([&](auto& cols) {
            const Rect left = cols.next();
            cols.next();
            const Rect right = cols.next();

            const Rect menu_rect{left.x, left.y + 10.0f * scale, 38.0f * scale, 38.0f * scale};
            const bool menu_hovered = interactive_hovered(state, input, menu_rect);
            draw_fill(ui, menu_rect, 0xFFFFFF, menu_rect.w * 0.5f, menu_hovered ? 1.0f : 0.92f);
            draw_stroke(ui, menu_rect, palette.border, menu_rect.w * 0.5f, 1.0f, 1.0f);
            draw_icon(ui, 0xF0C9u,
                      Rect{menu_rect.x + 11.0f * scale, menu_rect.y + 11.0f * scale, 16.0f * scale, 16.0f * scale},
                      palette.text, 0.92f);

            const Rect logo_rect{left.x + 56.0f * scale, left.y + 10.0f * scale, 38.0f * scale, 38.0f * scale};
            ui.shape()
                .in(logo_rect)
                .radius(12.0f * scale)
                .gradient(palette.accent, palette.accent_warm, 1.0f)
                .shadow(0.0f, 8.0f * scale, 18.0f * scale, palette.accent, 0.18f)
                .draw();
            draw_fill(ui, Rect{logo_rect.x + 10.0f * scale, logo_rect.y + 10.0f * scale, 18.0f * scale, 18.0f * scale}, 0xFFFFFF, 5.0f * scale, 0.92f);

            draw_text_left(ui, "Financial Dashboard",
                           Rect{left.x + 108.0f * scale, left.y + 10.0f * scale, left.w - 108.0f * scale, 24.0f * scale},
                           font_display(scale), palette.text, 0.98f);
            draw_text_left(ui, "AI planning workspace",
                           Rect{left.x + 108.0f * scale, left.y + 36.0f * scale, left.w - 108.0f * scale, 16.0f * scale},
                           font_meta(scale), palette.muted, 0.96f);

            ui.row(right)
                .tracks({fr(), px(42.0f * scale), px(136.0f * scale)})
                .gap(12.0f * scale)
                .begin([&](auto& right_cols) {
                    const Rect search_slot = right_cols.next();
                    const Rect plus_slot = right_cols.next();
                    const Rect profile_slot = right_cols.next();

                    const Rect search{search_slot.x, search_slot.y + 12.0f * scale, search_slot.w, 38.0f * scale};
                    const Rect plus{plus_slot.x + 1.0f * scale, plus_slot.y + 12.0f * scale, 40.0f * scale, 40.0f * scale};
                    const Rect profile{profile_slot.x, profile_slot.y + 6.0f * scale, profile_slot.w, 52.0f * scale};

                    draw_search_field(ui, search, scale, state.search_text);

                    const bool plus_hovered = interactive_hovered(state, input, plus);
                    state.quick_action_button_rect = plus;
                    if (interactive_clicked(state, input, plus)) {
                        state.open_menu = (state.open_menu == OpenMenu::QuickActions) ? OpenMenu::None : OpenMenu::QuickActions;
                    }
                    const float plus_mix = smoothstep01(ui.presence("financial/quickactions/button", plus_hovered || state.open_menu == OpenMenu::QuickActions, 18.0f, 16.0f));
                    ui.shape()
                        .in(plus)
                        .radius(14.0f * scale)
                        .gradient(palette.accent, palette.accent_warm, 1.0f)
                        .shadow(0.0f, 10.0f * scale, 20.0f * scale, palette.accent, 0.22f + plus_mix * 0.08f)
                        .draw();
                    draw_icon(ui, 0xF067u,
                              Rect{plus.x + 11.0f * scale, plus.y + 11.0f * scale, 18.0f * scale, 18.0f * scale},
                              0xFFFFFF, 1.0f);

                    const Rect avatar_rect{profile.x, profile.y + 3.0f * scale, 48.0f * scale, 48.0f * scale};
                    draw_avatar(ui, avatar_rect, scale);
                    draw_text_left(ui, "Mina Brooks",
                                   Rect{profile.x + 60.0f * scale, profile.y + 8.0f * scale, profile.w - 60.0f * scale, 18.0f * scale},
                                   font_body(scale), palette.text, 0.98f);
                    draw_text_left(ui, "Operations",
                                   Rect{profile.x + 60.0f * scale, profile.y + 30.0f * scale, profile.w - 60.0f * scale, 16.0f * scale},
                                   font_meta(scale), palette.muted, 0.94f);
                });
        });
}

void draw_chip_button(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                      int index, const Rect& rect, float scale) {
    const bool selected = state.assistant_chip == index;
    if (interactive_clicked(state, input, rect)) {
        state.assistant_chip = index;
    }
    ui.shape()
        .in(rect)
        .radius(rect.h * 0.5f)
        .fill(selected ? palette.accent_soft : 0xFFFFFF, selected ? 1.0f : 0.82f)
        .stroke(selected ? palette.accent : palette.border, 1.0f, 1.0f)
        .shadow(0.0f, 8.0f * scale, 18.0f * scale, palette.shadow, 0.04f)
        .draw();
    draw_text_center(ui, kAssistantChips[static_cast<std::size_t>(index)], rect, font_meta(scale),
                     selected ? palette.accent : palette.text, 0.98f);
}

void draw_hero(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
               float scale, const Palette& palette, double time_seconds) {
    paint_panel(ui, rect, 32.0f * scale, scale, palette, 0.0f, 1.0f, false);
    ui.clip(rect, [&] {
        draw_soft_glow(ui, Rect{rect.x + rect.w * 0.54f, rect.y - 40.0f * scale, 320.0f * scale, 220.0f * scale},
                       lighten_hex(palette.accent, 0.2f), 0xFFFFFF, 0.82f, 0.12f);
        draw_soft_glow(ui, Rect{rect.x + rect.w * 0.76f, rect.y + rect.h * 0.16f, 190.0f * scale, 190.0f * scale},
                       palette.accent_cool, 0xFFFFFF, 0.82f, 0.10f);
    });

    const Rect inner = inset_rect(rect, 28.0f * scale);
    ui.row(inner)
        .tracks({fr(), px(226.0f * scale)})
        .gap(18.0f * scale)
        .begin([&](auto& cols) {
            const Rect left = cols.next();
            const Rect right = cols.next();

            ui.column(left)
                .tracks({px(34.0f * scale), px(92.0f * scale), px(34.0f * scale)})
                .gap(16.0f * scale)
                .begin([&](auto& rows) {
                    const Rect top_row = rows.next();
                    const Rect title_row = rows.next();
                    const Rect chips = rows.next();

                    ui.row(top_row)
                        .tracks({px(120.0f * scale), px(122.0f * scale), px(122.0f * scale), fr()})
                        .gap(12.0f * scale)
                        .begin([&](auto& top_slots) {
                            const Rect date = top_slots.next();
                            const Rect task = top_slots.next();
                            const Rect calendar = top_slots.next();
                            top_slots.next();

                            ui.shape().in(date).radius(date.h * 0.5f).fill(0xFFF2E6, 1.0f).stroke(0xF2DFC8, 1.0f, 1.0f).draw();
                            draw_text_center(ui, "18 March", date, font_meta(scale), palette.text, 0.96f);

                            ui.shape().in(task).radius(task.h * 0.5f).fill(palette.panel_alt, 1.0f).stroke(palette.border, 1.0f, 1.0f).draw();
                            draw_icon(ui, 0xF0AEu,
                                      Rect{task.x + 14.0f * scale, task.y + 9.0f * scale, 15.0f * scale, 15.0f * scale},
                                      palette.accent, 0.94f);
                            draw_text_left(ui, "New task",
                                           Rect{task.x + 36.0f * scale, task.y + 9.0f * scale, task.w - 44.0f * scale, 16.0f * scale},
                                           font_meta(scale), palette.text, 0.96f);

                            ui.shape().in(calendar).radius(calendar.h * 0.5f).fill(0xFFFFFF, 0.92f).stroke(palette.border, 1.0f, 1.0f).draw();
                            draw_icon(ui, 0xF133u,
                                      Rect{calendar.x + 14.0f * scale, calendar.y + 9.0f * scale, 15.0f * scale, 15.0f * scale},
                                      palette.text, 0.90f);
                            draw_text_left(ui, "Calendar",
                                           Rect{calendar.x + 36.0f * scale, calendar.y + 9.0f * scale, calendar.w - 44.0f * scale, 16.0f * scale},
                                           font_meta(scale), palette.text, 0.96f);
                        });

                    draw_text_left(ui, "How can I help\nyou today?",
                                   Rect{title_row.x, title_row.y - 2.0f * scale, title_row.w, title_row.h},
                                   font_hero(scale), palette.text, 1.0f);

                    ui.row(chips)
                        .tracks({px(112.0f * scale), px(112.0f * scale), px(112.0f * scale), fr()})
                        .gap(10.0f * scale)
                        .begin([&](auto& chip_slots) {
                            for (int i = 0; i < 3; ++i) {
                                draw_chip_button(ui, input, state, palette, i, chip_slots.next(), scale);
                            }
                            chip_slots.next();
                        });
                });

            if (interactive_clicked(state, input, Rect{right.x + right.w - 88.0f * scale, right.y + 18.0f * scale, 70.0f * scale, 70.0f * scale})) {
                state.mic_active = !state.mic_active;
            }
            const float mic_hover = interactive_hovered(state, input, Rect{right.x + right.w - 88.0f * scale, right.y + 18.0f * scale, 70.0f * scale, 70.0f * scale}) ? 1.0f : 0.0f;
            const float speaking = 0.5f + 0.5f * static_cast<float>(std::sin(time_seconds * 2.2));
            const float mic_mix = smoothstep01(ui.presence("financial/mic", state.mic_active || mic_hover > 0.0f, 18.0f, 16.0f));
            paint_panel(ui, right, 26.0f * scale, scale, palette, 0.15f, 0.92f, true);
            draw_text_left(ui, "Assistant", Rect{right.x + 18.0f * scale, right.y + 18.0f * scale, right.w - 36.0f * scale, 18.0f * scale},
                           font_heading(scale), palette.text, 0.98f);
            draw_text_left(ui, "Tap the mic to capture a natural language request.",
                           Rect{right.x + 18.0f * scale, right.y + 40.0f * scale, right.w - 36.0f * scale, 34.0f * scale},
                           font_meta(scale), palette.muted, 0.96f);

            const Rect mic{right.x + right.w - 88.0f * scale, right.y + 18.0f * scale, 70.0f * scale, 70.0f * scale};
            draw_soft_glow(ui, Rect{mic.x - 28.0f * scale, mic.y - 28.0f * scale, mic.w + 56.0f * scale, mic.h + 56.0f * scale},
                           palette.accent, 0xFFFFFF, 0.82f, 0.08f + mic_mix * 0.14f + speaking * 0.04f * mic_mix);
            ui.shape()
                .in(mic)
                .radius(mic.w * 0.5f)
                .gradient(palette.accent, palette.accent_warm, 1.0f)
                .shadow(0.0f, 14.0f * scale, 22.0f * scale, palette.accent, 0.18f + mic_mix * 0.10f)
                .scale(1.0f + mic_mix * 0.04f)
                .origin_center()
                .draw();
            draw_icon(ui, 0xF130u,
                      Rect{mic.x + 22.0f * scale, mic.y + 20.0f * scale, 26.0f * scale, 26.0f * scale},
                      0xFFFFFF, 1.0f);
            draw_text_left(ui, state.mic_active ? "Listening for intent" : "Natural language command",
                           Rect{right.x + 18.0f * scale, right.y + 112.0f * scale, right.w - 36.0f * scale, 16.0f * scale},
                           font_meta(scale), palette.text, 0.96f);
            draw_text_left(ui, kQuickActions[static_cast<std::size_t>(state.active_quick_action)].label,
                           Rect{right.x + 18.0f * scale, right.y + 130.0f * scale, right.w - 36.0f * scale, 16.0f * scale},
                           font_meta(scale), palette.accent, 0.98f);
        });
}

void draw_utility_rail(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                       float scale, const Palette& palette) {
    paint_panel(ui, rect, 26.0f * scale, scale, palette, 0.0f, 1.0f, false);
    const Rect inner = inset_rect(rect, 12.0f * scale);
    for (int i = 0; i < 3; ++i) {
        const Rect item{
            inner.x + 7.0f * scale,
            inner.y + static_cast<float>(i) * 70.0f * scale + 10.0f * scale,
            inner.w - 14.0f * scale,
            54.0f * scale,
        };
        const bool item_hovered = interactive_hovered(state, input, item);
        if (interactive_clicked(state, input, item)) {
            state.utility_selection = i;
        }
        const bool selected = state.utility_selection == i;
        ui.shape()
            .in(item)
            .radius(18.0f * scale)
            .fill(selected ? palette.accent_soft : 0xFFFFFF, selected ? 1.0f : (item_hovered ? 0.96f : 0.0f))
            .stroke(selected ? palette.accent : palette.border, 1.0f, selected ? 1.0f : (item_hovered ? 0.75f : 0.0f))
            .draw();
        draw_icon(ui, kUtilityIcons[static_cast<std::size_t>(i)],
                  Rect{item.x + (item.w - 18.0f * scale) * 0.5f, item.y + (item.h - 18.0f * scale) * 0.5f, 18.0f * scale, 18.0f * scale},
                  selected ? palette.accent : palette.text, 0.94f);
    }
}

void draw_balance_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                       float scale, const Palette& palette, double time_seconds) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/balance-card"), rect, card_hovered, scale);
    ui.shape()
        .in(visual)
        .radius(28.0f * scale)
        .gradient(0xF39F59, 0xEA8D4A, 1.0f)
        .shadow(0.0f, 22.0f * scale, 42.0f * scale, palette.accent, 0.18f)
        .draw();
    ui.clip(visual, [&] {
        const float orbit = static_cast<float>(std::sin(time_seconds * 1.2));
        draw_soft_glow(ui, Rect{visual.x + visual.w * 0.52f + orbit * 18.0f * scale, visual.y - 20.0f * scale, visual.w * 0.50f, visual.h * 0.78f},
                       lighten_hex(0xFFD9B3, 0.06f), 0xFFFFFF, 0.82f, 0.22f);
        draw_soft_glow(ui, Rect{visual.x - 16.0f * scale, visual.y + visual.h * 0.44f, visual.w * 0.42f, visual.h * 0.52f},
                       lighten_hex(palette.accent, 0.10f), 0xFFFFFF, 0.82f, 0.10f);
    });

    draw_text_left(ui, "VISA", Rect{visual.x + 22.0f * scale, visual.y + 20.0f * scale, 100.0f * scale, 16.0f * scale},
                   font_heading(scale), 0xFFFFFF, 0.98f);
    draw_text_left(ui, "**** 3248", Rect{visual.x + visual.w - 110.0f * scale, visual.y + 22.0f * scale, 88.0f * scale, 16.0f * scale},
                   font_meta(scale), 0xFFFFFF, 0.76f);
    draw_text_left(ui, "Available balance",
                   Rect{visual.x + 22.0f * scale, visual.y + 70.0f * scale, visual.w - 44.0f * scale, 18.0f * scale},
                   font_meta(scale), 0xFFF1E0, 0.94f);
    draw_text_left(ui, "$14,240.50",
                   Rect{visual.x + 22.0f * scale, visual.y + 92.0f * scale, visual.w - 44.0f * scale, 36.0f * scale},
                   31.0f * scale, 0xFFFFFF, 1.0f);
    draw_text_left(ui, "Next payout in 2 days",
                   Rect{visual.x + 22.0f * scale, visual.y + visual.h - 76.0f * scale, visual.w - 44.0f * scale, 18.0f * scale},
                   font_meta(scale), 0xFFF1E0, 0.92f);

    const Rect segmented{visual.x + 20.0f * scale, visual.y + visual.h - 50.0f * scale, visual.w - 40.0f * scale, 32.0f * scale};
    draw_fill(ui, segmented, 0xFFFFFF, segmented.h * 0.5f, 0.16f);
    const float segment_w = segmented.w * 0.5f;
    const float indicator_x = ui.motion("financial/balance-toggle", segmented.x + segment_w * static_cast<float>(state.balance_mode), 14.0f);
    if (interactive_clicked(state, input, Rect{segmented.x, segmented.y, segment_w, segmented.h})) {
        state.balance_mode = 0;
    }
    if (interactive_clicked(state, input, Rect{segmented.x + segment_w, segmented.y, segment_w, segmented.h})) {
        state.balance_mode = 1;
    }
    ui.shape()
        .in(Rect{indicator_x + 3.0f * scale, segmented.y + 3.0f * scale, segment_w - 6.0f * scale, segmented.h - 6.0f * scale})
        .radius((segmented.h - 6.0f * scale) * 0.5f)
        .fill(0xFFFFFF, 0.92f)
        .shadow(0.0f, 6.0f * scale, 12.0f * scale, 0xA94E12, 0.12f)
        .draw();
    draw_text_center(ui, "Receive", Rect{segmented.x, segmented.y, segment_w, segmented.h}, font_meta(scale),
                     state.balance_mode == 0 ? palette.text : 0xFFF7EC, state.balance_mode == 0 ? 0.98f : 0.86f);
    draw_text_center(ui, "Send", Rect{segmented.x + segment_w, segmented.y, segment_w, segmented.h}, font_meta(scale),
                     state.balance_mode == 1 ? palette.text : 0xFFF7EC, state.balance_mode == 1 ? 0.98f : 0.86f);
}

void draw_stat_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale,
                    const Palette& palette, std::string_view title, std::string_view value, std::string_view caption,
                    std::uint32_t accent, std::uint32_t icon, std::uint64_t id_seed) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/stat-card", id_seed), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence(ui.id("financial/stat-card-hover", id_seed), card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 24.0f * scale, scale, palette, hover_mix, 1.0f, false);
    const Rect bubble{visual.x + 18.0f * scale, visual.y + 16.0f * scale, 34.0f * scale, 34.0f * scale};
    ui.shape()
        .in(bubble)
        .radius(12.0f * scale)
        .fill(lighten_hex(accent, 0.82f), 1.0f)
        .stroke(lighten_hex(accent, 0.56f), 1.0f, 0.88f)
        .draw();
    draw_icon(ui, icon,
              Rect{bubble.x + 9.0f * scale, bubble.y + 9.0f * scale, 16.0f * scale, 16.0f * scale},
              accent, 0.96f);
    draw_text_left(ui, title, Rect{visual.x + 18.0f * scale, visual.y + 62.0f * scale, visual.w - 36.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.96f);
    draw_text_left(ui, value, Rect{visual.x + 18.0f * scale, visual.y + 84.0f * scale, visual.w - 36.0f * scale, 28.0f * scale},
                   24.0f * scale, palette.text, 1.0f);
    draw_text_left(ui, caption, Rect{visual.x + 18.0f * scale, visual.y + visual.h - 26.0f * scale, visual.w - 36.0f * scale, 16.0f * scale},
                   font_meta(scale), accent, 0.92f);
}

void draw_lock_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/lock-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/lock-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 24.0f * scale, scale, palette, hover_mix, 1.0f, false);
    const Rect icon_rect{visual.x + 18.0f * scale, visual.y + 18.0f * scale, 42.0f * scale, 42.0f * scale};
    ui.shape().in(icon_rect).radius(14.0f * scale).fill(0xF4F7FD, 1.0f).stroke(palette.border, 1.0f, 1.0f).draw();
    draw_icon(ui, 0xF023u,
              Rect{icon_rect.x + 12.0f * scale, icon_rect.y + 11.0f * scale, 18.0f * scale, 18.0f * scale},
              palette.text, 0.94f);
    draw_text_left(ui, "System lock", Rect{visual.x + 18.0f * scale, visual.y + 74.0f * scale, visual.w - 36.0f * scale, 16.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "Protected access for treasury approvals",
                   Rect{visual.x + 18.0f * scale, visual.y + 98.0f * scale, visual.w - 36.0f * scale, 32.0f * scale},
                   font_meta(scale), palette.muted, 0.95f);
}

void draw_days_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/days-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/days-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 24.0f * scale, scale, palette, hover_mix, 1.0f, false);
    draw_text_left(ui, "13 Days", Rect{visual.x + 18.0f * scale, visual.y + 18.0f * scale, visual.w - 36.0f * scale, 22.0f * scale},
                   24.0f * scale, palette.text, 1.0f);
    draw_text_left(ui, "Until monthly goal closes",
                   Rect{visual.x + 18.0f * scale, visual.y + 44.0f * scale, visual.w - 36.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);
    const float dot_size = 10.0f * scale;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 5; ++col) {
            const int index = row * 5 + col;
            const bool active = index < 13;
            const Rect dot{
                visual.x + 18.0f * scale + static_cast<float>(col) * 22.0f * scale,
                visual.y + 80.0f * scale + static_cast<float>(row) * 20.0f * scale,
                dot_size,
                dot_size,
            };
            draw_fill(ui, dot, active ? palette.accent : 0xE8EDF5, dot_size * 0.5f, active ? 0.92f : 1.0f);
        }
    }
}

template <std::size_t N>
void draw_line_chart(UI& ui, const Rect& rect, const std::array<DayValue, N>& values, float scale,
                     const Palette& palette, int hovered_index, std::uint64_t key_prefix) {
    std::array<std::array<float, 2>, N> points{};
    float min_value = values[0].value;
    float max_value = values[0].value;
    for (const auto& item : values) {
        min_value = std::min(min_value, item.value);
        max_value = std::max(max_value, item.value);
    }
    const float span = std::max(1.0f, max_value - min_value);
    for (std::size_t i = 0; i < N; ++i) {
        const float x = rect.x + rect.w * (static_cast<float>(i) / static_cast<float>(N - 1));
        const float y = rect.y + rect.h - ((values[i].value - min_value) / span) * rect.h;
        points[i] = {x, y};
    }

    for (std::size_t i = 0; i + 1 < N; ++i) {
        const float x0 = points[i][0];
        const float y0 = points[i][1];
        const float x1 = points[i + 1][0];
        const float y1 = points[i + 1][1];
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float length = std::sqrt(dx * dx + dy * dy);
        const float angle = std::atan2(dy, dx) * 180.0f / kPi;
        const bool active = static_cast<int>(i) == hovered_index || static_cast<int>(i + 1) == hovered_index;
        ui.shape()
            .in(Rect{(x0 + x1) * 0.5f - length * 0.5f, (y0 + y1) * 0.5f - 2.0f * scale, length, 4.0f * scale})
            .radius(2.0f * scale)
            .fill(active ? palette.accent : mix_hex(palette.accent_cool, palette.accent, 0.35f), active ? 1.0f : 0.74f)
            .rotate(angle)
            .origin_center()
            .draw();
    }

    for (std::size_t i = 0; i < N; ++i) {
        const bool active = static_cast<int>(i) == hovered_index;
        const float dot = active ? 11.0f * scale : 8.0f * scale;
        const Rect point{points[i][0] - dot * 0.5f, points[i][1] - dot * 0.5f, dot, dot};
        if (active) {
            draw_fill(ui, Rect{point.x - 5.0f * scale, point.y - 5.0f * scale, point.w + 10.0f * scale, point.h + 10.0f * scale},
                      palette.accent, point.w, 0.10f);
        }
        draw_fill(ui, point, 0xFFFFFF, point.w * 0.5f, 1.0f);
        draw_stroke(ui, point, active ? palette.accent : palette.accent_cool, point.w * 0.5f, active ? 3.0f * scale : 2.0f * scale, 1.0f);
    }

    if (hovered_index >= 0) {
        const std::size_t safe = static_cast<std::size_t>(hovered_index);
        const float bubble_x = ui.motion(ui.id("financial/tooltip-x", key_prefix), points[safe][0], 18.0f);
        const float bubble_y = ui.motion(ui.id("financial/tooltip-y", key_prefix), points[safe][1] - 26.0f * scale, 18.0f);
        const Rect bubble{bubble_x - 42.0f * scale, bubble_y - 14.0f * scale, 84.0f * scale, 28.0f * scale};
        ui.shape()
            .in(bubble)
            .radius(14.0f * scale)
            .fill(0x1C2333, 0.94f)
            .shadow(0.0f, 8.0f * scale, 14.0f * scale, 0x0F172A, 0.16f)
            .draw();
        const std::string value = "$" + std::to_string(static_cast<int>(std::lround(values[safe].value * 120.0f)));
        draw_text_center(ui, value, bubble, font_meta(scale), 0xFFFFFF, 0.98f);
    }
}

void draw_trend_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                     float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/trend-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/trend-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 24.0f * scale, scale, palette, hover_mix, 1.0f, false);

    draw_text_left(ui, "Year trend", Rect{visual.x + 18.0f * scale, visual.y + 18.0f * scale, 120.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_dropdown_control(ui, input, state, palette,
                          Rect{visual.x + visual.w - 92.0f * scale, visual.y + 14.0f * scale, 74.0f * scale, 30.0f * scale},
                          OpenMenu::TrendRange, state.trend_range, kTrendRanges, scale);
    draw_text_left(ui, "+18.4%", Rect{visual.x + 18.0f * scale, visual.y + 42.0f * scale, 80.0f * scale, 18.0f * scale},
                   font_meta(scale), palette.success, 0.98f);

    const Rect plot = Rect{visual.x + 18.0f * scale, visual.y + 72.0f * scale, visual.w - 36.0f * scale, visual.h - 98.0f * scale};
    int hovered_index = -1;
    for (std::size_t i = 0; i < kTrendValues.size(); ++i) {
        const float x = plot.x + plot.w * (static_cast<float>(i) / static_cast<float>(kTrendValues.size() - 1));
        const float y = plot.y + plot.h - ((kTrendValues[i].value - 18.0f) / 51.0f) * plot.h;
        const Rect hit{x - 14.0f * scale, y - 14.0f * scale, 28.0f * scale, 28.0f * scale};
        if (interactive_hovered(state, input, hit)) {
            hovered_index = static_cast<int>(i);
        }
    }
    state.hovered_trend = hovered_index;
    draw_line_chart(ui, plot, kTrendValues, scale, palette, hovered_index, 1u);
}

void draw_growth_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                      float scale, const Palette& palette, double time_seconds) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/growth-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/growth-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 24.0f * scale, scale, palette, hover_mix, 1.0f, false);

    draw_text_left(ui, "Growth", Rect{visual.x + 18.0f * scale, visual.y + 18.0f * scale, visual.w - 36.0f * scale, 16.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "Q1 revenue mix", Rect{visual.x + 18.0f * scale, visual.y + 38.0f * scale, visual.w - 36.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);

    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(time_seconds * 1.6));
    const float progress = 0.58f + hover_mix * 0.08f;
    const Rect ring_rect{visual.x + 26.0f * scale, visual.y + 62.0f * scale, 96.0f * scale, 96.0f * scale};
    const float cx = ring_rect.x + ring_rect.w * 0.5f;
    const float cy = ring_rect.y + ring_rect.h * 0.5f;
    const float radius = ring_rect.w * 0.38f;

    draw_soft_glow(ui, Rect{ring_rect.x - 20.0f * scale, ring_rect.y - 18.0f * scale, ring_rect.w + 40.0f * scale, ring_rect.h + 40.0f * scale},
                   palette.accent, 0xFFFFFF, 0.82f, 0.04f + 0.08f * hover_mix + pulse * 0.03f * hover_mix);

    for (int i = 0; i < 36; ++i) {
        const float t = static_cast<float>(i) / 36.0f;
        const float angle = (-90.0f + t * 360.0f) * kPi / 180.0f;
        const float dot_radius = (t <= progress) ? 4.5f * scale : 3.6f * scale;
        const Rect dot{
            cx + std::cos(angle) * radius - dot_radius,
            cy + std::sin(angle) * radius - dot_radius,
            dot_radius * 2.0f,
            dot_radius * 2.0f,
        };
        draw_fill(ui, dot, t <= progress ? palette.accent : 0xE8EDF5, dot.w * 0.5f, t <= progress ? 0.96f : 1.0f);
    }

    draw_text_center(ui, "58%", ring_rect, 22.0f * scale, palette.text, 0.98f);
    draw_text_left(ui, "Strong momentum", Rect{visual.x + 136.0f * scale, visual.y + 78.0f * scale, visual.w - 148.0f * scale, 18.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "AI forecast suggests another +8% in April.",
                   Rect{visual.x + 136.0f * scale, visual.y + 102.0f * scale, visual.w - 148.0f * scale, 34.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);
}

void draw_stock_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                     float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/stock-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/stock-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 24.0f * scale, scale, palette, hover_mix, 1.0f, false);

    draw_text_left(ui, "Stock summary", Rect{visual.x + 18.0f * scale, visual.y + 18.0f * scale, visual.w - 36.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "AAPL", Rect{visual.x + 18.0f * scale, visual.y + 44.0f * scale, 80.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.92f);
    draw_text_left(ui, "$192.12", Rect{visual.x + 18.0f * scale, visual.y + 64.0f * scale, 120.0f * scale, 24.0f * scale},
                   24.0f * scale, palette.text, 1.0f);
    draw_text_left(ui, "+3.4%", Rect{visual.x + visual.w - 64.0f * scale, visual.y + 44.0f * scale, 48.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.success, 0.98f);

    const Rect plot{visual.x + 18.0f * scale, visual.y + visual.h - 64.0f * scale, visual.w - 36.0f * scale, 42.0f * scale};
    std::array<std::array<float, 2>, kStockValues.size()> points{};
    float min_value = kStockValues[0].value;
    float max_value = kStockValues[0].value;
    for (const auto& value : kStockValues) {
        min_value = std::min(min_value, value.value);
        max_value = std::max(max_value, value.value);
    }
    const float span = std::max(1.0f, max_value - min_value);
    int hovered_index = -1;
    for (std::size_t i = 0; i < kStockValues.size(); ++i) {
        const float x = plot.x + plot.w * (static_cast<float>(i) / static_cast<float>(kStockValues.size() - 1));
        const float y = plot.y + plot.h - ((kStockValues[i].value - min_value) / span) * plot.h;
        points[i] = {x, y};
        const Rect hit{x - 10.0f * scale, y - 10.0f * scale, 20.0f * scale, 20.0f * scale};
        if (interactive_hovered(state, input, hit)) {
            hovered_index = static_cast<int>(i);
        }
    }
    state.hovered_stock = hovered_index;
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const float x0 = points[i][0];
        const float y0 = points[i][1];
        const float x1 = points[i + 1][0];
        const float y1 = points[i + 1][1];
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float length = std::sqrt(dx * dx + dy * dy);
        const float angle = std::atan2(dy, dx) * 180.0f / kPi;
        ui.shape()
            .in(Rect{(x0 + x1) * 0.5f - length * 0.5f, (y0 + y1) * 0.5f - 1.5f * scale, length, 3.0f * scale})
            .radius(2.0f * scale)
            .fill(palette.accent_cool, 0.92f)
            .rotate(angle)
            .origin_center()
            .draw();
    }
    if (hovered_index >= 0) {
        const Rect marker{points[static_cast<std::size_t>(hovered_index)][0] - 5.0f * scale,
                          points[static_cast<std::size_t>(hovered_index)][1] - 5.0f * scale,
                          10.0f * scale, 10.0f * scale};
        draw_fill(ui, Rect{marker.x - 6.0f * scale, marker.y - 6.0f * scale, marker.w + 12.0f * scale, marker.h + 12.0f * scale},
                  palette.accent_cool, marker.w, 0.10f);
        draw_fill(ui, marker, 0xFFFFFF, marker.w * 0.5f, 1.0f);
        draw_stroke(ui, marker, palette.accent_cool, marker.w * 0.5f, 2.0f * scale, 1.0f);
    }
}

void draw_profit_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                      float scale, const Palette& palette, double time_seconds) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/profit-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/profit-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 28.0f * scale, scale, palette, hover_mix, 1.0f, false);

    draw_text_left(ui, "Annual profits", Rect{visual.x + 20.0f * scale, visual.y + 18.0f * scale, visual.w - 40.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "4 active rings", Rect{visual.x + 20.0f * scale, visual.y + 40.0f * scale, visual.w - 40.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);

    const Rect rings{visual.x + 24.0f * scale, visual.y + 70.0f * scale, visual.w - 48.0f * scale, visual.h - 116.0f * scale};
    const float cx = rings.x + rings.w * 0.5f;
    const float cy = rings.y + rings.h * 0.5f;
    const float base = std::min(rings.w, rings.h) * 0.18f;
    const std::array<float, 4> ring_radii{{base, base + 20.0f * scale, base + 40.0f * scale, base + 60.0f * scale}};
    const std::array<std::uint32_t, 4> ring_colors{{palette.accent, palette.accent_cool, palette.success, palette.warning}};

    for (std::size_t i = 0; i < ring_radii.size(); ++i) {
        const float radius = ring_radii[i];
        const Rect circle{cx - radius, cy - radius, radius * 2.0f, radius * 2.0f};
        ui.shape().in(circle).radius(radius).no_fill().stroke(0xE8EDF5, 8.0f * scale, 1.0f).draw();
        const float angle = static_cast<float>(time_seconds * (0.4 + static_cast<double>(i) * 0.12) + static_cast<double>(i) * 0.9);
        const Rect dot{
            cx + std::cos(angle) * radius - 7.0f * scale,
            cy + std::sin(angle) * radius - 7.0f * scale,
            14.0f * scale,
            14.0f * scale,
        };
        draw_fill(ui, dot, ring_colors[i], dot.w * 0.5f, 0.96f);
    }

    draw_text_center(ui, "$82k", Rect{cx - 48.0f * scale, cy - 18.0f * scale, 96.0f * scale, 24.0f * scale},
                     24.0f * scale, palette.text, 1.0f);
    draw_text_center(ui, "YTD profit", Rect{cx - 48.0f * scale, cy + 10.0f * scale, 96.0f * scale, 16.0f * scale},
                     font_meta(scale), palette.muted, 0.94f);

    draw_text_left(ui, "+12.8% YoY", Rect{visual.x + 20.0f * scale, visual.y + visual.h - 30.0f * scale, visual.w - 40.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.success, 0.98f);
}

void draw_task_card(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                    int index, const Rect& rect, float scale) {
    const bool hovered_state = interactive_hovered(state, input, rect);
    if (interactive_clicked(state, input, rect)) {
        state.selected_task = index;
    }
    const bool selected = state.selected_task == index;
    const float mix = smoothstep01(ui.presence(ui.id("financial/task-card", static_cast<std::uint64_t>(index)), selected || hovered_state, 16.0f, 18.0f));
    const Rect visual{rect.x, rect.y - mix * 4.0f * scale, rect.w, rect.h};
    ui.shape()
        .in(visual)
        .radius(20.0f * scale)
        .fill(selected ? lighten_hex(kTasks[static_cast<std::size_t>(index)].accent, 0.88f) : 0xFFFFFF, 1.0f)
        .stroke(selected ? kTasks[static_cast<std::size_t>(index)].accent : palette.border, 1.0f, 1.0f)
        .shadow(0.0f, 12.0f * scale, 22.0f * scale, palette.shadow, 0.06f + mix * 0.03f)
        .draw();
    draw_text_left(ui, kTasks[static_cast<std::size_t>(index)].title,
                   Rect{visual.x + 16.0f * scale, visual.y + 16.0f * scale, visual.w - 32.0f * scale, 18.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, kTasks[static_cast<std::size_t>(index)].subtitle,
                   Rect{visual.x + 16.0f * scale, visual.y + 38.0f * scale, visual.w - 32.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);
    draw_text_left(ui, kTasks[static_cast<std::size_t>(index)].amount,
                   Rect{visual.x + 16.0f * scale, visual.y + 62.0f * scale, visual.w - 32.0f * scale, 22.0f * scale},
                   20.0f * scale, palette.text, 1.0f);

    const Rect bar{visual.x + 16.0f * scale, visual.y + visual.h - 18.0f * scale, visual.w - 32.0f * scale, 8.0f * scale};
    draw_fill(ui, bar, 0xE9EEF6, bar.h * 0.5f, 1.0f);
    draw_fill(ui, Rect{bar.x, bar.y, bar.w * kTasks[static_cast<std::size_t>(index)].progress, bar.h},
              kTasks[static_cast<std::size_t>(index)].accent, bar.h * 0.5f, 1.0f);
}

void draw_activity_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                        float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/activity-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/activity-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 28.0f * scale, scale, palette, hover_mix, 1.0f, false);

    draw_text_left(ui, "Activity manager",
                   Rect{visual.x + 24.0f * scale, visual.y + 20.0f * scale, 180.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "Track work, approvals and campaign status",
                   Rect{visual.x + 24.0f * scale, visual.y + 42.0f * scale, 240.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);

    draw_search_field(ui, Rect{visual.x + visual.w - 350.0f * scale, visual.y + 18.0f * scale, 230.0f * scale, 36.0f * scale}, scale, state.activity_search);
    draw_dropdown_control(ui, input, state, palette,
                          Rect{visual.x + visual.w - 108.0f * scale, visual.y + 18.0f * scale, 92.0f * scale, 36.0f * scale},
                          OpenMenu::ActivityRange, state.activity_range, kActivityRanges, scale);

    const Rect tasks{visual.x + 22.0f * scale, visual.y + 82.0f * scale, visual.w - 44.0f * scale, 128.0f * scale};
    ui.row(tasks)
        .tracks({fr(), fr(), fr()})
        .gap(12.0f * scale)
        .begin([&](auto& cols) {
            for (int i = 0; i < 3; ++i) {
                draw_task_card(ui, input, state, palette, i, cols.next(), scale);
            }
        });

    draw_text_left(ui, "Today load", Rect{visual.x + 24.0f * scale, visual.y + visual.h - 78.0f * scale, 100.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.92f);
    const Rect timeline{visual.x + 24.0f * scale, visual.y + visual.h - 48.0f * scale, visual.w - 48.0f * scale, 24.0f * scale};
    for (int i = 0; i < 12; ++i) {
        const float intensity = (i == state.selected_task * 4 + 1 || i == state.selected_task * 4 + 2) ? 1.0f : (0.24f + static_cast<float>(i % 4) * 0.08f);
        const float h = 8.0f * scale + intensity * 14.0f * scale;
        const Rect bar{
            timeline.x + static_cast<float>(i) * (timeline.w / 12.0f) + 4.0f * scale,
            timeline.y + timeline.h - h,
            timeline.w / 12.0f - 8.0f * scale,
            h,
        };
        draw_fill(ui, bar, i >= state.selected_task * 4 && i <= state.selected_task * 4 + 2 ? palette.accent : 0xDCE4F0, 5.0f * scale, 1.0f);
    }
}

void draw_review_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                      float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const Rect visual = lifted_rect(ui, ui.id("financial/review-card"), rect, card_hovered, scale);
    const float hover_mix = smoothstep01(ui.presence("financial/review-card-hover", card_hovered, 16.0f, 18.0f));
    paint_panel(ui, visual, 28.0f * scale, scale, palette, hover_mix, 1.0f, false);

    draw_text_left(ui, "Review rating", Rect{visual.x + 20.0f * scale, visual.y + 18.0f * scale, visual.w - 40.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "Client satisfaction", Rect{visual.x + 20.0f * scale, visual.y + 40.0f * scale, visual.w - 40.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);

    for (int i = 0; i < 5; ++i) {
        const Rect star{
            visual.x + 18.0f * scale + static_cast<float>(i) * 34.0f * scale,
            visual.y + 78.0f * scale,
            26.0f * scale,
            26.0f * scale,
        };
        if (interactive_clicked(state, input, star)) {
            state.selected_rating = i + 1;
        }
        const bool active = i < state.selected_rating;
        ui.shape()
            .in(Rect{star.x - 3.0f * scale, star.y - 3.0f * scale, star.w + 6.0f * scale, star.h + 6.0f * scale})
            .radius(12.0f * scale)
            .fill(active ? palette.accent_soft : 0xFFFFFF, active ? 1.0f : 0.0f)
            .draw();
        draw_icon(ui, 0xF005u, star, active ? palette.accent : 0xD3DCE8, 0.92f);
    }

    draw_text_left(ui, state.selected_rating >= 4 ? "Excellent clarity" : "Needs refinement",
                   Rect{visual.x + 20.0f * scale, visual.y + 124.0f * scale, visual.w - 40.0f * scale, 18.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "The motion system feels responsive and premium in review mode.",
                   Rect{visual.x + 20.0f * scale, visual.y + 150.0f * scale, visual.w - 40.0f * scale, 36.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);

    const Rect pill{visual.x + 20.0f * scale, visual.y + visual.h - 40.0f * scale, 96.0f * scale, 28.0f * scale};
    ui.shape().in(pill).radius(pill.h * 0.5f).fill(palette.accent_soft, 1.0f).stroke(palette.accent, 1.0f, 0.88f).draw();
    draw_text_center(ui, "4.9 score", pill, font_meta(scale), palette.accent, 0.98f);
}

template <std::size_t N>
void draw_dropdown_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                           const Rect& button, OpenMenu menu_id, int& selected_index,
                           const std::array<const char*, N>& labels, float scale) {
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float menu_mix = smoothstep01(ui.presence(ui.id("financial/dropdown", static_cast<std::uint64_t>(menu_id)),
                                                    state.open_menu == menu_id, 18.0f, 16.0f));
    if (menu_mix <= 0.01f) {
        return;
    }

    const Rect base_menu{
        button.x,
        button.y + button.h + 8.0f * scale,
        std::max(button.w, 128.0f * scale),
        static_cast<float>(N) * 34.0f * scale + 10.0f * scale,
    };
    const Rect menu{
        base_menu.x,
        base_menu.y + (1.0f - menu_mix) * 10.0f * scale,
        base_menu.w,
        base_menu.h,
    };
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});

    ui.shape()
        .in(menu)
        .radius(18.0f * scale)
        .fill(0xF9FBFF, 0.36f * menu_mix)
        .stroke(0xFFFFFF, 1.0f, 0.22f * menu_mix)
        .blur(18.0f * scale, 26.0f * scale)
        .shadow(0.0f, 8.0f * scale, 24.0f * scale, palette.shadow, 0.12f * menu_mix)
        .draw();
    ui.shape()
        .in(inset_rect(menu, 1.0f))
        .radius(17.0f * scale)
        .fill(0xFFFFFF, 0.96f * menu_mix)
        .stroke(palette.border, 1.0f, 0.8f * menu_mix)
        .draw();

    ui.ctx().set_global_alpha(menu_mix);
    ui.clip(menu, [&] {
        for (std::size_t i = 0; i < N; ++i) {
            const Rect item{
                menu.x + 5.0f * scale,
                menu.y + 5.0f * scale + static_cast<float>(i) * 34.0f * scale + (1.0f - menu_mix) * 6.0f * scale,
                menu.w - 10.0f * scale,
                30.0f * scale,
            };
            const bool item_hovered = hovered(input, item);
            const bool item_selected = selected_index == static_cast<int>(i);
            if (item_selected || item_hovered) {
                ui.shape()
                    .in(item)
                    .radius(item.h * 0.5f)
                    .fill(item_selected ? palette.accent_soft : 0xF6F9FF, item_selected ? 1.0f : 0.92f)
                    .draw();
            }
            draw_text_left(ui, labels[i],
                           Rect{item.x + 12.0f * scale, item.y + 7.0f * scale, item.w - 24.0f * scale, 16.0f * scale},
                           font_meta(scale), item_selected ? palette.accent : palette.text, 0.98f);
            if (menu_mix > 0.9f && clicked(input, item)) {
                selected_index = static_cast<int>(i);
                state.open_menu = OpenMenu::None;
            }
        }
    });
    ui.ctx().set_global_alpha(1.0f);

    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base_menu.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_quick_actions_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                                float scale) {
    const Rect button = state.quick_action_button_rect;
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float menu_mix = smoothstep01(ui.presence("financial/quickactions/menu",
                                                    state.open_menu == OpenMenu::QuickActions, 18.0f, 16.0f));
    if (menu_mix <= 0.01f) {
        return;
    }

    const Rect menu{
        button.x - 150.0f * scale,
        button.y + button.h + 10.0f * scale + (1.0f - menu_mix) * 12.0f * scale,
        196.0f * scale,
        142.0f * scale,
    };
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});
    paint_panel(ui, menu, 20.0f * scale, scale, palette, 0.4f, 0.94f, true);
    ui.ctx().set_global_alpha(menu_mix);
    for (std::size_t i = 0; i < kQuickActions.size(); ++i) {
        const Rect item{
            menu.x + 10.0f * scale,
            menu.y + 10.0f * scale + static_cast<float>(i) * 40.0f * scale + (1.0f - menu_mix) * 5.0f * scale,
            menu.w - 20.0f * scale,
            34.0f * scale,
        };
        const bool item_hovered = hovered(input, item);
        const bool item_selected = state.active_quick_action == static_cast<int>(i);
        if (item_hovered || item_selected) {
            ui.shape()
                .in(item)
                .radius(12.0f * scale)
                .fill(item_selected ? palette.accent_soft : 0xF6F9FF, item_selected ? 1.0f : 0.94f)
                .draw();
        }
        draw_icon(ui, kQuickActions[i].icon,
                  Rect{item.x + 10.0f * scale, item.y + 9.0f * scale, 16.0f * scale, 16.0f * scale},
                  item_selected ? palette.accent : palette.text, 0.96f);
        draw_text_left(ui, kQuickActions[i].label,
                       Rect{item.x + 34.0f * scale, item.y + 5.0f * scale, item.w - 40.0f * scale, 14.0f * scale},
                       font_body(scale), palette.text, 0.98f);
        draw_text_left(ui, kQuickActions[i].caption,
                       Rect{item.x + 34.0f * scale, item.y + 18.0f * scale, item.w - 40.0f * scale, 12.0f * scale},
                       font_meta(scale), palette.muted, 0.92f);
        if (menu_mix > 0.9f && clicked(input, item)) {
            state.active_quick_action = static_cast<int>(i);
            state.open_menu = OpenMenu::None;
        }
    }
    ui.ctx().set_global_alpha(1.0f);

    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !menu.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_overlay_menus(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                        const Rect& clip_rect, float scale) {
    ui.clip(clip_rect, [&] {
        draw_quick_actions_overlay(ui, input, state, palette, scale);
        draw_dropdown_overlay(ui, input, state, palette, state.trend_dropdown_rect,
                              OpenMenu::TrendRange, state.trend_range, kTrendRanges, scale);
        draw_dropdown_overlay(ui, input, state, palette, state.activity_dropdown_rect,
                              OpenMenu::ActivityRange, state.activity_range, kActivityRanges, scale);
    });
}

void draw_dashboard(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                    float scale, const Palette& palette, double time_seconds) {
    ui.shape()
        .in(rect)
        .radius(36.0f * scale)
        .gradient(palette.shell, palette.shell_alt, 1.0f)
        .stroke(lighten_hex(palette.border, 0.08f), 1.0f, 1.0f)
        .shadow(0.0f, 24.0f * scale, 64.0f * scale, palette.shadow, 0.12f)
        .draw();

    ui.clip(rect, [&] {
        draw_soft_glow(ui, Rect{rect.x - 80.0f * scale, rect.y + rect.h * 0.12f, 280.0f * scale, 280.0f * scale},
                       palette.accent_soft, 0xFFFFFF, 0.82f, 0.18f);
        draw_soft_glow(ui, Rect{rect.x + rect.w * 0.68f, rect.y - 120.0f * scale, 320.0f * scale, 320.0f * scale},
                       lighten_hex(palette.accent_cool, 0.68f), 0xFFFFFF, 0.82f, 0.12f);
    });

    ui.view(rect)
        .padding(22.0f * scale)
        .column()
        .tracks({
            px(64.0f * scale),
            px(182.0f * scale),
            px(296.0f * scale),
            fr(),
        })
        .gap(18.0f * scale)
        .begin([&](auto& rows) {
            draw_header(ui, input, state, rows.next(), scale, palette);
            draw_hero(ui, input, state, rows.next(), scale, palette, time_seconds);

            const Rect upper = rows.next();
            ui.row(upper)
                .tracks({
                    px(82.0f * scale),
                    px(338.0f * scale),
                    px(168.0f * scale),
                    px(168.0f * scale),
                    fr(),
                })
                .gap(16.0f * scale)
                .begin([&](auto& cols) {
                    draw_utility_rail(ui, input, state, cols.next(), scale, palette);
                    draw_balance_card(ui, input, state, cols.next(), scale, palette, time_seconds);

                    ui.column(cols.next())
                        .tracks({fr(), fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& stat_rows) {
                            draw_stat_card(ui, input, state, stat_rows.next(), scale, palette, "Income", "$9,410", "+14% month to date", palette.success, 0xF201u, 1u);
                            draw_stat_card(ui, input, state, stat_rows.next(), scale, palette, "Paid", "$5,925", "14 invoices settled", palette.accent_cool, 0xF555u, 2u);
                        });

                    ui.column(cols.next())
                        .tracks({fr(), fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& mini_rows) {
                            draw_lock_card(ui, input, state, mini_rows.next(), scale, palette);
                            draw_days_card(ui, input, state, mini_rows.next(), scale, palette);
                        });

                    const Rect analytics = cols.next();
                    ui.column(analytics)
                        .tracks({px(112.0f * scale), fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& analytics_rows) {
                            draw_trend_card(ui, input, state, analytics_rows.next(), scale, palette);
                            const Rect bottom = analytics_rows.next();
                            ui.row(bottom)
                                .tracks({px(224.0f * scale), fr()})
                                .gap(16.0f * scale)
                                .begin([&](auto& bottom_cols) {
                                    draw_growth_card(ui, input, state, bottom_cols.next(), scale, palette, time_seconds);
                                    draw_stock_card(ui, input, state, bottom_cols.next(), scale, palette);
                                });
                        });
                });

            const Rect lower = rows.next();
            ui.row(lower)
                .tracks({
                    px(258.0f * scale),
                    fr(),
                    px(222.0f * scale),
                })
                .gap(16.0f * scale)
                .begin([&](auto& cols) {
                    draw_profit_card(ui, input, state, cols.next(), scale, palette, time_seconds);
                    draw_activity_card(ui, input, state, cols.next(), scale, palette);
                    draw_review_card(ui, input, state, cols.next(), scale, palette);
                });
        });

    draw_overlay_menus(ui, input, state, palette, rect, scale);
}

}  // namespace

int main() {
    DashboardState state{};
    const Palette palette = make_palette();

    eui::app::AppOptions options{};
    options.title = "EUI Financial Assistant Dashboard";
    options.width = 1460;
    options.height = 940;
    options.vsync = true;
    options.continuous_render = true;
    options.max_fps = 120.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";
    options.enable_icon_font_fallback = true;
    options.text_backend = eui::app::AppOptions::TextBackend::Auto;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const auto& input = ctx.input_state();
            const float framebuffer_w = static_cast<float>(metrics.framebuffer_w);
            const float framebuffer_h = static_cast<float>(metrics.framebuffer_h);
            const float scale = std::max(0.72f, std::min(framebuffer_w / 1500.0f, framebuffer_h / 980.0f));
            const float margin = 18.0f * scale;
            const Rect root{margin, margin, framebuffer_w - margin * 2.0f, framebuffer_h - margin * 2.0f};

            ctx.set_theme_mode(eui::ThemeMode::Light);
            ctx.set_primary_color(
                eui::rgba(static_cast<float>((palette.accent >> 16u) & 0xffu) / 255.0f,
                          static_cast<float>((palette.accent >> 8u) & 0xffu) / 255.0f,
                          static_cast<float>(palette.accent & 0xffu) / 255.0f, 1.0f));
            ctx.set_corner_radius(16.0f * scale);

            state.quick_action_button_rect = Rect{};
            state.trend_dropdown_rect = Rect{};
            state.activity_dropdown_rect = Rect{};
            if (state.open_menu == OpenMenu::None) {
                state.overlay_block_active = false;
                state.overlay_block_rect = Rect{};
            }

            UI ui(ctx);
            draw_fill(ui, Rect{0.0f, 0.0f, framebuffer_w, framebuffer_h}, palette.background, 0.0f, 1.0f);
            draw_soft_glow(ui, Rect{-120.0f * scale, 80.0f * scale, 360.0f * scale, 360.0f * scale},
                           palette.accent_soft, 0xFFFFFF, 0.82f, 0.24f);
            draw_soft_glow(ui, Rect{framebuffer_w - 320.0f * scale, -100.0f * scale, 360.0f * scale, 360.0f * scale},
                           lighten_hex(palette.accent_cool, 0.68f), 0xFFFFFF, 0.82f, 0.16f);

            draw_dashboard(ui, input, state, root, scale, palette, input.time_seconds);
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
