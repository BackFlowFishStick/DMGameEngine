# Vulkan 后端实现评审与优化建议

> 评审范围：`src/DMGameEngine/Platform/Vulkan/` 全部实现
> 日期：2026-07-21
> 视角：架构审查 + 性能优化（Vulkan 渲染工程）

## 结论

这是一套**结构清晰、能跑通**的 Vulkan 后端，正确采用了 Vulkan 1.3 的 Dynamic Rendering + Synchronization2，每帧同步链路（acquire -> submit -> present 的 semaphore/fence）闭环，image layout 转移路径基本正确。但它是**把 OpenGL 的逐 draw uniform/material 语义直接映射到 Vulkan** 的实现，代价是 CPU 侧每帧 descriptor/uniform 抖动严重；同时存在若干**正确性与生命周期隐患**，其中「pipeline 缓存不随 swapchain 重建清空」和「无帧内延迟销毁」最值得优先修。

---

## Vulkan 对象链路 / 渲染路径

- `Instance`(VulkanGraphicsContext.cpp:122) -> `Surface`(glfw, :175) -> `VulkanDevice::Init`(:70 物理设备选型 + 逻辑设备，pNext 链接 dynamic rendering + sync2，:195) -> `VmaAllocator`(:251) -> `VulkanSwapchain`(swapchain + 深度图，VulkanSwapchain.cpp:79) -> 每帧资源（2 帧 in-flight：CB pool + CB + imageAvailable/renderFinished semaphore + signaled fence，VulkanGraphicsContext.cpp:205）。
- 每帧：`Clear()` -> `BeginFrame()`(acquire -> wait fence -> reset/begin CB -> 颜色/深度 UNDEFINED->OPTIMAL barrier -> `vkCmdBeginRendering` clear，VulkanGraphicsContext.cpp:277) -> `DrawIndexed()`(commit uniform -> alloc+write descriptor set -> bind VBO/IBO -> get-or-create pipeline -> bind pipeline+descriptor sets -> draw，VulkanRendererAPI.cpp:180) -> `SwapBuffers()` -> `EndFrame()`(end rendering -> 颜色->PRESENT_SRC barrier -> submit -> present -> 推进 frame，VulkanGraphicsContext.cpp EndFrame)。
- 资源上传：`ImmediateSubmit`(单条 transient CB + fence + `vkQueueWaitIdle`，VulkanDevice.cpp:301)。

---

## 高风险检查点（正确性 / 生命周期）

### A. Pipeline 缓存不随 swapchain 重建而失效

`m_Pipelines` 键只含 shader + vertex + blend/depth/cull，pipeline 创建时把当前 swapchain 的 `colorFormat/depthFormat` 烧进 `VkPipelineRenderingCreateInfo`(VulkanRendererAPI.cpp:300,379,406)。`RecreateSwapchain`(VulkanGraphicsContext.cpp:257) 不清理该缓存。

- 后果：swapchain 格式变化（换显示器 / HDR / 驱动重选 format）时，`vkCmdBeginRendering` 的 attachment format 与 pipeline 声明格式不符 -> `VUID-vkCmdBeginRendering-None-06197` 及潜在 device lost。
- 标签：`[SPEC][VUID]`
- 修复：recreate 时 `vkDestroyPipeline` 全部 + `m_Pipelines.clear()`（保留 `VkPipelineCache` 对象本身即可）。

### B. 无帧内延迟销毁，资源析构与 in-flight frame 竞争

`VulkanTexture2D::~`/`Invalidate()`、`VulkanVertexBuffer/IndexBuffer::~` 析构直接 `vmaDestroyImage/Buffer`，不等待该资源被引用的帧完成。一旦 `shared_ptr` 在某帧提交后、GPU 未完成前释放 -> use-after-free。`~VulkanRendererAPI` 有 `vkDeviceWaitIdle`(VulkanRendererAPI.cpp:106)，但逐资源没有。

- 标签：`[SPEC][ENGINE]`
- 修复：引入 per-frame deletion queue，在对应 fence 被 `vkWaitForFences` 确认后销毁。

### C. `RequestResize` 是死代码

`Renderer::OnWindowResize`(Renderer.cpp:105) 只调 `SetViewport`，从不调 `VulkanGraphicsContext::RequestResize`。resize 完全依赖 `vkAcquireNextImageKHR` 返回 `OUT_OF_DATE/SUBOPTIMAL`(VulkanGraphicsContext.cpp:293,299)。

- 后果：resize 后 viewport 已是新尺寸但 swapchain 仍是旧 extent，中间会有 1–N 帧渲染区/视口不匹配（裁剪/闪烁）。
- 标签：`[ENGINE]`
- 修复：在 window resize 回调里 `ctx.RequestResize(w, h)`。

### D. `VK_CHECK` 在 release 为 no-op

无 `DMGE_ENABLE_ASSERTS` 时宏退化为 `(void)(x)`(VulkanDebug.h)。release 下所有 `vkCreate*/Allocate*` 的错误被静默吞掉，后续用 `VK_NULL_HANDLE` -> 崩溃且无诊断。

- 标签：`[ENGINE]`
- 修复：release 也至少 `DMGE_LOG_ERROR` + 合理降级。

### E. 反射式 UBO（regex + 手算 std140）覆盖不全

`TypeStd140Layout`(VulkanShader.cpp:50) 只覆盖固定标量/向量/mat2/3/4，遇 `double/struct/mat4x3/嵌套` 默认 `{16,16}` -> 偏移错位；`reUniform`(:229) 不处理多变量声明 `uniform vec2 a,b;`、struct 内 uniform、带 layout 限定符的 uniform，`RewriteStageBody`(:336) 剥离会漏 -> 与合成 UBO 块重复声明编译失败；`in/out` location 注入按源序自增，跨 stage 顺序不一致即静默错位。

- 标签：`[ENGINE]`，风险中等。
- 建议：长期改用 SPIR-V reflection（spirv-cross / SPIRV-Reflect）拿真实 offset 与 binding。

### F. 未绑定 sampler 槽 descriptor 留空

`WriteDescriptorSet` 中 `m_BoundTextures[i]` 为 null 时直接 `continue`，该 binding 不写(VulkanRendererAPI.cpp:558)。新分配 set 该 binding 为未更新 -> 采样未定义 + 未更新 binding 校验错误。

- 标签：`[SPEC][VUID]`
- 修复：绑定一个全局默认 dummy texture，或在缺失时 assert/警告。

### G. Uniform scratch 溢出在 release 下静默

`CommitUniforms` 超过 16MB 仅 `DMGE_CORE_ASSERT(false)`(VulkanRendererAPI.cpp)，release 下断言被剥，返回 offset 0 -> 所有后续 draw 读同一块 uniform -> 渲染错乱。

- 标签：`[ENGINE]`
- 修复：真实检查 + 刷新/分块分配 + 日志。

### H. Sampler descriptor `stageFlags` 仅 FRAGMENT

(VulkanShader.cpp CreatePipelineLayout)：vertex shader 做 vertex texture fetch（置换贴图等）时 descriptor 不可见 -> 校验错误。

- 标签：`[SPEC]`
- 修复：按 shader 用量合并 stage。

### I. `m_ImmediateCmd` 单 buffer 无并发保护

(VulkanDevice.cpp:301)：当前单线程无碍，但任何后台加载线程并发 `ImmediateSubmit` 即竞态。

- 标签：`[ENGINE]`

---

## 同步 / Layout 复核（已正确）

- 颜色 `UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL`：`srcStage=COLOR_ATTACHMENT_OUTPUT, srcAccess=0`，dst 正确--acquire semaphore 在 submit 时于该 stage 等待(VulkanGraphicsContext.cpp:322)。`[SPEC]`
- 颜色 `COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC`：`srcStage=COLOR_ATTACHMENT_OUTPUT/Write -> dstStage=BOTTOM_OF_PIPE/0`，present 前写完成保证正确。`[SPEC]`
- 深度 `UNDEFINED -> DEPTH_ATTACHMENT_OPTIMAL`：`src=TOP_OF_PIPE/0 -> dst=EARLY|LATE_FRAGMENT_TESTS, DepthRW`，新图无前置依赖，正确。`[SPEC]`
- dynamic UBO offset 已按 `MinUniformBufferOffsetAlignment` 对齐(VulkanRendererAPI.cpp CommitUniforms)：满足动态偏移对齐要求。`[SPEC]`

---

## 性能优化优先级

瓶颈初判：**CPU bound（descriptor/uniform 逐 draw 抖动）为主**，资源加载期 **sync bound（ImmediateSubmit 全 stall）**，GPU 侧当前无明显瓶颈（单 pass、1×MSAA、loadOp clear）。

### P0 - descriptor 逐 draw 分配 + 全量写 + 每帧 reset pool

(VulkanRendererAPI.cpp:203,476,581)：每 draw `vkAllocateDescriptorSets` + `vkUpdateDescriptorSets`（含每个 sampler 的 image info 写）。这是 OpenGL 抽象映射到 Vulkan 的主开销。

- 最小改动：对 `(shaderID, 已绑定纹理集合 hash)` 做缓存，绑定不变时复用同一 set（UBO 是 dynamic，每 draw 仅改 dynamic offset，set 不必变）。
- 中期：sampler 改 bindless（`VK_EXT_descriptor_indexing`，一个大数组 + 着色器内索引），消除逐 draw sampler 写。

### P0 - `ImmediateSubmit` 用 `vkQueueWaitIdle`

(VulkanDevice.cpp:321)：每次纹理/顶点/索引/mipmap 上传都 stall 整条 graphics 队列，且会等当前 in-flight 帧。

- 最小改动：上传批量化（一条 CB 录多份 copy，一次 submit + fence）。
- 中期：专用 transfer 队列 + ownership transfer barrier，staging ring buffer，按帧 fence 回收。

### P1 - `VulkanVertexArray::Bind()` 每 draw 堆分配

每次 draw 新建两个 `std::vector` + 逐 buffer `dynamic_pointer_cast`(VulkanVertexArray.cpp Bind)。改用栈数组 / 预留容量 / 缓存 `VkBuffer` 列表。

### P1 - `CommitUniforms` 每 draw 整块 memcpy

仅脏 uniform 段拷贝；或把高频改的 transform 改 push constants。

### P1 - `VkPipelineCache` 不落盘

(VulkanRendererAPI.cpp:404)：冷启动重编译所有 pipeline。用 `vkGetPipelineCacheData` 序列化到磁盘，启动加载。

### P2 - per-frame CB pool 用 `RESET_COMMAND_BUFFER` 而非 `TRANSIENT`

(VulkanGraphicsContext.cpp:213, VulkanDevice.cpp:268)：每帧重录的 pool 用 `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT` 更贴合驱动预期。`[GUIDE][ENGINE]`

### P2 - descriptor pool 上限静默截断

`maxSets=8192`、sampler=32768/帧(VulkanRendererAPI.cpp:424,426)，超限 `AllocateDescriptorSet` 失败仅 `DMGE_LOG_ERROR` 后 draw 被静默丢弃(:520)。加可观测计数与扩容/告警。

### P3 - `MaxUsableSampleCount` 死代码

(VulkanDevice.cpp:196) 计算了却硬编 `VK_SAMPLE_COUNT_1_BIT`(VulkanRendererAPI.cpp:341)。要么接 MSAA，要么删查询。

### P3 - 单线程单 primary CB

要进一步扩展到海量 draw，考虑 secondary CB + 多线程录制。

---

## 验证路径

- **Validation Layer**：`DMGE_ENABLE_ASSERTS` 下已开 Khronos validation。重点跑：resize/拖拽窗口跨屏（验 A 项 pipeline-format 失配）、加载大量纹理中途销毁（验 B 项 lifetime）、绑定未设纹理的 shader（验 F 项）。预期会看到 `VUID-vkCmdBeginRendering-...` 与 "Descriptor set ... encountered uninitialized binding" 类报错。
- **RenderDoc**：逐 draw 看 descriptor set 是否每帧新建、binding 是否更新；texture 释放后帧捕获查 "resource destroyed while in use"。
- **GPU profiler / AGI**：量 CPU 录制时间占比、`vkUpdateDescriptorSets` 与 `vkQueueWaitIdle` 等待时长，定位 P0 优化收益。
- **定向代码检查**：在 release 配置下注释 `VK_CHECK` 后构造 `vkAllocateDescriptorSets` 失败场景（D 项）；构造 >16MB uniform 场景（G 项）；构造含 `uniform vec2 a,b;` 或 struct uniform 的 shader（E 项）。

---

## 建议的修复顺序

1. **正确性**：pipeline 缓存随 swapchain recreate 清空 + 接 `RequestResize` 回调 + release 下 `VK_CHECK` 至少记日志（A / C / D）。
2. **生命周期**：per-frame deletion queue，资源析构延后到引用帧 fence 信号（B）。
3. **性能 P0**：descriptor set 复用 + `ImmediateSubmit` 批量化（两项主开销）。
4. **P1 / P2**：VertexArray 栈数组、PipelineCache 落盘、TRANSIENT pool。

---

## 准确性标签说明

- `[SPEC]` Vulkan Specification 明确要求
- `[VUID]` Valid Usage ID，Validation Layer 可验
- `[ENGINE]` 图形引擎工程经验，需案例/上下文支持
- `[GUIDE]` Vulkan Guide 官方建议
