# DMGameEditor - 构建路线与设计思路

> ⚠️ **状态更新（2026-10-05，docs-agent 核验）**：本文件写于阶段 1（2026-08-03），下文"当前架构 / 已落地功能 / 路线图"各节反映的是当时快照。**阶段 3/4 规划内容此后已实际落地**——多标签 + 全局单 Play、Prefab、导出可运行工程、Play 快照隔离（`SceneDuplicator` 深拷贝 + Ref 共享）等均已合并（明细见 `TASKS.md` 已合并区，以及本文件 §7 之后由集成提交沉淀的演进记录）。§3.1 目录结构也已增长，现状见该节末尾的"目录补录（2026-10-05）"。**正文按工程档案"只增不毁"约定保持原样**，阅读时以补录与 TASKS.md 为准。

> 创建日期：2026-08-03
> 评估基准：engine 截至 2026-08-03（ECS + 序列化 + Asset/assimp + 光照 + 实例化渲染）+ editor 首版落地
> 目的：记录独立编辑器工程的**架构决策演进、当前状态、后续路线**，让思路可追溯

---

## 0. 为什么是独立工程（第一版教训）

### 0.1 第一版做法（已被废弃并清理）
最初把 `EditorLayer` 直接塞进**引擎 DLL 内部**，由 `Application::Initialize` 在 `DMGE_BUILD_EDITOR` 下自动 `PushOverlay`。`OnAttach` 时遍历 LayerStack，"adopt" demo 的 `DefaultSceneLayer` 的 Scene + CameraController，渲染到离屏 viewport。

### 0.2 暴露的三个根本问题
1. **寄生而非独立**：编辑器只是叠在 demo 上的 ImGui 面板，挡住 demo 画面，又拿不到 demo 的真实编辑能力。
2. **无法真正编辑**：不能独立建 Entity / 加 Component / 管 System，因为场景归属 demo，编辑器只是"借看"。
3. **耦合引擎**：编辑器代码进 DLL，发布游戏时要么带着死代码，要么靠编译开关裁剪--与"引擎保持纯净"冲突。

### 0.3 结论
编辑器必须是**独立的可执行工程**，链接 `DMGameEngine` DLL，只消费引擎公共 API；自己创建并拥有 Scene，独立于任何 demo。引擎一行编辑器代码都不该有。

> 已执行清理：第一版的 `EditorLayer` 从引擎完全移除（工作区干净，仅 git index 残留待 `git reset` 清理）。

---

## 1. 设计目标

| 目标 | 含义 |
|---|---|
| 独立可运行 | `DMGameEditor.exe` 自带窗口，不依赖任何 demo 才能启动 |
| 独立拥有场景 | 自建 Scene + 相机 + 离屏目标，不"借用"别的工程的数据 |
| 可编辑场景 | 建/删 Entity、编辑 Component、注册/查看 System、save/load `.scene` |
| 可运行场景 | Play/Pause 运行场景模拟（首版）；导出独立可运行工程（后续） |
| 引擎零侵入 | 引擎不改核心逻辑，只为"工具化"开必要的只读/标识接口 |

---

## 2. 关键架构决策

### 2.1 独立 exe，消费引擎 DLL
- `editor/` 作为 top-level CMake 的 `add_subdirectory`，`target_link_libraries(DMGameEditor PRIVATE DMGameEngine)`。
- 编辑器只 `#include <DMGameEngine/DMGameEngine.h>`，通过引擎公共 API 操作 ECS/序列化/Asset/渲染。
- POST_BUILD 拷贝 `DMGameEngine.dll` 到编辑器运行目录。

### 2.2 ImGui 跨 DLL（本次最难的问题）

**问题本质**：ImGui 用全局 `GImGui` 指针持有当前 context。如果 exe 和 dll **各编译一份** ImGui 源码，会产生两个 `GImGui`：
- 引擎 `ImGuiLayer`（在 DLL）的 `NewFrame`/`Render` 操作 DLL 的 context
- 编辑器（在 exe）的 `ImGui::Begin` 操作 exe 的 context
- 两者不同步 -> 面板不显示或崩溃

所以 ImGui **只能编译一份**，且 context 必须单一。

**尝试 A：`WINDOWS_EXPORT_ALL_SYMBOLS=ON`（失败）**
- 思路：让引擎 DLL 导出所有符号（含 ImGui），编辑器直接链接 DLL 的 ImGui。
- 结果：`exports.def` 因 shaderc/glslang 静态库的 PCH 符号报 `LNK2001: unresolved external symbol __` + 大量 `LNK4002`。全量导出与第三方静态库的预编译头符号冲突，链不通。
- 弃用。

**尝试 B：编辑器自己编译 ImGui（不可行）**
- 会产生两份 `GImGui`，见上。直接排除。

**最终方案：`IMGUI_API` 宏定向导出（成功）**
- 引擎编译 ImGui 5 个核心源（`imgui.cpp`/`imgui_draw`/`imgui_tables`/`imgui_widgets`/`imgui_demo`）时加 `IMGUI_API=__declspec(dllexport)`：
  ```cmake
  set_source_files_properties(
        ".../imgui/imgui.cpp" ... ".../imgui_demo.cpp"
        PROPERTIES COMPILE_DEFINITIONS "IMGUI_API=__declspec(dllexport)")
  ```
- 编辑器侧加 `target_compile_definitions(DMGameEditor PRIVATE "IMGUI_API=__declspec(dllimport)")`，并 `target_include_directories` 指向 `engine/dependencies/imgui` 拿头文件。
- 效果：ImGui 函数从 DLL 导出，编辑器 dllimport 复用**DLL 里唯一的 context**。`GImGui` 只在 DLL 一份。
- ImGui backend（`imgui_impl_glfw/opengl3`）不导出（编辑器不直接用 backend，由引擎 `ImGuiLayer` 驱动）。

> 这是"只导出需要的符号"而非"全量导出"，避开了 shaderc PCH 的雷。

### 2.3 引擎侧最小让步（3 处，+22 行）
| 文件 | 改动 | 理由 |
|---|---|---|
| `CMakeLists.txt` | ImGui 源 `IMGUI_API=dllexport` | 2.2 的落地 |
| `Scene/Scene.h` | `GetSystems()` 只读 accessor | 编辑器列出已注册 System |
| `Scene/Systems/System.h` | `virtual const char* GetName()` | 工具显示 System 名称 |
| 3 个 System 子类 | `override` GetName | 具体名称 |

原则：**不改核心逻辑**（渲染/ECS/序列化/Asset 零改动），只开"工具可见性"的口子。`GetName` 是新增虚函数（有默认实现，不影响现有 System），`GetSystems` 是只读 accessor。

---

## 3. 当前架构

### 3.1 目录结构
```
editor/
├── CMakeLists.txt              # 链接 DMGameEngine + IMGUI_API dllimport + imgui/glm include
└── src/
    ├── main.cpp                # CreateApplication -> OpenGL
    ├── EditorApplication.h/.cpp  # Application 子类：建窗口、push EditorLayer
    ├── EditorScene.h/.cpp        # 独立拥有 Scene + EditorCamera + 离屏 FB + play/pause + 默认场景
    └── EditorLayer.h/.cpp        # Tool overlay：全部面板逻辑
```

> **目录补录（2026-10-05，docs-agent 核验）**：上图为阶段 1 快照。当前 `editor/src/` 另有：
> `SceneDuplicator.h/.cpp`（Play 快照与 Prefab 共用的实体子树深拷贝）、
> `ProjectExporter.h/.cpp`（导出可运行工程：资产收集 + 引擎 add_subdirectory 模板，见 KB-07 K-015 论证）、
> `LogPanel.h/.cpp`（spdlog sink 日志面板，见 §7.3）。

### 3.2 数据流
```
EditorLayer::OnRender
  └─ EditorScene::Render
       └─ Renderer::BeginScene(cam, offscreenFB)   // 离屏 pass，含 bind+clear
            └─ RenderCommand::SetViewport(FB尺寸)   // Bind 不设 viewport，手动设
            └─ Scene::OnRender                       // MeshRenderSystem 提交 draw
       └─ Renderer::EndScene

EditorLayer::OnImGuiRender
  └─ DrawViewport: FB->GetColorAttachment(0)->GetRendererID()  // OpenGL 纹理 ID
       └─ ImGui::Image(texID, size, uv(0,1),(1,0))            // Y 翻转（GL 纹理原点左下）
```

### 3.3 与引擎的关系
- 编辑器是引擎的**消费者**，不是子模块。
- 引擎 `Application` 的 5 阶段主循环驱动：Update -> Render(编辑器渲染离屏) -> ImGui(编辑器画面板) -> Swap。
- 引擎 `ImGuiLayer`（在 DLL）提供 ImGui context + `Begin/End`，编辑器 `OnImGuiRender` 画面板。
- 编辑器相机用引擎的 `EditorCameraController`（orbit/pan/zoom）。

---

## 4. 已落地功能（MVP，阶段 1）

| 功能 | 状态 |
|---|---|
| 独立 exe 运行 | ✅ `DMGameEditor.exe` 编译链接通过 |
| 自建 Scene + 默认场景（cube + 平行光） | ✅ |
| Viewport（离屏 FB -> ImGui::Image，尺寸自适应，焦点驱动相机） | ✅ |
| Scene Hierarchy（Entity 树 + 父子层级 + 点击选中） | ✅ |
| Inspector（Tag/Transform/Camera/Light/Mesh 编辑） | ✅ |
| Entity CRUD（菜单 Create Empty / Delete） | ✅ |
| Systems 面板（列出 System + 名称 + Play/Pause/Stop） | ✅ |
| save/load `.scene`（复用 `SceneSerializer`） | ✅ |
| Play/Pause 运行场景模拟 | ✅ |

---

## 5. 路线图（后续）

### 阶段 2：交互增强（推荐先做）
- **ImGuizmo**：引入 `editor/dependencies/ImGuizmo`，viewport gizmo（translate/rotate/scale）。这是"选中即操作"的关键，当前只能拖 Inspector 数值。
- **Mouse pick**：点击 viewport 选中 entity（raycast 或 entity-ID attachment）。当前只能 Hierarchy 点选。
- **Component 动态添加**：Inspector 的 "Add Component" 下拉，运行时挂/卸 Component。

> 状态标注（2026-10-03）：阶段 2 三项均已实现（gizmo + raycast 拾取 + Add/Remove Component，见 EditorLayer.cpp），roadmap 文字此前未同步。

### 阶段 3：内容能力
- **Asset Browser**：缩略图网格，拖入 MeshComponent；浏览/加载 `engine/shaders`、模型、材质。
- **多 Scene / Scene 标签页**：同时编辑多个场景。
- **Prefab / Entity 预制件**：保存/实例化 Entity 模板。

> 状态标注（2026-10-03）：Asset Browser 拖拽建实体（拖到 Viewport/Hierarchy）+ 类型图标 ✅（缩略图渲染未做）；Scene 管理（New 确认/Open/Save/Save As/最近文件持久化 editor_config.ini）✅；Prefab 最小版（Hierarchy 右键 Save As Prefab / Instantiate，临时 Scene + SceneSerializer，引擎零改动）✅。多 Scene 标签页 ✅（2026-10-03 第二波：`SceneTab` + `EditorScene` 多标签管理，顶部 "Scenes" TabBar（dock 在 Viewport 上方，旧 imgui.ini 需删除才有新默认布局），每 tab 独立选中/相机/Play 状态，同一时刻仅一个场景可 Play，后台 Play 的场景模拟继续 tick 不渲染，File 菜单语义适配多标签（New/Open 开新标签、Save 作用当前标签、Close Tab 未保存确认弹窗），未保存以 `*` 标注（Dirty 跟踪覆盖 CRUD/gizmo/Inspector/拖拽/Prefab）。

### 阶段 4：工程化（你说的"可创建可运行工程"）
- **导出可运行工程**：编辑器生成一个引用 `DMGameEngine` 的最小 game 工程（CMakeLists + main + 加载 `.scene`），脱离编辑器独立运行。
- **Play mode 增强**：play 时隔离编辑（不可改 Transform 等），保证一致性；stop 后恢复编辑态。
- **编辑器配置持久化**：窗口布局 / 最近场景 / 设置（`editor.ini`）。

> 状态标注（2026-10-03）：Play mode 隔离 ✅（编辑态/运行态双 Scene + SceneDuplicator 深拷贝，Stop 恢复快照，选中按 UUID 回映射；多标签下按场景生效，全局单 runtime 槽位）；最近场景配置持久化 ✅（editor_config.ini）。导出可运行工程 ✅（2026-10-03 第二波：`editor/src/ProjectExporter.{h,cpp}` + File > Export Runnable Project... 对话框——生成 CMakeLists/main.cpp/README/assets（场景 JSON + UUID registry + 引用模型拷贝 + shaders 目录）；因引擎 install(EXPORT) 无法生成、无 Config 文件（KB-07 K-014），find_package 不可用，模板走 `add_subdirectory(DMGE_ENGINE_DIR)` 由用户填引擎路径；导出不自动执行构建，人工验证步骤见 agent 报告）。

> 状态标注（2026-10-05）：菜单快捷键真键盘处理 ✅（Ctrl+N/S/O、Ctrl+Shift+A、Del、F5/F6——`EditorLayer::ProcessShortcuts` ImGui 轮询，WantTextInput/模态弹窗让路，Play 模式禁用编辑类快捷键，Alt+F4 交系统；菜单与快捷键共用 action helper）。动画预览 UI ✅（DMGE_ANIMATION=ON：RegisterSystems 成对注册 AnimationSystem+SkinnedMeshRenderSystem、Inspector Animator 段（Skeleton/Clip 显示与切换 + UUID 追加 + 播放控制 + scrub）、SceneDuplicator 补拷 AnimatorComponent；编辑态显示 CurrentTime 静止姿势，自动播放仅 Play 副本，详见 KB-07 K-026/K-027/K-028）。

### 阶段 5：质量
- Vulkan 后端适配：viewport 的 `ImGui::Image` 当前用 OpenGL 纹理 ID；切 Vulkan 时 `ImTextureID` 是 `VkDescriptorSet`，需后端适配。
- 单测 / CI：编辑器无测试，后续补 headless 冒烟。

---

## 6. 技术备忘

### 6.1 ImGui 跨 DLL 最终方案（务必保留）
- 引擎：`set_source_files_properties(...imgui 5 源... COMPILE_DEFINITIONS "IMGUI_API=__declspec(dllexport)")`
- 编辑器：`target_compile_definitions(DMGameEditor PRIVATE "IMGUI_API=__declspec(dllimport)")` + `target_include_directories(... engine/dependencies/imgui .../backends)`
- **不要**回退到 `WINDOWS_EXPORT_ALL_SYMBOLS`（shaderc PCH 符号致 LNK2001）。
- **不要**让编辑器自己编译 ImGui（双 context 崩溃）。

### 6.2 离屏 viewport 三要点
1. `Renderer::BeginScene(cam, target)` 内部 `BeginRenderPass` 会 bind FBO + `glClear`（无拖影）。
2. `OpenGLFrameBuffer::Bind()` **不设 glViewport**，必须手动 `RenderCommand::SetViewport(0,0,fbW,fbH)`，否则被窗口 viewport 裁剪。
3. `ImGui::Image` UV 用 `(0,1)->(1,0)` 翻 Y（GL 纹理原点左下）。

### 6.3 编码 / 行尾
- 新文件用 `[IO.File]::WriteAllText(..., UTF8 无 BOM)`。
- `git show HEAD:path` 还原文件会写 LF；引擎 `core.autocrlf=true`，git status 可能短暂报 M（行尾假阳性），`git diff --ignore-cr-at-eol` 为空即内容一致。

---

## 7. 阶段 1 增量：Unity 风格布局 + Asset/Log 面板

MVP 之后、阶段 2 之前补齐"形似主流商业引擎"的界面体验。

### 7.1 Unity 风格默认布局（DockBuilder）
- `#include <imgui_internal.h>`（DockBuilder API 在 internal 头，非 imgui.h）
- `DrawDockspace` 首次启动用 `ImGui::DockBuilder*` 建固定布局：
  - 左 18% Scene Hierarchy
  - 中 Viewport
  - 右 24% Inspector + Systems（同 tab）
  - 下 28% Asset Browser + Log（同 tab）
- 之后 ImGui 把布局持久化到 `imgui.ini`，用户可自由拖拽重排。
- **改默认布局需删除 `imgui.ini`**（DockBuilder 只在 node 为空时建）。

### 7.2 Asset Browser（基础版）
- 浏览 `engine/shaders` + `editor/assets`，点击选中显示路径。
- 完整版（缩略图、拖入 MeshComponent、材质预览）属阶段 3。

### 7.3 Log 面板（spdlog sink）
- 新增 `LogPanel`（`src/LogPanel.h/.cpp`）：自定义 `spdlog::sinks::base_sink<std::mutex>` 挂到引擎 core + client logger。
- `DMGE_LOG_*` / `DMGE_CLIENT_*` 实时进面板，按级别着色（trace 灰 / info 白 / warn 黄 / error 红 / critical 粉）。
- **跨 DLL**：sink 对象在 exe，引擎 logger（DLL）经虚函数调 `sink_it_`；同 CRT（`/MDd`）使 `vector::push_back` 跨 DLL 安全。`OnDetach` 从 logger sinks 移除。
- dock 到下方（与 Asset Browser 同 tab）。

### 7.4 踩过的坑（备忘）
- CMake 加 spdlog include 时，`glm-master)` 末尾的 `)` 提前闭合 `target_include_directories` -> CMake parse error。加 include 到末尾参数需去掉前一项的 `)`。
- `EditorLayer.h` 重复声明（一次失败的命令已改过 .h，第二次又加）-> `C2535/C2086`。重写 .h 修复。

## 附：引擎侧改动清单（已提交为一次 commit）
- `CMakeLists.txt`：ImGui 源 `IMGUI_API=dllexport`（+10）
- `Scene/Scene.h`：`GetSystems()`（+3）
- `Scene/Systems/System.h`：`GetName()`（+3）
- `Scene/Systems/{Transform,MeshRender,Light}System.h`：override（各 +2）

引擎核心逻辑零改动；demo 不受影响。