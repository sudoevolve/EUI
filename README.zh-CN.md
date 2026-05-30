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
  <a href="README.md">English</a>
  ·
  <a href="https://sudoevolve.github.io/pages/eui-neo.html">官网</a>
</p>

EUI-NEO 是一个基于 C++17、OpenGL 和 GLFW 的跨平台高性能轻量级 UI 框架，低占用、开箱即用。

## 预览

|  |  |
| --- | --- |
| ![preview 1](docs/pic/1.jpg) | ![preview 2](docs/pic/2.jpg) |
| ![preview 3](docs/pic/3.jpg) | ![preview 4](docs/pic/4.jpg) |
| ![示例 1](docs/pic/示例1.jpg) | ![示例 2](docs/pic/示例2.jpg) |

## 快速开始

环境要求：

- CMake 3.14+
- 支持 C++17 的编译器
- OpenGL
- 平台 OpenGL/windowing 开发文件。Linux 构建还需要 X11 和 libcurl 开发包。

GLFW、glad、tray、FreeType、HarfBuzz、libpng、zlib 等构建期第三方源码已内置在 `3rd/` 下。默认依赖模式是 `auto`：本地 `3rd/` 源码存在时直接使用，缺失时才从固定上游地址联网拉取。需要严格离线构建时，可配置 `-DEUI_DEPS_MODE=bundled`；需要强制联网拉取时，可配置 `-DEUI_DEPS_MODE=fetch`。HarfBuzz shaping 默认启用，可通过 `-DEUI_ENABLE_HARFBUZZ=OFF` 关闭。

macOS / Linux 示例：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/gallery
```

Windows / PowerShell 示例：

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\gallery.exe
```

Linux 依赖提示：

```sh
sudo apt-get install -y ninja-build libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libcurl4-openssl-dev
```

顶层构建会为 `app/*.cpp` 下的每个页面源文件生成一个可执行程序，例如 `gallery` 和 `demo`。构建后会自动把 `assets/` 复制到可执行文件目录。

推送 `v*` tag 后，GitHub Actions 会构建 Windows、Linux、macOS 包，并且 release assets 只上传运行包。

## 接入到你的项目

EUI-NEO 也提供给外部 C++ 项目使用的 CMake 库 target：

```cmake
add_subdirectory(external/EUI-NEO)

add_executable(my_app external/EUI-NEO/main.cpp app.cpp)
eui_neo_configure_app(my_app)
```

最简单的方式是把 EUI-NEO 下载到 `external/EUI-NEO`，加入上面的 CMake 片段，然后在 `app.cpp` 里实现 `app::dslAppConfig()` 和 `app::compose()`。EUI-NEO 会接管 GLFW 窗口、事件循环、OpenGL 渲染和资源复制。EUI-NEO 作为子目录接入时，默认不会构建仓库自带示例。需要构建 `gallery`、`demo` 等示例时，配置 `-DEUI_BUILD_APPS=ON`。小白接入、`FetchContent` 和嵌入已有 GLFW 主循环的写法见 [集成指南](docs/集成指南.md)。

## 目录结构

```text
app/          页面入口和 gallery 示例
assets/       字体、PNG、SVG 和图标等运行资源
components/   基于 DSL 封装的通用组件
core/         DSL、Runtime、图元、文本、图片、网络和平台能力
docs/         项目实现文档
3rd/          内置第三方构建源码和单文件依赖
```

## Docs

- [DSL 设计与当前实现](docs/DSL.md)
- [组件](docs/组件.md)
- [基础图元与文本图元](docs/基础图元文本图元.md)
- [布局](docs/布局.md)
- [事件](docs/事件.md)
- [动画](docs/动画.md)
- [异步](docs/异步.md)
- [渲染流程](docs/渲染流程.md)
- [图片](docs/图片.md)
- [网络](docs/网络.md)
- [平台能力](docs/平台能力.md)
- [窗口页面](docs/窗口页面.md)
- [集成指南](docs/集成指南.md)
- [开发与发布](docs/开发与发布.md)
- [Review 清单](docs/Review清单.md)

## 当前组件

`components/components.h` 聚合导出当前组件层：

- 基础包装：`panel`、`text` / `label`、`image`、`theme`
- 控件：`button`、`checkbox`、`radio`、`toggleSwitch`、`progress`、`slider`、`input`、`segmented`、`stepper`、`tabs`、`scroll`、`scrollView`
- 弹层和反馈：`dialog`、`toast`、`contextMenu`、`dropdown`
- 选择器：`datepicker`、`timepicker`、`colorpicker`
- 数据展示：`dataTable`（`datatable` 兼容别名）、`carousel`
- 图表：`linechart`（`lineChart` 兼容别名）、`barchart`（`barChart` 兼容别名）、`piechart`（`pieChart` 兼容别名）
- 输入热区：`mouseArea`

组件只组合 DSL 树，不直接持有 OpenGL primitive。业务状态仍然放在页面或业务层，通过 builder 参数传入当前值，再从回调写回 next value。

## 许可

EUI-NEO 的原创源码采用 Apache License 2.0。`3rd/` 下的第三方代码、CMake 可选联网拉取的构建期依赖，以及 `assets/` 下随项目分发的字体和图标字体，遵循各自上游许可证和版权声明。

## Star History

<a href="https://www.star-history.com/#sudoevolve/EUI-NEO&Date">
  <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=sudoevolve/EUI-NEO&type=Date">
</a>
