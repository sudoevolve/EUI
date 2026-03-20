#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
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
    Search = 1,
    Date = 2,
    Product = 3,
    Profile = 4,
    Glow = 5,
    Notifications = 6,
    CustomerCard = 7,
    ProductCard = 8,
    BubbleCard = 9,
    TimelineCard = 10,
};

struct Palette {
    std::uint32_t background{};
    std::uint32_t background_alt{};
    std::uint32_t shell{};
    std::uint32_t shell_soft{};
    std::uint32_t panel{};
    std::uint32_t panel_alt{};
    std::uint32_t panel_tint{};
    std::uint32_t border{};
    std::uint32_t text{};
    std::uint32_t muted{};
    std::uint32_t soft{};
    std::uint32_t lime{};
    std::uint32_t lime_soft{};
    std::uint32_t orange{};
    std::uint32_t orange_soft{};
    std::uint32_t white_soft{};
    std::uint32_t red{};
    std::uint32_t blue{};
    std::uint32_t shadow{};
};

struct NavItem {
    std::uint32_t icon;
    const char* label;
};

struct ScreenPreset {
    const char* title;
    float customer_left;
    float customer_right;
    const char* customer_left_label;
    const char* customer_right_label;
    float product_left;
    float product_right;
    const char* product_left_label;
    const char* product_right_label;
};

struct BubblePair {
    int top_value;
    int bottom_value;
    int top_kind;
    int bottom_kind;
    int dot_kind;
};

struct TimelineTask {
    const char* date;
    float start;
    float duration;
    int category;
    int badge_kind;
    int count;
};

struct SearchSuggestion {
    const char* label;
    const char* caption;
    std::uint32_t icon;
    int nav_index;
    int rail_index;
    int product_filter;
    int resource_filter;
};

struct AvatarStyle {
    std::uint32_t top;
    std::uint32_t bottom;
    std::uint32_t hair;
    std::uint32_t shirt;
    std::uint32_t skin;
};

struct DashboardState {
    int selected_nav{0};
    int selected_rail{0};
    int selected_date{0};
    int selected_product_filter{0};
    int selected_profile{0};
    int glow_mode{1};
    int selected_customer_mode{0};
    int selected_product_mode{0};
    int product_legend{-1};
    int selected_timeline{1};
    int hovered_customer_point{-1};
    int hovered_matrix_dot{-1};
    int hovered_bubble_column{-1};
    int hovered_timeline{-1};
    OpenMenu open_menu{OpenMenu::None};
    std::string search_text{"Check Box"};
    Rect search_button_rect{};
    Rect header_profile_rect{};
    Rect date_button_rect{};
    Rect product_button_rect{};
    Rect profile_button_rect{};
    Rect glow_button_rect{};
    Rect customer_menu_button_rect{};
    Rect product_menu_button_rect{};
    Rect bubble_menu_button_rect{};
    Rect timeline_menu_button_rect{};
    Rect overlay_block_rect{};
    bool overlay_block_active{false};
};

static constexpr std::array<NavItem, 3> kNavItems{{
    {0xF1B2u, "Check Box"},
    {0xF080u, "Monitoring"},
    {0xF086u, "Support"},
}};

static constexpr std::array<std::uint32_t, 4> kRailIcons{{0xF004u, 0xF073u, 0xF3A5u, 0xF013u}};
static constexpr std::array<const char*, 3> kDateOptions{{"Now", "This Week", "This Month"}};
static constexpr std::array<const char*, 4> kProductFilters{{"All", "Customer", "Product", "Web"}};
static constexpr std::array<const char*, 3> kProfileShortNames{{"Bogdan", "Studio", "Core"}};
static constexpr std::array<const char*, 3> kProfileNames{{"Bogdan Nikitin", "Nix Studio", "Core Team"}};
static constexpr std::array<const char*, 3> kProfileHandles{{"@Nixtio", "@NixStudio", "@CoreTeam"}};
static constexpr std::array<const char*, 3> kGlowModes{{"Focus", "Balanced", "Ambient"}};
static constexpr std::array<const char*, 3> kCustomerModes{{"Traffic mix", "Funnel mix", "Retention"}};
static constexpr std::array<const char*, 3> kProductModes{{"Partner grid", "Owner grid", "Signal grid"}};
static constexpr std::array<const char*, 3> kBubbleModes{{"Resources", "Valid", "Invalid"}};
static constexpr std::array<const char*, 4> kTimelineModes{{"All", "Customer", "Product", "Web"}};

static constexpr std::array<ScreenPreset, 3> kPresets{{
    {"CHECK BOX", 2.4f, 1.1f, "Web Surfing", "Radio Station", 2.8f, 3.2f, "Partners", "Owners"},
    {"MONITORING", 1.8f, 2.2f, "Live Feeds", "Search Calls", 3.4f, 2.6f, "Signals", "Alerts"},
    {"SUPPORT", 2.1f, 1.6f, "Open Tickets", "Escalations", 2.5f, 3.0f, "Replies", "Owners"},
}};

static constexpr std::array<std::array<float, 9>, 3> kCustomerSeriesLime{{
    {{0.42f, 0.39f, 0.38f, 0.48f, 0.44f, 0.36f, 0.52f, 0.48f, 0.39f}},
    {{0.33f, 0.40f, 0.52f, 0.47f, 0.44f, 0.50f, 0.58f, 0.54f, 0.49f}},
    {{0.50f, 0.46f, 0.42f, 0.40f, 0.48f, 0.54f, 0.51f, 0.45f, 0.37f}},
}};

static constexpr std::array<std::array<float, 9>, 3> kCustomerSeriesOrange{{
    {{0.55f, 0.58f, 0.30f, 0.18f, 0.64f, 0.31f, 0.56f, 0.52f, 0.33f}},
    {{0.24f, 0.36f, 0.34f, 0.58f, 0.27f, 0.61f, 0.44f, 0.39f, 0.55f}},
    {{0.60f, 0.46f, 0.53f, 0.28f, 0.62f, 0.36f, 0.30f, 0.48f, 0.57f}},
}};

static constexpr std::array<BubblePair, 9> kBubblePairs{{
    {52, 81, 0, 2, 1},
    {96, 25, 1, 2, 0},
    {48, 51, 1, 0, 2},
    {80, 49, 1, 2, 0},
    {34, 67, 2, 1, 0},
    {92, 28, 1, 0, 2},
    {58, 20, 1, 2, 0},
    {84, 39, 2, 1, 0},
    {36, 72, 0, 2, 1},
}};

static constexpr std::array<TimelineTask, 8> kTimelineTasks{{
    {"30.09", 0.8f, 11.0f, 0, 0, 16},
    {"29.09", 16.4f, 12.6f, 1, 1, 29},
    {"28.09", 2.4f, 12.1f, 2, 2, 15},
    {"27.09", 4.8f, 14.8f, 0, 3, 21},
    {"26.09", 0.2f, 8.2f, 2, 4, 10},
    {"25.09", 5.1f, 8.5f, 1, 5, 15},
    {"24.09", 10.0f, 16.8f, 0, 6, 19},
    {"23.09", 7.0f, 8.1f, 2, 7, 8},
}};

static constexpr std::array<SearchSuggestion, 6> kSearchSuggestions{{
    {"Check Box", "Switch to the check box preset", 0xF1B2u, 0, -1, -1, -2},
    {"Monitoring", "Show the monitoring preset", 0xF080u, 1, -1, -1, -2},
    {"Support", "Jump to the support preset", 0xF086u, 2, -1, -1, -2},
    {"Projects Timeline", "Focus the timeline and product filter", 0xF201u, -1, 1, 2, -2},
    {"Valid Resources", "Filter the resource card to valid items", 0xF058u, -1, 2, -1, 1},
    {"Customer Signals", "Focus the customer view", 0xF004u, -1, 0, 1, -2},
}};

static constexpr std::array<AvatarStyle, 4> kAvatarStyles{{
    {0xE3EEF9, 0xCADBF0, 0x4C5565, 0x7E926B, 0xF1C7A8},
    {0xF6D8E4, 0xD89BBB, 0x7B4E63, 0x526C9D, 0xF3C8AC},
    {0xF6E8D8, 0xDFC29F, 0x9B7C4F, 0xC0A170, 0xF0D0AE},
    {0xE7E2DB, 0xCFBFAA, 0x4A4036, 0x4A8F6D, 0xE4BE9A},
}};

Palette make_palette() {
    return Palette{
        0xD5ECB7,
        0xC0E198,
        0x050505,
        0x0E0D0D,
        0x242221,
        0x2A2726,
        0x343130,
        0x3A3635,
        0xF5F1ED,
        0xA7A09C,
        0x746F6B,
        0xB8FF58,
        0xD9FF90,
        0xFFA62C,
        0xFFBF63,
        0xF5F3EF,
        0xC73A3A,
        0x2D79F3,
        0x0B1407,
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

float font_title(float scale) {
    return 17.0f * scale;
}

float font_body(float scale) {
    return 14.2f * scale;
}

float font_meta(float scale) {
    return 12.0f * scale;
}

float font_value(float scale) {
    return 28.0f * scale;
}

float font_hero(float scale) {
    return 54.0f * scale;
}

float font_capsule(float scale) {
    return 13.0f * scale;
}

std::uint64_t overlay_motion_id(UI& ui, OpenMenu menu_id) {
    return ui.id("checkbox/overlay", static_cast<std::uint64_t>(menu_id));
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

bool timeline_category_visible(const DashboardState& state, int category) {
    return state.selected_product_filter == 0 || (state.selected_product_filter - 1) == category;
}

bool resource_visible(const DashboardState& state, int kind) {
    return state.product_legend < 0 || state.product_legend == kind;
}

std::uint32_t timeline_color(const Palette& palette, int category) {
    switch (category) {
        case 0:
            return palette.lime;
        case 1:
            return palette.orange;
        case 2:
        default:
            return palette.white_soft;
    }
}

std::uint32_t resource_color(const Palette& palette, int kind) {
    switch (kind) {
        case 1:
            return palette.lime;
        case 2:
            return palette.orange;
        case 0:
        default:
            return palette.white_soft;
    }
}

const char* category_label(int category) {
    switch (category) {
        case 0:
            return "Customer";
        case 1:
            return "Product";
        case 2:
        default:
            return "Web";
    }
}

float date_value_offset(int selected_date) {
    switch (selected_date) {
        case 1:
            return 0.3f;
        case 2:
            return 0.6f;
        case 0:
        default:
            return 0.0f;
    }
}

float date_duration_scale(int selected_date) {
    switch (selected_date) {
        case 1:
            return 0.90f;
        case 2:
            return 1.08f;
        case 0:
        default:
            return 1.0f;
    }
}

float glow_alpha(const DashboardState& state) {
    switch (state.glow_mode) {
        case 0:
            return 0.22f;
        case 2:
            return 0.36f;
        case 1:
        default:
            return 0.28f;
    }
}

float glow_radius_mix(const DashboardState& state) {
    switch (state.glow_mode) {
        case 0:
            return 0.75f;
        case 2:
            return 1.20f;
        case 1:
        default:
            return 1.0f;
    }
}

void format_percent_text(float value, char* buffer, std::size_t size) {
    std::snprintf(buffer, size, "%.1f%%", value);
    for (char* cursor = buffer; *cursor != '\0'; ++cursor) {
        if (*cursor == '.') {
            *cursor = ',';
        }
    }
}

void format_int_text(int value, char* buffer, std::size_t size) {
    std::snprintf(buffer, size, "%d", value);
}

bool contains_case_insensitive(std::string_view text, std::string_view query) {
    if (query.empty()) {
        return true;
    }
    if (query.size() > text.size()) {
        return false;
    }
    for (std::size_t i = 0; i + query.size() <= text.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < query.size(); ++j) {
            const char lhs = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j])));
            const char rhs = static_cast<char>(std::tolower(static_cast<unsigned char>(query[j])));
            if (lhs != rhs) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
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

Rect lifted_rect(UI& ui, std::uint64_t id, const Rect& rect, bool hovered_state, float scale) {
    const float lift = smoothstep01(ui.presence(id, hovered_state, 16.0f, 18.0f)) * 6.0f * scale;
    return Rect{rect.x, rect.y - lift, rect.w, rect.h};
}

void paint_panel(UI& ui, const Rect& rect, float radius, float scale, const Palette& palette,
                 float accent_mix = 0.0f, float alpha = 1.0f, bool glass = false) {
    const std::uint32_t top = mix_hex(palette.panel, palette.panel_tint, accent_mix * 0.35f);
    const std::uint32_t bottom = mix_hex(palette.panel_alt, palette.shell_soft, accent_mix * 0.18f);
    auto shape = ui.shape()
                     .in(rect)
                     .radius(radius)
                     .gradient(top, bottom, alpha)
                     .stroke(mix_hex(palette.border, palette.lime_soft, accent_mix * 0.18f), 1.0f, 0.92f * alpha)
                     .shadow(0.0f, 18.0f * scale, 38.0f * scale, palette.shadow, (0.18f + accent_mix * 0.05f) * alpha);
    if (glass) {
        shape.blur(16.0f * scale, 22.0f * scale);
    }
    shape.draw();
}

void draw_logo(UI& ui, const Rect& rect, float scale) {
    draw_fill(ui, rect, 0xFFFFFF, rect.w * 0.5f, 1.0f);
    const Rect inner = inset_rect(rect, 11.0f * scale);
    const Rect left{inner.x + inner.w * 0.04f, inner.y + inner.h * 0.08f, inner.w * 0.16f, inner.h * 0.84f};
    const Rect right{inner.x + inner.w * 0.64f, inner.y + inner.h * 0.08f, inner.w * 0.16f, inner.h * 0.84f};
    const Rect diagonal{inner.x + inner.w * 0.22f, inner.y + inner.h * 0.14f, inner.w * 0.60f, inner.h * 0.16f};
    ui.shape().in(left).radius(left.w * 0.5f).fill(0x000000, 1.0f).draw();
    ui.shape().in(right).radius(right.w * 0.5f).fill(0x000000, 1.0f).draw();
    ui.shape()
        .in(diagonal)
        .origin_center()
        .rotate(52.0f)
        .radius(diagonal.h * 0.5f)
        .fill(0x000000, 1.0f)
        .draw();
    const Rect dot{inner.x + inner.w * 0.46f, inner.y + inner.h * 0.40f, inner.w * 0.12f, inner.w * 0.12f};
    draw_fill(ui, dot, 0xFFFFFF, dot.w * 0.5f, 1.0f);
}

void draw_avatar(UI& ui, const Rect& rect, const AvatarStyle& style, float scale, bool ring = true) {
    if (ring) {
        draw_fill(ui, rect, 0xFFFFFF, rect.w * 0.5f, 1.0f);
    }
    const Rect inner = inset_rect(rect, ring ? 2.0f * scale : 0.0f);
    ui.shape().in(inner).radius(inner.w * 0.5f).gradient(style.top, style.bottom, 1.0f).draw();
    draw_fill(ui, Rect{inner.x + inner.w * 0.18f, inner.y + inner.h * 0.60f, inner.w * 0.64f, inner.h * 0.26f},
              style.shirt, inner.h * 0.14f, 0.98f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.43f, inner.y + inner.h * 0.45f, inner.w * 0.14f, inner.h * 0.10f},
              style.skin, inner.w * 0.06f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.29f, inner.y + inner.h * 0.18f, inner.w * 0.42f, inner.h * 0.40f},
              style.skin, inner.w * 0.21f, 1.0f);
    draw_fill(ui, Rect{inner.x + inner.w * 0.19f, inner.y + inner.h * 0.08f, inner.w * 0.62f, inner.h * 0.24f},
              style.hair, inner.w * 0.28f, 1.0f);
}

void draw_avatar_stack(UI& ui, const Rect& rect, float scale, int count) {
    const float size = rect.h;
    const float overlap = size * 0.36f;
    const int safe_count = std::clamp(count, 1, 4);
    for (int index = safe_count - 1; index >= 0; --index) {
        const float x = rect.x + static_cast<float>(index) * (size - overlap);
        draw_avatar(ui, Rect{x, rect.y, size, size}, kAvatarStyles[static_cast<std::size_t>(index) % kAvatarStyles.size()], scale, true);
    }
}

void draw_slider_glyph(UI& ui, const Rect& rect, const Palette& palette, float scale, float alpha = 1.0f) {
    const float line_h = 4.0f * scale;
    const float knob = 10.0f * scale;
    const Rect top_line{rect.x + 14.0f * scale, rect.y + 17.0f * scale, rect.w - 28.0f * scale, line_h};
    const Rect bottom_line{rect.x + 14.0f * scale, rect.y + rect.h - 21.0f * scale, rect.w - 28.0f * scale, line_h};
    draw_fill(ui, top_line, palette.text, line_h * 0.5f, 0.95f * alpha);
    draw_fill(ui, bottom_line, palette.text, line_h * 0.5f, 0.95f * alpha);
    draw_fill(ui, Rect{top_line.x + top_line.w * 0.62f, top_line.y - 3.0f * scale, knob, knob},
              palette.shell, knob * 0.5f, alpha);
    draw_stroke(ui, Rect{top_line.x + top_line.w * 0.62f, top_line.y - 3.0f * scale, knob, knob},
                palette.text, knob * 0.5f, 1.0f, alpha);
    draw_fill(ui, Rect{bottom_line.x + top_line.w * 0.22f, bottom_line.y - 3.0f * scale, knob, knob},
              palette.shell, knob * 0.5f, alpha);
    draw_stroke(ui, Rect{bottom_line.x + top_line.w * 0.22f, bottom_line.y - 3.0f * scale, knob, knob},
                palette.text, knob * 0.5f, 1.0f, alpha);
}

void draw_dots_button(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                      OpenMenu menu_id, float scale, const Palette& palette) {
    const bool hovered_state = interactive_hovered(state, input, rect);
    const bool active = state.open_menu == menu_id;
    const float mix = smoothstep01(ui.presence(ui.id("checkbox/dots-hover", static_cast<std::uint64_t>(menu_id)),
                                               hovered_state || active, 18.0f, 16.0f));
    if (interactive_clicked(state, input, rect)) {
        toggle_menu(ui, state, menu_id);
    }
    if (mix > 0.01f) {
        draw_fill(ui, rect, mix_hex(palette.panel_alt, palette.lime_soft, mix * 0.06f), rect.h * 0.5f, 0.88f);
    }
    const float dot = 4.0f * scale;
    const float spacing = 11.0f * scale;
    for (int i = 0; i < 3; ++i) {
        draw_fill(ui, Rect{rect.x + 8.0f * scale + spacing * static_cast<float>(i), rect.y + rect.h * 0.5f - dot * 0.5f, dot, dot},
                  palette.text, dot * 0.5f, 0.92f);
    }
}

void draw_nav_button(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& slot, int index,
                     float scale, const Palette& palette) {
    const Rect rect{slot.x, slot.y + (slot.h - 56.0f * scale) * 0.5f, slot.w, 56.0f * scale};
    const bool selected = state.selected_nav == index;
    const bool hovered_state = interactive_hovered(state, input, rect);
    const float mix = smoothstep01(ui.presence(ui.id("checkbox/nav", static_cast<std::uint64_t>(index)),
                                               selected || hovered_state, 16.0f, 18.0f));
    if (interactive_clicked(state, input, rect)) {
        if (state.selected_nav != index) {
            ui.reset_motion("checkbox/page-reveal", 0.0f);
        }
        state.selected_nav = index;
    }
    const Rect visual = lifted_rect(ui, ui.id("checkbox/nav-lift", static_cast<std::uint64_t>(index)), rect,
                                    hovered_state || selected, scale);
    ui.shape()
        .in(visual)
        .radius(visual.h * 0.5f)
        .fill(mix_hex(0x232120, palette.panel_tint, mix * 0.25f), 0.98f)
        .stroke(mix_hex(palette.border, palette.lime_soft, selected ? 0.18f : mix * 0.10f), 1.0f, 0.92f)
        .shadow(0.0f, 10.0f * scale, 22.0f * scale, palette.shadow, (0.08f + mix * 0.03f))
        .draw();
    if (selected) {
        draw_soft_glow(ui, Rect{visual.x + visual.w * 0.24f, visual.y - 6.0f * scale, visual.w * 0.52f, visual.h * 1.3f},
                       palette.lime, palette.shell, 0.88f, 0.12f);
    }
    draw_icon(ui, kNavItems[static_cast<std::size_t>(index)].icon,
              Rect{visual.x + 20.0f * scale, visual.y + 16.0f * scale, 20.0f * scale, 20.0f * scale},
              palette.text, 0.94f);
    draw_text_left(ui, kNavItems[static_cast<std::size_t>(index)].label,
                   Rect{visual.x + 52.0f * scale, visual.y + 14.0f * scale, visual.w - 68.0f * scale, 22.0f * scale},
                   font_body(scale), palette.text, 0.98f);
}

void draw_header_profile(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& slot,
                         float scale, const Palette& palette) {
    const Rect rect{slot.x, slot.y + (slot.h - 70.0f * scale) * 0.5f, slot.w, 70.0f * scale};
    state.header_profile_rect = rect;
    const bool hovered_state = interactive_hovered(state, input, rect);
    const bool active = state.open_menu == OpenMenu::Notifications;
    const float mix = smoothstep01(ui.presence("checkbox/profile-hover", hovered_state || active, 18.0f, 16.0f));
    if (interactive_clicked(state, input, rect)) {
        toggle_menu(ui, state, OpenMenu::Notifications);
    }
    const Rect avatar_rect{rect.x + rect.w - 58.0f * scale, rect.y + 6.0f * scale, 52.0f * scale, 52.0f * scale};
    if (mix > 0.01f) {
        draw_fill(ui, Rect{rect.x + rect.w - 170.0f * scale, rect.y + 2.0f * scale, 170.0f * scale, rect.h - 4.0f * scale},
                  mix_hex(palette.panel_alt, palette.lime_soft, mix * 0.08f), 18.0f * scale, 0.88f);
    }
    draw_text_right(ui, kProfileNames[static_cast<std::size_t>(state.selected_profile)],
                    Rect{rect.x, rect.y + 10.0f * scale, rect.w - 74.0f * scale, 18.0f * scale},
                    font_body(scale), palette.text, 0.98f);
    draw_text_right(ui, kProfileHandles[static_cast<std::size_t>(state.selected_profile)],
                    Rect{rect.x, rect.y + 36.0f * scale, rect.w - 74.0f * scale, 16.0f * scale},
                    font_meta(scale), palette.muted, 0.98f);
    draw_avatar(ui, avatar_rect, kAvatarStyles[0], scale, true);
    const Rect badge{avatar_rect.x + avatar_rect.w - 8.0f * scale, avatar_rect.y - 6.0f * scale, 24.0f * scale, 24.0f * scale};
    draw_fill(ui, badge, palette.red, badge.w * 0.5f, 1.0f);
    draw_text_center(ui, "2", badge, font_meta(scale), 0xFFFFFF, 1.0f);
}

void draw_header(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                 float scale, const Palette& palette) {
    ui.row(rect)
        .tracks({
            px(70.0f * scale),
            px(204.0f * scale),
            px(212.0f * scale),
            px(184.0f * scale),
            px(70.0f * scale),
            fr(),
            px(270.0f * scale),
        })
        .gap(16.0f * scale)
        .begin([&](auto& cols) {
            const Rect logo_slot = cols.next();
            draw_logo(ui, Rect{logo_slot.x + 4.0f * scale, logo_slot.y + 12.0f * scale, 58.0f * scale, 58.0f * scale}, scale);

            draw_nav_button(ui, input, state, cols.next(), 0, scale, palette);
            draw_nav_button(ui, input, state, cols.next(), 1, scale, palette);
            draw_nav_button(ui, input, state, cols.next(), 2, scale, palette);

            const Rect search_slot = cols.next();
            const Rect search_rect{search_slot.x + 7.0f * scale, search_slot.y + 12.0f * scale, 56.0f * scale, 56.0f * scale};
            state.search_button_rect = search_rect;
            const bool hovered_state = interactive_hovered(state, input, search_rect);
            const bool active = state.open_menu == OpenMenu::Search;
            const float mix = smoothstep01(ui.presence("checkbox/search-hover", hovered_state || active, 18.0f, 16.0f));
            if (interactive_clicked(state, input, search_rect)) {
                toggle_menu(ui, state, OpenMenu::Search);
            }
            ui.shape()
                .in(search_rect)
                .radius(search_rect.w * 0.5f)
                .fill(mix_hex(0x201F1E, palette.panel_tint, mix * 0.20f), 0.98f)
                .stroke(mix_hex(palette.border, palette.lime_soft, mix * 0.12f), 1.0f, 0.92f)
                .draw();
            draw_icon(ui, 0xF002u,
                      Rect{search_rect.x + 17.0f * scale, search_rect.y + 17.0f * scale, 22.0f * scale, 22.0f * scale},
                      palette.text, 0.95f);

            cols.next();
            draw_header_profile(ui, input, state, cols.next(), scale, palette);
        });
}

template <std::size_t N>
void draw_filter_button(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                        const Rect& rect, const char* prefix, OpenMenu menu_id, int selected_index,
                        const std::array<const char*, N>& labels, float scale) {
    const bool hovered_state = interactive_hovered(state, input, rect);
    const bool active = state.open_menu == menu_id;
    const float mix = smoothstep01(ui.presence(ui.id("checkbox/filter-button", static_cast<std::uint64_t>(menu_id)),
                                               hovered_state || active, 18.0f, 16.0f));
    if (interactive_clicked(state, input, rect)) {
        toggle_menu(ui, state, menu_id);
    }

    switch (menu_id) {
        case OpenMenu::Date:
            state.date_button_rect = rect;
            break;
        case OpenMenu::Product:
            state.product_button_rect = rect;
            break;
        case OpenMenu::Profile:
            state.profile_button_rect = rect;
            break;
        default:
            break;
    }

    ui.shape()
        .in(rect)
        .radius(rect.h * 0.5f)
        .fill(mix_hex(0x201F1E, palette.panel_tint, mix * 0.18f), 0.98f)
        .stroke(mix_hex(palette.border, palette.lime_soft, mix * 0.14f), 1.0f, 0.92f)
        .draw();
    const float prefix_w = ui.measure_text(prefix, font_body(scale)) + 2.0f * scale;
    draw_text_left(ui, prefix,
                   Rect{rect.x + 20.0f * scale, rect.y + 18.0f * scale, prefix_w, 18.0f * scale},
                   font_body(scale), palette.soft, 0.98f);
    draw_text_left(ui, labels[static_cast<std::size_t>(std::clamp(selected_index, 0, static_cast<int>(N - 1)))],
                   Rect{rect.x + 20.0f * scale + prefix_w, rect.y + 18.0f * scale,
                        rect.w - prefix_w - 44.0f * scale, 18.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_icon(ui, 0xF078u,
              Rect{rect.x + rect.w - 24.0f * scale, rect.y + 21.0f * scale, 12.0f * scale, 12.0f * scale},
              palette.text, 0.80f);
}

void draw_glow_button(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                      float scale, const Palette& palette) {
    state.glow_button_rect = rect;
    const bool hovered_state = interactive_hovered(state, input, rect);
    const bool active = state.open_menu == OpenMenu::Glow;
    const float mix = smoothstep01(ui.presence("checkbox/glow-button", hovered_state || active, 18.0f, 16.0f));
    if (interactive_clicked(state, input, rect)) {
        toggle_menu(ui, state, OpenMenu::Glow);
    }
    ui.shape()
        .in(rect)
        .radius(rect.w * 0.5f)
        .fill(mix_hex(0x201F1E, palette.panel_tint, mix * 0.20f), 0.98f)
        .stroke(mix_hex(palette.border, palette.lime_soft, mix * 0.12f), 1.0f, 0.92f)
        .draw();
    draw_slider_glyph(ui, rect, palette, scale, 0.95f);
}

void draw_title_row(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                    float scale, const Palette& palette, const ScreenPreset& preset) {
    ui.row(rect)
        .tracks({fr(), px(176.0f * scale), px(188.0f * scale), px(206.0f * scale), px(66.0f * scale)})
        .gap(14.0f * scale)
        .begin([&](auto& cols) {
            const Rect title_rect = cols.next();
            draw_text_left(ui, preset.title,
                           Rect{title_rect.x, title_rect.y + 20.0f * scale, title_rect.w, 62.0f * scale},
                           font_hero(scale), palette.text, 1.0f);
            const Rect date_rect{cols.next().x, rect.y + 18.0f * scale, 176.0f * scale, 58.0f * scale};
            const Rect product_rect{cols.next().x, rect.y + 18.0f * scale, 188.0f * scale, 58.0f * scale};
            const Rect profile_rect{cols.next().x, rect.y + 18.0f * scale, 206.0f * scale, 58.0f * scale};
            const Rect glow_rect{cols.next().x, rect.y + 18.0f * scale, 58.0f * scale, 58.0f * scale};

            draw_filter_button(ui, input, state, palette, date_rect, "Date:", OpenMenu::Date,
                               state.selected_date, kDateOptions, scale);
            draw_filter_button(ui, input, state, palette, product_rect, "Product:", OpenMenu::Product,
                               state.selected_product_filter, kProductFilters, scale);
            draw_filter_button(ui, input, state, palette, profile_rect, "Profile:", OpenMenu::Profile,
                               state.selected_profile, kProfileShortNames, scale);
            draw_glow_button(ui, input, state, glow_rect, scale, palette);
        });
}

void draw_rail_button(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect, int index,
                      float scale, const Palette& palette) {
    const bool selected = state.selected_rail == index;
    const bool hovered_state = interactive_hovered(state, input, rect);
    const float mix = smoothstep01(ui.presence(ui.id("checkbox/rail", static_cast<std::uint64_t>(index)),
                                               selected || hovered_state, 18.0f, 18.0f));
    if (interactive_clicked(state, input, rect)) {
        state.selected_rail = index;
    }
    const Rect visual = lifted_rect(ui, ui.id("checkbox/rail-lift", static_cast<std::uint64_t>(index)), rect,
                                    selected || hovered_state, scale);
    ui.shape()
        .in(visual)
        .radius(visual.w * 0.5f)
        .fill(mix_hex(0x1A1918, palette.panel_tint, mix * 0.18f), 0.98f)
        .stroke(mix_hex(palette.border, palette.lime_soft, selected ? 0.18f : mix * 0.10f), 1.0f, 0.92f)
        .shadow(0.0f, 10.0f * scale, 22.0f * scale, palette.shadow, 0.08f + mix * 0.02f)
        .draw();
    draw_icon(ui, kRailIcons[static_cast<std::size_t>(index)],
              Rect{visual.x + 20.0f * scale, visual.y + 20.0f * scale, 22.0f * scale, 22.0f * scale},
              palette.text, 0.96f);
}

void draw_rail(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
               float scale, const Palette& palette) {
    const float size = 64.0f * scale;
    const float gap = 18.0f * scale;
    const float top = rect.y + 132.0f * scale;
    for (int index = 0; index < 4; ++index) {
        const Rect button{rect.x, top + static_cast<float>(index) * (size + gap), size, size};
        draw_rail_button(ui, input, state, button, index, scale, palette);
    }

    const Rect plus_rect{rect.x, rect.y + rect.h - 72.0f * scale, 72.0f * scale, 72.0f * scale};
    const bool plus_hovered = interactive_hovered(state, input, plus_rect);
    const float plus_mix = smoothstep01(ui.presence("checkbox/rail-plus", plus_hovered, 18.0f, 16.0f));
    if (interactive_clicked(state, input, plus_rect)) {
        state.selected_nav = (state.selected_nav + 1) % static_cast<int>(kNavItems.size());
        ui.reset_motion("checkbox/page-reveal", 0.0f);
    }
    ui.shape()
        .in(plus_rect)
        .radius(plus_rect.w * 0.5f)
        .fill(mix_hex(0x1A1918, palette.panel_tint, plus_mix * 0.16f), 0.98f)
        .stroke(mix_hex(palette.border, palette.lime_soft, plus_mix * 0.10f), 1.0f, 0.92f)
        .draw();
    draw_fill(ui, Rect{plus_rect.x + 20.0f * scale, plus_rect.y + plus_rect.h * 0.5f - 1.0f * scale, plus_rect.w - 40.0f * scale, 2.0f * scale},
              palette.text, 1.0f * scale, 0.94f);
    draw_fill(ui, Rect{plus_rect.x + plus_rect.w * 0.5f - 1.0f * scale, plus_rect.y + 20.0f * scale, 2.0f * scale, plus_rect.h - 40.0f * scale},
              palette.text, 1.0f * scale, 0.94f);
}

void draw_metric_pair(UI& ui, const Rect& left_rect, const Rect& right_rect, float left_value, float right_value,
                      const char* left_label, const char* right_label, float scale, const Palette& palette) {
    char left_text[16]{};
    char right_text[16]{};
    format_percent_text(left_value, left_text, sizeof(left_text));
    format_percent_text(right_value, right_text, sizeof(right_text));

    draw_icon(ui, 0xF0D8u,
              Rect{left_rect.x, left_rect.y + 4.0f * scale, 14.0f * scale, 14.0f * scale},
              palette.lime, 0.98f);
    draw_text_left(ui, left_text,
                   Rect{left_rect.x, left_rect.y + 34.0f * scale, left_rect.w, 38.0f * scale},
                   font_value(scale) * 1.25f, palette.text, 1.0f);
    draw_text_left(ui, left_label,
                   Rect{left_rect.x, left_rect.y + 92.0f * scale, left_rect.w, 18.0f * scale},
                   font_body(scale), palette.text, 0.96f);

    draw_icon(ui, 0xF0D7u,
              Rect{right_rect.x, right_rect.y + 4.0f * scale, 14.0f * scale, 14.0f * scale},
              palette.orange, 0.98f);
    draw_text_left(ui, right_text,
                   Rect{right_rect.x, right_rect.y + 34.0f * scale, right_rect.w, 38.0f * scale},
                   font_value(scale) * 1.25f, palette.text, 1.0f);
    draw_text_left(ui, right_label,
                   Rect{right_rect.x, right_rect.y + 92.0f * scale, right_rect.w, 18.0f * scale},
                   font_body(scale), palette.text, 0.96f);
}

void draw_customer_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                        float scale, const Palette& palette, const ScreenPreset& preset) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const float rail_mix = state.selected_rail == 0 ? 0.14f : 0.0f;
    const float hover_mix = smoothstep01(ui.presence("checkbox/customer-card", card_hovered, 16.0f, 18.0f)) + rail_mix;
    const Rect visual = lifted_rect(ui, ui.id("checkbox/customer-card-lift"), rect, card_hovered, scale);
    paint_panel(ui, visual, 30.0f * scale, scale, palette, hover_mix, 1.0f, false);
    const Rect inner = inset_rect(visual, 28.0f * scale);

    draw_text_left(ui, "CUSTOMER",
                   Rect{inner.x, inner.y + 6.0f * scale, inner.w - 80.0f * scale, 20.0f * scale},
                   font_title(scale), palette.text, 1.0f);
    state.customer_menu_button_rect = Rect{inner.x + inner.w - 34.0f * scale, inner.y, 34.0f * scale, 22.0f * scale};
    draw_dots_button(ui, input, state, state.customer_menu_button_rect, OpenMenu::CustomerCard, scale, palette);

    const float date_offset = date_value_offset(state.selected_date);
    const float left_value = preset.customer_left + date_offset + static_cast<float>(state.selected_customer_mode) * 0.1f;
    const float right_value = preset.customer_right + date_offset * 0.6f + static_cast<float>(state.selected_customer_mode) * 0.08f;
    draw_metric_pair(ui,
                     Rect{inner.x, inner.y + 32.0f * scale, inner.w * 0.40f, 116.0f * scale},
                     Rect{inner.x + inner.w * 0.56f, inner.y + 32.0f * scale, inner.w * 0.32f, 116.0f * scale},
                     left_value, right_value, preset.customer_left_label, preset.customer_right_label, scale, palette);

    const Rect chart{inner.x - 4.0f * scale, inner.y + 154.0f * scale, inner.w + 8.0f * scale, inner.h - 178.0f * scale};
    const auto series_index = static_cast<std::size_t>((state.selected_customer_mode + state.selected_nav) % 3);
    const auto& lime = kCustomerSeriesLime[series_index];
    const auto& orange = kCustomerSeriesOrange[series_index];
    std::array<float, 9> points_x{};
    std::array<float, 9> lime_y{};
    std::array<float, 9> orange_y{};

    for (std::size_t i = 0; i < lime.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(lime.size() - 1);
        points_x[i] = chart.x + 8.0f * scale + t * (chart.w - 16.0f * scale);
        lime_y[i] = chart.y + chart.h * (1.0f - lime[i]) + 10.0f * scale;
        orange_y[i] = chart.y + chart.h * (1.0f - orange[i]) + 4.0f * scale;
    }

    int hovered_index = -1;
    if (interactive_hovered(state, input, chart)) {
        float best_distance = 1.0e9f;
        for (std::size_t i = 0; i < points_x.size(); ++i) {
            const float distance = std::fabs(points_x[i] - input.mouse_x);
            if (distance < best_distance) {
                best_distance = distance;
                hovered_index = static_cast<int>(i);
            }
        }
    }
    state.hovered_customer_point = hovered_index;

    ui.clip(chart, [&] {
        for (std::size_t i = 0; i + 1 < points_x.size(); ++i) {
            draw_segment(ui, points_x[i], lime_y[i], points_x[i + 1], lime_y[i + 1], 3.0f * scale, palette.lime, 0.92f);
            draw_segment(ui, points_x[i], orange_y[i], points_x[i + 1], orange_y[i + 1], 3.0f * scale, palette.orange, 0.92f);
        }
        if (hovered_index >= 0) {
            const std::size_t safe = static_cast<std::size_t>(hovered_index);
            draw_fill(ui, Rect{points_x[safe] - 6.0f * scale, lime_y[safe] - 6.0f * scale, 12.0f * scale, 12.0f * scale},
                      palette.lime, 6.0f * scale, 1.0f);
            draw_fill(ui, Rect{points_x[safe] - 6.0f * scale, orange_y[safe] - 6.0f * scale, 12.0f * scale, 12.0f * scale},
                      palette.orange, 6.0f * scale, 1.0f);
        }
    });

    const float tooltip_alpha = ui.presence("checkbox/customer-tooltip", hovered_index >= 0, 18.0f, 14.0f);
    if (hovered_index >= 0 && tooltip_alpha > 0.01f) {
        const std::size_t safe = static_cast<std::size_t>(hovered_index);
        const float target_x = points_x[safe];
        const float target_y = std::min(lime_y[safe], orange_y[safe]) - 38.0f * scale;
        const float bubble_x = ui.motion("checkbox/customer-tip-x", target_x, 18.0f);
        const float bubble_y = ui.motion("checkbox/customer-tip-y", target_y, 18.0f);
        const Rect bubble{bubble_x - 54.0f * scale, bubble_y - 18.0f * scale, 108.0f * scale, 36.0f * scale};
        ui.shape()
            .in(bubble)
            .radius(14.0f * scale)
            .fill(palette.shell, 0.96f * tooltip_alpha)
            .stroke(palette.border, 1.0f, 0.92f * tooltip_alpha)
            .shadow(0.0f, 10.0f * scale, 20.0f * scale, palette.shadow, 0.16f * tooltip_alpha)
            .draw();
        char value_pair[32]{};
        std::snprintf(value_pair, sizeof(value_pair), "%.0f / %.0f",
                      lime[safe] * 100.0f, orange[safe] * 100.0f);
        draw_text_center(ui, value_pair, bubble, font_meta(scale), palette.text, 0.98f * tooltip_alpha);
    }
}

void draw_product_matrix_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                              float scale, const Palette& palette, const ScreenPreset& preset) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const float rail_mix = state.selected_rail == 2 ? 0.14f : 0.0f;
    const float hover_mix = smoothstep01(ui.presence("checkbox/product-card", card_hovered, 16.0f, 18.0f)) + rail_mix;
    const Rect visual = lifted_rect(ui, ui.id("checkbox/product-card-lift"), rect, card_hovered, scale);
    paint_panel(ui, visual, 30.0f * scale, scale, palette, hover_mix, 1.0f, false);
    const Rect inner = inset_rect(visual, 28.0f * scale);

    draw_text_left(ui, "PRODUCT",
                   Rect{inner.x, inner.y + 6.0f * scale, inner.w - 80.0f * scale, 20.0f * scale},
                   font_title(scale), palette.text, 1.0f);
    state.product_menu_button_rect = Rect{inner.x + inner.w - 34.0f * scale, inner.y, 34.0f * scale, 22.0f * scale};
    draw_dots_button(ui, input, state, state.product_menu_button_rect, OpenMenu::ProductCard, scale, palette);

    const float date_offset = date_value_offset(state.selected_date) * 0.75f;
    const float left_value = preset.product_left + date_offset + static_cast<float>(state.selected_product_mode) * 0.12f;
    const float right_value = preset.product_right + date_offset * 0.6f + static_cast<float>(state.selected_product_mode) * 0.10f;
    draw_metric_pair(ui,
                     Rect{inner.x, inner.y + 32.0f * scale, inner.w * 0.40f, 116.0f * scale},
                     Rect{inner.x + inner.w * 0.56f, inner.y + 32.0f * scale, inner.w * 0.32f, 116.0f * scale},
                     left_value, right_value, preset.product_left_label, preset.product_right_label, scale, palette);

    const Rect grid{inner.x - 4.0f * scale, inner.y + 164.0f * scale, inner.w + 8.0f * scale, inner.h - 188.0f * scale};
    int hovered_dot = -1;
    int hovered_kind = -1;
    int dot_index = 0;
    ui.clip(grid, [&] {
        for (int column = 0; column < 10; ++column) {
            const float x = grid.x + 4.0f * scale + static_cast<float>(column) * ((grid.w - 8.0f * scale) / 9.0f);
            const int rows = 3 + ((column + state.selected_product_mode + state.selected_nav) % 4);
            for (int row = 0; row < rows; ++row) {
                const int kind = (column + row + state.selected_product_mode) % 3;
                const float y = grid.y + grid.h - 14.0f * scale - static_cast<float>(row) * 23.0f * scale
                                - static_cast<float>((column + row) % 2) * 3.0f * scale;
                const float base_size = (column >= 8 && row == rows - 1) ? 12.0f * scale : 10.0f * scale;
                const Rect dot_rect{x - base_size * 0.5f, y - base_size * 0.5f, base_size, base_size};
                const bool visible = timeline_category_visible(state, kind);
                const bool dot_hovered = interactive_hovered(state, input, Rect{x - 10.0f * scale, y - 10.0f * scale, 20.0f * scale, 20.0f * scale});
                if (dot_hovered) {
                    hovered_dot = dot_index;
                    hovered_kind = kind;
                    if (interactive_clicked(state, input, Rect{x - 10.0f * scale, y - 10.0f * scale, 20.0f * scale, 20.0f * scale})) {
                        state.selected_product_filter = kind + 1;
                    }
                }
                const float emphasis = smoothstep01(ui.presence(ui.id("checkbox/matrix-dot", static_cast<std::uint64_t>(dot_index)),
                                                                dot_hovered || (visible && state.selected_product_filter == kind + 1), 18.0f, 18.0f));
                const float alpha = visible ? 1.0f : 0.24f;
                const Rect visual_dot{dot_rect.x - emphasis * 1.0f * scale, dot_rect.y - emphasis * 1.0f * scale,
                                      dot_rect.w + emphasis * 2.0f * scale, dot_rect.h + emphasis * 2.0f * scale};
                if (emphasis > 0.01f) {
                    draw_soft_glow(ui, Rect{visual_dot.x - 6.0f * scale, visual_dot.y - 6.0f * scale,
                                            visual_dot.w + 12.0f * scale, visual_dot.h + 12.0f * scale},
                                   resource_color(palette, kind), palette.panel, 0.88f, 0.18f * emphasis * alpha);
                }
                draw_fill(ui, visual_dot, resource_color(palette, kind), visual_dot.w * 0.5f, alpha);
                ++dot_index;
            }
        }
    });
    state.hovered_matrix_dot = hovered_dot;
    if (hovered_kind >= 0) {
        const Rect tag{inner.x + inner.w - 116.0f * scale, inner.y + inner.h - 32.0f * scale, 116.0f * scale, 26.0f * scale};
        ui.shape().in(tag).radius(tag.h * 0.5f).fill(palette.shell, 0.96f).stroke(palette.border, 1.0f, 0.88f).draw();
        draw_text_center(ui, category_label(hovered_kind), tag, font_meta(scale), palette.text, 0.98f);
    }
}

void draw_capsule(UI& ui, const Rect& rect, int kind, int value, float scale, const Palette& palette, float alpha = 1.0f) {
    const std::uint32_t fill = resource_color(palette, kind);
    ui.shape()
        .in(rect)
        .radius(rect.w * 0.5f)
        .fill(fill, alpha)
        .stroke((kind == 0) ? palette.border : darken_hex(fill, 0.24f), 1.0f, 0.55f * alpha)
        .shadow(0.0f, 6.0f * scale, 16.0f * scale, palette.shadow, 0.08f * alpha)
        .draw();
    char text[12]{};
    format_int_text(value, text, sizeof(text));
    draw_text_center(ui, text, rect, font_capsule(scale), palette.shell, 0.98f * alpha);
}

void draw_product_bubbles_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                               float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const float rail_mix = state.selected_rail == 1 ? 0.12f : 0.0f;
    const float hover_mix = smoothstep01(ui.presence("checkbox/bubble-card", card_hovered, 16.0f, 18.0f)) + rail_mix;
    const Rect visual = lifted_rect(ui, ui.id("checkbox/bubble-card-lift"), rect, card_hovered, scale);
    paint_panel(ui, visual, 34.0f * scale, scale, palette, hover_mix, 1.0f, false);
    const Rect inner = inset_rect(visual, 30.0f * scale);

    draw_text_left(ui, "PRODUCT",
                   Rect{inner.x, inner.y + 4.0f * scale, inner.w - 80.0f * scale, 20.0f * scale},
                   font_title(scale), palette.text, 1.0f);
    state.bubble_menu_button_rect = Rect{inner.x + inner.w - 34.0f * scale, inner.y, 34.0f * scale, 22.0f * scale};
    draw_dots_button(ui, input, state, state.bubble_menu_button_rect, OpenMenu::BubbleCard, scale, palette);

    const Rect plot{inner.x + 6.0f * scale, inner.y + 30.0f * scale, inner.w - 12.0f * scale, inner.h - 126.0f * scale};
    const float baseline_y = plot.y + plot.h * 0.55f;
    const float column_gap = plot.w / static_cast<float>(kBubblePairs.size());
    const float column_width = 52.0f * scale;
    int hovered_column = -1;
    int visible_total = 0;

    ui.clip(plot, [&] {
        for (std::size_t i = 0; i < kBubblePairs.size(); ++i) {
            const BubblePair pair = kBubblePairs[i];
            const float center_x = plot.x + column_gap * (static_cast<float>(i) + 0.5f);
            const Rect hover_rect{center_x - column_gap * 0.45f, plot.y, column_gap * 0.9f, plot.h};
            const bool column_hovered = interactive_hovered(state, input, hover_rect);
            if (column_hovered) {
                hovered_column = static_cast<int>(i);
            }
            const float mix = smoothstep01(ui.presence(ui.id("checkbox/bubble-column", static_cast<std::uint64_t>(i)),
                                                       column_hovered, 16.0f, 18.0f));

            draw_fill(ui, Rect{center_x - 0.5f * scale, plot.y + 18.0f * scale, 1.0f * scale, plot.h - 36.0f * scale},
                      palette.border, 0.0f, 0.78f);

            const float top_h = (30.0f + static_cast<float>(pair.top_value) * 0.42f) * scale;
            const float bottom_h = (30.0f + static_cast<float>(pair.bottom_value) * 0.34f) * scale;
            const float top_y = plot.y + 26.0f * scale + (100.0f - static_cast<float>(pair.top_value)) * 0.85f * scale;
            const float bottom_y = baseline_y + 16.0f * scale + (100.0f - static_cast<float>(pair.bottom_value)) * 0.16f * scale;
            const Rect top_rect{center_x - column_width * 0.5f, top_y - mix * 5.0f * scale, column_width, top_h};
            const Rect bottom_rect{center_x - column_width * 0.5f, bottom_y + (1.0f - mix) * 2.0f * scale, column_width, bottom_h};
            const Rect dot_rect{center_x - 6.0f * scale, baseline_y - 6.0f * scale, 12.0f * scale, 12.0f * scale};

            const float top_alpha = resource_visible(state, pair.top_kind) ? 1.0f : 0.18f;
            const float bottom_alpha = resource_visible(state, pair.bottom_kind) ? 1.0f : 0.18f;
            draw_capsule(ui, top_rect, pair.top_kind, pair.top_value, scale, palette, top_alpha);
            draw_capsule(ui, bottom_rect, pair.bottom_kind, pair.bottom_value, scale, palette, bottom_alpha);
            draw_fill(ui, dot_rect, resource_color(palette, pair.dot_kind), dot_rect.w * 0.5f,
                      resource_visible(state, pair.dot_kind) ? 1.0f : 0.18f);

            if (resource_visible(state, pair.top_kind)) {
                visible_total += pair.top_value;
            }
            if (resource_visible(state, pair.bottom_kind)) {
                visible_total += pair.bottom_value;
            }
        }
    });

    state.hovered_bubble_column = hovered_column;

    const float tooltip_alpha = ui.presence("checkbox/bubble-tooltip", hovered_column >= 0, 18.0f, 14.0f);
    if (hovered_column >= 0 && tooltip_alpha > 0.01f) {
        const BubblePair pair = kBubblePairs[static_cast<std::size_t>(hovered_column)];
        const float center_x = plot.x + column_gap * (static_cast<float>(hovered_column) + 0.5f);
        const float x = ui.motion("checkbox/bubble-tip-x", center_x, 18.0f);
        const float y = ui.motion("checkbox/bubble-tip-y", plot.y + 18.0f * scale, 18.0f);
        const Rect bubble{x - 72.0f * scale, y - 8.0f * scale, 144.0f * scale, 38.0f * scale};
        ui.shape()
            .in(bubble)
            .radius(14.0f * scale)
            .fill(palette.shell, 0.96f * tooltip_alpha)
            .stroke(palette.border, 1.0f, 0.90f * tooltip_alpha)
            .shadow(0.0f, 10.0f * scale, 20.0f * scale, palette.shadow, 0.14f * tooltip_alpha)
            .draw();
        char summary[32]{};
        std::snprintf(summary, sizeof(summary), "%d / %d", pair.top_value, pair.bottom_value);
        draw_text_center(ui, summary, bubble, font_meta(scale), palette.text, 0.98f * tooltip_alpha);
    }

    const Rect legend_row{inner.x + 2.0f * scale, visual.y + visual.h - 58.0f * scale, inner.w - 4.0f * scale, 28.0f * scale};
    const std::array<const char*, 3> labels{{"Resources", "Valid", "Invalid"}};
    for (int index = 0; index < 3; ++index) {
        const float x = legend_row.x + static_cast<float>(index) * 124.0f * scale;
        const Rect item{x, legend_row.y, 104.0f * scale, legend_row.h};
        const bool selected = state.product_legend == index;
        const bool item_hovered = interactive_hovered(state, input, item);
        const float mix = smoothstep01(ui.presence(ui.id("checkbox/bubble-legend", static_cast<std::uint64_t>(index)),
                                                   selected || item_hovered, 18.0f, 18.0f));
        if (interactive_clicked(state, input, item)) {
            state.product_legend = (state.product_legend == index) ? -1 : index;
        }
        if (selected || item_hovered) {
            draw_fill(ui, item, mix_hex(palette.panel_alt, resource_color(palette, index), mix * 0.10f),
                      item.h * 0.5f, 0.92f);
        }
        draw_fill(ui, Rect{item.x + 2.0f * scale, item.y + 6.0f * scale, 16.0f * scale, 16.0f * scale},
                  resource_color(palette, index), 8.0f * scale, 1.0f);
        draw_fill(ui, Rect{item.x + 7.0f * scale, item.y + 11.0f * scale, 6.0f * scale, 6.0f * scale},
                  palette.shell, 3.0f * scale, 1.0f);
        draw_text_left(ui, labels[static_cast<std::size_t>(index)],
                       Rect{item.x + 26.0f * scale, item.y + 6.0f * scale, item.w - 26.0f * scale, 16.0f * scale},
                       font_body(scale), palette.text, 0.98f);
    }

    char total_text[32]{};
    std::snprintf(total_text, sizeof(total_text), "Total: %d", visible_total);
    draw_text_right(ui, total_text,
                    Rect{legend_row.x + legend_row.w - 160.0f * scale, legend_row.y + 6.0f * scale, 160.0f * scale, 18.0f * scale},
                    font_body(scale), palette.muted, 0.98f);
}

void draw_task_badge(UI& ui, const Rect& rect, int badge_kind, float scale, const Palette& palette) {
    switch (badge_kind) {
        case 0: {
            draw_fill(ui, rect, palette.white_soft, rect.w * 0.5f, 1.0f);
            draw_text_center(ui, "S", rect, font_body(scale), palette.blue, 1.0f);
            break;
        }
        case 1: {
            draw_fill(ui, rect, 0x101010, rect.w * 0.5f, 1.0f);
            draw_text_center(ui, "X", rect, font_body(scale), palette.orange, 1.0f);
            break;
        }
        case 2: {
            draw_avatar_stack(ui, Rect{rect.x - 4.0f * scale, rect.y, rect.w + 32.0f * scale, rect.h}, scale, 3);
            break;
        }
        case 3: {
            draw_fill(ui, rect, 0xE1529A, rect.w * 0.5f, 1.0f);
            draw_fill(ui, Rect{rect.x + rect.w * 0.48f, rect.y + 2.0f * scale, 3.0f * scale, rect.h - 4.0f * scale},
                      0xFFFFFF, 1.0f * scale, 0.78f);
            draw_fill(ui, Rect{rect.x + 2.0f * scale, rect.y + rect.h * 0.48f, rect.w - 4.0f * scale, 3.0f * scale},
                      0xFFFFFF, 1.0f * scale, 0.78f);
            break;
        }
        case 4: {
            draw_fill(ui, rect, 0x5865F2, rect.w * 0.5f, 1.0f);
            draw_text_center(ui, "D", rect, font_meta(scale), 0xFFFFFF, 1.0f);
            break;
        }
        case 5: {
            draw_fill(ui, rect, 0x2475F2, rect.w * 0.5f, 1.0f);
            draw_text_center(ui, "f", rect, font_body(scale), 0xFFFFFF, 1.0f);
            break;
        }
        case 6: {
            draw_avatar_stack(ui, Rect{rect.x - 6.0f * scale, rect.y, rect.w + 44.0f * scale, rect.h}, scale, 4);
            break;
        }
        case 7:
        default: {
            draw_fill(ui, rect, 0x3BA5F5, rect.w * 0.5f, 1.0f);
            draw_text_center(ui, "t", rect, font_body(scale), 0xFFFFFF, 1.0f);
            break;
        }
    }
}

void draw_timeline_card(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                        float scale, const Palette& palette) {
    const bool card_hovered = interactive_hovered(state, input, rect);
    const float hover_mix = smoothstep01(ui.presence("checkbox/timeline-card", card_hovered, 16.0f, 18.0f))
                            + (state.selected_rail == 1 ? 0.12f : 0.0f);
    const Rect visual = lifted_rect(ui, ui.id("checkbox/timeline-card-lift"), rect, card_hovered, scale);
    paint_panel(ui, visual, 34.0f * scale, scale, palette, hover_mix, 1.0f, false);
    const Rect inner = inset_rect(visual, 32.0f * scale);

    draw_text_left(ui, "PROJECTS TIMELINE",
                   Rect{inner.x, inner.y + 2.0f * scale, inner.w - 80.0f * scale, 20.0f * scale},
                   font_title(scale), palette.text, 1.0f);
    state.timeline_menu_button_rect = Rect{inner.x + inner.w - 34.0f * scale, inner.y, 34.0f * scale, 22.0f * scale};
    draw_dots_button(ui, input, state, state.timeline_menu_button_rect, OpenMenu::TimelineCard, scale, palette);

    const Rect plot{inner.x, inner.y + 40.0f * scale, inner.w, inner.h - 114.0f * scale};
    const float label_w = 78.0f * scale;
    const Rect axis{plot.x + label_w + 10.0f * scale, plot.y + 16.0f * scale, plot.w - label_w - 22.0f * scale, plot.h - 58.0f * scale};
    const float duration_scale = date_duration_scale(state.selected_date);
    const float count_scale = 1.0f + static_cast<float>(state.selected_profile) * 0.04f;
    const float row_h = axis.h / static_cast<float>(kTimelineTasks.size());
    int hovered_task = -1;
    int visible_total = 0;

    for (int tick = 0; tick <= 6; ++tick) {
        const float x = axis.x + axis.w * (static_cast<float>(tick) / 6.0f);
        draw_fill(ui, Rect{x - 0.5f * scale, axis.y, 1.0f * scale, axis.h}, palette.border, 0.0f, 0.80f);
    }

    for (std::size_t index = 0; index < kTimelineTasks.size(); ++index) {
        const TimelineTask task = kTimelineTasks[index];
        const float row_y = axis.y + row_h * static_cast<float>(index);
        draw_text_left(ui, task.date,
                       Rect{plot.x, row_y + row_h * 0.27f, label_w, 18.0f * scale},
                       font_body(scale), palette.text, 0.98f);

        const float duration = task.duration * duration_scale;
        const float start = task.start * duration_scale;
        const float count_value = static_cast<float>(task.count) * count_scale;
        const float x = axis.x + axis.w * (start / 30.0f);
        const float w = axis.w * (duration / 30.0f);
        const Rect bar{x, row_y + row_h * 0.19f, w, 42.0f * scale};
        const bool visible = timeline_category_visible(state, task.category);
        const bool bar_hovered = visible && interactive_hovered(state, input, bar);
        if (bar_hovered) {
            hovered_task = static_cast<int>(index);
        }
        if (visible && interactive_clicked(state, input, bar)) {
            state.selected_timeline = static_cast<int>(index);
        }
        const bool selected = state.selected_timeline == static_cast<int>(index);
        const float mix = smoothstep01(ui.presence(ui.id("checkbox/timeline-bar", static_cast<std::uint64_t>(index)),
                                                   selected || bar_hovered, 16.0f, 18.0f));
        const float alpha = visible ? 1.0f : 0.16f;
        const Rect visual_bar{bar.x, bar.y - mix * 3.0f * scale, bar.w, bar.h};
        ui.shape()
            .in(visual_bar)
            .radius(visual_bar.h * 0.5f)
            .fill(timeline_color(palette, task.category), alpha)
            .stroke((task.category == 2) ? palette.border : darken_hex(timeline_color(palette, task.category), 0.28f),
                    1.0f, 0.35f * alpha)
            .shadow(0.0f, 8.0f * scale, 20.0f * scale,
                    (task.category == 2) ? palette.shadow : darken_hex(timeline_color(palette, task.category), 0.62f),
                    (0.10f + mix * 0.04f) * alpha)
            .draw();

        const Rect badge{visual_bar.x + 10.0f * scale, visual_bar.y + 7.0f * scale, 28.0f * scale, 28.0f * scale};
        draw_task_badge(ui, badge, task.badge_kind, scale, palette);
        char count_text[12]{};
        format_int_text(static_cast<int>(std::lround(count_value)), count_text, sizeof(count_text));
        draw_text_right(ui, count_text,
                        Rect{visual_bar.x + visual_bar.w - 36.0f * scale, visual_bar.y + 12.0f * scale, 28.0f * scale, 18.0f * scale},
                        font_meta(scale), palette.shell, 0.92f * alpha);
        if (visible) {
            visible_total += static_cast<int>(std::lround(count_value));
        }
    }
    state.hovered_timeline = hovered_task;

    const int tooltip_index = (hovered_task >= 0) ? hovered_task : state.selected_timeline;
    const bool tooltip_visible = tooltip_index >= 0
                                 && tooltip_index < static_cast<int>(kTimelineTasks.size())
                                 && timeline_category_visible(state, kTimelineTasks[static_cast<std::size_t>(tooltip_index)].category);
    const float tooltip_alpha = ui.presence("checkbox/timeline-tooltip", tooltip_visible, 18.0f, 14.0f);
    if (tooltip_visible && tooltip_alpha > 0.01f) {
        const TimelineTask task = kTimelineTasks[static_cast<std::size_t>(tooltip_index)];
        const float duration = task.duration * duration_scale;
        const float start = task.start * duration_scale;
        const float row_y = axis.y + row_h * static_cast<float>(tooltip_index);
        const float center_x = axis.x + axis.w * ((start + duration * 0.5f) / 30.0f);
        const float anchor_y = row_y - 6.0f * scale;
        const float x = ui.motion("checkbox/timeline-tip-x", center_x, 18.0f);
        const float y = ui.motion("checkbox/timeline-tip-y", anchor_y, 18.0f);
        const Rect bubble{x - 90.0f * scale, y - 34.0f * scale, 180.0f * scale, 44.0f * scale};
        ui.shape()
            .in(bubble)
            .radius(16.0f * scale)
            .fill(palette.shell, 0.96f * tooltip_alpha)
            .stroke(palette.border, 1.0f, 0.90f * tooltip_alpha)
            .shadow(0.0f, 10.0f * scale, 22.0f * scale, palette.shadow, 0.16f * tooltip_alpha)
            .draw();
        draw_text_left(ui, category_label(task.category),
                       Rect{bubble.x + 12.0f * scale, bubble.y + 8.0f * scale, bubble.w - 24.0f * scale, 16.0f * scale},
                       font_meta(scale), timeline_color(palette, task.category), 0.98f * tooltip_alpha);
        char detail[32]{};
        std::snprintf(detail, sizeof(detail), "%d active items", static_cast<int>(std::lround(static_cast<float>(task.count) * count_scale)));
        draw_text_left(ui, detail,
                       Rect{bubble.x + 12.0f * scale, bubble.y + 24.0f * scale, bubble.w - 24.0f * scale, 16.0f * scale},
                       font_meta(scale), palette.text, 0.96f * tooltip_alpha);
    }

    const Rect axis_label_row{axis.x, axis.y + axis.h + 14.0f * scale, axis.w, 18.0f * scale};
    for (int tick = 0; tick <= 6; ++tick) {
        const float x = axis_label_row.x + axis_label_row.w * (static_cast<float>(tick) / 6.0f);
        char label[8]{};
        std::snprintf(label, sizeof(label), "%d", tick * 5);
        draw_text_center(ui, label,
                         Rect{x - 12.0f * scale, axis_label_row.y, 24.0f * scale, axis_label_row.h},
                         font_body(scale), palette.text, 0.98f);
    }

    const Rect legend{inner.x, visual.y + visual.h - 54.0f * scale, inner.w, 24.0f * scale};
    for (int index = 0; index < 3; ++index) {
        const Rect item{legend.x + static_cast<float>(index) * 138.0f * scale, legend.y, 118.0f * scale, legend.h};
        const bool selected = state.selected_product_filter == index + 1;
        const bool item_hovered = interactive_hovered(state, input, item);
        const float mix = smoothstep01(ui.presence(ui.id("checkbox/timeline-legend", static_cast<std::uint64_t>(index)),
                                                   selected || item_hovered, 18.0f, 18.0f));
        if (interactive_clicked(state, input, item)) {
            state.selected_product_filter = (state.selected_product_filter == index + 1) ? 0 : (index + 1);
        }
        if (selected || item_hovered) {
            draw_fill(ui, item, mix_hex(palette.panel_alt, timeline_color(palette, index), mix * 0.12f), item.h * 0.5f, 0.92f);
        }
        draw_fill(ui, Rect{item.x + 2.0f * scale, item.y + 4.0f * scale, 18.0f * scale, 18.0f * scale},
                  timeline_color(palette, index), 9.0f * scale, 1.0f);
        draw_fill(ui, Rect{item.x + 7.0f * scale, item.y + 9.0f * scale, 8.0f * scale, 8.0f * scale},
                  palette.shell, 4.0f * scale, 1.0f);
        draw_text_left(ui, category_label(index),
                       Rect{item.x + 28.0f * scale, item.y + 4.0f * scale, item.w - 28.0f * scale, 16.0f * scale},
                       font_body(scale), palette.text, 0.98f);
    }

    char total_text[32]{};
    std::snprintf(total_text, sizeof(total_text), "Total: %d", visible_total);
    draw_text_right(ui, total_text,
                    Rect{legend.x + legend.w - 140.0f * scale, legend.y + 4.0f * scale, 140.0f * scale, 16.0f * scale},
                    font_body(scale), palette.muted, 0.98f);
}

template <std::size_t N>
void draw_menu_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                       const Rect& button, OpenMenu menu_id, int& selected_index,
                       const std::array<const char*, N>& labels, float scale,
                       bool align_right = false, float min_width = 150.0f) {
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float menu_mix = smoothstep01(ui.presence(overlay_motion_id(ui, menu_id), state.open_menu == menu_id, 18.0f, 16.0f));
    if (menu_mix <= 0.01f) {
        return;
    }

    const float width = std::max(button.w, min_width * scale);
    const Rect base_menu{
        align_right ? (button.x + button.w - width) : button.x,
        button.y + button.h + 8.0f * scale,
        width,
        10.0f * scale + static_cast<float>(N) * 34.0f * scale,
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
        .fill(palette.panel_alt, 0.36f * menu_mix)
        .stroke(palette.white_soft, 1.0f, 0.08f * menu_mix)
        .blur(16.0f * scale, 24.0f * scale)
        .shadow(0.0f, 8.0f * scale, 24.0f * scale, palette.shadow, 0.14f * menu_mix)
        .draw();
    ui.shape()
        .in(inset_rect(menu, 1.0f))
        .radius(17.0f * scale)
        .fill(0x161514, 0.96f * menu_mix)
        .stroke(palette.border, 1.0f, 0.92f * menu_mix)
        .draw();

    ui.ctx().set_global_alpha(menu_mix);
    for (std::size_t i = 0; i < N; ++i) {
        const Rect item{
            menu.x + 6.0f * scale,
            menu.y + 5.0f * scale + static_cast<float>(i) * 34.0f * scale + (1.0f - menu_mix) * 5.0f * scale,
            menu.w - 12.0f * scale,
            30.0f * scale,
        };
        const bool item_hovered = hovered(input, item);
        const bool item_selected = selected_index == static_cast<int>(i);
        if (item_hovered || item_selected) {
            ui.shape()
                .in(item)
                .radius(item.h * 0.5f)
                .fill(mix_hex(palette.panel_tint, palette.lime_soft, item_selected ? 0.12f : 0.05f),
                      item_selected ? 1.0f : 0.94f)
                .draw();
        }
        draw_text_left(ui, labels[i],
                       Rect{item.x + 12.0f * scale, item.y + 6.0f * scale, item.w - 24.0f * scale, 16.0f * scale},
                       font_meta(scale), item_selected ? palette.lime_soft : palette.text, 0.98f);
        if (menu_mix > 0.9f && clicked(input, item)) {
            selected_index = static_cast<int>(i);
            state.open_menu = OpenMenu::None;
        }
    }
    ui.ctx().set_global_alpha(1.0f);

    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base_menu.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void apply_search_suggestion(DashboardState& state, const SearchSuggestion& suggestion) {
    if (suggestion.nav_index >= 0) {
        state.selected_nav = suggestion.nav_index;
    }
    if (suggestion.rail_index >= 0) {
        state.selected_rail = suggestion.rail_index;
    }
    if (suggestion.product_filter >= 0) {
        state.selected_product_filter = suggestion.product_filter;
    }
    if (suggestion.resource_filter >= 0) {
        state.product_legend = suggestion.resource_filter;
    }
}

void draw_search_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette, float scale) {
    const Rect button = state.search_button_rect;
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float mix = smoothstep01(ui.presence(overlay_motion_id(ui, OpenMenu::Search), state.open_menu == OpenMenu::Search, 18.0f, 16.0f));
    if (mix <= 0.01f) {
        return;
    }
    const Rect base{
        std::max(24.0f * scale, button.x - 116.0f * scale),
        button.y + button.h + 10.0f * scale,
        320.0f * scale,
        220.0f * scale,
    };
    const Rect menu{base.x, base.y + (1.0f - mix) * 10.0f * scale, base.w, base.h};
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});
    ui.shape()
        .in(menu)
        .radius(22.0f * scale)
        .fill(palette.panel_alt, 0.34f * mix)
        .stroke(palette.white_soft, 1.0f, 0.08f * mix)
        .blur(18.0f * scale, 26.0f * scale)
        .shadow(0.0f, 8.0f * scale, 24.0f * scale, palette.shadow, 0.14f * mix)
        .draw();
    ui.shape()
        .in(inset_rect(menu, 1.0f))
        .radius(21.0f * scale)
        .fill(0x141312, 0.98f * mix)
        .stroke(palette.border, 1.0f, 0.92f * mix)
        .draw();

    const Rect field{menu.x + 14.0f * scale, menu.y + 14.0f * scale, menu.w - 28.0f * scale, 42.0f * scale};
    ui.scope(field, [&](auto&) {
        ui.input("", state.search_text)
            .height(field.h)
            .padding_left(26.0f * scale)
            .placeholder("Search dashboard...")
            .draw();
    });
    draw_icon(ui, 0xF002u,
              Rect{field.x + 10.0f * scale, field.y + 12.0f * scale, 16.0f * scale, 16.0f * scale},
              palette.muted, 0.88f);

    ui.ctx().set_global_alpha(mix);
    int shown = 0;
    for (std::size_t i = 0; i < kSearchSuggestions.size(); ++i) {
        const SearchSuggestion& suggestion = kSearchSuggestions[i];
        if (!contains_case_insensitive(suggestion.label, state.search_text)
            && !contains_case_insensitive(suggestion.caption, state.search_text)) {
            continue;
        }
        const Rect item{
            menu.x + 10.0f * scale,
            menu.y + 64.0f * scale + static_cast<float>(shown) * 38.0f * scale,
            menu.w - 20.0f * scale,
            34.0f * scale,
        };
        const bool item_hovered = hovered(input, item);
        if (item_hovered) {
            draw_fill(ui, item, mix_hex(palette.panel_tint, palette.lime_soft, 0.10f), 12.0f * scale, 0.96f);
        }
        draw_icon(ui, suggestion.icon,
                  Rect{item.x + 10.0f * scale, item.y + 8.0f * scale, 16.0f * scale, 16.0f * scale},
                  item_hovered ? palette.lime_soft : palette.text, 0.98f);
        draw_text_left(ui, suggestion.label,
                       Rect{item.x + 34.0f * scale, item.y + 3.0f * scale, item.w - 40.0f * scale, 14.0f * scale},
                       font_body(scale), palette.text, 0.98f);
        draw_text_left(ui, suggestion.caption,
                       Rect{item.x + 34.0f * scale, item.y + 18.0f * scale, item.w - 40.0f * scale, 12.0f * scale},
                       font_meta(scale), palette.muted, 0.94f);
        if (mix > 0.9f && clicked(input, item)) {
            apply_search_suggestion(state, suggestion);
            state.search_text = suggestion.label;
            state.open_menu = OpenMenu::None;
        }
        ++shown;
        if (shown >= 4) {
            break;
        }
    }
    if (shown == 0) {
        draw_text_left(ui, "No matching dashboard shortcuts",
                       Rect{menu.x + 18.0f * scale, menu.y + 86.0f * scale, menu.w - 36.0f * scale, 16.0f * scale},
                       font_body(scale), palette.text, 0.88f);
        draw_text_left(ui, "Try customer, product, or support.",
                       Rect{menu.x + 18.0f * scale, menu.y + 110.0f * scale, menu.w - 36.0f * scale, 14.0f * scale},
                       font_meta(scale), palette.muted, 0.92f);
    }
    ui.ctx().set_global_alpha(1.0f);

    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_notifications_overlay(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette, float scale) {
    const Rect button = state.header_profile_rect;
    if (button.w <= 0.0f || button.h <= 0.0f) {
        return;
    }
    const float mix = smoothstep01(ui.presence(overlay_motion_id(ui, OpenMenu::Notifications), state.open_menu == OpenMenu::Notifications, 18.0f, 16.0f));
    if (mix <= 0.01f) {
        return;
    }
    const Rect base{
        button.x + button.w - 272.0f * scale,
        button.y + button.h + 10.0f * scale,
        272.0f * scale,
        146.0f * scale,
    };
    const Rect menu{base.x, base.y + (1.0f - mix) * 10.0f * scale, base.w, base.h};
    set_overlay_block(state, Rect{menu.x - 10.0f * scale, menu.y - 10.0f * scale, menu.w + 20.0f * scale, menu.h + 20.0f * scale});
    ui.shape()
        .in(menu)
        .radius(20.0f * scale)
        .fill(palette.panel_alt, 0.34f * mix)
        .stroke(palette.white_soft, 1.0f, 0.08f * mix)
        .blur(18.0f * scale, 26.0f * scale)
        .shadow(0.0f, 8.0f * scale, 24.0f * scale, palette.shadow, 0.14f * mix)
        .draw();
    ui.shape()
        .in(inset_rect(menu, 1.0f))
        .radius(19.0f * scale)
        .fill(0x141312, 0.98f * mix)
        .stroke(palette.border, 1.0f, 0.92f * mix)
        .draw();

    draw_text_left(ui, "Notifications",
                   Rect{menu.x + 16.0f * scale, menu.y + 12.0f * scale, menu.w - 32.0f * scale, 18.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_right(ui, "2 new",
                    Rect{menu.x + menu.w - 70.0f * scale, menu.y + 12.0f * scale, 54.0f * scale, 18.0f * scale},
                    font_meta(scale), palette.red, 0.98f);

    const Rect first{menu.x + 10.0f * scale, menu.y + 42.0f * scale, menu.w - 20.0f * scale, 38.0f * scale};
    const Rect second{menu.x + 10.0f * scale, menu.y + 84.0f * scale, menu.w - 20.0f * scale, 38.0f * scale};
    const bool first_hovered = hovered(input, first);
    const bool second_hovered = hovered(input, second);
    if (first_hovered) {
        draw_fill(ui, first, mix_hex(palette.panel_tint, palette.orange_soft, 0.08f), 14.0f * scale, 0.96f);
    }
    if (second_hovered) {
        draw_fill(ui, second, mix_hex(palette.panel_tint, palette.lime_soft, 0.08f), 14.0f * scale, 0.96f);
    }
    draw_fill(ui, Rect{first.x + 10.0f * scale, first.y + 10.0f * scale, 18.0f * scale, 18.0f * scale}, palette.orange, 9.0f * scale, 1.0f);
    draw_fill(ui, Rect{second.x + 10.0f * scale, second.y + 10.0f * scale, 18.0f * scale, 18.0f * scale}, palette.lime, 9.0f * scale, 1.0f);
    draw_text_left(ui, "Product row moved to 29.09",
                   Rect{first.x + 36.0f * scale, first.y + 4.0f * scale, first.w - 46.0f * scale, 14.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "Timeline card",
                   Rect{first.x + 36.0f * scale, first.y + 20.0f * scale, first.w - 46.0f * scale, 12.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);
    draw_text_left(ui, "Customer card needs review",
                   Rect{second.x + 36.0f * scale, second.y + 4.0f * scale, second.w - 46.0f * scale, 14.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "Customer panel",
                   Rect{second.x + 36.0f * scale, second.y + 20.0f * scale, second.w - 46.0f * scale, 12.0f * scale},
                   font_meta(scale), palette.muted, 0.94f);

    if (clicked(input, first)) {
        state.selected_product_filter = 2;
        state.selected_timeline = 1;
        state.open_menu = OpenMenu::None;
    }
    if (clicked(input, second)) {
        state.selected_rail = 0;
        state.selected_customer_mode = 1;
        state.open_menu = OpenMenu::None;
    }
    if (input.mouse_pressed && !button.contains(input.mouse_x, input.mouse_y) && !base.contains(input.mouse_x, input.mouse_y)) {
        state.open_menu = OpenMenu::None;
    }
}

void draw_overlay_menus(UI& ui, const eui::InputState& input, DashboardState& state, const Palette& palette,
                        const Rect& clip_rect, float scale) {
    ui.clip(clip_rect, [&] {
        draw_search_overlay(ui, input, state, palette, scale);
        draw_notifications_overlay(ui, input, state, palette, scale);
        draw_menu_overlay(ui, input, state, palette, state.date_button_rect,
                          OpenMenu::Date, state.selected_date, kDateOptions, scale, false, 176.0f);
        draw_menu_overlay(ui, input, state, palette, state.product_button_rect,
                          OpenMenu::Product, state.selected_product_filter, kProductFilters, scale, false, 188.0f);
        draw_menu_overlay(ui, input, state, palette, state.profile_button_rect,
                          OpenMenu::Profile, state.selected_profile, kProfileShortNames, scale, false, 188.0f);
        draw_menu_overlay(ui, input, state, palette, state.glow_button_rect,
                          OpenMenu::Glow, state.glow_mode, kGlowModes, scale, true, 156.0f);
        draw_menu_overlay(ui, input, state, palette, state.customer_menu_button_rect,
                          OpenMenu::CustomerCard, state.selected_customer_mode, kCustomerModes, scale, true, 170.0f);
        draw_menu_overlay(ui, input, state, palette, state.product_menu_button_rect,
                          OpenMenu::ProductCard, state.selected_product_mode, kProductModes, scale, true, 170.0f);
        draw_menu_overlay(ui, input, state, palette, state.bubble_menu_button_rect,
                          OpenMenu::BubbleCard, state.product_legend, kBubbleModes, scale, true, 156.0f);
        draw_menu_overlay(ui, input, state, palette, state.timeline_menu_button_rect,
                          OpenMenu::TimelineCard, state.selected_product_filter, kTimelineModes, scale, true, 166.0f);
    });
}

void draw_dashboard(UI& ui, const eui::InputState& input, DashboardState& state, const Rect& rect,
                    float scale, const Palette& palette, double time_seconds) {
    const float halo_alpha = glow_alpha(state);
    const float halo_scale = glow_radius_mix(state);
    draw_soft_glow(ui,
                   Rect{rect.x - 140.0f * scale * halo_scale, rect.y + rect.h - 180.0f * scale,
                        rect.w * 0.68f * halo_scale, 240.0f * scale * halo_scale},
                   palette.background_alt, palette.background, 0.92f, halo_alpha);
    draw_soft_glow(ui,
                   Rect{rect.x + rect.w * 0.62f - 180.0f * scale * halo_scale,
                        rect.y + rect.h - 160.0f * scale, 340.0f * scale * halo_scale, 240.0f * scale * halo_scale},
                   palette.lime_soft, palette.background, 0.88f, halo_alpha * 0.65f);

    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(time_seconds * 0.8));
    ui.shape()
        .in(rect)
        .radius(48.0f * scale)
        .gradient(palette.shell, darken_hex(palette.shell_soft, 0.18f), 1.0f)
        .stroke(mix_hex(palette.border, palette.lime_soft, 0.04f), 1.0f, 0.82f)
        .shadow(0.0f, 34.0f * scale, 82.0f * scale, 0x68834A, 0.22f)
        .draw();

    ui.clip(rect, [&] {
        draw_soft_glow(ui,
                       Rect{rect.x + rect.w * 0.58f - 120.0f * scale, rect.y - 120.0f * scale, 260.0f * scale, 220.0f * scale},
                       palette.lime, palette.shell, 0.88f, halo_alpha * 0.18f * (0.8f + pulse * 0.2f));
        draw_soft_glow(ui,
                       Rect{rect.x + 40.0f * scale, rect.y + rect.h * 0.56f, 220.0f * scale, 220.0f * scale},
                       palette.orange_soft, palette.shell, 0.88f, halo_alpha * 0.08f);

        const Rect content = inset_rect(rect, 42.0f * scale);
        ui.column(content)
            .tracks({px(80.0f * scale), fr()})
            .gap(30.0f * scale)
            .begin([&](auto& rows) {
                draw_header(ui, input, state, rows.next(), scale, palette);
                const Rect body = rows.next();
                ui.row(body)
                    .tracks({px(74.0f * scale), px(730.0f * scale), fr()})
                    .gap(28.0f * scale)
                    .begin([&](auto& cols) {
                        draw_rail(ui, input, state, cols.next(), scale, palette);
                        const Rect left = cols.next();
                        const Rect right = cols.next();
                        const ScreenPreset& preset = kPresets[static_cast<std::size_t>(state.selected_nav)];
                        const float reveal = smoothstep01(ui.motion("checkbox/page-reveal", 1.0f, 8.0f));
                        const float dx = (1.0f - reveal) * 20.0f * scale;
                        const float dy = (1.0f - reveal) * 8.0f * scale;
                        ui.ctx().set_global_alpha(reveal);
                        ui.column(Rect{left.x + dx, left.y + dy, left.w, left.h})
                            .tracks({px(108.0f * scale), px(332.0f * scale), fr()})
                            .gap(24.0f * scale)
                            .begin([&](auto& left_rows) {
                                draw_title_row(ui, input, state, left_rows.next(), scale, palette, preset);
                                const Rect top_cards = left_rows.next();
                                ui.row(top_cards)
                                    .tracks({fr(), fr()})
                                    .gap(24.0f * scale)
                                    .begin([&](auto& top_cols) {
                                        draw_customer_card(ui, input, state, top_cols.next(), scale, palette, preset);
                                        draw_product_matrix_card(ui, input, state, top_cols.next(), scale, palette, preset);
                                    });
                                draw_product_bubbles_card(ui, input, state, left_rows.next(), scale, palette);
                            });
                        ui.ctx().set_global_alpha(1.0f);
                        draw_timeline_card(ui, input, state, Rect{right.x, right.y + 108.0f * scale, right.w, right.h - 108.0f * scale}, scale, palette);
                    });
            });
    });

    draw_overlay_menus(ui, input, state, palette, rect, scale);
}

}  // namespace

int main() {
    DashboardState state{};
    const Palette palette = make_palette();

    eui::app::AppOptions options{};
    options.title = "EUI Check Box Dashboard Replica";
    options.width = 1280;
    options.height = 800;
    options.vsync = true;
    options.continuous_render = true;
    options.max_fps = 120.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 700;
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
            const float scale = std::max(0.74f, std::min(framebuffer_w / 1700.0f, framebuffer_h / 1240.0f));
            const float margin = 54.0f * scale;

            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(
                eui::rgba(static_cast<float>((palette.lime >> 16u) & 0xffu) / 255.0f,
                          static_cast<float>((palette.lime >> 8u) & 0xffu) / 255.0f,
                          static_cast<float>(palette.lime & 0xffu) / 255.0f, 1.0f));
            ctx.set_corner_radius(18.0f * scale);

            state.hovered_customer_point = -1;
            state.hovered_matrix_dot = -1;
            state.hovered_bubble_column = -1;
            state.hovered_timeline = -1;
            state.search_button_rect = Rect{};
            state.header_profile_rect = Rect{};
            state.date_button_rect = Rect{};
            state.product_button_rect = Rect{};
            state.profile_button_rect = Rect{};
            state.glow_button_rect = Rect{};
            state.customer_menu_button_rect = Rect{};
            state.product_menu_button_rect = Rect{};
            state.bubble_menu_button_rect = Rect{};
            state.timeline_menu_button_rect = Rect{};
            if (state.open_menu == OpenMenu::None) {
                state.overlay_block_active = false;
                state.overlay_block_rect = Rect{};
            }

            UI ui(ctx);
            draw_fill(ui, Rect{0.0f, 0.0f, framebuffer_w, framebuffer_h}, palette.background, 0.0f, 1.0f);
            draw_soft_glow(ui,
                           Rect{-120.0f * scale, -80.0f * scale, 420.0f * scale, 420.0f * scale},
                           palette.background_alt, palette.background, 0.84f, 0.28f);
            draw_soft_glow(ui,
                           Rect{framebuffer_w - 340.0f * scale, -80.0f * scale, 420.0f * scale, 420.0f * scale},
                           lighten_hex(palette.lime_soft, 0.14f), palette.background, 0.84f, 0.20f);

            draw_dashboard(ui, input, state,
                           Rect{margin, margin, framebuffer_w - margin * 2.0f, framebuffer_h - margin * 2.0f},
                           scale, palette, input.time_seconds);
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
