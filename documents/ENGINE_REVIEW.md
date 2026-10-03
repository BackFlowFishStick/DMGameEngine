# DMGameEngine - 框架评估与完善建议

> 创建日期：2026-07-06
> 最后更新：2026-07-26（ShaderLibrary 存在性更正 + 阶段 0 Vulkan 正确性修复 A/C/D/F 落地 + 测试/CI 骨架 E2 落地，5/5 单测通过；详见下方 2026-07-26 摘要 + `VULKAN_FIXES.md`）
> 评估基准：`ENGINE_SUMMARY.md`（截至 2026-07-19）+ 实际源码核验（engine 仓库 git 历史 + 源码直查）
> 目的：针对引擎框架需要完善的部分，给出分优先级的可执行建议，并补全面向未来的演进路线

---

## Review 变更摘要（2026-07-26）

**ShaderLibrary 存在性更正**：经源码 + git 历史复核，`ShaderLibrary` **并未移除**：

- 类声明当前位于 `src/DMGameEngine/Renderer/Shader.h`（与 `Shader` 同头文件，第 167 行起），实现完整在 `Shader.cpp`（第 83-120 行，`Add`/`Load`/`Get`/`Exists` 四方法全实现）。
- 经 `src/DMGameEngine/DMGameEngine.h` 第 42 行 `#include "DMGameEngine/Renderer/Shader.h"` 暴露为公共 API。
- `git log -S "ShaderLibrary" -- src/DMGameEngine/Renderer/Shader.h` 仅有 `fac8db3`（新增）一个提交，**无任何移除提交**；下文原「曾落地（`fac8db3`）后移除」表述系误判，本轮更正。

> 说明：`ShaderLibrary` 存在 ≠ 资源体系完备。它仍是「按名缓存 shader」的独立类，未纳入统一 `AssetManager`（无 `AssetHandle` / 异步加载 / 热重载 / 引用计数延迟释放），与 `Shader` 同头文件亦属耦合。统一资源管理仍属 E3 待办，但前提条件从「重做 ShaderLibrary」修正为「整合既有 `ShaderLibrary` 进 `AssetManager`」。

**阶段 0 落地（Vulkan 正确性 A/C/D/F + 测试/CI E2）**：`VulkanBackendReview.md` 列出的 Vulkan 后端正确性隐患全部修复并编译验证通过，详见 `VULKAN_FIXES.md`：

- **A**（pipeline 缓存随 swapchain recreate 清空）：`VulkanRendererAPI::OnSwapchainRecreate()` 销毁缓存 pipeline，`RecreateSwapchain` 末尾调用。注：07-25 离屏改动已把 `colorFormat/depthFormat` 纳入 `PipelineKey`，A 实际从「device lost」降为「小泄漏」，本次为防御性清理。
- **C**（RequestResize 接线）：`GraphicsContext` 加 `RequestResize` 虚函数，`WindowsWindow` resize 回调调用，`RecreateSwapchain` 改用 `glfwGetFramebufferSize` 取真实像素。
- **D**（VK_CHECK release 不静默）：release 分支由 `(void)(x)` 改为 `DMGE_LOG_ERROR`。
- **F**（未绑定 sampler 槽）：`VulkanRendererAPI` 持全局 1×1 dummy texture 填充 unbound descriptor 槽。
- **E2**（测试 + CI）：新增 `tests/`（GoogleTest，5 个纯逻辑 case）+ `CMakeLists.txt` `DMGE_BUILD_TESTS` 选项 + `.github/workflows/ci.yml`。CLion 构建验证 5/5 通过，Vulkan 后端 + A/C/D/F 编译链接通过。运行时 Validation 场景验证待后续。

阶段 0 剩余项（B per-frame deletion queue、P0 descriptor 复用 / ImmediateSubmit 批量化）设计完成、未落地，见 `VULKAN_FIXES.md` §6/§7。

---

## Review 变更摘要（2026-07-20）

距上一轮（2026-07-16）复核，引擎已落地多项重大基础设施（详见 `ENGINE_SUMMARY.md` 07-16->07-19 日志 + engine 仓库 git 历史）：

- **Vulkan 渲染后端**（07-18/19）- 镜像 OpenGL 抽象（`VulkanDevice`/`VulkanSwapchain`/`VulkanGraphicsContext`/`VulkanRendererAPI`/`VulkanShader`/`Vulkan{Vertex,Index}Buffer`/`VulkanVertexArray`/`VulkanTexture{2D,2DArray,Cube}`），Vulkan 1.3 dynamic rendering + VMA + shaderc，`-DDMGE_VULKAN_BACKEND=ON` 可选启用；ImGui 已接 `imgui_impl_vulkan`；投影 Y 翻转收敛于 `Renderer::BeginScene`。详见 `VULKAN_BACKEND.md`。
- **`RenderCommand` 命令门面层**（07-19）- 渲染抽象由两层（`RendererAPI`/`Renderer`）拆为三层（`RendererAPI`/`RenderCommand`/`Renderer`），后端实例所有权迁至 `RenderCommand`，`Renderer` 不再 include 后端头。
- **`Material`/`MaterialInstance`** - shader + uniform 包（`std::variant` 类型擦除），`Renderer::Submit` 新增 material 重载，后端无关。
- **Texture 体系重构** - `Texture2D` / `TextureCube` / `Texture2DArray` 三派生 + 规格，不可变存储 + `glTexSubImage` 更新。
- **`CameraController` 层级** - 基类 + `OrthographicCameraController`（2D）+ `EditorCameraController`（3D 轨道），输入驱动相机，`Application` 改持控制器。
- **`Renderer::SetAPI()`** - API 选择可在 `Run()` 前切换（OpenGL/Vulkan），D2「硬编码 API」基本解决（见 D2）。

本轮复核同时发现：`ShaderLibrary` 经源码 + git 历史复核**并未移除**（详见上方 2026-07-26 变更摘要）--它当前仍与 `Shader` 同处 `Renderer/Shader.h`、实现完整在 `Shader.cpp`，并经 `DMGameEngine.h` 暴露为公共 API；原「曾落地（`fac8db3`）后移除」表述系误判。资源管理仍缺统一 `AssetManager`（见 E3），仓库仍**无任何测试**，工程化建议见 E2。下文新增 **E 节「面向未来的演进建议」**，给出向「可用引擎」演进的方向性建议；D2 状态更新为「基本解决」。

---

## Review 变更摘要（2026-07-16）

D3「Input 覆盖面不足」全部四项已修复（详见 `ENGINE_SUMMARY.md` 2026-07-16 条目）：

- 手柄/控制器抽象 - `Core/GamepadCodes.h`（`GamepadButton`/`GamepadAxis`/`kMaxGamepads`）+ `Input` 手柄轮询五方法（含双快照 `IsGamepadButtonJustPressed`）+ `glfwSetJoystickCallback` 热插拔事件（`GamepadConnected/DisconnectedEvent`）。
- 逐帧鼠标位移 - `Input::GetMouseDeltaX/Y()`，`OnEvent` 累积 `MouseMovedEvent`、`BeginFrame` 清零，`CursorMode::Disabled` 下也准确。
- 光标模式控制 - `Window::SetCursorMode(CursorMode::{Normal,Hidden,Disabled})` / `GetCursorMode()`（`Disabled` 即 FPS 锁鼠）。
- raw mouse motion - `Window::SetRawMouseMotion(bool)` / `IsRawMouseMotion()`（经 `glfwRawMouseMotionSupported()`，仅 `Disabled` 下生效）。

D3 由「仍开放」转为 ✅ 已修复。光标模式与 raw motion 故意解耦，FPS 锁鼠需二者组合，由调用方决定。

---
## Review 变更摘要（2026-07-10）

自首轮评估（2026-07-06）以来，引擎已推进多项基础设施。本轮核验结论：

- **A1 / A2 已修复** - 公共头补全，`ENGINE_SUMMARY.md` 与源码同步。
- **B1 已修复** - `Camera` 基类 + `OrthographicCamera` / `PerspectiveCamera` / `SceneCamera` 全部落地。
- **B2 已修复** - `BeginScene(const Camera&)` 缓存 VP，`Submit` 自动上传 `u_ViewProjection` / `u_Transform`。
- **B5 已修复** - `DMGE_GL_CALL` 宏 + `OpenGLDebug.h`，Debug 下全后端覆盖。
- **C1 已修复** - `ImGuiLayer` 完整集成（OnAttach/Begin/End/OnDetach + 输入捕获），Stage 4 不再空转。
- **B3 / C2 / C3 仍开放** - 见下文；**B4 已修复**（管线状态可配置化，见下文）。
- **A3 已修复** - 窗口 resize 现经 `OnViewportResize()` 同步活动相机宽高比（见 A3）。
- **C4 已修复** - `Timestep` 抽象已封装，`MainLoop` 钳制 `deltaTime` 上限 0.1s（见 C4）。

下文保留原始条目编号，已修复项标注 ✅ 并移入「已完成」节，仍开放项保留原文并更新现状说明。

---

## 当前完成度评估

已完成且质量不错的部分：

- 分层架构（`Layer`/`LayerStack` + 5 级 `LayerType`）
- 事件系统（`Event`/`EventDispatcher` + 键盘/鼠标/应用事件）
- 带边沿检测的 `Input`（`IsKeyPressed` 连续 + `IsKeyJustPressed` 上升沿，GLFW_REPEAT 显式区分）
- 渲染后端抽象（`RendererAPI` pimpl + `Create()` 工厂分发）
- `Shader`/`Texture`/`VertexBuffer`/`IndexBuffer`/`VertexArray` 资源体系（基类 + 工厂 + OpenGL 后端）
- `BufferLayout`/`BufferElement` 顶点属性布局
- 主循环五阶段时序（Event Pump -> Update -> Render -> ImGui -> Swap）
- 清屏与深度测试（`glClear` 颜色 + 深度，`glEnable(GL_DEPTH_TEST)`）
- **可配置初始管线状态**（`RendererAPIInitConfig` + 模板方法 `Init(config)`，深度/剔除/混合可经抽象层定制）
- 视口同步（`WindowResizeEvent` -> `Renderer::OnWindowResize`）+ 相机宽高比（`OnViewportResize()`）
- **相机体系**（`Camera` 基类持有 projection + view + 缓存 VP；三派生类齐备）
- **`Renderer::BeginScene(const Camera&)`** 缓存 VP 并在 `Submit` 统一上传
- **`ImGuiLayer`** 完整集成，驱动 Stage 4 ImGui 通道
- **`DMGE_GL_CALL`** Debug GL 错误检查（零 Release 开销）
- **`Timestep` 抽象**（秒数封装 + `GetSeconds`/`GetMilliseconds` + 隐式 `float` 转换；MainLoop 钳制 deltaTime 上限 0.1s）

整体抽象层次清晰，符合主流引擎（Hazel 风格）的演进路径。

**2026-07-20 追加（07-16->07-20 落地项）：**

- **可选 Vulkan 后端**（dynamic rendering + VMA + shaderc，`-DDMGE_VULKAN_BACKEND=ON`）+ ImGui Vulkan 接入 + 投影 Y 翻转
- **`RenderCommand` 三层渲染抽象**（`RendererAPI` / `RenderCommand` / `Renderer`）
- **`Material`/`MaterialInstance`**（shader + uniform 包，`Submit` material 重载）
- **`Texture2D`/`TextureCube`/`Texture2DArray`** 三派生 + 不可变存储
- **`CameraController` 层级**（基类 + 2D/3D 控制器，输入驱动相机）
- **`Renderer::SetAPI()`** 运行前切换 OpenGL/Vulkan

---

## ✅ 已完成项（首轮建议已落地）

### ✅ A1. 公共头 `DMGameEngine.h` 缺少关键渲染类型（已修复）

`DMGameEngine.h` 现已补全 `VertexArray`/`VertexBuffer`/`IndexBuffer`/`Texture`，并额外 include 了 `OrthographicCamera`/`PerspectiveCamera`/`RendererAPI`/`SceneCamera`/`ImGuiLayer`。游戏侧通过单一头文件即可创建全部可绘制资源。

### ✅ A2. `ENGINE_SUMMARY.md` 与源码不同步（已修复）

`ENGINE_SUMMARY.md`（最后更新 2026-07-10）目录树与各章节已纳入 `VertexArray`、`BufferLayout`/`BufferElement`、相机三派生类、`OpenGLDebug.h`、`ImGuiLayer` 等，并附逐日变更日志。文档现为可信单一事实源。

### ✅ B1. `Camera` 体系不完整（已修复）

`Renderer/Camera.h` 现为基类，持有 projection + view + 缓存的 `GetViewProjection()`，并提供 `SetProjection()`/`SetView()` 访问器。三个派生类已落地：

- `OrthographicCamera`（`glm::ortho` + 位置/旋转 -> view，旋转为度数内部换算弧度）
- `PerspectiveCamera`（`glm::perspective` + position/target/up -> `glm::lookAt` view）
- `SceneCamera`（运行时 `SetProjectionType()` 切换正交/透视，单一宽高比驱动两者）

### ✅ B2. `Renderer::BeginScene()` 太薄（已修复）

- `BeginScene(const Camera& camera)` 缓存 `camera.GetViewProjection()` 到 `SceneData`
- `Submit(shader, vertexArray, transform)` 绑定 shader 并自动上传 `u_ViewProjection`（来自 `SceneData`）+ `u_Transform`
- `Application::MainLoop` 在 Stage 3 据活动相机选择 `BeginScene(camera)` 或无参重载
- 残留：`EndScene()` / `Flush()` 仍为空占位（批量 flush 待渲染管线复杂化后再填，当前可接受）

### ✅ B5. Debug 下缺 GL 错误检查（已修复）

`Platform/OpenGL/OpenGLDebug.h` 提供 `DMGE_GL_CALL(x)` 宏：Debug 下先清错误、执行调用、`glGetError()` 循环并以 `DMGE_CORE_ASSERT` 在调用点断言；Release 下零开销展开为 `x`。`OpenGLRendererAPI` 全方法已包裹。

残留子项：尚未注册 `GL_KHR_debug` message callback（运行期 GL 推送日志更精准，非阻塞，可后续补）。

### ✅ C1. ImGui 尚未真正集成（已修复）

`ImGuiLayer`（`ImGui/ImGuiLayer.h/.cpp`）完整实现：

- `OnAttach`：`ImGui::CreateContext` + `ImGui_ImplGlfw_InitForOpenGL` + `ImGui_ImplOpenGL3_Init`
- `Begin`/`End`：`NewFrame` / `Render` + `RenderDrawData` 包裹 Stage 4
- `OnEvent`：ImGui 捕获鼠标/键盘时标记 `event.Handled` 阻断下沉
- `OnDetach`：逆序清理后端与上下文
- `Application` 自动以 overlay 挂载，`Shutdown` 经 `LayerStack::Clear()` 正确触发 `OnDetach`（GL 上下文仍存活）

---

## A. 立即修复（具体缺陷）

### ✅ A3.（已修复）窗口 resize 同步活动相机宽高比

`Application::OnEvent` 对 `WindowResizeEvent` 原仅调用 `Renderer::OnWindowResize`（设视口），**未**更新 `m_ActiveCamera` 的宽高比，导致缩放后投影保持旧比例、画面拉伸。已修复。

**方案：** `Camera` 基类新增虚方法 `OnViewportResize(uint32_t, uint32_t)`（默认 no-op），各派生类按需重写；`Application::OnEvent` 的 `WindowResizeEvent` lambda 改捕获 `[this]`，设视口后调用 `m_ActiveCamera->OnViewportResize(w, h)`（若已设相机），仍返回 `false` 不阻断层传播。

```cpp
viewportDispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& ev) {
    Renderer::OnWindowResize(static_cast<int>(ev.GetWidth()), static_cast<int>(ev.GetHeight()));
    if (m_ActiveCamera)
        m_ActiveCamera->OnViewportResize(ev.GetWidth(), ev.GetHeight());
    return false;  // do not mark handled; layers may still react
});
```

**派生类重写（resize 调用链统一 `OnViewportResize -> SetViewportSize -> SetAspectRatio -> Recalculate*`）：**
- `PerspectiveCamera` - 新增 `SetViewportSize(uint32_t, uint32_t)`（零尺寸守卫 + 安全 float 除法，委托 `SetAspectRatio`），`OnViewportResize()` 委托之（镜像 `SceneCamera`，避免调用方整数除法踩坑）
- `SceneCamera` - `OnViewportResize()` 委托已有的 `SetViewportSize`
- `OrthographicCamera` - 投影用显式 L/R/B/T、不依赖宽高比，沿用基类 no-op

**涉及文件：** `Renderer/Camera.h`、`Renderer/PerspectiveCamera.h`、`Scene/SceneCamera.h`、`Core/Application.cpp`

---

## B. 渲染管线补全（近期，阻塞"真正能画东西"）

### B3. 缺 `Framebuffer` 抽象（仍开放）

当前只能渲染到默认 framebuffer。后续编辑器（渲染场景到 viewport 窗口）、后处理、阴影贴图都需要离屏渲染。

**现状：** `src/DMGameEngine/Renderer/Framebuffer.h` 不存在；`CMakeLists.txt` 未登记。

**建议：** 新增 `Renderer/Framebuffer.h` + 工厂 + `OpenGLFramebuffer`，规格包含尺寸、颜色/深度附件格式、采样数。`Application` 视口 resize 时也应 resize 活动 framebuffer。

### B4. 管线状态不可配置（已修复 ✅）

`OpenGLRendererAPI::Init()` 原硬编码 `glEnable(GL_DEPTH_TEST)`，混合 / 深度比较函数 / 面剔除均无法经抽象层控制。现已修复：

- `RendererAPI` 接口已补齐管线状态 setter：`SetBlendState` / `SetBlendEquation` / `SetDepthTest` / `SetDepthFunc` / `SetCullMode`，并由 `Renderer` 静态门面转发。
- `Init()` 改为可配置模板方法：`RendererAPI::Init(const RendererAPIInitConfig& config = {})` 经上述虚 setter 应用初始状态；`RendererAPIInitConfig` 聚合清屏色、深度测试开关与比较函数、面剔除、混合开关与源/目标因子及方程，默认值对齐原硬编码基线（深度测试开 + `Less`、剔除关、混合关），无参调用行为不变。
- `Renderer::Init(config)` 转发至后端；`OpenGLRendererAPI` 移除原硬编码 `Init()` 重写，直接继承基类模板方法。

**残留子项（后续）：** 线框模式（polygon mode / `glPolygonMode`）尚未暴露；待编辑器/调试视图需要时再补。

---

## C. 缺失的核心子系统（中期，框架级）

### C2. 无资源/资产管理（仍开放）

`LayerType::Resource` 层已定义但无任何实现。`Shader`/`Texture` 是临时 `Create()` 直加载，无缓存、无引用计数、无生命周期管理、无热重载。

**现状：** 无 `Asset` 目录，无 `AssetManager`（`ShaderLibrary` 已存在于 `Shader.h`/`Shader.cpp`，作为按名缓存 shader 的子集，但未纳入统一资源体系，无 `AssetHandle` / 异步加载 / 热重载）。

**建议：**

- `AssetManager`（按路径/handle 缓存 + 引用计数）
- `ShaderLibrary`（按 name 查询已加载 shader）

这与"游戏工程 `find_package` 引用"的目标强相关--资源管理是引擎对外能力的核心。

### C3. 无 ECS / 场景图 / Transform 层级（仍开放）

目前没有实体、组件、场景组织，所有渲染状态散落在层内。这是渲染基础设施完成后的自然下一步。

**现状：** 无 `ECS` 目录，无 `Transform` 类型。`SceneCamera` 注释明确写道「view 由外部 `SetView()` 供给，本类不管理视图」--正因尚无场景 transform 驱动相机。

**建议：**

- 先定 `Transform`（local + world 矩阵 + 父子层级）
- 再上 ECS（archetype 或 sparse-set）
- `Camera` 派生类完成后，场景图可直接驱动相机和实体

### ✅ C4. `Timestep` 抽象与帧率稳定性（已修复）

`Core/Timestep.h` 封装帧时间步（秒数 + `GetSeconds()`/`GetMilliseconds()` + 隐式 `float` 转换）。`Application::MainLoop` 现将 `deltaTime` 经 `std::clamp(elapsed.count(), 0.0f, 0.1f)` 钳制上限 0.1s，包进 `Timestep` 后分发至 `Layer::OnUpdate(Timestep)` 与 `Application::OnUpdate(Timestep)`。钳制位于 Application 层，符合 `Timestep.h` 设计（类型只持有/展示时长，不负责钳制）。

**风险已消除：** 超大 `deltaTime` 被截断至 0.1s，物理/动画积分不再爆炸；`std::clamp` 同时防负值（时钟回拨）。

**残留子项（后续）：** 固定步长物理累加器（fixed timestep + accumulator）尚未引入，待物理子系统落地时再上；当前可变步长 + 钳制已满足稳定性需求。

> ✅ 建议清单前两项（封装 `Timestep` 类型 + 钳制 `deltaTime` 上限 0.1s）已落地；后两项（固定步长累加器、帧边界对齐）留待物理/输入子系统演进时再做。

---

## D. 框架健壮性（长期）

### D1. 性能分析器未落地（仍开放）

`LayerType::Tool` 注释写了"性能分析器"但无实现。

**建议：** 先做轻量 `InstrumentationTimer`（作用域计时 + JSON 输出，可被 Chrome tracing 读取），并在主循环五阶段各插桩定位瓶颈。低成本高收益。

### D2. `Renderer` 静态门面 + 硬编码 API（基本解决，仍有残留）

**2026-07-20 更新：** `Renderer` 现提供 `SetAPI(API)` / `GetAPI()`，API 可在 `Application::Run()` 前切换；`Renderer::API` 枚举含 `None`/`OpenGL`/`Vulkan`/`DirectX`，OpenGL 与 Vulkan 后端均已落地，demo 经 `Renderer::SetAPI(Renderer::API::OpenGL)` 显式选择。原「硬编码 `s_API = OpenGL` 无法切换」问题已解决。

**残留：**

- `DirectX` 仍为枚举桩、无后端实现（若不计划支持，建议从枚举移除以免误导）。
- `Renderer`/`RenderCommand` 仍为全静态门面 + 全局 `s_API`/`s_SceneData`，多窗口/多实例与单元测试仍困难（见 E2）。
- 后端差异已开始渗入场景层（`Renderer::BeginScene` 内 `if (Vulkan) flipY`），收敛方案见 E1。

### D3. Input 覆盖面不足（✅ 已修复）

四项全部落地（详见 `ENGINE_SUMMARY.md` 2026-07-16 条目）：

- 手柄/控制器抽象 - `GamepadCodes.h` + `Input` 手柄轮询（`IsGamepadPresent`/`GetGamepadName`/`IsGamepadButtonPressed`/`IsGamepadButtonJustPressed`/`GetGamepadAxis`）+ `glfwSetJoystickCallback` 热插拔事件。
- `GetMouseDelta()`（逐帧位移） - `Input::GetMouseDeltaX/Y()`，`OnEvent` 累积 `MouseMovedEvent`、`BeginFrame` 清零。
- 光标模式控制 - `Window::SetCursorMode(CursorMode::{Normal,Hidden,Disabled})` / `GetCursorMode()`。
- raw mouse motion - `Window::SetRawMouseMotion(bool)` / `IsRawMouseMotion()`（仅 `Disabled` 下生效）。

**现状：** `Window` 已有 `SetCursorMode`/`GetCursorMode`/`SetRawMouseMotion`/`IsRawMouseMotion` + `CursorMode` 枚举；`Input` 已有 `GetMouseDeltaX/Y` + 完整手柄轮询；`GLFWInput` 全部实现。光标模式与 raw motion 解耦，FPS 锁鼠需二者组合，由调用方决定，引擎不施加策略。

### D4. 事件即时分发无缓冲（现状可接受）

事件在 GLFW 回调中即时分发到层，若层在处理中修改状态可能引发重入。部分引擎采用"收集到队列、在帧内固定点统一处理"的模式。当前规模下可接受，场景图/ECS 引入后建议评估。

### D5. 多线程缺失（仍开放）

`LayerType::Core` 注释提到"线程管理"，但无 Job System / 线程池。资源加载（尤其异步纹理上传）会受益于后台线程。可在资源管理子系统落地后引入。

---

## E. 面向未来的演进建议（2026-07-20 新增）

前四节（A/B/C/D）聚焦「补齐现有框架缺陷」。随着 Vulkan 后端、`Material`、`RenderCommand` 等落地，引擎已越过「能画一个三角形」阶段。本节给出向「可用引擎」演进的方向性建议，按收益/紧迫度排序；每条均标注与现有开放项的承接关系。

### E1. 双后端抽象的收敛与一致性（高优先 · 技术债，承接 D2 残留）

Vulkan 后端通过 GLSL 重写 + UBO 合成 + 管线状态缓存来「镜像」OpenGL 语义，是聪明但脆弱的桥接：

- `Material`/`Shader` 的 uniform 仍以「按名字 `SetMat4`」为主--OpenGL 天然支持、Vulkan 靠源码改写模拟；一旦 shader 用了 UBO block / SSBO / push constant / descriptor array，重写路径会失配。
- 每绘制管线的 `(shader, vertex layout, blend, depth, cull)` 缓存键随状态维度增长易膨胀。
- `Renderer::BeginScene` 内硬编码了 `if (GetAPI() == Vulkan) flipY`--后端差异开始渗入场景层。

**建议：**

- 中期把 uniform 上传从「按名查找」迁到「反射 + 显式 binding/offset」：Vulkan 侧已有 shaderc，加 `SpvReflect`（或 `spirv-cross`）产出统一 `ShaderReflection`（uniforms/blocks/samplers + offsets/bindings），OpenGL 与 Vulkan 共用同一份反射数据驱动 `Material` 上传，消除 GLSL 改写。
- 把 Y 翻转 / frontFace 等后端差异收敛进 `RendererAPI`（投影矩阵修正、面剔除朝向）而非 `Renderer::BeginScene`，保持 `Renderer` 后端无关。
- 抽象 `RenderPass`/`Attachment` 概念（见 E5），让 Vulkan 的 dynamic-rendering pass 与 OpenGL 默认 framebuffer 统一描述，减少 `BeginScene`/`SwapBuffers` 里散落的后端 if。

### E2. 测试与可验证性（高优先 · 工程化）

当前仓库**无任何测试**（无 `tests/`、无 catch2/gtest、无 CI 配置）。随子系统增多，回归风险随每次后端改动指数上升--尤其双后端下，OpenGL 改一行可能在 Vulkan 静默踩雷。

**建议：**

- 引入轻量单测框架（Catch2 或 doctest，header-only 与现有依赖风格一致），先覆盖纯逻辑层：`BufferLayout` 偏移/stride 计算、`Timestep` 钳制、`EventDispatcher`、`LayerStack` 插入顺序、`OrthographicCamera`/`PerspectiveCamera` 投影矩阵数值、`Material` uniform 覆盖优先级。
- 渲染后端用「GL 错误计数断言」或「截图基线」做冒烟（复杂，可后期）。
- 配 CI（GitHub Actions 或本地脚本）跑 `cmake --build` + 单测，至少保证两后端都能编译通过。这是引擎对外交付（`find_package`）的前置信任基线。

### E3. 资源/资产管理（承接 C2，需先定语义再写代码）

`ShaderLibrary`（commit `fac8db3`）当前仍存在于 `Shader.h`（与 `Shader` 同头文件），**此前「曾移除」表述系误判**（2026-07-26 核验：`git log -S` 仅有添加提交、无移除）。但它与 `Shader` 同头文件耦合、未纳入统一资源体系，仍属「先实现后定语义」。重写前应先确定：

- handle 还是路径键？强引用还是弱缓存 + 引用计数？
- 热重载粒度（文件 mtime 监听 vs 手动 `Reload(path)`）？
- 跨后端：同一份 shader 源在 OpenGL 走原生编译、Vulkan 走 shaderc->SPIR-V，`AssetManager` 是否持两份编译产物？

**建议：** 先写一页 `Asset` 设计文档（类型枚举、handle 生成、生命周期、热重载策略），再落地 `AssetManager` + `AssetHandle` + 按类型的 `Loader`。`ShaderLibrary` 作为 `AssetManager` 的 shader 特化子集回归，而非独立类。与 E1 的反射方案协同：shader 资产 = 源 + 反射元数据 + 编译产物。

### E4. 场景图 + Transform + ECS（承接 C3，引擎核心）

无实体/组件/场景组织是当前最大的功能缺口--所有可绘制状态散在 `Layer` 内手搓（见 `game/src/main.cpp` 的 `TestLayer`）。这是渲染基础设施完成后自然的下一步，也是编辑器、序列化、脚本的前置。

**建议落地顺序（小步可验证）：**

1. `Transform`（local + world `mat4`，父子层级，脏标记 + `GetWorldMatrix()` 递归重算）--独立可单测。
2. `Entity`/`Component`/`System` 三件套，选 sparse-set（迭代快、缓存友好）而非 archetype（实现复杂）。最小组件集：`TransformComponent`、`MeshComponent`（持 `VertexArray`+`Material`）、`CameraComponent`。
3. `Scene` 持有 `Registry` + 活动相机 + `OnUpdate`/`OnRender`；`Renderer::Submit` 改接受 entity 的 transform。
4. 固定步长累加器（C4 残留子项）随物理/动画系统一起引入。

### E5. Framebuffer + RenderPass + 后处理（承接 B3）

离屏渲染是编辑器 viewport、阴影贴图、后处理（Bloom/tonemap）的共同前置。当前 `BeginScene`/`EndScene`/`Flush` 仍是空占位，正好借 Framebuffer 落地把它们填实。

**建议：**

- `Renderer/Framebuffer.h` + 规格（尺寸、颜色/深度附件格式、采样数）+ `OpenGLFramebuffer` + `VulkanFramebuffer`（Vulkan 侧即 dynamic-rendering 的 attachment 描述）。
- 抽象 `RenderPass` 概念统一两后端：OpenGL = 默认 FBO + clear，Vulkan = `VkRenderingInfo`。`Renderer::BeginScene` 接受「渲染到哪个 RenderPass/Framebuffer」，`EndScene`/`Flush` 负责解析/屏障。
- 视口 resize 时 resize 活动 framebuffer（复用 A3 的 `OnViewportResize` 机制）。

### E6. 序列化与场景文件

有了 ECS（E4）后场景需要可持久化。这与「游戏工程 `find_package` 引用引擎」的目标强相关--游戏侧需要定义场景、引擎需要加载场景。

**建议：** 选定格式（YAML 可读性好、编辑器友好；或 JSON），先序列化 `TransformComponent` + `MeshComponent`（mesh 路径 + material 参数），随组件扩展逐步纳入。编辑器（E7）的保存/加载即建立在此之上。

### E7. 编辑器层（ImGui 面板）

`ImGuiLayer` 已完整集成但无任何面板。下一步建一个 `EditorLayer`：

- Scene viewport：把主场景渲染到 `Framebuffer`（E5），用 ImGui `Image` 显示，处理 viewport 内的鼠标拾取/相机（复用已落地的 `EditorCameraController`）。
- Hierarchy / Inspector / Console 面板，驱动 ECS（E4）+ 序列化（E6）。
- 这是把「引擎」从「库」变成「可用工具」的关键一跃，也最能反向暴露抽象缺口。

### E8. 其余子系统（按需 · 长尾）

- **音频**：`Audio` 抽象 + `OpenAL`/`miniaudio` 后端，资源走 `AssetManager`（E3）。
- **物理**：先 2D（Box2D）后 3D（Jolt/PhysX），固定步长随此引入。
- **脚本**：C#（Mono/CoreCLR）或 Lua/angelscript，绑定 ECS 接口。
- **Job System / 线程池**（D5）：资源异步加载（E3）落地后引入收益最大。
- **性能分析器**（D1）：`InstrumentationTimer` + Chrome tracing JSON，主循环五阶段插桩。

---

## 建议推进顺序（更新版）

| 阶段 | 内容 | 状态 | 预估 | 收益 |
|---|---|---|---|---|
| 1 | 补 `DMGameEngine.h` 四个 include + 同步 `ENGINE_SUMMARY.md` | ✅ 已完成 | - | - |
| 2 | `Camera` 派生类 + `BeginScene` 接受相机数据 | ✅ 已完成 | - | - |
| 3 | `ImGuiLayer` | ✅ 已完成 | - | - |
| 4 | **A3** resize 同步活动相机宽高比 | ✅ 已完成 | - | - |
| 5 | `Timestep` 钳制 ✅ + `Transform`/ECS | 部分完成 | 大 | 场景组织与物理稳定性 |
| 6 | `Framebuffer`（B3 开放）+ 管线状态可配置（B4 ✅） | 部分完成 | 中 | 编辑器 viewport 与复杂材质铺路 |
| 7 | `AssetManager`（`ShaderLibrary` 已存在、待整合） | 开放（见 E3） | 中大 | 资源体系，与对外交付目标对齐 |
| 8 | 性能分析器 / 输入扩展✅ / 多线程 | 输入扩展已完成，余开放 | 中 | 健壮性与开发体验 |
| 9 | **D2** API 切换 ✅ + Vulkan 后端 ✅ | ✅ 已完成 | - | 双后端能力 |
| 10 | **E1** 双后端抽象收敛（反射 + RenderPass 统一） | 新增·开放 | 中大 | 降低 OpenGL-first 渗漏、长期可维护 |
| 11 | **E2** 测试 + CI | ✅ 已落地（骨架 + 5 纯逻辑测试；运行时冒烟待补） | 中 | 回归防护、对外交付信任基线 |
| 12 | **E4** Transform + ECS + Scene | 新增·开放 | 大 | 引擎核心，编辑器/序列化前置 |
| 13 | **E5** Framebuffer + RenderPass + 后处理 | 新增·开放 | 中大 | viewport/阴影/后处理 |
| 14 | **E6** 序列化 + 场景文件 | 新增·开放 | 中 | 场景持久化 |
| 15 | **E7** 编辑器层（ImGui 面板） | 新增·开放 | 中大 | 引擎->可用工具 |

---

## 附：核验过的源码事实（2026-07-20 复核）

| 文件 / 条目 | 状态 |
|---|---|
| `DMGameEngine.h` | ✅ 已含 `VertexArray`/`VertexBuffer`/`IndexBuffer`/`Texture` + 相机三派生 + `ImGuiLayer` + `Timestep` |
| `ENGINE_SUMMARY.md` | ✅ 已同步，含逐日变更日志（最后更新 2026-07-19） |
| `Renderer/Camera.h` | ✅ 基类：projection + view + `GetViewProjection()` |
| `Renderer/OrthographicCamera.h` | ✅ 存在，位置/旋转(度)->view |
| `Renderer/PerspectiveCamera.h` | ✅ 存在，position/target/up->`glm::lookAt` view |
| `Scene/SceneCamera.h` | ✅ 存在，运行时切换正交/透视 |
| `Renderer.cpp::BeginScene` | ✅ 有 `(const Camera&)` 重载缓存 VP；`EndScene`/`Flush` 仍空占位 |
| `Renderer.cpp::Submit` | ✅ 上传 `u_ViewProjection` + `u_Transform` |
| `Platform/OpenGL/OpenGLDebug.h` | ✅ `DMGE_GL_CALL` 宏，Debug 全覆盖 |
| `OpenGLRendererAPI::Init` | ✅ 已可配置：继承基类 `RendererAPI::Init(config)` 模板方法，按 `RendererAPIInitConfig` 应用初始管线状态 |
| `ImGui/ImGuiLayer.h/.cpp` | ✅ 完整集成，Stage 4 实工作 |
| `RendererAPI` 接口 | ✅ 已补齐 `SetBlendState`/`SetBlendEquation`/`SetDepthTest`/`SetDepthFunc`/`SetCullMode` + 可配置 `Init(config)` |
| `Application.cpp::OnEvent` | ✅ resize 调 `Renderer::OnWindowResize` + `m_ActiveCamera->OnViewportResize()` 同步宽高比（A3 已修复） |
| `Application.cpp::MainLoop` | ✅ `deltaTime` 经 `std::clamp` 钳制上限 0.1s，包进 `Timestep` 后分发至 `OnUpdate` |
| `Renderer/Framebuffer.h` | ❌ 不存在 |
| `Asset` / `ECS` 目录 | ❌ 不存在 |
| `Window` 接口 | ✅ 有 `SetCursorMode`/`GetCursorMode`/`SetRawMouseMotion`/`IsRawMouseMotion` + `CursorMode` 枚举 |
| `Input` 接口 | ✅ 有 `GetMouseDeltaX/Y` + 手柄轮询（5 方法）+ 热插拔事件 |
| `Platform/Vulkan/*` | ✅ 完整后端：Device/Swapchain/Context/RendererAPI/Shader/Buffers/VertexArray/Texture{2D,2DArray,Cube}，`-DDMGE_VULKAN_BACKEND=ON` |
| `Renderer/RenderCommand.{h,cpp}` | ✅ 三层抽象中段：静态门面持后端实例，转发 clear/viewport/blend/depth/cull/draw |
| `Renderer/Material.{h,cpp}` | ✅ Material + MaterialInstance，`variant` uniform 包，`Submit` material 重载 |
| `Renderer/Texture{2D,2DArray,Cube}` | ✅ 三派生 + 规格，不可变存储 |
| `Renderer/CameraController.h` + `Scene/{Orthographic,Editor}CameraController.h` | ✅ 控制器基类 + 2D/3D，输入驱动相机 |
| `Renderer::SetAPI` / `Renderer::API` | ✅ 枚举含 OpenGL/Vulkan/DirectX/None；OpenGL+Vulkan 后端落地，DirectX 为桩 |
| `ShaderLibrary` | ✅ 仍存在于 `Shader.h`（与 `Shader` 同头文件，第 167 行）+ 完整实现在 `Shader.cpp`（第 83-120 行），经 `DMGameEngine.h` 第 42 行 include 暴露为公共 API；**此前「曾移除」表述系误判**（2026-07-26 核验更正）。待 E3 整合进统一 `AssetManager` |
| 测试 / CI | ❌ 仓库无任何测试与 CI（见 E2） |
