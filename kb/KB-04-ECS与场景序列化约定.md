# KB-04 ECS 与场景序列化约定

> 深度阅读：`documents/ECS_CONCEPTS.md`（概念）、`documents/ECS_DESIGN.md`（实现设计）、`documents/SCENE_DESIGN.md`、`documents/SCENE_SERIALIZATION_OPTIONS.md`。

## ECS 结构事实

- 基于 **entt**（`engine/dependencies/`）。`Entity` 是 registry 句柄包装；组件在 `Scene/Components.h`：ID/Tag/Transform/Mesh/Camera/Light。
- 层级：三叉链（firstChild/nextSibling/prevSibling/parent）+ **dirty 传播**（父变 → 子树标脏）+ 剪枝 + 环检测。
- System：`OnUpdate(ts)` / `OnRender()` / `OnEvent(e)`，由 `Scene` 持列表调度。现有：TransformSystem、MeshRenderSystem、LightSystem。

## 硬纪律（R4 展开）

1. Component **纯数据**：POD 优先；可以有 `DM::Ref` 成员（资源句柄），不许有行为方法。
2. 逻辑一律进 System；跨组件交互放 System 的遍历里，不许组件互相引用 Entity 再互相调方法（层级关系除外——parent 是数据）。
3. 新增"每帧逻辑"前先确认该挂哪个 System，而不是新建每 Entity 的 Update。
4. world matrix **运行时算，不存储**（序列化也不存）；只存 local TRS。

## 序列化事实（`.scene`，JSON）

- `SceneSerializer` 做往返；实体 = UUID + tag + 各组件 POD 字段。
- **层级重建靠父 UUID**，不是靠数组顺序。
- 资产引用（mesh/材质）**只存 UUID**（见 KB-05），反序列化经 AssetManager 恢复。
- MaterialOverrides 已支持序列化。
- 格式演进决策记录在 `SCENE_SERIALIZATION_OPTIONS.md`——想改格式先读它，别推翻已达成的取舍。

## 常见坑

- 遍历中增删组件 → entt 迭代器失效；需要结构性修改时先收集后改。
- 复制/移动 Entity 忘处理层级三叉链指针 → 孤儿节点；用 Scene 提供的操作接口，别直接操作 registry。
- 序列化加新组件字段忘同步 Loader → 加载静默丢字段；**往返测试是兜底**，新组件必配。
