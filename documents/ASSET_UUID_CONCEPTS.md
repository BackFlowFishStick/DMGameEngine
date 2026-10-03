# DMGameEngine Asset UUID 映射方案概念

> 性质：概念知识参考文档（讲"为什么需要 UUID + UUID 映射如何设计"）
> 创建：2026-07-27
> 与 ASSET_DESIGN.md 互补：ASSET_DESIGN.md 讲 1a 最小版实现（路径方案）；本文档讲 UUID 方案的概念、数据结构、工作流、对比、演进。
> 适用：理解资源身份与位置分离、资源移动稳定性、依赖追踪、热重载基础。

本文档整理九个主题：
1. 路径方案的局限（为什么需要 UUID）
2. UUID 映射核心：三张表
3. AssetMetadata 结构
4. AssetHandle 升级
5. AssetManager 升级
6. UUID 生成与持久化（三种方案对比）
7. 完整工作流
8. 路径 vs UUID 对比
9. 演进建议

---

# 第一部分：路径方案的局限（动机）

当前 1a 最小版 AssetManager（见 ASSET_DESIGN.md §5）用路径做缓存 key，序列化也存路径。这带来几个问题：

| 问题 | 说明 |
|------|------|
| 路径不稳定 | 资源文件移动/重命名（`assets/textures/wood.png` -> `assets/shared/wood.png`），所有引用该路径的地方（序列化文件、缓存）都失效 |
| 序列化冗长 | 每个资源引用存一长串路径，场景文件膨胀 |
| 不可移植 | 绝对路径跨机器失效；相对路径需约定根目录 |
| 无元数据 | 路径不携带资源类型、导入设置、依赖关系等信息 |
| 无法跨位置去重 | 同一资源出现在两个路径，被当成两份 |

**核心矛盾**：路径是"资源在文件系统中的位置"，会变；而资源引用需要的是"资源的稳定身份"。UUID 方案把这两者分离。

---

# 第二部分：UUID 映射核心：三张表

UUID 方案的核心是维护三张映射表：

```
┌─────────────────────────────────────────────────────────┐
│  UUID  ──┬──> AssetMetadata (path, type, dependencies)   │  表1: UUID -> 元数据
│          │                                                │
│          └──> weak_ptr<void> (cached Ref<T>)             │  表3: UUID -> 缓存
│                                                           │
│  path  ──> UUID                                            │  表2: 路径 -> UUID（反向查找）
└─────────────────────────────────────────────────────────┘
```

- **表1 `UUID -> AssetMetadata`**：UUID 查到资源的元数据（含当前路径、类型、依赖）。
- **表2 `path -> UUID`**：用路径加载时，反查到 UUID，再走表1/表3。让旧的 path-based API 仍可用。
- **表3 `UUID -> weak_ptr<void>`**：缓存已加载的 `Ref<T>`，去重 + 自动释放。

**关键**：`AssetHandle` 只存 UUID（不再存路径）。路径是元数据的一部分，可变；UUID 是身份，不变。

---

# 第三部分：AssetMetadata 结构

```cpp
enum class AssetType : uint8_t {
    None, Shader, Texture2D, TextureCube, Texture2DArray,
    Material, Mesh, Audio
};

struct AssetMetadata
{
    AssetUUID                   UUID = 0;
    std::string                 Path;          // 当前文件路径（可变）
    AssetType                   Type = AssetType::None;
    std::vector<AssetUUID>      Dependencies;  // Material 依赖 Shader + Textures 等
    // 后续可加：导入设置、内容 hash、mtime、...
};
```

`Dependencies` 让资源图可追踪：加载 Material 时自动加载它依赖的 Shader/Textures。这是热重载、依赖失效的基础。

---

# 第四部分：AssetHandle 升级

```cpp
using AssetUUID = uint64_t;   // 64-bit 学习级（2^64 足够，碰撞概率低）；生产用 128-bit

class AssetHandle
{
    AssetUUID m_UUID = 0;     // 只存 UUID，不存路径！
public:
    AssetHandle() = default;
    explicit AssetHandle(AssetUUID uuid) : m_UUID(uuid) {}
    AssetUUID GetUUID() const { return m_UUID; }
    bool IsValid() const { return m_UUID != 0; }
    explicit operator bool() const { return IsValid(); }
    bool operator==(const AssetHandle&) const = default;
};
```

`AssetHandle` 变成纯身份令牌，序列化只存一个 `uint64_t`（8 字节 + JSON 里一个短字符串），不存路径。

---

# 第五部分：AssetManager 升级

```cpp
class AssetManager
{
public:
    static AssetManager& Get();

    // 主入口：按 UUID 加载（序列化反序列化用这个）
    template<typename T>
    DM::Ref<T> Load(AssetUUID uuid);

    // 便利：按路径加载（编辑器拖入资源、临时加载用）
    template<typename T>
    DM::Ref<T> Load(const std::string& path);

    // 注册资源（导入器/编辑器调用）：分配 UUID + 记元数据
    AssetUUID Register(const std::string& path, AssetType type);

    AssetUUID GetUUID(const std::string& path) const;
    const AssetMetadata* GetMetadata(AssetUUID uuid) const;

private:
    std::unordered_map<AssetUUID, AssetMetadata>       m_Registry;     // 表1
    std::unordered_map<std::string, AssetUUID>         m_PathToUUID;   // 表2
    std::unordered_map<AssetUUID, std::weak_ptr<void>>  m_Cache;        // 表3
};
```

## Load<T>(uuid) 流程

```cpp
template<typename T>
DM::Ref<T> Load(AssetUUID uuid)
{
    // 1. 缓存命中?
    if (auto it = m_Cache.find(uuid); it != m_Cache.end())
        if (auto locked = it->second.lock())
            return std::static_pointer_cast<T>(locked);   // cache hit

    // 2. 查元数据拿路径
    auto mit = m_Registry.find(uuid);
    if (mit == m_Registry.end()) return nullptr;
    const auto& meta = mit->second;
    if (meta.Type != AssetTypeOf<T>()) return nullptr;   // 类型校验

    // 3. 加载（递归加载依赖，再加载本体）
    auto resource = AssetLoader<T>::Load(meta.Path);
    if (resource)
        m_Cache[uuid] = std::weak_ptr<void>(std::static_pointer_cast<void>(resource));
    return resource;
}
```

## Load<T>(path) 流程（便利接口）

```cpp
template<typename T>
DM::Ref<T> Load(const std::string& path)
{
    AssetUUID uuid = GetUUID(path);     // 表2 反查
    if (uuid == 0) uuid = Register(path, AssetTypeOf<T>());  // 未注册则注册
    return Load<T>(uuid);
}
```

---

# 第六部分：UUID 生成与持久化（三种方案对比）

这是 UUID 方案最关键的设计点：UUID 怎么生成、存哪、资源移动后怎么保持。

## 方案 A：.meta 旁文件（Unity / Unreal 做法）

每个资源文件旁放一个 `.meta`：
```
assets/textures/wood.png
assets/textures/wood.png.meta   <- { "uuid": 1719234567, "type": "Texture2D" }
```

- UUID 存 .meta，随资源文件移动（用户/编辑器负责 .meta 跟随）。
- 启动扫描所有 .meta，建表1 + 表2。
- 资源移动：文件搬走，.meta 一起搬，UUID 不变；表2 更新 path->uuid。
- 优点：UUID 真正稳定，资源移动/重命名不断引用；行业标准。
- 缺点：每个资源多一个文件；需扫描/导入管线。

## 方案 B：集中 registry 文件（Hazel 做法）

一个 `assets.registry`：
```json
{
  "1719234567": { "path": "assets/textures/wood.png", "type": "Texture2D" },
  "2837465012": { "path": "assets/shaders/flat.glsl", "type": "Shader" }
}
```

- UUID 存单文件，启动加载。
- 资源移动：编辑器更新 registry 里的 path。
- 优点：实现简单，单文件管理。
- 缺点：单点故障（registry 损坏全丢）；资源文件移动需编辑器主动更新 registry。

## 方案 C：内容/路径哈希确定性生成

`uuid = hash(path)` 或 `hash(file_content)`。

- 无存储，路径/内容算出 UUID。
- 优点：零存储，无需 .meta/registry。
- 缺点：路径变则 UUID 变（hash(path)），失去稳定性；hash(content) 内容变则 UUID 变（资源修改算新资源）。违背 UUID 稳定身份的初衷。

## 推荐

| 场景 | 方案 |
|------|------|
| 学习/最小 UUID 版 | 方案 B（集中 registry，实现简单，够用） |
| 编辑器/生产 | 方案 A（.meta 旁文件，行业标准，资源移动稳） |
| 不推荐 | 方案 C（失去 UUID 稳定性） |

---

# 第七部分：完整工作流

## 注册（导入资源时）
```
editor.Import("assets/textures/wood.png", AssetType::Texture2D)
  -> Register() 分配 UUID（随机 64-bit 或从 .meta 读）
  -> m_Registry[uuid] = {path, type}
  -> m_PathToUUID[path] = uuid
```

## 加载
```
handle = AssetManager::Load<Texture2D>(uuid)   // 序列化恢复时
  -> 表3 缓存命中? 返回
  -> 表1 拿 path -> AssetLoader<Texture2D>::Load(path) -> 缓存
```

## 序列化（1c）
```json
{ "Mesh": { "meshAsset": 1719234567, "material": 2837465012 } }
```
存 UUID（短整数），不存路径。

## 反序列化
```
读 uuid -> AssetHandle(uuid) -> AssetManager::Load<Mesh>(uuid)
```

## 资源移动（UUID 的价值体现）
```
wood.png 从 assets/textures/ 移到 assets/shared/
  -> 编辑器更新 registry: m_Registry[uuid].Path = "assets/shared/wood.png"
  -> m_PathToUUID 删旧 path，加新 path -> uuid
  -> UUID 不变！所有序列化引用（存 UUID）仍有效 ✅
```

路径方案下，移动 wood.png 会让所有存 `"assets/textures/wood.png"` 的序列化引用失效。UUID 方案下完全无感。

---

# 第八部分：路径 vs UUID 对比

| 维度 | 路径方案（1a 最小版） | UUID 方案 |
|------|---------------------|----------|
| 标识 | 路径（可变） | UUID（稳定身份） |
| 序列化 | 存路径字符串（长） | 存 UUID 整数（短） |
| 资源移动 | 引用全失效 | 引用有效（UUID 不变） |
| 资源去重 | 按路径 | 按 UUID（跨位置同一资源） |
| 元数据 | 无 | 有（类型/依赖/导入设置） |
| 依赖追踪 | 无 | 有（Material -> Shader + Textures） |
| 热重载基础 | 无 | 有（mtime + UUID 失效依赖） |
| 实现复杂度 | 低 | 中（需 registry + 注册/扫描） |
| 启动开销 | 直接 Load | 需扫描建 registry |

---

# 第九部分：演进建议

## 当前 1a 最小版（路径）够用的场景
- 1c 序列化跑通（存路径，反序列化 Load path）。
- 学习阶段，资源不常移动。

## UUID 方案价值显现的时点
1. **1c 序列化往返**：资源移动过，存路径的旧场景文件断引用。
2. **编辑器（2a）资源浏览器**：用户在浏览器选资源，引用应存 UUID（不是路径），否则用户移动资源就断。
3. **依赖追踪/热重载**：Material 改了 Shader 引用，要失效依赖，需元数据。

## 推荐升级时机
**在 1c 序列化之前或之中升级到 UUID 方案（方案 B 集中 registry）**，理由：
- 1c 序列化是 UUID 价值的第一次体现（存 UUID 而非路径）。
- 方案 B 实现不复杂（一个 registry 文件 + 三张表），比 .meta 旁文件简单。
- 升级后 1c 一次做对（存 UUID），避免"先存路径，后改 UUID"的返工。
- 1a 最小版的 `AssetLoader<T>` / `weak_ptr` 缓存逻辑完全复用，只加 registry 层。

## 升级工作量（方案 B）
- `AssetUUID` 类型 + `AssetMetadata` 结构
- `AssetManager` 加 `m_Registry` + `m_PathToUUID`，`Load<T>(uuid)` / `Load<T>(path)`（path 解析 uuid）/ `Register`
- `AssetHandle` 改存 UUID
- registry 文件加载/保存（JSON）
- 现有 `AssetLoader<T>` / 缓存逻辑不动

约 1-2 天，不大。

## 演进路径
- 当前（1a 最小版）：路径方案 ✅
- 1a 中期：UUID + AssetMetadata + 集中 registry
- 1a 完整版：.meta 旁文件 + 编辑器资源浏览器 + 导入管线 + 依赖追踪 + 热重载

---

# 一句话总结

> **UUID 映射把"资源身份"（UUID，稳定）和"资源位置"（路径，可变）分离**：维护 `UUID->元数据`、`路径->UUID`、`UUID->缓存` 三张表，`AssetHandle` 只存 UUID，序列化存 UUID（短且稳定），资源移动只更新元数据里的路径而不动 UUID，引用不断。
>
> **三种 UUID 持久化方案**：.meta 旁文件（Unity/Unreal，生产级）、集中 registry 文件（Hazel，学习级，推荐）、内容哈希（不推荐，失去稳定性）。
>
> **当前 1a 最小版用路径够跑通 1c**，但 UUID 方案的价值在"资源移动不断引用 + 元数据 + 依赖追踪"，建议在 1c 序列化之前/之中升级到方案 B（集中 registry），让 1c 一次存 UUID 做对，避免返工。