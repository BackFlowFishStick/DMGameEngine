# PB-02 新增 ECS 组件与系统

> 背景：ECS 概念读 `documents/ECS_CONCEPTS.md`，项目实现读 `documents/ECS_DESIGN.md`。
> 现有组件：ID/Tag/Transform/Mesh/Camera/Light；现有 System：Transform/MeshRender/Light。

## 步骤

### 1. 组件定义（`engine/src/DMGameEngine/Scene/Components.h`）
- **纯数据**（R4）： POD 或仅含 `DM::Ref` 资源句柄；不给组件写方法。
- 有默认值：成员用默认初始化（先例见现有组件写法）。
- 需要 UUID 引用外部资产的（如 `AudioClipHandle`）遵守 `documents/ASSET_UUID_CONCEPTS.md`：**只存 UUID，不存路径**。

### 2. 层级/脏标记
- 若组件参与 Transform 层级传播，明确它读 world matrix 还是 local；**world 不序列化**（运行时算）。
- 新的 dirty 语义先与现有 `TransformSystem` 的传播机制对齐，不要另起一套。

### 3. System（`engine/src/DMGameEngine/Scene/Systems/`）
- 实现 `System` 接口（`OnUpdate(ts)` / `OnRender()` / `OnEvent(e)`）。
- 在 `Scene` 注册（找到现有 System 注册处照做）；源文件注册进 CMakeLists（R2）。
- 遍历用 entt view/group，注意只读遍历不要在遍历中增删组件。

### 4. 测试（`engine/tests/`）
- 组件默认值、System 行为、层级传播（若涉及）各至少一例。
- 若组件依赖 DLL 导出类型，注意测试 target 与引擎 DLL 的链接方式（先例：assimp headless 测试）。

### 5. 序列化（`SceneSerializer`）
- Save/Load 各加一段；引用资产存 UUID；**save → load → 字段相等**往返测试是硬要求（先例：`.scene` 往返测试）。
- 格式决策有争议时读 `documents/SCENE_SERIALIZATION_OPTIONS.md`，不要重新发明格式。

### 6. 编辑器暴露
- Inspector 面板需要展示新组件：按 PB-05 在编辑器侧加 UI（或记 TASKS.md 移交编辑器 Agent）。
- 场景面板右键菜单"Add Component"列表同步。

### 7. 收尾
- PB-08 自检；变更日志 + KB-07；TASKS.md ✅。
