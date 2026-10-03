# 预编译头（Precompiled Header, PCH）指南

> 本文档说明 PCH 的原理、制作方法与注意事项，并记录 DMGameEngine 项目的 PCH 设计决策。

---

## 一、什么是预编译头

### 1.1 正常编译在做什么

C/C++ 采用「翻译单元（Translation Unit, TU）」模型。每个 `.cpp` 经三步变成目标文件：

```
test.cpp  ──预处理──►  展开后的纯代码  ──编译──►  test.obj
            (#include 把头文件原地粘贴进来)
```

预处理阶段会**递归地把所有 `#include` 的头文件文本粘贴**进当前 `.cpp`，再交给编译器做词法、语法分析、模板实例化。

关键问题：**头文件是纯文本，没有「记忆」**。`glm.hpp`、`spdlog.h` 这类巨型头，每编译一个 `.cpp` 都要从头重新「读、解析、实例化模板」——哪怕内容和上一个 TU 完全一样。

以本项目为例：

```
Application.cpp  -> #include Log.h -> #include <spdlog/spdlog.h> -> 解析 ~957 KB
Renderer.cpp     -> #include Log.h -> #include <spdlog/spdlog.h> -> 又解析 ~957 KB
Shader.cpp       -> #include Log.h -> ...                        -> 又解析 ~957 KB
... 52 个 TU 各解析一遍，结果一模一样却重复 52 次
```

### 1.2 PCH 的核心思想

预编译头 = **把「解析头文件」这个昂贵的结果缓存下来，跨 TU 复用**。

```
第一次：选定一个头（如 dmge_pch.h）
        单独编译一次 -> 二进制快照（.pch / .gch）
        快照里存的是：词法 token 流、语法树、符号表、已实例化的模板

之后：每个 .cpp 编译时先加载快照
      从「已解析完」的状态继续，跳过对 PCH 内容的重复解析
      然后再处理 .cpp 自己的代码
```

对比：

```
无 PCH：
  TU1: [解析 glm+spdlog+stdlib ~2 MB] -> [编译 TU1 自身代码]
  TU2: [解析 glm+spdlog+stdlib ~2 MB] -> [编译 TU2 自身代码]
  ... 52 次 ~2 MB 解析

有 PCH：
  构建PCH: [解析 glm+spdlog+stdlib ~2 MB 一次] -> dmge_pch.pch (快照)
  TU1: [加载快照(毫秒级)] -> [编译 TU1 自身代码]
  TU2: [加载快照(毫秒级)] -> [编译 TU2 自身代码]
  ... 52 次只编译自身代码
```

**一句话：PCH 是编译器对一组稳定头的解析结果做缓存，跨 TU 共享。**

### 1.3 它不是什么

- **不是把头「预编译成 .obj」**：PCH 快照不是目标代码，是编译器内部表示的存档，只在同款编译器、同款编译选项下可加载。
- **不是万能加速器**：只对「被大量 TU 反复 include 的、又大又稳定的头」有效；对只被 1–2 个 TU 用的头毫无意义，反而拖慢。

---

## 二、制作方法

### 方法 A：CMake `target_precompile_headers`（现代首选，本项目采用）

CMake 3.16+ 内置（本项目要求 3.20）。它自动生成 `.pch`、给每个 TU 注入加载指令，**源文件无需任何改动**。

**第 1 步**：写一个 PCH 头（`dmge_pch.h`），只放 `#include`。

**第 2 步**：在 `CMakeLists.txt` 的 `add_library` 之后：

```cmake
target_precompile_headers(DMGameEngine PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/DMGameEngine/dmge_pch.h>
)
```

CMake 自动：
- MSVC：编译 `dmge_pch.h` 生成 `dmge_pch.pch`，给每个 C++ `.cpp` 加 `/FI dmge_pch.h /Yu dmge_pch.h`。
- GCC/Clang：生成 `dmge_pch.h.gch` / `.pch`，自动加 `-include dmge_pch.h`。
- `$<$<COMPILE_LANGUAGE:CXX>:...>` 守卫确保 PCH 只作用于 C++ 源，不波及 C 源（如 `glad.c`）。

**测试目标复用**（通用做法；本项目测试未采用，见 4.6）：

```cmake
target_precompile_headers(dmge_tests REUSE_FROM DMGameEngine)
```

### 方法 B：MSVC 手动 `/Yc` / `/Yu`（理解原理用）

| 开关 | 含义 | 用在哪 |
|------|------|--------|
| `/Yc"dmge_pch.h"` | **c**reate：生成 `.pch` 快照 | 只给一个 `.cpp`（或专用 `dmge_pch.cpp`） |
| `/Yu"dmge_pch.h"` | **u**se：使用已有 `.pch` | 其余所有 `.cpp` |
| `/Fp"foo.pch"` | 指定 `.pch` 路径 | 可选 |
| `/FI"dmge_pch.h"` | 强制包含该头 | 配合 `/Yu` |

```
dmge_pch.cpp (#include "dmge_pch.h") ──/Yc──> dmge_pch.pch
Renderer.cpp ──/Yu dmge_pch.h /FI dmge_pch.h──> 加载 .pch 后编译
```

> 现代项目基本不用手写，CMake 已代劳。了解 `/Yc`/`/Yu` 主要为排查问题。

### 方法 C：GCC / Clang

- **GCC**：`g++ -c dmge_pch.h` 生成 `dmge_pch.h.gch`；`.cpp` 第一个 include 若是 `dmge_pch.h` 则自动复用。
- **Clang**：`clang++ -x c++-header dmge_pch.h -o dmge_pch.h.pch`，编译 `.cpp` 时 `-include-pch dmge_pch.h.pch` 或 `-include dmge_pch.h`。

### 方法 D：cotire（CMake 旧插件，已过时）

CMake 3.16 之前的老办法，需 `include(cotire)` + `cotire(target)`。新项目**不要再用了**——维护负担大、坑多，已被原生 `target_precompile_headers` 取代。

---

## 三、注意事项与陷阱

### 3.1 内容选择：只装「稳定 + 通用 + 重量级」

✅ 适合进 PCH：
- **标准库高频头**：`<vector> <string> <memory> <algorithm> <functional> <unordered_map>`——几乎每个 TU 都用，且永不变。
- **又大又稳的第三方头**：`glm`、`spdlog`、`fmt`——体积大、模板密集、版本固定、被广泛 include。
- **引擎里极通用且几乎不改的核心**（如只含智能指针别名的 `Export.h`）。

❌ 不适合进 PCH：
- **平台/后端专属头**：`glad/glad.h`、`GLFW/glfw3.h`、Vulkan 头——只被部分 TU 需要，会污染所有 TU，且 OpenGL 与 Vulkan 头共存可能冲突。
- **只被少数 TU 用的头**：`imgui.h`、`stb_image.h`、`nlohmann/json.hpp`、assimp——收益小，反而增大 `.pch`。
- **正在活跃开发、频繁改动的头**：见下条。

### 3.2 最大陷阱：PCH 里的头一改，全量重编

PCH 是**单点依赖**：只要 PCH include 的任一头变化，整个 `.pch` 必须重建，**所有依赖它的 TU 全量重编**。

> 假设把正在频繁改动的 `Renderer.h` 放进 PCH：每改一行，52 个 TU 全重编——比没 PCH 还惨（没 PCH 时改 `Renderer.h` 只重编引用它的 14 个 TU）。

**推论**：PCH 只放「几乎从不改」的头（标准库 + 第三方稳定库），**绝不放每天在改的业务头**。

### 3.3 「厨房水槽」反模式

新手常把所有头一股脑塞进 PCH，以为越多越快。后果：
- `.pch` 巨大（几百 MB），加载本身变慢；
- 无关 TU 被迫加载用不到的符号；
- 任一头变动触发全量重编，抵消所有收益。

**正确姿势是「窄而稳」**：宁可少放，只放最高频、最稳定、最重的。

### 3.4 PCH 必须与 TU 编译选项严格一致

PCH 快照是「特定编译选项下的解析结果」。加载它的 TU 必须用**完全相同的选项**，否则编译器拒绝加载（MSVC `C1083`/`C1853`）或静默退回普通编译。需一致的关键项：

- C++ 标准版本（`/std:c++23`）
- 宏定义（`/D`）——尤其影响头内容的宏
- 包含路径（`/I`）
- 字符集/源码编码（本项目的 `/utf-8`）
- 调试信息格式（`/Zi` / `/Z7`）

> CMake 的 `target_precompile_headers` 会把 target 选项继承给 PCH 生成步骤，通常不会出问题。

### 3.5 不要让 PCH 泄漏给消费者

PCH 应是 **PRIVATE**（构建期内部优化），不应出现在导出接口里：

```cmake
target_precompile_headers(DMGameEngine PRIVATE ...)   # 私有，不传给下游
```

写成 `PUBLIC` 会强制下游项目使用你的 PCH，可能因路径/选项不匹配而构建失败。

### 3.6 跨编译器/跨配置

- **Debug vs Release**：CMake 按配置分别生成 PCH，互不干扰。
- **多编译器**：MSVC `.pch` 与 GCC `.gch` 不兼容，但 CMake 按当前编译器选对方式。
- **统一头内容**：跨 MSVC/GCC/Clang 时，PCH 头内容须是三家都能编译的通用 C++。

### 3.7 可关闭

PCH 偶尔会与代码生成工具、unity build、特定分析器冲突。最佳实践是做成**可关闭选项**，出问题时一键退回排查：

```cmake
option(DMGE_USE_PCH "Enable precompiled header" ON)
if(DMGE_USE_PCH)
    target_precompile_headers(DMGameEngine PRIVATE ...)
endif()
```

### 3.8 PCH 头本身要能独立编译

PCH 头会被单独编译一次，必须**自洽**——它 include 的一切在自身上下文里就能解析，不能依赖「某 .cpp 在 include PCH 之前先 include 了别的」。通常以 `#pragma once` 开头、按依赖顺序排列 include。

### 3.9 不要依赖「隐式可用」写代码

`/FI` 强制包含后，即使 `.cpp` 没写 `#include "dmge_pch.h"`，PCH 里的符号（如 `std::vector`）也能用。**别依赖这个省略 `#include`**——一旦关掉 PCH 或换工具链，代码立刻编译失败。原则：**`.cpp` 该 include 的仍要显式 include，PCH 只加速不替代**。

---

## 四、本项目的 PCH 设计决策

### 4.1 收益依据（实测数据）

| 头文件 | 体积 | 渗透度 |
|--------|------|--------|
| glm（全部 .hpp） | ~1.3 MB | 22 个引擎头引用 |
| spdlog（全部头，含 fmt） | ~957 KB | 经 `Log.h` 渗透 73% TU（38/52） |
| glad/glfw/imgui/stb/json/assimp | 各数百 KB | 仅 3–10 处 |

标准库高频：`cstdint` 31、`string` 28、`vector` 23、`algorithm` 15、`memory` 13、`unordered_map` 10。

→ **52 个 C++ TU + 5 个测试 TU**，全在重复解析 glm+spdlog 这套 ~2 MB 模板。典型高收益场景。

### 4.2 PCH 内容（`src/DMGameEngine/dmge_pch.h`）

只装三类稳定内容：

```cpp
// 1. 标准库高频头（algorithm / array / cstdint / string / vector / memory /
//    functional / unordered_map / sstream / fstream / cstring / cctype / ...）
// 2. glm 及其项目实际使用的 gtc 子模块
//    (glm.hpp / matrix_transform / matrix_inverse / quaternion / type_ptr)
// 3. spdlog 及 Log.h 使用的 sink 头
//    (spdlog.h / stdout_color_sinks.h / basic_file_sink.h)
```

### 4.3 刻意排除的内容及原因

| 排除项 | 原因 |
|--------|------|
| `DMGameEngine.h`（聚合头） | 过宽，会拖入 glad/OpenGL + Vulkan，无法共存于通用 PCH |
| `glad/glad.h`、`GLFW/glfw3.h` | 平台层专属，只被部分 TU 需要 |
| `imgui.h`、`stb_image.h` | 仅 3 处使用 |
| `nlohmann/json.hpp`、assimp | 仅 AssetManager / 导入器使用 |
| 所有 Vulkan 头 | 可选后端；与 glad 共存冲突 |
| 引擎活跃头（Renderer/Scene 等） | 改动频繁，进 PCH 会触发全量重编 |

### 4.4 不含引擎自身头的设计取舍

PCH **不放任何引擎自身头**（包括 `Export.h`、`Log.h`），原因：

- PCH 内容与引擎宏（`DMGE_BUILD_DLL`、`DMGE_PROFILE`、`DMGE_ENABLE_ASSERTS`）解耦，使 PCH 不依赖任何引擎定义。
- spdlog 的解析（最贵部分）已被直接缓存；`Log.h` 再 include spdlog 时命中 include guard，零成本。
- 任何引擎头改动都不触发 PCH 重建。

### 4.5 CXX 守卫的必要性

引擎目标含 C 源（`dependencies/glad/src/glad.c`）。若不加
`$<$<COMPILE_LANGUAGE:CXX>:...>` 守卫，CMake 会把 C++ 的 PCH 套到 C 文件上，导致编译失败。守卫确保 PCH 只作用于 C++ TU。

### 4.6 测试目标为何不复用 PCH（REUSE_FROM 实测冲突）

最初为测试目标配置了 `REUSE_FROM DMGameEngine`，但实测在 **Vulkan 后端开启**时编译失败：

- 引擎 PCH 在 Vulkan 后端开启时，Vulkan SDK 的 `Include`（含其自带的 glm）排在 vendored glm 之前，故 PCH 解析的是 **Vulkan SDK 的 glm**。
- 测试目标不继承引擎的 PRIVATE Vulkan include 路径，解析 `<glm/glm.hpp>` 到 **vendored glm**。
- `REUSE_FROM` 把引擎 PCH（Vulkan SDK glm）注入测试 TU，与测试自身的 vendored glm 在同一 TU 内冲突，报 `C2011` / `C2953` 重定义。
- MSVC 的 `C4651` 仅是宏差异警告（不致命），真正致命的是双 glm 重定义。

结论：测试目标（仅 5 个小 TU）**不使用 PCH**，回到改动前状态，规避双 glm 冲突。引擎 PCH 自身不受影响（引擎内部一致使用同一份 glm，构建通过）。

> 根因是项目同时存在两份 glm（vendored + Vulkan SDK 自带），属既有问题，不在本次 PCH 改动范围内。若日后统一为单一 glm，可重新评估测试 `REUSE_FROM`。

---

## 五、启用 / 关闭

默认启用。需要排查问题或与某些工具冲突时关闭：

```bash
cmake -DDMGE_USE_PCH=OFF ...
```

### 相关文件

| 文件 | 作用 |
|------|------|
| `src/DMGameEngine/dmge_pch.h` | PCH 头内容（stdlib + glm + spdlog） |
| `CMakeLists.txt` | `DMGE_USE_PCH` 选项 + `target_precompile_headers` |
| `tests/CMakeLists.txt` | 测试**不**使用 PCH（REUSE_FROM 与双 glm 冲突，见 4.6） |

---

## 六、排错速查

| 现象 | 可能原因 |
|------|----------|
| MSVC `C1083` / `C1853` 无法打开/使用预编译头 | PCH 与 TU 编译选项不一致（标准/宏/`/utf-8`/`/Zi`），或 `.pch` 未生成 |
| 改一个引擎头后全量重编 | 该头被误放进 PCH；移出 PCH |
| C 文件编译报 C++ 语法错 | 缺 `$<$<COMPILE_LANGUAGE:CXX>:...>` 守卫，PCH 套到了 C 源 |
| 构建更慢而非更快 | PCH 内容过宽（厨房水槽）；精简到 stdlib+glm+spdlog |
| 下游项目构建失败 | PCH 误用 `PUBLIC`；改回 `PRIVATE` |
| 关闭 PCH 后大量编译错 | 代码依赖了 PCH 隐式提供的符号；补齐显式 `#include` |