# 术语表（Glossary）

> 目标：新人读 guide / 工程档案 / 代码注释时遇到的"项目特有词汇"，在这里能一句话看懂，并拿到**可跳转核验**的代码锚点。
> 每条锚点（路径、类名、常量值）均对照代码逐一 grep 核验（写作时点：2026-10，分支 `docs/glossary`，快照 `2d3fb17`）。代码会演进，以代码为准。
> 深入阅读只链接工程档案（`documents/` 根，只增不毁）与 kb 条目，不在本表复制其内容。

按主题分组：[工程与构建](#工程与构建) · [渲染](#渲染) · [ECS 与场景](#ecs-与场景) · [资产](#资产) · [编辑器](#编辑器)。每组内按字母序。

---

## 工程与构建

### DMGE_API
引擎公共 API 的导出/导入宏：编 DLL 时 `dllexport`、消费 DLL 时 `dllimport`（Windows），静态构建为空。标了它的类/函数才会出现在引擎 DLL 的公共接口上。
- 锚点：`engine/src/DMGameEngine/Core/Export.h`（四个分支的宏定义）
- 深入：`kb/KB-02-DLL边界与STL.md`；红线清单见 `AGENTS.md` §红线

### DM::Ref / DM::Scope
全项目统一的智能指针别名：`DM::Ref<T> = std::shared_ptr<T>`、`DM::Scope<T> = std::unique_ptr<T>`（另有 `WeakRef` 与 `CreateRef/CreateScope` 工厂）。禁止散写 `std::shared_ptr` 拼写（KB-06 风格基线）。
- 锚点：`engine/src/DMGameEngine/Core/Export.h`（`namespace DM`）
- 深入：`kb/KB-06-代码风格与提交规范.md`

### DLL 边界
引擎编为 `DMGameEngine.dll`，game/editor 是两个消费者 exe。跨 DLL 传"含 STL 成员的对象"有运行时崩溃风险——这是历史上真实踩过的坑，STL 容器跨边界修改有严格限制（如 `RenderQueue` 的实例化批次用定长数组而非 vector）。
- 锚点：`engine/CMakeLists.txt`（`DMGE_BUILD_SHARED`）、`engine/src/DMGameEngine/Renderer/RenderQueue.cpp`（定长批次数组的文件头注释）
- 深入：`kb/KB-07-已知问题与陷阱清单.md` 的 **K-001**、`kb/KB-02-DLL边界与STL.md`、档案 `documents/DLL_STL_BOUNDARY.md`

### PCH（预编译头）
`dmge_pch.h` 把高频标准库/框架头预编译成一次包含，加快全仓构建；CMake 选项 `DMGE_USE_PCH`（默认 ON）控制，glad.c 等 C 源被排除在 PCH 之外。
- 锚点：`engine/src/DMGameEngine/dmge_pch.h`、`engine/CMakeLists.txt`（`DMGE_USE_PCH` 与 `target_precompile_headers`）
- 深入：档案 `documents/PRECOMPILED_HEADER.md`、`kb/KB-01-构建系统与预编译头.md`

### RenderTarget / 离屏 RTT（render-to-texture）
把一帧渲染到 `FrameBuffer` 而不是交换链，颜色附件再作为纹理被采样/显示。编辑器 Viewport 是唯一已落地的消费者；`SceneCamera` 支持自带 RenderTarget 的钩子已就位但当前无调用方（为阴影/后处理预留）。
- 锚点：`engine/src/DMGameEngine/Renderer/FrameBuffer.h`（`SwapChainTarget=false` 即离屏）、`engine/src/DMGameEngine/Renderer/Renderer.h`（`BeginScene(camera, target)`）、`engine/src/DMGameEngine/Scene/SceneCamera.h`（`SetRenderTarget`）
- 深入：[RendererTour.md §4](RendererTour.md)

## 渲染

### Renderer（三层之一）
场景级静态门面，回答"这个场景要画什么"：缓存相机/光照、收绘制请求入队、`EndScene` 触发 `Flush`。自己不持有后端。
- 锚点：`engine/src/DMGameEngine/Renderer/Renderer.h/.cpp`
- 深入：[RendererTour.md §1](RendererTour.md)、[LearningPath.md 站③](LearningPath.md)

### RenderCommand（三层之二）
后端无关的**静态门面（facade）**，回答"怎么把一条 GPU 命令发出去"：内部持有唯一的后端实例 `s_RendererAPI`（`DM::Scope<RendererAPI>`）并逐条转发。
- 锚点：`engine/src/DMGameEngine/Renderer/RenderCommand.h/.cpp`
- 深入：[RendererTour.md §1](RendererTour.md)

### RendererAPI（三层之三）
纯虚接口 + `Create()` 工厂，回答"具体到某个图形 API 该调什么"。OpenGL/Vulkan 各派生一份，实现在 `Platform/` 下；初始管线状态经 `RendererAPIInitConfig` 数据包下发。
- 锚点：`engine/src/DMGameEngine/Renderer/RendererAPI.h/.cpp`、`engine/src/DMGameEngine/Platform/OpenGL/`、`Platform/Vulkan/`
- 深入：`kb/KB-03-渲染双后端注意事项.md`、档案 `documents/VulkanBackendReview.md`

### RenderPass / BeginRenderPass
渲染 pass 的括号抽象：`BeginRenderPass(FrameBuffer*)` / `EndRenderPass()`。OpenGL 侧是绑定/恢复 FBO（`m_PrevBoundFBO`），Vulkan 侧是动态渲染 pass 的开始/结束。
- 锚点：`engine/src/DMGameEngine/Renderer/RendererAPI.h`（虚函数声明）、`engine/src/DMGameEngine/Renderer/RenderCommand.h`（静态转发）、`engine/src/DMGameEngine/Platform/OpenGL/OpenGLFrameBuffer.cpp`
- 深入：[RendererTour.md §1/§4](RendererTour.md)

### FrameBuffer
渲染目标抽象：`FramebufferSpecification` 描述尺寸、颜色附件、深度格式；`GetColorAttachment(i)` 取颜色附件当 `Texture2D` 用（编辑器 Viewport 就是这么显示的）。
- 锚点：`engine/src/DMGameEngine/Renderer/FrameBuffer.h`、实现 `Platform/OpenGL/OpenGLFrameBuffer.h/.cpp`
- 深入：[RendererTour.md §4](RendererTour.md)（含一条已知注释漂移）

### Material / MaterialInstance
`Material` = 一个 `Ref<Shader>` + 命名 uniform 值表 + 纹理槽表，`Bind()` 一次上传全部；`MaterialInstance` 继承 `Material`，引用共享 base 并叠加自己的 uniform/纹理 override，永不触碰 base。纯抽象层，无后端子类。
- 锚点：`engine/src/DMGameEngine/Renderer/Material.h/.cpp`；用法示例 `game/src/main.cpp`
- 深入：[RendererTour.md §3](RendererTour.md)、档案 `documents/MATERIAL_OVERRIDE_SERIALIZATION.md`

### RenderQueue
延迟提交核心：`Submit` 只入队，`Flush` 时按 `(Material, Mesh)` 分组排序——每组只在首个 draw 绑材质、上传 `u_ViewProjection` 与光照 uniforms，后续 draw 只传 `u_Transform`/`u_NormalMatrix`。实例化批次另走定长数组（DLL 边界约束）。
- 锚点：`engine/src/DMGameEngine/Renderer/RenderQueue.h/.cpp`（`Flush()`）
- 深入：[RendererTour.md §2](RendererTour.md)、`kb/KB-02-DLL边界与STL.md`

### 实例化合批（kInstancingThreshold）
同一 (Material, Mesh) 组内实体数 ≥ `kInstancingThreshold`（常量 **8**）时打包成实例批次：实例矩阵作为顶点属性（location 0-3）进实例 buffer，一次 `DrawIndexedInstanced` 画完整组；每批上限 `kMaxInstances = 4096`，批次槽上限 `kMaxInstancedBatches = 32`。低于阈值退回逐 draw 提交。
- 锚点：`engine/src/DMGameEngine/Scene/Systems/MeshRenderSystem.h`（`kInstancingThreshold`/`kMaxInstances` 定义与双路逻辑）、`engine/src/DMGameEngine/Renderer/RenderQueue.cpp`（`kMaxInstancedBatches` 与实例化批次 Flush）
- 深入：[RendererTour.md §2](RendererTour.md)

### Blinn-Phong
当前前向光照模型（环境光来自首个方向光 + 点光/聚光，上限 `MAX_POINT_LIGHTS=16` / `MAX_SPOT_LIGHTS=8`，per-name uniform 上传、非 UBO）。shader 为 GLSL，实例化路径用独立变体。
- 锚点：`engine/shaders/BlinnPhong.glsl`、`engine/shaders/BlinnPhongInstanced.glsl`、数据结构 `engine/src/DMGameEngine/Renderer/Light.h`
- 深入：[RendererTour.md §5](RendererTour.md)、档案 `documents/LIGHTING_ASSESSMENT.md`

### SetAPI 双后端
`Renderer::SetAPI(Renderer::API::...)` 在运行期设定后端枚举，`RendererAPI::Create()` 据此实例化；Vulkan 还需编译期 `-DDMGE_VULKAN_BACKEND=ON`（宏 `DMGE_VULKAN`）。game/editor 目前都显式选 OpenGL。
- 锚点：`engine/src/DMGameEngine/Renderer/Renderer.h`（`SetAPI`）、`engine/src/DMGameEngine/Renderer/RendererAPI.cpp`（`Create()`）、`engine/CMakeLists.txt`（CMake 选项）
- 深入：[RendererTour.md §6](RendererTour.md)、档案 `documents/VULKAN_BACKEND.md`

## ECS 与场景

### Entity
实体句柄：`using Entity = uint32_t`（`NullEntity` 表示无效）。只是个数字，边界处经 `ToEntt`/`FromEntt` 与 `entt::entity` 互转；每次序列化加载 ID 都会变（靠 UUID 重建关系）。
- 锚点：`engine/src/DMGameEngine/Scene/Entity.h`
- 深入：档案 `documents/ECS_CONCEPTS.md`、`kb/KB-04-ECS与场景序列化约定.md`

### Component（纯数据纪律）
组件是纯数据（POD 优先），禁止塞逻辑——红线 R4，逻辑一律进 System。现有 6 个：`IDComponent / TagComponent / TransformComponent / MeshComponent / CameraComponent / LightComponent`，聚合入口 `Components.h`。
- 锚点：`engine/src/DMGameEngine/Scene/Components/`（逐个头文件）
- 深入：`AGENTS.md` §红线 R4、`kb/KB-04-ECS与场景序列化约定.md`

### System
纯逻辑单元：基类持 `Scene&`，子类覆盖 `OnUpdate/OnRender/OnEvent`。无自动发现机制，`AddSystem` 注册顺序即执行顺序（现有 3 个：TransformSystem 必须最先，LightSystem 必须在 MeshRenderSystem 之前）。
- 锚点：`engine/src/DMGameEngine/Scene/Systems/System.h` 及同目录 `TransformSystem.h` / `LightSystem.h` / `MeshRenderSystem.h`
- 深入：[LearningPath.md 站⑤](LearningPath.md)、档案 `documents/ECS_DESIGN.md`

### Registry（entt）
`Scene` 内部持有的 `entt::registry`（`m_Registry`），组件的增删查全部委托给它；`Scene` 是它的薄封装 + System 容器 + 层级操作的家。公共 API 不泄漏 entt 类型。
- 锚点：`engine/src/DMGameEngine/Scene/Scene.h/.cpp`（`m_Registry` 与 `ToEntt` 互转）
- 深入：档案 `documents/ECS_CONCEPTS.md`

### 三叉链层级
Transform 层级用"左孩子右兄弟"三叉链存在 `TransformComponent` 里：`Parent / FirstChild / NextSibling` 三条 `Entity` 句柄，`Scene::SetParent` 负责一致地重挂链。孤儿子实体在删除父后成为独立根（无级联重挂）。
- 锚点：`engine/src/DMGameEngine/Scene/Components/TransformComponent.h`（三个字段）、`engine/src/DMGameEngine/Scene/Scene.cpp`（`SetParent`）
- 深入：档案 `documents/SCENE_DESIGN.md`、`kb/KB-04-ECS与场景序列化约定.md`

### dirty 传播
局部或祖先变换变化时 `Dirty=true`，`MarkSubtreeDirty` 沿子树传播；`TransformSystem`（必须第一个注册）按拓扑序重算 `WorldMatrix` 并清脏标记。`LocalMatrix/WorldMatrix/Dirty` 是运行时字段，序列化不落盘。
- 锚点：`engine/src/DMGameEngine/Scene/Components/TransformComponent.h`（`Dirty` 字段与注释）、`engine/src/DMGameEngine/Scene/Systems/TransformSystem.h`、`engine/src/DMGameEngine/Scene/Scene.cpp`（`MarkSubtreeDirty` 调用点）
- 深入：[LearningPath.md 站⑤](LearningPath.md)

### SceneSerializer
场景 ↔ `.scene` JSON（nlohmann/json，引擎 PRIVATE 依赖不外泄）。Load 用**两趟**：先建全部实体并建 UUID→Entity 映射，再按父 UUID 重挂层级。Mesh 引用只序列化 AssetUUID——未注册进 AssetManager 的程序化网格会被静默丢掉（K-012）。
- 锚点：`engine/src/DMGameEngine/Scene/SceneSerializer.h/.cpp`
- 深入：`kb/KB-04-ECS与场景序列化约定.md`（含 **K-012**）、`kb/KB-07-已知问题与陷阱清单.md`

## 资产

### AssetUUID
资产的稳定 64 位身份，与文件路径解耦：`using AssetUUID = uint64_t`（`NullUUID = 0`）。移动文件只改元数据，引用仍有效。
- 锚点：`engine/src/DMGameEngine/Asset/AssetTypes.h`
- 深入：档案 `documents/ASSET_UUID_CONCEPTS.md`、`kb/KB-05-资产系统约定.md`

### AssetHandle
纯身份 token：只存一个 `AssetUUID`，**不存路径**；路径经 `AssetManager::GetMetadata(uuid)` 反查。不持有资产对象。
- 锚点：`engine/src/DMGameEngine/Asset/AssetHandle.h`
- 深入：`documents/ASSET_UUID_CONCEPTS.md` part 4

### AssetManager 三表
资产单例的三张核心表：`m_Registry`（UUID→`AssetMetadata`：路径/类型/依赖）、`m_PathToUUID`（路径反查）、`m_Cache`（UUID→`weak_ptr`，缓存命中同一 UUID 永远拿到同一 `Ref<T>`，无人引用自动释放）。`Register/LoadRegistry/SaveRegistry` 把映射持久化为 JSON。当前为同步加载。
- 锚点：`engine/src/DMGameEngine/Asset/AssetManager.h/.cpp`（三表成员与文件头注释）、导入 `Asset/MeshImporterAssimp.cpp`
- 深入：[LearningPath.md 站⑥](LearningPath.md)、档案 `documents/ASSET_DESIGN.md`

## 编辑器

> 注意：编辑器仍在活跃开发（多场景、导出等），本组词条只描述**稳定的机制与类**，不绑定具体菜单项文本/快捷键细节——以 `editor/src/EditorLayer.cpp` 当前实现为准。

### EditorLayer
编辑器的 ImGui 主体：dockspace、菜单、面板（Viewport / Hierarchy / Inspector / Systems / Asset Browser）、gizmo、点选、Play 控制都挂在它上面。`OnImGuiRender()` 是全引擎 API 的消费清单。
- 锚点：`editor/src/EditorLayer.h/.cpp`
- 深入：[LearningPath.md 站⑦](LearningPath.md)

### EditorScene
编辑器的场景容器：持有编辑场景、离屏 FrameBuffer 与编辑器相机（`EditorCameraController`），`Render()` 里括出离屏 RTT pass。Play mode 下内部持两份场景（编辑场景 + 运行副本），激活的是其中之一。
- 锚点：`editor/src/EditorScene.h/.cpp`（类头注释与 `m_EditScene / m_PlayScene / m_Scene` 三成员）
- 深入：[RendererTour.md §4](RendererTour.md)

### Play mode 快照隔离
进入 Play 时把编辑场景**深拷贝**成运行副本（`SceneDuplicator`，Mesh/Material 资源按 `Ref` 共享、只读），之后所有系统 tick 与渲染都作用于副本；Stop 时丢弃副本、恢复原封不动的编辑场景——主流引擎语义"Play 里的改动不保留"。选中集按 UUID 快照，Stop 后恢复。不用 JSON 快照的原因见 K-012（程序化网格会丢）。
- 锚点：`editor/src/EditorScene.h/.cpp`（`EnterPlayMode` / `ExitPlayMode`）、`editor/src/SceneDuplicator.h/.cpp`、`editor/src/EditorLayer.cpp`（`BeginPlay` / `EndPlay`）
- 深入：`kb/KB-07-已知问题与陷阱清单.md` **K-012**

### SceneDuplicator
编辑器侧的实体子树深拷贝器（命名空间 `EditorSceneCopy`）：按值复制组件、按引用共享 `DM::Ref<Mesh>` / `DM::Ref<MaterialInstance>` 资源，子树内层级链重映射、子树外链接断开为根。两个消费者：Play 快照隔离、Prefab 保存/实例化。
- 锚点：`editor/src/SceneDuplicator.h/.cpp`（`CopyEntityTree` / `CopyAllEntities`）
- 深入：文件头注释（含"为什么不用序列化往返"的完整论证）

### Prefab
把选中实体的整棵子树经临时 Scene + `SceneSerializer` 存成 `.scene` 文件（Prefab 即"单实体场景"）；实例化则反向加载并拷回当前编辑场景（Play 中禁止实例化）。无需引擎改动，全部在编辑器侧实现。
- 锚点：`editor/src/EditorLayer.cpp`（`SavePrefab` / `InstantiatePrefab`）、`editor/src/SceneDuplicator.cpp`（`CopyEntityTree` 的临时 Scene 用法）
- 深入：`kb/KB-04-ECS与场景序列化约定.md`

### gizmo（ImGuizmo）
Viewport 内的平移/旋转/缩放操纵器：ImGuizmo（第三方库）绘制于 Viewport 的 ImGui draw list 上，`Manipulate` 用编辑器相机的 view/projection 改写实体的 TRS；快捷键 1/2/3 切换平移/旋转/缩放、4 关闭。gizmo 激活时鼠标事件不透传（点选/相机控制都要让位）。
- 锚点：`editor/src/EditorLayer.cpp`（`ImGuizmo::Manipulate` 调用与 `m_GizmoType`）、第三方库在 `editor/dependencies/`
- 深入：`editor/GIZMO_HITTEST_FIX.md`（命中测试修复实录）

### Asset Browser
资产浏览面板：按目录树浏览仓库资产目录（含 `prefabs/` 等），与拖拽/双击加载衔接。
- 锚点：`editor/src/EditorLayer.cpp`（`DrawAssetBrowser`）
- 深入：[LearningPath.md 站⑥/⑦](LearningPath.md)

---

## 使用与维护约定

- 引用本表时直接锚到词条（`Glossary.md#术语`）；发现某条与代码不符，以代码为准并反馈文档 Agent。
- 新词条准入：**项目特有**（不是通用图形学/英语词汇）+ 能给出已核验的代码锚点 + 一句话能说清。
