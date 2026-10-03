# DMGameEngine 场景序列化方案选型

> 性质：方案调研与技术介绍文档（为 1c 序列化选型）
> 创建：2026-07-28
> 关联：ECS_DESIGN.md（1b，Entity/Component）、ASSET_DESIGN.md / ASSET_UUID_CONCEPTS.md（1a，AssetHandle UUID）
> 目标：给出场景文件序列化的若干可选方案 + 每个方案所用技术的介绍，供选型决策。

---

## 0. 文档结构

1. 序列化需求分析（场景文件存什么、约束）
2. 格式选项（JSON / 二进制 / TOML / YAML / 自定义文本）
3. 架构选项（手写 / 反射 / 序列化库自动绑定）
4. 五个组合方案（A-E，含技术栈 + 优缺点 + 适用）
5. 对比表
6. 推荐（基于引擎现状）
7. 技术库附录（nlohmann/json、cereal、FlatBuffers、yaml-cpp、toml++、反射库）

---

## 1. 序列化需求分析

### 1.1 场景文件要存什么

```
.scene 文件
├─ 场景元数据（版本号、引擎版本、UUID 生成器种子等）
└─ 实体列表
   └─ 每个实体：
      ├─ IDComponent     UUID（持久身份）
      ├─ TagComponent    Tag（名字）
      ├─ TransformComponent  local 变换 + 层级链
      │   （Parent 用父实体的 UUID 引用，非运行时 Entity ID）
      ├─ MeshComponent   meshAsset / material 的 AssetHandle（UUID）
      ├─ CameraComponent 参数（FOV/near/far/类型/Primary）
      └─ ...其他 Component
```

关键点：
- **层级引用用 UUID**：Parent/FirstChild/NextSibling 存的是运行时 Entity ID（每次加载变），序列化必须用 IDComponent UUID 做持久引用，反序列化时建 `UUID -> 新Entity` 映射表重建三叉链。
- **资源引用用 AssetHandle UUID**：MeshComponent 的 Material/Mesh 存 AssetUUID（见 ASSET_UUID_CONCEPTS.md），反序列化时 AssetManager::Load<T>(uuid) 恢复。
- **world matrix 不存**：TransformComponent 只存 local 变换（Translation/RotationEuler/Scale），world matrix 运行时由 TransformSystem 算。

### 1.2 约束与取舍维度

| 维度 | 取舍 |
|------|------|
| 人类可读 | 调试友好（JSON/YAML）vs 紧凑（二进制） |
| 文件大小 | JSON 约 2-5x 二进制 |
| 解析速度 | 二进制最快；JSON 中等（simdjson 极快但只读） |
| 浮点精度 | 二进制位精确；JSON 需配置位数 |
| 版本控制友好 | 文本格式 diff 友好；二进制不可 diff |
| schema 校验 | FlatBuffers/Protobuf 有 schema；JSON 无（靠手动） |
| 向前/向后兼容 | schema 驱动（FlatBuffers）最好；手写需版本字段 |
| 实现成本 | 手写序列化低；反射/库引入中；codegen 流程高 |
| 现有依赖 | nlohmann/json 已引入（UUID 升级时）；其他需新引 |

---

## 2. 格式选项

### 2.1 JSON

**技术**：nlohmann/json（已引入，header-only，MIT，C++11+）。API 直观：`j["key"] = value`、`j.dump(4)` 格式化、`j.parse()`。

**特点**：
- 人类可读可编辑，调试友好（文本编辑器/diff 直接看）。
- nlohmann/json 支持任意嵌套、STL 容器自动适配、异常报错清晰。
- 文件冗长（key 重复、引号、括号），约为二进制 2-5x。
- 解析中等速度（nlohmann/json 非 SIMD；simdjson 快 10x 但只解析不构造 C++ 对象）。
- 浮点默认 17 位有效数字（可精确往返），可配。

**示例**：
```json
{
  "version": 1,
  "entities": [
    {
      "uuid": 1719234567,
      "tag": "Player",
      "transform": { "translation": [1.0, 2.0, 3.0], "rotation": [0,0,0], "scale": [1,1,1], "parent": 0 },
      "mesh": { "meshAsset": 2837465, "material": 9988776 }
    }
  ]
}
```

### 2.2 二进制

紧凑、快、浮点位精确。子选项：

| 子选项 | 技术 | 特点 |
|--------|------|------|
| 自定义二进制 | 手写 fwrite/fread | 完全控制；要自己处理版本/对齐/字节序；无库依赖 |
| **cereal** | header-only C++ 序列化库（BSD），类内 `serialize(Archive&)` | 多 archive（二进制/JSON/XML）、少代码、侵入式 |
| **FlatBuffers** | Google，schema(.fbs) + codegen | 零拷贝反序列化、跨语言、schema 校验、向前向后兼容 |
| Cap'n Proto | 类似 FlatBuffers | 类似，schema + codegen |
| MessagePack | JSON-like 二进制 | 紧凑 JSON；nlohmann/json 有 msgpack adaptor |
| Bitsery | 轻量单头二进制 | 极简、快、配置少 |

### 2.3 TOML

**技术**：toml++（header-only，MIT，C++17）/ toml11。

**特点**：
- 人类可读，`key = value` + `[section]`，比 JSON 更易读（key 无引号）、注释友好。
- 不适合深嵌套/大数组（设计为配置文件），场景文件层级深时笨拙。
- 库 API 不如 nlohmann/json 优雅（构造嵌套繁琐）。

**适用**：配置文件、引擎设置；不太适合场景（层级深、实体多）。

### 2.4 YAML

**技术**：yaml-cpp（MIT，C++）。

**特点**：
- 可读性最好，支持引用（锚点 `&`/别名 `*`，避免重复资源定义）。
- Unity 用 YAML（.unity 场景文件）。
- 解析复杂、缩进敏感易错、安全风险（YAML billion laughs 攻击，大文件爆炸）。
- 库 API 较繁琐。

### 2.5 自定义文本

**技术**：手写解析器（如 Godot `.tscn`、Unreal `.t3d`）。

**特点**：
- 完全控制格式，可针对场景优化（如 Godot 的资源内联）。
- 工作量大（写 lexer/parser + 文档 + 版本兼容）。
- 无库依赖，但要维护解析器。

---

## 3. 架构选项

### 3.1 手写 Serialize/Deserialize

**技术**：每个 Component 写 `to_json(json&, const T&)` / `from_json(const json&, T&)`；Scene 持 `map<string, SerializeFn>` 类型注册表，Save/Load 遍历实体调各 Component 的函数。

**特点**：
- 简单、控制强、调试直观（每个字段手写）。
- 加 Component 要加 serialize 代码（易遗漏字段，但显式）。
- Hazel 用此方式。
- 无反射系统依赖。

### 3.2 反射驱动

**技术**：Component 字段反射（字段名 + 类型 + 偏移），自动序列化遍历反射元数据写所有字段。

反射来源：
- **手写注册表**：每 Component 注册字段元数据（`map<string, FieldInfo>`）。
- **RTTR**（反射库，MIT）：运行时反射，类注册属性/方法。
- **magic_enum**（仅枚举反射，header-only）。
- **C++26 静态反射**（未来，尚未可用）。

**特点**：
- 加 Component 只注册反射、不写 serialize 代码、不易遗漏。
- 反射系统本身复杂（手写注册表也要写；库引入依赖）。
- C++ 无内置反射。

### 3.3 序列化库自动绑定（cereal）

**技术**：类内写 `template<class Archive> void serialize(Archive& ar) { ar(NVP(x), NVP(y)); }`，cereal 自动处理二进制/JSON/XML archive。

**特点**：
- 少代码、多格式（一份 serialize 多 archive）。
- 侵入式（类要加 serialize 方法、适配 cereal 概念）。
- 调试不如手写直观（字段在宏里）。

---

## 4. 五个组合方案

### 方案 A：JSON + 手写（Hazel 风格）⭐

**技术栈**：nlohmann/json（已引入）+ Component 类型注册表（`map<string, function>`) + 手写 to_json/from_json。

**工作流**：
1. 每 Component 写 `to_json`/`from_json`（nlohmann/json 的 ADL 风格）。
2. Scene 持 `unordered_map<string, pair<SerializeFn, DeserializeFn>>`，每 Component 类型注册。
3. `Scene::Save(path)`：遍历实体，每实体写 UUID/Tag/各 Component（按注册表调 serialize）。
4. `Scene::Load(path)`：读 JSON，建 `UUID -> 新Entity` 映射，重建实体 + 层级链 + 资源引用（AssetManager::Load）。
5. 往返测试：save -> load -> 字段相等。

**优点**：nlohmann/json 现成（零新依赖）、手写清晰、调试友好、和 Hazel/教程一致、1c 阶段够用、文本可 diff。

**缺点**：加 Component 要加 serialize 代码；文件较大；解析非最快。

**适用**：学习引擎、调试友好、场景不大。**推荐起步方案。**

---

### 方案 B：cereal 二进制 + 自动绑定

**技术栈**：cereal（header-only）+ 类内 `serialize(Archive&)` + 二进制 archive。

**工作流**：
1. Component 类加 `template<class A> void serialize(Archive& ar) { ar(x, y, z); }`。
2. `Scene::Save`：`cereal::BinaryOutputArchive ar(ostream); ar(entities);`。
3. `Scene::Load`：`cereal::BinaryInputArchive ar(istream); ar(entities);`。

**优点**：少代码、二进制紧凑快、浮点精确、一份 serialize 多 archive。

**缺点**：不可读（调试难）、cereal 引入、侵入式（Component 类要适配 cereal）、版本兼容要手写（cereal 有版本支持但需声明）。

**适用**：生产、大场景、性能敏感、不需人读。

---

### 方案 C：FlatBuffers（schema 驱动）

**技术栈**：FlatBuffers（Google）+ schema(.fbs) + flatc codegen 生成 C++ + 零拷贝反序列化。

**工作流**：
1. 写 schema：`table TransformComponent { translation:Vec3; ... } table Entity { uuid:ulong; components:[...]; } root_type Scene;`
2. flatc 生成 C++ 头。
3. Save：构建 FlatBuffer builder，写入。
4. Load：`GetScene(buf)` 零拷贝访问（不解析，直接指针）。

**优点**：零拷贝（反序列化 = 指针解引用，极快）、schema 校验、向前向后兼容（字段加 ID）、跨语言。

**缺点**：schema 维护、codegen 流程（构建步骤加 flatc）、学习曲线、不可读（有反射文本 dump 但不便）。

**适用**：高性能、跨语言、严格 schema、生产引擎。

---

### 方案 D：JSON + 反射

**技术栈**：nlohmann/json + 反射注册表（手写或 RTTR）。

**工作流**：
1. 每 Component 注册字段反射（`map<string, FieldInfo{type, offset}>`）。
2. `Scene::Save`：遍历反射元数据，自动 `j[fieldName] = *(T*)((char*)&component + offset)`。
3. `Load`：反向。

**优点**：加 Component 只注册反射、自动序列化、不易遗漏字段。

**缺点**：反射系统本身复杂（手写注册表也要写；RTTR 引入依赖）、类型擦除的 uniform 处理繁琐、调试不如手写直观。

**适用**：Component 多、想减少手写、可投入做反射系统。

---

### 方案 E：YAML + 手写（Unity 风格）

**技术栈**：yaml-cpp + 手写序列化。

**工作流**：类似方案 A，但用 yaml-cpp 而非 nlohmann/json。

**优点**：可读性最好、引用减少重复（资源内联）。

**缺点**：yaml-cpp 引入（新依赖）、YAML 解析复杂、缩进敏感易错、安全（YAML bomb）。

**适用**：追求可读性、和 Unity 工作流一致。

---

## 5. 对比表

| 方案 | 格式 | 架构 | 可读 | 大小 | 速度 | 依赖 | 实现成本 | schema | 兼容性 |
|------|------|------|------|------|------|------|----------|--------|--------|
| A ⭐ | JSON | 手写 | ✅ | 大 | 中 | nlohmann/json（已有） | 低 | 无 | 手写版本字段 |
| B | 二进制 | cereal 自动 | ❌ | 小 | 快 | cereal（新） | 中 | 无 | cereal 版本 |
| C | 二进制 | FlatBuffers | ❌ | 小 | 极快(零拷贝) | FlatBuffers+codegen（新） | 高 | ✅ | ✅ 字段 ID |
| D | JSON | 反射 | ✅ | 大 | 中 | nlohmann/json + 反射库 | 中高 | 无 | 手写 |
| E | YAML | 手写 | ✅✅ | 中 | 慢 | yaml-cpp（新） | 中 | 无 | 手写 |

---

## 6. 推荐：方案 A（JSON + 手写）

### 理由

1. **零新依赖**：nlohmann/json 已在 UUID 升级时引入（AssetManager.cpp 用），1c 直接复用。
2. **手写清晰**：每个 Component 的 serialize 显式，调试直观，字段遗漏易发现。
3. **调试友好**：.scene 是文本，编辑器/diff 直接看，出问题易定位。
4. **和 Hazel/教程一致**：学习曲线低，社区资源多。
5. **1c 阶段够用**：学习引擎场景不大，JSON 大小/速度可接受。
6. **后续可演进**：先 JSON 手写跑通，再加反射（方案 D）减手写，或加二进制 archive（cereal）提性能。

### 演进路径

- **1c 起**：方案 A（JSON + 手写 + 类型注册表）。
- **后续（可选）**：加反射注册表（方案 D）减少手写；或加 cereal 二进制 archive 作高性能备选（大场景）。

---

## 7. 技术库附录

### 7.1 nlohmann/json（已引入）
- MIT，header-only（单头 json.hpp），C++11+。
- API：`json j; j["k"] = v; j.dump(4); parse(str)`。
- STL 容器自动适配（vector/map 等）。
- ADL `to_json`/`from_json` 支持自定义类型序列化。
- 支持 MessagePack / CBOR / BSON 等 binary archive（adaptor）。
- 已在引擎 CMake 引入（UUID 升级），AssetManager.cpp 用。

### 7.2 cereal
- BSD，header-only，C++11+。
- 类内 `template<class A> void serialize(Archive& ar){ ar(x,y); }`。
- archive：BinaryOutputArchive / JSONOutputArchive / XMLOutputArchive。
- 支持版本（`ar(version)`）、多态、指针（shared_ptr）。
- 侵入式（类要适配）。

### 7.3 FlatBuffers
- Apache 2.0，Google。
- schema(.fbs) + flatc codegen 生成 C++/Java/...
- 零拷贝反序列化（缓冲区直接访问，不解析）。
- 向前向后兼容（字段加 ID，旧读新忽略）。
- 跨语言。

### 7.4 yaml-cpp
- MIT，C++。
- 解析/发射 YAML。
- 支持锚点/别名（引用）。
- API 较繁琐（Node 树操作）。

### 7.5 toml++
- MIT，header-only，C++17。
- 解析/发射 TOML。
- 配置文件友好，不适合深嵌套场景。

### 7.6 反射库
- **RTTR**（MIT）：运行时反射，类注册属性/方法/构造，支持序列化。
- **magic_enum**（MIT，header-only）：仅枚举反射（枚举名 <-> 值），可辅助 AssetType 序列化。
- **手写注册表**：`map<string, FieldInfo{type_index, offset}>`，每 Component 注册字段。无依赖，但要写注册代码。
- **C++26 静态反射**（未来）：编译期反射，尚未标准化可用。

---

## 8. 引擎现状与建议

### 现状
- nlohmann/json 已引入（AssetManager registry 持久化用）。
- ECS 已就绪（Entity/Component/Scene，IDComponent UUID 预留）。
- AssetManager UUID 方案已就绪（资源引用存 UUID）。

### 建议
1. **采用方案 A（JSON + 手写）**：零新依赖，和 Hazel 一致，1c 起步。
2. **Component 类型注册表**：Scene 持 `map<string, SerializeFn>`，每 Component 注册 to_json/from_json。新增 Component 时注册。
3. **层级重建用 UUID 映射**：序列化 Parent 存父 UUID，反序列化建 `UUID->Entity` 表重建三叉链。
4. **资源引用用 AssetHandle UUID**：MeshComponent 的 Material/Mesh 存 AssetUUID，反序列化 AssetManager::Load<T>(uuid)。
5. **版本字段**：JSON 顶层加 `"version": 1`，为后续兼容预留。
6. **往返测试**：save -> load -> 字段相等（含 Transform 层级 + 资源引用）。

### 后续演进
- 反射注册表（方案 D）减少手写。
- cereal 二进制 archive 作高性能备选（大场景）。
- schema 驱动（FlatBuffers）若需跨语言/严格兼容。

---

## 9. 一句话总结

> **场景序列化五方案**：A（JSON+手写，Hazel 风格）、B（cereal 二进制）、C（FlatBuffers schema）、D（JSON+反射）、E（YAML+手写，Unity 风格）。
>
> **推荐方案 A**：nlohmann/json 已引入（零新依赖）、手写清晰、调试友好、和 Hazel 一致、1c 够用，后续可演进反射或二进制。
>
> **关键技术**：Component 类型注册表 + UUID 映射重建层级 + AssetHandle UUID 存资源引用 + 版本字段 + 往返测试。