# DMGameEngine 人类向文档库（guide/）

> 本目录是 **guide**：写给人类读者的转述与提炼——上手教程、架构总览、子系统讲解。
> 它与 `documents/` 根下的 17 篇**工程档案**（设计决策、评审、踩坑实录）分工不同：
>
> | 库 | 读者 | 内容 | 关系 |
> |---|---|---|---|
> | `documents/guide/`（本目录） | 新人 / 学习者 / 未来的自己 | 叙事、图示、上手路径 | **转述与提炼，链接到工程档案，不复制它们** |
> | `documents/*.md`（根） | 参与开发的人 | 历史设计 / 评审 / 事故实录 | 单一事实源，只增不毁 |
> | `kb/`、`playbooks/` | AI Agent | 步骤与避坑 | 可执行的操作规范 |
>
> 铁律：guide 里写的每个路径、类名、数字都必须对照代码核验过。发现 guide 与代码不符，以代码为准并请反馈给文档 Agent。

## 当前文档索引

| 文档 | 内容 | 适合谁 |
|---|---|---|
| [GettingStarted.md](GettingStarted.md) | 克隆 → 环境要求 → 构建 → 跑 game / editor → 跑测试 → 目录导览 | 第一次接触本仓库的人 |
| [ArchitectureOverview.md](ArchitectureOverview.md) | 模块图、一帧的生命周期（主循环五阶段）、各子系统简介与入口文件 | 想读懂代码结构的人 |
| [RendererTour.md](RendererTour.md) | 渲染三层抽象、一帧到 GPU 的数据流、RenderQueue 合批与实例化、Material/MaterialInstance、离屏 RTT、光照数据流、双后端差异速览 | 想读懂渲染代码的人 |
| [SceneAndECSTour.md](SceneAndECSTour.md) | 场景与 ECS：entt registry 与 Entity 句柄、6 个核心组件、三叉链层级与脏标记传播、System 注册顺序与一帧数据流、SceneSerializer 两趟加载与序列化取舍 | 想读懂 Scene/序列化代码的人 |
| [EditorTour.md](EditorTour.md) | 编辑器：独立 exe 消费 DLL（ImGui 跨 DLL 单 context）、多标签与全局单 Play、Play 快照隔离（SceneDuplicator）、Asset 拖拽、Prefab、导出可运行工程、viewport 坐标坑与面板操作流 | 想读懂/改编辑器代码的人 |
| [LearningPath.md](LearningPath.md) | 代码阅读路线图：从主循环到编辑器消费视角的 7 站阅读顺序，每站带入口文件、自检问题与常见误区 | 第一次通读源码的人 |
| [Glossary.md](Glossary.md) | 术语表：工程与构建 / 渲染 / ECS 与场景 / 资产 / 编辑器五组项目特有词汇，每条一句话定义 + 已核验代码锚点 + 深入阅读链接 | 读档案或代码注释时遇到生词的人 |

## 后续波次待写清单（P1 / P2）

> 按优先级排列；一次只写一篇，写完对照代码核验后再写下一篇。

| 优先级 | 文档 | 计划内容 | 素材来源 |
|---|---|---|---|
| P1 | Subsystem Tour — 资产系统 | AssetUUID 三表结构、Load 去重缓存、Registry 持久化、assimp 导入 | `Asset/**`、ASSET_DESIGN / ASSET_UUID_CONCEPTS |

> 已完成（2026-10，`docs/tours`）：场景与 ECS Tour（[SceneAndECSTour.md](SceneAndECSTour.md)）与编辑器 Tour（[EditorTour.md](EditorTour.md)），P1 清单只剩资产 Tour。

## 维护约定

- 工程档案（`documents/` 根）**只增不毁**：guide 发现档案过时时，不改档案，只在档案顶部加"⚠️ 状态更新"注记（该操作由文档 Agent 按 PB-06 执行）。
- 每次 develop 有集成合并后，文档 Agent 从 `git log` 与 ENGINE_SUMMARY.md 增量中提炼"本阶段进展"，同步更新对应 guide 章节，防止文档与代码脱节。
- 写作规范见 `playbooks/PB-06-人类向文档撰写.md`。
