# Getting Started — 从克隆到跑起来

> 目标：一个新读者按本文操作，能在自己的机器上构建出引擎、跑起 demo 游戏和编辑器、并通过全部单元测试。
> 本文所有路径、target 名、CMake 选项均对照代码核验过（2026-10，分支 `docs/guide-foundation`）。
> 构建系统的深层原理与排错实录见工程档案 `documents/PRECOMPILED_HEADER.md` 与 `kb/KB-01`。

## 1. 克隆

```bash
git clone <仓库地址> D:/CPPPractices/DMGameEngine
cd D:/CPPPractices/DMGameEngine
```

> ⚠️ 注意：当前 demo 代码里 shader 路径是**硬编码绝对路径**（`game/src/main.cpp` 中 `kShaderPath = "D:/CPPPractices/DMGameEngine/engine/shaders/BlinnPhongInstanced.glsl"`）。如果仓库不在 `D:/CPPPractices/DMGameEngine`，DMGameDemo 启动时会报 shader 加载失败——把仓库放在这个路径，或改这一行。

## 2. 环境要求

| 依赖 | 要求 | 说明 |
|---|---|---|
| 操作系统 | Windows 10+ | 平台层目前只有 Windows + GLFW（`engine/src/DMGameEngine/Platform/Windows/`） |
| 编译器 | MSVC（Visual Studio 工具链），C++23 | 本机实际环境：**VS 18 Community** 提供 MSVC 与环境变量 |
| CMake | ≥ 3.20 | 根 `CMakeLists.txt` 声明 |
| IDE | **CLion（推荐）** | 用 CLion 内置 Ninja 工具链构建最省心 |
| 可选 | Vulkan SDK | 仅当开 `-DDMGE_VULKAN_BACKEND=ON` 时需要（VMA + shaderc） |

> ⚠️ **重要经验**（来自 kb/KB-01）：裸终端里直接跑 cmake 常因缺 VS 环境变量报 `LNK1104`——这不是代码错误。首选在 CLion 内用内置 Ninja 工具链构建；如果一定要在裸终端跑，先在 "x64 Native Tools Command Prompt for VS" 里执行。

## 3. 构建

### 方式 A：CLion（推荐）

1. CLion → Open → 选择仓库**根目录**的 `CMakeLists.txt`（workspace 聚合 engine + game + editor 三个 target）。
2. 选 CLion 内置 Ninja 工具链 + Debug profile，等 CMake 配置完成。
3. Build All。

### 方式 B：命令行（Ninja）

```bash
# 配置（Debug + 单元测试）
cmake -S . -B cmake-build-debug -G Ninja -DDMGE_BUILD_TESTS=ON

# 构建
cmake --build cmake-build-debug
```

### 常用 CMake 选项（均已核验自 `engine/CMakeLists.txt`）

| 选项 | 默认 | 作用 |
|---|---|---|
| `DMGE_BUILD_TESTS` | `ON` | 构建 `dmge_tests`（GoogleTest 走 FetchContent，**首次配置需要联网**） |
| `DMGE_BUILD_SHARED` | `ON` | 引擎编为 DLL（`DMGameEngine.dll`）；`OFF` 则编静态库 |
| `DMGE_USE_PCH` | `ON` | 预编译头 `dmge_pch.h`；排错时可 `OFF` 复试 |
| `DMGE_VULKAN_BACKEND` | `OFF` | 编入 Vulkan 后端（需本机 Vulkan SDK） |

### 构建产物

| 产物 | 来源 |
|---|---|
| `DMGameEngine.dll`（或 .lib） | `engine/`（`DMGE_BUILD_SHARED=ON` 时为 SHARED，见 engine/CMakeLists.txt:207-210） |
| `DMGameDemo.exe` | `game/`（target 名 `DMGameDemo`） |
| `DMGameEditor.exe` | `editor/`（target 名 `DMGameEditor`） |
| `dmge_tests.exe` | `engine/tests/`（构建后自动把引擎 DLL 拷到测试 exe 旁，无需手动搬） |

## 4. 跑 demo 游戏（DMGameDemo）

```bash
./cmake-build-debug/DMGameDemo.exe
```

这是一个 ECS + 光照演示场景（`game/src/main.cpp` 的 `LitCubeScene`）：

- **1500 个翻滚的立方体**，按 (Material, Mesh) 分组走**实例化渲染**（`DrawIndexedInstanced`）
- **1 个方向光（太阳）+ 4 个彩色点光源**，Blinn-Phong 前向着色（shader：`engine/shaders/BlinnPhongInstanced.glsl`）
- 窗口 1280×720，VSync 开启

操作（核验自 `EditorCameraController.h` 与 `game/src/main.cpp`）：

| 按键 | 动作 |
|---|---|
| `W`/`A`/`S`/`D` | 相机前后左右移动 |
| `E`/`Q` | 相机上升/下降 |
| 鼠标左键拖动 | 环绕（orbit） |
| 鼠标右键/中键拖动 | 平移（pan） |
| 滚轮 | 缩放（改变相机距离） |
| `F1` | 开关 Profiler 覆盖层（FPS + 帧时间图 + 逐 scope 耗时表） |
| `` ` ``（GraveAccent） | 开关开发者控制台 |
| `Esc` | 退出 |

## 5. 跑编辑器（DMGameEditor）

```bash
./cmake-build-debug/DMGameEditor.exe
```

Unity 风格 ImGui 布局（`editor/src/EditorLayer.cpp`）：

- **Viewport**：场景渲染进一张**离屏 FrameBuffer**（render-to-texture），ImGui 里显示其颜色附件（`EditorScene.cpp`：`Renderer::BeginScene(camera, m_FB)`）
- **Hierarchy / Inspector / Systems / Asset Browser / Log** 面板：层级树、组件编辑、System 列表、资产加载、日志
- 快捷键：`Ctrl+N` 新建场景、`Ctrl+S` 保存场景、`Ctrl+O` 加载场景（`.scene` JSON 格式）、`Ctrl+Shift+A` 创建空实体
- Gizmo：`1` 平移 / `2` 旋转 / `3` 缩放 / `4` 关闭（ImGuizmo，`EditorLayer.cpp`）

## 6. 跑测试

```bash
ctest --test-dir cmake-build-debug --output-on-failure
```

- 测试 target 为 `dmge_tests`（`engine/tests/`，GoogleTest 经 FetchContent 拉取）。
- 当前共 **32 个 TEST 用例**，分布在 5 个文件：`test_core.cpp`（5）、`test_ecs.cpp`（11）、`test_scene.cpp`（8）、`test_asset.cpp`（6）、`test_assimp_import.cpp`（2）——数字为对源码 `grep "^TEST"` 核验所得。
- 测试 exe 经 `DMGE_TEST_HEADLESS` 编译定义运行（无窗口），引擎 DLL 由 POST_BUILD 自动拷贝，直接 `ctest` 即可。
- 提交前的门槛：**全量构建零 error + ctest 全绿 + 无新增 /W4 警告**（AGENTS.md §4）。

## 7. 目录结构导览

```
DMGameEngine/
├── CMakeLists.txt              workspace 根：聚合 engine + game + editor 三个 target
├── AGENTS.md                   多 Agent 协作章程（人类协作者同样受约束）
├── TASKS.md                    任务认领看板
├── engine/
│   ├── CMakeLists.txt          引擎 target；源文件显式列出（禁 GLOB，见 §8）
│   ├── src/DMGameEngine/       引擎源码（见 ArchitectureOverview.md 的模块图）
│   │   ├── Core/               Application / Layer / Window / Input / 事件 / 日志
│   │   ├── Renderer/           渲染抽象（Renderer、Material、RenderQueue、FrameBuffer…）
│   │   ├── Platform/           OpenGL/ 与 Vulkan/ 双后端 + Windows/ 窗口与输入实现
│   │   ├── Scene/              ECS：Scene、Entity、Components/、Systems/、序列化
│   │   ├── Asset/              AssetManager（UUID 三表）、Mesh、assimp 导入
│   │   ├── Debug/              Profiler、Console（ImGui 覆盖层）
│   │   ├── ImGui/              ImGuiLayer（引擎侧导出，编辑器侧复用同一份）
│   │   └── dmge_pch.h          预编译头（只允许 stdlib + glm + spdlog）
│   ├── shaders/                GLSL shader（BlinnPhong.glsl、BlinnPhongInstanced.glsl）
│   ├── tests/                  GoogleTest 单测（dmge_tests）
│   └── dependencies/           entt、assimp 等第三方
├── editor/                     独立编辑器 exe（EditorLayer + 面板，dependencies/ 内有 ImGui）
├── game/                       演示游戏 exe——引擎的"消费者视角"检验
├── documents/                  工程档案（根）+ 人类向 guide/（本目录）
├── kb/                         Agent 知识库（踩坑沉淀，开工前先查）
└── playbooks/                  Agent 可复用任务手册
```

## 8. 新人最容易踩的三个坑（速查）

1. **`LNK1104` 打不开 .lib/.dll** → 构建环境问题（裸终端缺 VS 环境），不是代码问题。换 CLion Ninja 或 VS 命令行。
2. **新增源文件"不存在"** → 忘了手动注册进对应 CMakeLists 的 `set(DMGE_SOURCES ...)`。本项目**禁 GLOB**，新文件必须显式列出（kb/KB-01、AGENTS.md 红线 R2）。
3. **改了头但行为没变** → PCH 缓存。删 `cmake-build-debug` 重建再判断（kb/KB-01）。

更多症状见 `kb/KB-01`；完整陷阱清单见 `kb/KB-07-已知问题与陷阱清单.md`。

## 下一步

读 [ArchitectureOverview.md](ArchitectureOverview.md)，了解引擎的模块划分与一帧的生命周期。
