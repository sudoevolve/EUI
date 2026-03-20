# EUI

[中文说明](readme.zh-CN.md)

EUI is a lightweight, header-only C++ UI toolkit focused on practical immediate-mode workflows.
The core API in `include/EUI.h` generates draw commands only.
A GLFW + OpenGL demo runtime is available when `EUI_ENABLE_GLFW_OPENGL_BACKEND` is enabled.
It now includes a more complete text pipeline for mixed text/icon rendering, editable inputs, and scrolling text areas.
Major widgets also include built-in lightweight motion feedback for hover, press, focus, dropdown reveal, and progress changes.
The current motion defaults are tuned to stay compatible with event-driven rendering and restrained GPU usage.

## Preview

<table>
  <tr>
    <td width="50%"><img src="preview/0.jpg" alt="Preview 0" width="100%" /></td>
    <td width="50%"><img src="preview/1.jpg" alt="Preview 1" width="100%" /></td>
  </tr>
  <tr>
    <td width="50%"><img src="preview/2.jpg" alt="Preview 2" width="100%" /></td>
    <td width="50%"><img src="preview/3.jpg" alt="Preview 3" width="100%" /></td>
  </tr>
</table>

## Project Analysis (Current Code)

### 1) Architecture

- **Core layer (`eui::Context`)**
  - Immediate-mode UI API that emits `DrawCommand` + text arena.
  - No hard dependency on GLFW/OpenGL for core usage.
- **Optional demo runtime (`eui::demo`)**
  - Window/input loop, DPI extraction, clipboard bridge, frame scheduling.
  - Calls your UI builder callback with `FrameContext`.
- **Renderer layer (inside `EUI.h`)**
  - OpenGL command renderer with clipping and batching.
- Optional `stb_truetype` renderer for text/icons with glyph texture caching (auto-enabled when `stb_truetype.h` is available).
- Text measurement is configured to follow the active renderer backend so caret, selection, wrapping, and hit-testing stay aligned with what is actually rendered.
- Uses text font + icon font fallback (private-use codepoints prefer icon font).
- Icon font defaults to bundled `include/Font Awesome 7 Free-Solid-900.otf`; text keeps system default.
- Falls back to built-in bitmap text only when font loading/rendering fails.
- You can force-disable STB at compile time with `-DEUI_ENABLE_STB_TRUETYPE=0`.

### 2) Rendering Pipeline

1. `ui.begin_frame(...)`
2. Build UI widgets and layout.
3. `ui.take_frame(...)` gets command buffer + text arena.
4. Runtime hashes frame payload.
5. If unchanged, skip redraw.
6. If changed, compute dirty regions vs previous frame.
7. Repaint only dirty scissor regions; reuse cached framebuffer texture for the rest.

### 3) Performance Mechanisms Already Implemented

- Event-driven rendering (`continuous_render = false` by default).
- Frame hash early-out to avoid redundant GPU work.
- Dirty-region diff between previous and current draw command streams.
- Cached framebuffer texture + partial redraw via scissor.
- Large dirty-area changes fall back to full redraw instead of forcing cache replay + many partial updates.
- Clip stack + command clipping in core.
- Tile-assisted command bucketing for large command counts.
- Motion states only request redraw while hover/focus/press/value animations are still settling.
- Weak glows are culled early, large surfaces stay static, and tiny motion deltas snap to rest quickly.

### 4) Text / Editing Model

- Single-line inputs support caret movement, drag selection, clipboard shortcuts, and horizontal scroll-to-caret.
- `text_area` supports multi-line editing, wrapping, drag selection, scrolling, and `Up` / `Down` caret navigation with preferred x tracking.
- Mixed text + icon labels use icon-aware measurement so selection and caret placement stay closer to rendered output.
- The demo runtime handles key repeat for editing keys such as `Backspace`, `Delete`, `Enter`, arrows, `Home`, and `End`.

## Implemented Features

### Theme

- `ThemeMode` (`Light` / `Dark`)
- Primary color (`set_primary_color`)
- Corner radius (`set_corner_radius`)
- Dark mode auto-lifts too-dark primary colors for better contrast

### Layout

- `begin_panel` / `end_panel`
- `begin_card` / `end_card`
- `begin_row` / `end_row`
- `begin_columns` / `end_columns`
- `begin_waterfall` / `end_waterfall`
- `spacer`
- `row_skip` / `row_flex_spacer`
- `set_next_item_span`

### Widgets

- `label`
- `button` (`Primary`, `Secondary`, `Ghost`, optional `text_scale`)
- `tab`
- `slider_float` (drag + right-click numeric edit)
- `input_float` (caret, selection, `Ctrl+A/C/V/X`)
- `input_text` (single-line editable text input)
- `input_readonly`
  - supports `align_right`, `value_font_scale`, `muted`
- `progress`
- `begin_dropdown` / `end_dropdown`
- `begin_scroll_area` / `end_scroll_area`
  - drag, wheel, inertia, overscroll bounce, scrollbar options
- `text_area` (editable, selection, caret, scrolling)
- `text_area_readonly`
- Built-in control motion
  - subtle button/tab press motion
  - restrained input focus ring and glow
  - dropdown reveal with rotating chevron
  - slider/thumb, scrollbar thumb, and progress fill easing

### Output / Integration

- `end_frame()` returns `std::vector<DrawCommand>`
- `take_frame(...)` for moving frame buffers out efficiently
- `text_arena()` returns text storage used by text commands

## Repository Layout

```text
EUI/
|- .github/
|  `- workflows/
|     `- release.yml
|- cmake/
|  `- EUIConfig.cmake.in
|- docs/
|  |- anchor-layout-checklist.md
|  `- backend-abstraction-checklist.md
|- include/
|  `- EUI.h
|  |- stb_truetype.h
|  `- Font Awesome 7 Free-Solid-900.otf
|- examples/
|  |- basic_demo.cpp
|  |- calculator_demo.cpp
|  |- minimal_demo.cpp
|  |- layout_examples_demo.cpp
|  |- multi_scrollbars_demo.cpp
|  `- sidebar_navigation_demo.cpp
|- CMakeLists.txt
|- index.html
|- readme.md
`- readme.zh-CN.md
```

## Build

Recommended generator: `Ninja`.

### 1) Build core only (no GLFW required)

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=OFF
cmake --build build
```

Targets:

- `EUI::eui` (interface)

### 2) Build demos (GLFW + OpenGL)

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON
cmake --build build
```

When OpenGL + GLFW are available, CMake creates:

- `eui_demo` (`examples/basic_demo.cpp`)
- `eui_calculator_demo` (`examples/calculator_demo.cpp`)
- `eui_layout_examples_demo` (`examples/layout_examples_demo.cpp`)
- `eui_minimal_demo` (`examples/minimal_demo.cpp`)
- `eui_multi_scrollbars_demo` (`examples/multi_scrollbars_demo.cpp`)
- `eui_sidebar_navigation_demo` (`examples/sidebar_navigation_demo.cpp`)

Important options:

```bash
-DEUI_BUILD_EXAMPLES=ON|OFF
-DEUI_STRICT_WARNINGS=ON|OFF
-DEUI_FETCH_GLFW_FROM_GIT=ON|OFF
-DEUI_GLFW_GIT_TAG=3.4
-DEUI_ENABLE_STB_TRUETYPE=1|0
```

If network/Git access is restricted:

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON -DEUI_FETCH_GLFW_FROM_GIT=OFF
```

## Release Packaging

GitHub Actions now includes an automated release workflow at [`.github/workflows/release.yml`](.github/workflows/release.yml).
Any pushed tag that starts with `v` triggers a release build on Windows and Linux, then runs CMake packaging through `cpack`.
The same workflow can also be started manually from the GitHub Actions page by providing a `release_tag`, which is useful when you need to rebuild and re-upload assets for an existing release.

Typical release flow:

```bash
git tag v0.1.0
git push origin v0.1.0
```

Produced release assets:

- Windows package: `.zip`
- Linux package: `.tar.gz`

The generated packages install:

- `include/`
- CMake package config files (`EUIConfig.cmake`, `EUIConfigVersion.cmake`, exported targets)
- `readme.md` and `readme.zh-CN.md`
- `docs/`
- `examples/`
- demo executables when example targets are enabled during packaging

## Run Examples

```bash
# core demo
cmake --build build --target eui_demo

# calculator demo
cmake --build build --target eui_calculator_demo

# layout examples demo
cmake --build build --target eui_layout_examples_demo

# minimal demo
cmake --build build --target eui_minimal_demo

# multi scrollbars demo
cmake --build build --target eui_multi_scrollbars_demo

# sidebar navigation demo
cmake --build build --target eui_sidebar_navigation_demo
```

## Minimal Core Usage

```cpp
#include "EUI.h"

eui::Context ui;
eui::InputState input{};

float value = 0.5f;
bool advanced_open = false;

ui.begin_frame(1280.0f, 720.0f, input);
ui.begin_panel("Demo", 20.0f, 20.0f, 640.0f);

ui.begin_row(2, 8.0f);
ui.button("Run", eui::ButtonStyle::Primary);
ui.input_float("Value", value, 0.0f, 1.0f, 2);
ui.end_row();

if (ui.begin_dropdown("Advanced", advanced_open, 80.0f)) {
    ui.progress("Loading", 0.42f);
    ui.end_dropdown();
}

ui.end_panel();

const auto& commands = ui.end_frame();
const auto& text_arena = ui.text_arena();
```

## Common Layout Recipes

### Width Rules (Important)

- Item width is controlled by layout, not widget args.
- `begin_row(n, gap)` gives `n` equal-width columns.
- `set_next_item_span(k)` lets the next control span `k` columns.
- Use `row_flex_spacer(keep_trailing_columns)` to push right-side controls.
- Use `row_skip(k)` to skip fixed columns.

### Sidebar Icon/Text Vertical Alignment

- For left-aligned sidebar buttons, prefix label with `\t` to enable left align with built-in left padding.
- For icon + text, use **two ASCII spaces** between them (for example `u8"\uF015  Dashboard"`).
- EUI will split icon/text and render them separately, which keeps vertical alignment stable.
- See `examples/sidebar_navigation_demo.cpp` for a minimal left-sidebar + page-transition sample.
- See `examples/multi_scrollbars_demo.cpp` for a window that keeps multiple scrollbars visible at once.

```cpp
// Left-aligned nav item with icon + text (stable vertical centering)
ui.button("\t" u8"\uF015  Dashboard", eui::ButtonStyle::Secondary, 34.0f);
```

### 1) Sidebar + Main Content

```cpp
const float pad = 16.0f;
const float sidebar_w = 220.0f;

ui.begin_panel("NAV", pad, pad, sidebar_w);
ui.button("Dashboard");
ui.button("Projects");
ui.button("Settings");
ui.end_panel();

ui.begin_panel("MAIN",
               pad * 2.0f + sidebar_w,
               pad,
               frame_w - sidebar_w - pad * 3.0f);
ui.begin_card("Overview");
ui.label("Main content area");
ui.end_card();
ui.end_panel();
```

### 2) Top Bar (Left / Right)

```cpp
ui.begin_card("TOPBAR", 0.0f, 10.0f);
ui.begin_row(8, 8.0f);
ui.button("Back");
ui.button("Forward");
ui.row_flex_spacer(2, 34.0f); // keep last 2 columns on the right
ui.button("Search");
ui.button("Profile");
ui.end_row();
ui.end_card();
```

### 3) Three-Zone Toolbar (Left / Center / Right)

```cpp
ui.begin_card("TOOLBAR");
ui.begin_row(12, 8.0f);
ui.button("New");
ui.button("Save");
ui.row_skip(2);               // leave center breathing space
ui.label("Build #128", 13.0f, true);
ui.row_flex_spacer(2, 34.0f); // keep right-side actions
ui.button("Run");
ui.button("Deploy");
ui.end_row();
ui.end_card();
```

### 4) Two-Column Settings Page

```cpp
ui.begin_waterfall(2, 10.0f); // equal 2 columns

ui.begin_card("General");
ui.input_float("Gamma", gamma, 0.1f, 4.0f, 2);
ui.end_card();

ui.begin_card("Display");
ui.slider_float("Exposure", exposure, 0.0f, 255.0f, 0);
ui.end_card();

ui.end_waterfall();
```

## Optional Demo Runtime Usage

```cpp
#define EUI_ENABLE_GLFW_OPENGL_BACKEND 1
#include "EUI.h"

int main() {
    eui::demo::AppOptions options{};
    options.width = 960;
    options.height = 710;
    options.title = "EUI Demo";
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 240.0;

    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600; // 100-900, larger = bolder
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";
    options.text_backend = eui::demo::AppOptions::TextBackend::Auto;
    // Optional but recommended on non-Windows: set explicit font file paths.
    // options.text_font_file = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    // options.icon_font_file = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    options.enable_icon_font_fallback = true;

    return eui::demo::run(
        [&](eui::demo::FrameContext frame) {
            auto& ui = frame.ui;
            ui.set_theme_mode(eui::ThemeMode::Dark);

            ui.begin_panel("Demo", 20.0f, 20.0f, 320.0f);
            ui.label("Hello EUI");
            ui.end_panel();

            // request_next_frame() if animation is needed in event-driven mode.
            frame.request_next_frame();
        },
        options
    );
}
```

### Text Backend Notes

- `AppOptions::TextBackend::Auto`
  - On Windows, prefers the STB text path when `stb_truetype` is enabled; otherwise falls back to Win32 text rendering.
  - On non-Windows, uses the STB text path when available.
- `AppOptions::TextBackend::Stb`
  - Uses `stb_truetype` glyph rasterization and atlas caching.
- `AppOptions::TextBackend::Win32`
  - Windows-only text renderer based on GDI/WGL font APIs.

For best icon coverage, keep `enable_icon_font_fallback = true` and ship an explicit icon font file.

## Notes

- `index.html` is a visual/prototype reference, not part of C++ build output.
- Keep source files in UTF-8 to avoid C4819/garbled literal issues on Windows toolchains.
