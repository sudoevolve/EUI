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
- Vulkan SDK 可选。默认渲染后端选择是 `auto`：EUI Vulkan 后端可用且检测到 SDK 时使用 Vulkan，否则回退 OpenGL。
- 平台 OpenGL/windowing 开发文件。Linux 构建还需要 X11 和 libcurl 开发包。

GLFW、glad、tray、FreeType、HarfBuzz、libpng、zlib 等构建期第三方源码已内置在 `3rd/` 下。默认依赖模式是 `auto`：本地 `3rd/` 源码存在时直接使用，缺失时才从固定上游地址联网拉取。需要严格离线构建时，可配置 `-DEUI_DEPS_MODE=bundled`；需要强制联网拉取时，可配置 `-DEUI_DEPS_MODE=fetch`。HarfBuzz shaping 默认启用，可通过 `-DEUI_ENABLE_HARFBUZZ=OFF` 关闭。

默认窗口后端是 GLFW。SDL2 是可选后端，不放进 `3rd/`：构建 SDL2 后端时，要么使用系统 SDL2 包，要么显式选择 fetch 下载 SDL2：

```sh
cmake --preset sdl2-release
cmake --build --preset sdl2-release
cmake --preset sdl2-fetch-release
cmake --build --preset sdl2-fetch-release
```

macOS / Linux 示例：

```sh
cmake --preset glfw-release
cmake --build --preset glfw-release
./build/glfw-release/gallery
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
# -DEUI_WINDOW_BACKEND=sdl2 可选安装：
sudo apt-get install -y libsdl2-dev
```

顶层构建会为 `examples/*.cpp` 下的每个页面源文件生成一个可执行程序，例如 `gallery` 和 `demo`。构建后会自动把 `assets/` 复制到可执行文件目录。

推送 `v*` tag 后，GitHub Actions 会构建 Windows、Linux、macOS 包，并且 release assets 只上传运行包。

## 接入到你的项目

推荐按下面三种方式选择。普通应用先走公共 facade 头文件，只有已有窗口循环时再直接接静态库。

### 1. 公共头文件方式

这是最简单的完整 app 接入方式。你的应用源码只需要包含公共入口：

```cpp
#include "eui_neo.h"
```

把 EUI-NEO 作为子目录加入，并使用框架提供的 app main：

```cmake
add_subdirectory(external/EUI-NEO)

add_executable(my_app external/EUI-NEO/core/app/glfw_app_main.cpp app.cpp)
eui_neo_configure_app(my_app)
```

然后在 `app.cpp` 里实现 `app::dslAppConfig()` 和 `app::compose()`。EUI-NEO 会接管窗口、事件循环、OpenGL 渲染和资源复制。这里是“单个公共 facade 头文件入口”，不是纯 header-only 库。

默认窗口后端是 GLFW。需要 SDL2 时，配置 `-DEUI_WINDOW_BACKEND=sdl2`，并把 app main 换成 `external/EUI-NEO/core/app/sdl2_app_main.cpp`。

### 2. 静态库方式

如果你的项目已经有自己的 main、窗口、OpenGL context 或事件循环，可以直接链接导出的静态库 target：

```cmake
add_subdirectory(external/EUI-NEO)

add_executable(my_app main.cpp app.cpp)
target_link_libraries(my_app PRIVATE eui::neo)
eui_neo_copy_assets(my_app)
```

普通 UI 代码仍然优先 `#include "eui_neo.h"`；只有接入边界需要碰底层 runtime / platform 头文件。

### 3. 直接在 `examples/` 开发

快速实验或新增内置示例时，直接创建 `examples/my_app.cpp`，包含 `eui_neo.h`，实现 `app::dslAppConfig()` 和 `app::compose()`。顶层构建会自动为每个 `examples/*.cpp` 生成一个可执行程序。

EUI-NEO 作为子目录接入时，默认不会构建仓库自带示例。需要构建 `gallery`、`demo`、`serial_tool` 等示例时，配置 `-DEUI_BUILD_APPS=ON`。完整 CMake 片段、`FetchContent` 和嵌入已有 GLFW 主循环的写法见 [集成指南](docs/集成指南.md)。

## 目录结构

```text
assets/       字体、PNG、SVG 和图标等运行资源
components/   基于 DSL 封装的通用组件
core/         DSL、Runtime、图元、文本、图片、网络和平台能力
docs/         项目实现文档
examples/     独立 gallery 和 demo 应用源码
include/      公共 include 路径：eui_neo.h 和 eui/* facade 头文件
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
