# TASKS.md — 任务认领看板

> 认领协议见 AGENTS.md §7。状态机：`⬜ 待认领 → 🚧 进行中 → 👀 待 review → ✅ 已合并`（另有 `🔍 需要更多信息`）。
> 认领格式：把状态改为 `🚧 @<Agent名> <分支名> <日期>`；完成后 `✅ <合并commit>`。
> 条目来源标注：ROADMAP 编号（ENGINE_ROADMAP / EDITOR_ROADMAP）或 BUG/IDEA。

## 进行中 / 待 review

（暂无——等待下一波认领）

## 已合并 ✅

- ✅ DirectX 阶段 B：窗口交换链 + 完整前向路径 @d3d-agent（合并 `e4ae7cb`；DirectXGraphicsContext（flip-discard/resize/Present）+ 前向 API 补齐（索引数组 uniform/Mat4 实例属性/SwapChainTarget）+ game 经 `DMGE_API=D3D11` 真窗口运行；窗口路径全自动像素回读验证（绕开 K-014）；K-031~035 五坑；已知限制：ImGui/延迟对齐留阶段 C）
- ✅ 2b 阶段 1：RenderPassDesc 统一 pass 语义 @render-agent（合并 `3c10480`波次内；RenderPassDesc.h 附件表/loadOp/clear/NdcZMin，两后端唯一消费路径，旧形态保留兼容；u_NdcZMin 渲染层 API 判断已移除；K-024 经 desc 直通验证；Y-flip 消除路径记录待单独立项；双后端 88/88）
- ✅ 编辑器动画预览 + 快捷键落成 @editor-agent（合并 `bc69019`；Inspector Animator 段（clip 下拉/播放/速度/scrub）、双 System 成对注册（K-029 exclude 语义）、SceneDuplicator 白名单补 Animator（K-028）、编辑态 scrub 预览决策；快捷键 Ctrl+N/S/O/Shift+A/Del/F5/F6 真实现（K-030 让路规则）；ON 74/74 OFF 50/50）
- ✅ 资产 Tour + 档案修正 @docs-agent（合并 `d939249` 波次内；AssetTour.md 九节+生命周期图；EDITOR_ROADMAP 状态注记 + KB-04 两处核验修正；新发现 KB-05 过时/registry 无生产调用方已登记）

- ✅ 3e 延迟渲染（可配置，双后端）@render-agent（合并 `3c10480`；G-buffer RT0 RGBA8/RT1 RGBA16F/深度复用 + 全屏光照 pass；OpenGL 零改动（MRT 已具备）、Vulkan 补齐 3 处 MRT（K-024：pipeline 键必须含附件数）；默认 Forward，运行时可切换；光源 uniform 与前向同名；透明物阶段 1 限制已记录；双后端 ctest 50/50）
- ✅ DirectX 后端阶段 A（D3D11）@d3d-agent（合并 `eadde05`；设备/shader 反射/纹理/缓冲/VA/状态对象/离屏 FB + headless 渲染回读 smoke 测试（硬件→WARP 回退）；K-025：D3D11 PS 输入按寄存器序链接——varying 必须声明在 SV_Position 前；ON/OFF/Vulkan 共存三组 41/41、37/37、41/41；管理员完成 RendererAPI 工厂接线）
- ✅ CI 首跑三连修复 @管理员（Ninja 化修复 VS2026 镜像 generator 失败；测试资产收编 engine/tests/assets + DMGE_TEST_ASSETS_DIR 宏；.gitignore *.obj 例外——K-022；第三跑全绿 26c1664）

- ✅ 骨骼动画系统阶段 1 @anim-agent（合并 `37eb198`；assimp aiBone/aiAnimation 导入、Skeleton/AnimationClip 派生资产 `<model>#skeleton`/`#anim/<i>`、AnimatorComponent+AnimationSystem、per-draw 调色板蒙皮（论证避开 descriptor 缓存失效）、BlinnPhongSkinned.glsl、序列化往返；DMGE_ANIMATION ON/OFF 双开关 61/61 与 37/37 全绿；Vulkan 蒙皮路径留后续）
- ✅ 2c Vulkan 性能 P1-P2 @render-agent（合并 `edb9f8e`；Bind 热路径去 vector/dynamic_cast ~200×、PipelineCache 落盘热建 ~21×（失效四路径实测）、TRANSIENT command pool、descriptor pool 告警+双倍扩容（扩容不清 P0 缓存）；Vulkan ON/OFF 37/37 双绿；PipelineCache 驱动拒绝坑 K-019）
- ✅ docs：3 处不一致修正 + SceneAndECSTour/EditorTour @docs-agent（合并 `d939249`；EDITOR_ROADMAP 死路径/Ctrl 快捷键假标注/KB-06 指涉修正；新发现菜单假快捷键与 EDITOR_ROADMAP 过时已登记）

- ✅ 编辑器阶段3收尾+阶段4 @editor-agent（合并 `d527f04`；多 Scene 标签页：SceneTab 容器/独立相机与选中态/全局单 Play 语义；导出可运行工程：ProjectExporter 生成 add_subdirectory 模板工程+资产拷贝+相对路径 shader；实测发现引擎 install(EXPORT) 根本不可用（K-015）与 EntryPoint 丢 argv（K-016）；构建 0 error、ctest 37/37、/W4 零新增。GUI 与导出工程构建待人工验证）
- ✅ 0c Vulkan 性能 P0 @render-agent（合并 `d1d357c`；descriptor (shaderID,纹理句柄hash) 缓存复用 ~7.6×、ImmediateSubmit Begin/End 批量化 ~450×（独立无 surface 微基准，RTX 4070 Ti Debug，方法与限制见 VULKAN_FIXES §11.3）；deletion queue flush 语义未变、test_deletion_queue 未改仍绿；Vulkan ON/OFF 双路径 37/37）
- ✅ K-011 正解：enable_testing() 挪根 CMakeLists @test-agent（合并 `6080623`；根 build 目录 ctest 直接发现 37 例全绿；engine 独立构建兜底保留；ci.yml/CI.md/AGENTS.md §4/KB-07 K-011 状态行同步；顺手加 clang-tidy 报告型 job 骨架 continue-on-error）
- ✅ guide P2：Glossary 32 条术语 + 过时段落回访 @docs-agent（合并 `80dc0c3`；RendererTour/LearningPath/ArchitectureOverview 修正，新发现 3 处死路径/失效快捷键/KB-06 指涉已登记待修）
- ✅ 编辑器功能补完（阶段 3/4）@editor-agent（分支 tip `7b435e9`；Play 快照隔离走 SceneDuplicator 对象级深拷贝而非 JSON 快照——K-012：序列化会丢程序化网格；Asset 拖拽建 Mesh 实体；File 菜单 + editor_config.ini 最近文件；Prefab 走临时 Scene 方案引擎零改动；构建 0 error、ctest 37/37、/W4 零新增。GUI 行为待人工验证，步骤见该 agent 报告 / EDITOR_ROADMAP 标注）
- ✅ guide P1：渲染导览 + 阅读路线图 @docs-agent（分支 tip `08b990f`；RendererTour/LearningPath 两篇全事实核验；发现 4 处既有文档与代码不一致已登记待修，见 RendererTour §4 注记）
- ✅ 0a Vulkan 正确性修复 A/B/C/D/F @render-agent（分支 tip `1ae2aa6`；A/C/D/F 核验为 07-26 已修，本次加固 A `9219cc7` + 落地 B per-frame deletion queue `1ae2aa6` 含 headless 性质单测；PB-08 通过：Vulkan ON/OFF 双路径构建零 error、/W4 零新增、ctest 37/37；遗留：Validation 运行时场景待人工跑 editor/game；E 项留 2b）
- ✅ 0b CI 骨架 @test-agent（分支 tip `35fedb2`；交付 `.github/workflows/ci.yml` + `.github/CI.md` + 保守版 `.clang-tidy` 暂不强制门禁；本地 Ninja+vcvars 全量构建 0 error、ctest 32/32 绿；Vulkan job 留注释骨架待固定 SDK 版本；ctest 目录坑见 KB-07/K-011）

## 待认领（按优先级）

> 2026-10-05 看板重整：已完成条目归档至"已合并 ✅"区，本区只保留真正可开工的任务。

### 渲染与后端

- [ ] ⬜ **DirectX 阶段 C**（DIRECTX_BACKEND_DESIGN 路线表）：ImGui D3D11 后端（编辑器面板当前 D3D11 下缺失）、蒙皮 `SetMat4Array`、TextureCube/2DArray、延迟路径对齐（当前 D3D11 固定 Forward）— 🟠
- [ ] ⬜ **2b 阶段 2**（ROADMAP 2b 余量）：SPIR-V 反射（取代 regex+手算 std140）、Y-flip 实移除（投影约定统一，消除路径见 DEFERRED_RENDERING_DESIGN §7.1）、per-target blend、资源屏障抽象 — 🟡
- [ ] ⬜ **3e 剩余**：后处理管线（bloom/tonemap）、阴影贴图、PBR/IBL — 🟢 远期（依赖 2b 余量）
- [ ] ⬜ **透明物体前向回退**（延迟渲染阶段 1 限制）：场景侧提供透明标记数据 + 光照 pass 后补画（挂载点已预留）— 🟢
- [ ] ⬜ **Vulkan 蒙皮路径**：`SetMat4Array` Vulkan 实现（阶段 1 骨骼动画为 OpenGL-only，TODO warn-once 中）— 🟢
- [ ] ⬜ **editor 的 D3D11 资产适配**：editor 的 GLSL shader 资产在 D3D11 下无法编译（D3D 阶段 B 未验证 editor 运行时）——依赖 DirectX 阶段 C 的 shader 侧方案 — 🟢

### 资产系统

- [ ] ⬜ **registry.json 生产接线**：`LoadRegistry/SaveRegistry` 目前只有测试调用——编辑器/游戏启动加载与退出保存，让"跨启动 UUID 稳定"真正闭环（AssetTour §6 已如实标注）— 🟠
- [ ] ⬜ **资产异步加载 + 热重载（mtime 监听）**（KB-05 待办；依赖 3d Job System 更佳）— 🟢
- [ ] ⬜ **GPU 延迟释放**：资产 `Ref` 归零后的延迟释放队列（对齐 Vulkan deletion queue，帧在飞不可即删）— 🟢

### 内容子系统（各子系统独立分支，走 PB-01）

- [ ] ⬜ **3a 物理：Box3D 集成**（dependencies 已有 box3d-main）— 🟢
- [ ] ⬜ **3b 音频：miniaudio** — 🟢
- [ ] ⬜ **3c 脚本：Lua（sol2）** — 🟢
- [ ] ⬜ **3d Job System**（资产异步加载的地基）— 🟢

### 文档（文档 Agent）

- [ ] ⬜ **KB-05 刷新**：特化导出清单（五个已导出）、纹理绑定已实现、骨骼动画已落地——过时描述会误导子系统 Agent（第六波 docs-agent 登记）— 🟠
- [ ] ⬜ **LearningPath 站⑦快捷键文本对齐**：editor-agent 第六波已实现真快捷键，按其报告更新 — 🟢
- [ ] ⬜ **EDITOR_ROADMAP 结构性进度对齐**：阶段 3/4 均已落地，正文严重滞后——文档 Agent 开 `docs/` 分支做"阶段 3-4 演进"新章（非增量注记）— 🟢
- [ ] ⬜ **动画子系统 Tour**（P2 候选）：Skeleton/AnimationClip/AnimationSystem/Palette — 🟢
- [ ] ⬜ **Play 模式与 SceneDuplicator 专题**（P2 候选）— 🟢

## 🔍 需要更多信息

（暂无）
