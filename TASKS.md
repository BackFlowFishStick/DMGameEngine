# TASKS.md — 任务认领看板

> 认领协议见 AGENTS.md §7。状态机：`⬜ 待认领 → 🚧 进行中 → 👀 待 review → ✅ 已合并`（另有 `🔍 需要更多信息`）。
> 认领格式：把状态改为 `🚧 @<Agent名> <分支名> <日期>`；完成后 `✅ <合并commit>`。
> 条目来源标注：ROADMAP 编号（ENGINE_ROADMAP / EDITOR_ROADMAP）或 BUG/IDEA。

## 进行中 / 待 review

| 任务 | 状态 | 备注 |
|---|---|---|
| 编辑器阶段3/4 收尾：多 Scene 标签页 + 导出可运行工程 | 🚧 @editor-agent agent/editor-agent/multi-scene-export 2026-10-03 | worktree `.worktrees/editor-agent`；同属 editor/ 热点区故单 Agent 串行 |
| 0c Vulkan 性能 P0：descriptor 复用 + ImmediateSubmit 批量化 | 🚧 @render-agent agent/render-agent/vulkan-perf-p0 2026-10-03 | worktree `.worktrees/render-agent`；0a 依赖已满足 |
| K-011 正解：enable_testing() 挪根 CMakeLists + CI 修正 | 🚧 @test-agent agent/test-agent/enable-testing-root 2026-10-03 | worktree `.worktrees/test-agent`；只动根 CMakeLists/.github/KB-07 |
| guide P2：术语表 + 回访 guide 过时段落 | 🚧 @docs-agent docs/glossary 2026-10-03 | worktree `.worktrees/docs-agent` |

## 已合并 ✅

- ✅ 编辑器功能补完（阶段 3/4）@editor-agent（分支 tip `7b435e9`；Play 快照隔离走 SceneDuplicator 对象级深拷贝而非 JSON 快照——K-012：序列化会丢程序化网格；Asset 拖拽建 Mesh 实体；File 菜单 + editor_config.ini 最近文件；Prefab 走临时 Scene 方案引擎零改动；构建 0 error、ctest 37/37、/W4 零新增。GUI 行为待人工验证，步骤见该 agent 报告 / EDITOR_ROADMAP 标注）
- ✅ guide P1：渲染导览 + 阅读路线图 @docs-agent（分支 tip `08b990f`；RendererTour/LearningPath 两篇全事实核验；发现 4 处既有文档与代码不一致已登记待修，见 RendererTour §4 注记）
- ✅ 0a Vulkan 正确性修复 A/B/C/D/F @render-agent（分支 tip `1ae2aa6`；A/C/D/F 核验为 07-26 已修，本次加固 A `9219cc7` + 落地 B per-frame deletion queue `1ae2aa6` 含 headless 性质单测；PB-08 通过：Vulkan ON/OFF 双路径构建零 error、/W4 零新增、ctest 37/37；遗留：Validation 运行时场景待人工跑 editor/game；E 项留 2b）
- ✅ 0b CI 骨架 @test-agent（分支 tip `35fedb2`；交付 `.github/workflows/ci.yml` + `.github/CI.md` + 保守版 `.clang-tidy` 暂不强制门禁；本地 Ninja+vcvars 全量构建 0 error、ctest 32/32 绿；Vulkan job 留注释骨架待固定 SDK 版本；ctest 目录坑见 KB-07/K-011）

## 待认领（按优先级）

### 阶段 0：技术债与基线加固

- [ ] ⬜ **0a Vulkan 正确性修复（A/B/C/D/F）**（ROADMAP 0a；KB-03 表格即工作分解）— 🔴 必做
  - 提示：动手前核对 `documents/VULKAN_FIXES.md` 确认各项现状；A/B 是高危项。
- [ ] ⬜ **0c Vulkan 性能 P0：descriptor 复用 + ImmediateSubmit 批量化**（ROADMAP 0c）— 🟠 推荐，依赖 0a
- [ ] ⬜ **CI：GitHub Actions 矩阵 + ctest + clang-tidy/cppcheck**（ROADMAP 0b 剩余；GoogleTest 已就位）— 🔴 必做
  - 提示：MSVC 环境依赖 VS 工具链，CI 上用 microsoft/setup-msvc 或 vsdevcmd。

### 阶段 2：渲染与工具化

- [ ] ⬜ **编辑器：ImGuizmo 集成 + 鼠标拾取 + Add Component 动态加组件**（EDITOR_ROADMAP 阶段 2；拾取注意 KB-07/K-007 坐标换算先例）— 🟠
- [ ] ⬜ **2b 双后端抽象收敛：SPIR-V 反射 + RenderPassDesc + 去除 Y 翻转 hack**（ROADMAP 2b）— 🟡 可延后但 3e 依赖它
- [ ] ⬜ **2c Vulkan 性能 P1-P2 + PipelineCache 落盘**（ROADMAP 2c）— 🟡
- [ ] ⬜ **资产：Material 纹理绑定 + 导出 AssetLoader<Material> 特化**（KB-05 待办）— 🟠
- [ ] ⬜ **资产：异步加载 + 热重载（mtime 监听）**（KB-05 待办）— 🟢

### 阶段 3：内容子系统（按需，各子系统独立分支）

- [ ] ⬜ **3a 物理：Box3D 集成**（dependencies 已有 box3d-main；走 PB-01）— 🟢
- [ ] ⬜ **3b 音频：miniaudio**（走 PB-01）— 🟢
- [ ] ⬜ **3c 脚本：Lua（sol2）**（走 PB-01）— 🟢
- [ ] ⬜ **3d Job System**（1a 异步加载的地基）— 🟢
- [ ] ⬜ **3e 后处理/阴影/PBR**（依赖 2b）— 🟢 远期

### 文档（文档 Agent 持续）

- [ ] ⬜ **建立 `documents/guide/`：GettingStarted + ArchitectureOverview**（PB-06 P0 清单）— 🔴

## 🔍 需要更多信息

（暂无）

## 已合并 ✅

（暂无——示例行：`✅ 0a-A pipeline 缓存重建修复 @render-agent agent/render-agent/vulkan-pipeline-cache abc1234`）
