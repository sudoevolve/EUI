# EUI

[English README](readme.md)

EUI 是一个以即时模式核心为基础、以现代 OpenGL 运行时为主线的 C++ GUI 框架。核心层只负责产出绘制命令和文本数据，不强绑某个窗口库；如果你想直接运行桌面程序，仓库也内置了基于 GLFW 或 SDL2 的应用运行时。

当前公开路线就是 modern GL 优先。GLFW 和 SDL2 共用同一条渲染主线，`EUI_OPENGL_ES` 作为 OpenGL ES 兼容分支存在，`Vulkan` 目前仍未实现。

## 预览

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

## EUI 现在能做什么

- 通过 `eui::Context` 产出即时模式 UI 的绘制命令和文本缓冲。
- 通过 `eui::quick::UI` 使用更高层的 builder API 组织控件、布局、图元和效果。
- 通过 `eui::app` 直接得到窗口创建、输入处理、DPI、帧调度和剪贴板能力。
- 通过现代 OpenGL 路径运行桌面 UI，GLFW 和 SDL2 共用同一套渲染器。
- 通过 TrueType 优先的文本链路获得更清晰的文字与图标渲染，Win32 和内置位图只做兜底。

## 内置组件

`eui::quick::UI` 当前公开的 builder 主要包括：

- 容器表面：`panel`、`card`
- 文本与展示：`label`、`text`、`icon`、`metric`、`readonly`
- 交互与状态：`button`、`tab`、`progress`
- 输入与编辑：`slider`、`input`、`text_area`、`text_area_readonly`、`dropdown`
- 容器与滚动：`scroll_area`
- 图元：`shape`、`image`
- 布局辅助：`row`、`anchor`、`scope`、`stack`、`clip`

补充说明：

- `input` 是单行可编辑输入框。
- `text_area` 是多行可编辑文本区。
- `image` 目前是带 fit 模式和变换能力的占位图元，不是完整的图片解码加载系统。

## 布局系统

EUI 的布局是小而直接的，不靠庞大的保留式布局树。

- Dock 布局：`dock_top`、`dock_bottom`、`dock_left`、`dock_right`
- 矩形切分：`split_h`、`split_v`、`split_h_ratio`、`split_v_ratio`
- 弹性行布局：`row().items({ fixed(...), flex(...), content(...) })`
- 锚点布局：`anchor()`，支持边、中心、百分比，以及 `to_last()`
- 放置辅助：`fill_parent`、`after_last`、`below_last`
- 区域辅助：`scope`、`stack`、`clip`
- 矩形访问：`content()`、`content_rect()`、`last_rect()`

这套组合足够覆盖常见的表单页、侧边栏、工具栏、仪表板、浮层徽标和多层叠放界面。

## 图元、效果与绘制能力

除了内置控件，EUI 本身也暴露了可直接使用的图元层。

- 纯色填充
- 线性渐变
- 径向渐变
- 描边
- 圆角
- 阴影
- 模糊与背景模糊
- 透明度控制
- 裁剪矩形
- 文本与图标绘制
- 带 `fill`、`contain`、`cover`、`stretch`、`center` 适配方式的占位图像表面

2D 变换接口：

- `translate`
- `scale`
- `rotate`
- `origin`
- `origin_percent`
- `origin_center`

3D 变换接口：

- `translate_3d`
- `scale_3d`
- `rotate_x`
- `rotate_y`
- `rotate_z`
- `origin_3d`
- `perspective`

`graphics_showcase_demo` 里已经实际用了模糊、阴影、渐变和 3D 倾斜卡片这些能力。

## 动画与动态效果

当前控件已经自带一套克制的状态动效：

- hover
- press
- focus
- dropdown 展开
- progress 过渡
- scrollbar 和 slider 的反馈

另外还提供了更底层的 `eui::animation` 能力：

- 三次贝塞尔缓动
- 预设：`linear`、`ease`、`ease_in`、`ease_out`、`ease_in_out`、`spring_soft`
- `TimelineClip`
- 可动画属性类型：透明度、颜色、位置、尺寸、圆角、模糊、阴影、2D 变换、3D 变换、自定义标量

整体思路不是把渲染循环一直跑满，而是在事件驱动渲染下只为必要的交互和动效继续请求下一帧。

## 文本与编辑系统

文本现在已经是主能力之一。

- `stb_truetype` 是主文本路径。
- 图标字体回退内置可用。
- Windows 桌面下可在必要时回退到 Win32 文本。
- 内置位图字形只作为最后兜底。
- 文本测量会尽量跟随当前实际渲染路径，保证光标、选区、换行和命中测试更接近最终显示。

当前已经支持：

- 单行编辑输入
- 多行文本编辑区
- 光标移动
- 拖拽选区
- 剪贴板快捷键
- 单行输入随光标自动横向滚动
- 多行自动换行与滚动
- 带 preferred x 的上下方向键导航

## 主题能力

主题控制保持在一个很实用的最小集合：

- `set_theme_mode(ThemeMode::Light | ThemeMode::Dark)`
- `set_primary_color(...)`
- `set_corner_radius(...)`

暗色模式下还会自动抬亮过暗主色，避免整体对比度太差。

## 运行时与后端支持

编译期后端选择现在可以直接写在示例代码里：

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
// 或者：#define EUI_PLATFORM_SDL2
// 可选：#define EUI_OPENGL_ES
#include "EUI.h"
```

当前状态：

- `EUI_BACKEND_OPENGL`：已实现，也是推荐公开路线
- `EUI_PLATFORM_GLFW`：已实现
- `EUI_PLATFORM_SDL2`：已实现
- `EUI_OPENGL_ES`：作为 OpenGL 后端下的 ES 兼容分支可用
- `EUI_BACKEND_VULKAN`：尚未实现

`eui::app::AppOptions` 目前支持：

- 窗口宽高与标题
- vsync
- 事件驱动渲染或持续渲染
- `max_fps`
- `WindowBackend::Auto | GLFW | SDL2`
- 文本字体 family、weight、file
- 图标字体 family、file
- `TextBackend::Auto | Stb | Win32`

## 最小应用示例

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

## 示例程序

仓库当前自带这些示例：

- `examples/basic_widgets_demo.cpp`
- `examples/anchor_layout_demo.cpp`
- `examples/sidebar_navigation_demo.cpp`
- `examples/calculator_demo.cpp`
- `examples/graphics_showcase_demo.cpp`

它们分别覆盖基础控件、锚点布局、侧边栏组合、可编辑表单和图形效果展示。

## 构建

只构建核心：

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=OFF
cmake --build build
```

构建示例：

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON -DEUI_WINDOW_BACKEND=AUTO
cmake --build build
```

如果本机没有可用的窗口后端，示例 target 会被跳过。日常开发里最简单的方式还是直接在源码里写后端宏，CMake 只负责把本地可用的后端接起来。

