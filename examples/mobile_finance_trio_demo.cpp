#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

using eui::Rect;
using eui::quick::UI;

constexpr float kPi = 3.14159265358979323846f;

enum class OpenMenu : int {
    None = 0,
    Account = 1,
    Notifications = 2,
    AnalyticsRange = 3,
    AnalyticsMore = 4,
    Amount = 5,
    Category = 6,
    Date = 7,
    Attachment = 8,
};

struct Palette {
    std::uint32_t background{};
    std::uint32_t background_alt{};
    std::uint32_t phone_shell{};
    std::uint32_t phone_border{};
    std::uint32_t phone_inner{};
    std::uint32_t card{};
    std::uint32_t card_alt{};
    std::uint32_t card_soft{};
    std::uint32_t field{};
    std::uint32_t field_alt{};
    std::uint32_t text{};
    std::uint32_t muted{};
    std::uint32_t soft{};
    std::uint32_t white{};
    std::uint32_t blue{};
    std::uint32_t blue_soft{};
    std::uint32_t green{};
    std::uint32_t green_soft{};
    std::uint32_t orange{};
    std::uint32_t pink{};
    std::uint32_t red{};
    std::uint32_t shadow{};
};

struct AccountOption {
    const char* label;
    const char* owner;
    float balance;
    std::uint32_t accent_a;
    std::uint32_t accent_b;
    std::uint32_t accent_c;
};

struct TransactionItem {
    const char* merchant;
    const char* subtitle;
    int icon_kind;
    float amount;
    int category;
    int date_index;
    std::string description;
    bool receipt_attached{false};
};

struct CategoryTile {
    const char* label;
    std::array<float, 3> expense;
    std::array<float, 3> income;
};

struct DashboardState {
    int active_nav{0};
    int active_account{0};
    bool balance_hidden{false};
    bool show_all_transactions{false};
    int last_action{2};
    int selected_transaction{0};
    int analytics_mode{0};
    int analytics_range{0};
    int selected_bar{5};
    int hovered_bar{-1};
    int focused_tile{4};
    float edit_amount{-10.99f};
    int edit_category{0};
    int edit_date{0};
    std::string edit_description{};
    bool receipt_attached{false};
    OpenMenu open_menu{OpenMenu::None};
    std::array<float, 3> account_balances{};
    std::array<TransactionItem, 5> transactions{};
    Rect account_button_rect{};
    Rect bell_button_rect{};
    Rect analytics_range_rect{};
    Rect analytics_more_rect{};
    Rect amount_button_rect{};
    Rect category_button_rect{};
    Rect date_button_rect{};
    Rect attachment_button_rect{};
    Rect overlay_block_rect{};
    bool overlay_block_active{false};
};

static constexpr std::array<AccountOption, 3> kAccounts{{
    {"Main card **** 6510", "Julie Quinn", 7912.03f, 0x7E87FF, 0xF39A58, 0x58CF77},
    {"Savings card **** 4408", "Julie Quinn", 12440.82f, 0x88B3FF, 0xB892FF, 0x6DE7B8},
    {"Travel card **** 1022", "Julie Quinn", 3018.44f, 0x71D0FF, 0xFD9B6A, 0xB0FF82},
}};

static constexpr std::array<const char*, 4> kNavLabels{{"Home", "Analytics", "Messages", "Settings"}};
static constexpr std::array<std::uint32_t, 4> kNavIcons{{0xF009u, 0xF201u, 0xF075u, 0xF013u}};
static constexpr std::array<const char*, 3> kAccountLabels{{"Main card **** 6510", "Savings card **** 4408", "Travel card **** 1022"}};
static constexpr std::array<const char*, 3> kRangeLabels{{"Week", "Month", "Quarter"}};
static constexpr std::array<const char*, 7> kCategoryLabels{{
    "Subscription",
    "Food & Drinks",
    "Transport",
    "Shopping",
    "Health",
    "Entertainment",
    "Other",
}};
static constexpr std::array<const char*, 4> kDateLabels{{"Today", "Tomorrow", "This week", "Next week"}};
static constexpr std::array<const char*, 3> kMoreLabels{{"Reset focus", "Show expenses", "Show income"}};
static constexpr std::array<const char*, 2> kAttachmentLabels{{"Attach receipt", "Remove receipt"}};
static constexpr std::array<int, 6> kTileCategoryMap{{3, 4, 5, 2, 0, 6}};
static constexpr std::array<float, 4> kAmountPresets{{-5.99f, -10.99f, -19.99f, -56.00f}};

static constexpr std::array<std::array<float, 7>, 3> kExpenseBars{{
    {{86.20f, 149.30f, 95.12f, 115.50f, 72.00f, 231.87f, 91.13f}},
    {{480.00f, 620.30f, 522.40f, 605.00f, 438.20f, 780.60f, 688.50f}},
    {{1200.00f, 1330.00f, 1180.00f, 1410.00f, 1090.00f, 1680.00f, 1490.00f}},
}};

static constexpr std::array<std::array<float, 7>, 3> kIncomeBars{{
    {{102.10f, 140.40f, 188.20f, 132.10f, 120.50f, 204.00f, 176.80f}},
    {{680.20f, 702.30f, 760.40f, 714.10f, 730.20f, 818.40f, 774.00f}},
    {{1540.00f, 1600.00f, 1705.00f, 1648.00f, 1720.00f, 1808.00f, 1766.00f}},
}};

static constexpr std::array<CategoryTile, 6> kTiles{{
    {"Shopping", {230.00f, 620.00f, 1450.00f}, {540.00f, 1460.00f, 3580.00f}},
    {"Health", {110.00f, 280.00f, 700.00f}, {220.00f, 520.00f, 1190.00f}},
    {"Entertainment", {56.00f, 132.00f, 360.00f}, {130.00f, 340.00f, 860.00f}},
    {"Transport", {12.99f, 88.00f, 210.00f}, {64.00f, 182.00f, 420.00f}},
    {"Subscriptions", {27.99f, 74.00f, 180.00f}, {94.00f, 210.00f, 510.00f}},
    {"Other", {404.14f, 900.00f, 1980.00f}, {820.00f, 1705.00f, 3920.00f}},
}};

Palette make_palette() {
    return Palette{
        0xC9CCF2,
        0xAEB3EE,
        0x131313,
        0x61636C,
        0x0D0D0D,
        0x2D2C2C,
        0x343333,
        0x424140,
        0x2F2E2E,
        0x3B3A39,
        0xF5F2EE,
        0xB8B3B0,
        0x8B8784,
        0xFFFFFF,
        0x8297FF,
        0xB4C0FF,
        0x73D992,
        0xBCF0CF,
        0xF4A15C,
        0xD078A7,
        0xE46B6B,
        0x151821,
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

std::uint32_t darken_hex(std::uint32_t hex, float amount) {
    return mix_hex(hex, 0x000000, amount);
}

Rect inset_rect(const Rect& rect, float padding) {
    return Rect{
        rect.x + padding,
        rect.y + padding,
        std::max(0.0f, rect.w - padding * 2.0f),
        std::max(0.0f, rect.h - padding * 2.0f),
    };
}

Rect inset_rect(const Rect& rect, float padding_x, float padding_y) {
    return Rect{
        rect.x + padding_x,
        rect.y + padding_y,
        std::max(0.0f, rect.w - padding_x * 2.0f),
        std::max(0.0f, rect.h - padding_y * 2.0f),
    };
}

float font_small(float scale) {
    return 12.0f * scale;
}

float font_body(float scale) {
    return 14.5f * scale;
}

float font_heading(float scale) {
    return 17.0f * scale;
}

float font_display(float scale) {
    return 31.0f * scale;
}

float font_large(float scale) {
    return 58.0f * scale;
}

std::uint64_t overlay_motion_id(UI& ui, OpenMenu menu_id) {
    return ui.id("finance-trio/overlay", static_cast<std::uint64_t>(menu_id));
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

void toggle_menu(UI& ui, DashboardState& state, OpenMenu menu_id) {
    const std::uint64_t motion_id = overlay_motion_id(ui, menu_id);
    if (state.open_menu != menu_id) {
        ui.reset_motion(motion_id, 0.0f);
    }
    state.open_menu = (state.open_menu == menu_id) ? OpenMenu::None : menu_id;
}

void format_currency_text(float value, char* buffer, std::size_t size, bool absolute = false) {
    const float abs_value = std::fabs(value);
    long long dollars = static_cast<long long>(std::floor(abs_value + 0.0001f));
    int cents = static_cast<int>(std::lround((abs_value - static_cast<float>(dollars)) * 100.0f));
    if (cents >= 100) {
        dollars += 1;
        cents -= 100;
    }

    char digits[32]{};
    int index = 0;
    int group = 0;
    do {
        if (group == 3) {
            digits[index++] = ',';
            group = 0;
        }
        digits[index++] = static_cast<char>('0' + (dollars % 10));
        dollars /= 10;
        ++group;
    } while (dollars > 0 && index < static_cast<int>(sizeof(digits) - 1));

    char reversed[32]{};
    for (int i = 0; i < index; ++i) {
        reversed[i] = digits[index - 1 - i];
    }
    reversed[index] = '\0';

    const bool negative = value < 0.0f && !absolute;
    std::snprintf(buffer, size, "%s$%s.%02d", negative ? "-" : "", reversed, cents);
}

float current_balance(const DashboardState& state) {
    return state.account_balances[static_cast<std::size_t>(state.active_account)];
}

float tile_value(const DashboardState& state, int tile_index) {
    const std::size_t safe_tile = static_cast<std::size_t>(std::clamp(tile_index, 0, static_cast<int>(kTiles.size() - 1)));
    const std::size_t safe_range = static_cast<std::size_t>(std::clamp(state.analytics_range, 0, 2));
    return (state.analytics_mode == 0) ? kTiles[safe_tile].expense[safe_range] : kTiles[safe_tile].income[safe_range];
}

const std::array<float, 7>& current_bars(const DashboardState& state) {
    const std::size_t safe_range = static_cast<std::size_t>(std::clamp(state.analytics_range, 0, 2));
    return (state.analytics_mode == 0) ? kExpenseBars[safe_range] : kIncomeBars[safe_range];
}

float bars_total(const DashboardState& state) {
    const auto& bars = current_bars(state);
    float total = 0.0f;
    for (float value : bars) {
        total += value;
    }
    return total;
}

void sync_edit_from_transaction(DashboardState& state) {
    const TransactionItem& tx = state.transactions[static_cast<std::size_t>(state.selected_transaction)];
    state.edit_amount = tx.amount;
    state.edit_category = tx.category;
    state.edit_date = tx.date_index;
    state.edit_description = tx.description;
    state.receipt_attached = tx.receipt_attached;
}

void commit_edit(DashboardState& state) {
    TransactionItem& tx = state.transactions[static_cast<std::size_t>(state.selected_transaction)];
    state.account_balances[static_cast<std::size_t>(state.active_account)] += state.edit_amount - tx.amount;
    tx.amount = state.edit_amount;
    tx.category = state.edit_category;
    tx.date_index = state.edit_date;
    tx.description = state.edit_description;
    tx.receipt_attached = state.receipt_attached;
}

void apply_quick_action(DashboardState& state, int action) {
    state.last_action = action;
    float delta = 0.0f;
    switch (action) {
        case 0:
            delta = -24.90f;
            break;
        case 1:
            delta = 64.30f;
            break;
        case 2:
        default:
            delta = 120.00f;
            break;
    }
    state.account_balances[static_cast<std::size_t>(state.active_account)] += delta;
}

DashboardState make_state() {
    DashboardState state{};
    state.account_balances = {7912.03f, 12440.82f, 3018.44f};
    state.transactions = {{
        {"Netflix", "Subscription", 0, -10.99f, 0, 0, "Netflix monthly subscription payment", true},
        {"Starbucks", "Food & Drinks", 1, -3.29f, 1, 0, "Coffee and breakfast before the meeting", false},
        {"Uber", "Transport", 2, -16.99f, 2, 0, "Ride to the studio downtown", false},
        {"Amazon", "Shopping", 3, -92.99f, 3, 0, "Office accessories and notebook stand", true},
        {"Airbnb", "Other", 4, -42.00f, 6, 1, "Upcoming stay deposit", false},
    }};
    sync_edit_from_transaction(state);
    return state;
}

void draw_fill(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).fill(hex, alpha).draw();
}

void draw_stroke(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float width = 1.0f, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).no_fill().stroke(hex, width, alpha).draw();
}

void draw_text_left(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex, float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).left().color(hex, alpha).draw();
}

void draw_text_center(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex, float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).center().color(hex, alpha).draw();
}

void draw_text_right(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex, float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).right().color(hex, alpha).draw();
}

void draw_icon(UI& ui, std::uint32_t codepoint, const Rect& rect, std::uint32_t hex, float alpha = 1.0f) {
    ui.glyph(codepoint).in(rect).tint(hex, alpha).draw();
}

void draw_soft_glow(UI& ui, const Rect& rect, std::uint32_t inner, std::uint32_t outer, float radius = 0.82f, float alpha = 1.0f) {
    ui.shape().in(rect).radius(std::max(rect.w, rect.h) * 0.5f).radial_gradient(inner, outer, radius, alpha).draw();
}

void draw_segment(UI& ui, float x1, float y1, float x2, float y2, float thickness, std::uint32_t hex, float alpha = 1.0f) {
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    const float angle = std::atan2(dy, dx) * 180.0f / kPi;
    const Rect segment{
        (x1 + x2) * 0.5f - length * 0.5f,
        (y1 + y2) * 0.5f - thickness * 0.5f,
        length,
        thickness,
    };
    ui.shape()
        .in(segment)
        .origin_center()
        .rotate(angle)
        .radius(thickness * 0.5f)
        .fill(hex, alpha)
        .draw();
}

Rect lifted_rect(UI& ui, std::uint64_t id, const Rect& rect, bool active, float scale, float max_lift = 6.0f) {
    const float lift = smoothstep01(ui.presence(id, active, 16.0f, 18.0f)) * max_lift * scale;
    return Rect{rect.x, rect.y - lift, rect.w, rect.h};
}

void paint_card(UI& ui, const Rect& rect, float radius, float scale, const Palette& palette,
                float accent_mix = 0.0f, float alpha = 1.0f, bool glass = false) {
    const std::uint32_t top = mix_hex(palette.card, palette.card_soft, accent_mix * 0.28f);
    const std::uint32_t bottom = mix_hex(palette.card_alt, palette.phone_inner, accent_mix * 0.12f);
    auto shape = ui.shape()
                     .in(rect)
                     .radius(radius)
                     .gradient(top, bottom, alpha)
                     .stroke(mix_hex(palette.card_soft, palette.white, accent_mix * 0.08f), 1.0f, 0.96f * alpha)
                     .shadow(0.0f, 16.0f * scale, 30.0f * scale, palette.shadow, (0.14f + accent_mix * 0.04f) * alpha);
    if (glass) {
        shape.blur(16.0f * scale, 22.0f * scale);
    }
    shape.draw();
}

void draw_wifi_icon(UI& ui, const Rect& rect, float scale, const Palette& palette) {
    const float t = 2.0f * scale;
    draw_segment(ui, rect.x + 2.0f * scale, rect.y + rect.h * 0.72f,
                 rect.x + rect.w * 0.5f, rect.y + rect.h * 0.30f, t, palette.white, 0.94f);
    draw_segment(ui, rect.x + rect.w - 2.0f * scale, rect.y + rect.h * 0.72f,
                 rect.x + rect.w * 0.5f, rect.y + rect.h * 0.30f, t, palette.white, 0.94f);
    draw_fill(ui, Rect{rect.x + rect.w * 0.5f - 2.2f * scale, rect.y + rect.h * 0.72f - 2.2f * scale,
                       4.4f * scale, 4.4f * scale},
              palette.white, 2.2f * scale, 1.0f);
}

void draw_signal_icon(UI& ui, const Rect& rect, float scale, const Palette& palette) {
    const float bar_w = 3.0f * scale;
    const float gap = 2.0f * scale;
    for (int i = 0; i < 4; ++i) {
        const float h = (5.0f + static_cast<float>(i) * 3.0f) * scale;
        draw_fill(ui, Rect{rect.x + static_cast<float>(i) * (bar_w + gap), rect.y + rect.h - h, bar_w, h},
                  palette.white, 1.4f * scale, 0.96f);
    }
}

void draw_battery_icon(UI& ui, const Rect& rect, float scale, const Palette& palette) {
    const Rect body{rect.x, rect.y + 2.0f * scale, rect.w - 4.0f * scale, rect.h - 4.0f * scale};
    draw_stroke(ui, body, palette.white, 4.0f * scale, 1.4f * scale, 0.92f);
    draw_fill(ui, Rect{body.x + 2.2f * scale, body.y + 2.2f * scale, body.w * 0.66f, body.h - 4.4f * scale},
              palette.white, 2.6f * scale, 0.92f);
    draw_fill(ui, Rect{body.x + body.w + 0.6f * scale, body.y + body.h * 0.30f, 3.2f * scale, body.h * 0.40f},
              palette.white, 1.2f * scale, 0.92f);
}

void draw_status_bar(UI& ui, const Rect& rect, float scale, const Palette& palette, double time_seconds) {
    draw_text_left(ui, "9:41",
                   Rect{rect.x + 6.0f * scale, rect.y + 2.0f * scale, 60.0f * scale, 18.0f * scale},
                   font_body(scale), palette.white, 0.98f);

    const Rect island{rect.x + rect.w * 0.5f - 47.0f * scale, rect.y - 2.0f * scale, 94.0f * scale, 28.0f * scale};
    draw_fill(ui, island, 0x070707, island.h * 0.5f, 1.0f);
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(time_seconds * 1.2));
    draw_fill(ui, Rect{island.x + 12.0f * scale, island.y + 10.0f * scale, 5.0f * scale, 5.0f * scale},
              mix_hex(palette.blue, palette.pink, pulse * 0.3f), 2.5f * scale, 0.78f);

    draw_signal_icon(ui, Rect{rect.x + rect.w - 88.0f * scale, rect.y + 2.0f * scale, 18.0f * scale, 14.0f * scale}, scale, palette);
    draw_wifi_icon(ui, Rect{rect.x + rect.w - 62.0f * scale, rect.y + 1.0f * scale, 18.0f * scale, 14.0f * scale}, scale, palette);
    draw_battery_icon(ui, Rect{rect.x + rect.w - 34.0f * scale, rect.y + 1.0f * scale, 24.0f * scale, 14.0f * scale}, scale, palette);
}

Rect draw_phone_shell(UI& ui, const eui::InputState& input, const DashboardState& state, const Rect& rect,
                      int phone_index, float scale, const Palette& palette, double time_seconds) {
    const bool focused = state.active_nav == phone_index;
    const bool hovered_state = interactive_hovered(state, input, rect);
    const float mix = smoothstep01(ui.presence(ui.id("finance-trio/phone", static_cast<std::uint64_t>(phone_index)),
                                               focused || hovered_state, 16.0f, 18.0f));
    const Rect visual = lifted_rect(ui, ui.id("finance-trio/phone-lift", static_cast<std::uint64_t>(phone_index)),
                                    rect, focused || hovered_state, scale, 10.0f);
    draw_soft_glow(ui, Rect{visual.x - 28.0f * scale, visual.y + visual.h - 160.0f * scale,
                            visual.w + 56.0f * scale, 180.0f * scale},
                   (phone_index == 1) ? palette.blue_soft : palette.green_soft,
                   palette.background, 0.88f, 0.12f + mix * 0.08f);
    ui.shape()
        .in(visual)
        .radius(58.0f * scale)
        .gradient(palette.phone_shell, darken_hex(palette.phone_shell, 0.16f), 1.0f)
        .stroke(mix_hex(palette.phone_border, palette.white, mix * 0.12f), 1.2f * scale, 0.94f)
        .shadow(0.0f, 22.0f * scale, 40.0f * scale, palette.shadow, 0.22f + mix * 0.06f)
        .draw();
    const Rect inner = inset_rect(visual, 10.0f * scale);
    draw_fill(ui, inner, palette.phone_inner, 50.0f * scale, 1.0f);

    draw_fill(ui, Rect{visual.x - 3.0f * scale, visual.y + 260.0f * scale, 3.0f * scale, 56.0f * scale},
              palette.phone_border, 1.5f * scale, 0.72f);
    draw_fill(ui, Rect{visual.x - 3.0f * scale, visual.y + 330.0f * scale, 3.0f * scale, 72.0f * scale},
              palette.phone_border, 1.5f * scale, 0.72f);
    draw_fill(ui, Rect{visual.x + visual.w, visual.y + 300.0f * scale, 3.0f * scale, 84.0f * scale},
              palette.phone_border, 1.5f * scale, 0.72f);

    draw_status_bar(ui, Rect{inner.x + 20.0f * scale, inner.y + 18.0f * scale, inner.w - 40.0f * scale, 22.0f * scale},
                    scale, palette, time_seconds);
    draw_fill(ui, Rect{inner.x + inner.w * 0.5f - 64.0f * scale, inner.y + inner.h - 22.0f * scale,
                       128.0f * scale, 5.0f * scale},
              palette.white, 2.5f * scale, 0.94f);
    return Rect{inner.x + 18.0f * scale, inner.y + 56.0f * scale, inner.w - 36.0f * scale, inner.h - 88.0f * scale};
}

void draw_avatar(UI& ui, const Rect& rect, float scale, const Palette& palette) {
    draw_fill(ui, rect, palette.white, rect.w * 0.5f, 1.0f);
    const Rect inner = inset_rect(rect, 2.0f * scale);
    ui.shape().in(inner).radius(inner.w * 0.5f).gradient(0xD9BBA4, 0xC18B65, 1.0f).draw();
    draw_fill(ui, Rect{inner.x + inner.w * 0.17f, inner.y + inner.h * 0.60f, inner.w * 0.66f, inner.h * 0.24f},
              0xFFFFFF, inner.h * 0.12f, 0.98f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.30f, inner.y + inner.h * 0.18f, inner.w * 0.40f, inner.h * 0.38f},
              0xF4CFBB, inner.w * 0.20f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.17f, inner.y + inner.h * 0.06f, inner.w * 0.66f, inner.h * 0.24f},
              0x4E3A30, inner.w * 0.26f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.44f, inner.y + inner.h * 0.45f, inner.w * 0.12f, inner.h * 0.08f},
              0xF2C8AE, inner.w * 0.05f, 1.0f);
}

void draw_holo_surface(UI& ui, const Rect& rect, float scale, const Palette& palette,
                       std::uint32_t accent_a, std::uint32_t accent_b, std::uint32_t accent_c,
                       double time_seconds, bool bright = false) {
    ui.shape()
        .in(rect)
        .radius(26.0f * scale)
        .gradient(0x111111, 0x181818, 1.0f)
        .stroke(mix_hex(palette.white, accent_b, 0.10f), 1.0f, 0.18f)
        .draw();
    ui.clip(rect, [&] {
        const float drift = static_cast<float>(std::sin(time_seconds * 0.45)) * 18.0f * scale;
        draw_soft_glow(ui, Rect{rect.x - 40.0f * scale + drift, rect.y - 60.0f * scale, rect.w * 0.70f, rect.h * 1.12f},
                       accent_a, 0x040404, 0.88f, bright ? 0.58f : 0.42f);
        draw_soft_glow(ui, Rect{rect.x + rect.w * 0.46f - drift * 0.6f, rect.y + rect.h * 0.18f, rect.w * 0.72f, rect.h * 1.10f},
                       accent_b, 0x040404, 0.86f, bright ? 0.52f : 0.38f);
        draw_soft_glow(ui, Rect{rect.x + rect.w * 0.10f, rect.y + rect.h * 0.54f + drift * 0.2f, rect.w * 0.84f, rect.h * 0.76f},
                       accent_c, 0x040404, 0.84f, bright ? 0.44f : 0.30f);
        ui.shape()
            .in(Rect{rect.x - 10.0f * scale, rect.y + rect.h * 0.20f, rect.w * 1.22f, rect.h * 0.44f})
            .origin_center()
            .rotate(-18.0f)
            .radius(36.0f * scale)
            .fill(0xFFFFFF, bright ? 0.16f : 0.10f)
            .blur(12.0f * scale, 12.0f * scale)
            .draw();
    });
}

void draw_round_icon_button(UI& ui, const Rect& rect, std::uint32_t icon, float scale, const Palette& palette,
                            float mix = 0.0f, bool dark = true) {
    ui.shape()
        .in(rect)
        .radius(rect.w * 0.5f)
        .fill(dark ? mix_hex(palette.card_soft, palette.white, mix * 0.06f) : palette.white, dark ? 0.96f : 1.0f)
        .stroke(mix_hex(palette.card_soft, palette.white, mix * 0.12f), 1.0f, dark ? 0.92f : 0.24f)
        .draw();
    draw_icon(ui, icon,
              Rect{rect.x + (rect.w - 16.0f * scale) * 0.5f, rect.y + (rect.h - 16.0f * scale) * 0.5f,
                   16.0f * scale, 16.0f * scale},
              dark ? palette.white : palette.phone_inner, 0.94f);
}

void draw_action_symbol(UI& ui, const Rect& rect, int action, float scale, const Palette& palette, bool dark_symbol) {
    const std::uint32_t color = dark_symbol ? palette.phone_inner : palette.white;
    switch (action) {
        case 0: {
            draw_segment(ui, rect.x + 8.0f * scale, rect.y + rect.h - 8.0f * scale,
                         rect.x + rect.w - 8.0f * scale, rect.y + 8.0f * scale, 2.6f * scale, color, 0.96f);
            draw_segment(ui, rect.x + rect.w - 14.0f * scale, rect.y + 8.0f * scale,
                         rect.x + rect.w - 8.0f * scale, rect.y + 8.0f * scale, 2.6f * scale, color, 0.96f);
            draw_segment(ui, rect.x + rect.w - 8.0f * scale, rect.y + 8.0f * scale,
                         rect.x + rect.w - 8.0f * scale, rect.y + 14.0f * scale, 2.6f * scale, color, 0.96f);
            break;
        }
        case 1: {
            draw_segment(ui, rect.x + rect.w - 8.0f * scale, rect.y + 8.0f * scale,
                         rect.x + 8.0f * scale, rect.y + rect.h - 8.0f * scale, 2.6f * scale, color, 0.96f);
            draw_segment(ui, rect.x + 8.0f * scale, rect.y + rect.h - 8.0f * scale,
                         rect.x + 8.0f * scale, rect.y + rect.h - 14.0f * scale, 2.6f * scale, color, 0.96f);
            draw_segment(ui, rect.x + 8.0f * scale, rect.y + rect.h - 8.0f * scale,
                         rect.x + 14.0f * scale, rect.y + rect.h - 8.0f * scale, 2.6f * scale, color, 0.96f);
            break;
        }
        case 2:
        default: {
            draw_fill(ui, Rect{rect.x + 8.0f * scale, rect.y + rect.h * 0.5f - 1.2f * scale, rect.w - 16.0f * scale, 2.4f * scale},
                      color, 1.2f * scale, 0.96f);
            draw_fill(ui, Rect{rect.x + rect.w * 0.5f - 1.2f * scale, rect.y + 8.0f * scale, 2.4f * scale, rect.h - 16.0f * scale},
                      color, 1.2f * scale, 0.96f);
            break;
        }
    }
}

void draw_merchant_badge(UI& ui, const Rect& rect, int kind, float scale, const Palette& palette) {
    std::uint32_t fill = palette.card_soft;
    const char* label = "N";
    switch (kind) {
        case 0:
            fill = 0x262626;
            label = "N";
            break;
        case 1:
            fill = 0xF3F2EF;
            label = "S";
            break;
        case 2:
            fill = 0x1B1B1B;
            label = "U";
            break;
        case 3:
            fill = 0x1F1F1F;
            label = "a";
            break;
        case 4:
        default:
            fill = 0xD96E6E;
            label = "A";
            break;
    }
    draw_fill(ui, rect, fill, rect.w * 0.5f, 1.0f);
    draw_text_center(ui, label, rect, font_heading(scale), (kind == 1) ? palette.phone_inner : palette.white, 0.98f);
}

void draw_home_screen(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                      float scale, const Palette& palette, double time_seconds) {
    const Rect top_row{rect.x, rect.y + 6.0f * scale, rect.w, 62.0f * scale};
    const Rect avatar_rect{top_row.x + 4.0f * scale, top_row.y + 6.0f * scale, 46.0f * scale, 46.0f * scale};
    draw_avatar(ui, avatar_rect, scale, palette);
    draw_text_left(ui, "Welcome back,",
                   Rect{avatar_rect.x + 60.0f * scale, top_row.y + 6.0f * scale, 170.0f * scale, 16.0f * scale},
                   font_body(scale), palette.muted, 0.96f);
    draw_text_left(ui, "Julie Quinn",
                   Rect{avatar_rect.x + 60.0f * scale, top_row.y + 28.0f * scale, 180.0f * scale, 20.0f * scale},
                   font_heading(scale), palette.white, 0.98f);

    state.bell_button_rect = Rect{top_row.x + top_row.w - 52.0f * scale, top_row.y + 2.0f * scale, 48.0f * scale, 48.0f * scale};
    const bool bell_hovered = interactive_hovered(state, input, state.bell_button_rect);
    const float bell_mix = smoothstep01(ui.presence("finance-trio/bell", bell_hovered || state.open_menu == OpenMenu::Notifications, 18.0f, 16.0f));
    if (interactive_clicked(state, input, state.bell_button_rect)) {
        toggle_menu(ui, state, OpenMenu::Notifications);
    }
    draw_round_icon_button(ui, state.bell_button_rect, 0xF0F3u, scale, palette, bell_mix, true);

    const Rect balance_card{rect.x, rect.y + 84.0f * scale, rect.w, 230.0f * scale};
    draw_holo_surface(ui, balance_card, scale, palette,
                      kAccounts[static_cast<std::size_t>(state.active_account)].accent_a,
                      kAccounts[static_cast<std::size_t>(state.active_account)].accent_b,
                      kAccounts[static_cast<std::size_t>(state.active_account)].accent_c,
                      time_seconds, false);

    state.account_button_rect = Rect{balance_card.x + 8.0f * scale, balance_card.y + 12.0f * scale, 196.0f * scale, 44.0f * scale};
    const bool account_hovered = interactive_hovered(state, input, state.account_button_rect);
    const float account_mix = smoothstep01(ui.presence("finance-trio/account-pill", account_hovered || state.open_menu == OpenMenu::Account, 18.0f, 16.0f));
    if (interactive_clicked(state, input, state.account_button_rect)) {
        toggle_menu(ui, state, OpenMenu::Account);
    }
    ui.shape()
        .in(state.account_button_rect)
        .radius(state.account_button_rect.h * 0.5f)
        .fill(palette.white, 0.98f)
        .stroke(mix_hex(palette.blue_soft, palette.white, account_mix * 0.08f), 1.0f, 0.28f + account_mix * 0.06f)
        .draw();
    draw_text_left(ui, kAccounts[static_cast<std::size_t>(state.active_account)].label,
                   Rect{state.account_button_rect.x + 18.0f * scale, state.account_button_rect.y + 11.0f * scale,
                        state.account_button_rect.w - 38.0f * scale, 18.0f * scale},
                   font_body(scale), palette.phone_inner, 0.98f);
    draw_icon(ui, 0xF078u,
              Rect{state.account_button_rect.x + state.account_button_rect.w - 22.0f * scale, state.account_button_rect.y + 15.0f * scale,
                   12.0f * scale, 12.0f * scale},
              palette.phone_inner, 0.86f);

    const Rect toggle_rect{balance_card.x + balance_card.w - 56.0f * scale, balance_card.y + 14.0f * scale, 40.0f * scale, 24.0f * scale};
    if (interactive_clicked(state, input, toggle_rect)) {
        state.balance_hidden = !state.balance_hidden;
    }
    const float toggle_mix = ui.motion("finance-trio/balance-toggle", state.balance_hidden ? 1.0f : 0.0f, 14.0f);
    draw_fill(ui, toggle_rect, 0xFFFFFF, toggle_rect.h * 0.5f, 0.30f);
    const Rect knob{
        toggle_rect.x + 2.0f * scale + (toggle_rect.w - toggle_rect.h) * (1.0f - toggle_mix),
        toggle_rect.y + 2.0f * scale,
        toggle_rect.h - 4.0f * scale,
        toggle_rect.h - 4.0f * scale,
    };
    draw_fill(ui, knob, palette.white, knob.w * 0.5f, 0.92f);

    draw_text_left(ui, "Total balance",
                   Rect{balance_card.x + 10.0f * scale, balance_card.y + 142.0f * scale, 180.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.white, 0.92f);
    char balance_text[32]{};
    format_currency_text(current_balance(state), balance_text, sizeof(balance_text), false);
    draw_text_left(ui, state.balance_hidden ? "$----.--" : balance_text,
                   Rect{balance_card.x + 10.0f * scale, balance_card.y + 176.0f * scale, balance_card.w - 90.0f * scale, 52.0f * scale},
                   font_large(scale), palette.white, 0.98f);
    const Rect eye_rect{balance_card.x + balance_card.w - 36.0f * scale, balance_card.y + 186.0f * scale, 22.0f * scale, 16.0f * scale};
    if (interactive_clicked(state, input, eye_rect)) {
        state.balance_hidden = !state.balance_hidden;
    }
    draw_icon(ui, 0xF06Eu, eye_rect, palette.white, 0.90f);

    const float button_gap = 10.0f * scale;
    const float button_w = (rect.w - button_gap * 2.0f) / 3.0f;
    const float button_y = balance_card.y + balance_card.h + 18.0f * scale;
    static constexpr std::array<const char*, 3> kActionLabels{{"Send", "Receive", "Top Up"}};
    for (int action = 0; action < 3; ++action) {
        const Rect button{rect.x + static_cast<float>(action) * (button_w + button_gap), button_y, button_w, 104.0f * scale};
        const bool hovered_state = interactive_hovered(state, input, button);
        const bool selected = state.last_action == action;
        const float mix = smoothstep01(ui.presence(ui.id("finance-trio/action", static_cast<std::uint64_t>(action)),
                                                   hovered_state || selected, 18.0f, 16.0f));
        if (interactive_clicked(state, input, button)) {
            apply_quick_action(state, action);
        }
        const bool bright = action == 2;
        ui.shape()
            .in(button)
            .radius(22.0f * scale)
            .fill(bright ? palette.white : mix_hex(palette.card, palette.card_soft, mix * 0.10f), bright ? 1.0f : 0.98f)
            .stroke(mix_hex(palette.card_soft, palette.white, mix * 0.10f), 1.0f, bright ? 0.12f : 0.92f)
            .draw();
        draw_action_symbol(ui, Rect{button.x + button.w * 0.5f - 14.0f * scale, button.y + 22.0f * scale, 28.0f * scale, 28.0f * scale},
                           action, scale, palette, bright);
        draw_text_center(ui, kActionLabels[static_cast<std::size_t>(action)],
                         Rect{button.x, button.y + 58.0f * scale, button.w, 18.0f * scale},
                         font_heading(scale), bright ? palette.phone_inner : palette.white, 0.98f);
    }

    const float list_y = button_y + 126.0f * scale;
    draw_text_left(ui, "Recent Transaction",
                   Rect{rect.x, list_y, rect.w * 0.6f, 26.0f * scale},
                   23.0f * scale, palette.white, 0.98f);
    const Rect view_all{rect.x + rect.w - 72.0f * scale, list_y + 2.0f * scale, 72.0f * scale, 20.0f * scale};
    const bool view_hovered = interactive_hovered(state, input, view_all);
    if (interactive_clicked(state, input, view_all)) {
        state.show_all_transactions = !state.show_all_transactions;
    }
    draw_text_right(ui, state.show_all_transactions ? "Less" : "View all",
                    view_all, font_body(scale), view_hovered ? palette.white : palette.muted, 0.96f);

    const int visible_count = state.show_all_transactions ? 5 : 4;
    for (int i = 0; i < visible_count; ++i) {
        const TransactionItem& tx = state.transactions[static_cast<std::size_t>(i)];
        const Rect row{rect.x, list_y + 52.0f * scale + static_cast<float>(i) * 86.0f * scale, rect.w, 76.0f * scale};
        const bool selected = state.selected_transaction == i;
        const bool row_hovered = interactive_hovered(state, input, row);
        const float mix = smoothstep01(ui.presence(ui.id("finance-trio/tx", static_cast<std::uint64_t>(i)),
                                                   selected || row_hovered, 18.0f, 18.0f));
        if (interactive_clicked(state, input, row)) {
            state.selected_transaction = i;
            sync_edit_from_transaction(state);
            state.active_nav = 2;
        }
        if (mix > 0.01f) {
            draw_fill(ui, row, mix_hex(palette.card, palette.card_soft, selected ? 0.10f : 0.04f + mix * 0.02f), 18.0f * scale, 0.96f);
        }
        draw_merchant_badge(ui, Rect{row.x + 6.0f * scale, row.y + 8.0f * scale, 48.0f * scale, 48.0f * scale}, tx.icon_kind, scale, palette);
        draw_text_left(ui, tx.merchant,
                       Rect{row.x + 68.0f * scale, row.y + 8.0f * scale, row.w - 150.0f * scale, 20.0f * scale},
                       font_heading(scale), palette.white, 0.98f);
        draw_text_left(ui, kCategoryLabels[static_cast<std::size_t>(tx.category)],
                       Rect{row.x + 68.0f * scale, row.y + 34.0f * scale, row.w - 150.0f * scale, 18.0f * scale},
                       font_body(scale), palette.muted, 0.94f);
        char tx_amount[24]{};
        format_currency_text(tx.amount, tx_amount, sizeof(tx_amount), false);
        draw_text_right(ui, tx_amount,
                        Rect{row.x + row.w - 110.0f * scale, row.y + 14.0f * scale, 104.0f * scale, 20.0f * scale},
                        font_heading(scale), palette.white, 0.98f);
    }

    const Rect nav_rect{rect.x + 18.0f * scale, rect.y + rect.h - 84.0f * scale, rect.w - 36.0f * scale, 64.0f * scale};
    draw_fill(ui, nav_rect, palette.card, nav_rect.h * 0.5f, 0.98f);
    const float slot_w = nav_rect.w / 4.0f;
    const float indicator_x = ui.motion("finance-trio/nav-indicator", nav_rect.x + slot_w * static_cast<float>(state.active_nav), 14.0f);
    const Rect indicator{indicator_x + 6.0f * scale, nav_rect.y + 6.0f * scale, slot_w - 12.0f * scale, nav_rect.h - 12.0f * scale};
    draw_fill(ui, indicator, palette.white, indicator.h * 0.5f, state.active_nav < 3 ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) {
        const Rect slot{nav_rect.x + slot_w * static_cast<float>(i), nav_rect.y, slot_w, nav_rect.h};
        if (interactive_clicked(state, input, slot)) {
            state.active_nav = i;
        }
        draw_icon(ui, kNavIcons[static_cast<std::size_t>(i)],
                  Rect{slot.x + (slot.w - 18.0f * scale) * 0.5f, slot.y + (slot.h - 18.0f * scale) * 0.5f,
                       18.0f * scale, 18.0f * scale},
                  (state.active_nav == i && i < 3) ? palette.phone_inner : palette.white, 0.96f);
    }
}

void draw_analytics_screen(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                           float scale, const Palette& palette) {
    const Rect top_row{rect.x, rect.y + 4.0f * scale, rect.w, 56.0f * scale};
    const Rect back_button{top_row.x, top_row.y + 4.0f * scale, 48.0f * scale, 48.0f * scale};
    const bool back_hovered = interactive_hovered(state, input, back_button);
    if (interactive_clicked(state, input, back_button)) {
        state.active_nav = 0;
    }
    draw_round_icon_button(ui, back_button, 0xF060u, scale, palette, back_hovered ? 1.0f : 0.0f, true);
    draw_text_center(ui, "Analytics",
                     Rect{top_row.x + 56.0f * scale, top_row.y + 12.0f * scale, top_row.w - 112.0f * scale, 24.0f * scale},
                     font_heading(scale), palette.white, 0.98f);
    state.analytics_more_rect = Rect{top_row.x + top_row.w - 48.0f * scale, top_row.y + 4.0f * scale, 48.0f * scale, 48.0f * scale};
    const bool more_hovered = interactive_hovered(state, input, state.analytics_more_rect);
    if (interactive_clicked(state, input, state.analytics_more_rect)) {
        toggle_menu(ui, state, OpenMenu::AnalyticsMore);
    }
    draw_round_icon_button(ui, state.analytics_more_rect, 0xF141u, scale, palette,
                           more_hovered || state.open_menu == OpenMenu::AnalyticsMore ? 1.0f : 0.0f, true);

    const Rect segment{rect.x, rect.y + 82.0f * scale, rect.w, 56.0f * scale};
    draw_fill(ui, segment, palette.card, segment.h * 0.5f, 0.98f);
    const float half_w = segment.w * 0.5f;
    const float seg_x = ui.motion("finance-trio/analytics-segment", segment.x + half_w * static_cast<float>(state.analytics_mode), 14.0f);
    draw_fill(ui, Rect{seg_x + 4.0f * scale, segment.y + 4.0f * scale, half_w - 8.0f * scale, segment.h - 8.0f * scale},
              palette.white, (segment.h - 8.0f * scale) * 0.5f, 1.0f);
    if (interactive_clicked(state, input, Rect{segment.x, segment.y, half_w, segment.h})) {
        state.analytics_mode = 0;
    }
    if (interactive_clicked(state, input, Rect{segment.x + half_w, segment.y, half_w, segment.h})) {
        state.analytics_mode = 1;
    }
    draw_text_center(ui, "Expenses", Rect{segment.x, segment.y + 14.0f * scale, half_w, 18.0f * scale},
                     font_heading(scale), state.analytics_mode == 0 ? palette.phone_inner : palette.white, 0.98f);
    draw_text_center(ui, "Income", Rect{segment.x + half_w, segment.y + 14.0f * scale, half_w, 18.0f * scale},
                     font_heading(scale), state.analytics_mode == 1 ? palette.phone_inner : palette.white, 0.98f);

    const Rect chart_card{rect.x, rect.y + 160.0f * scale, rect.w, 398.0f * scale};
    paint_card(ui, chart_card, 28.0f * scale, scale, palette, 0.02f, 1.0f, false);
    char total_text[32]{};
    format_currency_text(bars_total(state), total_text, sizeof(total_text), true);
    draw_text_left(ui, total_text,
                   Rect{chart_card.x + 22.0f * scale, chart_card.y + 18.0f * scale, 200.0f * scale, 38.0f * scale},
                   font_display(scale), palette.white, 0.98f);
    state.analytics_range_rect = Rect{chart_card.x + chart_card.w - 122.0f * scale, chart_card.y + 16.0f * scale, 104.0f * scale, 44.0f * scale};
    const bool range_hovered = interactive_hovered(state, input, state.analytics_range_rect);
    if (interactive_clicked(state, input, state.analytics_range_rect)) {
        toggle_menu(ui, state, OpenMenu::AnalyticsRange);
    }
    ui.shape()
        .in(state.analytics_range_rect)
        .radius(state.analytics_range_rect.h * 0.5f)
        .fill(palette.card_soft, 0.98f)
        .stroke(mix_hex(palette.card_soft, palette.white, range_hovered ? 0.10f : 0.04f), 1.0f, 0.92f)
        .draw();
    draw_text_left(ui, kRangeLabels[static_cast<std::size_t>(state.analytics_range)],
                   Rect{state.analytics_range_rect.x + 16.0f * scale, state.analytics_range_rect.y + 12.0f * scale,
                        state.analytics_range_rect.w - 34.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.white, 0.96f);
    draw_icon(ui, 0xF078u,
              Rect{state.analytics_range_rect.x + state.analytics_range_rect.w - 20.0f * scale, state.analytics_range_rect.y + 15.0f * scale,
                   12.0f * scale, 12.0f * scale},
              palette.white, 0.80f);

    const Rect plot{chart_card.x + 18.0f * scale, chart_card.y + 96.0f * scale, chart_card.w - 36.0f * scale, chart_card.h - 140.0f * scale};
    const auto& bars = current_bars(state);
    float max_value = 1.0f;
    for (float value : bars) {
        max_value = std::max(max_value, value);
    }
    const float bar_gap = 12.0f * scale;
    const float bar_w = (plot.w - bar_gap * 6.0f) / 7.0f;
    int hovered_bar = -1;
    std::array<Rect, 7> bar_rects{};
    for (int i = 0; i < 7; ++i) {
        const float h = (bars[static_cast<std::size_t>(i)] / max_value) * (plot.h - 40.0f * scale);
        const Rect bar{plot.x + static_cast<float>(i) * (bar_w + bar_gap), plot.y + plot.h - h - 28.0f * scale, bar_w, h};
        bar_rects[static_cast<std::size_t>(i)] = bar;
        const bool bar_hovered = interactive_hovered(state, input, Rect{bar.x, plot.y, bar.w, plot.h - 12.0f * scale});
        if (bar_hovered) {
            hovered_bar = i;
        }
        if (bar_hovered && interactive_clicked(state, input, Rect{bar.x, plot.y, bar.w, plot.h - 12.0f * scale})) {
            state.selected_bar = i;
        }
        const bool selected = state.selected_bar == i;
        const float mix = smoothstep01(ui.presence(ui.id("finance-trio/bar", static_cast<std::uint64_t>(i)),
                                                   selected || bar_hovered, 16.0f, 18.0f));
        if (selected) {
            ui.shape()
                .in(Rect{bar.x, bar.y - mix * 3.0f * scale, bar.w, bar.h})
                .radius(16.0f * scale)
                .gradient(palette.blue_soft, palette.blue, 1.0f)
                .shadow(0.0f, 10.0f * scale, 22.0f * scale, palette.blue, 0.16f + mix * 0.04f)
                .draw();
        } else {
            draw_fill(ui, Rect{bar.x, bar.y, bar.w, bar.h}, mix_hex(palette.card_soft, palette.white, mix * 0.06f), 16.0f * scale, 0.98f);
        }
    }
    state.hovered_bar = hovered_bar;

    static constexpr std::array<const char*, 7> kDayLabels{{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"}};
    for (int i = 0; i < 7; ++i) {
        draw_text_center(ui, kDayLabels[static_cast<std::size_t>(i)],
                         Rect{bar_rects[static_cast<std::size_t>(i)].x, plot.y + plot.h - 20.0f * scale, bar_w, 16.0f * scale},
                         font_body(scale), palette.white, 0.94f);
    }

    const int tooltip_index = (hovered_bar >= 0) ? hovered_bar : state.selected_bar;
    const float tooltip_alpha = ui.presence("finance-trio/bar-tooltip", tooltip_index >= 0, 18.0f, 14.0f);
    if (tooltip_index >= 0 && tooltip_alpha > 0.01f) {
        const Rect anchor = bar_rects[static_cast<std::size_t>(tooltip_index)];
        const float x = ui.motion("finance-trio/bar-tip-x", anchor.x + anchor.w * 0.5f, 18.0f);
        const float y = ui.motion("finance-trio/bar-tip-y", anchor.y - 18.0f * scale, 18.0f);
        const Rect bubble{x - 44.0f * scale, y - 16.0f * scale, 88.0f * scale, 34.0f * scale};
        ui.shape()
            .in(bubble)
            .radius(12.0f * scale)
            .fill(palette.white, 0.98f * tooltip_alpha)
            .shadow(0.0f, 8.0f * scale, 18.0f * scale, palette.shadow, 0.14f * tooltip_alpha)
            .draw();
        char value_text[24]{};
        format_currency_text(bars[static_cast<std::size_t>(tooltip_index)], value_text, sizeof(value_text), true);
        draw_text_center(ui, value_text, bubble, font_heading(scale), palette.phone_inner, 0.98f * tooltip_alpha);
    }

    const float grid_y = chart_card.y + chart_card.h + 18.0f * scale;
    const float tile_gap = 12.0f * scale;
    const float tile_w = (rect.w - tile_gap) * 0.5f;
    for (int i = 0; i < 6; ++i) {
        const int row = i / 2;
        const int col = i % 2;
        const Rect tile{
            rect.x + static_cast<float>(col) * (tile_w + tile_gap),
            grid_y + static_cast<float>(row) * (116.0f * scale + tile_gap),
            tile_w,
            116.0f * scale,
        };
        const bool selected = state.focused_tile == i;
        const bool tile_hovered = interactive_hovered(state, input, tile);
        if (interactive_clicked(state, input, tile)) {
            state.focused_tile = i;
            state.edit_category = kTileCategoryMap[static_cast<std::size_t>(i)];
            state.active_nav = 2;
        }
        paint_card(ui, tile, 22.0f * scale, scale, palette, selected ? 0.08f : (tile_hovered ? 0.04f : 0.0f), 1.0f, false);
        draw_text_left(ui, kTiles[static_cast<std::size_t>(i)].label,
                       Rect{tile.x + 16.0f * scale, tile.y + 14.0f * scale, tile.w - 32.0f * scale, 18.0f * scale},
                       font_body(scale), palette.muted, 0.96f);
        char tile_text[24]{};
        format_currency_text(tile_value(state, i), tile_text, sizeof(tile_text), true);
        draw_text_left(ui, tile_text,
                       Rect{tile.x + 16.0f * scale, tile.y + 58.0f * scale, tile.w - 32.0f * scale, 26.0f * scale},
                       font_display(scale), palette.white, 0.98f);
    }
}

void draw_edit_screen(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                      float scale, const Palette& palette, double time_seconds) {
    const Rect amount_card{rect.x, rect.y + 22.0f * scale, rect.w, 282.0f * scale};
    draw_holo_surface(ui, amount_card, scale, palette, palette.blue, palette.orange, palette.green, time_seconds, true);
    char amount_text[24]{};
    format_currency_text(state.edit_amount, amount_text, sizeof(amount_text), true);
    draw_text_center(ui, amount_text,
                     Rect{amount_card.x, amount_card.y + 72.0f * scale, amount_card.w, 72.0f * scale},
                     font_large(scale), palette.white, 0.98f);

    state.amount_button_rect = Rect{amount_card.x + amount_card.w * 0.5f - 92.0f * scale, amount_card.y + 166.0f * scale, 184.0f * scale, 46.0f * scale};
    const bool amount_hovered = interactive_hovered(state, input, state.amount_button_rect);
    if (interactive_clicked(state, input, state.amount_button_rect)) {
        toggle_menu(ui, state, OpenMenu::Amount);
    }
    ui.shape()
        .in(state.amount_button_rect)
        .radius(state.amount_button_rect.h * 0.5f)
        .fill(palette.white, 0.18f)
        .stroke(mix_hex(palette.white, palette.blue_soft, amount_hovered ? 0.10f : 0.04f), 1.0f, 0.20f)
        .blur(8.0f * scale, 8.0f * scale)
        .draw();
    draw_icon(ui, 0xF040u,
              Rect{state.amount_button_rect.x + 14.0f * scale, state.amount_button_rect.y + 14.0f * scale, 16.0f * scale, 16.0f * scale},
              palette.white, 0.92f);
    draw_text_left(ui, "Change amount",
                   Rect{state.amount_button_rect.x + 38.0f * scale, state.amount_button_rect.y + 12.0f * scale,
                        state.amount_button_rect.w - 48.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.white, 0.94f);

    const float label_x = rect.x;
    const float field_w = rect.w;
    const float category_y = amount_card.y + amount_card.h + 42.0f * scale;
    draw_text_left(ui, "Category", Rect{label_x, category_y, field_w, 18.0f * scale}, font_heading(scale), palette.white, 0.98f);
    state.category_button_rect = Rect{label_x, category_y + 28.0f * scale, field_w, 62.0f * scale};
    const bool category_hovered = interactive_hovered(state, input, state.category_button_rect);
    if (interactive_clicked(state, input, state.category_button_rect)) {
        toggle_menu(ui, state, OpenMenu::Category);
    }
    paint_card(ui, state.category_button_rect, 18.0f * scale, scale, palette, category_hovered ? 0.04f : 0.0f, 1.0f, false);
    draw_text_left(ui, kCategoryLabels[static_cast<std::size_t>(state.edit_category)],
                   Rect{state.category_button_rect.x + 18.0f * scale, state.category_button_rect.y + 18.0f * scale,
                        state.category_button_rect.w - 40.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.white, 0.98f);
    draw_icon(ui, 0xF078u,
              Rect{state.category_button_rect.x + state.category_button_rect.w - 26.0f * scale, state.category_button_rect.y + 22.0f * scale,
                   12.0f * scale, 12.0f * scale},
              palette.white, 0.80f);

    const float date_y = category_y + 126.0f * scale;
    draw_text_left(ui, "Date", Rect{label_x, date_y, field_w, 18.0f * scale}, font_heading(scale), palette.white, 0.98f);
    state.date_button_rect = Rect{label_x, date_y + 28.0f * scale, field_w, 62.0f * scale};
    const bool date_hovered = interactive_hovered(state, input, state.date_button_rect);
    if (interactive_clicked(state, input, state.date_button_rect)) {
        toggle_menu(ui, state, OpenMenu::Date);
    }
    paint_card(ui, state.date_button_rect, 18.0f * scale, scale, palette, date_hovered ? 0.04f : 0.0f, 1.0f, false);
    draw_text_left(ui, kDateLabels[static_cast<std::size_t>(state.edit_date)],
                   Rect{state.date_button_rect.x + 18.0f * scale, state.date_button_rect.y + 18.0f * scale,
                        state.date_button_rect.w - 54.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.white, 0.98f);
    draw_icon(ui, 0xF073u,
              Rect{state.date_button_rect.x + state.date_button_rect.w - 28.0f * scale, state.date_button_rect.y + 18.0f * scale,
                   16.0f * scale, 16.0f * scale},
              palette.white, 0.88f);

    const float desc_y = date_y + 126.0f * scale;
    draw_text_left(ui, "Description", Rect{label_x, desc_y, field_w, 18.0f * scale}, font_heading(scale), palette.white, 0.98f);
    const Rect description_rect{label_x, desc_y + 30.0f * scale, field_w, 176.0f * scale};
    paint_card(ui, description_rect, 20.0f * scale, scale, palette, 0.0f, 1.0f, false);
    ui.scope(Rect{description_rect.x + 8.0f * scale, description_rect.y + 8.0f * scale,
                  description_rect.w - 16.0f * scale, description_rect.h - 16.0f * scale},
             [&](auto&) {
                 ui.text_area("", state.edit_description).height(description_rect.h - 16.0f * scale).draw();
             });

    const float button_y = rect.y + rect.h - 106.0f * scale;
    const float gap = 10.0f * scale;
    const float button_w = (rect.w - gap * 2.0f) / 3.0f;
    const Rect reset_rect{rect.x, button_y, button_w, 88.0f * scale};
    const Rect attach_rect{rect.x + button_w + gap, button_y, button_w, 88.0f * scale};
    const Rect confirm_rect{rect.x + (button_w + gap) * 2.0f, button_y, button_w, 88.0f * scale};
    if (interactive_clicked(state, input, reset_rect)) {
        sync_edit_from_transaction(state);
    }
    if (interactive_clicked(state, input, attach_rect)) {
        toggle_menu(ui, state, OpenMenu::Attachment);
    }
    if (interactive_clicked(state, input, confirm_rect)) {
        commit_edit(state);
    }
    paint_card(ui, reset_rect, 18.0f * scale, scale, palette, interactive_hovered(state, input, reset_rect) ? 0.04f : 0.0f, 1.0f, false);
    paint_card(ui, attach_rect, 18.0f * scale, scale, palette, interactive_hovered(state, input, attach_rect) ? 0.04f : 0.0f, 1.0f, false);
    ui.shape()
        .in(confirm_rect)
        .radius(18.0f * scale)
        .fill(palette.white, 1.0f)
        .stroke(mix_hex(palette.white, palette.blue_soft, 0.06f), 1.0f, 0.14f)
        .draw();
    draw_icon(ui, 0xF00Du, Rect{reset_rect.x + reset_rect.w * 0.5f - 10.0f * scale, reset_rect.y + 34.0f * scale, 20.0f * scale, 20.0f * scale},
              palette.white, 0.96f);
    draw_icon(ui, 0xF0C6u, Rect{attach_rect.x + attach_rect.w * 0.5f - 10.0f * scale, attach_rect.y + 34.0f * scale, 20.0f * scale, 20.0f * scale},
              palette.white, 0.96f);
    draw_icon(ui, 0xF00Cu, Rect{confirm_rect.x + confirm_rect.w * 0.5f - 10.0f * scale, confirm_rect.y + 34.0f * scale, 20.0f * scale, 20.0f * scale},
              palette.phone_inner, 0.96f);
    state.attachment_button_rect = attach_rect;
}

template <std::size_t N>
void draw_menu_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                       const Rect& button, OpenMenu menu_id, int& selected_index,
                       const std::array<const char*, N>& labels, float scale,
                       bool align_right = false, float min_width = 170.0f) {
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float mix = smoothstep01(ui.presence(overlay_motion_id(ui, menu_id), state.open_menu == menu_id, 18.0f, 16.0f));
    if (mix <= 0.01f) {
        return;
    }

    const float width = std::max(button.w, min_width * scale);
    const Rect base{
        align_right ? (button.x + button.w - width) : button.x,
        button.y + button.h + 8.0f * scale,
        width,
        10.0f * scale + static_cast<float>(N) * 34.0f * scale,
    };
    const Rect menu{base.x, base.y + (1.0f - mix) * 10.0f * scale, base.w, base.h};
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});

    ui.shape()
        .in(menu)
        .radius(18.0f * scale)
        .fill(palette.card_alt, 0.34f * mix)
        .stroke(palette.white, 1.0f, 0.08f * mix)
        .blur(16.0f * scale, 24.0f * scale)
        .shadow(0.0f, 10.0f * scale, 24.0f * scale, palette.shadow, 0.14f * mix)
        .draw();
    ui.shape()
        .in(inset_rect(menu, 1.0f))
        .radius(17.0f * scale)
        .fill(0x171717, 0.98f * mix)
        .stroke(palette.card_soft, 1.0f, 0.92f * mix)
        .draw();

    ui.ctx().set_global_alpha(mix);
    for (std::size_t i = 0; i < N; ++i) {
        const Rect item{
            menu.x + 6.0f * scale,
            menu.y + 5.0f * scale + static_cast<float>(i) * 34.0f * scale + (1.0f - mix) * 5.0f * scale,
            menu.w - 12.0f * scale,
            30.0f * scale,
        };
        const bool item_hovered = hovered(input, item);
        const bool item_selected = selected_index == static_cast<int>(i);
        if (item_hovered || item_selected) {
            draw_fill(ui, item, mix_hex(palette.card_soft, palette.blue_soft, item_selected ? 0.10f : 0.04f), item.h * 0.5f, 0.96f);
        }
        draw_text_left(ui, labels[i],
                       Rect{item.x + 12.0f * scale, item.y + 6.0f * scale, item.w - 24.0f * scale, 16.0f * scale},
                       font_body(scale), item_selected ? palette.white : palette.muted, 0.98f);
        if (mix > 0.9f && clicked(input, item)) {
            selected_index = static_cast<int>(i);
            state.open_menu = OpenMenu::None;
        }
    }
    ui.ctx().set_global_alpha(1.0f);

    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_notifications_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette, float scale) {
    const Rect button = state.bell_button_rect;
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float mix = smoothstep01(ui.presence(overlay_motion_id(ui, OpenMenu::Notifications), state.open_menu == OpenMenu::Notifications, 18.0f, 16.0f));
    if (mix <= 0.01f) {
        return;
    }
    const Rect base{button.x - 180.0f * scale, button.y + button.h + 10.0f * scale, 232.0f * scale, 132.0f * scale};
    const Rect menu{base.x, base.y + (1.0f - mix) * 10.0f * scale, base.w, base.h};
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});
    paint_card(ui, menu, 18.0f * scale, scale, palette, 0.04f, 0.98f, true);
    draw_text_left(ui, "Notifications",
                   Rect{menu.x + 14.0f * scale, menu.y + 12.0f * scale, menu.w - 28.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.white, 0.98f * mix);
    const Rect first{menu.x + 10.0f * scale, menu.y + 40.0f * scale, menu.w - 20.0f * scale, 34.0f * scale};
    const Rect second{menu.x + 10.0f * scale, menu.y + 82.0f * scale, menu.w - 20.0f * scale, 34.0f * scale};
    if (hovered(input, first)) {
        draw_fill(ui, first, mix_hex(palette.card_soft, palette.orange, 0.08f), 12.0f * scale, 0.96f);
    }
    if (hovered(input, second)) {
        draw_fill(ui, second, mix_hex(palette.card_soft, palette.blue, 0.08f), 12.0f * scale, 0.96f);
    }
    draw_text_left(ui, "Subscription payment due",
                   Rect{first.x + 10.0f * scale, first.y + 6.0f * scale, first.w - 20.0f * scale, 14.0f * scale},
                   font_body(scale), palette.white, 0.98f);
    draw_text_left(ui, "Tap to edit Netflix",
                   Rect{first.x + 10.0f * scale, first.y + 19.0f * scale, first.w - 20.0f * scale, 12.0f * scale},
                   font_small(scale), palette.muted, 0.94f);
    draw_text_left(ui, "Analytics updated this week",
                   Rect{second.x + 10.0f * scale, second.y + 6.0f * scale, second.w - 20.0f * scale, 14.0f * scale},
                   font_body(scale), palette.white, 0.98f);
    draw_text_left(ui, "Focus the Saturday spike",
                   Rect{second.x + 10.0f * scale, second.y + 19.0f * scale, second.w - 20.0f * scale, 12.0f * scale},
                   font_small(scale), palette.muted, 0.94f);
    if (clicked(input, first)) {
        state.selected_transaction = 0;
        sync_edit_from_transaction(state);
        state.active_nav = 2;
        state.open_menu = OpenMenu::None;
    }
    if (clicked(input, second)) {
        state.active_nav = 1;
        state.selected_bar = 5;
        state.open_menu = OpenMenu::None;
    }
    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_amount_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette, float scale) {
    const Rect button = state.amount_button_rect;
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float mix = smoothstep01(ui.presence(overlay_motion_id(ui, OpenMenu::Amount), state.open_menu == OpenMenu::Amount, 18.0f, 16.0f));
    if (mix <= 0.01f) {
        return;
    }
    const Rect base{button.x - 32.0f * scale, button.y + button.h + 10.0f * scale, 248.0f * scale, 146.0f * scale};
    const Rect menu{base.x, base.y + (1.0f - mix) * 10.0f * scale, base.w, base.h};
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});
    paint_card(ui, menu, 18.0f * scale, scale, palette, 0.06f, 0.98f, true);
    draw_text_left(ui, "Quick amounts",
                   Rect{menu.x + 14.0f * scale, menu.y + 12.0f * scale, menu.w - 28.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.white, 0.98f);
    for (int i = 0; i < 4; ++i) {
        const Rect chip{menu.x + 12.0f * scale + static_cast<float>(i % 2) * 112.0f * scale,
                        menu.y + 42.0f * scale + static_cast<float>(i / 2) * 40.0f * scale,
                        100.0f * scale, 32.0f * scale};
        if (hovered(input, chip)) {
            draw_fill(ui, chip, mix_hex(palette.card_soft, palette.blue_soft, 0.08f), chip.h * 0.5f, 0.96f);
        }
        char chip_text[20]{};
        format_currency_text(kAmountPresets[static_cast<std::size_t>(i)], chip_text, sizeof(chip_text), true);
        draw_text_center(ui, chip_text, chip, font_body(scale), palette.white, 0.96f);
        if (clicked(input, chip)) {
            state.edit_amount = kAmountPresets[static_cast<std::size_t>(i)];
            state.open_menu = OpenMenu::None;
        }
    }
    const Rect less{menu.x + 12.0f * scale, menu.y + menu.h - 40.0f * scale, 100.0f * scale, 28.0f * scale};
    const Rect more{menu.x + menu.w - 112.0f * scale, menu.y + menu.h - 40.0f * scale, 100.0f * scale, 28.0f * scale};
    draw_fill(ui, less, palette.card_soft, less.h * 0.5f, 0.98f);
    draw_fill(ui, more, palette.card_soft, more.h * 0.5f, 0.98f);
    draw_text_center(ui, "-$1", less, font_body(scale), palette.white, 0.96f);
    draw_text_center(ui, "+$1", more, font_body(scale), palette.white, 0.96f);
    if (clicked(input, less)) {
        state.edit_amount = std::min(-0.01f, state.edit_amount + 1.0f);
    }
    if (clicked(input, more)) {
        state.edit_amount -= 1.0f;
    }
    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_more_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette, float scale) {
    const Rect button = state.analytics_more_rect;
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float mix = smoothstep01(ui.presence(overlay_motion_id(ui, OpenMenu::AnalyticsMore), state.open_menu == OpenMenu::AnalyticsMore, 18.0f, 16.0f));
    if (mix <= 0.01f) {
        return;
    }
    const Rect base{button.x - 110.0f * scale, button.y + button.h + 10.0f * scale, 164.0f * scale, 116.0f * scale};
    const Rect menu{base.x, base.y + (1.0f - mix) * 10.0f * scale, base.w, base.h};
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});
    paint_card(ui, menu, 18.0f * scale, scale, palette, 0.04f, 0.98f, true);
    for (int i = 0; i < 3; ++i) {
        const Rect item{menu.x + 8.0f * scale, menu.y + 8.0f * scale + static_cast<float>(i) * 34.0f * scale, menu.w - 16.0f * scale, 28.0f * scale};
        if (hovered(input, item)) {
            draw_fill(ui, item, mix_hex(palette.card_soft, palette.blue_soft, 0.06f), item.h * 0.5f, 0.96f);
        }
        draw_text_left(ui, kMoreLabels[static_cast<std::size_t>(i)],
                       Rect{item.x + 10.0f * scale, item.y + 6.0f * scale, item.w - 20.0f * scale, 16.0f * scale},
                       font_body(scale), palette.white, 0.96f);
        if (clicked(input, item)) {
            if (i == 0) {
                state.focused_tile = 4;
                state.selected_bar = 5;
            } else if (i == 1) {
                state.analytics_mode = 0;
            } else {
                state.analytics_mode = 1;
            }
            state.open_menu = OpenMenu::None;
        }
    }
    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_attachment_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette, float scale) {
    const Rect button = state.attachment_button_rect;
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float mix = smoothstep01(ui.presence(overlay_motion_id(ui, OpenMenu::Attachment), state.open_menu == OpenMenu::Attachment, 18.0f, 16.0f));
    if (mix <= 0.01f) {
        return;
    }
    const Rect base{button.x - 18.0f * scale, button.y - 92.0f * scale, 160.0f * scale, 78.0f * scale};
    const Rect menu{base.x, base.y - (1.0f - mix) * 10.0f * scale, base.w, base.h};
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});
    paint_card(ui, menu, 18.0f * scale, scale, palette, 0.04f, 0.98f, true);
    for (int i = 0; i < 2; ++i) {
        const Rect item{menu.x + 8.0f * scale, menu.y + 8.0f * scale + static_cast<float>(i) * 30.0f * scale, menu.w - 16.0f * scale, 24.0f * scale};
        if (hovered(input, item)) {
            draw_fill(ui, item, mix_hex(palette.card_soft, palette.blue_soft, 0.06f), item.h * 0.5f, 0.96f);
        }
        draw_text_left(ui, kAttachmentLabels[static_cast<std::size_t>(i)],
                       Rect{item.x + 10.0f * scale, item.y + 4.0f * scale, item.w - 20.0f * scale, 14.0f * scale},
                       font_body(scale), palette.white, 0.96f);
        if (clicked(input, item)) {
            state.receipt_attached = (i == 0);
            state.open_menu = OpenMenu::None;
        }
    }
    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_overlay_pass(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                       const Rect& clip_rect, float scale) {
    ui.clip(clip_rect, [&] {
        draw_notifications_overlay(ui, input, state, palette, scale);
        draw_amount_overlay(ui, input, state, palette, scale);
        draw_more_overlay(ui, input, state, palette, scale);
        draw_attachment_overlay(ui, input, state, palette, scale);
        draw_menu_overlay(ui, input, state, palette, state.account_button_rect,
                          OpenMenu::Account, state.active_account, kAccountLabels, scale, false, 220.0f);
        draw_menu_overlay(ui, input, state, palette, state.analytics_range_rect,
                          OpenMenu::AnalyticsRange, state.analytics_range, kRangeLabels, scale, true, 120.0f);
        draw_menu_overlay(ui, input, state, palette, state.category_button_rect,
                          OpenMenu::Category, state.edit_category, kCategoryLabels, scale, false, 220.0f);
        draw_menu_overlay(ui, input, state, palette, state.date_button_rect,
                          OpenMenu::Date, state.edit_date, kDateLabels, scale, false, 180.0f);
    });
}

void draw_dashboard(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                    float scale, const Palette& palette, double time_seconds) {
    draw_fill(ui, rect, palette.background, 0.0f, 1.0f);
    draw_soft_glow(ui, Rect{rect.x + rect.w * 0.08f, rect.y + rect.h * 0.60f, 440.0f * scale, 340.0f * scale},
                   lighten_hex(palette.blue_soft, 0.20f), palette.background, 0.86f, 0.28f);
    draw_soft_glow(ui, Rect{rect.x + rect.w * 0.42f, rect.y + rect.h * 0.18f, 480.0f * scale, 360.0f * scale},
                   lighten_hex(palette.white, 0.10f), palette.background, 0.84f, 0.18f);
    draw_soft_glow(ui, Rect{rect.x + rect.w * 0.72f, rect.y + rect.h * 0.56f, 460.0f * scale, 320.0f * scale},
                   lighten_hex(palette.green_soft, 0.12f), palette.background, 0.86f, 0.26f);

    const Rect left_phone{rect.x + 92.0f * scale, rect.y + 150.0f * scale, 510.0f * scale, 1090.0f * scale};
    const Rect middle_phone{rect.x + 774.0f * scale, rect.y + 210.0f * scale, 510.0f * scale, 1048.0f * scale};
    const Rect right_phone{rect.x + 1452.0f * scale, rect.y + 150.0f * scale, 510.0f * scale, 1090.0f * scale};

    const Rect left_content = draw_phone_shell(ui, input, state, left_phone, 0, scale, palette, time_seconds);
    const Rect middle_content = draw_phone_shell(ui, input, state, middle_phone, 1, scale, palette, time_seconds);
    const Rect right_content = draw_phone_shell(ui, input, state, right_phone, 2, scale, palette, time_seconds);

    draw_home_screen(ui, input, state, left_content, scale, palette, time_seconds);
    draw_analytics_screen(ui, input, state, middle_content, scale, palette);
    draw_edit_screen(ui, input, state, right_content, scale, palette, time_seconds);
    draw_overlay_pass(ui, input, state, palette, rect, scale);
}

}  // namespace

int main() {
    DashboardState state = make_state();
    const Palette palette = make_palette();

    eui::app::AppOptions options{};
    options.title = "EUI Mobile Finance Trio Demo";
    options.width = 1900;
    options.height = 1400;
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
            const float scale = std::max(0.68f, std::min(framebuffer_w / 2048.0f, framebuffer_h / 1466.0f));

            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(
                eui::rgba(static_cast<float>((palette.blue >> 16u) & 0xffu) / 255.0f,
                          static_cast<float>((palette.blue >> 8u) & 0xffu) / 255.0f,
                          static_cast<float>(palette.blue & 0xffu) / 255.0f, 1.0f));
            ctx.set_corner_radius(18.0f * scale);

            state.account_button_rect = Rect{};
            state.bell_button_rect = Rect{};
            state.analytics_range_rect = Rect{};
            state.analytics_more_rect = Rect{};
            state.amount_button_rect = Rect{};
            state.category_button_rect = Rect{};
            state.date_button_rect = Rect{};
            state.attachment_button_rect = Rect{};
            if (state.open_menu == OpenMenu::None) {
                state.overlay_block_active = false;
                state.overlay_block_rect = Rect{};
            }

            UI ui(ctx);
            draw_dashboard(ui, input, state, Rect{0.0f, 0.0f, framebuffer_w, framebuffer_h}, scale, palette, input.time_seconds);
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
