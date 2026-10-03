# DMGameEngine AssetManager 实现设计（阶段 1a 最小版）

> 状态：设计 + 实现（1a 最小版）
> 创建：2026-07-27
> 分支：main
> 对应路线图：ENGINE_ROADMAP.md 阶段 1a（AssetManager + ShaderLibrary，E3）
> 关联文档：ECS_DESIGN.md（MeshComponent 持 Ref&lt;Material&gt;）、ECS_CONCEPTS.md

---

## 1. 目标与范围

### 1.1 目标
提供统一资源加载入口 + 缓存去重，让资源（Shader/Texture2D/Material）成为可被引用、可去重、可生命周期管理的 first-class 对象。为 1c 序列化（资源引用存 AssetHandle）和编辑器（2a 资源选择）打基础。

### 1.2 范围（1a 最小版交付）
- AssetHandle（路径标识）
- AssetManager（单例 + Load&lt;T&gt; + weak_ptr 缓存去重）
- AssetLoader&lt;T&gt;（Shader / Texture2D 特化）
- 接入公共 API（DMGameEngine.h）

### 1.3 不做（留后续 1a 完整版）
- 异步加载（LoadAsync + Job System + 主线程 GPU 上传）
- 热重载（监听文件 mtime）
- 延迟释放队列（GPU 资源等帧完成，对齐 Vulkan 0a-B）
- Material 的 AssetLoader（需 .mat 文件格式，留 1c 序列化时一起设计）
- ShaderLibrary 收敛（现有 ShaderLibrary 保留，AssetManager 是新统一入口）

### 1.4 前置依赖
- 无强依赖。现有 Shader::Create(filepath) / Texture2D::Create(filepath) 工厂已存在，AssetLoader 直接复用。

---

## 2. 架构决策

- **AssetHandle 用路径作标识**（最小版）：简单，足够 1c 序列化用。完整版可升级为 UUID + 路径映射。
- **weak_ptr 缓存**：引用归零自动释放（隐式引用计数），无需手动延迟释放逻辑。Load 时 lock 检查，过期则重新加载。
- **类型擦除缓存**：map&lt;type_index, map&lt;path, weak_ptr&lt;void&gt;&gt;&gt;，static_pointer_cast 转回 T。一种容器存所有类型。
- **AssetLoader 模板特化**：每种资源类型特化 Load。新增类型时特化。
- **单例 AssetManager**：Meyers singleton（static local），线程安全初始化（C++11+）。

---

## 3. 目录结构

```
src/DMGameEngine/Asset/
├── AssetHandle.h          # AssetHandle（路径标识）
├── AssetLoader.h          # AssetLoader<T> 模板 + Shader/Texture2D 特化（header-only）
├── AssetManager.h         # AssetManager 单例 + Load<T> 模板（header-only 模板）
└── AssetManager.cpp       # Get() + CleanUnused() 非模板实现
```

---

## 4. AssetHandle

```cpp
class AssetHandle
{
public:
    AssetHandle() = default;
    explicit AssetHandle(std::string path);
    explicit AssetHandle(std::string_view path);
    const std::string& GetPath() const;
    bool IsValid() const;
    explicit operator bool() const;
    bool operator==(const AssetHandle&) const = default;
private:
    std::string m_Path;
};
```

- 序列化时存 AssetHandle（路径字符串）。
- 反序列化时 AssetManager::Load&lt;T&gt;(handle) 恢复 Ref&lt;T&gt;。

---

## 5. AssetManager

```cpp
class AssetManager
{
public:
    static AssetManager& Get();

    template<typename T>
    DM::Ref<T> Load(const std::string& path);      // by path

    template<typename T>
    DM::Ref<T> Load(const AssetHandle& handle);    // by handle

    template<typename T>
    bool IsLoaded(const std::string& path) const;   // still alive in cache?

    void CleanUnused();   // drop expired weak_ptr entries

private:
    AssetManager();
    std::unordered_map<std::type_index,
                       std::unordered_map<std::string, std::weak_ptr<void>>> m_Cache;
};
```

Load&lt;T&gt; 流程：
1. 查 m_Cache[typeid(T)][path] 的 weak_ptr
2. lock() 成功 -&gt; 缓存命中，static_pointer_cast&lt;T&gt; 返回
3. 过期 -&gt; 移除，AssetLoader&lt;T&gt;::Load(path)
4. 存 weak_ptr&lt;void&gt;（从 shared_ptr&lt;T&gt; static_pointer_cast&lt;void&gt;）
5. 返回 Ref&lt;T&gt;

---

## 6. AssetLoader&lt;T&gt;

```cpp
template<typename T> struct AssetLoader;  // must specialize

template<> struct AssetLoader<Shader> {
    static DM::Ref<Shader> Load(const std::string& path) { return Shader::Create(path); }
};

template<> struct AssetLoader<Texture2D> {
    static DM::Ref<Texture2D> Load(const std::string& path) { return Texture2D::Create(path); }
};
```

复用现有工厂。Material 特化留后续（需 .mat 格式）。

---

## 7. ShaderLibrary 整合策略

- 现有 ShaderLibrary 保留（按名索引，Add/Load/Get/Exists）。
- AssetManager 是新的统一路径加载入口（去重 + 缓存 + 类型擦除，覆盖所有资源类型）。
- 后续可让 ShaderLibrary::Load(filepath) 委托 AssetManager::Load&lt;Shader&gt;(filepath)，收敛为单一缓存。
- 最小版不动 ShaderLibrary，避免破坏现有调用方。

---

## 8. 与 1c 序列化的关系

- 1c 序列化 MeshComponent 时，Material/Mesh 引用存 AssetHandle（路径），反序列化时 AssetManager::Load&lt;T&gt;(handle) 恢复。
- AssetManager 的去重让同一材质被多个实体引用时只加载一份。
- weak_ptr 缓存让场景 unload 时资源自动释放。
- 详见 ECS_CONCEPTS.md 关联讲解。

---

## 9. 实现步骤

1. 写 AssetHandle.h
2. 写 AssetLoader.h（模板 + Shader/Texture2D 特化）
3. 写 AssetManager.h/.cpp（单例 + Load&lt;T&gt; + 缓存 + CleanUnused）
4. 接入 CMakeLists.txt（DMGE_SOURCES + DMGE_HEADERS）
5. 接入 DMGameEngine.h（Asset include 块）
6. 编译验证

---

## 10. 后续演进（1a 完整版）

- AssetHandle 升级 UUID + 路径映射
- AssetLoader&lt;Material&gt;（.mat 文件格式）
- 异步加载（LoadAsync + Job System）
- 热重载（mtime 监听）
- 延迟释放队列（GPU 资源）
- ShaderLibrary 委托 AssetManager

---

## 11. 参考
- ECS_DESIGN.md（MeshComponent 用 Ref&lt;Material&gt;）
- ENGINE_ROADMAP.md 阶段 1a

---

## 12. UUID 方案（补充章节）

> 详见 `ASSET_UUID_CONCEPTS.md`（概念详述）。本节为索引性概述。

### 12.1 为什么需要 UUID
当前路径方案（§5）的局限：路径不稳定（资源移动断引用）、序列化冗长、无元数据/依赖追踪。UUID 把"资源身份"（稳定）与"资源位置"（可变）分离。

### 12.2 UUID 方案核心
- 维护三张表：`UUID->AssetMetadata`、`path->UUID`、`UUID->缓存(weak_ptr)`。
- `AssetHandle` 只存 UUID（不再存路径），序列化存 UUID（短且稳定）。
- `AssetMetadata` 含 path + type + dependencies（依赖追踪基础）。
- 资源移动：只更新元数据里的 path，UUID 不变，引用不断。

### 12.3 UUID 持久化三种方案
| 方案 | UUID 存储 | 资源移动 | 复杂度 | 适用 |
|------|----------|----------|--------|------|
| A. .meta 旁文件 | 每资源一 .meta | .meta 跟随，UUID 稳 | 中 | 生产（Unity/Unreal） |
| B. 集中 registry | 单文件 | 编辑器更新 registry | 低 | 学习（Hazel），推荐 |
| C. 内容哈希生成 | 无存储 | 路径/内容变则 UUID 变 | 低 | 不推荐（失去稳定性） |

### 12.4 推荐
1a 最小版（路径）够 1c 跑通；建议在 **1c 序列化之前/之中升级到方案 B（集中 registry）**，让 1c 一次存 UUID 做对，避免"先存路径后改 UUID"返工。现有 `AssetLoader<T>` / `weak_ptr` 缓存逻辑复用，只加 registry 层。

### 12.5 演进路径
- 当前（1a 最小版）：路径方案 ✅（已实现）
- 1a 中期：UUID + AssetMetadata + 集中 registry（方案 B）
- 1a 完整版：.meta 旁文件（方案 A）+ 编辑器资源浏览器 + 导入管线 + 依赖追踪 + 热重载

### 12.6 升级工作量（方案 B）
`AssetUUID` + `AssetMetadata` + `AssetManager` 加 `m_Registry`/`m_PathToUUID` + `Load(uuid)` + `Register` + registry JSON 加载/保存。现有 `AssetLoader<T>` / 缓存不动。约 1-2 天。