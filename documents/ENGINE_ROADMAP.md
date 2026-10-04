# DMGameEngine - 发展路线图

> 创建日期：2026-07-26
> 评估基准：`ENGINE_SUMMARY.md`（截至 2026-07-25）+ `ENGINE_REVIEW.md`（2026-07-20）+ `VulkanBackendReview.md`（2026-07-21）+ 实际源码核验
> 目的：给出从「学习型渲染引擎」演进到「可用游戏引擎」的分阶段、可执行补全计划，含优先级与依赖关系

---

## 0. 当前完成度快照（2026-07-26）

### 已落地且质量良好

| 子系统 | 状态 | 说明 |
|---|---|---|
| Core 架构 | ✅ | `Application`/`Layer`/`LayerStack` + 5 级 `LayerType` + 5 阶段主循环 |
| 事件系统 | ✅ | `Event`/`EventDispatcher` + 键鼠/应用/手柄热插拔事件 |
| Input | ✅ | 边沿检测 + 手柄 5 方法 + 逐帧鼠标位移 + 光标模式 + raw motion |
| Camera | ✅ | 基类 + 正交/透视/SceneCamera + Controller 层级（2D/3D 轨道） |
| 渲染抽象 | ✅ | 三层 `RendererAPI`/`RenderCommand`/`Renderer`，后端工厂分发 |
| 双后端 | ✅ | OpenGL（默认）+ Vulkan 1.3（可选），`Renderer::SetAPI` 运行时切换 |
| 资源体系 | ✅ | Shader/Texture{2D,Cube,2DArray}/Vertex/IndexBuffer/VertexArray/FrameBuffer |
| Material | ✅ | Material/MaterialInstance + `variant` uniform 包 + Submit 重载 |
| RenderQueue | ✅ | 延迟提交 + 按 material/shader 排序分组 + 实例化绘制 |
| 离屏渲染 | ✅ | `BeginRenderPass`/`EndRenderPass` + `BeginScene(cam, target)` 多 pass RTT |
| 管线状态 | ✅ | Blend/Depth/Cull 可配置 + `RendererAPIInitConfig` 模板方法 |
| ImGui | ✅ | 完整集成 + 多视口 + imgui_impl_vulkan |
| Debug 工具 | ✅ | 自研 Profiler（RAII + 聚合）+ ProfilerLayer（F1）+ Console（反引号）|
| 构建系统 | ✅ | CMake 3.20 + C++23 + 显式源文件 + install 规则 + `find_package` |
| 公共 API | ✅ | `DMGameEngine.h` + `DMGE_API` + `DM::Scope/Ref` 智能指针别名 |

### 完全缺失的关键子系统

> 快照原评于 2026-07-26；下方状态已随阶段 1 落地（AssetManager / ECS / 序列化，见 `ENGINE_SUMMARY.md` 2026-07-31）与 assimp 网格导入（2026-08-01）更新。

| 子系统 | 状态 | 影响 / 现状 |
|---|---|---|
| **ECS / 场景图** | ✅ 已落地（1b） | `Scene`/`Entity`/`Components`/`Systems` + Transform 层级（dirty 传播/剪枝/环检测）--不再是缺口 |
| **AssetManager** | ✅ 已落地（1a）+ 模型导入 | UUID 注册表 + 去重缓存 + `.mat`/`.mesh` 加载；2026-08-01 接入 assimp，支持 `.fbx/.obj/.gltf/.glb` -> `Mesh`（节点烘焙/超集布局/`.mat` 材质映射）。**异步加载/热重载仍缺** |
| **序列化 / 场景文件** | ✅ 已落地（1c） | `SceneSerializer` + `.scene` JSON 往返（含 MaterialOverrides） |
| **测试 / CI** | 🟡 部分 | GoogleTest + `dmge_tests` 已有（含 assimp headless 测试）；**CI 仍缺** |
| **编辑器层** | ❌ | ImGuiLayer 存在但无编辑器面板，引擎非「可用工具」 |
| **音频** | ❌ | 无 Audio 抽象 |
| **物理** | ❌ | 无 Physics 抽象；`dependencies/box3d-main` 已下载未集成 |
| **脚本** | ❌ | 无脚本绑定 |

### 技术债（待修）

- **Vulkan 后端正确性**：pipeline 缓存不随 swapchain 重建清空（A）、无帧内延迟销毁（B use-after-free）、`RequestResize` 死代码（C）、`VK_CHECK` release no-op（D）、反射式 UBO 覆盖不全（E）、未绑定 sampler 槽留空（F）。
- **Vulkan 后端性能**：descriptor 逐 draw 分配（P0）、`ImmediateSubmit` 全 stall（P0）、VertexArray 每 draw 堆分配（P1）、PipelineCache 不落盘（P1）等。
- **双后端抽象收敛**：OpenGL-first 渗漏（E1），需反射 + RenderPass 统一。
- **assimp 网格导入局限**（2026-08-01 引入）：仅静态网格（无骨骼/动画/形变目标）；同步加载（无异步/后台解码，待 3d Job System）；材质仅外部 `.mat` 查找（不生成材质、不绑纹理--`Material` 尚无纹理绑定）；UV 方向按 GL 翻转纹理加载器假定（未用 `FlipUVs`），个别格式可能需 per-format 调整；`AssetLoader<Material>`/`VertexArray` 特化仍未导出（仅 `Mesh` 已 `DMGE_API`），消费者 exe 直接 `Load<Material/VertexArray>` 仍会 `LNK2019`，需走 DLL 内路径或补导出。

---

## 1. 路线图总览

按「**正确性优先 → 引擎核心 → 工具化 → 内容能力**」四层推进，每层目标明确、可独立交付。

```
阶段 0  技术债与基线加固（防御性，1-2 周）
   │
   ├─ 0a Vulkan 后端正确性修复（A/B/C/D/F）
   ├─ 0b 测试 + CI 骨架（E2）
   └─ 0c Vulkan 性能 P0（descriptor 复用 + ImmediateSubmit 批量化）
   ▼
阶段 1  引擎核心：ECS + 资源管理（2-4 周）
   │
   ├─ 1a AssetManager + ShaderLibrary（E3）
   ├─ 1b Transform + Entity + Component（ECS，E4）
   └─ 1c 序列化 + 场景文件（E6，依赖 1b）
   ▼
阶段 2  渲染与工具化（2-3 周）
   │
   ├─ 2a 编辑器层（E7，依赖 1b/1c）
   ├─ 2b 双后端抽象收敛：反射 + RenderPass 统一（E1）
   └─ 2c Vulkan 性能 P1-P2 + PipelineCache 落盘
   ▼
阶段 3  内容子系统（按需，长尾）
   │
   ├─ 3a 物理：Box3D 集成（已下载，E8）
   ├─ 3b 音频：miniaudio/OpenAL（E8）
   ├─ 3c 脚本：Lua/angelscript 或 C#（E8）
   ├─ 3d 多线程 Job System（D5）
   └─ 3e 后处理 / 阴影 / 高级材质
```

---

## 2. 阶段 0：技术债与基线加固

> 目标：堵住 Vulkan 后端的正确性漏洞，建立回归防护基线，让后续重构「改得动、改得对」。

### 0a. Vulkan 后端正确性修复（最高优先）

> 状态（2026-10-03）：A/C/D/F 已落地（07-26，A 本次补 device idle 加固）+ B 已落地（per-frame deletion queue + 性质单测）；E 留 2b。详见 `VULKAN_FIXES.md` §10，分支 `agent/render-agent/vulkan-correctness`。

依据 `VulkanBackendReview.md`，按风险排序：

| 项 | 问题 | 修复 | 风险/收益 |
|---|---|---|---|
| A | pipeline 缓存不随 swapchain 重建清空 | `RecreateSwapchain` 中 `vkDestroyPipeline` 全部 + `m_Pipelines.clear()` | 高（device lost） |
| B | 无帧内延迟销毁，析构与 in-flight 帧竞争 | 引入 per-frame deletion queue，fence 信号后销毁 | 高（use-after-free） |
| C | `RequestResize` 死代码 | window resize 回调里 `ctx.RequestResize(w,h)` | 中（闪烁/裁剪） |
| D | `VK_CHECK` release no-op | release 至少 `DMGE_LOG_ERROR` + 降级 | 中（静默崩溃） |
| F | 未绑定 sampler 槽 descriptor 留空 | 绑定全局 dummy texture（1×1） | 中（validation/未定义） |
| E | 反射式 UBO（regex + 手算 std140）覆盖不全 | 短期补 `vec2 a,b;`/struct；长期改 SPIRV-Reflect/spirv-cross | 中（编译失败/错位） |

**验证路径**：开 Validation Layer，跑 resize/拖拽跨屏（验 A）、加载纹理中途销毁（验 B）、绑定未设纹理 shader（验 F）。

### 0b. 测试 + CI 骨架（E2）

**目标**：建立回归基线，不追求覆盖率，先让管线能跑。

- 引入 GoogleTest（CMake `FetchContent`），`tests/` 目录，编译开关 `DMGE_BUILD_TESTS`。
- 先补「纯逻辑」单测（零 GPU 依赖）：`Timestep` 钳制、`EventDispatcher` 分发、`Input` 边沿检测、`Material` uniform 包 set/get、`RenderQueue` 排序键、`Profiler` 聚合。
- 增设「渲染冒烟测试」：用 headless 上下文（PBuffer / offscreen FrameBuffer）跑一帧 `Init->BeginScene->Submit->EndScene->Shutdown`，断言不崩溃 + 像素非全黑。
- CI：GitHub Actions 矩阵（MSVC + GCC/Clang），`cmake -DBUILD_TESTING=ON` + `ctest`，Debug + Validation Layer 跑 Vulkan 冒烟。
- 静态分析：clang-tidy / cppcheck 接入 CI。

**为什么现在做**：阶段 1 的 ECS 重构、阶段 2 的抽象收敛都是大改，没有测试网兜底风险极高；`ShaderLibrary` 曾返工就是前车之鉴。

### 0c. Vulkan 性能 P0（可选，与 0a 并行）

> 状态（2026-10-03）：两项均已落地——descriptor set `(shaderID, 纹理句柄 hash)` 缓存复用 + `Begin/EndImmediateBatch` 批量化（旧单次入口保持可用）。量化证据与设计见 `VULKAN_FIXES.md` §11，分支 `agent/render-agent/vulkan-perf-p0`。中期 bindless 仍留 2c。

- descriptor set 复用：对 `(shaderID, 已绑定纹理集合 hash)` 缓存，绑定不变时复用（UBO 是 dynamic，仅改 offset）。
- `ImmediateSubmit` 批量化：一条 CB 录多份 copy + 一次 submit + fence。
- 中期：sampler 改 bindless（`VK_EXT_descriptor_indexing`）。

---

## 3. 阶段 1：引擎核心（ECS + 资源管理）

> 目标：让引擎从「渲染器」变成「引擎」——有场景组织、资源生命周期、数据持久化。

### 1a. AssetManager + ShaderLibrary（E3）

**问题**：`ShaderLibrary` 当前仍存在于 `Shader.h`（与 `Shader` 同头文件，2026-07-26 核验：`git log -S` 仅有添加提交、未移除，原「曾移除」说法系误判），但它是「按名缓存 shader」的独立类，未纳入统一资源体系。当前资源是裸 `Ref<>`，无统一加载/缓存/释放/异步/热重载。

**设计要点**：
- `AssetManager` 单例 + `AssetHandle`（UUID/路径 → 资源），按类型分发 `AssetLoader<T>`。
- 统一加载入口：`AssetManager::LoadAsync<T>(path)` → 后台线程解码 → 主线程上传 GPU（GPU 上传不能跨线程，需 Job System 配合，见 3d）。
- `ShaderLibrary` 作为 `AssetManager` 的 Shader 特化：路径/名字 → `Ref<Shader>`，支持热重载（监听文件 mtime）。
- 资源去重 + 引用计数，`Ref` 归零后入「延迟释放队列」（GPU 资源需等帧完成，对齐 Vulkan 0a-B）。
- 抽象需后端无关：`AssetLoader<Texture2D>` 对 OpenGL/Vulkan 都适用，差异在后端 Texture::Create。

**交付物**：`src/DMGameEngine/Asset/` 目录；示例：从 JSON manifest 加载一批纹理 + shader 并去重。

### 1b. Transform + Entity + Component（ECS，E4）

> 这是整个引擎的「分水岭」——没有 ECS，物理/脚本/编辑器/序列化都无依附点。

**设计要点**：
- 自研轻量 ECS（参考 `entt` 的 registry 思路或直接引入 entt 单头）：`Entity = uint32_t`，`Registry` 持 `Component` 池（`std::unordered_map<type_id, SparseSet>`）。
- 核心 Component：`TransformComponent`（位置/旋转/缩放 + world/local 矩阵 + parent 层级）、`MeshComponent`（`Ref<VertexArray>` + `Ref<Material>`）、`CameraComponent`（包装 SceneCamera）、`TagComponent`。
- `Scene` 类持有 `Registry` + 活动相机 Entity；`OnUpdate` 遍历 transform 计算 world matrix；`OnRender` 遍历 mesh component 调 `Renderer::Submit(material, vertexArray, transform)`。
- 层级 Transform：parent 变更标记 dirty，dirty propagation 到子树，避免每帧全量重算。
- `System` 接口（`OnUpdate(ts)` / `OnRender()` / `OnEvent(e)`），`Scene` 持有 System 列表，替代当前 `DefaultSceneLayer` 的硬编码逻辑。

**依赖**：无（可与 1a 并行，但 1a 的 AssetManager 让 MeshComponent 的材质加载更自然）。

**交付物**：`src/DMGameEngine/Scene/`（新增 ECS + Scene 实现）；`DefaultSceneLayer` 重写为「创建一组 Entity + 注册 System」。

### 1c. 序列化 + 场景文件（E6，依赖 1b）

**设计要点**：
- 选 JSON（nlohmann/json，单头）或 TOML；二进制格式（场景大时）留后。
- 反射是难点：可先用「Component 类型 → 序列化函数」手动注册表（`std::unordered_map<std::string, SerializeFn>`），避免上完整反射系统。
- 序列化 `Entity` = (UUID + tag + 各 component 的 POD 字段)；`TransformComponent` 序列化 local transform（world 运行时算）。
- 资源引用序列化为路径（`AssetHandle`），反序列化时经 `AssetManager::Load` 恢复。
- `Scene::Save(path)` / `Scene::Load(path)`；编辑器（2a）的 save/load 即此封装。

**交付物**：`.scene` 文件格式 + 往返测试（save → load → 字段相等）。

---

## 4. 阶段 2：渲染与工具化

### 2a. 编辑器层（E7，依赖 1b/1c）

**目标**：把引擎变成「可用工具」，反过来暴露抽象缺口。

**设计要点**：
- `EditorLayer`（`LayerType::Tool` overlay）：Gizmo（ImGuizmo，translate/rotate/scale 选 Entity）、Inspector（反射各 Component 字段）、Scene Hierarchy（树状 Entity 列表，支持父子拖拽）、Asset Browser（缩略图网格）、Viewport（离屏 FrameBuffer → ImGui Image，鼠标 pick Entity）。
- 复用 1c 序列化做 save/load 按钮；复用 1a AssetManager 做资产拖入。
- 编辑器与运行时分离：编辑器代码在 `DMGE_BUILD_EDITOR` 开关下，发布游戏时不链接。

**交付物**：基本可用的 2D/3D 场景编辑器，能创建/选中/编辑 Entity 并保存场景。

### 2b. 双后端抽象收敛（E1）

**问题**：当前 OpenGL-first 渗漏——`Renderer::BeginScene` 的 Y 翻转、Vulkan 的 GLSL 重写桥接、逐 draw uniform/material 语义都是把 OpenGL 心智模型硬映射到 Vulkan，导致 Vulkan 侧 descriptor/uniform 抖动严重（见 VulkanBackendReview P0）。

**设计要点**：
- 引入 SPIR-V 反射（SPIRV-Reflect 或 spirv-cross）取代 `VulkanShader` 的 regex + 手算 std140（修 0a-E 的长期方案）：拿真实 offset/binding/set。
- 统一 RenderPass 概念：当前 `BeginRenderPass(FrameBuffer*)` 是雏形，需把「pass 内的 attachment/format/clear/viewport」提升为一等公民的 `RenderPassDesc`，OpenGL/Vulkan 都按它配置（OpenGL 映射到 FBO + glClear，Vulkan 映射到 dynamic rendering attachment）。
- 资源屏障抽象：`Texture::TransitionLayout` 等 Vulkan 专有概念需在更高层包装为 `ResourceBarrier` 通用接口，避免 OpenGL 侧空实现。
- 评估是否引入 RHI 层（Render Hardware Interface）把 `CommandBuffer`/`Pipeline`/`DescriptorSet` 提到后端无关层——若 DirectX 真要落地则必要，若维持双后端可延后。
- **状态标注（2026-10-04）**：DirectX 11 第三后端阶段 A 已由 d3d-agent 落地（`DMGE_D3D11` 开关，默认 OFF）——设备/基础资源/离屏渲染 + headless 冒烟测试（三后端中唯一可全自动 GPU 验证），设计与对齐表见 `DIRECTX_BACKEND_DESIGN.md`；阶段 B（窗口交换链/工厂接线）待启动，其反射方案（D3DReflect per-name uniform 桥接）与 2b 的 SPIR-V 反射方向一致。

**交付物**：去除 `Renderer::BeginScene` 的 Y 翻转 hack；shader 反射统一；RenderPassDesc 落地。

### 2c. Vulkan 性能 P1-P2

> 状态（2026-10-03）：四项全部落地——Bind 零堆分配/零 RTTI、PipelineCache 落盘（头校验 + 损坏回退）、per-frame/一次性 pool 补 TRANSIENT、descriptor pool 超限 WARN + 自动扩容/回收缩（与 P0-1 缓存的失效交互见 `VULKAN_FIXES.md` §12.4）。量化证据（Bind ~200×、pipeline 热建 ~21×）见 §12.5，分支 `agent/render-agent/vulkan-p1p2`。

- `VulkanVertexArray::Bind` 改栈数组/预留容量，去掉每 draw `std::vector` + `dynamic_pointer_cast`。
- `VkPipelineCache` 落盘（`vkGetPipelineCacheData` 序列化，启动加载）。
- per-frame CB pool 改 `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT`。
- descriptor pool 超限告警 + 扩容（当前静默截断）。

---

## 5. 阶段 3：内容子系统（按需，长尾）

### 3a. 物理：Box3D 集成（E8，已下载未集成）

**现状**：`dependencies/box3d-main`（Box3D，erincatto 的 3D 物理引擎）已在 dependencies 但 CMakeLists 未引用。

**步骤**：
1. CMake：`add_subdirectory(dependencies/box3d-main)` + `target_link_libraries(... box3d)`，加 `DMGE_PHYSICS` 开关。
2. `src/DMGameEngine/Physics/`：`PhysicsWorld` 抽象（wrap `b3World`）+ `RigidbodyComponent`/`ColliderComponent`（与 1b ECS 对接）。
3. `PhysicsSystem`：固定步长（accumulator）推进 `b3World::Step`，同步 Transform（physics → render，渲染侧只读）。
4. 2D 物理若需要可加 Box2D（同作者，2D）；Box3D 已含 character mover。

**依赖**：1b ECS（组件挂载点）+ 固定步长随 PhysicsSystem 引入。

### 3b. 音频（E8）

- `Audio` 抽象 + `miniaudio`（单头，零依赖）后端优先于 OpenAL。
- 资源走 `AssetManager`（1a）：`AudioClip` = `Ref<AudioBuffer>`。
- `AudioSourceComponent`（ECS）+ `AudioSystem`。

### 3c. 脚本（E8）

- 优先 Lua（sol2，单头绑定）或 angelscript（C++ 风格语法，易上手）；C#（Mono/CoreCLR）重，按需。
- 绑定 ECS 接口：脚本可读写 Component、订阅事件。
- `ScriptComponent`（ECS）持有 `Ref<Script>` + 实例，`ScriptSystem::OnUpdate` 调脚本钩子。

### 3d. 多线程 Job System（D5）

- 线程池 + 任务图（依赖关系）。
- 落地收益最大点：1a 资源异步加载（后台解码 → 主线程 GPU 上传）。
- 渲染并行化（多线程录制 CB）属 P3，远期。

### 3e. 高级渲染（依赖 2b）

- 后处理管线（bloom/tonemap/SSAO）：基于 `BeginRenderPass` 多 pass + 全屏 quad。
- 阴影贴图：depth-only FrameBuffer（当前离屏路径断言 `GetColorAttachmentCount()>0`，需放开）。
- **可配置延迟渲染（阶段 1）✅ 已落地**（2026-10-04，分支 `agent/render-agent/deferred`）：`Renderer::RenderPath`（默认 Forward，运行时可切）+ G-buffer MRT（RT0 RGBA8 albedo+spec / RT1 RGBA16F 法线+shininess / 深度复用）+ 全屏 quad 光照 pass，OpenGL（`glDrawBuffers`，已有）与 Vulkan（dynamic rendering 多附件 + pipeline 键扩展）双后端；透明回退、RenderPassDesc 统一留 2b/后续。设计与 2b 欠债清单见 `documents/DEFERRED_RENDERING_DESIGN.md`。
- PBR 材质 + IBL + 阴影（待做；依赖 2b 收敛）。

### 3f. 骨骼动画（2026-10-03 立项，TASKS.md 第四波）

- **阶段 1（导入 + Skeleton/Clip 资产 + Animator/AnimationSystem + OpenGL 蒙皮）✅ 已落地**（分支 `agent/anim-agent/skeleton-stage1`，编译开关 `DMGE_ANIMATION` 默认 OFF；Vulkan 蒙皮、动画混合/状态机、编辑器时间轴 UI 留后续波次）。

---

## 6. 推进顺序与依赖矩阵

| 序 | 任务 | 依赖 | 预估 | 优先级 |
|---|---|---|---|---|
| 0a | Vulkan 正确性修复（A/B/C/D/F） | 无 | 中 | 🔴 必做 |
| 0b | 测试 + CI 骨架 | 无 | 中 | 🔴 必做 |
| 0c | Vulkan 性能 P0 | 0a | 中 | 🟠 推荐 |
| 1a | AssetManager + ShaderLibrary | 0b（回归网） | 中大 | 🔴 必做 |
| 1b | Transform + ECS | 0b | 大 | 🔴 必做 |
| 1c | 序列化 + 场景文件 | 1b | 中 | 🟠 推荐 |
| 2a | 编辑器层 | 1b, 1c | 中大 | 🟠 推荐 |
| 2b | 双后端抽象收敛 | 0a, 1b | 中大 | 🟡 可延后 |
| 2c | Vulkan 性能 P1-P2 | 0c | 中 | 🟡 可延后 |
| 3a | 物理 Box3D 集成 | 1b | 中 | 🟢 按需 |
| 3b | 音频 | 1a | 中 | 🟢 按需 |
| 3c | 脚本 | 1b | 中大 | 🟢 按需 |
| 3d | Job System | 1a | 中 | 🟢 按需 |
| 3e | 后处理/阴影/PBR | 2b | 大 | 🟢 远期 |

**关键路径**：`0a/0b → 1b(ECS) → 1c(序列化) → 2a(编辑器)` 是「引擎变可用」的最短路径；其余可并行或按需插入。

---

## 7. 工程化建议（贯穿各阶段）

- **源文件注册**：`CMakeLists.txt` 已改显式列出源文件（非 GLOB），新增 `.cpp/.h` 必须手动加 `set(...)` 条目——各阶段新增目录（Asset/ECS/Physics/...）记得同步。
- **文档同步**：`ENGINE_SUMMARY.md` 的逐日变更日志是好习惯，各阶段落地后补条目；`ENGINE_REVIEW.md` 的「已完成项标 ✅」机制沿用。
- **公共 API 边界**：新模块对外暴露的头加进 `DMGameEngine.h` + `DMGE_API`；内部实现细节不暴露。
- **后端无关优先**：新抽象（Asset/ECS/Physics）写在 `Renderer`/`Platform` 之上，跨后端；后端差异下沉到现有 OpenGL/Vulkan 实现层。
- **小步提交**：每个子任务独立 commit + 单测，避免 `ShaderLibrary` 那种「先实现后定语义」返工重演。

---

## 附：与已有评审的对应关系

- `ENGINE_REVIEW.md` 的 D 节（D1 Profiler ✅、D2 API 切换 ✅、D3 Input ✅、D5 多线程 → 3d）。
- `ENGINE_REVIEW.md` 的 E 节（E1 抽象收敛 → 2b、E2 测试 → 0b、E3 AssetManager → 1a、E4 ECS → 1b、E5 Framebuffer/RenderPass ✅+后处理 → 3e、E6 序列化 → 1c、E7 编辑器 → 2a、E8 其余子系统 → 3a/3b/3c）。
- `VulkanBackendReview.md` 的 A-F 正确性 → 0a、P0-P3 性能 → 0c/2c。
- 新增：Box3D 已下载未集成 → 3a（这是评审文档未覆盖的新事实，2026-07-26 核验发现）。
