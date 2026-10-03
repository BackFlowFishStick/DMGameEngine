# DMGameEngine 场景序列化实现设计（阶段 1c）

> 状态：设计（方案 A：JSON + 手写，Hazel 风格）
> 创建：2026-07-28
> 关联：`SCENE_SERIALIZATION_OPTIONS.md`（方案选型）、`ECS_DESIGN.md`（1b ECS）、`ASSET_DESIGN.md`（1a AssetHandle UUID）

---

## 1. 目标与范围

### 1.1 目标
场景文件 `.scene` 的保存/加载：实体 + Component + 层级 + 资源引用，方案 A（JSON + 手写，nlohmann/json 已在 1a 引入）。

### 1.2 范围（本阶段交付）
- Component 类型注册表（`name -> SerializeFn/DeserializeFn`）
- 每 Component 的 `to_json`/`from_json`（手写）
- `SceneSerializer::Save(path)` / `Load(path)`
- UUID 层级重建（Parent 存父 UUID，反序列化重建三叉链）
- 资源引用存 `AssetHandle` UUID
- 版本字段
- 往返测试

### 1.3 不做（留后续）
- `AssetLoader<Mesh/Material>`（.mat 格式）-- MeshComponent 资源加载，反序列化时 VAO/Material 暂 null
- 异步加载 / 热重载
- 编辑器（2a）

### 1.4 前置依赖
- 1b ECS ✅（Entity/Component/Scene，IDComponent UUID 预留）
- 1a AssetManager ✅（AssetHandle UUID，资源引用格式）
- nlohmann/json ✅（1a 引入）

---

## 2. 文件结构

```
src/DMGameEngine/Scene/
├── SceneSerializer.h / .cpp    # 注册表 + Save/Load + Component to_json/from_json
└── Components/MeshComponent.h  # 改：加 AssetHandle 字段（meshAsset/materialAsset UUID）
```

---

## 3. Component 序列化

每 Component 手写 `to_json`/`from_json`（nlohmann/json，集中在 `SceneSerializer.cpp`）：

| Component | JSON 字段 |
|-----------|----------|
| IDComponent | `"uuid": uint64` |
| TagComponent | `"tag": string` |
| TransformComponent | `"translation": [x,y,z], "rotation": [x,y,z]（欧拉弧度）, "scale": [x,y,z], "parent": uint64(父UUID, 0=根)` |
| CameraComponent | `"primary": bool, "fixedAspectRatio": bool, "projectionType", "perspective":{fov,near,far}, "orthographic":{size,near,far}` |
| MeshComponent | `"meshAsset": uint64(AssetUUID), "material": uint64(AssetUUID)` |

### 3.1 TransformComponent 层级
- **序列化**：`parent` 存**父实体的 IDComponent UUID**（非运行时 Entity ID），0 表示根。不存 FirstChild/NextSibling（反序列化重建）。
- **反序列化**：建 `map<UUID, Entity>`，遍历实体按 parent UUID 找父 Entity，调 `Scene::SetParent` 重建三叉链。
- **local transform 序列化**：只存 Translation/RotationEuler/Scale（world 运行时由 TransformSystem 算）。

### 3.2 MeshComponent 改动（加 AssetHandle）
MeshComponent 当前只有 `Ref<VertexArray> VAO + Ref<Material> Material`。改为加 AssetHandle 字段：
```cpp
struct MeshComponent {
    AssetHandle MeshAsset;      // UUID of mesh resource (serialized)
    AssetHandle MaterialAsset;  // UUID of material resource (serialized)
    DM::Ref<VertexArray> VAO;    // runtime, loaded from MeshAsset (AssetLoader<Mesh> 留后续)
    DM::Ref<Material>    Material; // runtime, loaded from MaterialAsset (AssetLoader<Material> 留后续)
};
```
- 序列化存 `MeshAsset.GetUUID()` / `MaterialAsset.GetUUID()`。
- 反序列化读 UUID 填 AssetHandle；`VAO`/`Material` 运行时加载（`AssetLoader<Mesh/Material>` 留后续，当前反序列化后为 null，`MeshRenderSystem` 跳过空 VAO）。

---

## 4. 注册表

```cpp
using SerializeFn   = std::function<void(const Scene&, Entity, nlohmann::json&)>;
using DeserializeFn = std::function<void(Scene&, Entity, const nlohmann::json&)>;

class SceneSerializer {
public:
    static void Register(const std::string& name,
                         SerializeFn ser, DeserializeFn deser);
    static void Save(const Scene& scene, const std::string& path);
    static bool Load(Scene& scene, const std::string& path);
private:
    static std::unordered_map<std::string,
        std::pair<SerializeFn, DeserializeFn>>& Registry();
};
```

注册时机：`SceneSerializer.cpp` 顶部 static 注册（或首次 Save/Load 时延迟注册）。新增 Component 时 `Register("Mesh", serFn, deserFn)`。

---

## 5. Save 流程

```
1. json j; j["version"] = 1; j["entities"] = array;
2. 遍历 view<>（所有实体）：
     json e;
     e["uuid"] = IDComponent.UUID;
     e["tag"]  = TagComponent.Tag;
     遍历 Registry：对每 Component 类型调 SerializeFn(scene, entity, e[typeName]);
     j["entities"].push_back(e);
3. ofstream(path) << j.dump(2);
```

---

## 6. Load 流程

```
1. ifstream(path) >> j; 检查 version;
2. 第一遍（建实体 + UUID->Entity 映射）：
     遍历 j["entities"]：
       Entity e = scene.CreateEntity(tag);  // 自动挂 ID+Tag+Transform
       scene.GetComponent<IDComponent>(e).UUID = uuid;  // 覆盖随机 UUID 为序列化值
       map[uuid] = e;
       反序列化 local Transform（Translation/Rotation/Scale），parent 先存 UUID（待第二遍重建）;
       反序列化其他 Component（Camera/Mesh 资源 UUID）;
3. 第二遍（重建层级）：
     遍历实体：按 parent UUID 查 map，若父存在调 scene.SetParent(e, parentEntity);
4. Scene::OnUpdate（触发 TransformSystem 算 world matrix）;
```

---

## 7. .scene 文件格式示例

```json
{
  "version": 1,
  "entities": [
    {
      "uuid": 1719234567,
      "tag": "Player",
      "Transform": {
        "translation": [1.0, 2.0, 3.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0],
        "parent": 0
      },
      "Mesh": {
        "meshAsset": 2837465,
        "material": 9988776
      },
      "Camera": {
        "primary": true,
        "fixedAspectRatio": false
      }
    }
  ]
}
```

---

## 8. 实现步骤

1. **MeshComponent 加 AssetHandle 字段**（meshAsset/materialAsset + VAO/Material 保留）。
2. **写 `SceneSerializer.h/.cpp`**：注册表 + Save/Load + 5 个 Component 的 to_json/from_json。
3. **接入 CMake/DMGameEngine.h**（DMGE_SOURCES/HEADERS + include）。
4. **写测试** `tests/test_scene.cpp`：往返（Save -> Load -> 字段相等 + 层级重建 + 资源 UUID 保持）。
5. **编译验证 + 跑测试**。

---

## 9. 测试用例

| 测试 | 验证 |
|------|------|
| `RoundTripPreservesIdentity` | Save -> Load，UUID + Tag 保持 |
| `RoundTripPreservesTransform` | Translation/Rotation/Scale 保持 |
| `RoundTripRebuildsHierarchy` | 父子层级（parent UUID -> 三叉链重建，WorldMatrix 正确） |
| `RoundTripPreservesMeshAssetUUID` | MeshComponent meshAsset/materialAsset UUID 保持（VAO/Material null，资源加载留后续） |
| `RoundTripPreservesCamera` | CameraComponent 参数保持 |
| `VersionField` | 顶层 version 字段存在 |

---

## 10. 不做（留后续）

- `AssetLoader<Mesh>` / `AssetLoader<Material>`（.mat 格式）-- MeshComponent 资源运行时加载。
- Material 序列化（.mat 文件，含 Shader 引用 + uniforms）。
- 异步加载 / 热重载。
- 编辑器（2a）：场景 UI 编辑、Gizmo、Inspector。

---

## 11. 参考
- `SCENE_SERIALIZATION_OPTIONS.md`（方案选型，方案 A 推荐）
- `ECS_DESIGN.md`（1b，Entity/Component/Scene）
- `ASSET_DESIGN.md` / `ASSET_UUID_CONCEPTS.md`（1a，AssetHandle UUID）
- nlohmann/json 文档