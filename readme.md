# EUI

[Chinese README](readme.zh-CN.md)

EUI is a header-only C++ GUI framework built around a practical immediate-mode core and a modern OpenGL runtime. The core layer emits draw commands and text data without hard-wiring you to a specific window toolkit, while the built-in app runtime gives you a ready-to-run desktop path with GLFW or SDL2.

The current public route is modern GL first. GLFW and SDL2 share the same renderer path, `EUI_OPENGL_ES` is supported as an OpenGL ES compatibility branch, and Vulkan is still not implemented.

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

## What EUI Provides

- A core immediate-mode UI context, `eui::Context`, that produces draw commands and a text arena.
- A higher-level builder API, `eui::quick::UI`, for composing widgets, layout, primitives, and effects.
- An optional app runtime, `eui::app`, with window creation, input, DPI handling, frame scheduling, and clipboard integration.
- A modern OpenGL renderer path used by both GLFW and SDL2 examples.
- A TrueType-first text pipeline with icon font fallback, Windows fallback text, and built-in bitmap fallback only as a last resort.

## Built-In Components

Public builders currently available through `eui::quick::UI`:

- Surfaces: `panel`, `card`
- Text and display: `label`, `text`, `icon`, `metric`, `readonly`
- Actions and state: `button`, `tab`, `progress`
- Input and editing: `slider`, `input`, `text_area`, `text_area_readonly`, `dropdown`
- Containers: `scroll_area`
- Graphics primitives: `shape`, `image`
- Layout helpers: `row`, `anchor`, `scope`, `stack`, `clip`

Notes:

- `input` is the single-line editable text input.
- `text_area` is the multiline editable control.
- `image` currently acts as a styled image placeholder surface with fit policies and transforms. It is not a full image decoding pipeline.

## Layout System

EUI ships with layout tools that are small, explicit, and easy to combine:

- Docking: `dock_top`, `dock_bottom`, `dock_left`, `dock_right`
- Rect splitting: `split_h`, `split_v`, `split_h_ratio`, `split_v_ratio`
- Flex rows: `row().items({ fixed(...), flex(...), content(...) })`
- Anchoring: `anchor()` with edges, centers, percentages, and `to_last()`
- Placement helpers: `fill_parent`, `after_last`, `below_last`
- Region helpers: `scope`, `stack`, `clip`
- Rect access: `content()`, `content_rect()`, `last_rect()`

This makes it practical to build dashboards, forms, sidebars, editors, floating badges, and layered composition without introducing a separate retained layout tree.

## Graphics, Primitives, and Effects

The primitive layer is designed to be useful even outside stock widgets.

- Solid fills
- Linear gradients
- Radial gradients
- Strokes
- Corner radius
- Shadows
- Blur and backdrop blur
- Opacity control
- Clip rects
- Text and icon painting
- Placeholder image surfaces with `fill`, `contain`, `cover`, `stretch`, and `center`

2D transform helpers:

- `translate`
- `scale`
- `rotate`
- `origin`
- `origin_percent`
- `origin_center`

3D transform helpers:

- `translate_3d`
- `scale_3d`
- `rotate_x`
- `rotate_y`
- `rotate_z`
- `origin_3d`
- `perspective`

The graphics showcase example demonstrates blur, shadow, gradient composition, and tilt-style 3D cards on the same public API.

## Motion and Animation

EUI already uses built-in motion for common control states:

- hover
- press
- focus
- dropdown reveal
- progress transitions
- scrollbar and slider feedback

There is also a lower-level animation layer under `eui::animation`:

- cubic bezier easing
- presets: `linear`, `ease`, `ease_in`, `ease_out`, `ease_in_out`, `spring_soft`
- `TimelineClip`
- animatable property kinds for opacity, color, position, size, radius, blur, shadow, 2D transform, 3D transform, and custom scalar values

The intended model is restrained motion that still works well with event-driven rendering, rather than a permanently hot render loop.

## Text and Editing

Text is one of the main framework paths now.

- `stb_truetype` is the primary text renderer.
- Icon font fallback is built in.
- On Windows desktop, Win32 text is available as a fallback when needed.
- Built-in bitmap glyphs are the final fallback only.
- Text measurement is aligned with the active rendering path so caret placement, selection, wrapping, and hit testing stay closer to what is actually drawn.

Editing support already includes:

- single-line editable input
- multiline text area
- caret movement
- drag selection
- clipboard shortcuts
- horizontal scroll-to-caret for single-line input
- wrapped multiline editing with scrolling
- `Up` and `Down` caret navigation with preferred x tracking

## Themes

Core theme controls are intentionally simple:

- `set_theme_mode(ThemeMode::Light | ThemeMode::Dark)`
- `set_primary_color(...)`
- `set_corner_radius(...)`

The dark theme path also lifts overly dark primary colors to keep contrast usable.

## Runtime and Backend Support

Compile-time runtime selection is designed to stay in code:

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
// or: #define EUI_PLATFORM_SDL2
// optional: #define EUI_OPENGL_ES
#include "EUI.h"
```

Current status:

- `EUI_BACKEND_OPENGL`: implemented, recommended public route
- `EUI_PLATFORM_GLFW`: implemented
- `EUI_PLATFORM_SDL2`: implemented
- `EUI_OPENGL_ES`: supported as the ES-compatible branch of the OpenGL backend
- `EUI_BACKEND_VULKAN`: not implemented yet

Runtime options in `eui::app::AppOptions` include:

- window width, height, title
- vsync
- event-driven vs continuous render
- `max_fps`
- `WindowBackend::Auto | GLFW | SDL2`
- text font family, weight, file
- icon font family, file
- `TextBackend::Auto | Stb | Win32`

## Minimal App Example

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

int main() {
    eui::app::AppOptions options{};
    options.width = 960;
    options.height = 720;
    options.title = "EUI Demo";
    options.vsync = true;
    options.continuous_render = false;
    options.text_backend = eui::app::AppOptions::TextBackend::Auto;
    options.enable_icon_font_fallback = true;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(eui::rgb(0x3B82F6));

            eui::quick::UI ui(ctx);
            ui.panel("Workspace")
                .in(eui::Rect{24.0f, 24.0f, 520.0f, 320.0f})
                .padding(16.0f)
                .begin([&](auto&) {
                    ui.label("EUI").font(22.0f).draw();
                    ui.label("Immediate-mode C++ GUI with a modern GL runtime.")
                        .font(14.0f)
                        .muted()
                        .draw();
                    ui.spacer(10.0f);
                    ui.progress("Loading", 0.72f).height(10.0f).draw();
                    ui.spacer(12.0f);
                    ui.button("Run").primary().height(38.0f).draw();
                });
        },
        options
    );
}
```

## Examples

The repository currently ships these examples:

- `examples/basic_widgets_demo.cpp`
- `examples/anchor_layout_demo.cpp`
- `examples/sidebar_navigation_demo.cpp`
- `examples/calculator_demo.cpp`
- `examples/graphics_showcase_demo.cpp`

They cover stock widgets, anchor-based layout, sidebar composition, editable forms, and graphics/effects.

## Build

Core only:

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=OFF
cmake --build build
```

Examples:

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON -DEUI_WINDOW_BACKEND=AUTO
cmake --build build
```

If no enabled local window backend is found, example targets are skipped. For local development, the simplest route is to keep the backend macros in your example source and let CMake wire whichever backend is available.
