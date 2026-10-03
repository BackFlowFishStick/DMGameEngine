# MaterialOverrides 序列化路线

> 状态：路线设计（待实现）
> 创建：2026-07-31
> 关联：`ECS_DESIGN.md`（1b）、`ASSET_DESIGN.md`（1a）、`SCENE_DESIGN.md`（1c）、`Material.h`（MaterialInstance）

---

## 1. 序列化目标：存什么、不存什么

`MaterialInstance` 持 `m_BaseMaterial`（Ref&lt;Material&gt;）+ `m_Overrides`（map&lt;string, UniformValue&gt;）。

| 字段 | 是否序列化 | 原因 |
|------|----------|------|
| `m_BaseMaterial` | ❌ 不存 | base = `Mesh.SubMeshes[i].MaterialAsset` -&gt; `Load<Material>`，反序列化时从 Mesh 重建，不重复存 |
| `m_Overrides` | ✅ 存 | per-instance uniform 覆盖，是 MaterialInstance 的核心价值 |
| submesh 索引 i | ✅ 存 | 标识覆盖哪个 SubMesh |

**核心：只存 overrides（uniform name + type + value）+ submesh 索引，base 不存（从 Mesh 重建）。**

---

## 2. .scene 格式扩展

```json
{
  "Mesh": {
    "meshAsset": 12345,
    "materialOverrides": [
      {
        "submesh": 0,
        "uniforms": {
          "u_Color":  { "type": "Float4", "value": [1.0, 0.0, 0.0, 1.0] },
          "u_Tiling": { "type": "Float",  "value": 2.0 }
        }
      },
      {
        "submesh": 1,
        "uniforms": { "u_Roughness": { "type": "Float", "value": 0.5 } }
      }
    ]
  }
}
```

- `materialOverrides`：数组，每元素一个 SubMesh 的覆盖。
- `submesh`：SubMesh 索引。
- `uniforms`：name -&gt; {type, value}，格式和 `.mat` 文件一致（复用）。

---

## 3. 实现步骤

### 步骤1：MaterialInstance 暴露 overrides

`Material.h` 的 `MaterialInstance` 当前 `m_Overrides` 是 private。加公开访问：

```cpp
class MaterialInstance : public Material {
public:
    const std::unordered_map<std::string, UniformValue>& GetOverrides() const { return m_Overrides; }
    // ...
};
```

### 步骤2：提取 ApplyUniform 共享

`AssetLoader<Material>` 已有 type dispatch（JSON -&gt; Material::Set*）。提取为共享函数 `ApplyUniform(Material*, name, json)`，`AssetLoader<Material>` 和 `SceneSerializer` 都用，避免重复。

### 步骤3：UniformValueToJson（variant -&gt; JSON）

UniformValue 是 `variant<int, float, vec2, vec3, vec4, mat4, vector<int>>`。用 `std::visit` + `if constexpr`：

```cpp
json UniformValueToJson(const UniformValue& v) {
    return std::visit([](auto&& arg) -> json {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>)           return {{"type","Int"},    {"value", arg}};
        else if constexpr (std::is_same_v<T, float>)     return {{"type","Float"},  {"value", arg}};
        else if constexpr (std::is_same_v<T, glm::vec2>) return {{"type","Float2"}, {"value", {arg.x, arg.y}}};
        else if constexpr (std::is_same_v<T, glm::vec3>) return {{"type","Float3"}, {"value", {arg.x, arg.y, arg.z}}};
        else if constexpr (std::is_same_v<T, glm::vec4>) return {{"type","Float4"}, {"value", {arg.x, arg.y, arg.z, arg.w}}};
        else if constexpr (std::is_same_v<T, glm::mat4>) { /* 16 floats */ }
        else if constexpr (std::is_same_v<T, std::vector<int>>) { /* IntArray */ }
    }, v);
}
```

### 步骤4：SceneSerializer SerializeMesh 扩展

```cpp
mj["meshAsset"] = mc.MeshAsset.GetUUID();
if (!mc.MaterialOverrides.empty()) {
    json overrides = json::array();
    for (size_t i = 0; i < mc.MaterialOverrides.size(); ++i) {
        if (!mc.MaterialOverrides[i]) continue;
        const auto& map = mc.MaterialOverrides[i]->GetOverrides();
        if (map.empty()) continue;
        json ov; ov["submesh"] = i;
        json uniforms;
        for (const auto& [name, val] : map)
            uniforms[name] = UniformValueToJson(val);
        ov["uniforms"] = uniforms;
        overrides.push_back(ov);
    }
    if (!overrides.empty()) mj["materialOverrides"] = overrides;
}
```

### 步骤5：SceneSerializer DeserializeMesh 扩展

```cpp
if (mj.contains("materialOverrides") && mc.Mesh) {
    for (const auto& ov : mj["materialOverrides"]) {
        size_t i = ov.value("submesh", 0);
        if (i >= mc.Mesh->SubMeshes.size()) continue;
        auto base = AssetManager::Get().Load<Material>(mc.Mesh->SubMeshes[i].MaterialAsset);
        if (!base) continue;
        auto mi = DM::CreateRef<MaterialInstance>(base);
        if (ov.contains("uniforms"))
            for (auto& [name, val] : ov["uniforms"].items())
                ApplyUniform(mi.get(), name, val);
        if (mc.MaterialOverrides.size() <= i) mc.MaterialOverrides.resize(i + 1);
        mc.MaterialOverrides[i] = mi;
    }
}
```

### 步骤6：测试

`test_scene.cpp` 加 `RoundTripPreservesMaterialOverrides`：设 MaterialOverrides[0] -&gt; Save -&gt; Load -&gt; 验证 overrides uniform 值保持。

**难点**：MaterialInstance 需 base Material（Load&lt;Material&gt; 需 registry + .mat）。headless 测试要么 mock base，要么只验 JSON 结构。简化策略：只验序列化结构（materialOverrides 字段 + uniform 值），不实际 Load base。

---

## 4. 关键设计点

1. **base 不存**：MaterialInstance 的 base 从 `Mesh.SubMeshes[i].MaterialAsset` 重建（Load&lt;Material&gt;），不重复存。节省 + 避免不一致。
2. **复用 .mat 格式**：uniform 的 {type, value} 格式和 `.mat` 一致，`ApplyUniform` 共享（AssetLoader&lt;Material&gt; + SceneSerializer 都用）。
3. **共享 ApplyUniform**：从 `AssetLoader.cpp` 提取到共享（如 `UniformSerializer.h` 或 AssetLoader.h 公开），避免重复。
4. **std::visit 序列化**：UniformValue variant -&gt; JSON 用 `std::visit` + `if constexpr`（C++17）。
5. **null override 跳过**：MaterialOverrides[i] 为 null 或空 overrides 不序列化（省空间）。

---

## 5. 挑战/风险

| 挑战 | 难度 | 说明 |
|------|------|------|
| UniformValue variant 序列化 | 中 | std::visit + if constexpr，Mat4/IntArray 稍繁琐 |
| 共享 ApplyUniform 提取 | 低 | 从 AssetLoader.cpp 提取到共享头/函数 |
| MaterialInstance GetOverrides | 低 | 加一个 public 方法 |
| base Material 依赖 | 中 | 反序列化需 Load&lt;Material&gt;（registry + .mat），测试需 mock |
| 测试 headless | 中 | 需 base Material，难 headless；简化测结构或留 demo |

---

## 6. 工作量

约 **1-1.5 天**：
- MaterialInstance GetOverrides：10 分钟
- UniformValueToJson + ApplyUniform 共享提取：2-3 小时
- SceneSerializer Serialize/Deserialize 扩展：2-3 小时
- 测试：2-3 小时（含 mock 策略）

---

## 7. 路线总结

```
1. MaterialInstance 加 GetOverrides()                    [小]
2. 提取 ApplyUniform 到共享（AssetLoader + SceneSerializer 都用）  [小]
3. UniformValueToJson（variant -> JSON, std::visit）      [中]
4. SceneSerializer SerializeMesh 加 materialOverrides     [中]
5. SceneSerializer DeserializeMesh 重建 MaterialInstance  [中]
6. 测试 RoundTripPreservesMaterialOverrides              [中，需 mock base]
```

---

## 8. 和现有架构的契合

- 复用 `.mat` 的 uniform {type, value} 格式 + ApplyUniform 逻辑。
- base 不存（从 Mesh 重建），和"材质在 Mesh 资源"的架构一致。
- MaterialInstance 是引擎已有类（base + overrides），序列化只补 overrides 持久化。