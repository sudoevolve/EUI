# EUI 现代 GL 主线

最后更新：2026-03-19

## 结论

当前仓库的公开图形路线已经明确收敛为：

1. `EUI_BACKEND_OPENGL` 是唯一已经落地的渲染主线。
2. 默认桌面路径走 modern GL，不再把旧 fixed-function 视为主实现。
3. `GLFW` 和 `SDL2` 共用同一套 OpenGL renderer 与大部分运行时逻辑。
4. `EUI_OPENGL_ES` 是这条 OpenGL 主线下的兼容分支，不是独立 renderer。
5. `Vulkan` 仍然只是占位；当前不是实验实现，而是明确未实现。

## 对外入口

示例和用户代码仍然保持代码内宏选择，不要求额外写一堆构建配置：

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"
```

或：

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_SDL2
#include "EUI.h"
```

如需切到 OpenGL ES 兼容上下文，可额外定义：

```cpp
#define EUI_OPENGL_ES
```

当前状态说明：

- `EUI_BACKEND_OPENGL`：已实现，也是推荐公开路线
- `EUI_PLATFORM_GLFW`：已实现
- `EUI_PLATFORM_SDL2`：已实现
- `EUI_OPENGL_ES`：作为 OpenGL 后端下的 ES 兼容分支可用
- `EUI_BACKEND_VULKAN`：未实现，当前会直接报错而不是进入半成品路径

## 支持矩阵

下表描述的是当前代码路径状态，而不是所有平台组合都已经完成充分设备验证。

| 宏组合 | 当前状态 | 说明 |
| --- | --- | --- |
| `EUI_BACKEND_OPENGL + EUI_PLATFORM_GLFW` | 已实现，推荐 | 桌面 OpenGL 主路径之一 |
| `EUI_BACKEND_OPENGL + EUI_PLATFORM_SDL2` | 已实现，推荐 | 桌面 OpenGL 主路径之一 |
| `EUI_BACKEND_OPENGL + EUI_PLATFORM_GLFW + EUI_OPENGL_ES` | 已实现，兼容分支 | 复用同一套 modern GL 数据流，针对 ES 做差异处理 |
| `EUI_BACKEND_OPENGL + EUI_PLATFORM_SDL2 + EUI_OPENGL_ES` | 已实现，兼容分支 | 同上 |
| `EUI_BACKEND_VULKAN` | 未实现 | 当前不是可用分支 |

## 当前已经落实的方向

- 主绘制路径已经改成 `shader + VBO + batched vertices`。
- 文本主路径优先 `stb_truetype`。
- Windows 下的 Win32/native 文本只保留为非 ES 桌面路径的兜底方案。
- dirty cache 与 backdrop blur 已经接入 modern GL 提交路径。
- `GLFW` 和 `SDL2` 都会为 modern GL 设置 proc loader。
- OpenGL 主路径已经不再依赖“旧 renderer 一套、现代 renderer 一套”的分裂实现。

## 当前渲染架构

### 统一的 modern GL 提交层

- 统一 shader program。
- 统一 VBO 上传与顶点提交。
- 填充、描边、文本、缓存纹理、backdrop blur 都走同一条现代 OpenGL 提交链。
- `GLFW`/`SDL2` 的差异主要停留在窗口、输入和上下文创建层。

### 文本路径

- 文本默认优先走 `stb_truetype` atlas。
- 图标字体继续通过同一套文本 atlas 机制接入。
- Windows 非 ES 模式保留 Win32 native 文本兜底，但它不是主路径。
- ES 模式下不再启用 Win32 fixed-function 文本 fallback。

### dirty cache 与实时效果

- 增量重绘仍然保留。
- dirty diff 已经基于轻量快照与缓存可见区域工作，不再依赖整份上一帧绘制命令复制。
- backdrop blur 已经并入 modern GL 路径，不再走单独旧式绘制分支。

## 当前已经落地的性能与内存收敛

这部分不是“未来规划”，而是当前 modern GL 主线的一部分。

- `DrawCommand` 已做热冷拆分。
  - 命令本体保留热字段。
  - 渐变 brush 和 3D transform 通过 payload arena 间接引用。
- brush/transform payload 已支持帧内去重。
- 渐变 payload 已支持写时复制，避免动画期修改污染共享数据。
- 命令可见区域已经缓存到 `visible_rect`，dirty diff 不再反复重算投影包围盒。
- 上一帧比较缓存已改成轻量快照，而不是完整 `DrawCommand` 复制。
- runtime、renderer、font renderer 的 scratch/vector 已接入 hysteresis trim。
- spatial index 已经从更保守的索引类型收紧，降低常驻内存。
- cache texture 与 backdrop texture 已接入闲置回收。
- STB 字体 atlas 已支持空闲页回收，保留 font blob/metrics，释放不活跃 atlas 纹理页。

## Desktop GL 与 GLES 的当前差异

- Desktop GL：
  - 描边和 chevron 继续使用 `GL_LINES`，优先维持更低顶点量和更低内存占用。
  - Windows 非 ES 路径允许保留 Win32 native 文本兜底。
- GLES：
  - 描边和 chevron 改用三角形厚线，避免依赖 `glLineWidth` 的兼容性。
  - 继续走 modern GL shader/VBO 路径，不回退到旧 fixed-function 逻辑。
- 共享部分：
  - shader 驱动的普通图元绘制
  - 统一 VBO 上传
  - 文本 atlas
  - dirty cache
  - backdrop blur
  - 缓存纹理绘制

## 为了 GLES 已做的兼容处理

- GLFW/SDL2 窗口层已经支持 ES context hint。
- modern GL shader 已按 desktop GL / GLES 分开处理。
- 文本 atlas 纹理格式使用兼容分支处理，采样路径已经统一到 shader。
- Windows 下 ES 模式不再启用 Win32 fixed-function 文本 fallback。

## 当前仍未完成的部分

- `Vulkan` renderer 未实现。
- renderer capability 还没有提炼成完整的公共抽象层。
- dirty cache 仍然带有 OpenGL framebuffer copy 假设。
- backdrop blur 仍然依赖 OpenGL 侧的 framebuffer/texture copy 语义。
- 部分历史兼容 helper 还留在代码里，但已经不再是主路径。
- `EUI_OPENGL_ES` 虽然已经纳入同一条现代 OpenGL 路线，但仍应视为兼容分支，而不是一条完全独立维护的前线实现。

## 当前建议

- 新功能优先继续落在 modern GL 主线上。
- `GLES` 兼容继续沿用同一套数据流补齐，不再回头扩张旧 fixed-function 逻辑。
- 对外文档、示例、代码片段都继续围绕：
  - `EUI_BACKEND_OPENGL`
  - `EUI_PLATFORM_GLFW`
  - `EUI_PLATFORM_SDL2`
  - 可选 `EUI_OPENGL_ES`
- `Vulkan` 等 renderer 边界、资源生命周期和 capability 抽象稳定一轮之后再接。

## 下一阶段优先级

### P0

- 继续维持 modern GL 单主线，不再引入第二套并行主 renderer。
- 继续收敛内存与运行时缓存峰值。
- 继续保证 `GLFW` 和 `SDL2` 共用同一套 renderer 行为。

### P1

- 补更明确的 GLES 验证矩阵。
- 收敛 renderer capability 与 backend contract。
- 继续减少 OpenGL 特定假设在 dirty cache/backdrop blur 中的耦合。

### P2

- 在 renderer contract 稳定后，再讨论 Vulkan 的资源模型、命令提交流程和平台接入边界。
