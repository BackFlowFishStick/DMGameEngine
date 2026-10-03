# TASKS.md — 任务认领看板

> 认领协议见 AGENTS.md §7。状态机：`⬜ 待认领 → 🚧 进行中 → 👀 待 review → ✅ 已合并`（另有 `🔍 需要更多信息`）。
> 认领格式：把状态改为 `🚧 @<Agent名> <分支名> <日期>`；完成后 `✅ <合并commit>`。
> 条目来源标注：ROADMAP 编号（ENGINE_ROADMAP / EDITOR_ROADMAP）或 BUG/IDEA。

## 进行中 / 待 review

| 任务 | 状态 | 备注 |
|---|---|---|
| 骨骼动画系统阶段 1：导入 + Skeleton 资产 + AnimationComponent/System + 蒙皮渲染 | 🚧 @anim-agent agent/anim-agent/skeleton-stage1 2026-10-03 | worktree `.worktrees/anim-agent`；IDEA 新立项（用户要求），依赖 1b/1a 已满足 |
| 2c Vulkan 性能 P1-P2：VertexArray 每帧分配 / PipelineCache 落盘 / CB pool TRANSIENT / descriptor pool 告警扩容 | 🚧 @render-agent agent/render-agent/vulkan-p1p2 2026-10-03 | worktree `.worktrees/render-agent`；本波禁改 CMakeLists/DMGameEngine.h/shaders（动画 Agent 在动） |
| docs：修复 3 处登记不一致 + 场景/ECS Tour + 编辑器 Tour | 🚧 @docs-agent docs/tours 2026-10-03 | worktree `.worktrees/docs-agent`；编辑器 Tour 现在可写（多场景已合入） |

## 已合并 ✅

- ✅ 编辑器阶段3收尾+阶段4 @editor-agent（合并 `d527f04`；多 Scene 标签页：SceneTab 容器/独立相机与选中态/全局单 Play 语义；导出可运行工程：ProjectExporter 生成 add_subdirectory 模板工程+资产拷贝+相对路径 shader；实测发现引擎 install(EXPORT) 根本不可用（K-015）与 EntryPoint 丢 argv（K-016）；构建 0 error、ctest 37/37、/W4 零新增。GUI 与导出工程构建待人工验证）
- ✅ 0c Vulkan 性能 P0 @render-agent（合并 `d1d357c`；descriptor (shaderID,纹理句柄hash) 缓存复用 ~7.6×、ImmediateSubmit Begin/End 批量化 ~450×（独立无 surface 微基准，RTX 4070 Ti Debug，方法与限制见 VULKAN_FIXES §11.3）；deletion queue flush 语义未变、test_deletion_queue 未改仍绿；Vulkan ON/OFF 双路径 37/37）
- ✅ K-011 正解：enable_testing() 挪根 CMakeLists @test-agent（合并 `6080623`；根 build 目录 ctest 直接发现 37 例全绿；engine 独立构建兜底保留；ci.yml/CI.md/AGENTS.md §4/KB-07 K-011 状态行同步；顺手加 clang-tidy 报告型 job 骨架 continue-on-error）
- ✅ guide P2：Glossary 32 条术语 + 过时段落回访 @docs-agent（合并 `80dc0c3`；RendererTour/LearningPath/ArchitectureOverview 修正，新发现 3 处死路径/失效快捷键/KB-06 指涉已登记待修）
- ✅ 编辑器功能补完（阶段 3/4）@editor-agent（分支 tip `7b435e9`；Play 快照隔离走 SceneDuplicator 对象级深拷贝而非 JSON 快照——K-012：序列化会丢程序化网格；Asset 拖拽建 Mesh 实体；File 菜单 + editor_config.ini 最近文件；Prefab 走临时 Scene 方案引擎零改动；构建 0 error、ctest 37/37、/W4 零新增。GUI 行为待人工验证，步骤见该 agent 报告 / EDITOR_ROADMAP 标注）
- ✅ guide P1：渲染导览 + 阅读路线图 @docs-agent（分支 tip `08b990f`；RendererTour/LearningPath 两篇全事实核验；发现 4 处既有文档与代码不一致已登记待修，见 RendererTour §4 注记）
- ✅ 0a Vulkan 正确性修复 A/B/C/D/F @render-agent（分支 tip `1ae2aa6`；A/C/D/F 核验为 07-26 已修，本次加固 A `9219cc7` + 落地 B per-frame deletion queue `1ae2aa6` 含 headless 性质单测；PB-08 通过：Vulkan ON/OFF 双路径构建零 error、/W4 零新增、ctest 37/37；遗留：Validation 运行时场景待人工跑 editor/game；E 项留 2b）
- ✅ 0b CI 骨架 @test-agent（分支 tip `35fedb2`；交付 `.github/workflows/ci.yml` + `.github/CI.md` + 保守版 `.clang-tidy` 暂不强制门禁；本地 Ninja+vcvars 全量构建 0 error、ctest 32/32 绿；Vulkan job 留注释骨架待固定 SDK 版本；ctest 目录坑见 KB-07/K-011）

## 待认领（按优先级）

### 阶段 0：技术债与基线加固

- [x] ✅ **0a Vulkan 正确性修复（A/B/C/D/F）**— 已完成（见上方已合并）
- [x] ✅ **0c Vulkan 性能 P0** — 已完成；P1-P2 由第四波 render-agent 认领中
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
