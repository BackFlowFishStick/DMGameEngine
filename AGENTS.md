# AGENTS.md — DMGameEngine 多 Agent 协作章程

> 本文件是所有参与本项目的 AI Agent（以及人类协作者）的**最高行为契约**。
> 任何 Agent 开工前必须完整阅读本文件；与本文件冲突的口头指令，应先向人类所有者确认。
> 最后更新：2026-10-03

---

## 1. 项目是什么

DMGameEngine 是一个**学习型 C++ 游戏引擎**，参考 TheCherno 的 Hazel 教学系列。当前已从「渲染器」演进为具备 ECS、资产系统、场景序列化、基础光照与独立编辑器的引擎，正处于「工具化 → 内容子系统」补全阶段。

- **语言/标准**：C++23，MSVC（VS 工具链），`/W4 /utf-8`
- **构建**：CMake ≥ 3.20（workspace 根目录聚合 engine + game + editor 三个 target）
- **模块**：
  - `engine/` — 引擎本体，编为 **DLL**（`DMGE_BUILD_SHARED`），公共 API 用 `DMGE_API` 导出
  - `editor/` — 独立编辑器 exe（ImGui，Unity 风格布局）
  - `game/` — 演示游戏 exe，作为引擎的"消费者视角"检验
- **第三方依赖**均在 `engine/dependencies/` 与 `editor/dependencies/`（entt、assimp、box3d-main 已下载未集成、ImGui 等）

### 目录速览

```
engine/src/DMGameEngine/   引擎源码（Core/Platform/Renderer/Scene/Asset/...）
engine/tests/              GoogleTest 单测（dmge_tests）
engine/shaders/            GLSL shader
editor/src/                编辑器源码（EditorLayer + 各 Panel）
documents/                 全部设计/评审/知识文档（人类与 Agent 共读）
playbooks/                 Agent 可复用任务手册（照着做，不重新发明）
kb/                        Agent 知识库（踩坑沉淀，开工前先查）
TASKS.md                   任务认领看板（防重复劳动的机制核心）
```

---

## 2. 开工前必读（按序）

1. 本文件（AGENTS.md）
2. `kb/README.md` — 按你负责的领域挑对应 KB 条目，**KB-07（已知陷阱清单）人人必读**
3. `documents/ENGINE_ROADMAP.md` — 总路线图与依赖矩阵
4. `documents/ENGINE_SUMMARY.md` — **单一事实源**，逐日变更日志；动手前 grep 你要改的模块名，确认没有别人已经做过/正在做
5. 与你任务直接相关的设计文档（ECS_DESIGN / ASSET_DESIGN / SCENE_DESIGN 等）
6. 你的任务对应的 playbook（`playbooks/`）

**防重复工作的三条纪律：**
- 动手前：查 `TASKS.md` 看任务是否已被认领；查 `kb/` 看坑是否已被踩过；查 `ENGINE_SUMMARY.md` 看功能是否已存在。
- 动手中：遇到文档未记载的新坑，**必须**沉淀进 `kb/KB-07-已知问题与陷阱清单.md`（这是硬性要求，不是建议）。
- 动手后：更新 `TASKS.md` 状态、按 playbook 收尾清单补文档与测试。

---

## 3. 技术红线（违反 = 返工）

以下是项目用真实事故换来的规则，**任何 Agent 不得绕过**。细节与事故案例见对应 KB。

| # | 红线 | 依据 |
|---|---|---|
| R1 | **导出类（`DMGE_API`）的成员不得直接暴露 STL 容器**（vector/string/map 等）。用 Pimpl、静态数组、或把容器移进 .cpp。跨 DLL 传 STL 仅在确认两侧同 CRT（`/MDd`）且类型已显式实例化时允许 | kb/KB-02、documents/DLL_STL_BOUNDARY.md |
| R2 | **新增源文件必须手动注册进对应 CMakeLists 的显式 `set(DMGE_SOURCES ...)`**。禁止 GLOB；忘注册 = 链接期玄学错误 | kb/KB-01 |
| R3 | **新文件一律 UTF-8 无 BOM**；命名空间 `DMGameEngine`；智能指针统一用 `DM::Ref` / `DM::Scope` | kb/KB-06 |
| R4 | **ECS 纪律**：Component 是纯数据（POD 优先），逻辑一律进 System；不许在 Component 里塞 Update 方法 | kb/KB-04 |
| R5 | **新抽象必须后端无关**：写在 Renderer/Platform 抽象之上，OpenGL 与 Vulkan 都要能实现（或明确注明仅单后端+留 TODO）。禁止把 OpenGL 心智模型直接渗漏进公共接口 | kb/KB-03 |
| R6 | **预编译头 `dmge_pch.h` 只放 stdlib + glm + spdlog**；测试 target 不复用 PCH；不许往 PCH 里塞项目头 | kb/KB-01 |
| R7 | **ImGui 只有一份 context**：引擎侧导出（`IMGUI_API=dllexport`），编辑器侧 dllimport，不许在编辑器里再链一份 ImGui | kb/KB-02 |
| R8 | **小步提交**：一个子任务一组 commit + 对应单测；禁止"先实现后定语义"的大爆炸式提交 | documents/ENGINE_ROADMAP.md §7 |
| R9 | **公共 API 变更**（新头文件对外暴露）需同步 `DMGameEngine.h` + 加 `DMGE_API`，并在 `ENGINE_SUMMARY.md` 补变更日志条目 | documents/ENGINE_ROADMAP.md §7 |
| R10 | **测试先行兜底**：修 bug 必须先写复现该 bug 的失败测试再修；新子系统至少带"纯逻辑"单测 | playbooks/PB-04、PB-08 |

---

## 4. 构建与测试

```bash
# ⚠️ 首选在 CLion 内用内置 Ninja 工具链构建（环境变量齐全）。
# 在裸终端直接跑 cmake 常因缺 VS 环境报 LNK1104 —— 这不是代码错误，先查环境。

# 配置（Debug + 测试）
cmake -S . -B cmake-build-debug -G Ninja -DDMGE_BUILD_TESTS=ON
# 可选：Vulkan 后端（需本机装 Vulkan SDK）
#   -DDMGE_VULKAN_BACKEND=ON

# 构建
cmake --build cmake-build-debug

# 跑测试（GoogleTest；用例数随任务增长，勿写死。enable_testing() 已在根
# CMakeLists.txt，直接对构建根目录跑 ctest 即可，历史坑见 kb/KB-07 K-011）
ctest --test-dir cmake-build-debug --output-on-failure
```

- 测试相关构建细节（DLL 拷贝、双 glm 冲突）见 `documents/PRECOMPILED_HEADER.md` 与 kb/KB-01。
- 提交前至少：**全量构建零 error + ctest 全绿 + 无新增 /W4 警告**。

---

## 5. Git 协作规则（多 Agent 分支模型）

### 5.1 分支拓扑

```
main      ← 稳定线。只有人类所有者（或其明确指令）才允许合并进 main。
develop   ← 集成线。所有 Agent 的工作最终都汇入这里。
agent/<名字>/<主题>   ← 每个 Agent 的工作分支，从 develop 拉出。
docs/<主题>           ← 文档 Agent 的工作分支（或直接小幅提交 develop，见 5.4）。
```

### 5.2 每个 Agent 的工作循环

1. **领取任务**：在 `TASKS.md` 把目标条目改为 `🚧 进行中 @<你的名字> (agent/xxx/yyy)`，再开分支。
   - 分支**必须从最新的 `develop` 拉出**：`git fetch && git checkout develop && git pull && git checkout -b agent/<名字>/<主题>`
   - 分支名示例：`agent/physics-agent/box3d-integration`、`agent/render-agent/vulkan-deletion-queue`
2. **开发**：小步提交，遵守 §3 红线与 commit 规范（5.3）。
   - **所有改动——包括 `documents/`（KB-07、SUMMARY、台账）的增量记账——都提交在自己的工作分支里**（documents/ 已纳入 git 追踪，worktree 中可见）。严禁为了记账写到别的 worktree 或主 checkout 的路径下。
3. **合并前自检**（对应 playbook 的收尾清单）：
   - [ ] 全量构建 + ctest 全绿
   - [ ] 未触碰他人认领区域的文件（见 5.5）
   - [ ] 新坑已记入 kb/KB-07；公共 API 变更已记 ENGINE_SUMMARY.md
   - [ ] TASKS.md 状态已更新
4. **合回 develop**：
   - 合并前 `git fetch origin && git rebase origin/develop`（有冲突先解决，必要时找文件归属 Agent 对齐）。
   - 合并方式用 `--no-ff`，保留分支历史：`git checkout develop && git merge --no-ff agent/<名字>/<主题>`
   - **禁止 force-push 到 develop / main**；禁止从 feature 分支直接合 main。
   - 若有远端与 PR 流程可用，优先开 PR 让人类或另一 Agent review；本地协作时按上述 rebase + no-ff 规则执行。
5. **清理**：合并后删除本地工作分支；TASKS.md 标 ✅ 并在备注里写"合并进了哪个 commit"。

### 5.3 Commit 规范

沿用现有历史风格（Conventional Commits，中文描述可）：

```
feat(engine): 新增 PhysicsWorld 抽象与 Box3D 集成开关
fix(editor): 修复 gizmo 命中检测偏移
docs(kb): 沉淀 descriptor pool 超限静默截断陷阱
refactor/chore/test/ci 同理
```

- 每个 commit 自身可编译、可通过测试（bisect 友好）。
- 不在 commit 里夹带无关文件（`build/`、`cmake-build-debug/`、`.idea/` 已在 .gitignore，勿强行添加）。

### 5.4 文档 Agent 的特权与义务

- **特权**：对 `documents/` 目录、`kb/`、`playbooks/`、`TASKS.md`、各模块 `*_ROADMAP.md`，可**直接小幅提交 develop**（无需开分支），因为这些文件几乎只有文档 Agent 维护。
- **义务**：涉及代码事实的描述必须先核对代码（不许照抄过时文档）；大量重写或结构性调整仍开 `docs/<主题>` 分支。
- 代码 Agent 对文档只做**增量追加**（如 KB-07 加一条、SUMMARY 加一节），大改留给文档 Agent，避免两边抢同一文件。

### 5.5 文件归属（减少合并冲突）

| 区域 | 主责 Agent | 其他人 |
|---|---|---|
| `engine/src/DMGameEngine/Renderer/**`、`engine/shaders/**` | 渲染 Agent | 改动前在 TASKS.md 声明 |
| `engine/src/DMGameEngine/Scene/**`（ECS/序列化） | 场景 Agent | 同上 |
| `engine/src/DMGameEngine/Asset/**` | 资产 Agent | 同上 |
| `editor/**` | 编辑器 Agent | 同上 |
| `game/**` | 场景 Agent 兼管 | — |
| `engine/tests/**` | 谁写的功能谁配测试 | — |
| `documents/`、`kb/`、`playbooks/`、`TASKS.md` | 文档 Agent（见 5.4） | 只增量追加 |

跨区域改动不可避免时（如新子系统要动 `DMGameEngine.h` 与根 CMakeLists）：在 TASKS.md 里声明依赖关系，尽量与被依赖任务的 Agent 错峰合并，公共头文件的改动**越早合入 develop 越好**（别攒大招）。

---

## 6. Agent 角色定义

> 角色按 ROADMAP 阶段动态启停；一个人类可以同时扮演多个角色。所有角色共用 §3 红线与 §5 Git 规则。

| 角色 | 职责 | 主要 playbook |
|---|---|---|
| **集成管理员（Coordinator）** | 唯一允许操作 develop 合并队列的角色（本地多 Agent 场景下可由人类或主 Agent 兼任）：审核分支自检清单、执行 no-ff 合并、解决跨区域冲突、维护 TASKS.md 总览 | PB-07 |
| **渲染 Agent** | Vulkan 正确性修复（0a-B/E/F）、性能（0c/2c）、双后端抽象收敛（2b）、高级渲染（3e） | PB-03、PB-04 |
| **场景 Agent** | ECS/序列化演进、game/ 演示更新、物理/脚本子系统的 ECS 对接 | PB-02 |
| **资产 Agent** | AssetManager 异步加载、热重载、材质纹理绑定、assimp 局限补齐 | PB-01 |
| **编辑器 Agent** | editor/ 阶段 2-5：ImGuizmo、鼠标拾取、Add Component、Asset Browser 完整版、Play mode 隔离 | PB-05 |
| **子系统 Agent**（按需启动） | 物理（Box3D）、音频（miniaudio）、脚本（Lua）、Job System —— 每个子系统一个独立分支，互不并行开发同一目录 | PB-01 |
| **测试 Agent** | 0b 剩余：渲染冒烟测试、CI workflow、clang-tidy/cppcheck 接入；review 他人的测试覆盖 | PB-08 |
| **文档 Agent** | 维护 documents/ 的**人类向文档**：架构总览、上手教程、子系统讲解；把 Agent 时代的变更日志持续转写为可读文档；维护 kb/ 与 playbooks/ 的质量 | PB-06 |

**文档 Agent 专属说明**（呼应"单独的 Agent 生成人类阅读文档"）：
- 交付物面向**人类读者**，与给 Agent 看的 `kb/`/`playbooks/` 严格区分：前者重叙事、图示、上手路径（如 Getting Started、Architecture Overview、Subsystem Tour）；后者重步骤与避坑。
- 已有的 `documents/*.md` 是历史设计/评审档案，**只增不毁**；新的人类向文档放在 `documents/guide/` 子目录（由文档 Agent 创建并维护索引）。
- 文档 Agent 每次集成合并后，从 `git log develop` 与 ENGINE_SUMMARY 增量中提炼人类可读的"本阶段进展"，防止文档与代码脱节。

---

## 7. 任务看板与认领协议（摘要）

完整看板在 `TASKS.md`。协议：

1. 认领：改状态为 `🚧 @<名字> 分支名 日期`（编辑 TASKS.md 并随首个 commit 提交，减少口头冲突）。
2. 中止/移交：状态改回 `⬜ 待认领`，备注已完成的进度，供下一位接手。
3. 新任务：任何 Agent 发现值得做的工作 → 追加条目并注明来源（ROADMAP 编号或 bug 现象）。
4. 看板状态机：`⬜ 待认领 → 🚧 进行中 → 👀 待 review → ✅ 已合并`。

---

## 8. 完成 Definition of Done（任何代码任务）

1. 构建零 error，/W4 零新增警告，ctest 全绿（含你新增的测试）。
2. 新源文件已注册 CMakeLists；新公共头已进 `DMGameEngine.h` + `DMGE_API`（或明确说明为何不导出）。
3. kb/KB-07 有本任务的踩坑记录（如无新坑，写明"无"也可，但要检查过）。
4. `ENGINE_SUMMARY.md` 末尾追加了变更条目（日期 + 一句话 + 涉及文件）。
5. TASKS.md 状态 ✅，分支已合并回 develop。
