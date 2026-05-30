# EUI-NEO

<p align="center">
  <img src="assets/icon.svg" width="104" alt="EUI icon">
</p>

<p align="center">
  <a href="https://github.com/sudoevolve/EUI-NEO/actions/workflows/release.yml"><img alt="Release Build" src="https://github.com/sudoevolve/EUI-NEO/actions/workflows/release.yml/badge.svg"></a>
  <a href="https://github.com/sudoevolve/EUI-NEO/releases"><img alt="Release" src="https://img.shields.io/github/v/release/sudoevolve/EUI-NEO?include_prereleases&sort=semver"></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/badge/license-Apache%202.0-blue"></a>
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white">
  <img alt="CMake 3.14+" src="https://img.shields.io/badge/CMake-3.14%2B-064F8C?logo=cmake&logoColor=white">
  <img alt="OpenGL" src="https://img.shields.io/badge/OpenGL-rendering-5586A4?logo=opengl&logoColor=white">
  <img alt="GLFW" src="https://img.shields.io/badge/GLFW-windowing-111111">
  <a href="https://github.com/sudoevolve/EUI-NEO/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/sudoevolve/EUI-NEO?style=flat"></a>
</p>

<p align="center">
  <a href="README.zh-CN.md">简体中文</a>
  ·
  <a href="https://sudoevolve.github.io/pages/eui-neo.html">Website</a>
</p>

EUI-NEO is a cross-platform, high-performance, low-overhead C++17 UI framework built on OpenGL and GLFW, ready to use out of the box.

## Preview

|  |  |
| --- | --- |
| ![preview 1](docs/pic/1.jpg) | ![preview 2](docs/pic/2.jpg) |
| ![preview 3](docs/pic/3.jpg) | ![preview 4](docs/pic/4.jpg) |
| ![example 1](docs/pic/示例1.jpg) | ![example 2](docs/pic/示例2.jpg) |

## Quick Start

Requirements:

- CMake 3.14+
- A C++17 compiler
- OpenGL
- Platform OpenGL/windowing development files. Linux builds also need X11 and libcurl development packages.

Build-time sources for GLFW, glad, tray, FreeType, HarfBuzz, libpng, and zlib are vendored under `3rd/`. The default dependency mode is `auto`: CMake uses the local `3rd/` sources when they are present, and fetches only missing dependencies from pinned upstream URLs. Use `-DEUI_DEPS_MODE=bundled` for strict offline builds, or `-DEUI_DEPS_MODE=fetch` to force online dependency fetches. HarfBuzz shaping is enabled by default and can be disabled with `-DEUI_ENABLE_HARFBUZZ=OFF`.

macOS / Linux example:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/gallery
```

Windows / PowerShell example:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\gallery.exe
```

Linux package hint:

```sh
sudo apt-get install -y ninja-build libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libcurl4-openssl-dev
```

Top-level builds create one executable for each `app/*.cpp` page source, such as `gallery` and `demo`. After build, `assets/` is copied next to the executable automatically.

Tagged releases (`v*`) build Windows, Linux, and macOS packages through GitHub Actions and upload only runtime packages as release assets.

## Use In Your Project

EUI-NEO also exposes a CMake library target for external C++ projects:

```cmake
add_subdirectory(external/EUI-NEO)

add_executable(my_app external/EUI-NEO/main.cpp app.cpp)
eui_neo_configure_app(my_app)
```

For the easiest path, download EUI-NEO into `external/EUI-NEO`, add the snippet above, and implement `app::dslAppConfig()` plus `app::compose()` in `app.cpp`. EUI-NEO will own the GLFW window, event loop, OpenGL rendering, and asset copying. When EUI-NEO is added as a subdirectory, bundled examples are disabled by default. Set `-DEUI_BUILD_APPS=ON` to build `gallery`, `demo`, and the other sample apps. See [Integration Guide](docs/集成指南.md) for the beginner setup, `FetchContent`, and embedded GLFW loop examples.

## Project Layout

```text
app/          Page entry points and gallery examples
assets/       Runtime assets: fonts, PNG, SVG, and icons
components/   Reusable UI components built on top of the DSL
core/         DSL, Runtime, primitives, text, image, network, and platform code
docs/         Implementation notes and API documentation
3rd/          Vendored third-party build sources and single-file dependencies
```

## Docs

- [DSL Design And Current Implementation](docs/DSL.md)
- [Components](docs/组件.md)
- [Primitives And Text](docs/基础图元文本图元.md)
- [Layout](docs/布局.md)
- [Events](docs/事件.md)
- [Animation](docs/动画.md)
- [Async](docs/异步.md)
- [Rendering Pipeline](docs/渲染流程.md)
- [Images](docs/图片.md)
- [Network](docs/网络.md)
- [Platform Capabilities](docs/平台能力.md)
- [Window And Pages](docs/窗口页面.md)
- [Integration Guide](docs/集成指南.md)
- [Development And Release](docs/开发与发布.md)
- [Review Checklist](docs/Review清单.md)

## Current Components

`components/components.h` exports the current component layer:

- Basic wrappers: `panel`, `text` / `label`, `image`, `theme`
- Controls: `button`, `checkbox`, `radio`, `toggleSwitch`, `progress`, `slider`, `input`, `segmented`, `stepper`, `tabs`, `scroll`, `scrollView`
- Popups and feedback: `dialog`, `toast`, `contextMenu`, `dropdown`
- Pickers: `datepicker`, `timepicker`, `colorpicker`
- Data display: `dataTable` (`datatable` alias), `carousel`
- Charts: `linechart` (`lineChart` alias), `barchart` (`barChart` alias), `piechart` (`pieChart` alias)
- Input areas: `mouseArea`

Components only compose DSL trees. They do not own OpenGL primitives directly. Business state stays in the page or application layer: pass the current value into the builder, then write the next value back from callbacks.

## License

EUI-NEO's original source code is licensed under the Apache License 2.0. Third-party code under `3rd/`, optional build-time dependencies fetched by CMake, and bundled fonts or icon fonts under `assets/` follow their respective upstream licenses and copyright notices.

## Star History

<a href="https://www.star-history.com/#sudoevolve/EUI-NEO&Date">
  <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=sudoevolve/EUI-NEO&type=Date">
</a>
