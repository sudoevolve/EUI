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

enum class NavItem : int {
    Dashboard = 0,
    Deliveries = 1,
    Documents = 2,
    Wallet = 3,
    Analytics = 4,
};

enum class OpenMenu : int {
    None = 0,
    Orders = 1,
    Activity = 2,
    Couriers = 3,
};

struct Palette {
    bool dark{true};
    std::uint32_t shell{};
    std::uint32_t panel{};
    std::uint32_t panel_alt{};
    std::uint32_t divider{};
    std::uint32_t divider_strong{};
    std::uint32_t text{};
    std::uint32_t muted{};
    std::uint32_t soft{};
    std::uint32_t accent{};
    std::uint32_t accent_bright{};
    std::uint32_t accent_soft{};
    std::uint32_t chip{};
    std::uint32_t chip_active{};
    std::uint32_t chip_active_text{};
    std::uint32_t pill_dark{};
    std::uint32_t pill_blue{};
    std::uint32_t pill_blue_text{};
};

struct MetricCard {
    const char* title;
    const char* value;
    const char* delta;
};

struct OrderRow {
    const char* id;
    const char* address;
    const char* expires;
    const char* status;
    bool in_transit;
};

struct Courier {
    const char* name;
    const char* orders;
    std::uint32_t avatar_top;
    std::uint32_t avatar_bottom;
    std::uint32_t hair;
    std::uint32_t shirt;
    std::uint32_t skin;
};

struct PageCopy {
    const char* badge;
    const char* orders_title;
    const char* activity_title;
    const char* couriers_title;
    int highlighted_metric;
};

struct AccentTheme {
    const char* name;
    std::uint32_t accent;
};

struct DashboardState {
    NavItem nav{NavItem::Dashboard};
    int filter_index{2};
    int selected_order{4};
    int orders_range{0};
    int activity_range{1};
    int couriers_range{0};
    int hovered_bar{-1};
    std::string search_text{};
    OpenMenu open_menu{OpenMenu::None};
    bool settings_open{false};
    bool theme_dark{true};
    int accent_index{0};
    Rect overlay_block_rect{};
    bool overlay_block_active{false};
};

static constexpr std::array<MetricCard, 4> kMetrics{{
    {"On-Time Deliveries", "96%", "+2% vs last month"},
    {"Total Deliveries", "568", "+24% vs last month"},
    {"Total Vehicles", "159", "+27 vs last month"},
    {"Driver Behavior Score", "95%", "+1% vs last month"},
}};
static constexpr std::array<MetricCard, 4> kDeliveryMetrics{{
    {"Active Routes", "128", "+14 vs last week"},
    {"Delivered Parcels", "1,284", "+11% vs last week"},
    {"Vehicles On Road", "64", "+6 vs yesterday"},
    {"Missed Windows", "3.2%", "-0.8% vs last week"},
}};
static constexpr std::array<MetricCard, 4> kDocumentMetrics{{
    {"Open Waybills", "182", "+17 pending"},
    {"Approved Today", "96", "+22 vs yesterday"},
    {"Awaiting Signature", "34", "-4 vs yesterday"},
    {"Document Accuracy", "99.1%", "+0.3% vs last week"},
}};
static constexpr std::array<MetricCard, 4> kWalletMetrics{{
    {"Pending Payouts", "$18.4k", "+$2.2k today"},
    {"Processed Today", "$42.8k", "+12% vs yesterday"},
    {"Driver Bonuses", "$6.9k", "+$940 today"},
    {"Failed Charges", "0.4%", "-0.2% vs last week"},
}};
static constexpr std::array<MetricCard, 4> kAnalyticsMetrics{{
    {"Conversion Rate", "18.2%", "+1.4% vs last week"},
    {"Avg Route Time", "28 min", "-3 min vs last week"},
    {"Demand Peaks", "4", "+1 region"},
    {"Forecast Score", "92%", "+2% vs last month"},
}};

static constexpr std::array<OrderRow, 6> kOrders{{
    {"#10234", "47 Maple Ave, Cambridge", "26 Jan", "Picked up", false},
    {"#10444", "99 Ocean Rd, Brooklyn, NY", "28 Jan", "In transit", true},
    {"#10345", "22 Palm St, San Diego", "30 Jan", "In transit", true},
    {"#10376", "21 King Rd, Ottawa", "6 Feb", "Picked up", false},
    {"#10254", "912nd Ave, Portland", "3 Feb", "Picked up", false},
    {"#10254", "79 Lakeshore Blvd, Kelowna", "8 Feb", "Picked up", false},
}};

static constexpr std::array<Courier, 4> kCouriers{{
    {"Adem Barnes", "23 orders", 0xE3EEF9, 0xC9D8EA, 0x4A5565, 0x6E8EB7, 0xF1C8A8},
    {"Annette Black", "23 orders", 0xF6D5E3, 0xD59EC0, 0x915775, 0x4F6FA8, 0xF2C5AD},
    {"Kristin Miles", "21 orders", 0xF7E7D7, 0xE1C9AF, 0xC6A66C, 0x90A1C4, 0xF4CFB2},
    {"Marvin McKinney", "21 orders", 0xECE7E0, 0xD2C4B5, 0x5C4B3F, 0x8F8177, 0xE8C09A},
}};

static constexpr std::array<float, 14> kBarsWeekA{{4.0f, 15.0f, 29.0f, 35.0f, 25.0f, 16.0f, 30.0f, 38.0f, 34.0f, 14.0f, 20.0f, 26.0f, 40.0f, 36.0f}};
static constexpr std::array<float, 14> kBarsWeekB{{6.0f, 18.0f, 24.0f, 31.0f, 27.0f, 19.0f, 28.0f, 36.0f, 32.0f, 17.0f, 23.0f, 29.0f, 37.0f, 34.0f}};
static constexpr std::array<float, 14> kBarsMonth{{8.0f, 11.0f, 19.0f, 27.0f, 24.0f, 22.0f, 25.0f, 33.0f, 30.0f, 26.0f, 29.0f, 36.0f, 41.0f, 39.0f}};
static constexpr std::array<const char*, 14> kBarLabels{{
    "5 Jan", "6 Jan", "7 Jan", "8 Jan", "9 Jan", "10 Jan", "11 Jan",
    "12 Jan", "13 Jan", "14 Jan", "15 Jan", "16 Jan", "17 Jan", "18 Jan",
}};

static constexpr std::array<const char*, 3> kOrdersRanges{{"This week", "This month", "This quarter"}};
static constexpr std::array<const char*, 3> kActivityRanges{{"Last 7 days", "Last 2 weeks", "Last month"}};
static constexpr std::array<const char*, 2> kCouriersRanges{{"This week", "This month"}};
static constexpr std::array<const char*, 4> kFilterLabels{{"Pending", "Responded", "Assigned", "Completed"}};
static constexpr std::array<const char*, 4> kFilterCounts{{"21", "24", "15", "76"}};
static constexpr std::array<std::uint32_t, 5> kSidebarIcons{{0xF00Au, 0xF0D1u, 0xF15Cu, 0xF555u, 0xF080u}};
static constexpr std::array<PageCopy, 5> kPages{{
    {"Dashboard", "Orders", "Activity", "Top couriers", 0},
    {"Deliveries", "Delivery queue", "Route activity", "Drivers on shift", 1},
    {"Documents", "Waybills", "Approval flow", "Owners", 2},
    {"Wallet", "Payouts", "Cash movement", "Top earners", 3},
    {"Analytics", "Segments", "Traffic volume", "Top regions", 1},
}};

static constexpr std::array<AccentTheme, 12> kAccentThemes{{
    {"Indigo", 0x5E76D8},
    {"Sky", 0x46A9FF},
    {"Cyan", 0x33B7D6},
    {"Mint", 0x38C89D},
    {"Emerald", 0x32B882},
    {"Lime", 0x79C64A},
    {"Amber", 0xD7A43B},
    {"Orange", 0xE58C42},
    {"Coral", 0xE06A78},
    {"Rose", 0xD96CB3},
    {"Violet", 0x8B72E8},
    {"Slate", 0x7C8AA6},
}};

std::uint32_t mix_hex(std::uint32_t lhs, std::uint32_t rhs, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
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

std::uint32_t darken_hex(std::uint32_t hex, float amount) {
    return mix_hex(hex, 0x000000, amount);
}

Palette make_palette(bool dark, int accent_index) {
    const std::size_t safe_index =
        static_cast<std::size_t>(std::clamp(accent_index, 0, static_cast<int>(kAccentThemes.size() - 1)));
    const std::uint32_t accent = kAccentThemes[safe_index].accent;
    const std::uint32_t accent_bright = dark ? lighten_hex(accent, 0.42f) : darken_hex(accent, 0.10f);
    const std::uint32_t accent_soft = dark ? mix_hex(accent, 0x0D1322, 0.78f) : lighten_hex(accent, 0.86f);

    if (dark) {
        return Palette{
            true,
            0x171A28,
            0x1A1D2C,
            0x1E2234,
            0x33384B,
            0x42485F,
            0xF5F6FB,
            0xA2A8BF,
            0x7D839C,
            accent,
            accent_bright,
            accent_soft,
            0x222738,
            lighten_hex(accent, 0.76f),
            0x171A28,
            0x222737,
            lighten_hex(accent, 0.72f),
            0x1B2436,
        };
    }

    return Palette{
        false,
        0xEDF2F8,
        0xFFFFFF,
        0xF3F6FB,
        0xD9E1EC,
        0xC7D2E1,
        0x182033,
        0x6D778A,
        0x8F98AA,
        accent,
        accent_bright,
        accent_soft,
        0xEEF3F9,
        accent,
        0xF9FBFF,
        0xE6ECF5,
        lighten_hex(accent, 0.82f),
        0x182033,
    };
}

const std::array<float, 14>& active_bars(const DashboardState& state) {
    if (state.activity_range == 0) {
        return kBarsWeekB;
    }
    if (state.activity_range == 2) {
        return kBarsMonth;
    }
    return kBarsWeekA;
}

const PageCopy& page_copy(NavItem nav) {
    return kPages[static_cast<std::size_t>(nav)];
}

const std::array<MetricCard, 4>& page_metrics(NavItem nav) {
    switch (nav) {
        case NavItem::Deliveries:
            return kDeliveryMetrics;
        case NavItem::Documents:
            return kDocumentMetrics;
        case NavItem::Wallet:
            return kWalletMetrics;
        case NavItem::Analytics:
            return kAnalyticsMetrics;
        case NavItem::Dashboard:
        default:
            return kMetrics;
    }
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

std::size_t menu_slot(OpenMenu menu) {
    return static_cast<std::size_t>(static_cast<int>(menu) - 1);
}

Rect inset_rect(const Rect& rect, float padding) {
    return Rect{
        rect.x + padding,
        rect.y + padding,
        std::max(0.0f, rect.w - padding * 2.0f),
        std::max(0.0f, rect.h - padding * 2.0f),
    };
}

Rect inset_rect(const Rect& rect, float horizontal, float vertical) {
    return Rect{
        rect.x + horizontal,
        rect.y + vertical,
        std::max(0.0f, rect.w - horizontal * 2.0f),
        std::max(0.0f, rect.h - vertical * 2.0f),
    };
}

Rect inset_rect(const Rect& rect, float left, float top, float right, float bottom) {
    return Rect{
        rect.x + left,
        rect.y + top,
        std::max(0.0f, rect.w - left - right),
        std::max(0.0f, rect.h - top - bottom),
    };
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

void draw_avatar(UI& ui, const Rect& rect, const Courier& avatar, float scale) {
    const float radius = rect.w * 0.5f;
    draw_fill(ui, rect, 0xFFFFFF, radius, 0.92f);
    const Rect inner = inset_rect(rect, 2.0f * scale);
    ui.shape().in(inner).radius(inner.w * 0.5f).gradient(avatar.avatar_top, avatar.avatar_bottom).draw();
    draw_fill(ui, Rect{inner.x + inner.w * 0.18f, inner.y + inner.h * 0.58f, inner.w * 0.64f, inner.h * 0.34f}, avatar.shirt, inner.h * 0.17f, 0.95f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.43f, inner.y + inner.h * 0.46f, inner.w * 0.14f, inner.h * 0.11f}, avatar.skin, inner.w * 0.06f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.30f, inner.y + inner.h * 0.19f, inner.w * 0.40f, inner.h * 0.40f}, avatar.skin, inner.w * 0.20f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.20f, inner.y + inner.h * 0.06f, inner.w * 0.60f, inner.h * 0.36f}, avatar.hair, inner.w * 0.30f, 0.98f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.19f, inner.y + inner.h * 0.24f, inner.w * 0.12f, inner.h * 0.22f}, avatar.hair, inner.w * 0.06f, 0.98f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.69f, inner.y + inner.h * 0.24f, inner.w * 0.12f, inner.h * 0.22f}, avatar.hair, inner.w * 0.06f, 0.98f);
}

void draw_top_profile(UI& ui, const Rect& rect, float scale, const Palette& palette) {
    const Courier hero{"Zaina Riddle", "Admin", 0xF0D7E4, 0xD89FBE, 0x9A6381, 0x5D76A9, 0xF2C7AF};
    const Rect avatar_rect{rect.x + 18.0f * scale, rect.y + (rect.h - 40.0f * scale) * 0.5f, 40.0f * scale, 40.0f * scale};
    draw_avatar(ui, avatar_rect, hero, scale);
    draw_text_left(ui, "Zaina Riddle", Rect{avatar_rect.x + avatar_rect.w + 14.0f * scale, rect.y + 10.0f * scale,
                                            rect.w - avatar_rect.w - 40.0f * scale, 18.0f * scale},
                   13.5f * scale, palette.text, 0.98f);
    draw_text_left(ui, "Admin", Rect{avatar_rect.x + avatar_rect.w + 14.0f * scale, rect.y + 32.0f * scale,
                                     rect.w - avatar_rect.w - 40.0f * scale, 16.0f * scale},
                   11.0f * scale, palette.muted, 0.95f);
}

template <std::size_t N>
void draw_dropdown_control(UI& ui, const eui::InputState& input, Rect button, OpenMenu menu_id, OpenMenu& open_menu,
                           int& selected_index, const std::array<const char*, N>& labels, float scale,
                           DashboardState& state, const Palette& palette) {
    const std::size_t menu_index = menu_slot(menu_id);
    const std::uint64_t menu_motion_id =
        ui.id("dashboard/dropdown", static_cast<std::uint64_t>(menu_index + 1u));
    const float radius = button.h * 0.5f;
    const bool button_hovered = interactive_hovered(state, input, button);
    if (interactive_clicked(state, input, button)) {
        if (open_menu != menu_id) {
            ui.reset_motion(menu_motion_id, 0.0f);
        }
        open_menu = (open_menu == menu_id) ? OpenMenu::None : menu_id;
    }

    const float menu_mix = smoothstep01(ui.presence(menu_motion_id, open_menu == menu_id, 18.0f, 14.0f));

    ui.shape()
        .in(button)
        .radius(radius)
        .fill(palette.chip, button_hovered ? 0.96f : 0.90f)
        .stroke(button_hovered ? palette.accent_bright : palette.divider, 1.0f, button_hovered ? 0.34f : 1.0f)
        .draw();
    draw_text_center(ui, labels[static_cast<std::size_t>(selected_index)],
                     inset_rect(button, 12.0f * scale, 0.0f, 22.0f * scale, 0.0f), 11.5f * scale, palette.text, 0.96f);
    draw_icon(ui, 0xF078u, Rect{button.x + button.w - 22.0f * scale, button.y + (button.h - 12.0f * scale) * 0.5f,
                                12.0f * scale, 12.0f * scale}, palette.text, 0.82f);

    if (menu_mix <= 0.01f) {
        return;
    }

    const Rect base_menu{
        button.x,
        button.y + button.h + 8.0f * scale,
        std::max(button.w, 132.0f * scale),
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
        .radius(12.0f * scale)
        .blur(26.0f * scale, 26.0f * scale)
        .fill(0xF7FAFF, (palette.dark ? 0.05f : 0.12f) * menu_mix)
        .stroke(0xFFFFFF, 1.0f, (palette.dark ? 0.10f : 0.16f) * menu_mix)
        .shadow(0.0f, 2.0f * scale, 8.0f * scale, 0x030712, 0.06f * menu_mix)
        .draw();
    ui.shape()
        .in(inset_rect(menu, 1.0f))
        .radius(11.0f * scale)
        .fill(palette.panel_alt, (palette.dark ? 0.34f : 0.46f) * menu_mix)
        .stroke(palette.divider, 1.0f, 0.68f * menu_mix)
        .draw();

    ui.ctx().set_global_alpha(menu_mix);
    ui.clip(menu, [&] {
        for (std::size_t i = 0; i < N; ++i) {
            const Rect item{menu.x + 5.0f * scale,
                            menu.y + 5.0f * scale + static_cast<float>(i) * 34.0f * scale + (1.0f - menu_mix) * 6.0f * scale,
                            menu.w - 10.0f * scale, 30.0f * scale};
            const bool item_hovered = hovered(input, item);
            const bool item_selected = selected_index == static_cast<int>(i);
            if (item_selected || item_hovered) {
                ui.shape()
                    .in(item)
                    .radius(item.h * 0.5f)
                    .fill(item_selected ? palette.chip_active : palette.panel_alt, item_selected ? 1.0f : 0.92f)
                    .shadow(0.0f, 2.0f * scale, 6.0f * scale, 0x030712, item_selected ? 0.08f : 0.04f)
                    .draw();
            }
            draw_text_left(ui, labels[i],
                           Rect{item.x + 12.0f * scale, item.y + 7.0f * scale, item.w - 24.0f * scale, 16.0f * scale},
                           11.5f * scale, item_selected ? palette.chip_active_text : palette.text, 0.98f);
            if (menu_mix > 0.9f && clicked(input, item)) {
                selected_index = static_cast<int>(i);
                open_menu = OpenMenu::None;
            }
        }
    });
    ui.ctx().set_global_alpha(1.0f);

    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base_menu.contains(input.mouse_x, input.mouse_y)) {
        open_menu = OpenMenu::None;
    }
}

void draw_sidebar(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale, const Palette& palette) {
    draw_fill(ui, rect, palette.panel, 0.0f, 1.0f);
    ui.shape().in(Rect{rect.x + rect.w - 1.0f, rect.y, 1.0f, rect.h}).fill(palette.divider, 0.95f).draw();

    const float slot_h = 58.0f * scale;
    const float indicator_target_y = rect.y + static_cast<float>(static_cast<int>(state.nav)) * slot_h;
    const std::uint64_t indicator_motion_id = ui.id("dashboard/sidebar-indicator");
    if (ui.motion_value(indicator_motion_id, -1.0f) < 0.0f) {
        ui.reset_motion(indicator_motion_id, indicator_target_y);
    }
    const float indicator_y = ui.motion(indicator_motion_id, indicator_target_y, 16.0f);
    const Rect indicator{rect.x, indicator_y, rect.w, slot_h};
    draw_fill(ui, indicator, 0xF8F8FA, 0.0f, 1.0f);
    draw_fill(ui, Rect{indicator.x, indicator.y, 4.0f * scale, indicator.h}, palette.accent, 0.0f, 1.0f);

    for (std::size_t i = 0; i < kSidebarIcons.size(); ++i) {
        const Rect slot{rect.x, rect.y + static_cast<float>(i) * slot_h, rect.w, slot_h};
        if (interactive_clicked(state, input, slot)) {
            const NavItem next = static_cast<NavItem>(static_cast<int>(i));
            if (next != state.nav) {
                state.nav = next;
                ui.reset_motion("dashboard/page-reveal", 0.0f);
                state.hovered_bar = -1;
                state.settings_open = false;
            }
        }
        const bool selected = static_cast<int>(state.nav) == static_cast<int>(i);
        if (!selected && interactive_hovered(state, input, slot)) {
            draw_fill(ui, slot, 0xFFFFFF, 0.0f, 0.05f);
        }
        draw_icon(ui, kSidebarIcons[i],
                  Rect{slot.x + (slot.w - 18.0f * scale) * 0.5f, slot.y + (slot.h - 18.0f * scale) * 0.5f, 18.0f * scale, 18.0f * scale},
                  selected ? 0x171A28 : palette.text, selected ? 1.0f : 0.90f);
        ui.shape().in(Rect{slot.x, slot.y + slot.h - 1.0f, slot.w, 1.0f}).fill(palette.divider, 1.0f).draw();
    }

    const Rect gear_slot{rect.x, rect.y + rect.h - slot_h * 2.0f, rect.w, slot_h};
    const Rect logout_slot{rect.x, rect.y + rect.h - slot_h, rect.w, slot_h};
    if (interactive_clicked(state, input, gear_slot)) {
        if (!state.settings_open) {
            ui.reset_motion("dashboard/settings-panel", 0.0f);
        }
        state.settings_open = !state.settings_open;
        state.open_menu = OpenMenu::None;
    }
    const bool gear_hovered = interactive_hovered(state, input, gear_slot);
    if (state.settings_open || gear_hovered) {
        draw_fill(ui, gear_slot, state.settings_open ? palette.panel_alt : 0xFFFFFF, 0.0f,
                  state.settings_open ? 0.92f : 0.06f);
    }
    ui.shape().in(Rect{gear_slot.x, gear_slot.y, gear_slot.w, 1.0f}).fill(palette.divider, 1.0f).draw();
    draw_icon(ui, 0xF013u, Rect{gear_slot.x + (gear_slot.w - 18.0f * scale) * 0.5f, gear_slot.y + (gear_slot.h - 18.0f * scale) * 0.5f,
                                18.0f * scale, 18.0f * scale},
              state.settings_open ? palette.accent_bright : palette.text, state.settings_open ? 1.0f : 0.88f);
    draw_icon(ui, 0xF2F5u, Rect{logout_slot.x + (logout_slot.w - 18.0f * scale) * 0.5f, logout_slot.y + (logout_slot.h - 18.0f * scale) * 0.5f,
                                18.0f * scale, 18.0f * scale}, palette.text, 0.88f);
}

void draw_header(UI& ui, const Rect& rect, float scale, DashboardState& state, const Palette& palette, float sidebar_w, const PageCopy& page) {
    draw_fill(ui, rect, palette.panel, 0.0f, 1.0f);
    ui.shape().in(Rect{rect.x, rect.y + rect.h - 1.0f, rect.w, 1.0f}).fill(palette.divider, 1.0f).draw();
    ui.shape().in(Rect{rect.x + sidebar_w, rect.y, 1.0f, rect.h}).fill(palette.divider, 1.0f).draw();

    const float profile_w = 250.0f * scale;
    const float bell_w = 62.0f * scale;
    const Rect bell_rect{rect.x + rect.w - profile_w - bell_w, rect.y, bell_w, rect.h};
    const Rect profile_rect{rect.x + rect.w - profile_w, rect.y, profile_w, rect.h};

    ui.shape().in(Rect{bell_rect.x, bell_rect.y, 1.0f, bell_rect.h}).fill(palette.divider, 1.0f).draw();
    ui.shape().in(Rect{profile_rect.x, profile_rect.y, 1.0f, profile_rect.h}).fill(palette.divider, 1.0f).draw();

    const Rect badge_rect{rect.x + sidebar_w + 18.0f * scale, rect.y + 12.0f * scale, 124.0f * scale, 32.0f * scale};
    ui.shape().in(badge_rect).radius(badge_rect.h * 0.5f).fill(palette.panel_alt, 1.0f).stroke(palette.divider, 1.0f, 1.0f).draw();
    draw_text_center(ui, page.badge, badge_rect, 11.2f * scale, palette.text, 0.96f);

    const Rect search_rect{badge_rect.x + badge_rect.w + 14.0f * scale, rect.y + 10.0f * scale, 206.0f * scale, 36.0f * scale};
    draw_icon(ui, 0xF002u, Rect{search_rect.x + 14.0f * scale, search_rect.y + (search_rect.h - 16.0f * scale) * 0.5f,
                                16.0f * scale, 16.0f * scale},
              palette.text, 0.80f);
    ui.scope(Rect{search_rect.x + 34.0f * scale, search_rect.y, search_rect.w - 34.0f * scale, search_rect.h}, [&](auto&) {
        ui.input("", state.search_text).height(search_rect.h).placeholder("Search...").draw();
    });
    draw_icon(ui, 0xF0F3u, Rect{bell_rect.x + (bell_rect.w - 18.0f * scale) * 0.5f, bell_rect.y + (bell_rect.h - 18.0f * scale) * 0.5f,
                                18.0f * scale, 18.0f * scale},
              palette.text, 0.92f);
    draw_top_profile(ui, profile_rect, scale, palette);
}

void draw_metric_card(UI& ui, const Rect& rect, const MetricCard& card, bool highlighted, float scale, const Palette& palette) {
    draw_fill(ui, rect, palette.panel, 0.0f, 1.0f);
    draw_stroke(ui, rect, palette.divider, 0.0f, 1.0f, 1.0f);

    if (highlighted) {
        ui.shape()
            .in(rect)
            .gradient(palette.dark ? darken_hex(palette.accent, 0.58f) : lighten_hex(palette.accent, 0.84f),
                      palette.dark ? lighten_hex(palette.accent, 0.24f) : lighten_hex(palette.accent, 0.10f), 1.0f)
            .shadow(0.0f, 14.0f * scale, 32.0f * scale, 0x07101A, 0.28f)
            .draw();
        ui.clip(rect, [&] {
            draw_soft_glow(ui, Rect{rect.x + rect.w * 0.46f, rect.y + rect.h * 0.12f, rect.w * 0.60f, rect.h * 0.88f},
                           palette.accent_bright, 0xFFFFFF, 0.84f, palette.dark ? 0.34f : 0.18f);
            draw_soft_glow(ui, Rect{rect.x + rect.w * 0.02f, rect.y + rect.h * 0.18f, rect.w * 0.54f, rect.h * 0.74f},
                           palette.accent, palette.dark ? palette.shell : 0xFFFFFF, 0.84f, palette.dark ? 0.32f : 0.14f);
        });
    }

    const Rect title_rect{rect.x + 26.0f * scale, rect.y + 26.0f * scale, rect.w - 124.0f * scale, 18.0f * scale};
    const Rect value_rect{rect.x + 26.0f * scale, rect.y + rect.h - 64.0f * scale, 98.0f * scale, 40.0f * scale};
    const Rect delta_rect{value_rect.x + value_rect.w + 10.0f * scale, value_rect.y + 12.0f * scale,
                          rect.w - value_rect.w - 126.0f * scale, 16.0f * scale};
    const Rect circle{rect.x + rect.w - 58.0f * scale, rect.y + 24.0f * scale, 28.0f * scale, 28.0f * scale};

    draw_fill(ui, circle, highlighted ? (palette.dark ? 0xFFFFFF : palette.accent) : palette.panel_alt,
              circle.w * 0.5f, highlighted ? 0.96f : 0.92f);
    draw_icon(ui, 0xF061u, Rect{circle.x + 6.0f * scale, circle.y + 6.0f * scale, 16.0f * scale, 16.0f * scale},
              highlighted ? (palette.dark ? 0x1B2032 : 0xFFFFFF) : palette.text, 0.96f);
    draw_text_left(ui, card.title, title_rect, 11.8f * scale, palette.text, 0.96f);
    draw_text_left(ui, card.value, value_rect, 29.0f * scale, palette.text, 1.0f);
    draw_text_left(ui, card.delta, delta_rect, 11.8f * scale, palette.text, 0.95f);
}

void draw_filter_chip(UI& ui, const eui::InputState& input, DashboardState& state, int filter_index, Rect rect, float scale,
                      const Palette& palette) {
    const bool selected = state.filter_index == filter_index;
    if (interactive_clicked(state, input, rect)) {
        state.filter_index = filter_index;
    }

    const std::size_t index = static_cast<std::size_t>(filter_index);
    const float active_mix =
        ui.presence(ui.id("dashboard/filter-chip", static_cast<std::uint64_t>(index)), selected, 18.0f, 18.0f);
    const Rect visual{rect.x, rect.y - active_mix * 2.0f * scale, rect.w, rect.h};
    const float radius = visual.h * 0.5f;
    draw_fill(ui, visual, palette.chip, radius, 1.0f);
    if (active_mix > 0.01f) {
        ui.shape()
            .in(visual)
            .radius(radius)
            .fill(palette.chip_active, active_mix)
            .shadow(0.0f, 2.0f * scale, 6.0f * scale, 0x040913, active_mix * 0.07f)
            .draw();
    } else {
        draw_stroke(ui, visual, palette.divider, radius, 1.0f, 1.0f);
    }

    const float count_w = std::max(visual.h - 8.0f * scale, ui.measure_text(kFilterCounts[index], 11.0f * scale) + 14.0f * scale);
    const Rect count_rect{visual.x + visual.w - count_w - 6.0f * scale, visual.y + 4.0f * scale, count_w, visual.h - 8.0f * scale};
    draw_fill(ui, count_rect,
              selected ? (palette.dark ? 0x171A28 : darken_hex(palette.accent, 0.18f)) : (palette.dark ? 0xF8F8FB : 0xFFFFFF),
              count_rect.h * 0.5f, 1.0f);
    draw_text_center(ui, kFilterCounts[index], count_rect, 11.0f * scale,
                     selected ? 0xF5F6FB : (palette.dark ? 0x171A28 : palette.text), 1.0f);
    draw_text_left(ui, kFilterLabels[index],
                   Rect{visual.x + 16.0f * scale, visual.y + 9.0f * scale, visual.w - count_rect.w - 24.0f * scale, 16.0f * scale},
                   11.0f * scale, selected ? palette.chip_active_text : palette.text, 0.98f);
}

void draw_status_pill(UI& ui, const Rect& rect, const OrderRow& row, float scale, const Palette& palette) {
    const float radius = rect.h * 0.5f;
    if (row.in_transit) {
        draw_fill(ui, rect, palette.pill_blue, radius, 1.0f);
        draw_text_center(ui, row.status, rect, 11.5f * scale, palette.pill_blue_text, 1.0f);
    } else {
        draw_fill(ui, rect, palette.pill_dark, radius, 0.92f);
        draw_text_center(ui, row.status, rect, 11.0f * scale, palette.text, 1.0f);
    }
}

void draw_order_row(UI& ui, const eui::InputState& input, DashboardState& state, int index, const Rect& rect, const OrderRow& row,
                    float scale, const Palette& palette) {
    if (interactive_clicked(state, input, rect)) {
        state.selected_order = index;
    }

    const bool selected = state.selected_order == index;
    const std::size_t row_index = static_cast<std::size_t>(index);
    const float selected_mix =
        ui.presence(ui.id("dashboard/order-row", static_cast<std::uint64_t>(row_index)), selected, 10.0f, 10.0f);
    const Rect visual = rect;
    if (selected_mix > 0.01f) {
        draw_fill(ui, visual, palette.panel_alt, 14.0f * scale, selected_mix * 0.92f);
        ui.clip(visual, [&] {
            draw_soft_glow(ui, Rect{visual.x + visual.w * 0.10f, visual.y - visual.h * 0.20f, visual.w * 0.60f, visual.h * 1.40f},
                           palette.accent, palette.dark ? 0x151926 : 0xFFFFFF, 0.82f, 0.22f * selected_mix);
        });
    }
    ui.shape().in(Rect{visual.x, visual.y, visual.w, 1.0f}).fill(palette.divider, 1.0f).draw();

    const Rect radio_outer{visual.x + 20.0f * scale, visual.y + 21.0f * scale, 14.0f * scale, 14.0f * scale};
    if (selected_mix > 0.01f) {
        const Rect radio_halo = inset_rect(radio_outer, -1.2f * scale * selected_mix);
        draw_fill(ui, radio_halo, palette.accent, radio_halo.w * 0.5f, 0.045f * selected_mix);
    }
    draw_fill(ui, radio_outer, palette.dark ? 0x161C2A : 0xFFFFFF, radio_outer.w * 0.5f, 1.0f);
    draw_stroke(ui, radio_outer, selected_mix > 0.12f ? palette.accent_bright : palette.divider,
                radio_outer.w * 0.5f, 1.0f + selected_mix * 0.8f, 1.0f);
    if (selected_mix > 0.01f) {
        const float inset = std::max(1.9f * scale, 5.0f * scale - selected_mix * 2.6f * scale);
        const Rect radio_inner = inset_rect(radio_outer, inset);
        draw_fill(ui, radio_inner, palette.accent_bright, radio_inner.w * 0.5f, 0.96f * selected_mix);
        const Rect radio_core = inset_rect(radio_inner, 1.3f * scale);
        draw_fill(ui, radio_core, palette.dark ? 0xFFFFFF : lighten_hex(palette.accent_bright, 0.16f),
                  radio_core.w * 0.5f, 0.36f * selected_mix);
    }

    draw_text_left(ui, row.id, Rect{visual.x + 46.0f * scale, visual.y + 20.0f * scale, visual.w * 0.16f, 20.0f * scale},
                   12.0f * scale, palette.text, 0.96f);
    draw_text_left(ui, row.address, Rect{visual.x + visual.w * 0.23f, visual.y + 20.0f * scale, visual.w * 0.41f, 20.0f * scale},
                   12.0f * scale, palette.text, 0.94f);
    draw_text_left(ui, row.expires, Rect{visual.x + visual.w * 0.64f, visual.y + 20.0f * scale, visual.w * 0.10f, 20.0f * scale},
                   12.0f * scale, palette.text, 0.94f);
    draw_status_pill(ui, Rect{visual.x + visual.w - 126.0f * scale, visual.y + 12.0f * scale, 104.0f * scale, 34.0f * scale},
                     row, scale, palette);
}

void draw_orders_panel(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale,
                       const Palette& palette, std::string_view title) {
    draw_fill(ui, rect, palette.panel, 0.0f, 1.0f);
    draw_stroke(ui, rect, palette.divider, 0.0f, 1.0f, 1.0f);

    const Rect inner = inset_rect(rect, 26.0f * scale);
    draw_text_left(ui, title, Rect{inner.x, inner.y + 8.0f * scale, 220.0f * scale, 28.0f * scale}, 23.0f * scale, palette.text, 1.0f);
    draw_dropdown_control(ui, input, Rect{inner.x + inner.w - 128.0f * scale, inner.y, 128.0f * scale, 42.0f * scale},
                          OpenMenu::Orders, state.open_menu, state.orders_range, kOrdersRanges, scale, state, palette);

    const float chip_y = inner.y + 56.0f * scale;
    float chip_x = inner.x;
    for (int i = 0; i < 4; ++i) {
        const float w = ui.measure_text(kFilterLabels[static_cast<std::size_t>(i)], 11.0f * scale) + 52.0f * scale;
        const float count_w = std::max(24.0f * scale, ui.measure_text(kFilterCounts[static_cast<std::size_t>(i)], 11.0f * scale) + 14.0f * scale);
        const Rect chip{chip_x, chip_y, w + count_w, 34.0f * scale};
        draw_filter_chip(ui, input, state, i, chip, scale, palette);
        chip_x += chip.w + 10.0f * scale;
    }

    const float header_y = chip_y + 54.0f * scale;
    draw_text_left(ui, "Order ID", Rect{inner.x + 24.0f * scale, header_y, 110.0f * scale, 16.0f * scale}, 11.0f * scale, palette.muted, 0.95f);
    draw_text_left(ui, "Delivery address", Rect{inner.x + inner.w * 0.23f, header_y, 180.0f * scale, 16.0f * scale}, 11.0f * scale, palette.muted, 0.95f);
    draw_text_left(ui, "Expires", Rect{inner.x + inner.w * 0.64f, header_y, 80.0f * scale, 16.0f * scale}, 11.0f * scale, palette.muted, 0.95f);
    draw_text_left(ui, "Status", Rect{inner.x + inner.w - 126.0f * scale, header_y, 80.0f * scale, 16.0f * scale}, 11.0f * scale, palette.muted, 0.95f);
    ui.shape().in(Rect{inner.x, header_y + 24.0f * scale, inner.w, 1.0f}).fill(palette.divider, 1.0f).draw();

    const float row_h = 74.0f * scale;
    float row_y = header_y + 26.0f * scale;
    for (std::size_t i = 0; i < kOrders.size(); ++i) {
        draw_order_row(ui, input, state, static_cast<int>(i), Rect{inner.x, row_y, inner.w, row_h}, kOrders[i], scale, palette);
        row_y += row_h;
    }
}

void draw_activity_panel(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale,
                         const Palette& palette, std::string_view title) {
    draw_fill(ui, rect, palette.panel, 0.0f, 1.0f);
    draw_stroke(ui, rect, palette.divider, 0.0f, 1.0f, 1.0f);
    ui.clip(rect, [&] {
        const Rect haze_a{rect.x + rect.w * 0.04f, rect.y + rect.h * 0.12f, rect.w * 0.36f, rect.h * 0.46f};
        const Rect haze_b{rect.x + rect.w * 0.28f, rect.y + rect.h * 0.08f, rect.w * 0.42f, rect.h * 0.54f};
        const Rect haze_c{rect.x + rect.w * 0.60f, rect.y + rect.h * 0.10f, rect.w * 0.24f, rect.h * 0.40f};
        draw_fill(ui, haze_a, 0xFFFFFF, haze_a.h * 0.5f, 0.038f);
        draw_fill(ui, haze_b, 0xD9E2FF, haze_b.h * 0.5f, 0.032f);
        draw_fill(ui, haze_c, 0xAFC0FF, haze_c.h * 0.5f, 0.026f);
        draw_fill(ui, Rect{rect.x + rect.w * 0.18f, rect.y + rect.h * 0.58f, rect.w * 0.22f, rect.h * 0.02f}, 0xFFFFFF, 8.0f * scale, 0.05f);
        draw_fill(ui, Rect{rect.x + rect.w * 0.48f, rect.y + rect.h * 0.52f, rect.w * 0.26f, rect.h * 0.02f}, 0xD7E1FF, 8.0f * scale, 0.04f);
    });

    ui.shape()
        .in(inset_rect(rect, 1.0f))
        .radius(0.0f)
        .blur(12.0f * scale, 12.0f * scale)
        .fill(0xF7F9FF, palette.dark ? 0.040f : 0.085f)
        .stroke(0xFFFFFF, 1.0f, palette.dark ? 0.055f : 0.18f)
        .draw();
    ui.shape()
        .in(inset_rect(rect, 1.0f))
        .radius(0.0f)
        .fill(palette.dark ? 0x0B1020 : 0xFFFFFF, palette.dark ? 0.155f : 0.48f)
        .draw();
    ui.shape()
        .in(inset_rect(rect, 8.0f * scale))
        .radius(0.0f)
        .fill(palette.dark ? 0x0D1322 : lighten_hex(palette.accent, 0.90f), palette.dark ? 0.060f : 0.34f)
        .draw();

    const Rect inner = inset_rect(rect, 28.0f * scale, 22.0f * scale);
    draw_text_left(ui, title, Rect{inner.x, inner.y, 180.0f * scale, 28.0f * scale}, 22.0f * scale, palette.text, 1.0f);
    draw_dropdown_control(ui, input, Rect{inner.x + inner.w - 134.0f * scale, inner.y - 2.0f * scale, 134.0f * scale, 42.0f * scale},
                          OpenMenu::Activity, state.open_menu, state.activity_range, kActivityRanges, scale, state, palette);

    const Rect chart_rect = inset_rect(Rect{inner.x, inner.y + 44.0f * scale, inner.w, inner.h - 44.0f * scale},
                                       0.0f, 18.0f * scale, 0.0f, 16.0f * scale);
    const Rect plot_rect{chart_rect.x + 34.0f * scale, chart_rect.y, chart_rect.w - 34.0f * scale, chart_rect.h - 26.0f * scale};
    const float max_value = 50.0f;
    const float gap = 10.0f * scale;
    const float bar_w = (plot_rect.w - gap * static_cast<float>(kBarLabels.size() - 1u)) / static_cast<float>(kBarLabels.size());
    const auto& bars = active_bars(state);
    int hovered_index = -1;

    for (int value = 0; value <= 50; value += 10) {
        const float t = static_cast<float>(value) / max_value;
        const float y = plot_rect.y + plot_rect.h - plot_rect.h * t;
        const char* label = value == 0 ? "0" : value == 10 ? "10" : value == 20 ? "20" : value == 30 ? "30" : value == 40 ? "40" : "50";
        draw_text_left(ui, label, Rect{chart_rect.x, y - 8.0f * scale, 24.0f * scale, 16.0f * scale}, 10.5f * scale, palette.muted, 0.74f);
    }

    if (!pointer_captured_by_overlay(state, input) && plot_rect.contains(input.mouse_x, input.mouse_y)) {
        const float relative_x = input.mouse_x - plot_rect.x;
        const float step = bar_w + gap;
        hovered_index = std::clamp(static_cast<int>(relative_x / step), 0, static_cast<int>(bars.size() - 1u));
    }

    float bar_x = plot_rect.x;
    for (std::size_t i = 0; i < bars.size(); ++i) {
        const float h = plot_rect.h * (bars[i] / max_value);
        const Rect bar{bar_x, plot_rect.y + plot_rect.h - h, bar_w, h};
        const bool active = hovered_index == static_cast<int>(i);
        const float base_alpha = (hovered_index >= 0) ? (active ? 1.0f : 0.18f) : (0.42f + 0.024f * static_cast<float>(i));
        const std::uint32_t inactive_bar = palette.dark ? lighten_hex(palette.accent, 0.38f) : lighten_hex(palette.accent, 0.14f);
        const std::uint32_t active_bar = palette.accent;
        if (active) {
            const Rect bar_glow = inset_rect(bar, -4.0f * scale, -6.0f * scale);
            draw_fill(ui, bar_glow, palette.accent, 6.0f * scale, 0.14f);
        }
        draw_fill(ui, bar, active ? active_bar : inactive_bar, 3.0f * scale, base_alpha);
        draw_text_center(ui, kBarLabels[i],
                         Rect{bar.x - gap * 0.25f, plot_rect.y + plot_rect.h + 6.0f * scale, bar.w + gap * 0.5f, 14.0f * scale},
                         9.4f * scale, active ? palette.accent_bright : palette.text, active ? 1.0f : 0.84f);
        bar_x += bar_w + gap;
    }

    state.hovered_bar = hovered_index;
    const std::uint64_t tooltip_alpha_id = ui.id("dashboard/activity-tooltip-alpha");
    const std::uint64_t tooltip_x_id = ui.id("dashboard/activity-tooltip-x");
    const std::uint64_t tooltip_y_id = ui.id("dashboard/activity-tooltip-y");
    const float tooltip_alpha = ui.presence(tooltip_alpha_id, hovered_index >= 0, 18.0f, 12.0f);

    if (hovered_index >= 0) {
        const float hovered_h = plot_rect.h * (bars[static_cast<std::size_t>(hovered_index)] / max_value);
        const float hovered_x = plot_rect.x + static_cast<float>(hovered_index) * (bar_w + gap);
        const float target_center_x = hovered_x + bar_w * 0.5f;
        const float target_anchor_y = plot_rect.y + plot_rect.h - hovered_h;
        if (tooltip_alpha < 0.02f) {
            ui.reset_motion(tooltip_x_id, target_center_x);
            ui.reset_motion(tooltip_y_id, target_anchor_y);
        }
        ui.motion(tooltip_x_id, target_center_x, 18.0f);
        ui.motion(tooltip_y_id, target_anchor_y, 18.0f);
    }

    if (tooltip_alpha > 0.02f) {
        const float tooltip_center_x = ui.motion_value(tooltip_x_id, plot_rect.x + plot_rect.w * 0.5f);
        const float tooltip_anchor_y = ui.motion_value(tooltip_y_id, plot_rect.y + plot_rect.h * 0.5f);
        const float bubble_w = 108.0f * scale;
        const float bubble_h = 36.0f * scale;
        const float bubble_x = std::clamp(tooltip_center_x - bubble_w * 0.5f, plot_rect.x, plot_rect.x + plot_rect.w - bubble_w);
        const float bubble_y = std::clamp(tooltip_anchor_y - bubble_h - 18.0f * scale,
                                          chart_rect.y + 6.0f * scale,
                                          plot_rect.y + plot_rect.h - bubble_h - 8.0f * scale);
        const Rect bubble{bubble_x, bubble_y, bubble_w, bubble_h};
        const float arrow_offset = std::clamp(tooltip_center_x - bubble.x - 6.0f * scale, 16.0f * scale, bubble.w - 16.0f * scale);
        const Rect arrow{bubble.x + arrow_offset, bubble.y + bubble.h - 4.0f * scale, 12.0f * scale, 12.0f * scale};
        const int safe_index = std::clamp(std::max(hovered_index, 0), 0, static_cast<int>(bars.size() - 1u));
        const std::string tooltip_text = std::to_string(static_cast<int>(std::lround(bars[static_cast<std::size_t>(safe_index)]))) + " orders";

        ui.shape()
            .in(bubble)
            .radius(bubble.h * 0.5f)
            .fill(0xFBFBFD, tooltip_alpha * 0.95f)
            .stroke(0xFFFFFF, 1.0f, tooltip_alpha)
            .shadow(0.0f, 4.0f * scale, 10.0f * scale, 0x020617, tooltip_alpha * 0.12f)
            .draw();
        draw_text_center(ui, tooltip_text, bubble, 11.4f * scale, 0x181B2A, tooltip_alpha);
        ui.shape().in(arrow)
            .radius(2.0f * scale)
            .fill(0xFBFBFD, tooltip_alpha * 0.95f)
            .rotate(45.0f)
            .origin_center()
            .draw();
    }
}

void draw_courier_entry(UI& ui, const Rect& rect, const Courier& courier, float scale, const Palette& palette) {
    draw_avatar(ui, Rect{rect.x, rect.y + 4.0f * scale, 40.0f * scale, 40.0f * scale}, courier, scale);
    draw_text_left(ui, courier.name, Rect{rect.x + 56.0f * scale, rect.y + 8.0f * scale, rect.w - 96.0f * scale, 18.0f * scale},
                   12.8f * scale, palette.text, 0.98f);
    draw_text_left(ui, courier.orders, Rect{rect.x + 56.0f * scale, rect.y + 32.0f * scale, rect.w - 96.0f * scale, 16.0f * scale},
                   11.0f * scale, palette.muted, 0.92f);
    draw_icon(ui, 0xF061u, Rect{rect.x + rect.w - 24.0f * scale, rect.y + 14.0f * scale, 16.0f * scale, 16.0f * scale}, palette.text, 0.92f);
}

void draw_couriers_panel(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale,
                         const Palette& palette, std::string_view title) {
    draw_fill(ui, rect, palette.panel, 0.0f, 1.0f);
    draw_stroke(ui, rect, palette.divider, 0.0f, 1.0f, 1.0f);

    const Rect inner = inset_rect(rect, 28.0f * scale, 22.0f * scale);
    draw_text_left(ui, title, Rect{inner.x, inner.y, 220.0f * scale, 28.0f * scale}, 22.0f * scale, palette.text, 1.0f);
    draw_dropdown_control(ui, input, Rect{inner.x + inner.w - 116.0f * scale, inner.y - 4.0f * scale, 116.0f * scale, 40.0f * scale},
                          OpenMenu::Couriers, state.open_menu, state.couriers_range, kCouriersRanges, scale, state, palette);

    const Rect content{inner.x, inner.y + 60.0f * scale, inner.w, inner.h - 60.0f * scale};
    const float gap_x = 26.0f * scale;
    const float gap_y = 18.0f * scale;
    const float cell_w = (content.w - gap_x) * 0.5f;
    const float cell_h = (content.h - gap_y) * 0.5f;

    draw_courier_entry(ui, Rect{content.x, content.y, cell_w, cell_h}, kCouriers[0], scale, palette);
    draw_courier_entry(ui, Rect{content.x + cell_w + gap_x, content.y, cell_w, cell_h}, kCouriers[1], scale, palette);
    draw_courier_entry(ui, Rect{content.x, content.y + cell_h + gap_y, cell_w, cell_h}, kCouriers[2], scale, palette);
    draw_courier_entry(ui, Rect{content.x + cell_w + gap_x, content.y + cell_h + gap_y, cell_w, cell_h}, kCouriers[3], scale, palette);

    ui.shape().in(Rect{content.x, content.y + cell_h + gap_y * 0.5f, content.w, 1.0f}).fill(palette.divider, 1.0f).draw();
    ui.shape().in(Rect{content.x + cell_w + gap_x * 0.5f, content.y, 1.0f, content.h}).fill(palette.divider, 1.0f).draw();
}

Rect settings_panel_rect(const Rect& rect, float scale, float mix) {
    const float panel_w = 336.0f * scale;
    const float panel_h = rect.h - 28.0f * scale;
    const float target_x = rect.x + rect.w - panel_w - 14.0f * scale;
    const float slide = 1.0f - mix;
    return Rect{
        target_x + slide * (panel_w + 28.0f * scale),
        rect.y + 14.0f * scale,
        panel_w,
        panel_h,
    };
}

void draw_settings_panel(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale,
                         const Palette& palette, float panel_mix) {
    panel_mix = std::clamp(panel_mix, 0.0f, 1.0f);
    const Rect panel = settings_panel_rect(rect, scale, panel_mix);
    ui.shape()
        .in(panel)
        .radius(22.0f * scale)
        .opacity(1.0f)
        .blur(26.0f * scale, 26.0f * scale)
        .fill(0xF7FAFF, palette.dark ? 0.07f : 0.18f)
        .stroke(0xFFFFFF, 1.0f, palette.dark ? 0.10f : 0.18f)
        .shadow(0.0f, 6.0f * scale, 14.0f * scale, 0x020617, 0.08f)
        .draw();
    ui.shape()
        .in(inset_rect(panel, 1.0f))
        .radius(21.0f * scale)
        .opacity(1.0f)
        .fill(palette.panel, palette.dark ? 0.78f : 0.84f)
        .stroke(palette.divider, 1.0f, 0.62f)
        .draw();

    ui.ctx().set_global_alpha(1.0f);
    const Rect inner = inset_rect(panel, 24.0f * scale);
    draw_text_left(ui, "Settings", Rect{inner.x, inner.y, inner.w - 44.0f * scale, 28.0f * scale},
                   22.0f * scale, palette.text, 1.0f);
    draw_text_left(ui, "Theme color and daytime appearance", Rect{inner.x, inner.y + 28.0f * scale, inner.w, 18.0f * scale},
                   11.0f * scale, palette.muted, 0.96f);

    const Rect close_rect{panel.x + panel.w - 44.0f * scale, panel.y + 18.0f * scale, 28.0f * scale, 28.0f * scale};
    if (clicked(input, close_rect)) {
        state.settings_open = false;
    }
    ui.shape()
        .in(close_rect)
        .radius(close_rect.w * 0.5f)
        .fill(palette.panel_alt, hovered(input, close_rect) ? 1.0f : 0.92f)
        .stroke(palette.divider, 1.0f, 1.0f)
        .draw();
    draw_icon(ui, 0xF00Du, Rect{close_rect.x + 7.0f * scale, close_rect.y + 7.0f * scale, 14.0f * scale, 14.0f * scale},
              palette.text, 0.92f);

    const Rect appearance_rect{inner.x, inner.y + 72.0f * scale, inner.w, 150.0f * scale};
    draw_fill(ui, appearance_rect, palette.panel_alt, 18.0f * scale, 0.86f);
    draw_stroke(ui, appearance_rect, palette.divider, 18.0f * scale, 1.0f, 1.0f);
    draw_text_left(ui, "Appearance", Rect{appearance_rect.x + 18.0f * scale, appearance_rect.y + 18.0f * scale,
                                          appearance_rect.w - 36.0f * scale, 18.0f * scale},
                   13.0f * scale, palette.text, 0.98f);
    draw_text_left(ui, "Switch between dark and daytime mode", Rect{appearance_rect.x + 18.0f * scale,
                                                                    appearance_rect.y + 40.0f * scale,
                                                                    appearance_rect.w - 36.0f * scale, 16.0f * scale},
                   10.8f * scale, palette.muted, 0.94f);

    const Rect mode_wrap{appearance_rect.x + 18.0f * scale, appearance_rect.y + 74.0f * scale,
                         appearance_rect.w - 36.0f * scale, 46.0f * scale};
    draw_fill(ui, mode_wrap, palette.chip, mode_wrap.h * 0.5f, 1.0f);
    const float mode_gap = 8.0f * scale;
    const float mode_w = (mode_wrap.w - mode_gap) * 0.5f;
    const Rect dark_rect{mode_wrap.x, mode_wrap.y, mode_w, mode_wrap.h};
    const Rect light_rect{mode_wrap.x + mode_w + mode_gap, mode_wrap.y, mode_w, mode_wrap.h};
    if (clicked(input, dark_rect)) {
        state.theme_dark = true;
    }
    if (clicked(input, light_rect)) {
        state.theme_dark = false;
    }
    const auto draw_mode_button = [&](const Rect& button, bool active, std::string_view label, std::uint32_t icon) {
        ui.shape()
            .in(button)
            .radius(button.h * 0.5f)
            .fill(active ? palette.accent : palette.panel, active ? 0.96f : 0.92f)
            .stroke(active ? palette.accent_bright : palette.divider, 1.0f, active ? 0.55f : 1.0f)
            .draw();
        draw_icon(ui, icon, Rect{button.x + 14.0f * scale, button.y + 14.0f * scale, 16.0f * scale, 16.0f * scale},
                  active ? palette.chip_active_text : palette.text, 0.96f);
        draw_text_left(ui, label, Rect{button.x + 36.0f * scale, button.y + 13.0f * scale, button.w - 48.0f * scale, 18.0f * scale},
                       11.5f * scale, active ? palette.chip_active_text : palette.text, 0.98f);
    };
    draw_mode_button(dark_rect, state.theme_dark, "Dark", 0xF186u);
    draw_mode_button(light_rect, !state.theme_dark, "Day", 0xF185u);

    const Rect accent_rect{inner.x, appearance_rect.y + appearance_rect.h + 16.0f * scale, inner.w, 240.0f * scale};
    draw_fill(ui, accent_rect, palette.panel_alt, 18.0f * scale, 0.86f);
    draw_stroke(ui, accent_rect, palette.divider, 18.0f * scale, 1.0f, 1.0f);
    draw_text_left(ui, "Theme Color", Rect{accent_rect.x + 18.0f * scale, accent_rect.y + 18.0f * scale,
                                           accent_rect.w - 36.0f * scale, 18.0f * scale},
                   13.0f * scale, palette.text, 0.98f);
    draw_text_left(ui, "Pick a primary accent", Rect{accent_rect.x + 18.0f * scale, accent_rect.y + 40.0f * scale,
                                                     accent_rect.w - 36.0f * scale, 16.0f * scale},
                   10.8f * scale, palette.muted, 0.94f);

    const float swatch_gap = 12.0f * scale;
    const float cell_w = (accent_rect.w - 36.0f * scale - swatch_gap * 3.0f) / 4.0f;
    const float swatch_size = 36.0f * scale;
    for (std::size_t i = 0; i < kAccentThemes.size(); ++i) {
        const std::size_t row = i / 4u;
        const std::size_t col = i % 4u;
        const Rect cell{
            accent_rect.x + 18.0f * scale + static_cast<float>(col) * (cell_w + swatch_gap),
            accent_rect.y + 74.0f * scale + static_cast<float>(row) * (swatch_size + 18.0f * scale),
            cell_w,
            swatch_size,
        };
        const Rect swatch{
            cell.x + (cell.w - swatch_size) * 0.5f,
            cell.y,
            swatch_size,
            swatch_size,
        };
        if (clicked(input, swatch)) {
            state.accent_index = static_cast<int>(i);
        }
        const bool selected = state.accent_index == static_cast<int>(i);
        const std::uint32_t swatch_color = kAccentThemes[i].accent;
        ui.shape()
            .in(swatch)
            .radius(swatch.w * 0.5f)
            .fill(swatch_color, 1.0f)
            .stroke(selected ? lighten_hex(swatch_color, 0.42f) : palette.divider,
                    selected ? 2.0f : 1.0f, 1.0f)
            .draw();
        if (selected) {
            draw_fill(ui, Rect{swatch.x + 10.0f * scale, swatch.y + 10.0f * scale, swatch.w - 20.0f * scale,
                               swatch.h - 20.0f * scale},
                      palette.dark ? 0xFFFFFF : 0x182033, (swatch.w - 20.0f * scale) * 0.5f, 0.92f);
        }
    }
    ui.ctx().set_global_alpha(1.0f);
}

void draw_dashboard(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, float scale,
                    const Palette& palette) {
    const PageCopy& page = page_copy(state.nav);
    draw_fill(ui, rect, palette.shell, 10.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.divider_strong, 10.0f * scale, 1.0f, 1.0f);

    const float header_h = 56.0f * scale;
    const float sidebar_w = 58.0f * scale;
    const Rect header_rect{rect.x, rect.y, rect.w, header_h};
    const Rect body_rect{rect.x, rect.y + header_h, rect.w, rect.h - header_h};
    const Rect sidebar_rect{body_rect.x, body_rect.y, sidebar_w, body_rect.h};
    const Rect main_rect{body_rect.x + sidebar_w, body_rect.y, body_rect.w - sidebar_w, body_rect.h};

    draw_header(ui, header_rect, scale, state, palette, sidebar_w, page);
    draw_sidebar(ui, input, state, sidebar_rect, scale, palette);

    const float settings_mix = ui.presence("dashboard/settings-panel", state.settings_open, 7.0f, 7.4f);
    if (settings_mix > 0.01f) {
        set_overlay_block(state, Rect{rect.x, rect.y, rect.w, rect.h});
    }

    const float reveal = smoothstep01(ui.motion("dashboard/page-reveal", 1.0f, 8.0f));
    const float reveal_offset_x = (1.0f - reveal) * 26.0f * scale;
    const float reveal_offset_y = (1.0f - reveal) * 10.0f * scale;
    ui.ctx().set_global_alpha(reveal);
    ui.clip(main_rect, [&] {
        const Rect animated_main{main_rect.x + reveal_offset_x, main_rect.y + reveal_offset_y, main_rect.w, main_rect.h};
        const auto& metrics = page_metrics(state.nav);
        const float metric_h = 120.0f * scale;
        const float card_w = animated_main.w * 0.25f;
        for (std::size_t i = 0; i < metrics.size(); ++i) {
            draw_metric_card(ui, Rect{animated_main.x + card_w * static_cast<float>(i), animated_main.y, card_w, metric_h},
                             metrics[i], static_cast<int>(i) == page.highlighted_metric, scale, palette);
        }

        const Rect lower_rect{animated_main.x, animated_main.y + metric_h, animated_main.w, animated_main.h - metric_h};
        switch (state.nav) {
            case NavItem::Deliveries: {
                const float left_w = lower_rect.w * 0.62f;
                const Rect activity_rect{lower_rect.x, lower_rect.y, left_w, lower_rect.h};
                const Rect side_rect{lower_rect.x + left_w, lower_rect.y, lower_rect.w - left_w, lower_rect.h};
                const float top_h = side_rect.h * 0.54f;
                draw_activity_panel(ui, input, state, activity_rect, scale, palette, page.activity_title);
                draw_orders_panel(ui, input, state, Rect{side_rect.x, side_rect.y, side_rect.w, top_h}, scale, palette, page.orders_title);
                draw_couriers_panel(ui, input, state, Rect{side_rect.x, side_rect.y + top_h, side_rect.w, side_rect.h - top_h},
                                    scale, palette, page.couriers_title);
                break;
            }
            case NavItem::Documents: {
                const float top_h = lower_rect.h * 0.56f;
                const Rect top_rect{lower_rect.x, lower_rect.y, lower_rect.w, top_h};
                const Rect bottom_rect{lower_rect.x, lower_rect.y + top_h, lower_rect.w, lower_rect.h - top_h};
                const float split_w = bottom_rect.w * 0.58f;
                draw_orders_panel(ui, input, state, top_rect, scale, palette, page.orders_title);
                draw_activity_panel(ui, input, state, Rect{bottom_rect.x, bottom_rect.y, split_w, bottom_rect.h},
                                    scale, palette, page.activity_title);
                draw_couriers_panel(ui, input, state, Rect{bottom_rect.x + split_w, bottom_rect.y, bottom_rect.w - split_w, bottom_rect.h},
                                    scale, palette, page.couriers_title);
                break;
            }
            case NavItem::Wallet: {
                const float top_h = lower_rect.h * 0.46f;
                const Rect top_rect{lower_rect.x, lower_rect.y, lower_rect.w, top_h};
                const Rect bottom_rect{lower_rect.x, lower_rect.y + top_h, lower_rect.w, lower_rect.h - top_h};
                const float left_w = bottom_rect.w * 0.57f;
                draw_activity_panel(ui, input, state, top_rect, scale, palette, page.activity_title);
                draw_orders_panel(ui, input, state, Rect{bottom_rect.x, bottom_rect.y, left_w, bottom_rect.h}, scale, palette, page.orders_title);
                draw_couriers_panel(ui, input, state, Rect{bottom_rect.x + left_w, bottom_rect.y, bottom_rect.w - left_w, bottom_rect.h},
                                    scale, palette, page.couriers_title);
                break;
            }
            case NavItem::Analytics: {
                const float chart_h = lower_rect.h * 0.66f;
                const Rect chart_rect{lower_rect.x, lower_rect.y, lower_rect.w, chart_h};
                const Rect bottom_rect{lower_rect.x, lower_rect.y + chart_h, lower_rect.w, lower_rect.h - chart_h};
                const float left_w = bottom_rect.w * 0.52f;
                draw_activity_panel(ui, input, state, chart_rect, scale, palette, page.activity_title);
                draw_orders_panel(ui, input, state, Rect{bottom_rect.x, bottom_rect.y, left_w, bottom_rect.h}, scale, palette, page.orders_title);
                draw_couriers_panel(ui, input, state, Rect{bottom_rect.x + left_w, bottom_rect.y, bottom_rect.w - left_w, bottom_rect.h},
                                    scale, palette, page.couriers_title);
                break;
            }
            case NavItem::Dashboard:
            default: {
                const float left_w = lower_rect.w * 0.51f;
                const Rect orders_rect{lower_rect.x, lower_rect.y, left_w, lower_rect.h};
                const Rect right_rect{lower_rect.x + left_w, lower_rect.y, lower_rect.w - left_w, lower_rect.h};
                const float activity_h = right_rect.h * 0.62f;
                draw_orders_panel(ui, input, state, orders_rect, scale, palette, page.orders_title);
                draw_activity_panel(ui, input, state, Rect{right_rect.x, right_rect.y, right_rect.w, activity_h}, scale, palette, page.activity_title);
                draw_couriers_panel(ui, input, state, Rect{right_rect.x, right_rect.y + activity_h, right_rect.w, right_rect.h - activity_h},
                                    scale, palette, page.couriers_title);
                break;
            }
        }
    });
    ui.ctx().set_global_alpha(1.0f);

    if (settings_mix > 0.01f) {
        draw_fill(ui, rect, 0x020617, 10.0f * scale, 0.10f * smoothstep01(settings_mix));
        draw_settings_panel(ui, input, state, rect, scale, palette, settings_mix);
    }
}

}  // namespace

int main() {
    DashboardState state{};

    eui::app::AppOptions options{};
    options.title = "EUI Reference Dashboard Demo";
    options.width = 1200;
    options.height = 800;
    options.vsync = true;
    options.continuous_render = false;
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
            const float scale = std::min(framebuffer_w / 1440.0f, framebuffer_h / 900.0f);
            const float margin = 18.0f * scale;
            const Palette palette = make_palette(state.theme_dark, state.accent_index);
            const std::uint32_t accent_hex =
                kAccentThemes[static_cast<std::size_t>(std::clamp(state.accent_index, 0, static_cast<int>(kAccentThemes.size() - 1)))].accent;

            ctx.set_theme_mode(state.theme_dark ? eui::ThemeMode::Dark : eui::ThemeMode::Light);
            ctx.set_primary_color(
                eui::rgba(static_cast<float>((accent_hex >> 16u) & 0xffu) / 255.0f,
                          static_cast<float>((accent_hex >> 8u) & 0xffu) / 255.0f,
                          static_cast<float>(accent_hex & 0xffu) / 255.0f, 1.0f));
            ctx.set_corner_radius(10.0f * scale);

            state.overlay_block_active = false;
            state.overlay_block_rect = Rect{};
            UI ui(ctx);
            draw_dashboard(ui, input, state, Rect{margin, margin, framebuffer_w - margin * 2.0f, framebuffer_h - margin * 2.0f}, scale, palette);

            if (ctx.consume_repaint_request()) {
                frame.request_next_frame();
            }
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
