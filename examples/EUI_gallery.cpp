#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#endif

namespace {

using eui::Rect;
using eui::quick::UI;

struct GalleryState {
    int selected_page{0};
    int selected_animation_demo{0};
    int selected_control_icon{0};
    float control_slider{0.64f};
    float progress_ratio{0.72f};
    float layout_gap{18.0f};
    float layout_radius{18.0f};
    float settings_blur{18.0f};
    bool light_mode{false};
    bool controls_dropdown_open{false};
    bool design_icon_library_open{false};
    int controls_mode{0};
    int design_icon_page{0};
    int accent_index{0};
    float custom_accent_r{96.0f};
    float custom_accent_g{165.0f};
    float custom_accent_b{250.0f};
    std::array<bool, 3> controls_multi_select{{true, false, true}};
    std::string search_text{"Search gallery"};
    std::string notes_text{
        "Panels, text, blur and motion all run through the same immediate-mode renderer.\n\n"
        "Use this editor to test multiline input, line wrapping and longer form content while you tune the surrounding layout.\n\n"
        "The gallery is meant to double as a developer reference, so the default text is intentionally longer than a placeholder.\n\n"
        "Try editing this copy, pasting multiple paragraphs, and scrolling through the document while sliders and theme settings update around it.\n\n"
        "A healthy default body here is useful because developers can immediately validate caret movement, selection painting, wheel scrolling, clipboard flow and long-form wrapping without seeding data first.\n\n"
        "This extra block is only here to guarantee the scroll bar stays active during demo startup."
    };
};

struct DemoDescriptor {
    const char* title;
    const char* summary;
    const char* api_label;
};

struct PageDescriptor {
    const char* title;
    const char* summary;
    const char* api_label;
};

struct IconDescriptor {
    const char* label;
    const char* usage;
    std::uint32_t codepoint;
};

struct IconLibraryEntry {
    std::string label;
    std::string usage;
    std::uint32_t codepoint{0};
};

struct GalleryPalette {
    bool light{false};
    std::uint32_t shell_top{};
    std::uint32_t shell_bottom{};
    std::uint32_t surface{};
    std::uint32_t surface_alt{};
    std::uint32_t surface_deep{};
    std::uint32_t border{};
    std::uint32_t border_soft{};
    std::uint32_t text{};
    std::uint32_t muted{};
    std::uint32_t accent{};
    std::uint32_t accent_soft{};
    std::uint32_t grid{};
};

struct Point2 {
    float x{0.0f};
    float y{0.0f};
};

enum GalleryPageIndex {
    kPageBasicControls = 0,
    kPageDesign = 1,
    kPageLayout = 2,
    kPageAnimation = 3,
    kPageDashboard = 4,
    kPageSettings = 5,
    kPageAbout = 6,
};

static constexpr std::array<DemoDescriptor, 9> kDemos{{
    {"Translate", "Move a rounded rect on one axis", "translate() + ease_in_out"},
    {"Scale", "Grow and shrink around the center", "scale() + eased scalar"},
    {"Rotate", "Rotate a single rounded rect in place", "rotate() + linear loop"},
    {"Arc Motion", "Move on a curved arc instead of a line", "curve path + ease_out"},
    {"Bezier Path", "Follow a cubic Bezier trajectory", "Bezier path + custom cubic"},
    {"Color Shift", "Crossfade the actor between two palettes", "gradient() + color lerp"},
    {"Opacity", "Fade between solid and ghosted states", "opacity() + ease_in_out"},
    {"Blur", "Animate backdrop blur on the actor", "blur() + eased scalar"},
    {"Combo", "Translation, scale, rotate, color, opacity and blur", "combined channels"},
}};

static constexpr std::array<PageDescriptor, 7> kPages{{
    {"Basic Controls", "Buttons, sliders, inputs and text surfaces.", "buttons + inputs"},
    {"Design", "Typography, icon ids and palette tokens for fast UI work.", "fonts + icons + colors"},
    {"Layout", "Declarative row, column, grid and stack composition.", "row() + column() + grid()"},
    {"Animation", "Motion lives here instead of in the main sidebar.", "motion showcase"},
    {"Dashboard Example", "A dashboard-style composition preview.", "dashboard preview"},
    {"Settings", "Theme mode, accent color and gallery tuning.", "theme + accent"},
    {"About", "Project identity, repository link and contact details.", "project + license"},
}};

static constexpr std::array<std::uint32_t, 6> kAccentHexes{{
    0x60A5FA,
    0x22C55E,
    0xF97316,
    0xA855F7,
    0xEAB308,
    0x14B8A6,
}};

constexpr int custom_accent_slot() {
    return static_cast<int>(kAccentHexes.size());
}

static constexpr std::array<std::uint32_t, 7> kPageIcons{{
    0xF1DEu,
    0xF1FCu,
    0xF0DBu,
    0xF061u,
    0xF080u,
    0xF013u,
    0xF05Au,
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

eui::Color color_from_hex(std::uint32_t hex, float alpha = 1.0f) {
    const float r = static_cast<float>((hex >> 16) & 0xffu) / 255.0f;
    const float g = static_cast<float>((hex >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>(hex & 0xffu) / 255.0f;
    return eui::rgba(r, g, b, alpha);
}

std::string format_percent(float ratio) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", clamp01(ratio) * 100.0f);
    return std::string(buffer);
}

std::string format_pixels(float value) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.0f px", value);
    return std::string(buffer);
}

std::string format_precise_pixels(float value) {
    char buffer[32]{};
    if (std::fabs(value - std::round(value)) < 0.05f) {
        std::snprintf(buffer, sizeof(buffer), "%.0f px", value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.1f px", value);
    }
    return std::string(buffer);
}

std::string format_hex_color(std::uint32_t hex) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "0x%06X", hex & 0xFFFFFFu);
    return std::string(buffer);
}

std::string format_codepoint(std::uint32_t codepoint) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "0x%04X", codepoint & 0xFFFFu);
    return std::string(buffer);
}

void open_external_uri(std::string_view uri) {
#if defined(_WIN32)
    const std::wstring wide_uri(uri.begin(), uri.end());
    ShellExecuteW(nullptr, L"open", wide_uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    const std::string command = "open \"" + std::string(uri) + "\"";
    static_cast<void>(std::system(command.c_str()));
#else
    const std::string command = "xdg-open \"" + std::string(uri) + "\" >/dev/null 2>&1";
    static_cast<void>(std::system(command.c_str()));
#endif
}

std::uint32_t pack_rgb_hex(float red, float green, float blue) {
    const auto to_u8 = [](float value) -> std::uint32_t {
        return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0f, 255.0f)));
    };
    return (to_u8(red) << 16) | (to_u8(green) << 8) | to_u8(blue);
}

bool accent_uses_custom_rgb(const GalleryState& state) {
    return state.accent_index >= custom_accent_slot();
}

std::uint32_t custom_accent_hex(const GalleryState& state) {
    return pack_rgb_hex(state.custom_accent_r, state.custom_accent_g, state.custom_accent_b);
}

std::filesystem::path find_icon_library_json() {
    namespace fs = std::filesystem;
    fs::path current = fs::current_path();
    for (int depth = 0; depth < 6; ++depth) {
        const fs::path candidate = current / "examples" / "EUI_gallery_icons.json";
        if (fs::exists(candidate)) {
            return candidate;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

std::vector<IconLibraryEntry> fallback_icon_library() {
    return {
        {"Search", "Query / filter", 0xF002u},
        {"Heart", "Favorite / like", 0xF004u},
        {"Star", "Rating / featured", 0xF005u},
        {"User", "Profile / identity", 0xF007u},
        {"Home", "Landing / dashboard", 0xF015u},
        {"Clock", "Time / recent", 0xF017u},
        {"Camera", "Capture / media", 0xF030u},
        {"Image", "Artwork / preview", 0xF03Eu},
        {"Video", "Playback / motion", 0xF03Du},
        {"Check", "Confirm / success", 0xF00Cu},
        {"Close", "Dismiss / remove", 0xF00Du},
        {"Arrow", "Forward / next", 0xF061u},
    };
}

std::vector<IconLibraryEntry> load_icon_library_entries() {
    const std::filesystem::path path = find_icon_library_json();
    if (path.empty()) {
        return fallback_icon_library();
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return fallback_icon_library();
    }

    const std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const std::regex item_re(
        R"ICON(\{\s*"label"\s*:\s*"([^"]+)"\s*,\s*"usage"\s*:\s*"([^"]+)"\s*,\s*"codepoint"\s*:\s*"(0x[0-9A-Fa-f]+)"\s*\})ICON");
    std::vector<IconLibraryEntry> result;
    for (std::sregex_iterator it(content.begin(), content.end(), item_re), end; it != end; ++it) {
        IconLibraryEntry entry{};
        entry.label = (*it)[1].str();
        entry.usage = (*it)[2].str();
        entry.codepoint = static_cast<std::uint32_t>(std::stoul((*it)[3].str(), nullptr, 16));
        result.push_back(std::move(entry));
    }
    return result.empty() ? fallback_icon_library() : result;
}

const std::vector<IconLibraryEntry>& icon_library_entries() {
    static const std::vector<IconLibraryEntry> entries = load_icon_library_entries();
    return entries;
}

std::uint32_t accent_hex(const GalleryState& state) {
    if (accent_uses_custom_rgb(state)) {
        return custom_accent_hex(state);
    }
    return kAccentHexes[static_cast<std::size_t>(std::clamp(state.accent_index, 0, static_cast<int>(kAccentHexes.size() - 1)))];
}

GalleryPalette make_gallery_palette(const GalleryState& state) {
    const std::uint32_t accent = accent_hex(state);
    if (state.light_mode) {
        return GalleryPalette{
            true,
            0xEEF4FA,
            0xE1EBF5,
            0xFBFDFF,
            0xF3F8FC,
            0xEAF1F7,
            0xC8D5E1,
            0xD6E0E8,
            0x102033,
            0x5F738A,
            accent,
            mix_hex(0xF3F8FC, accent, 0.20f),
            0xD3DEE7,
        };
    }
    return GalleryPalette{
        false,
        0x060D17,
        0x0C1625,
        0x07111C,
        0x09131F,
        0x0F1B2A,
        0x20324A,
        0x26364E,
        0xF5FAFF,
        0x8EA0BA,
        accent,
        mix_hex(0x0E1A2A, accent, 0.18f),
        0x203247,
    };
}

std::uint32_t panel_shadow_hex(const GalleryPalette& palette) {
    return palette.light ? 0x93A8BC : 0x020617;
}

std::uint32_t actor_primary_top_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xFFFFFF, palette.accent, 0.34f) : mix_hex(0x72B2FF, palette.accent, 0.18f);
}

std::uint32_t actor_primary_bottom_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xDCEAFE, palette.accent, 0.76f) : mix_hex(0x2563EB, palette.accent, 0.60f);
}

std::uint32_t actor_focus_top_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xF8FCFF, palette.accent, 0.22f) : mix_hex(0xA7E2FF, palette.accent, 0.18f);
}

std::uint32_t actor_focus_bottom_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xE2EEFF, palette.accent, 0.58f) : mix_hex(0x2563EB, palette.accent, 0.72f);
}

std::uint32_t actor_glass_top_hex(const GalleryPalette& palette) {
    return palette.light ? 0xFFFFFF : 0xF8FBFF;
}

std::uint32_t actor_glass_bottom_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xEEF5FD, palette.accent, 0.22f) : mix_hex(0xD7EAFE, palette.accent, 0.16f);
}

std::uint32_t actor_stroke_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xD7E6F7, palette.accent, 0.18f) : mix_hex(0xDBECFF, palette.accent, 0.16f);
}

std::uint32_t actor_inner_hex(const GalleryPalette& palette) {
    return palette.light ? 0xFFFFFF : 0xEFF6FF;
}

std::uint32_t actor_outline_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.24f : 0.18f);
}

std::uint32_t demo_track_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.12f : 0.18f);
}

std::uint32_t demo_axis_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border, palette.accent, palette.light ? 0.18f : 0.24f);
}

std::uint32_t demo_curve_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.42f : 0.22f);
}

std::uint32_t demo_curve_soft_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.34f : 0.16f);
}

std::uint32_t demo_anchor_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xFFFFFF, palette.accent, 0.32f) : mix_hex(0x7DD3FC, palette.accent, 0.22f);
}

std::uint32_t demo_anchor_end_hex(const GalleryPalette& palette) {
    return mix_hex(palette.accent, 0x38BDF8, palette.light ? 0.24f : 0.52f);
}

std::uint32_t demo_anchor_muted_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.text, palette.light ? 0.18f : 0.28f);
}

std::uint32_t metric_surface_top_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.08f : 0.10f);
}

std::uint32_t metric_surface_bottom_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_alt, palette.accent, palette.light ? 0.18f : 0.16f);
}

std::uint32_t metric_surface_border_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.34f : 0.32f);
}

std::uint32_t nav_selected_fill_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.18f : 0.24f);
}

std::uint32_t nav_idle_fill_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(palette.surface_deep, palette.border, 0.18f) : palette.surface_alt;
}

std::uint32_t preview_backdrop_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.10f : 0.06f);
}

std::array<std::uint32_t, 5> blur_reference_hexes(const GalleryPalette& palette) {
    return {{
        mix_hex(palette.accent, 0x22C55E, palette.light ? 0.34f : 0.16f),
        mix_hex(palette.accent, 0xF97316, palette.light ? 0.56f : 0.74f),
        mix_hex(palette.accent, 0xA855F7, palette.light ? 0.62f : 0.76f),
        mix_hex(palette.accent, 0x38BDF8, palette.light ? 0.36f : 0.58f),
        mix_hex(palette.accent, 0xEAB308, palette.light ? 0.58f : 0.72f),
    }};
}

void draw_icon(UI& ui, std::uint32_t codepoint, const Rect& rect, std::uint32_t hex, float alpha = 1.0f) {
    ui.glyph(codepoint).in(rect).tint(hex, alpha).draw();
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

void draw_text_right(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex,
                     float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).align(eui::TextAlign::Right).draw();
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

void draw_stage_background(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette) {
    draw_fill(ui, rect, palette.surface_alt, 22.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.border, 22.0f * scale, 1.0f, 0.92f);

    const float gap = 34.0f * scale;
    for (float x = rect.x + gap; x < rect.x + rect.w; x += gap) {
        ui.shape().in(Rect{x, rect.y, 1.0f, rect.h}).fill(palette.grid, palette.light ? 0.36f : 0.46f).draw();
    }
    for (float y = rect.y + gap; y < rect.y + rect.h; y += gap) {
        ui.shape().in(Rect{rect.x, y, rect.w, 1.0f}).fill(palette.grid, palette.light ? 0.36f : 0.46f).draw();
    }
}

void draw_actor(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette, std::uint32_t top_hex,
                std::uint32_t bottom_hex, float alpha = 1.0f, float blur_radius = 0.0f,
                float backdrop_blur = 0.0f, float translate_x = 0.0f, float translate_y = 0.0f,
                float actor_scale = 1.0f, float angle_deg = 0.0f, float fill_alpha = 1.0f) {
    auto outer = ui.shape()
                     .in(rect)
                     .radius(18.0f * scale)
                     .gradient(top_hex, bottom_hex, fill_alpha)
                     .stroke(actor_stroke_hex(palette), 1.0f, 0.92f)
                     .shadow(0.0f, 8.0f * scale, 20.0f * scale, panel_shadow_hex(palette), palette.light ? 0.08f : 0.12f)
                     .opacity(alpha)
                     .translate(translate_x, translate_y)
                     .scale(actor_scale)
                     .rotate(angle_deg)
                     .origin_center();
    if (blur_radius > 0.0f || backdrop_blur > 0.0f) {
        outer.blur(blur_radius, backdrop_blur);
    }
    outer.draw();

    auto inner = ui.shape()
                     .in(inset_rect(rect, 10.0f * scale))
                     .radius(10.0f * scale)
                     .fill(actor_inner_hex(palette), (palette.light ? 0.32f : 0.18f) * fill_alpha)
                     .opacity(alpha)
                     .translate(translate_x, translate_y)
                     .scale(actor_scale)
                     .rotate(angle_deg)
                     .origin_center();
    if (blur_radius > 0.0f || backdrop_blur > 0.0f) {
        inner.blur(blur_radius * 0.35f, backdrop_blur * 0.35f);
    }
    inner.draw();
}

void draw_actor(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette, float alpha = 1.0f,
                float blur_radius = 0.0f, float backdrop_blur = 0.0f, float translate_x = 0.0f,
                float translate_y = 0.0f, float actor_scale = 1.0f, float angle_deg = 0.0f,
                float fill_alpha = 1.0f) {
    draw_actor(ui, rect, scale, palette, actor_primary_top_hex(palette), actor_primary_bottom_hex(palette), alpha,
               blur_radius, backdrop_blur, translate_x, translate_y, actor_scale, angle_deg, fill_alpha);
}

void draw_blur_reference(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette) {
    const float cx = rect.x + rect.w * 0.5f;
    const float cy = rect.y + rect.h * 0.5f;
    const auto colors = blur_reference_hexes(palette);

    draw_fill(ui, Rect{cx - 240.0f * scale, cy - 94.0f * scale, 122.0f * scale, 122.0f * scale},
              colors[0], 61.0f * scale, 0.90f);
    draw_fill(ui, Rect{cx + 116.0f * scale, cy - 86.0f * scale, 130.0f * scale, 130.0f * scale},
              colors[1], 65.0f * scale, 0.88f);
    draw_fill(ui, Rect{cx - 214.0f * scale, cy + 72.0f * scale, 168.0f * scale, 22.0f * scale},
              colors[2], 11.0f * scale, 0.82f);
    draw_fill(ui, Rect{cx + 34.0f * scale, cy + 62.0f * scale, 150.0f * scale, 22.0f * scale},
              colors[3], 11.0f * scale, 0.84f);
    draw_fill(ui, Rect{cx - 9.0f * scale, cy - 116.0f * scale, 18.0f * scale, 220.0f * scale},
              colors[4], 9.0f * scale, 0.78f);
}

void draw_translate_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const Rect track{rect.x + 48.0f * scale, rect.y + rect.h * 0.5f - 3.0f * scale, rect.w - 96.0f * scale, 6.0f * scale};
    draw_fill(ui, track, demo_track_hex(palette), 3.0f * scale, 0.86f);

    const Rect start_dot{track.x - 8.0f * scale, track.y - 8.0f * scale, 16.0f * scale, 16.0f * scale};
    const Rect end_dot{track.x + track.w - 8.0f * scale, track.y - 8.0f * scale, 16.0f * scale, 16.0f * scale};
    draw_fill(ui, start_dot, demo_anchor_hex(palette), 8.0f * scale, 1.0f);
    draw_fill(ui, end_dot, demo_anchor_end_hex(palette), 8.0f * scale, 1.0f);

    const float travel = track.w - 72.0f * scale;
    const float t = timeline_ping_pong(ui, time_seconds, 1.35f, eui::animation::EasingPreset::ease_in_out);
    draw_actor(ui, Rect{track.x + travel * t, rect.y + rect.h * 0.5f - 36.0f * scale, 72.0f * scale, 72.0f * scale}, scale,
               palette);
}

void draw_scale_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.55f, eui::animation::EasingPreset::ease_in_out);
    const float actor_scale = lerp(0.62f, 1.28f, t);
    const Rect actor{
        rect.x + rect.w * 0.5f - 72.0f * scale,
        rect.y + rect.h * 0.5f - 72.0f * scale,
        144.0f * scale,
        144.0f * scale,
    };
    draw_actor(ui, actor, scale, palette, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, actor_scale);
}

void draw_rotate_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = loop_progress(time_seconds, 2.40f);
    const float angle = 360.0f * t;
    const float cx = rect.x + rect.w * 0.5f;
    const float cy = rect.y + rect.h * 0.5f;
    const Rect actor{cx - 92.0f * scale, cy - 42.0f * scale, 184.0f * scale, 84.0f * scale};
    draw_stroke(ui, Rect{cx - 132.0f * scale, cy - 132.0f * scale, 264.0f * scale, 264.0f * scale},
                demo_axis_hex(palette), 132.0f * scale, 1.0f * scale, 0.42f);
    ui.shape()
        .in(Rect{cx - 1.0f, rect.y + 48.0f * scale, 2.0f, rect.h - 96.0f * scale})
        .fill(demo_axis_hex(palette), 0.66f)
        .draw();
    ui.shape()
        .in(Rect{rect.x + 48.0f * scale, cy - 1.0f, rect.w - 96.0f * scale, 2.0f})
        .fill(demo_axis_hex(palette), 0.66f)
        .draw();
    draw_actor(ui, actor, scale, palette, actor_focus_top_hex(palette), actor_focus_bottom_hex(palette), 1.0f, 0.0f,
               0.0f, 0.0f, 0.0f, 1.0f, angle);
    draw_fill(ui, Rect{cx - 7.0f * scale, cy - 7.0f * scale, 14.0f * scale, 14.0f * scale},
              actor_glass_top_hex(palette), 7.0f * scale, 0.98f);
}

void draw_arc_motion_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.75f, eui::animation::EasingPreset::ease_out);
    const float left = rect.x + 52.0f * scale;
    const float right = rect.x + rect.w - 52.0f * scale;
    const float base_y = rect.y + rect.h * 0.70f;
    const float height = rect.h * 0.34f;

    for (int i = 0; i <= 30; ++i) {
        const float sample = static_cast<float>(i) / 30.0f;
        const float x = lerp(left, right, sample);
        const float y = base_y - std::sin(sample * 3.14159265f) * height;
        draw_fill(ui, Rect{x - 3.0f * scale, y - 3.0f * scale, 6.0f * scale, 6.0f * scale}, demo_curve_soft_hex(palette),
                  3.0f * scale, 0.70f);
    }

    const float x = lerp(left, right, t);
    const float y = base_y - std::sin(t * 3.14159265f) * height;
    draw_actor(ui, Rect{x - 36.0f * scale, y - 36.0f * scale, 72.0f * scale, 72.0f * scale}, scale, palette);
}

void draw_bezier_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const Point2 p0{rect.x + 58.0f * scale, rect.y + rect.h - 74.0f * scale};
    const Point2 p1{rect.x + rect.w * 0.30f, rect.y + 38.0f * scale};
    const Point2 p2{rect.x + rect.w * 0.68f, rect.y + rect.h - 42.0f * scale};
    const Point2 p3{rect.x + rect.w - 58.0f * scale, rect.y + 74.0f * scale};

    for (int i = 0; i <= 40; ++i) {
        const float sample = static_cast<float>(i) / 40.0f;
        const Point2 point = cubic_bezier_point(p0, p1, p2, p3, sample);
        draw_fill(ui, Rect{point.x - 3.0f * scale, point.y - 3.0f * scale, 6.0f * scale, 6.0f * scale},
                  demo_curve_hex(palette), 3.0f * scale, sample < 0.5f ? 0.72f : 0.90f);
    }

    draw_fill(ui, Rect{p0.x - 6.0f * scale, p0.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_hex(palette),
              6.0f * scale, 1.0f);
    draw_fill(ui, Rect{p1.x - 6.0f * scale, p1.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_muted_hex(palette),
              6.0f * scale, 0.92f);
    draw_fill(ui, Rect{p2.x - 6.0f * scale, p2.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_muted_hex(palette),
              6.0f * scale, 0.92f);
    draw_fill(ui, Rect{p3.x - 6.0f * scale, p3.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_end_hex(palette),
              6.0f * scale, 1.0f);

    const eui::animation::CubicBezier velocity{0.20f, 0.0f, 0.10f, 1.0f};
    const float t = timeline_ping_pong(ui, time_seconds, 2.10f, velocity);
    const Point2 actor = cubic_bezier_point(p0, p1, p2, p3, t);
    draw_actor(ui, Rect{actor.x - 34.0f * scale, actor.y - 34.0f * scale, 68.0f * scale, 68.0f * scale}, scale, palette);
}

void draw_color_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.80f, eui::animation::EasingPreset::ease_in_out);
    const std::uint32_t top = mix_hex(actor_primary_top_hex(palette), mix_hex(palette.accent, 0xF59E0B, 0.72f), t);
    const std::uint32_t bottom = mix_hex(actor_primary_bottom_hex(palette), mix_hex(palette.accent, 0xEF4444, 0.68f), t);
    const Rect actor{
        rect.x + rect.w * 0.5f - 88.0f * scale,
        rect.y + rect.h * 0.5f - 52.0f * scale,
        176.0f * scale,
        104.0f * scale,
    };
    draw_actor(ui, actor, scale, palette, top, bottom);

    draw_fill(ui, Rect{rect.x + 58.0f * scale, rect.y + rect.h - 78.0f * scale, 84.0f * scale, 18.0f * scale},
              actor_primary_top_hex(palette), 9.0f * scale, 0.92f);
    draw_fill(ui, Rect{rect.x + 154.0f * scale, rect.y + rect.h - 78.0f * scale, 84.0f * scale, 18.0f * scale},
              mix_hex(actor_primary_top_hex(palette), mix_hex(palette.accent, 0xF59E0B, 0.72f), 0.5f), 9.0f * scale, 0.92f);
    draw_fill(ui, Rect{rect.x + 250.0f * scale, rect.y + rect.h - 78.0f * scale, 84.0f * scale, 18.0f * scale},
              mix_hex(palette.accent, 0xEF4444, 0.68f), 9.0f * scale, 0.92f);
}

void draw_opacity_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const Rect actor{
        rect.x + rect.w * 0.5f - 88.0f * scale,
        rect.y + rect.h * 0.5f - 52.0f * scale,
        176.0f * scale,
        104.0f * scale,
    };
    const float alpha = lerp(0.20f, 1.0f, timeline_ping_pong(ui, time_seconds, 1.60f, eui::animation::EasingPreset::ease_in_out));

    draw_stroke(ui, actor, actor_outline_hex(palette), 18.0f * scale, 1.0f, 0.26f);
    draw_actor(ui, actor, scale, palette, alpha);
}

void draw_blur_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);
    draw_blur_reference(ui, inset_rect(rect, 18.0f * scale), scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.85f, eui::animation::EasingPreset::ease_in_out);
    const float blur_value = lerp(0.0f, 22.0f * scale, t);
    const Rect left_actor{
        rect.x + rect.w * 0.5f - 214.0f * scale,
        rect.y + rect.h * 0.5f - 58.0f * scale,
        188.0f * scale,
        116.0f * scale,
    };
    const Rect right_actor{
        rect.x + rect.w * 0.5f + 26.0f * scale,
        rect.y + rect.h * 0.5f - 58.0f * scale,
        188.0f * scale,
        116.0f * scale,
    };
    draw_text_center(ui, "No blur", Rect{left_actor.x, left_actor.y - 34.0f * scale, left_actor.w, 18.0f * scale},
                     font_body(scale), palette.muted, 0.96f);
    draw_text_center(ui, "Animated blur", Rect{right_actor.x, right_actor.y - 34.0f * scale, right_actor.w, 18.0f * scale},
                     font_body(scale), palette.muted, 0.96f);

    draw_actor(ui, left_actor, scale, palette, actor_glass_top_hex(palette), actor_glass_bottom_hex(palette), 1.0f, 0.0f,
               0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.12f);
    draw_actor(ui, right_actor, scale, palette, actor_glass_top_hex(palette), actor_glass_bottom_hex(palette), 1.0f,
               blur_value, blur_value, 0.0f, 0.0f, 1.0f, 0.0f, 0.12f);
}

void draw_combo_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);
    draw_blur_reference(ui, inset_rect(rect, 24.0f * scale), scale, palette);

    const float translate_t = timeline_ping_pong(ui, time_seconds, 1.65f, eui::animation::EasingPreset::ease_in_out);
    const float scale_t = timeline_ping_pong(ui, time_seconds + 0.28, 1.65f, eui::animation::EasingPreset::spring_soft);
    const float rotate_t = loop_progress(time_seconds, 2.60f);
    const float color_t = timeline_ping_pong(ui, time_seconds + 0.44, 2.00f, eui::animation::EasingPreset::ease_in_out);
    const float alpha_t = timeline_ping_pong(ui, time_seconds + 0.18, 1.70f, eui::animation::EasingPreset::ease_in_out);
    const float blur_t = timeline_ping_pong(ui, time_seconds + 0.33, 1.90f, eui::animation::EasingPreset::ease_in_out);

    const float dx = lerp(-rect.w * 0.18f, rect.w * 0.18f, translate_t);
    const float actor_scale = lerp(0.72f, 1.18f, scale_t);
    const float angle = lerp(-28.0f, 332.0f, rotate_t);
    const float alpha = lerp(0.42f, 1.0f, alpha_t);
    const float blur_value = lerp(0.0f, 16.0f * scale, blur_t);
    const std::uint32_t top = mix_hex(actor_primary_top_hex(palette), mix_hex(palette.accent, 0xA855F7, 0.62f), color_t);
    const std::uint32_t bottom = mix_hex(actor_primary_bottom_hex(palette), mix_hex(palette.accent, 0xF97316, 0.72f), color_t);
    const Rect actor{
        rect.x + rect.w * 0.5f - 94.0f * scale,
        rect.y + rect.h * 0.5f - 58.0f * scale,
        188.0f * scale,
        116.0f * scale,
    };
    draw_actor(ui, actor, scale, palette, top, bottom, alpha, blur_value, blur_value, dx, 0.0f, actor_scale, angle, 0.24f);
}

void draw_demo_scene(UI& ui, int demo_index, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    switch (demo_index) {
        case 0:
            draw_translate_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 1:
            draw_scale_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 2:
            draw_rotate_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 3:
            draw_arc_motion_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 4:
            draw_bezier_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 5:
            draw_color_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 6:
            draw_opacity_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 7:
            draw_blur_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 8:
        default:
            draw_combo_demo(ui, rect, scale, time_seconds, palette);
            break;
    }
}

void draw_design_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const auto& library_icons = icon_library_entries();
    const float gap = 18.0f * scale;

    ui.row(rect)
        .tracks({eui::quick::fr(1.05f), eui::quick::fr(), eui::quick::fr()})
        .gap(gap)
        .begin([&](auto& columns) {
            const Rect typography_rect = columns.next();
            const Rect color_rect = columns.next();
            const Rect icon_rect = columns.next();

            ui.card("Typography")
                .in(typography_rect)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    struct TypeRow {
                        const char* name;
                        const char* usage;
                        const char* sample;
                        float size;
                    };

                    const std::array<TypeRow, 4> rows{{
                        {"font_display", "Page title / shell header", "Display Title", font_display(scale)},
                        {"font_heading", "Card / section heading", "Section Heading", font_heading(scale)},
                        {"font_body", "Navigation / readable text", "Readable Body", font_body(scale)},
                        {"font_meta", "Summary / caption / helper", "Meta Caption", font_meta(scale)},
                    }};
                    const std::array<std::string_view, 4> code_lines{{
                        "float font_display(float scale) { return 22.0f * scale; }",
                        "float font_heading(float scale) { return 16.0f * scale; }",
                        "float font_body(float scale) { return 14.0f * scale; }",
                        "float font_meta(float scale) { return 12.2f * scale; }",
                    }};

                    ui.view(card.content())
                        .column()
                        .tracks({eui::quick::fr(), eui::quick::fr(), eui::quick::fr(), eui::quick::fr(), eui::quick::px(92.0f * scale)})
                        .gap(8.0f * scale)
                        .begin([&](auto& rows_scope) {
                            for (const auto& row_info : rows) {
                                const Rect row = rows_scope.next();
                                draw_fill(ui, row, palette.surface_deep, 14.0f * scale, 0.94f);
                                draw_stroke(ui, row, palette.border_soft, 14.0f * scale, 1.0f, 0.86f);
                                draw_text_left(ui, row_info.name,
                                               Rect{row.x + 12.0f * scale, row.y + 8.0f * scale, row.w - 132.0f * scale, 18.0f * scale},
                                               font_body(scale), palette.text, 0.98f);
                                draw_text_right(ui, format_precise_pixels(row_info.size / std::max(1.0f, scale)),
                                                Rect{row.x + row.w - 120.0f * scale, row.y + 8.0f * scale, 108.0f * scale, 18.0f * scale},
                                                font_meta(scale), palette.muted, 0.98f);
                                draw_text_left(ui, row_info.usage,
                                               Rect{row.x + 12.0f * scale, row.y + 28.0f * scale, row.w - 24.0f * scale, 16.0f * scale},
                                               font_meta(scale), palette.muted, 0.96f);
                                draw_text_left(ui, row_info.sample,
                                               Rect{row.x + 12.0f * scale, row.y + 44.0f * scale, row.w - 24.0f * scale, 24.0f * scale},
                                               row_info.size, mix_hex(palette.text, palette.accent, 0.18f), 0.98f);
                            }

                            const Rect code_rect = rows_scope.next();
                            draw_fill(ui, code_rect, palette.surface_deep, 16.0f * scale, 0.92f);
                            draw_stroke(ui, code_rect, palette.border_soft, 16.0f * scale, 1.0f, 0.86f);
                            for (std::size_t i = 0; i < code_lines.size(); ++i) {
                                draw_text_left(ui, code_lines[i],
                                               Rect{code_rect.x + 12.0f * scale, code_rect.y + 10.0f * scale + static_cast<float>(i) * 20.0f * scale,
                                                    code_rect.w - 24.0f * scale, 18.0f * scale},
                                               font_meta(scale), palette.muted, 0.98f);
                            }
                        });
                });

            ui.card("Color System")
                .in(color_rect)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    const std::array<std::pair<const char*, std::uint32_t>, 10> tokens{{
                        {"shell_top", palette.shell_top},
                        {"shell_bottom", palette.shell_bottom},
                        {"surface", palette.surface},
                        {"surface_alt", palette.surface_alt},
                        {"surface_deep", palette.surface_deep},
                        {"border", palette.border},
                        {"border_soft", palette.border_soft},
                        {"accent", palette.accent},
                        {"accent_soft", palette.accent_soft},
                        {"muted", palette.muted},
                    }};

                    ui.view(card.content())
                        .column()
                        .tracks({eui::quick::fr(1.0f), eui::quick::fr(2.1f)})
                        .gap(12.0f * scale)
                        .begin([&](auto& sections) {
                            const Rect accents = sections.next();
                            const Rect tokens_area = sections.next();

                            draw_fill(ui, accents, palette.surface_deep, 18.0f * scale, 0.92f);
                            draw_stroke(ui, accents, palette.border_soft, 18.0f * scale, 1.0f, 0.90f);
                            ui.view(accents)
                                .padding(14.0f * scale)
                                .column()
                                .tracks({eui::quick::px(18.0f * scale), eui::quick::fr()})
                                .gap(12.0f * scale)
                                .begin([&](auto& accent_sections) {
                                    const Rect title = accent_sections.next();
                                    const Rect grid_rect = accent_sections.next();
                                    draw_text_left(ui, "Built-in Accent Presets", title, font_body(scale), palette.text, 0.98f);
                                    ui.grid(grid_rect)
                                        .repeat_rows(2, eui::quick::fr())
                                        .repeat_columns(3, eui::quick::fr())
                                        .gap(10.0f * scale)
                                        .begin([&](auto& accent_grid) {
                                            for (std::size_t i = 0; i < kAccentHexes.size(); ++i) {
                                                const Rect tile = accent_grid.next();
                                                draw_fill(ui, tile, palette.surface_alt, 14.0f * scale, 0.96f);
                                                draw_stroke(ui, tile, palette.border_soft, 14.0f * scale, 1.0f, 0.86f);
                                                draw_fill(ui, Rect{tile.x + 10.0f * scale, tile.y + 10.0f * scale, tile.w - 20.0f * scale, 14.0f * scale},
                                                          kAccentHexes[i], 7.0f * scale, 0.98f);
                                                draw_text_left(ui, format_hex_color(kAccentHexes[i]),
                                                               Rect{tile.x + 10.0f * scale, tile.y + 30.0f * scale, tile.w - 20.0f * scale, 14.0f * scale},
                                                               font_meta(scale), palette.text, 0.98f);
                                            }
                                        });
                                });

                            draw_fill(ui, tokens_area, palette.surface_deep, 18.0f * scale, 0.92f);
                            draw_stroke(ui, tokens_area, palette.border_soft, 18.0f * scale, 1.0f, 0.90f);
                            ui.view(tokens_area)
                                .padding(14.0f * scale)
                                .column()
                                .tracks({eui::quick::px(18.0f * scale), eui::quick::fr()})
                                .gap(12.0f * scale)
                                .begin([&](auto& token_sections) {
                                    const Rect title = token_sections.next();
                                    const Rect grid_rect = token_sections.next();
                                    draw_text_left(ui, state.light_mode ? "Current Theme Tokens: Light" : "Current Theme Tokens: Dark",
                                                   title, font_body(scale), palette.text, 0.98f);
                                    ui.grid(grid_rect)
                                        .repeat_rows(5, eui::quick::fr())
                                        .repeat_columns(2, eui::quick::fr())
                                        .gap(8.0f * scale)
                                        .begin([&](auto& token_grid) {
                                            for (std::size_t i = 0; i < tokens.size(); ++i) {
                                                const Rect tile = token_grid.next();
                                                draw_fill(ui, tile, palette.surface_alt, 14.0f * scale, 0.96f);
                                                draw_stroke(ui, tile, palette.border_soft, 14.0f * scale, 1.0f, 0.86f);
                                                const Rect swatch{
                                                    tile.x + 10.0f * scale,
                                                    tile.y + 10.0f * scale,
                                                    16.0f * scale,
                                                    std::max(14.0f * scale, tile.h - 20.0f * scale),
                                                };
                                                draw_fill(ui, swatch, tokens[i].second, std::min(swatch.w * 0.5f, 8.0f * scale), 0.98f);
                                                draw_text_left(ui, tokens[i].first,
                                                               Rect{tile.x + 34.0f * scale, tile.y + 10.0f * scale, tile.w - 46.0f * scale, 16.0f * scale},
                                                               font_body(scale), palette.text, 0.98f);
                                                draw_text_left(ui, format_hex_color(tokens[i].second),
                                                               Rect{tile.x + 34.0f * scale, tile.y + tile.h - 22.0f * scale, tile.w - 46.0f * scale, 14.0f * scale},
                                                               font_meta(scale), palette.muted, 0.96f);
                                            }
                                        });
                                });
                        });
                });

            ui.card("Icon Reference")
                .in(icon_rect)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.view(card.content())
                        .column()
                        .tracks({eui::quick::px(38.0f * scale), eui::quick::fr()})
                        .gap(12.0f * scale)
                        .begin([&](auto& rows) {
                            const Rect action_row = rows.next();
                            const Rect content = rows.next();
                            const bool open_hovered = hovered(input, action_row);
                            const float open_mix =
                                ui.presence(ui.id("gallery/design/open-icon-sidebar"), open_hovered || state.design_icon_library_open, 18.0f, 12.0f);
                            draw_fill(ui, action_row,
                                      state.design_icon_library_open ? nav_selected_fill_hex(palette)
                                                                     : mix_hex(palette.surface_deep, palette.accent, 0.10f * open_mix),
                                      action_row.h * 0.5f, 0.96f);
                            draw_stroke(ui, action_row,
                                        state.design_icon_library_open ? palette.accent
                                                                       : mix_hex(palette.border_soft, palette.accent, 0.18f * open_mix),
                                        action_row.h * 0.5f, 1.0f, 0.88f);
                            draw_icon(ui, 0xF03Au,
                                      Rect{action_row.x + 12.0f * scale, action_row.y + (action_row.h - 16.0f * scale) * 0.5f,
                                           16.0f * scale, 16.0f * scale},
                                      state.design_icon_library_open ? palette.accent : palette.text, 0.98f);
                            draw_text_left(ui, "Open Icon Sidebar",
                                           Rect{action_row.x + 34.0f * scale, action_row.y + 10.0f * scale, action_row.w - 46.0f * scale, 18.0f * scale},
                                           font_body(scale), state.design_icon_library_open ? palette.text : palette.muted, 0.98f);
                            if (clicked(input, action_row)) {
                                state.design_icon_library_open = true;
                            }

                            const std::size_t preview_count = std::min<std::size_t>(12, library_icons.size());
                            auto preview_grid = ui.grid(content)
                                                    .repeat_rows(4, eui::quick::fr())
                                                    .repeat_columns(3, eui::quick::fr())
                                                    .gap(10.0f * scale)
                                                    .begin();
                            for (std::size_t i = 0; i < preview_count; ++i) {
                                const Rect tile = preview_grid.next();
                                draw_fill(ui, tile, palette.surface_deep, 16.0f * scale, 0.94f);
                                draw_stroke(ui, tile, i == 0 ? palette.accent : palette.border_soft, 16.0f * scale, 1.0f, 0.90f);
                                draw_icon(ui, library_icons[i].codepoint,
                                          Rect{tile.x + 12.0f * scale, tile.y + 12.0f * scale, 18.0f * scale, 18.0f * scale},
                                          i == 0 ? palette.accent : palette.text, 0.98f);
                                draw_text_left(ui, library_icons[i].label,
                                               Rect{tile.x + 38.0f * scale, tile.y + 10.0f * scale, tile.w - 50.0f * scale, 18.0f * scale},
                                               font_body(scale), palette.text, 0.98f);
                                draw_text_left(ui, format_codepoint(library_icons[i].codepoint),
                                               Rect{tile.x + 12.0f * scale, tile.y + 38.0f * scale, tile.w - 24.0f * scale, 16.0f * scale},
                                               font_meta(scale), palette.accent, 0.98f);
                                draw_text_left(ui, library_icons[i].usage,
                                               Rect{tile.x + 12.0f * scale, tile.y + 56.0f * scale, tile.w - 24.0f * scale, tile.h - 60.0f * scale},
                                               font_meta(scale), palette.muted, 0.96f);
                            }
                        });
                });
        });

    const std::size_t page_size = 12;
    const int icon_page_count = static_cast<int>((library_icons.size() + page_size - 1) / page_size);
    state.design_icon_page = std::clamp(state.design_icon_page, 0, std::max(0, icon_page_count - 1));
    const float sidebar_w = std::min(rect.w * 0.38f, 400.0f * scale);
    const std::uint64_t sidebar_motion_id = ui.id("gallery/design/icon-sidebar");
    const float sidebar_closed_x = rect.x + rect.w + 18.0f * scale;
    if (ui.motion_value(sidebar_motion_id, -10000.0f) < -9999.0f) {
        ui.reset_motion(sidebar_motion_id, sidebar_closed_x);
    }
    const float sidebar_target_x =
        state.design_icon_library_open ? (rect.x + rect.w - sidebar_w) : sidebar_closed_x;
    const float sidebar_x = ui.motion(sidebar_motion_id, sidebar_target_x, 18.0f);
    if (sidebar_x < rect.x + rect.w - 1.0f) {
        ui.panel("Icon Library")
            .in(Rect{sidebar_x, rect.y, sidebar_w, rect.h})
            .title_font(font_heading(scale))
            .radius(24.0f * scale)
            .fill(palette.surface_alt)
            .stroke(palette.border, 1.0f)
            .shadow(0.0f, 12.0f * scale, 24.0f * scale, panel_shadow_hex(palette), palette.light ? 0.10f : 0.18f)
            .begin([&](auto& panel) {
                const std::string page_text =
                    "Page " + std::to_string(state.design_icon_page + 1) + " / " + std::to_string(std::max(1, icon_page_count));
                ui.view(panel.content())
                    .column()
                    .tracks({eui::quick::px(18.0f * scale), eui::quick::px(34.0f * scale), eui::quick::px(34.0f * scale),
                             eui::quick::px(36.0f * scale), eui::quick::fr()})
                    .gap(8.0f * scale)
                    .begin([&](auto& rows) {
                        const Rect header_info = rows.next();
                        const Rect page_row = rows.next();
                        const Rect actions = rows.next();
                        const Rect close_row = rows.next();
                        const Rect grid = rows.next();

                        draw_text_left(ui, "Font Awesome JSON Catalog", header_info, font_body(scale), palette.text, 0.98f);
                        draw_fill(ui, page_row, palette.surface_deep, page_row.h * 0.5f, 0.96f);
                        draw_stroke(ui, page_row, palette.border_soft, page_row.h * 0.5f, 1.0f, 0.88f);
                        draw_text_center(ui, page_text, page_row, font_meta(scale), palette.text, 0.98f);

                        ui.row(actions)
                            .tracks({eui::quick::fr(), eui::quick::fr()})
                            .gap(8.0f * scale)
                            .begin([&](auto& action_cols) {
                                const Rect prev_button = action_cols.next();
                                const Rect next_button = action_cols.next();
                                draw_fill(ui, prev_button, palette.surface_deep, actions.h * 0.5f, 0.96f);
                                draw_stroke(ui, prev_button, palette.border_soft, actions.h * 0.5f, 1.0f, 0.88f);
                                draw_text_center(ui, "Prev Page", prev_button, font_meta(scale), palette.text, 0.98f);
                                if (clicked(input, prev_button)) {
                                    state.design_icon_page = std::max(0, state.design_icon_page - 1);
                                }
                                draw_fill(ui, next_button, palette.surface_deep, actions.h * 0.5f, 0.96f);
                                draw_stroke(ui, next_button, palette.border_soft, actions.h * 0.5f, 1.0f, 0.88f);
                                draw_text_center(ui, "Next Page", next_button, font_meta(scale), palette.text, 0.98f);
                                if (clicked(input, next_button)) {
                                    state.design_icon_page = std::min(std::max(0, icon_page_count - 1), state.design_icon_page + 1);
                                }
                            });

                        draw_fill(ui, close_row, nav_selected_fill_hex(palette), close_row.h * 0.5f, 0.96f);
                        draw_stroke(ui, close_row, palette.accent, close_row.h * 0.5f, 1.0f, 0.92f);
                        draw_text_center(ui, "Close Sidebar", close_row, font_body(scale), palette.text, 0.98f);
                        if (clicked(input, close_row)) {
                            state.design_icon_library_open = false;
                        }

                        const std::size_t start = static_cast<std::size_t>(state.design_icon_page) * page_size;
                        const std::size_t end = std::min(library_icons.size(), start + page_size);
                        auto icon_grid = ui.grid(grid)
                                             .repeat_rows(4, eui::quick::fr())
                                             .repeat_columns(3, eui::quick::fr())
                                             .gap(10.0f * scale)
                                             .begin();
                        for (std::size_t i = start; i < end; ++i) {
                            const Rect tile = icon_grid.next();
                            draw_fill(ui, tile, palette.surface_deep, 16.0f * scale, 0.96f);
                            draw_stroke(ui, tile, palette.border_soft, 16.0f * scale, 1.0f, 0.88f);
                            draw_icon(ui, library_icons[i].codepoint,
                                      Rect{tile.x + 12.0f * scale, tile.y + 10.0f * scale, 18.0f * scale, 18.0f * scale},
                                      palette.accent, 0.98f);
                            draw_text_left(ui, library_icons[i].label,
                                           Rect{tile.x + 38.0f * scale, tile.y + 9.0f * scale, tile.w - 50.0f * scale, 18.0f * scale},
                                           font_body(scale), palette.text, 0.98f);
                            draw_text_left(ui, format_codepoint(library_icons[i].codepoint),
                                           Rect{tile.x + 12.0f * scale, tile.y + 34.0f * scale, tile.w - 24.0f * scale, 16.0f * scale},
                                           font_meta(scale), palette.accent, 0.98f);
                            draw_text_left(ui, library_icons[i].usage,
                                           Rect{tile.x + 12.0f * scale, tile.y + 52.0f * scale, tile.w - 24.0f * scale,
                                                tile.h - 58.0f * scale},
                                           font_meta(scale), palette.muted, 0.96f);
                        }
                    });
            });
    }
}

void draw_basic_controls_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const std::array<std::string_view, 3> control_modes{{"Compact", "Balanced", "Comfortable"}};
    const std::array<std::string_view, 3> control_toggles{{"Toolbar", "Cards", "Blur"}};
    const std::array<IconDescriptor, 3> control_icons{{
        {"Search", "Query / filter", 0xF002u},
        {"Layout", "Panels / splits", 0xF0DBu},
        {"Motion", "Animation / easing", 0xF061u},
    }};
    const auto layout_gap = eui::quick::bind([&] { return 18.0f * scale; });
    const auto card_radius = eui::quick::bind([&] { return 22.0f * scale; });
    const auto heading_font = eui::quick::bind([&] { return font_heading(scale); });
    const auto compact_height = eui::quick::bind([&] { return 32.0f * scale; });
    const auto normal_height = eui::quick::bind([&] { return 36.0f * scale; });
    const float compact_h = 32.0f * scale;
    const float normal_h = 36.0f * scale;

    ui.row(rect)
        .tracks({eui::quick::fr(), eui::quick::fr(), eui::quick::fr(), eui::quick::fr()})
        .gap(layout_gap)
        .begin([&](auto& columns) {
            ui.card("Buttons")
                .in(columns.next())
                .title_font(heading_font)
                .radius(card_radius)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.view(card.content())
                        .column()
                        .tracks({eui::quick::px(72.0f * scale), eui::quick::fr(), eui::quick::px(18.0f * scale)})
                        .gap(12.0f * scale)
                        .begin([&](auto& rows) {
                            const Rect icon_row = rows.next();
                            const Rect content = rows.next();
                            const Rect summary_row = rows.next();

                            ui.row(icon_row)
                                .tracks({eui::quick::fr(), eui::quick::fr(), eui::quick::fr()})
                                .gap(eui::quick::bind([&] { return 8.0f * scale; }))
                                .begin([&](auto& icon_slots) {
                                    for (std::size_t i = 0; i < control_icons.size(); ++i) {
                                        const Rect chip = icon_slots.next();
                                        const bool is_selected = state.selected_control_icon == static_cast<int>(i);
                                        const bool is_hovered = hovered(input, chip);
                                        const float mix =
                                            ui.presence(ui.id("gallery/basic/icon-chip", static_cast<std::uint64_t>(i + 1u)),
                                                        is_hovered || is_selected, 18.0f, 12.0f);
                                        draw_fill(ui, chip,
                                                  is_selected ? nav_selected_fill_hex(palette)
                                                              : mix_hex(palette.surface_deep, palette.accent, 0.10f * mix),
                                                  16.0f * scale, 0.96f);
                                        draw_stroke(ui, chip,
                                                    is_selected ? palette.accent
                                                                : mix_hex(palette.border_soft, palette.accent, 0.18f * mix),
                                                    16.0f * scale, 1.0f, 0.88f);
                                        draw_icon(ui, control_icons[i].codepoint,
                                                  Rect{chip.x + chip.w * 0.5f - 8.0f * scale, chip.y + 12.0f * scale,
                                                       16.0f * scale, 16.0f * scale},
                                                  is_selected ? palette.accent : palette.text, 0.98f);
                                        draw_text_center(ui, control_icons[i].label,
                                                         Rect{chip.x + 6.0f * scale, chip.y + 38.0f * scale, chip.w - 12.0f * scale,
                                                              18.0f * scale},
                                                         font_meta(scale), is_selected ? palette.text : palette.muted, 0.98f);
                                        if (clicked(input, chip)) {
                                            state.selected_control_icon = static_cast<int>(i);
                                        }
                                    }
                                });

                            ui.view(content)
                                .column()
                                .tracks({eui::quick::px(normal_h), eui::quick::px(compact_h), eui::quick::px(compact_h)})
                                .gap(8.0f * scale)
                                .begin([&](auto& button_rows) {
                                    if (ui.button("Primary Action").in(button_rows.next()).primary().height(normal_height).draw()) {
                                        state.progress_ratio = std::min(1.0f, state.progress_ratio + 0.08f);
                                    }
                                    if (ui.button("Reset Progress").in(button_rows.next()).ghost().height(compact_height).draw()) {
                                        state.progress_ratio = 0.18f;
                                    }
                                    ui.readonly("Selected",
                                                std::string(control_icons[static_cast<std::size_t>(std::clamp(state.selected_control_icon, 0, 2))].label))
                                        .in(button_rows.next())
                                        .height(compact_height)
                                        .draw();
                                });

                            draw_text_left(ui, "Compact button sizing keeps the page readable.", summary_row, font_meta(scale), palette.muted, 0.96f);
                        });
                });

            ui.card("Selection")
                .in(columns.next())
                .title_font(heading_font)
                .radius(card_radius)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.column(card.content())
                        .tracks({eui::quick::px(compact_h), eui::quick::px(compact_h), eui::quick::px(compact_h), eui::quick::px(compact_h),
                                 eui::quick::px(compact_h), eui::quick::px(compact_h), eui::quick::px(compact_h), eui::quick::px(compact_h),
                                 eui::quick::px(compact_h)})
                        .gap(8.0f * scale)
                        .begin([&](auto& selection_rows) {
                            ui.readonly("Single", "Exclusive tab group").in(selection_rows.next()).height(compact_height).draw();
                            for (int i = 0; i < static_cast<int>(control_modes.size()); ++i) {
                                if (ui.tab(control_modes[static_cast<std::size_t>(i)], state.controls_mode == i)
                                        .in(selection_rows.next())
                                        .height(compact_height)
                                        .draw()) {
                                    state.controls_mode = i;
                                }
                            }
                            ui.readonly("Multi", "Independent toggles").in(selection_rows.next()).height(compact_height).draw();
                            for (int i = 0; i < static_cast<int>(control_toggles.size()); ++i) {
                                if (ui.tab(control_toggles[static_cast<std::size_t>(i)],
                                           state.controls_multi_select[static_cast<std::size_t>(i)])
                                        .in(selection_rows.next())
                                        .height(compact_height)
                                        .draw()) {
                                    state.controls_multi_select[static_cast<std::size_t>(i)] =
                                        !state.controls_multi_select[static_cast<std::size_t>(i)];
                                }
                            }
                            const std::string enabled_summary =
                                std::string(state.controls_multi_select[0] ? "Toolbar " : "") +
                                (state.controls_multi_select[1] ? "Cards " : "") +
                                (state.controls_multi_select[2] ? "Blur" : "");
                            ui.readonly("Enabled", enabled_summary.empty() ? "None" : enabled_summary)
                                .in(selection_rows.next())
                                .height(compact_height)
                                .draw();
                        });
                });

            ui.card("Inputs")
                .in(columns.next())
                .title_font(heading_font)
                .radius(card_radius)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    const std::string spacing_text = format_pixels(state.layout_gap);
                    const std::string density_text = std::string(control_modes[static_cast<std::size_t>(std::clamp(state.controls_mode, 0, 2))]);
                    ui.column(card.content())
                        .tracks({eui::quick::px(38.0f * scale), eui::quick::px(normal_h), eui::quick::px(normal_h), eui::quick::px(normal_h),
                                 eui::quick::px(10.0f * scale), eui::quick::px(compact_h), eui::quick::px(normal_h)})
                        .gap(8.0f * scale)
                        .begin([&](auto& input_rows) {
                            ui.input("Search", state.search_text)
                                .in(input_rows.next())
                                .placeholder("Type to filter")
                                .height(eui::quick::bind([&] { return 38.0f * scale; }))
                                .draw();

                            std::string dropdown_label = "Density: ";
                            dropdown_label += density_text;
                            const Rect dropdown_row = input_rows.next();
                            ui.scope(dropdown_row, [&](auto&) {
                                ui.dropdown(dropdown_label, state.controls_dropdown_open)
                                    .body_height(eui::quick::bind([&] { return 144.0f * scale; }))
                                    .padding(eui::quick::bind([&] { return 14.0f * scale; }))
                                    .begin([&](auto& menu) {
                                        ui.column(menu.content())
                                            .repeat(control_modes.size(), eui::quick::px(normal_h))
                                            .gap(8.0f * scale)
                                            .begin([&](auto& menu_rows) {
                                                for (int i = 0; i < static_cast<int>(control_modes.size()); ++i) {
                                                    if (ui.button(control_modes[static_cast<std::size_t>(i)])
                                                            .in(menu_rows.next())
                                                            .ghost()
                                                            .height(normal_height)
                                                            .draw()) {
                                                        state.controls_mode = i;
                                                        state.controls_dropdown_open = false;
                                                    }
                                                }
                                            });
                                    });
                            });

                            ui.slider("Progress", state.progress_ratio).in(input_rows.next()).range(0.0f, 1.0f).decimals(2).height(normal_height).draw();
                            ui.slider("Live Value", state.control_slider).in(input_rows.next()).range(0.0f, 1.0f).decimals(2).height(normal_height).draw();
                            ui.progress("Completion", state.progress_ratio).in(input_rows.next()).height(eui::quick::bind([&] { return 10.0f * scale; })).draw();
                            ui.readonly("Gap / Accent", spacing_text + " / " + format_hex_color(accent_hex(state)))
                                .in(input_rows.next())
                                .height(compact_height)
                                .draw();
                            if (ui.button("Jump To Animation Page").in(input_rows.next()).secondary().height(normal_height).draw()) {
                                state.selected_page = kPageAnimation;
                            }
                        });
                });

            ui.card("Editor")
                .in(columns.next())
                .title_font(heading_font)
                .radius(card_radius)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.readonly("Purpose", "Multiline input, wrapping and scroll behavior").height(compact_height).draw();
                    ui.text_area("Notes", state.notes_text)
                        .height(eui::quick::bind([&] { return std::max(160.0f * scale, card.content().h - 40.0f * scale); }))
                        .draw();
                });
        });
}

void draw_layout_page(UI& ui, GalleryState& state, const Rect& rect, float scale) {
    const std::uint32_t accent = accent_hex(state);
    const GalleryPalette palette = make_gallery_palette(state);
    ui.column(rect)
        .tracks({eui::quick::px(168.0f * scale), eui::quick::fr()})
        .gap(18.0f * scale)
        .begin([&](auto& page_rows) {
            ui.card("Layout Controls")
                .in(page_rows.next())
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.column(card.content())
                        .tracks({eui::quick::px(40.0f * scale), eui::quick::px(40.0f * scale), eui::quick::px(36.0f * scale)})
                        .gap(10.0f * scale)
                        .begin([&](auto& control_rows) {
                            ui.slider("Gap", state.layout_gap).in(control_rows.next()).range(8.0f, 28.0f).decimals(0).height(40.0f * scale).draw();
                            ui.slider("Radius", state.layout_radius).in(control_rows.next()).range(8.0f, 28.0f).decimals(0).height(40.0f * scale).draw();
                            ui.readonly("Core APIs", "view(), row(), column(), grid(), zstack()")
                                .in(control_rows.next())
                                .height(36.0f * scale)
                                .draw();
                        });
                });

            const Rect preview_rect = page_rows.next();
            const float gap = state.layout_gap * scale;
            const float radius = state.layout_radius * scale;

            ui.card("Layout Composition")
                .in(preview_rect)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.view(card.content())
                        .column()
                        .tracks({eui::quick::px(52.0f * scale), eui::quick::fr()})
                        .gap(gap)
                        .begin([&](auto& rows) {
                            const Rect toolbar = rows.next();
                            const Rect body = rows.next();

                            draw_fill(ui, toolbar, palette.surface_deep, radius, 0.98f);
                            draw_stroke(ui, toolbar, accent, radius, 1.0f, 0.52f);
                            draw_text_left(ui, "Toolbar / Filters", inset_rect(toolbar, 16.0f * scale, 14.0f * scale), font_heading(scale), palette.text, 0.98f);

                            ui.row(body)
                                .tracks({eui::quick::px(120.0f * scale), eui::quick::fr(1.8f), eui::quick::px(160.0f * scale)})
                                .gap(gap)
                                .begin([&](auto& cols) {
                                    const Rect sidebar = cols.next();
                                    const Rect center = cols.next();
                                    const Rect inspector = cols.next();

                                    draw_fill(ui, sidebar, palette.surface_deep, radius, 0.98f);
                                    draw_stroke(ui, sidebar, palette.border_soft, radius, 1.0f, 0.96f);
                                    draw_text_left(ui, "Sidebar", inset_rect(sidebar, 14.0f * scale, 14.0f * scale), font_heading(scale), palette.text, 0.98f);

                                    ui.column(center)
                                        .tracks({eui::quick::fr(3.0f), eui::quick::fr(1.0f)})
                                        .gap(gap)
                                        .begin([&](auto& center_rows) {
                                            const Rect canvas = center_rows.next();
                                            const Rect footer = center_rows.next();

                                            draw_fill(ui, canvas, palette.surface, radius, 0.98f);
                                            draw_stroke(ui, canvas, palette.border_soft, radius, 1.0f, 0.96f);
                                            draw_text_left(ui, "Canvas Region", inset_rect(canvas, 18.0f * scale, 16.0f * scale), font_heading(scale), palette.text, 0.98f);

                                            ui.zstack(canvas)
                                                .layers(2)
                                                .padding(18.0f * scale)
                                                .begin([&](auto& layers) {
                                                    const Rect base = layers.next();
                                                    const Rect overlay = inset_rect(layers.next(), 18.0f * scale, 18.0f * scale);
                                                    const Rect center_title{
                                                        base.x + 32.0f * scale,
                                                        base.y + base.h * 0.5f - 18.0f * scale,
                                                        base.w - 64.0f * scale,
                                                        20.0f * scale,
                                                    };
                                                    const Rect center_subtitle{
                                                        base.x + 40.0f * scale,
                                                        center_title.y + 22.0f * scale,
                                                        base.w - 80.0f * scale,
                                                        18.0f * scale,
                                                    };
                                                    draw_fill(ui, base, mix_hex(palette.surface_deep, accent, 0.08f), 18.0f * scale, 0.92f);
                                                    draw_stroke(ui, base, palette.border_soft, 18.0f * scale, 1.0f, 0.88f);
                                                    draw_stroke(ui, overlay, mix_hex(palette.border_soft, accent, 0.26f), 14.0f * scale, 1.0f, 0.90f);
                                                    draw_text_center(ui, "Centered Content", center_title, font_heading(scale), palette.text, 0.98f);
                                                    draw_text_center(ui, "zstack() keeps the middle region aligned while side panels resize.",
                                                                     center_subtitle, font_meta(scale), palette.muted, 0.96f);
                                                });

                                            draw_fill(ui, footer, palette.surface_deep, radius, 0.98f);
                                            draw_stroke(ui, footer, palette.border_soft, radius, 1.0f, 0.96f);
                                            draw_text_left(ui, "Footer Tools", inset_rect(footer, 18.0f * scale, 16.0f * scale), font_heading(scale), palette.text, 0.98f);
                                        });

                                    draw_fill(ui, inspector, palette.surface_deep, radius, 0.98f);
                                    draw_stroke(ui, inspector, mix_hex(palette.border_soft, accent, 0.26f), radius, 1.0f, 0.96f);
                                    draw_text_left(ui, "Inspector", inset_rect(inspector, 14.0f * scale, 14.0f * scale), font_heading(scale), palette.text, 0.98f);
                                });
                        });
                });
        });
}

void draw_animation_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale,
                         double time_seconds) {
    const GalleryPalette palette = make_gallery_palette(state);
    const std::uint32_t accent = accent_hex(state);
    const DemoDescriptor& current =
        kDemos[static_cast<std::size_t>(std::clamp(state.selected_animation_demo, 0, static_cast<int>(kDemos.size() - 1)))];
    ui.column(rect)
        .tracks({eui::quick::px(154.0f * scale), eui::quick::fr()})
        .gap(12.0f * scale)
        .begin([&](auto& rows) {
            const Rect selector_rect = rows.next();
            const Rect stage_rect = rows.next();

            ui.card("Animation Catalog")
                .in(selector_rect)
                .title_font(font_heading(scale))
                .radius(20.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.view(card.content())
                        .column()
                        .tracks({eui::quick::fr(), eui::quick::px(24.0f * scale)})
                        .gap(12.0f * scale)
                        .begin([&](auto& sections) {
                            const Rect grid_rect = sections.next();
                            const Rect info_row = sections.next();

                            auto demo_grid = ui.grid(grid_rect)
                                                 .repeat_rows(3, eui::quick::px(28.0f * scale))
                                                 .repeat_columns(3, eui::quick::fr())
                                                 .gap(10.0f * scale)
                                                 .begin();
                            for (std::size_t i = 0; i < kDemos.size(); ++i) {
                                const Rect item = demo_grid.next();
                                const bool is_selected = state.selected_animation_demo == static_cast<int>(i);
                                const bool is_hovered = hovered(input, item);
                                if (is_selected || is_hovered) {
                                    draw_fill(ui, item,
                                              is_selected ? mix_hex(palette.surface_deep, accent, 0.28f) : palette.surface_deep,
                                              item.h * 0.5f, is_selected ? 0.98f : 0.92f);
                                    draw_stroke(ui, item, is_selected ? accent : palette.border_soft, item.h * 0.5f, 1.0f,
                                                is_selected ? 0.96f : 0.88f);
                                } else {
                                    draw_fill(ui, item, palette.surface_deep, item.h * 0.5f, 0.84f);
                                }
                                draw_text_center(ui, kDemos[i].title, item, font_body(scale),
                                                 is_selected ? palette.text : palette.muted, 0.98f);
                                if (clicked(input, item)) {
                                    state.selected_animation_demo = static_cast<int>(i);
                                }
                            }

                            ui.row(info_row)
                                .tracks({eui::quick::fr(), eui::quick::px(222.0f * scale)})
                                .gap(14.0f * scale)
                                .begin([&](auto& info_cols) {
                                    const Rect summary_rect = info_cols.next();
                                    const Rect api_rect = info_cols.next();
                                    draw_text_left(ui, current.summary, summary_rect, font_meta(scale), palette.muted, 0.96f);
                                    draw_fill(ui, api_rect, palette.surface_deep, api_rect.h * 0.5f, 0.96f);
                                    draw_stroke(ui, api_rect, accent, api_rect.h * 0.5f, 1.0f, 0.90f);
                                    draw_text_center(ui, current.api_label, api_rect, font_meta(scale), mix_hex(palette.text, accent, 0.40f), 0.98f);
                                });
                        });
                });

            ui.card("Animation Stage")
                .in(stage_rect)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    const Rect scene = card.content();
                    for (std::size_t i = 0; i < kDemos.size(); ++i) {
                        const float mix =
                            ui.presence(ui.id("gallery/animation/view", static_cast<std::uint64_t>(i + 1u)),
                                        state.selected_animation_demo == static_cast<int>(i), 12.0f, 12.0f);
                        if (mix <= 0.01f) {
                            continue;
                        }
                        const float alpha = mix * mix * (3.0f - 2.0f * mix);
                        ui.ctx().set_global_alpha(alpha);
                        ui.clip(scene, [&] {
                            const Rect scene_rect{scene.x + (1.0f - alpha) * 14.0f * scale, scene.y, scene.w, scene.h};
                            draw_demo_scene(ui, static_cast<int>(i), scene_rect, scale, time_seconds, palette);
                        });
                        ui.ctx().set_global_alpha(1.0f);
                    }
                });
        });
}

void draw_dashboard_page(UI& ui, const GalleryState& state, const Rect& rect, float scale) {
    const std::uint32_t accent = accent_hex(state);
    const GalleryPalette palette = make_gallery_palette(state);
    ui.column(rect)
        .tracks({eui::quick::fr(2.1f), eui::quick::fr()})
        .gap(18.0f * scale)
        .begin([&](auto& page_rows) {
            const Rect preview = page_rows.next();
            const Rect stats = page_rows.next();

            ui.card("Dashboard Preview")
                .in(preview)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.row(card.content())
                        .tracks({eui::quick::px(120.0f * scale), eui::quick::fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& cols) {
                            const Rect sidebar = cols.next();
                            const Rect main = cols.next();

                            draw_fill(ui, sidebar, palette.surface_deep, 18.0f * scale, 0.98f);
                            draw_stroke(ui, sidebar, palette.border_soft, 18.0f * scale, 1.0f, 0.96f);
                            draw_text_left(ui, "Dashboard",
                                           Rect{sidebar.x + 14.0f * scale, sidebar.y + 16.0f * scale, sidebar.w - 28.0f * scale, 18.0f * scale},
                                           font_heading(scale), palette.text, 1.0f);
                            for (int i = 0; i < 4; ++i) {
                                const Rect item{sidebar.x + 12.0f * scale, sidebar.y + 48.0f * scale + static_cast<float>(i) * 42.0f * scale,
                                                sidebar.w - 24.0f * scale, 30.0f * scale};
                                draw_fill(ui, item, i == 1 ? nav_selected_fill_hex(palette) : nav_idle_fill_hex(palette), 15.0f * scale,
                                          i == 1 ? 0.98f : 0.84f);
                                draw_text_left(ui, i == 0 ? "Overview" : i == 1 ? "Orders" : i == 2 ? "Activity" : "Settings",
                                               Rect{item.x + 12.0f * scale, item.y + 7.0f * scale, item.w - 24.0f * scale, 16.0f * scale},
                                               font_body(scale), i == 1 ? palette.text : palette.muted, 0.98f);
                            }

                            ui.column(main)
                                .tracks({eui::quick::px(76.0f * scale), eui::quick::px(94.0f * scale), eui::quick::fr()})
                                .gap(14.0f * scale)
                                .begin([&](auto& main_rows) {
                                    const Rect hero = main_rows.next();
                                    const Rect cards_row = main_rows.next();
                                    const Rect activity = main_rows.next();

                                    draw_fill(ui, hero, palette.surface_deep, 18.0f * scale, 0.98f);
                                    draw_stroke(ui, hero, palette.border_soft, 18.0f * scale, 1.0f, 0.96f);
                                    draw_text_left(ui, "Dashboard Preview",
                                                   Rect{hero.x + 18.0f * scale, hero.y + 14.0f * scale, hero.w - 36.0f * scale, 18.0f * scale},
                                                   font_heading(scale), palette.text, 1.0f);
                                    draw_text_left(ui, "The full standalone example remains available as reference_dashboard_demo.cpp",
                                                   Rect{hero.x + 18.0f * scale, hero.y + 40.0f * scale, hero.w - 36.0f * scale, 16.0f * scale},
                                                   font_meta(scale), palette.muted, 0.96f);

                                    ui.row(cards_row)
                                        .tracks({eui::quick::fr(), eui::quick::fr(), eui::quick::fr()})
                                        .gap(12.0f * scale)
                                        .begin([&](auto& metric_cols) {
                                            ui.metric("Revenue", "$128k").in(metric_cols.next()).tag("up").caption("Month over month +18%")
                                                .label_font(font_body(scale)).value_font(font_heading(scale)).caption_font(font_body(scale)).tag_font(font_meta(scale))
                                                .gradient(mix_hex(palette.surface_deep, accent, 0.10f), mix_hex(palette.surface_alt, accent, 0.18f)).draw();
                                            ui.metric("Orders", "1,284").in(metric_cols.next()).tag("live").caption("Refreshed from shared state")
                                                .label_font(font_body(scale)).value_font(font_heading(scale)).caption_font(font_body(scale)).tag_font(font_meta(scale))
                                                .gradient(mix_hex(palette.surface_deep, accent, 0.10f), mix_hex(palette.surface_alt, accent, 0.18f)).draw();
                                            ui.metric("Retention", "92%").in(metric_cols.next()).tag("stable").caption("Reusable metric surface")
                                                .label_font(font_body(scale)).value_font(font_heading(scale)).caption_font(font_body(scale)).tag_font(font_meta(scale))
                                                .gradient(mix_hex(palette.surface_deep, accent, 0.10f), mix_hex(palette.surface_alt, accent, 0.18f)).draw();
                                        });

                                    draw_fill(ui, activity, palette.surface_deep, 18.0f * scale, 0.98f);
                                    draw_stroke(ui, activity, palette.border_soft, 18.0f * scale, 1.0f, 0.96f);
                                    draw_text_left(ui, "Activity",
                                                   Rect{activity.x + 18.0f * scale, activity.y + 14.0f * scale, 120.0f * scale, 18.0f * scale},
                                                   font_heading(scale), palette.text, 1.0f);
                                    const float plot_bottom = activity.y + activity.h - 26.0f * scale;
                                    float bar_x = activity.x + 24.0f * scale;
                                    for (int i = 0; i < 7; ++i) {
                                        const float height = (0.28f + static_cast<float>((i * 37) % 53) / 100.0f) * (activity.h - 66.0f * scale);
                                        draw_fill(ui, Rect{bar_x, plot_bottom - height, 18.0f * scale, height},
                                                  i == 5 ? accent : demo_track_hex(palette), 9.0f * scale, i == 5 ? 0.98f : 0.86f);
                                        bar_x += 28.0f * scale;
                                    }
                                });
                        });
                });

            ui.row(stats)
                .tracks({eui::quick::fr(), eui::quick::fr()})
                .gap(18.0f * scale)
                .begin([&](auto& stats_cols) {
                    ui.card("What This Proves")
                        .in(stats_cols.next())
                        .title_font(font_heading(scale))
                        .radius(22.0f * scale)
                        .fill(palette.surface_alt)
                        .stroke(palette.border, 1.0f)
                        .begin([&](auto& card) {
                            ui.column(card.content())
                                .tracks({eui::quick::px(36.0f * scale), eui::quick::px(36.0f * scale)})
                                .gap(8.0f * scale)
                                .begin([&](auto& info_rows) {
                                    ui.readonly("Composition", "Sidebar + cards + chart + metrics")
                                        .in(info_rows.next())
                                        .height(36.0f * scale)
                                        .draw();
                                    ui.readonly("Reuse", "Same primitives power the standalone dashboard")
                                        .in(info_rows.next())
                                        .height(36.0f * scale)
                                        .draw();
                                });
                        });
                    ui.card("Usage")
                        .in(stats_cols.next())
                        .title_font(font_heading(scale))
                        .radius(22.0f * scale)
                        .fill(palette.surface_alt)
                        .stroke(palette.border, 1.0f)
                        .begin([&](auto&) {
                            ui.text_area_readonly("Standalone File", "reference_dashboard_demo.cpp remains as the dedicated dashboard example target.")
                                .height(120.0f * scale)
                                .draw();
                        });
                });
        });
}

void draw_settings_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    ui.row(rect)
        .tracks({eui::quick::fr(1.08f), eui::quick::fr(0.92f)})
        .gap(18.0f * scale)
        .begin([&](auto& cols) {
            const Rect settings_rect = cols.next();
            const Rect preview_rect = cols.next();

            ui.card("Gallery Settings")
                .in(settings_rect)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    ui.view(card.content())
                        .column()
                        .tracks({eui::quick::px(14.0f * scale), eui::quick::px(38.0f * scale), eui::quick::px(14.0f * scale),
                                 eui::quick::px(40.0f * scale), eui::quick::px(14.0f * scale), eui::quick::px(42.0f * scale),
                                 eui::quick::px(36.0f * scale), eui::quick::px(36.0f * scale), eui::quick::px(36.0f * scale),
                                 eui::quick::px(36.0f * scale), eui::quick::px(36.0f * scale)})
                        .gap(12.0f * scale)
                        .begin([&](auto& rows) {
                            const Rect mode_label = rows.next();
                            const Rect mode_row = rows.next();
                            const Rect accent_label = rows.next();
                            const Rect accent_row = rows.next();
                            const Rect custom_label = rows.next();
                            const Rect custom_row = rows.next();
                            const Rect red_row = rows.next();
                            const Rect green_row = rows.next();
                            const Rect blue_row = rows.next();
                            const Rect radius_row = rows.next();
                            const Rect blur_row = rows.next();

                            draw_text_left(ui, "Theme Mode", mode_label, font_meta(scale), palette.muted, 0.96f);
                            ui.row(mode_row)
                                .tracks({eui::quick::fr(), eui::quick::fr()})
                                .gap(10.0f * scale)
                                .begin([&](auto& mode_cols) {
                                    const Rect dark_rect = mode_cols.next();
                                    const Rect light_rect = mode_cols.next();
                                    const std::uint32_t accent = accent_hex(state);
                                    draw_fill(ui, dark_rect, state.light_mode ? palette.surface_deep : nav_selected_fill_hex(palette), dark_rect.h * 0.5f, 0.98f);
                                    draw_stroke(ui, dark_rect, state.light_mode ? palette.border_soft : accent, dark_rect.h * 0.5f, 1.0f, 0.96f);
                                    draw_text_center(ui, "Dark", dark_rect, font_body(scale), state.light_mode ? palette.muted : palette.text, 0.98f);
                                    draw_fill(ui, light_rect, state.light_mode ? nav_selected_fill_hex(palette) : palette.surface_deep, light_rect.h * 0.5f, 0.98f);
                                    draw_stroke(ui, light_rect, state.light_mode ? accent : palette.border_soft, light_rect.h * 0.5f, 1.0f, 0.96f);
                                    draw_text_center(ui, "Light", light_rect, font_body(scale), state.light_mode ? palette.text : palette.muted, 0.98f);
                                    if (clicked(input, dark_rect)) {
                                        state.light_mode = false;
                                    }
                                    if (clicked(input, light_rect)) {
                                        state.light_mode = true;
                                    }
                                });

                            draw_text_left(ui, "Accent Color", accent_label, font_meta(scale), palette.muted, 0.96f);
                            const float swatch = 28.0f * scale;
                            const int active_accent = accent_uses_custom_rgb(state) ? custom_accent_slot() : state.accent_index;
                            ui.row(accent_row)
                                .repeat(static_cast<std::size_t>(custom_accent_slot() + 1), eui::quick::px(swatch))
                                .gap(10.0f * scale)
                                .begin([&](auto& chips) {
                                    for (int i = 0; i <= custom_accent_slot(); ++i) {
                                        const std::uint32_t swatch_hex =
                                            i == custom_accent_slot() ? custom_accent_hex(state) : kAccentHexes[static_cast<std::size_t>(i)];
                                        const Rect chip = chips.next();
                                        draw_fill(ui, chip, swatch_hex, chip.w * 0.5f, 0.98f);
                                        if (i == custom_accent_slot()) {
                                            draw_stroke(ui, Rect{chip.x + 4.0f * scale, chip.y + 4.0f * scale, chip.w - 8.0f * scale, chip.h - 8.0f * scale},
                                                        palette.surface_alt, (chip.w - 8.0f * scale) * 0.5f, 1.0f, 0.90f);
                                        }
                                        if (active_accent == i) {
                                            draw_stroke(ui, Rect{chip.x - 3.0f, chip.y - 3.0f, chip.w + 6.0f, chip.h + 6.0f},
                                                        palette.text, (chip.w + 6.0f) * 0.5f, 1.0f, 0.92f);
                                        }
                                        if (clicked(input, Rect{chip.x - 4.0f, chip.y - 4.0f, chip.w + 8.0f, chip.h + 8.0f})) {
                                            state.accent_index = i;
                                        }
                                    }
                                });

                            draw_text_left(ui, "Custom RGB", custom_label, font_meta(scale), palette.muted, 0.96f);
                            ui.row(custom_row)
                                .tracks({eui::quick::fr(0.24f), eui::quick::fr(0.76f)})
                                .gap(10.0f * scale)
                                .begin([&](auto& custom_cols) {
                                    const Rect custom_chip = custom_cols.next();
                                    const Rect custom_info = custom_cols.next();
                                    const std::uint32_t custom_hex = custom_accent_hex(state);
                                    draw_fill(ui, custom_chip, custom_hex, 14.0f * scale, 0.98f);
                                    draw_stroke(ui, custom_chip,
                                                active_accent == custom_accent_slot() ? palette.text : palette.border_soft,
                                                14.0f * scale, 1.0f, active_accent == custom_accent_slot() ? 0.94f : 0.86f);
                                    draw_fill(ui, custom_info, palette.surface_deep, custom_info.h * 0.5f, 0.96f);
                                    draw_stroke(ui, custom_info, palette.border_soft, custom_info.h * 0.5f, 1.0f, 0.86f);
                                    draw_text_left(ui, "Live custom accent",
                                                   Rect{custom_info.x + 12.0f * scale, custom_info.y + 6.0f * scale,
                                                        custom_info.w - 24.0f * scale, 14.0f * scale},
                                                   font_meta(scale), palette.muted, 0.96f);
                                    draw_text_left(ui, format_hex_color(custom_hex),
                                                   Rect{custom_info.x + 12.0f * scale, custom_info.y + 20.0f * scale,
                                                        custom_info.w - 24.0f * scale, 16.0f * scale},
                                                   font_body(scale), palette.text, 0.98f);
                                    if (clicked(input, custom_row)) {
                                        state.accent_index = custom_accent_slot();
                                    }
                                });

                            if (ui.slider("Red", state.custom_accent_r).in(red_row).range(0.0f, 255.0f).decimals(0).draw()) {
                                state.accent_index = custom_accent_slot();
                            }
                            if (ui.slider("Green", state.custom_accent_g).in(green_row).range(0.0f, 255.0f).decimals(0).draw()) {
                                state.accent_index = custom_accent_slot();
                            }
                            if (ui.slider("Blue", state.custom_accent_b).in(blue_row).range(0.0f, 255.0f).decimals(0).draw()) {
                                state.accent_index = custom_accent_slot();
                            }
                            ui.slider("Corner Radius", state.layout_radius).in(radius_row).range(8.0f, 28.0f).decimals(0).draw();
                            ui.slider("Glass Blur", state.settings_blur).in(blur_row).range(0.0f, 28.0f).decimals(0).draw();
                        });
                });

            ui.card("Live Preview")
                .in(preview_rect)
                .title_font(font_heading(scale))
                .radius(22.0f * scale)
                .fill(palette.surface_alt)
                .stroke(palette.border, 1.0f)
                .begin([&](auto& card) {
                    const std::uint32_t accent = accent_hex(state);
                    const float preview_radius = state.layout_radius * scale;
                    const Rect preview = inset_rect(card.content(), 6.0f * scale);
                    ui.clip(preview, [&] {
                        draw_fill(ui, preview, preview_backdrop_hex(palette), preview_radius, 1.0f);
                        draw_stroke(ui, preview, accent, preview_radius, 1.0f, 0.76f);
                        const Rect inner = inset_rect(preview, 18.0f * scale);
                        draw_stage_background(ui, inner, scale, make_gallery_palette(state));
                        draw_blur_reference(ui, inset_rect(inner, 34.0f * scale), scale * 0.82f, palette);
                        draw_actor(ui,
                                   Rect{inner.x + inner.w * 0.5f - 80.0f * scale, inner.y + inner.h * 0.5f - 50.0f * scale,
                                        160.0f * scale, 100.0f * scale},
                                   scale, palette, actor_glass_top_hex(palette), actor_glass_bottom_hex(palette), 1.0f,
                                   state.settings_blur * scale, state.settings_blur * scale, 0.0f, 0.0f, 1.0f, 0.0f, 0.18f);
                    });
                });
        });
}

void draw_about_page(UI& ui, const eui::InputState& input, const GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const std::uint32_t accent = accent_hex(state);
    const Rect content = inset_rect(rect, 32.0f * scale);
    const float hero_w = std::min(content.w, 720.0f * scale);
    const float hero_h = std::min(content.h * 0.44f, 236.0f * scale);

    ui.view(content)
        .column()
        .tracks({eui::quick::px(hero_h), eui::quick::fr()})
        .gap(18.0f * scale)
        .begin([&](auto& rows) {
            const Rect hero_slot = rows.next();
            const Rect info_area = rows.next();

            ui.row(hero_slot)
                .tracks({eui::quick::fr(), eui::quick::px(hero_w), eui::quick::fr()})
                .begin([&](auto& hero_cols) {
                    hero_cols.next();
                    const Rect hero = hero_cols.next();

                    draw_fill(ui, hero, palette.surface_deep, 24.0f * scale, 0.96f);
                    draw_stroke(ui, hero, mix_hex(palette.border_soft, accent, 0.18f), 24.0f * scale, 1.0f, 0.90f);
                    draw_text_center(ui, "EUI", Rect{hero.x, hero.y + 20.0f * scale, hero.w, 22.0f * scale},
                                     font_heading(scale) * 2.0f, mix_hex(palette.text, accent, 0.24f), 0.98f);
                    draw_text_center(ui, "Created by SudoEvolve",
                                     Rect{hero.x, hero.y + 50.0f * scale, hero.w, 18.0f * scale},
                                     font_body(scale), palette.text, 0.98f);
                    draw_text_center(ui,
                                     "Immediate-mode GUI with crisp text, modern surfaces, reusable controls and fast iteration for desktop tooling.",
                                     Rect{hero.x + 28.0f * scale, hero.y + 60.0f * scale, hero.w - 56.0f * scale, 38.0f * scale},
                                     font_body(scale), palette.muted, 0.98f);

                    const float buttons_w = std::min(hero.w - 64.0f * scale, 420.0f * scale);
                    const Rect buttons_row{
                        hero.x + (hero.w - buttons_w) * 0.5f,
                        hero.y + hero.h - 80.0f * scale,
                        buttons_w,
                        42.0f * scale,
                    };
                    ui.row(buttons_row)
                        .tracks({eui::quick::fr(), eui::quick::fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& button_cols) {
                            const Rect github_button = button_cols.next();
                            const Rect mail_button = button_cols.next();
                            const bool github_hovered = hovered(input, github_button);
                            const bool mail_hovered = hovered(input, mail_button);
                            const float github_mix = ui.presence(ui.id("gallery/about/github"), github_hovered, 18.0f, 12.0f);
                            const float mail_mix = ui.presence(ui.id("gallery/about/mail"), mail_hovered, 18.0f, 12.0f);

                            draw_fill(ui, github_button, mix_hex(palette.accent, 0xFFFFFF, palette.light ? 0.14f : 0.04f * github_mix),
                                      github_button.h * 0.5f, 0.98f);
                            draw_stroke(ui, github_button, mix_hex(palette.accent, palette.text, 0.12f), github_button.h * 0.5f, 1.0f, 0.94f);
                            draw_icon(ui, 0xF121u,
                                      Rect{github_button.x + 24.0f * scale, github_button.y + 13.0f * scale, 18.0f * scale, 18.0f * scale},
                                      palette.light ? 0x0F172A : 0xFFFFFF, 0.98f);
                            draw_text_left(ui, "GitHub",
                                           Rect{github_button.x + 52.0f * scale, github_button.y + 13.0f * scale,
                                                github_button.w - 68.0f * scale, 18.0f * scale},
                                           font_body(scale), palette.light ? 0x0F172A : 0xFFFFFF, 0.98f);

                            draw_fill(ui, mail_button, palette.surface_deep, mail_button.h * 0.5f, 0.96f);
                            draw_stroke(ui, mail_button, mix_hex(palette.border_soft, accent, 0.20f + 0.18f * mail_mix), mail_button.h * 0.5f, 1.0f, 0.90f);
                            draw_icon(ui, 0xF0E0u,
                                      Rect{mail_button.x + 24.0f * scale, mail_button.y + 13.0f * scale, 18.0f * scale, 18.0f * scale},
                                      palette.text, 0.98f);
                            draw_text_left(ui, "Email",
                                           Rect{mail_button.x + 52.0f * scale, mail_button.y + 13.0f * scale, mail_button.w - 68.0f * scale, 18.0f * scale},
                                           font_body(scale), palette.text, 0.98f);

                            if (clicked(input, github_button)) {
                                open_external_uri("https://github.com/sudoevolve");
                            }
                            if (clicked(input, mail_button)) {
                                open_external_uri("mailto:sudoevolve@gmail.com");
                            }
                        });
                });

            ui.row(info_area)
                .tracks({eui::quick::fr(), eui::quick::fr()})
                .gap(16.0f * scale)
                .begin([&](auto& info_cols) {
                    const Rect project = info_cols.next();
                    const Rect license = info_cols.next();

                    draw_fill(ui, project, palette.surface_deep, 22.0f * scale, 0.96f);
                    draw_stroke(ui, project, palette.border_soft, 22.0f * scale, 1.0f, 0.88f);
                    draw_text_left(ui, "Project", Rect{project.x + 20.0f * scale, project.y + 18.0f * scale,
                                                       project.w - 40.0f * scale, 18.0f * scale},
                                   font_heading(scale), palette.text, 0.98f);
                    draw_text_left(ui, "Name: EUI", Rect{project.x + 20.0f * scale, project.y + 52.0f * scale,
                                                         project.w - 40.0f * scale, 16.0f * scale},
                                   font_body(scale), palette.text, 0.98f);
                    draw_text_left(ui, "Author: SudoEvolve", Rect{project.x + 20.0f * scale, project.y + 78.0f * scale,
                                                                  project.w - 40.0f * scale, 16.0f * scale},
                                   font_body(scale), palette.text, 0.98f);
                    draw_text_left(ui, "Renderer: Modern OpenGL", Rect{project.x + 20.0f * scale, project.y + 104.0f * scale,
                                                                       project.w - 40.0f * scale, 16.0f * scale},
                                   font_meta(scale), palette.muted, 0.98f);
                    draw_text_left(ui, "Repository: github.com/sudoevolve", Rect{project.x + 20.0f * scale, project.y + 130.0f * scale,
                                                                                 project.w - 40.0f * scale, 16.0f * scale},
                                   font_meta(scale), palette.muted, 0.98f);

                    draw_fill(ui, license, palette.surface_deep, 22.0f * scale, 0.96f);
                    draw_stroke(ui, license, palette.border_soft, 22.0f * scale, 1.0f, 0.88f);
                    draw_text_left(ui, "License & Contact", Rect{license.x + 20.0f * scale, license.y + 18.0f * scale,
                                                                 license.w - 40.0f * scale, 18.0f * scale},
                                   font_heading(scale), palette.text, 0.98f);
                    draw_text_left(ui, "MIT License", Rect{license.x + 20.0f * scale, license.y + 52.0f * scale,
                                                           license.w - 40.0f * scale, 16.0f * scale},
                                   font_body(scale), palette.text, 0.98f);
                    draw_text_left(ui, "Copyright (c) 2026 SudoEvolve", Rect{license.x + 20.0f * scale, license.y + 78.0f * scale,
                                                                             license.w - 40.0f * scale, 16.0f * scale},
                                   font_meta(scale), palette.muted, 0.98f);
                    draw_text_left(ui, "Email: sudoevolve@gmail.com", Rect{license.x + 20.0f * scale, license.y + 104.0f * scale,
                                                                           license.w - 40.0f * scale, 16.0f * scale},
                                   font_body(scale), palette.text, 0.98f);
                    draw_text_left(ui, "GitHub button opens the profile directly in your browser.", Rect{license.x + 20.0f * scale,
                                                                                                           license.y + 132.0f * scale,
                                                                                                           license.w - 40.0f * scale,
                                                                                                           28.0f * scale},
                                   font_meta(scale), palette.muted, 0.96f);
                });
        });
}

void draw_sidebar(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const std::uint32_t accent = accent_hex(state);
    draw_fill(ui, rect, palette.surface, 26.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.border, 26.0f * scale, 1.0f, 1.0f);

    ui.view(rect)
        .padding(18.0f * scale)
        .column()
        .tracks({eui::quick::px(54.0f * scale), eui::quick::fr()})
        .gap(8.0f * scale)
        .begin([&](auto& rows) {
            const Rect header = rows.next();
            const Rect list_rect = rows.next();

            draw_text_left(ui, "EUI Gallery", Rect{header.x, header.y, header.w, 22.0f * scale}, font_display(scale), palette.text, 1.0f);
            draw_text_left(ui, "Controls, design, layout, motion, dashboard, settings and about.",
                           Rect{header.x, header.y + 26.0f * scale, header.w, 18.0f * scale}, font_meta(scale), palette.muted, 0.96f);

            const float row_h = 42.0f * scale;
            const float row_gap = 8.0f * scale;
            std::array<Rect, kPages.size()> page_rows{};
            auto nav_rows = ui.column(list_rect).repeat(kPages.size(), eui::quick::px(row_h)).gap(row_gap).begin();
            for (std::size_t i = 0; i < kPages.size(); ++i) {
                page_rows[i] = nav_rows.next();
            }

            const Rect selected_row = page_rows[static_cast<std::size_t>(std::clamp(state.selected_page, 0, static_cast<int>(kPages.size() - 1)))];
            const std::uint64_t indicator_id = ui.id("gallery/sidebar-indicator");
            if (ui.motion_value(indicator_id, -1.0f) < 0.0f) {
                ui.reset_motion(indicator_id, selected_row.y);
            }
            const float indicator_y = ui.motion(indicator_id, selected_row.y, 18.0f);
            draw_fill(ui, Rect{selected_row.x, indicator_y, selected_row.w, row_h}, nav_selected_fill_hex(palette), 18.0f * scale, 0.96f);
            draw_stroke(ui, Rect{selected_row.x, indicator_y, selected_row.w, row_h}, accent, 18.0f * scale, 1.0f, 0.72f);
            draw_fill(ui, Rect{selected_row.x, indicator_y, 4.0f * scale, row_h}, accent, 2.0f * scale, 0.98f);

            for (std::size_t i = 0; i < kPages.size(); ++i) {
                const Rect row = page_rows[i];
                const bool is_hovered = hovered(input, row);
                const bool is_selected = state.selected_page == static_cast<int>(i);
                const float hover_mix =
                    ui.presence(ui.id("gallery/sidebar-hover", static_cast<std::uint64_t>(i + 1u)), is_hovered, 18.0f, 12.0f);

                if (!is_selected && is_hovered) {
                    draw_fill(ui, row, palette.surface_deep, 18.0f * scale, 0.92f * hover_mix);
                    draw_stroke(ui, row, palette.border_soft, 18.0f * scale, 1.0f, 0.88f * hover_mix);
                }
                if (clicked(input, row)) {
                    state.selected_page = static_cast<int>(i);
                }

                draw_icon(ui, kPageIcons[i],
                          Rect{row.x + 14.0f * scale, row.y + (row.h - 14.0f * scale) * 0.5f, 14.0f * scale, 14.0f * scale},
                          is_selected ? accent : palette.muted, 0.98f);
                draw_text_left(ui, kPages[i].title,
                               Rect{row.x + 38.0f * scale, row.y + 12.0f * scale, row.w - 54.0f * scale, 16.0f * scale},
                               font_body(scale), is_selected ? palette.text : mix_hex(palette.text, palette.muted, 0.22f), 0.98f);
            }
        });
}

void draw_stage(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale, double time_seconds) {
    const GalleryPalette palette = make_gallery_palette(state);
    const PageDescriptor& page = kPages[static_cast<std::size_t>(std::clamp(state.selected_page, 0, static_cast<int>(kPages.size() - 1)))];

    draw_fill(ui, rect, palette.surface, 26.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.border, 26.0f * scale, 1.0f, 1.0f);
    ui.view(rect)
        .padding(22.0f * scale)
        .column()
        .tracks({eui::quick::px(24.0f * scale), eui::quick::px(18.0f * scale), eui::quick::px(30.0f * scale), eui::quick::fr()})
        .gap(12.0f * scale)
        .begin([&](auto& rows) {
            const Rect title_rect = rows.next();
            const Rect summary_rect = rows.next();
            const Rect chip_rect = rows.next();
            const Rect stage_rect = rows.next();

            draw_text_left(ui, page.title, title_rect, font_display(scale), palette.text, 1.0f);
            draw_text_left(ui, page.summary, summary_rect, font_meta(scale), palette.muted, 0.96f);
            draw_fill(ui, chip_rect, palette.surface_deep, chip_rect.h * 0.5f, 0.96f);
            draw_stroke(ui, chip_rect, palette.border_soft, chip_rect.h * 0.5f, 1.0f, 0.90f);
            draw_text_center(ui, page.api_label, chip_rect, font_meta(scale), mix_hex(palette.text, palette.accent, 0.40f), 0.98f);

            switch (state.selected_page) {
                case kPageBasicControls:
                    draw_basic_controls_page(ui, input, state, stage_rect, scale);
                    break;
                case kPageDesign:
                    draw_design_page(ui, input, state, stage_rect, scale);
                    break;
                case kPageLayout:
                    draw_layout_page(ui, state, stage_rect, scale);
                    break;
                case kPageAnimation:
                    draw_animation_page(ui, input, state, stage_rect, scale, time_seconds);
                    break;
                case kPageDashboard:
                    draw_dashboard_page(ui, state, stage_rect, scale);
                    break;
                case kPageSettings:
                    draw_settings_page(ui, input, state, stage_rect, scale);
                    break;
                case kPageAbout:
                default:
                    draw_about_page(ui, input, state, stage_rect, scale);
                    break;
            }
        });
}

}  // namespace

int main() {
    GalleryState state{};

    eui::app::AppOptions options{};
    options.title = "EUI Gallery";
    options.width = 1420;
    options.height = 860;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 240.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const auto& input = ctx.input_state();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const double time_seconds = input.time_seconds;

            const std::uint32_t accent = accent_hex(state);
            const GalleryPalette palette = make_gallery_palette(state);
            ctx.set_theme_mode(state.light_mode ? eui::ThemeMode::Light : eui::ThemeMode::Dark);
            ctx.set_primary_color(color_from_hex(accent));
            ctx.set_corner_radius(std::max(8.0f, state.layout_radius * scale * 0.7f));

            UI ui(ctx);
            const float margin = 18.0f * scale;
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel()
                .in(frame_rect)
                .padding(18.0f * scale)
                .radius(30.0f * scale)
                .gradient(palette.shell_top, palette.shell_bottom)
                .stroke(palette.border, 1.0f)
                .shadow(0.0f, 14.0f * scale, 28.0f * scale, panel_shadow_hex(palette), palette.light ? 0.10f : 0.18f)
                .begin([&](auto& root) {
                    ui.view(root.content())
                        .column()
                        .tracks({eui::quick::px(68.0f * scale), eui::quick::fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& shell_rows) {
                            const Rect header = shell_rows.next();
                            const Rect body = shell_rows.next();

                            draw_fill(ui, header, palette.surface_alt, 22.0f * scale, 0.96f);
                            draw_stroke(ui, header, palette.border, 22.0f * scale, 1.0f, 1.0f);
                            draw_text_left(ui, "EUI Gallery",
                                           Rect{header.x + 22.0f * scale, header.y + 14.0f * scale, header.w - 44.0f * scale, 20.0f * scale},
                                           font_display(scale), palette.text, 1.0f);
                            draw_text_left(ui, "Left side is now top-level navigation only. Animation detail lives inside the Animation page.",
                                           Rect{header.x + 22.0f * scale, header.y + 36.0f * scale, header.w - 44.0f * scale, 16.0f * scale},
                                           font_meta(scale), palette.muted, 0.96f);

                            const float sidebar_w = std::min(320.0f * scale, body.w * 0.26f);
                            ui.row(body)
                                .tracks({eui::quick::px(sidebar_w), eui::quick::fr()})
                                .gap(18.0f * scale)
                                .begin([&](auto& shell_cols) {
                                    draw_sidebar(ui, input, state, shell_cols.next(), scale);
                                    draw_stage(ui, input, state, shell_cols.next(), scale, time_seconds);
                                });
                        });
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
