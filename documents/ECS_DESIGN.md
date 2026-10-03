# DMGameEngine ECS 实现设计（阶段 1b）

> 状态：设计草案（待确认 4 个决策点后进入实现）  
> 创建：2026-07-26  
> 分支：main  
> 对应路线图：`ENGINE_ROADMAP.md` 阶段 1b（Transform + Entity + Component，E4）  
> 关联文档：`ENGINE_ROADMAP.md`（1b 设计要点）、`ENGINE_SUMMARY.md`（现状）

---

## 1. 目标与范围

### 1.1 目标
让引擎从「渲染器」升级为「引擎」：引入 ECS（Entity-Component-System）作为场景组织、组件挂载、系统调度的统一基础，为后续物理（阶段 3a）、脚本（3c）、序列化（1c）、编辑器（2a）提供依附点。

### 1.2 范围（本阶段交付）
- 引入 entt 作为 ECS 底层。
- 定义核心 Component：`IDComponent` / `TagComponent` / `TransformComponent` / `MeshComponent` / `CameraComponent`。
- 实现 `Scene` 类（持 `entt::registry` + System 列表）。
- 实现 `TransformSystem`（层级 + dirty propagation）和 `MeshRenderSystem`（对接现有 `Renderer`）。
- 整合 `DefaultSceneLayer`：增量接入 `Scene`，相机仍沿用 `CameraController`。

### 1.3 不做（留后续）
- 不自研 ECS（用 entt）。
- 不实现序列化（1c）。
- 不实现编辑器（2a）。
- 不启用 `CameraComponent` 接管相机（留 1b 后期或独立步骤）。
- 不做骨骼动画 / 物理 / 脚本（阶段 3）。

### 1.4 前置依赖
- 无强依赖（可与阶段 1a AssetManager 并行；1a 让 `MeshComponent` 材质加载更自然，但非阻塞）。
- 联网：引入 entt 需 `FetchContent` 下载。

---

## 2. 现状与约束（调研结论）

实现前已确认的现有代码约束：

| 约束 | 现状 | 对 ECS 实现的影响 |
|------|------|-------------------|
| `Renderer::Submit` 签名 | `Submit(const Ref<Material>&, const Ref<VertexArray>&, const glm::mat4& transform)` | 与 `MeshRenderSystem` **零适配对接** |
| `DefaultSceneLayer` | header-only，持 `Ref<CameraController>`，已建 `BeginScene / OnSceneRender / EndScene` bracket，`OnSceneRender()` 是空 protected virtual | ECS 的最佳切入点（见 §10） |
| CMake 源文件注册 | `set(DMGE_SOURCES ...)` 显式列源文件（非 GLOB） | 新增 `.cpp/.h` 必须**手动加进列表** |
| 依赖引入 | `dependencies/` + `add_subdirectory`（glm/glfw/imgui）；测试用 `FetchContent` | entt 用 `FetchContent`（header-only） |
| 公共 API | `DMGameEngine.h` 聚合 include，分 Core/Events/Renderer/Scene/ImGui/Debug 六块 | ECS 头文件加进 `// ── Scene ──` 块 |
| 智能指针别名 | `DM::Ref` / `DM::Scope`（`Core/Export.h`） | Component / System 用 `Ref` |

---

## 3. 架构决策：引入 entt（不自研）

**决策**：用 [entt](https://github.com/skypjack/entt) v3.13.2，`FetchContent` 引入，header-only。

**理由**：
- 路线图原文即倾向 entt；1b 目标是「让引擎变可用」而非「造 ECS 轮子」。
- 自研 ECS 难点（SparseSet + 可变参数 `view<...>` + 类型擦除 + 实体版本号）是几千行易错代码，投入产出比低。
- entt header-only，`FetchContent` 即用，与现有 GoogleTest 引入方式一致。
- API（`registry.view<T,U>()`、`registry.emplace<T>(e, args...)`、`registry.get<T>(e)`）天然贴合路线图设计。

**不做**：不自己写 registry。若日后想学 DOD 内部实现，单开 spike 分支研究，不进 main。

---

## 4. 目录结构

```
src/DMGameEngine/Scene/
├── Scene.h / Scene.cpp              # 持 entt::registry + System 列表 + 生命周期
├── Entity.h                        # Entity 类型封装（uint32_t）+ 辅助常量
├── Components/
│   ├── Components.h                 # 聚合 include（供 DMGameEngine.h 引用）
│   ├── IDComponent.h                # uint64_t UUID（为 1c 序列化预留）
│   ├── TagComponent.h               # std::string Tag
│   ├── TransformComponent.h         # 本地变换 + 层级三叉链 + world 缓存 + dirty
│   ├── MeshComponent.h              # Ref<VertexArray> + Ref<Material>
│   └── CameraComponent.h            # 包装 SceneCamera + Primary 标记
└── Systems/
    ├── System.h                     # 基类：OnUpdate/OnRender/OnEvent
    ├── TransformSystem.h / .cpp     # dirty propagation + world matrix
    └── MeshRenderSystem.h / .cpp     # view<Transform,Mesh> -> Renderer::Submit
```

新增 `.cpp/.h` 必须**手动加进 `CMakeLists.txt` 的 `DMGE_SOURCES`**。

---

## 5. Entity 定义

```cpp
// Entity.h
#pragma once
#include <cstdint>

namespace DMGameEngine {

// Entity 是 ID，不是对象。内部即 entt::entity 的底层值。
// DMGE 不在公共 API 直接暴露 entt 头（见 §12 暴露策略），用 DMGE 自己的别名。
using Entity = uint32_t;
constexpr Entity NullEntity = static_cast<Entity>(-1);

} // namespace DMGameEngine
```

- Entity 不存数据、不存方法、不存父子指针。
- 实体的「身份 + 版本」由 `entt::registry` 维护（防止复用已销毁实体 ID 指向新实体）。

---

## 6. Component 定义

**纪律**：Component 是纯数据（POD），**不写逻辑方法**（顶多 `static Create()` 工厂）。逻辑全部下放到 System。

### 6.1 IDComponent
```cpp
struct IDComponent {
    uint64_t UUID = 0;
};
```

### 6.2 TagComponent
```cpp
struct TagComponent {
    std::string Tag;
    TagComponent() = default;
    TagComponent(const std::string& t) : Tag(t) {}
};
```

### 6.3 TransformComponent（核心，含层级 + dirty）
```cpp
struct TransformComponent {
    // 本地变换（序列化存这些）
    glm::vec3 Translation{0.0f};
    glm::vec3 RotationEuler{0.0f};   // 欧拉角（弧度），序列化友好
    glm::vec3 Scale{1.0f};

    // 层级关系（存 Entity ID，非指针）—— 三叉链表（左孩子右兄弟）
    Entity Parent       = NullEntity;
    Entity FirstChild   = NullEntity;
    Entity NextSibling  = NullEntity;

    // 运行时缓存（不序列化）
    glm::mat4 LocalMatrix{1.0f};
    glm::mat4 WorldMatrix{1.0f};
    bool Dirty = true;

    TransformComponent() = default;
};
```

### 6.4 MeshComponent（对接现有 Renderer）
```cpp
struct MeshComponent {
    Ref<VertexArray> VAO;
    Ref<Material>   Material;
};
```
`MeshRenderSystem` 调 `Renderer::Submit(Material, VAO, WorldMatrix)` —— **签名完全匹配，零适配**。

### 6.5 CameraComponent（1b 定义但不启用，留后续）
```cpp
struct CameraComponent {
    SceneCamera Camera;
    bool Primary = false;             // 标记活动相机
    bool FixedAspectRatio = false;   // 编辑器 viewport 用
};
```

---

## 7. System 设计

### 7.1 基类
```cpp
// System.h
class System {
public:
    virtual ~System() = default;
    virtual void OnUpdate(Timestep ts) {}
    virtual void OnRender() {}
    virtual void OnEvent(Event& e) {}
protected:
    Scene& m_Scene;   // System 通过 Scene 拿 registry
    explicit System(Scene& s) : m_Scene(s) {}
};
```

### 7.2 TransformSystem（难点：层级 + dirty propagation）
```cpp
class TransformSystem : public System {
public:
    using System::System;
    void OnUpdate(Timestep) override;
    // 流程：
    //   1. 收集根节点（Parent == NullEntity）
    //   2. 深度优先拓扑序遍历（保证父先于子）
    //   3. 对每个 Dirty==true 的实体：
    //        LocalMatrix = Translate(T) * toMat4(quat(R)) * Scale(S)
    //        WorldMatrix = (Parent != Null) ? parent.WorldMatrix * LocalMatrix
    //                                        : LocalMatrix
    //        Dirty = false
};
```

### 7.3 MeshRenderSystem（对接点）
```cpp
class MeshRenderSystem : public System {
public:
    using System::System;
    void OnRender() override {
        auto view = m_Scene.GetRegistry().view<TransformComponent, MeshComponent>();
        for (auto e : view) {
            auto& tc = view.get<TransformComponent>(e);
            auto& mc = view.get<MeshComponent>(e);
            Renderer::Submit(mc.Material, mc.VAO, tc.WorldMatrix);
        }
    }
};
```
注意：`OnRender` **不调 `BeginScene/EndScene`**，只 Submit draws。bracket 由 `DefaultSceneLayer` 负责。

---

## 8. Scene 类

```cpp
class Scene {
public:
    Scene();
    Entity CreateEntity(const std::string& tag = "Entity");  // 自动挂 ID+Tag+Transform
    void   DestroyEntity(Entity e);

    // 模板访问（委托给 entt::registry）
    template<typename T, typename... Args>
    T& AddComponent(Entity e, Args&&... args);
    template<typename T> T& GetComponent(Entity e);
    template<typename T> bool HasComponent(Entity e);
    template<typename T> void RemoveComponent(Entity e);

    // System 注册（顺序即执行顺序）
    void AddSystem(Ref<System> sys);

    // 生命周期
    void OnUpdate(Timestep ts);   // 调各 System::OnUpdate（TransformSystem 先算 world matrix）
    void OnRender();              // 调各 System::OnRender（不含 BeginScene/EndScene）

    // Transform 层级辅助（标 dirty + 传播）
    void SetTranslation(Entity e, glm::vec3 t);
    void SetRotation(Entity e, glm::vec3 r);
    void SetScale(Entity e, glm::vec3 s);
    void SetParent(Entity child, Entity newParent);
    void MarkSubtreeDirty(Entity root);

    entt::registry& GetRegistry() { return m_Registry; }   // 内部用（见 §12）
private:
    entt::registry m_Registry;
    std::vector<Ref<System>> m_Systems;
};
```

---

## 9. Transform 层级 + dirty propagation

### 9.1 为什么需要
场景图层级中，子节点的 world matrix 依赖父节点：`world(child) = world(parent) × local(child)`。父动则整条链都得重算。

### 9.2 三叉链表（左孩子右兄弟）
`TransformComponent` 用 `Parent / FirstChild / NextSibling` 三个 Entity ID 表达树，而非 `vector<children>`：
- 组件固定大小（POD 连续存储要求）。
- 无动态内存分配。
- 遍历子树：`child = parent.FirstChild; while(child != Null){ recurse(child); child = child.NextSibling; }`

### 9.3 脏标记 + 传播
- **修改本地变换时**：只标记 `Dirty=true`，**不立即重算**；同时 `MarkSubtreeDirty(e)` 递归标记所有后代 dirty。
- **传播剪枝**：若后代已是 dirty，停止向下走（其子树必然也已脏）。
- **重算集中**：`TransformSystem::OnUpdate` 统一重算，只对 `Dirty==true` 的做，按拓扑序（父先子后）。

### 9.4 与场景图 dirty propagation 的差异
骨骼动画层级**不**用这套 Entity+dirty 方案：骨头数量小但每帧几乎全动，dirty 收益小；骨头应作为 `SkeletalMeshComponent` 内部的连续数组（数组索引表达层级），每帧全量重算。骨骼动画是阶段 3 的事，本阶段不涉及。

---

## 10. 与 DefaultSceneLayer 整合（增量方案）

**采用方案 A（最小改动，相机仍用 CameraController）**：

```cpp
class DefaultSceneLayer : public Layer {
    Ref<CameraController> m_CameraController;
    Ref<Scene>            m_Scene;          // 新增
public:
    void SetScene(const Ref<Scene>& s) { m_Scene = s; }
protected:
    void OnUpdate(Timestep ts) override {
        if (m_CameraController) m_CameraController->OnUpdate(ts);
        if (m_Scene) m_Scene->OnUpdate(ts);    // ECS tick
    }
    void OnRender() override {
        if (m_CameraController) Renderer::BeginScene(m_CameraController->GetCamera());
        else Renderer::BeginScene();
        if (m_Scene) m_Scene->OnRender();       // 只 Submit，不 bracket
        Renderer::EndScene();
    }
};
```

- bracket 仍由 `DefaultSceneLayer`（基于 `CameraController`）负责，**渲染器零改动**。
- `Scene::OnRender` 只 Submit draws，不碰 `BeginScene/EndScene`。
- `CameraComponent` 接管相机是**后续演进**（1b 后期或独立步骤），届时 `DefaultSceneLayer` 可退化为纯 `Scene` 容器。

**理由**：渐进、低风险。先让 `MeshRenderSystem` 跑通验证 ECS 管线，相机改造留到管线稳定后。

---

## 11. CMake / 依赖引入

### 11.1 引入 entt
```cmake
include(FetchContent)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(entt)

target_link_libraries(DMGameEngine PUBLIC EnTT::EnTT)   # PUBLIC 见 §12
```

### 11.2 DMGE_SOURCES 追加
```cmake
set(DMGE_SOURCES
    # ... 现有 ...
    "src/DMGameEngine/Scene/Scene.cpp"
    "src/DMGameEngine/Scene/Systems/TransformSystem.cpp"
    "src/DMGameEngine/Scene/Systems/MeshRenderSystem.cpp"
    "src/DMGameEngine/Scene/Scene.h"
    "src/DMGameEngine/Scene/Entity.h"
    "src/DMGameEngine/Scene/Components/Components.h"
    "src/DMGameEngine/Scene/Components/IDComponent.h"
    "src/DMGameEngine/Scene/Components/TagComponent.h"
    "src/DMGameEngine/Scene/Components/TransformComponent.h"
    "src/DMGameEngine/Scene/Components/MeshComponent.h"
    "src/DMGameEngine/Scene/Components/CameraComponent.h"
    "src/DMGameEngine/Scene/Systems/System.h"
    "src/DMGameEngine/Scene/Systems/TransformSystem.h"
    "src/DMGameEngine/Scene/Systems/MeshRenderSystem.h"
)
```

### 11.3 DMGameEngine.h 追加（`// ── Scene ──` 块）
```cpp
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/Components.h"
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Systems/TransformSystem.h"
#include "DMGameEngine/Scene/Systems/MeshRenderSystem.h"
```

---

## 12. 设计权衡：entt 暴露策略

| 方案 | 做法 | 优点 | 代价 |
|------|------|------|------|
| **A. PUBLIC 暴露**（Hazel 做法，推荐） | `EnTT::EnTT` PUBLIC 链接，`Scene::GetRegistry()` 返回 `entt::registry&`，游戏工程可直接用 entt API | 简单、灵活、和 Hazel/教程一致 | entt 头传播给消费者，耦合 entt 版本，升级 entt 破坏 ABI |
| **B. PImpl 隐藏**（生产做法） | entt PRIVATE，`Scene` 用 PImpl 挡掉，公共 API 只暴露 DMGE 自己的 `Entity` + 模板方法 | 解耦、可控 | 模板方法仍会实例化 entt，彻底隐藏需类型擦除，复杂 |

**推荐方案 A**（学习引擎阶段）：和 Hazel 一致，开发顺滑。本文档标注「未来可考虑 PImpl 收敛」。如此 `Renderer::Submit` 对接、`view` 查询都直接用 entt，最省力。

---

## 13. 实现步骤（可执行清单，按依赖排序）

1. **引入 entt**：`FetchContent` + `target_link_libraries`，验证能 `#include <entt/entt.hpp>` 编译通过。
2. **写 Component 头**（纯数据，header-only）：`IDComponent`/`TagComponent`/`TransformComponent`/`MeshComponent`/`CameraComponent` + `Components.h` 聚合。
3. **写 `Entity.h`**：`using Entity = uint32_t` + `NullEntity`。
4. **写 `System.h` 基类**：`OnUpdate/OnRender/OnEvent`。
5. **写 `Scene.h/.cpp`**：`entt::registry` + System 列表 + `CreateEntity`/`DestroyEntity` + 模板 Add/Get/Has/Remove + `OnUpdate`/`OnRender` + `SetTranslation`/`SetRotation`/`SetScale`/`SetParent`/`MarkSubtreeDirty`。
6. **写 `TransformSystem`**：dirty propagation + 拓扑序 world matrix 计算。
7. **写 `MeshRenderSystem`**：`view<Transform,Mesh>` -> `Renderer::Submit`。
8. **改 `DefaultSceneLayer`**：加 `Ref<Scene>` + `SetScene()` + `OnUpdate`/`OnSceneRender` 接入。
9. **改 `CMakeLists.txt` + `DMGameEngine.h`**：注册新源文件 + include。
10. **编译验证**：最小示例（创建 3 个 Entity 挂 Mesh + Transform，设父子层级，跑一帧）确认 `MeshRenderSystem` 能渲染 + 父子 world matrix 正确。

---

## 14. 为后续阶段预留接口

| 预留项 | 服务阶段 | 说明 |
|--------|----------|------|
| `IDComponent`（UUID） | 1c 序列化 | 实体身份 |
| `TagComponent` | 2a 编辑器 | 实体名显示 |
| `System` 列表 | 2a 编辑器 | Gizmo/Inspector 专用 System 注册点 |
| Component 类型注册表（`std::unordered_map<std::string, SerializeFn>`） | 1c | 1b 先不实现，留扩展点 |
| `CameraComponent` | 2a 编辑器 | viewport 相机 |
| `MeshComponent` 持 `Ref<Material>` | 1a AssetManager | 材质统一加载 |

---

## 15. 设计决策点（待确认）

1. **entt 引入方式**：`FetchContent` v3.13.2（需联网下载）—— 是否认可？
2. **entt 暴露策略**：方案 A（PUBLIC，Hazel 风格）—— 是否认可？
3. **相机整合**：方案 A（`DefaultSceneLayer` 仍管相机，`CameraComponent` 留后续）—— 是否认可？
4. **分支策略**：直接在 `main` 上做 1b，还是为 1b 开 `feature/ecs-1b` 分支隔离？

---

## 16. 参考

- `ENGINE_ROADMAP.md` 阶段 1b（原始设计要点）
- `ENGINE_SUMMARY.md`（现状）
- [entt 官方文档](https://github.com/skypjack/entt/wiki)
- [Hazel 引擎教程](https://github.com/TheCherno/Hazel)（ECS 参考）
- 数据导向设计（DOD）/ cache locality 原则
