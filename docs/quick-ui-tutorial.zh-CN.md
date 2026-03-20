# EUI Quick 声明式 UI 教程

最后更新：2026-03-20

这份文档是给第一次上手 EUI 的开发者写的。

目标只有一个：让你看完之后，知道下面这些东西该怎么写，而且能自己扩展。

- 怎么起一个最小可运行界面
- 怎么写布局
- 怎么画矩形、文字、图标、自定义图元
- 怎么控制各种数值
- 怎么处理按钮点击和自绘区域点击
- 怎么写最常用的动画
- 怎么把代码整理成页面和组件

当前仓库推荐先对照两份示例一起读：

- `examples/minimal_quick_demo.cpp`
- `examples/EUI_gallery.cpp`

---

## 1. 先理解这套 API 的本质

EUI 现在这套 Quick 写法，不是 QML 那种“保留一棵长期存在的对象树”。

它更接近：

1. 你每一帧都重新描述一次界面
2. 状态存在普通 C++ 变量里
3. 布局 builder 负责先算出一组 `Rect`
4. 控件和图元只是“画进某个 `Rect`”
5. 动画本质是“给一个 key 和目标值，运行时帮你平滑过渡”

所以你写 UI 时，脑子里要分三层：

1. `状态`
   例子：`bool open`、`float radius`、`int selected_tab`
2. `布局`
   例子：左边 300 像素，右边占剩余；卡片里分 6 行；中间做 3x2 网格
3. `绘制 / 交互`
   例子：按钮、滑杆、矩形、文字、图标、点击检测、动画值

这一点非常重要，因为它决定了你怎么组织代码。

---

## 2. 最小可运行入口

如果你要新建一个例子，最简单的入口长这样：

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>

using eui::Rect;
using eui::quick::UI;

struct AppState {
    float radius{22.0f};
};

int main() {
    AppState state{};

    eui::app::AppOptions options{};
    options.title = "My First EUI App";
    options.width = 1280;
    options.height = 720;
    options.vsync = true;
    options.continuous_render = true;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const float scale = std::max(1.0f, metrics.dpi_scale);

            UI ui(ctx);
            const Rect root{
                20.0f * scale,
                20.0f * scale,
                static_cast<float>(metrics.framebuffer_w) - 40.0f * scale,
                static_cast<float>(metrics.framebuffer_h) - 40.0f * scale,
            };

            ui.panel("Hello")
                .in(root)
                .radius(24.0f * scale)
                .begin([&](auto& panel) {
                    ui.text("First screen")
                        .in(panel.content())
                        .center()
                        .font(22.0f * scale)
                        .draw();
                });
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
```

这里有几个关键点：

- `#define EUI_BACKEND_OPENGL`
  现在公开图形主路线就是 OpenGL。
- `#define EUI_PLATFORM_GLFW`
  你也可以换成 `EUI_PLATFORM_SDL2`。
- `eui::app::run(...)`
  每帧都会进这个 lambda。
- `AppState`
  你的 UI 状态就放在普通 C++ 结构体里。
- `UI ui(ctx);`
  这一帧的 Quick builder 入口。

如果你只是静态界面，不需要持续动画，可以把：

```cpp
options.continuous_render = false;
```

然后在需要动画或变化时调用：

```cpp
frame.request_next_frame();
```

如果你就是做大量实时动画，最省事的方式还是：

```cpp
options.continuous_render = true;
```

---

## 3. 先学会布局，否则后面全会重叠

这是现在最容易犯错的地方。

### 3.1 新布局 API 的核心思想

`row()`、`column()`、`grid()`、`zstack()` 不是“自动把后面的控件一个个排好”。

它们是：

1. 先根据轨道规则算出一组槽位 `Rect`
2. 你再用 `next()` 把这些槽位拿出来
3. 最后把控件或图元放进去

也就是说，**先分槽，再放内容**。

如果你只写：

```cpp
ui.column(rect).begin([&](auto&) {
    ui.button("A").draw();
    ui.button("B").draw();
});
```

这不是你想要的“声明式列布局”，因为你根本没有显式消费槽位。

推荐写法一定是：

```cpp
ui.column(rect)
    .tracks({
        eui::quick::px(36.0f),
        eui::quick::px(36.0f),
        eui::quick::fr(),
    })
    .gap(8.0f)
    .begin([&](auto& rows) {
        ui.button("A").in(rows.next()).draw();
        ui.button("B").in(rows.next()).draw();
        ui.text("Bottom").in(rows.next()).draw();
    });
```

---

## 4. 三个最常用的长度单位

声明式布局最常见的是这三个：

```cpp
eui::quick::px(120.0f)   // 固定像素
eui::quick::fr(1.0f)     // 剩余空间按权重分配
eui::quick::fit()        // 内容型轨道，当前主要用于兼容 Content 语义
```

例子：

```cpp
ui.row(body)
    .tracks({
        eui::quick::px(320.0f),
        eui::quick::fr(),
    })
    .gap(18.0f)
    .begin([&](auto& cols) {
        const Rect left = cols.next();
        const Rect right = cols.next();
    });
```

含义很简单：

- 左边固定 320 像素
- 右边吃掉剩余宽度

---

## 5. `view / row / column / grid / zstack` 到底怎么选

### 5.1 `ui.view(rect)`

`view(rect)` 本质是一个通用容器。

它本身不决定布局方式，但你可以在它后面接：

- `.row()`
- `.column()`
- `.grid()`
- `.zstack()`

常见写法：

```cpp
ui.view(root.content())
    .padding(18.0f * scale)
    .column()
    .tracks({eui::quick::px(64.0f * scale), eui::quick::fr()})
    .gap(16.0f * scale)
    .begin([&](auto& rows) {
        const Rect header = rows.next();
        const Rect body = rows.next();
    });
```

适合场景：

- 你已经有一个根 `Rect`
- 想先做内边距
- 再决定内部是行、列、网格还是叠层

### 5.2 `ui.row(rect)`

横向分栏。

```cpp
ui.row(body)
    .tracks({
        eui::quick::px(280.0f),
        eui::quick::fr(),
        eui::quick::px(240.0f),
    })
    .gap(16.0f)
    .begin([&](auto& cols) {
        const Rect left = cols.next();
        const Rect center = cols.next();
        const Rect right = cols.next();
    });
```

### 5.3 `ui.column(rect)`

纵向分行。

```cpp
ui.column(panel.content())
    .tracks({
        eui::quick::px(40.0f),
        eui::quick::px(40.0f),
        eui::quick::fr(),
    })
    .gap(10.0f)
    .begin([&](auto& rows) {
        ui.slider("Radius", state.radius).in(rows.next()).draw();
        ui.slider("Blur", state.blur).in(rows.next()).draw();
        ui.text_area_readonly("Code", code).in(rows.next()).draw();
    });
```

### 5.4 `ui.grid(rect)`

规则网格。

```cpp
ui.grid(content)
    .repeat_rows(2, eui::quick::fr())
    .repeat_columns(3, eui::quick::fr())
    .gap(12.0f)
    .begin([&](auto& grid) {
        for (int i = 0; i < 6; ++i) {
            ui.card("Item").in(grid.next()).begin([&](auto&) {});
        }
    });
```

适合：

- 色板
- 图标库
- 统计卡片矩阵
- 规则图库

### 5.5 `ui.zstack(rect)`

叠层布局，所有层共享同一个区域。

```cpp
ui.zstack(stage)
    .layers(3)
    .begin([&](auto& layers) {
        const Rect back = layers.next();
        const Rect middle = layers.next();
        const Rect front = layers.next();
    });
```

适合：

- 背景 + 发光 + 主卡片
- 模糊层 + 内容层
- 底图 + 选中框 + 文字

---

## 6. 最重要的布局习惯：所有可交互控件都尽量 `.in(slot)`

现在 EUI 的这些 builder 都支持直接放进指定矩形：

- `button`
- `tab`
- `slider`
- `input`
- `input_float`
- `readonly`
- `progress`
- `text_area`
- `text_area_readonly`
- `card`
- `panel`
- `shape`
- `text`
- `glyph`

推荐写法：

```cpp
ui.button("Confirm").in(rows.next()).primary().height(36.0f * scale).draw();
ui.slider("Radius", state.radius).in(rows.next()).range(8.0f, 28.0f).draw();
ui.input("Search", state.search).in(rows.next()).draw();
```

不要再依赖“当前布局栈可能会自动给你排位置”的侥幸行为。

这样写的好处是：

1. 不会重叠
2. 不会因为后续改布局而出错
3. 一眼就能看出哪个控件对应哪个槽位

---

## 7. `card` 和 `panel` 是最常用的容器

它们本质上是已经带背景、描边、阴影、标题的 surface。

典型写法：

```cpp
ui.card("Controls")
    .in(left_rect)
    .title_font(16.0f * scale)
    .radius(22.0f * scale)
    .fill(0x151B2B)
    .stroke(0x2A3245, 1.0f)
    .begin([&](auto& card) {
        ui.column(card.content())
            .tracks({eui::quick::px(40.0f), eui::quick::fr()})
            .gap(10.0f)
            .begin([&](auto& rows) {
                ui.button("Run").in(rows.next()).draw();
                ui.text_area_readonly("Code", code).in(rows.next()).draw();
            });
    });
```

你在 `begin(...)` 里拿到的 `card` / `panel`：

- `card.content()`
  返回去掉标题、内边距之后的内部内容区

这一层非常适合当“组件边界”。

---

## 8. 显式指定位置和宽高怎么写

除了声明式布局拿槽位，EUI 还支持非常直接的“我就要把这个东西画在这里”。

这套用法来自所有基于 `PositionedBuilderBase` 的 builder，最常用的方法有：

- `.in(rect)`
- `.at(x, y)`
- `.size(w, h)`
- `.fill_parent()`
- `.after_last(dx, dy)`
- `.below_last(dx, dy)`

### 8.0 为什么位置参数不是只写在 `shape` 里

这是刻意的 API 设计，不是遗漏。

EUI 现在把“放在哪”和“长什么样”分成了两层：

- 放置层
  负责 `in / at / size / fill_parent / after_last / below_last`
- 样式层
  负责 `radius / fill / stroke / blur / rotate / font / tint`

所以你看到的不是“只有 `shape` 能设置位置”，而是：

- `shape` 能设置位置
- `text` 能设置位置
- `button` 能设置位置
- `slider` 能设置位置
- `input` 能设置位置
- `card / panel` 也能设置位置

这样设计有几个实际好处：

1. 所有元素共享一套定位心智模型
   你不用分别记 `shape.x/y/w/h`、`button.rect`、`text.bounds` 这种互不统一的 API。

2. 更容易和声明式布局衔接
   `row / column / grid / anchor` 的输出本质都是 `Rect`。
   统一用 `.in(slot)`，任何元素都能直接消费布局结果。

3. 布局和样式真正解耦
   今天你在某个槽位里放 `shape`，明天改成 `button` 或 `card`，定位代码不用重写。

4. 更适合组件复用
   组件函数最自然的参数就是 `Rect rect`。
   然后组件内部决定是画 `shape`、`text`、`button` 还是组合它们。

5. 能同时兼容两种写法
   一种是声明式布局先分槽，再 `.in(slot)`。
   一种是手工定位，直接 `.at(...).size(...)`。

如果把位置只做成 `shape` 自己的属性，那么 `button / input / text / card` 又得各做一套完全平行的位置 API，项目会很快变乱。

### 8.1 `.in(rect)` 是最推荐的显式方式

```cpp
const Rect button_rect{40.0f, 80.0f, 160.0f, 36.0f};
ui.button("Save").in(button_rect).primary().draw();
```

优点：

- 可读性最好
- 和声明式布局最兼容
- 你一眼就知道控件最终落在哪个区域

### 8.2 `.at(x, y) + .size(w, h)` 适合纯手工摆放

```cpp
ui.shape()
    .at(48.0f, 120.0f)
    .size(220.0f, 80.0f)
    .radius(20.0f)
    .fill(0x1F2937)
    .draw();
```

这和：

```cpp
ui.shape().in(Rect{48.0f, 120.0f, 220.0f, 80.0f}).draw();
```

本质上是同一件事，只是写法不同。

推荐使用场景：

- 画调试面板
- 画一块固定位置的装饰图元
- 你就是想手工定位，不想走布局容器

### 8.3 `.fill_parent()` 让图元直接吃满当前内容区

```cpp
ui.shape()
    .fill_parent()
    .radius(24.0f)
    .gradient(0x0F172A, 0x111827)
    .draw();
```

这个通常放在 `panel/card` 内部，或者某个 `scope` / `view` 里当背景层。

### 8.4 `.after_last()` 和 `.below_last()` 是相对上一个元素摆放

```cpp
ui.text("Title").at(40.0f, 40.0f).size(180.0f, 24.0f).draw();
ui.text("Subtitle").below_last(0.0f, 8.0f).size(220.0f, 18.0f).draw();
ui.button("Action").below_last(0.0f, 12.0f).size(140.0f, 36.0f).draw();
```

或者横向串起来：

```cpp
ui.button("Prev").at(40.0f, 220.0f).size(100.0f, 36.0f).draw();
ui.button("Next").after_last(12.0f, 0.0f).size(100.0f, 36.0f).draw();
```

适合：

- 少量手工堆叠
- 临时调试
- 不想额外开 `row/column`

但如果一整块区域里元素很多，还是更推荐 `row/column/grid`。

### 8.5 什么时候用显式尺寸，什么时候用声明式布局

推荐规则：

- 页面级结构：优先 `row/column/grid/zstack`
- 小范围装饰图元：优先 `in(rect)` 或 `at()+size()`
- 背景层：优先 `fill_parent()`
- 少量紧邻元素：可以用 `after_last()/below_last()`

如果你发现一个区域要手工算 7 个以上的坐标，大概率应该改成声明式布局。

---

## 9. 锚点布局怎么用

锚点布局不是直接画东西，它是先帮你“解出一个矩形”。

核心写法是：

```cpp
const Rect rect = ui.anchor()
    .in(parent_rect)
    .left(24.0f)
    .top(18.0f)
    .size(220.0f, 40.0f)
    .resolve();
```

然后你再把这个结果传给任何控件或图元：

```cpp
ui.button("Pinned").in(rect).draw();
```

### 9.1 最常用锚点方法

- `.in(rect)`：指定参考区域
- `.to_last()`：以上一个元素为参考区域
- `.left(px)` / `.right(px)` / `.top(px)` / `.bottom(px)`
- `.center_x(offset)` / `.center_y(offset)`
- `.width(px)` / `.height(px)` / `.size(w, h)`
- `.left_percent(...)` / `.top_percent(...)` / `.width_percent(...)`
- `.center_x_percent(...)` / `.center_y_percent(...)`

### 9.2 左上角固定偏移

```cpp
const Rect title = ui.anchor()
    .in(card.content())
    .left(18.0f * scale)
    .top(16.0f * scale)
    .size(240.0f * scale, 24.0f * scale)
    .resolve();

ui.text("Anchored Title").in(title).font(font_heading(scale)).draw();
```

### 9.3 相对父区域居中

```cpp
const Rect dialog = ui.anchor()
    .in(frame_rect)
    .center_x()
    .center_y()
    .size(420.0f * scale, 260.0f * scale)
    .resolve();

ui.panel("Dialog").in(dialog).draw();
```

### 9.4 百分比锚点

```cpp
const Rect badge = ui.anchor()
    .in(stage_rect)
    .left_percent(0.72f)
    .top_percent(0.12f)
    .width_percent(0.18f)
    .height(32.0f * scale)
    .resolve();
```

这适合：

- 需要按父容器比例定位
- 响应式小浮层
- 图表上的相对标记

### 9.5 相对上一个元素锚定

```cpp
ui.shape().in(card_rect).radius(20.0f).fill(0x1F2937).draw();

const Rect chip = ui.anchor()
    .to_last()
    .right(12.0f * scale)
    .top(12.0f * scale)
    .size(88.0f * scale, 28.0f * scale)
    .resolve();

ui.button("Live").in(chip).primary().draw();
```

`to_last()` 的意思是：以上一个绘制元素的矩形为参考。

### 9.6 锚点和 `row/column/grid` 怎么选

建议这样理解：

- `row/column/grid`
  负责规则布局
- `anchor`
  负责补充式定位

最常见的组合方式是：

1. 先用 `row/column/grid` 把大结构分出来
2. 再在某个局部区域里用 `anchor()` 精确摆标题、角标、浮层、按钮

也就是说，锚点更像“精修工具”，不是整页主布局工具。

---

## 10. 怎么画形状、文字和图标

### 8.1 画一个最简单的矩形

```cpp
ui.shape()
    .in(rect)
    .radius(18.0f)
    .fill(0x1F2937, 1.0f)
    .stroke(0x334155, 1.0f, 0.9f)
    .draw();
```

### 10.2 画渐变

```cpp
ui.shape()
    .in(rect)
    .radius(20.0f)
    .gradient(0x0F172A, 0x1E293B, 1.0f)
    .draw();
```

### 10.3 画阴影和模糊

```cpp
ui.shape()
    .in(rect)
    .radius(20.0f)
    .fill(0x111827, 0.92f)
    .shadow(0.0f, 16.0f, 28.0f, 0x000000, 0.22f, 6.0f)
    .blur(10.0f, 20.0f)
    .draw();
```

`blur(radius, backdrop_radius)` 的含义：

- 第一个参数更偏向图元本身的软化
- 第二个参数更偏向背景采样模糊

如果你想做玻璃感，通常第二个参数更关键。

### 10.4 画文字

```cpp
ui.text("Dashboard")
    .in(title_rect)
    .font(22.0f * scale)
    .left()
    .color(0xF8FAFC, 1.0f)
    .draw();
```

可选对齐：

- `.left()`
- `.center()`
- `.right()`

### 10.5 画图标

```cpp
ui.glyph(0xF013u)
    .in(icon_rect)
    .tint(0xE2E8F0, 1.0f)
    .draw();
```

如果你已经在工程里设置了图标字体，这种写法就够了。

---

## 11. 怎么控制图元的各种数值

图元本身就是链式调用，属性都直接写在 builder 上。

例如：

```cpp
ui.shape()
    .in(card_rect)
    .radius(state.radius * scale)
    .gradient(top_hex, bottom_hex, state.card_alpha)
    .stroke(0xFFFFFF, 1.0f, 0.12f)
    .shadow(0.0f, 18.0f * scale, 30.0f * scale, 0x000000, 0.22f, 6.0f * scale)
    .blur(glow * 0.20f, glow)
    .scale(card_scale)
    .rotate(card_rotation)
    .origin_center()
    .draw();
```

这段代码里，实际被状态驱动的量有：

- `radius`
- `alpha`
- `shadow`
- `blur`
- `scale`
- `rotate`

所以控制图元并不需要单独的“属性系统”，普通变量就够了。

比如：

```cpp
struct State {
    float radius{24.0f};
    float blur{16.0f};
    float card_alpha{0.92f};
};
```

然后通过滑杆直接改：

```cpp
ui.slider("Radius", state.radius).in(rows.next()).range(12.0f, 32.0f).draw();
ui.slider("Blur", state.blur).in(rows.next()).range(0.0f, 24.0f).draw();
ui.slider("Alpha", state.card_alpha).in(rows.next()).range(0.3f, 1.0f).draw();
```

这就是最直接也最容易维护的写法。

---

## 12. 控件点击怎么写

绝大多数内置控件，点击就是看 `draw()` 返回值。

### 10.1 按钮点击

```cpp
if (ui.button("Apply").in(rows.next()).primary().draw()) {
    state.expanded = true;
}
```

### 10.2 Tab 点击

```cpp
if (ui.tab("General", state.page == 0).in(slot).draw()) {
    state.page = 0;
}
```

### 10.3 Dropdown 选项点击

```cpp
ui.dropdown("Density", state.dropdown_open)
    .begin([&](auto& menu) {
        ui.column(menu.content())
            .repeat(3, eui::quick::px(36.0f * scale))
            .gap(8.0f * scale)
            .begin([&](auto& items) {
                if (ui.button("Compact").in(items.next()).ghost().draw()) {
                    state.mode = 0;
                    state.dropdown_open = false;
                }
                if (ui.button("Balanced").in(items.next()).ghost().draw()) {
                    state.mode = 1;
                    state.dropdown_open = false;
                }
                if (ui.button("Comfortable").in(items.next()).ghost().draw()) {
                    state.mode = 2;
                    state.dropdown_open = false;
                }
            });
    });
```

---

## 13. 自绘图元点击怎么写

如果你不是用内置按钮，而是自己画了一个形状，那点击就要自己做 hit test。

EUI 里 `Rect` 自带：

```cpp
bool contains(float px, float py) const;
```

输入状态在：

```cpp
const auto& input = ctx.input_state();
```

常见辅助函数写法：

```cpp
bool hovered(const eui::InputState& input, const Rect& rect) {
    return rect.contains(input.mouse_x, input.mouse_y);
}

bool clicked(const eui::InputState& input, const Rect& rect) {
    return hovered(input, rect) && input.mouse_pressed;
}
```

然后这样用：

```cpp
const Rect swatch = grid.next();

ui.shape().in(swatch).radius(14.0f).fill(0x3B82F6).draw();

if (clicked(input, swatch)) {
    state.selected_color = 0x3B82F6;
}
```

这就是自绘组件交互最常见的写法。

---

## 14. 动画到底怎么写

EUI 里你最先要掌握的是三种动画方式。

### 12.1 `ui.motion(...)`

给一个 key 和目标值，运行时自动帮你平滑逼近。

```cpp
const float x = ui.motion("sidebar-x", open ? 0.0f : 320.0f, 12.0f);
```

适合：

- 面板滑入滑出
- 位移
- 缩放
- 旋转角度
- 模糊半径
- 透明度目标值

你可以自己包一层更短的 helper：

```cpp
float slide(UI& ui, std::string_view key, float target, float speed = 12.0f) {
    return ui.motion(key, target, speed);
}
```

然后直接写：

```cpp
const float card_y = slide(ui, "card-y", state.expanded ? -12.0f : 26.0f, 10.0f);
```

### 12.2 `ui.presence(...)`

它返回一个 0 到 1 的显隐过渡值。

```cpp
const float reveal = ui.presence("dialog-visible", state.open, 12.0f, 16.0f);
```

适合：

- 出现 / 消失
- hover 高亮
- tooltip 淡入淡出
- 控件选中态的过渡权重

也可以自己包成 helper：

```cpp
float show(UI& ui, std::string_view key, bool visible, float enter_speed = 12.0f, float exit_speed = 16.0f) {
    return ui.presence(key, visible, enter_speed, exit_speed);
}
```

### 12.3 `ui.animate(...)`

这是更偏时间轴的动画。

如果你要明确控制一段时长、曲线、from/to，可以用它。

```cpp
eui::animation::TimelineClip clip{};
clip.scalar.from = 0.0f;
clip.scalar.to = 1.0f;
clip.duration_seconds = 0.8f;
clip.easing = eui::animation::preset_bezier(eui::animation::EasingPreset::ease_in_out);

const float t = ui.animate(clip, elapsed_seconds);
```

仓库里 `examples/EUI_gallery.cpp` 还封装了一个往返动画辅助函数，思路就是：

1. 算当前 loop 进度
2. 交给 `ui.animate`
3. 超过半程后反向映射

如果你只是做普通 UI 动画，优先级建议是：

1. 先用 `motion`
2. 再用 `presence`
3. 真正需要时间轴曲线时再用 `animate`

---

## 15. 动画值最后怎么喂给图元

动画系统本身只给你“数值”，真正的效果还是你自己把这些数值喂给图元属性。

例子：

```cpp
const float reveal = show(ui, "card-visible", state.expanded);
const float card_y = slide(ui, "card-y", state.expanded ? -12.0f : 26.0f);
const float card_rotation = slide(ui, "card-rotation", state.expanded ? 5.0f : -6.0f);
const float card_scale = slide(ui, "card-scale", state.expanded ? 1.0f : 0.92f);
const float glow = slide(ui, "card-glow", state.glow ? state.blur * scale : 0.0f);

ui.shape()
    .in(card_rect)
    .gradient(top_hex, bottom_hex, state.card_alpha)
    .blur(glow * 0.20f, glow)
    .scale(card_scale)
    .rotate(card_rotation)
    .translate(0.0f, card_y)
    .origin_center()
    .draw();
```

这就是动画最真实的样子：

- 动画系统只算数
- 图元 builder 消费这些数

---

## 16. 动画要记得持续刷新

如果你写了动画，但画面不刷新，那数值虽然在变，屏幕不会动。

两种方式：

### 14.1 持续刷新

```cpp
options.continuous_render = true;
```

最简单，适合 demo 和大量实时动效页面。

### 14.2 按需刷新

```cpp
if (ctx.consume_repaint_request()) {
    frame.request_next_frame();
}
```

更省资源，适合正式产品页面。

如果你不确定，先用持续刷新把逻辑做对，再考虑优化。

---

## 17. `bind(...)` 什么时候用

大部分时候你直接传数值就够了。

```cpp
ui.card("Panel").radius(22.0f * scale).draw();
```

如果某个属性想保持“每次 draw 时按当前状态重新求值”，可以用 `bind(...)`：

```cpp
const auto control_height = eui::quick::bind([&] {
    return 40.0f * scale;
});

ui.button("Run").height(control_height).draw();
```

常见场景：

- 属性依赖 `scale`
- 属性依赖当前状态
- 不想在外面重复写同一套计算

---

## 18. 一个完整页面该怎么组织

推荐这样拆：

### 16.1 状态结构

```cpp
struct DemoState {
    bool expanded{true};
    bool glow{true};
    float gap{18.0f};
    float radius{24.0f};
    float blur{16.0f};
    float card_alpha{0.92f};
};
```

### 16.2 小辅助函数

```cpp
float slide(UI& ui, std::string_view key, float target, float speed = 12.0f);
float show(UI& ui, std::string_view key, bool visible, float enter_speed = 12.0f, float exit_speed = 16.0f);
void draw_chip(UI& ui, const Rect& rect, std::string_view text, ...);
```

### 16.3 左侧控制区

负责：

- 按钮
- 滑杆
- 输入框
- 下拉框

只改状态，不负责复杂视觉表现。

### 16.4 右侧预览区

负责：

- 读取状态
- 算动画值
- 画图元

这样以后你换布局、换视觉、换动画，都不会把整页搅成一团。

---

## 19. 一个推荐的页面骨架

下面这段结构是非常通用的：

```cpp
ui.panel("Demo")
    .in(frame_rect)
    .begin([&](auto& root) {
        ui.view(root.content())
            .column()
            .tracks({
                eui::quick::px(52.0f * scale),
                eui::quick::fr(),
            })
            .gap(16.0f * scale)
            .begin([&](auto& rows) {
                const Rect header = rows.next();
                const Rect body = rows.next();

                draw_header(ui, header, scale);

                ui.row(body)
                    .tracks({
                        eui::quick::px(320.0f * scale),
                        eui::quick::fr(),
                    })
                    .gap(state.gap * scale)
                    .begin([&](auto& cols) {
                        draw_controls(ui, state, cols.next(), scale);
                        draw_preview(ui, state, cols.next(), scale, time_seconds);
                    });
            });
    });
```

这个骨架基本可以复用到：

- 设置页
- 设计页
- 动画页
- dashboard
- 组件展示页

---

## 20. 你真正需要记住的 API 清单

布局：

- `ui.view(rect)`
- `ui.row(rect)`
- `ui.column(rect)`
- `ui.grid(rect)`
- `ui.zstack(rect)`
- `ui.anchor()`
- `eui::quick::px(...)`
- `eui::quick::fr(...)`
- `eui::quick::fit(...)`

显式定位：

- `.in(rect)`
- `.at(x, y)`
- `.size(w, h)`
- `.fill_parent()`
- `.after_last(dx, dy)`
- `.below_last(dx, dy)`

容器：

- `ui.panel(...)`
- `ui.card(...)`

控件：

- `ui.button(...)`
- `ui.tab(...)`
- `ui.slider(...)`
- `ui.input(...)`
- `ui.input_float(...)`
- `ui.readonly(...)`
- `ui.progress(...)`
- `ui.text_area(...)`
- `ui.text_area_readonly(...)`
- `ui.dropdown(...)`

自绘：

- `ui.shape()`
- `ui.text(...)`
- `ui.glyph(...)`
- `ui.image(...)`

动画：

- `ui.motion(...)`
- `ui.presence(...)`
- `ui.animate(...)`
- `ui.ease(...)`
- `ui.reset_motion(...)`

---

## 21. 最常见的错误

### 错误 1：把 `row/column/grid` 当成自动排版器

错误写法：

```cpp
ui.column(rect).begin([&](auto&) {
    ui.button("A").draw();
    ui.button("B").draw();
});
```

问题：

- 没显式使用槽位
- 容易重叠
- 视觉结果不可控

正确思路：

```cpp
ui.column(rect)
    .tracks({eui::quick::px(36.0f), eui::quick::px(36.0f)})
    .begin([&](auto& rows) {
        ui.button("A").in(rows.next()).draw();
        ui.button("B").in(rows.next()).draw();
    });
```

### 错误 2：动画值算出来了，但没喂给任何属性

错误：

```cpp
const float t = ui.motion("x", 100.0f, 12.0f);
```

如果后面没用 `t`，就不会有任何视觉效果。

### 错误 3：写了动画，但没有持续刷新

症状：

- 点一下只更新一帧
- 动画像卡住一样

解决：

- 打开 `continuous_render`
- 或者在需要时 `request_next_frame()`

### 错误 4：自绘区域以为会自动点击

`shape()` 只是画图，不会自动变按钮。

如果你要点它，必须自己做 hit test。

---

## 22. 从哪里继续看

建议阅读顺序：

1. 先读这篇文档
2. 再打开 `examples/minimal_quick_demo.cpp`
3. 然后看 `examples/EUI_gallery.cpp`
4. 如果要看目录职责，再看 `docs/project-structure.zh-CN.md`
5. 如果要看渲染路线，再看 `docs/modern-gl-roadmap.zh-CN.md`

团队定开发规范约定：

1. 所有新页面统一用 `view/row/column/grid/zstack`
2. 所有交互控件统一 `.in(slot)`
3. 所有页面状态统一放到显式 `State` 结构体
4. 所有动画统一先用 `motion/presence`
5. 复杂时间轴才用 `animate`
6. 自绘交互统一自己写 `hovered/clicked`

这样整个项目会非常清晰，而且不会再回到“到处手工算 rect、到处重叠”的状态。
