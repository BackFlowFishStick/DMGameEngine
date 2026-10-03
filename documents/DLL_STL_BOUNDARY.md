# STL 容器跨 DLL 边界问题

> 创建日期：2026-08-03
> 来源：DMGameEngine 实例化渲染开发中遇到的实际崩溃

---

## 1. 什么是 DLL 边界

当引擎编译为 DLL（`DMGE_BUILD_SHARED=ON`）时，内存空间被分为两个独立的编译单元：

```
┌─────────────────────────────┐    ┌──────────────────────────────┐
│  DMGameEngine.dll           │    │  DMGameDemo.exe              │
│  (引擎，编译时 DMGE_BUILD_DLL) │    │  (游戏，不定义 DMGE_BUILD_DLL)  │
│                             │    │                              │
│  class DMGE_API RenderQueue │    │  #include "RenderQueue.h"    │
│  {                          │    │  // 看到 dllimport 版本        │
│      vector<...> m_Queue;   │    │                              │
│      vector<...> m_InstancedQueue; │                          │
│  };                         │    │  // 调用导出方法              │
│                             │    │  Renderer::SubmitInstanced() │
│  s_Queue 实例生活在这里       │    │  // DLL 内部操作 s_Queue     │
│  (DLL 的 .data 段)           │    │                              │
└─────────────────────────────┘    └──────────────────────────────┘
         ▲                                      │
         │    函数调用跨过 DLL 边界              │
         └──────────────────────────────────────┘
```

**"跨 DLL 边界"** = 一个编译单元（exe）创建/传递的对象，被另一个编译单元（dll）访问，或反过来。

---

## 2. 为什么 STL 容器跨边界会出问题

`std::vector` 不是"一个指针"。在 MSVC 实现中，它内部有多个字段：

```cpp
// MSVC 的 std::vector 内部结构（简化）
struct vector {
    pointer _Myfirst;   // 数据缓冲区起始
    pointer _Mylast;    // 已使用末尾
    pointer _Myend;     // 容量末尾
    // 调试模式下还有更多字段：
    #if _HAS_ITERATOR_DEBUGGING
        _Container_proxy* _Myproxy;  // 迭代器调试链表
    #endif
};
```

问题在于：**exe 和 DLL 各自编译 `RenderQueue.h` 时，看到的 `vector` 大小可能不同**。

### 核心原因：`_HAS_ITERATOR_DEBUGGING`（迭代器调试）

MSVC 的 STL 在 Debug 模式下默认开启 `_ITERATOR_DEBUG_LEVEL=2`，它会：
- 在 `vector` 内部增加 `_Myproxy` 字段（迭代器安全链表）
- 在堆分配时添加调试头/尾 cookie（检测缓冲区溢出）
- 给容器分配更大的内存

```cpp
// DLL 编译时（Debug, /MDd, 有 PCH）:
sizeof(vector<InstancedRenderable>) = 40 bytes  // 含 _Myproxy

// exe 编译时（Debug, /MDd, 无 PCH）:
sizeof(vector<InstancedRenderable>) = 40 bytes  // 理论上相同...

// 但如果编译选项有任何差异（比如 PCH 中包含的不同头文件影响了宏）:
sizeof(vector<InstancedRenderable>) = 32 bytes  // 不含 _Myproxy
```

当 `sizeof(vector)` 不一致时：

```
RenderQueue 对象在 DLL 内存中的布局:

  DLL 视角:  [m_Queue(40B) | m_InstancedQueue(40B) | ...]
  exe 视角:  [m_Queue(32B) | m_InstancedQueue(32B) | ...]
                                ↑
                     exe 计算的偏移量与 DLL 不同！
                     exe 通过偏移量访问到的是错误的内存位置！
```

---

## 3. 实际案例：DMGameEngine 的崩溃

### 问题代码

```cpp
// RenderQueue.h（DMGE_API 类，DLL 导出）
class DMGE_API RenderQueue {
    std::vector<Renderable>         m_Queue;          // 也在类中
    std::vector<InstancedRenderable> m_InstancedQueue; // ← 问题成员
};
```

编译器警告：
```
warning C4251: 'DMGameEngine::RenderQueue::m_InstancedQueue':
'std::vector<DMGameEngine::InstancedRenderable>' needs to have dll-interface
to be used by clients of 'DMGameEngine::RenderQueue'
```

这个警告的意思是：**`m_InstancedQueue` 是 `DMGE_API` 类的成员，但 `std::vector<InstancedRenderable>` 本身没有 `DMGE_API`，它的内存布局对 exe 消费者不可靠。**

### 崩溃时间线

```
Frame 1:
  1. exe 调用 Renderer::SubmitInstanced() -> 进入 DLL
  2. DLL 内 s_Queue.SubmitInstanced()
  3. m_InstancedQueue.push_back(...) -> 分配内存，写入数据 ✅
  4. DLL 内 Flush() -> DrawIndexedInstanced -> clear() ✅

Frame 2:
  1. exe 调用 Renderer::SubmitInstanced() -> 进入 DLL
  2. DLL 内 s_Queue.SubmitInstanced()
  3. m_InstancedQueue.push_back(...) -> 💥 崩溃
     // vector 的 data/size/capacity 指针已被损坏
     // 第一帧的 clear() 和第二帧的 push_back 对同一块内存
     // 的理解不一致（迭代器调试链表指针错乱）
```

第一帧成功是因为 `push_back` 在空 vector 上只需分配（`cap=0`）。第二帧崩溃是因为 `push_back` 在 `cap=1, size=0` 的 vector 上需要操作已有缓冲区，而缓冲区的内部簿记已被损坏。

### 诊断过程

通过逐步添加 `fprintf(stderr, ...)` 诊断，精确定位到：

1. **Frame 1** 全流程成功（SubmitInstanced -> push_back OK -> Flush -> DrawIndexedInstanced -> clear）
2. **Frame 2** 在 `push_back` 调用时崩溃（`cap=1, size=0`，不需要分配内存）
3. `reserve(16)` 预分配无效 -- 崩溃不在内存分配，而在元素构造
4. 逐步赋值（`r.Material = material; r.VertexArray = vertexArray; ...`）成功，但 `push_back(std::move(r))` 崩溃
5. 改用 `emplace_back()` 仍在同一位置崩溃

结论：vector 的内部缓冲区指针/状态已损坏，不是参数问题。

### 为什么 `m_Queue` 没出问题

`m_Queue`（`vector<Renderable>`）也是 `DMGE_API` 类的成员，也有同样的 C4251 警告。但它没有崩溃，因为：

- `m_Queue` 的操作（`push_back`、`clear`、迭代）**全部在 DLL 内部完成**
- exe 从不直接访问 `m_Queue`
- `Renderer::Submit()` 是 DLL 导出函数，exe 调用它传入参数，DLL 内部操作 `m_Queue`

`m_InstancedQueue` 理论上也应该只在 DLL 内部操作。但 **exe 编译 `MeshRenderSystem.h` 时包含了 `RenderQueue.h`**，编译器需要知道 `RenderQueue` 的完整布局（包括 `m_InstancedQueue` 的大小和偏移）。如果 exe 和 DLL 对 `vector` 大小的理解不同，即使 exe 不直接访问 `m_InstancedQueue`，内存布局的错位也会影响 `m_Queue` 等相邻成员。

---

## 4. 常见解决方案

| 方案 | 做法 | 优缺点 |
|------|------|--------|
| **固定数组**（本项目的修复） | 用 `static InstancedRenderable[32]` 替代 vector | ✅ 简单、零边界问题；❌ 固定容量 |
| **Pimpl 惯用法** | `DMGE_API class RenderQueue { struct Impl; Impl* m_Impl; };` 所有成员藏在 .cpp 内 | ✅ 彻底解决、ABI 稳定；❌ 多一次间接访问 |
| **纯指针接口** | 只暴露 `void SubmitInstanced(Material*, VertexArray*, uint32_t)` 等 C 风格接口 | ✅ 最安全；❌ 丢失 C++ 类型安全 |
| **静态链接** | `DMGE_BUILD_SHARED=OFF`，引擎编译为 .lib | ✅ 无边界问题；❌ 无法热替换 DLL |
| **统一编译选项** | 确保 exe 和 DLL 的 `_ITERATOR_DEBUG_LEVEL`、`/MD`/`/MT` 完全一致 | ✅ 可能有效；❌ 脆弱，依赖编译器实现 |

---

## 5. 本项目的修复

```cpp
// 之前：DMGE_API 类的 vector 成员（跨边界不安全）
// RenderQueue.h
class DMGE_API RenderQueue {
    std::vector<Renderable>         m_Queue;
    std::vector<InstancedRenderable> m_InstancedQueue;  // ← C4251 警告
};

// 之后：DLL 内部的静态数组（不暴露给 exe）
// RenderQueue.cpp 内部：
static constexpr uint32_t kMaxInstancedBatches = 32;
static InstancedRenderable s_InstancedBatches[kMaxInstancedBatches];
static uint32_t s_InstancedBatchCount = 0;

// RenderQueue.h 中移除 m_InstancedQueue 成员：
class DMGE_API RenderQueue {
    std::vector<Renderable> m_Queue;  // 只剩这个
    // m_InstancedQueue 已移除
};
```

这本质上是 **Pimpl 思想的简化版**：把不安全的成员从导出类中移走，放到 DLL 的 .cpp 内部，exe 完全看不到它。

---

## 6. 经验总结

1. **C4251 警告不能忽视**：它提示了 STL 容器作为 DLL 导出类成员的风险。即使看似"不出问题"，也可能在特定使用模式下崩溃。
2. **Debug 模式更容易触发**：`_ITERATOR_DEBUG_LEVEL=2` 增加了容器的内部状态，使布局不一致问题更容易暴露。
3. **崩溃可能在第二帧才出现**：第一帧的分配（空 vector -> 分配）可能成功，但后续帧在已有缓冲区上操作时才暴露损坏。
4. **诊断手段**：逐步 `fprintf(stderr, ...)` + `fflush(stderr)` 是定位崩溃的可靠方法，特别是当调试器因 DLL 符号问题无法正确断点时。
5. **DLL 设计原则**：导出类的成员应尽量使用 POD 类型或固定大小数组。STL 容器应藏在 .cpp 内部（Pimpl 或 file-scope static）。
