# Vulkan 后端阶段 0 修复实施

> 日期：2026-07-26
> 范围：`ENGINE_ROADMAP.md` 阶段 0 三项（A/B/C/D/F 正确性、测试+CI 骨架、P0 性能）
> 依据：`VulkanBackendReview.md`（2026-07-21）+ 实际源码核验（2026-07-26）
> 编译：由用户执行（本次不编译，仅落代码与设计）

---

## 0. 本次落地清单

| 项 | 状态 | 改动文件 |
|---|---|---|
| A pipeline 缓存随 swapchain recreate 失效 | ✅ 已落地 | VulkanRendererAPI.h/.cpp、VulkanGraphicsContext.cpp |
| C RequestResize 死代码 | ✅ 已落地 | GraphicsContext.h、VulkanGraphicsContext.h/.cpp、WindowsWindow.h/.cpp |
| D VK_CHECK release no-op | ✅ 已落地 | VulkanDebug.h |
| F 未绑定 sampler 槽 descriptor 留空 | ✅ 已落地 | VulkanRendererAPI.h/.cpp |
| E2 测试 + CI 骨架 | ✅ 已落地 | tests/、CMakeLists.txt、.github/workflows/ci.yml |
| B 无帧内延迟销毁 | ⏳ 设计完成，未落地（架构性改动，见 §3） |
| P0 descriptor 逐 draw 分配 | ⏳ 设计完成，未落地（见 §4） |
| P0 ImmediateSubmit vkQueueWaitIdle | ⏳ 设计完成，未落地（见 §4） |

> **A 项严重度修正**：`VulkanBackendReview.md`（07-21）将 A 列为"高风险 device lost"。07-25 离屏渲染改动已把 `colorFormat/depthFormat` 纳入 `PipelineKey` + `Clear()` 每帧从 swapchain 刷新 `m_ActiveColorFormat`，故 swapchain 格式变化时新 pipeline 用新格式 key、不会错配复用触发 `VUID-vkCmdBeginRendering-None-06197`。A 项实际降为"旧 format pipeline 小泄漏"。本次落地的 `OnSwapchainRecreate()` 是防御性清理（消除泄漏 + 双保险）。`[ENGINE]`

---

## 0.1 实施记录与验证结果（2026-07-26）

**编译环境**：CLion + Ninja + MSVC 19.51（VS 18 Community）+ Vulkan SDK 1.4.350.0，`DMGE_BUILD_SHARED=ON` + `DMGE_VULKAN_BACKEND=ON` + `DMGE_BUILD_TESTS=ON`。

**编译验证 ✅**：A/C/D/F 改动 + Vulkan 后端整体编译链接通过（`dmge_tests` 链接含 Vulkan 后端的 `DMGameEngine.dll`，链接成功即证明 Vulkan 后端代码可编译）。

**测试验证 ✅**：5/5 纯逻辑单测通过：
- `Timestep.ConvertsSecondsAndMilliseconds` / `ImplicitFloatConversion` / `ZeroIsZero`
- `ShaderDataType.SizeBytes`
- `BufferLayout.ComputesStrideAndOffsets`

**落地踩坑修复**（`tests/CMakeLists.txt`）：
- CMP0135 设 NEW（消除 FetchContent URL 下载时间戳警告）。
- dmge_tests 补 `/utf-8`（spdlog fmt bundled 在 MSVC static_assert 要求；引擎 `/utf-8` 是 PRIVATE 不传播给消费者）。
- `DMGE_BUILD_SHARED=ON` 时 POST_BUILD `copy_if_different` 把 `DMGameEngine.dll` 拷到 `dmge_tests.exe` 同目录（`gtest_discover_tests` 构建时跑 exe 列测试用例，DLL 不在同目录则 `0xc0000135 STATUS_DLL_NOT_FOUND`）。

**待验证（运行时 ⏳）**：A/C/D/F 的 Validation Layer 场景（见 §8）需跑实际 Vulkan 渲染程序确认；当前仅编译通过。

---

## 1. A 项 — pipeline 缓存随 swapchain recreate 清空

**问题**：`RecreateSwapchain` 不通知 `VulkanRendererAPI`，旧 `VkPipeline` 留在 `m_Pipelines` 缓存。格式变化时泄漏；若格式恰好回退，可能复用旧 pipeline。`[ENGINE]`

**修复**：`VulkanRendererAPI::OnSwapchainRecreate()` 销毁全部缓存 pipeline、清空 map（保留 `VkPipelineCache` 对象）。`VulkanGraphicsContext::RecreateSwapchain()` 在 `m_Swapchain.Recreate()` 后调用它。

**对象链路**：
```
window resize -> WindowsWindow size callback -> context.RequestResize()
  -> m_NeedsResize=true
  -> 下一帧 BeginFrame() -> RecreateSwapchain()
     -> m_Swapchain.Recreate(w,h)              [新 swapchain + 新 format]
     -> VulkanRendererAPI::Get()->OnSwapchainRecreate()
        -> for (key,pipeline) in m_Pipelines: vkDestroyPipeline()
        -> m_Pipelines.clear()
        (VkPipelineCache m_PipelineCache 保留)
  -> Clear() -> m_ActiveColorFormat = swapchain.GetImageFormat()  [刷新]
  -> GetOrCreatePipeline() -> key 含新 colorFormat -> cache miss -> 重建
```

**同步**：`RecreateSwapchain` 在 `BeginFrame` 的 `vkWaitForFences` 之后调用（该帧 slot 已 idle），销毁 pipeline 安全。pipeline 不被 in-flight 帧引用（上一帧已提交且 fence 已信号）。`[SPEC]`

**销毁流程**：`vkDestroyPipeline(device, pipeline, nullptr)` × N，然后 `m_Pipelines.clear()`。`VkPipelineCache` 不销毁（跨 swapchain 复用，冷启动加速）。

**改动文件**：
- `VulkanRendererAPI.h`：加 `void OnSwapchainRecreate();`
- `VulkanRendererAPI.cpp`：实现（遍历销毁 + clear）+ Init 后插入
- `VulkanGraphicsContext.cpp`：include VulkanRendererAPI.h + RecreateSwapchain 末尾调用

---

## 2. C 项 — RequestResize 接线（消除 resize 后中间帧闪烁）

**问题**：`Renderer::OnWindowResize` 只调 `SetViewport`，window resize 回调不调 `RequestResize`。`m_NeedsResize` 仅在 `vkAcquireNextImageKHR` 返回 `OUT_OF_DATE` 时设置 -> resize 后 1..N 帧 viewport 已新尺寸、swapchain 仍是旧 extent -> 裁剪/闪烁。`[ENGINE]`

**修复**：
1. `GraphicsContext` 基类加 `virtual void RequestResize(uint32_t, uint32_t) {}`（默认 no-op，OpenGL 隐式 resize 无需实现）。
2. `VulkanGraphicsContext::RequestResize` 加 `override`（设 `m_NeedsResize=true`）。
3. `RecreateSwapchain` 改为始终用 `glfwGetFramebufferSize` 取真实像素尺寸（RequestResize 仅作触发标志，避免 DPI 下逻辑/像素不一致）。
4. `WindowsWindow`：`WindowData` 加 `GraphicsContext* context`；Init 设 `m_data.context = m_context.get()`；`glfwSetWindowSizeCallback` 里调 `data.context->RequestResize(w,h)`。

**对象链路**：
```
glfwSetWindowSizeCallback -> data.context->RequestResize(w,h)
  -> m_NeedsResize=true
  -> 下一帧 BeginFrame() -> 检测 m_NeedsResize -> RecreateSwapchain()
     -> glfwGetFramebufferSize() 取真实像素
     -> m_Swapchain.Recreate(pixelW, pixelH)
     -> OnSwapchainRecreate() [A 项]
  -> vkAcquireNextImageKHR 用新 swapchain（不再 OUT_OF_DATE）
```

**同步**：resize 触发路径与 acquire OUT_OF_DATE 路径都汇聚到 `RecreateSwapchain`，均在 `vkWaitForFences` 后，无 in-flight 帧冲突。`[SPEC]`

**验证**：拖拽窗口边框快速 resize，观察是否还有 resize 后短暂裁剪/闪烁；Validation Layer 应无 `VK_ERROR_OUT_OF_DATE` 之外的报错。

**改动文件**：`GraphicsContext.h`、`VulkanGraphicsContext.h`（override）、`VulkanGraphicsContext.cpp`（RecreateSwapchain）、`WindowsWindow.h`（WindowData）、`WindowsWindow.cpp`（context 指针 + callback）。

---

## 3. D 项 — VK_CHECK release 不再静默

**问题**：`#define VK_CHECK(x) (void)(x)` 在 release 下吞掉所有 `vkCreate*/vkAllocate*` 的 `VkResult`，后续用 `VK_NULL_HANDLE` 崩溃且无诊断。`[ENGINE]`

**修复**：release 分支改为检查 `VkResult != VK_SUCCESS` 并 `DMGE_LOG_ERROR`（含可读名 + 整数值 + 调用文本），**不**断言（避免 release 崩溃）。调用仍执行（副作用不变，`x` 仍求值）。

```cpp
#else
    #define VK_CHECK(x) \
        do { \
            VkResult _r = (x); \
            if (_r != VK_SUCCESS) \
                DMGE_LOG_ERROR("Vulkan error {} ({}) in: {}", \
                    ::DMGameEngine::Detail::VKResultString(_r), \
                    static_cast<int>(_r), #x); \
        } while (false)
#endif
```

**注意**：VK_CHECK 后若立即使用 handle（如 `VK_CHECK(vkCreateBuffer(..., &buf)); ... use buf`），失败时 buf 仍是 `VK_NULL_HANDLE`，后续仍崩，但日志已留诊断线索。对真正需要根据返回值分支的调用，应直接检查 VkResult 而非用 VK_CHECK。`[ENGINE]`

**改动文件**：`VulkanDebug.h`。

---

## 4. F 项 — 未绑定 sampler 槽绑定 dummy texture

**问题**：`WriteDescriptorSet` 中 `m_BoundTextures[i]` 为 null 时 `continue`，该 binding 不写。新分配 descriptor set 该 binding 未更新 -> `VUID-vkCmdDraw-None-02692`"descriptor set encountered uninitialized binding" + 未定义采样。`[SPEC][VUID]`

**修复**：`VulkanRendererAPI` 持有全局 1×1 dummy（`VkImage`+`VkImageView`+`VkSampler`）。Init 创建（`vmaCreateImage` + `ImmediateSubmit` 转 `UNDEFINED->SHADER_READ_ONLY_OPTIMAL` + view + sampler）。`WriteDescriptorSet` 中 unbound slot 写 dummy 的 sampler/view。

**对象链路**：
```
Init -> CreateFrameResources() -> CreateDummyResources()
  -> vmaCreateImage(1x1 RGBA8, SAMPLED|TRANSFER_DST)  [VMA_MEMORY_USAGE_AUTO]
  -> ImmediateSubmit: TransitionImageLayout(UNDEFINED -> SHADER_READ_ONLY_OPTIMAL,
       src=TOP_OF_PIPE/0, dst=FRAGMENT_SHADER/SHADER_READ)
  -> vkCreateImageView
  -> vkCreateSampler(nearest, clamp-to-edge, opaque-black border)
DrawIndexedCommon -> WriteDescriptorSet
  -> for each sampler slot: tex? tex->GetVk*() : {m_DummySampler, m_DummyView}
~VulkanRendererAPI -> DestroyDummyResources()
  -> vkDestroySampler / vkDestroyImageView / vmaDestroyImage
```

**同步**：dummy 在 Init 创建，Init 早于任何 draw。layout transition 经 `ImmediateSubmit`（`vkQueueWaitIdle`）同步完成。dummy 跨帧只读，无 in-flight 竞争。`[SPEC]`

**销毁**：析构在 `DestroyFrameResources` 前，`vkDeviceWaitIdle` 已在析构开头调用，安全销毁 dummy。

**验证**：构造一个 shader 声明 2 个 sampler 但只 Bind 1 个 texture，draw 后 Validation Layer 不应再报 "uninitialized binding"。

**改动文件**：`VulkanRendererAPI.h`（dummy 成员 + 方法声明）、`VulkanRendererAPI.cpp`（CreateDummyResources/DestroyDummyResources 实现 + Init 调用 + 析构调用 + WriteDescriptorSet unbound 分支）。

> ⚠️ 若项目 VMA 版本 < 3.0，`VMA_MEMORY_USAGE_AUTO` 不存在，改为 `VMA_MEMORY_USAGE_GPU_ONLY`。核验现有 `VulkanTextureHelpers.h` 的 `CreateImage` 确认。

---

## 5. E2 — 测试 + CI 骨架

**落地**：
- `tests/CMakeLists.txt`：GoogleTest（FetchContent v1.14.0）+ `dmge_tests` 链接 `DMGameEngine`。
- `tests/test_core.cpp`：纯逻辑测试（Timestep 转换、ShaderDataTypeSize、BufferLayout stride/offset），无 GPU 依赖，headless 可跑。
- 顶层 `CMakeLists.txt`：`option(DMGE_BUILD_TESTS)` + `add_subdirectory(tests)` + `enable_testing()`。
- `.github/workflows/ci.yml`：Windows + Ubuntu 矩阵，`cmake -DDMGE_BUILD_TESTS=ON` + build + ctest；Vulkan 编译 job（待加 Vulkan SDK 安装步骤）。

**启用**：`cmake -B build -S . -DDMGE_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure`

**待补**（后续）：渲染冒烟测试（headless offscreen FrameBuffer 跑一帧 Init->BeginScene->Submit->EndScene->Shutdown，断言不崩溃 + Validation Layer 无 error）；Vulkan SDK CI job。

---

## 6. B 项设计 — per-frame deletion queue（未落地）

**问题**：`VulkanTexture2D::~` / `Invalidate()`、`VulkanVertexBuffer::~`、`VulkanIndexBuffer::~` 析构直接 `vmaDestroyImage/Buffer`，不等引用帧完成。`shared_ptr` 在帧提交后、GPU 未完成前释放 -> use-after-free。`[SPEC][ENGINE]`

**设计**：在 `VulkanDevice`（中央，所有资源经 `VulkanDevice::Get()`）加 per-frame deletion queue。

**对象链路**：
```
资源析构（任意线程，通常主线程）
  -> VulkanDevice::PushDeletion(frameIndex, [dev, image, alloc]{ vmaDestroyImage(...) })
     // frameIndex = VulkanGraphicsContext::GetCurrentFrame() (该资源最后被用的帧)
     // push 到 m_DeletionQueues[frameIndex]

BeginFrame (下一轮同一 frameIndex)
  -> vkWaitForFences(frameIndex)        [该帧 slot 的上一轮提交完成]
  -> flush m_DeletionQueues[frameIndex]  [逐个执行 lambda 销毁]
  -> vkResetCommandBuffer / begin
```

**关键点**：
- 资源析构时取 `currentFrame` 作为桶。但析构可能发生在帧外（Shutdown）-- 此时 `vkDeviceWaitIdle` 后直接销毁（deletion queue 也可在 Shutdown flush 全部）。
- `BeginFrame` 已有 `vkWaitForFences`，在它之后 flush 该帧桶最自然。
- 桶里存 `std::function<void()>`（类型擦除，覆盖 image/buffer/sampler/view/pipeline）。

**代码骨架**（`VulkanDevice.h`）：
```cpp
#include <functional>
#include <vector>
// ...
void PushDeferDestroy(uint32_t frameIndex, std::function<void()>&& fn);
void FlushDeletions(uint32_t frameIndex);
private:
std::vector<std::function<void()>> m_DeletionQueues[kMaxFramesInFlight]; // 2 桶
```
`VulkanGraphicsContext::BeginFrame` 在 `vkWaitForFences` 后调 `VulkanDevice::Get().FlushDeletions(m_CurrentFrame)`。

**资源析构改造**（示例 `VulkanTexture2D::~`）：
```cpp
VulkanTexture2D::~VulkanTexture2D() {
    auto& dev = VulkanDevice::Get();
    VkSampler s = m_Sampler; VkImageView v = m_ImageView;
    VkImage im = m_Image; VmaAllocation a = m_Alloc;
    auto fn = [s, v, im, a]() {
        auto& d = VulkanDevice::Get();
        if (s) vkDestroySampler(d.Device, s, nullptr);
        if (v) vkDestroyImageView(d.Device, v, nullptr);
        if (im) vmaDestroyImage(d.Allocator, im, a);
    };
    uint32_t f = VulkanGraphicsContext::Get().GetCurrentFrame();
    dev.PushDeferDestroy(f, std::move(fn));  // 改为延迟
}
```
（Shutdown 路径：`vkDeviceWaitIdle` 后各桶 flush，或析构检测 `VulkanDevice` 是否已 Shutdown 直接销毁。）

**验证**：RenderDoc 逐帧捕获，加载大量纹理中途 `texture.reset()`，查 "resource destroyed while in use" 消失；GPU-assisted validation 无 use-after-free。

**风险**：改动所有资源析构（VulkanTexture2D/2DArray/Cube/VertexBuffer/IndexBuffer），跨多文件。建议在 A/C/D/F 验证通过后单独一个 PR 落地 + 跑渲染冒烟测试。

---

## 7. P0 性能设计（未落地）

### 瓶颈判断（先于优化）

`[ENGINE][TOOL]` 当前瓶颈：**CPU bound**（descriptor 逐 draw allocate+update）+ **sync bound**（资源上传期 `ImmediateSubmit` 全 stall）。GPU 侧无明显瓶颈（单 pass、1×MSAA、clear）。用 RenderDoc/AGI 量 `vkUpdateDescriptorSets` 与 `vkQueueWaitIdle` 占比确认。

### P0-1 descriptor set 复用

**问题**：`DrawIndexedCommon` 每 draw `vkAllocateDescriptorSets` + `vkUpdateDescriptorSets`（含每个 sampler 的 image info）。OpenGL 逐 draw uniform 语义直接映射到 Vulkan 的主开销。`[ENGINE]`

**设计**：按 `(shaderID, 已绑定纹理集合 hash)` 缓存 descriptor set。UBO 是 dynamic，每 draw 仅改 dynamic offset，set 不必变。

**对象链路**：
```
DrawIndexedCommon
  -> key = (shader.GetID(), HashBoundTextures())
  -> cache[frame][key] 命中? 复用 set（仅改 dynamic offset）
                       未命中: allocate + write（首次）
  -> ResetFrame: vkResetDescriptorPool + cache[frame].clear()
```

**代码骨架**（`VulkanRendererAPI`）：
```cpp
struct DescKey { uint64_t shaderID; uint64_t texturesHash; bool operator==(...) const; };
struct DescKeyHash { size_t operator()(const DescKey&) const noexcept; };
std::unordered_map<DescKey, VkDescriptorSet, DescKeyHash> m_DescCache[kMaxFramesInFlight];
```
`DrawIndexedCommon` 先查缓存；`ResetFrame` 清缓存（因 pool reset 后 set 失效）。

**中期**：sampler 改 bindless（`VK_EXT_descriptor_indexing`，一个大数组 + 着色器内索引），消除逐 draw sampler 写。需设备特性 `descriptorBindingPartiallyBound` + shader 改造，较大。

**验证**：RenderDoc 对比改前后每帧 `vkUpdateDescriptorSets` 调用数（应从 N 降到"组数"）。

### P0-2 ImmediateSubmit 批量化

**问题**：`ImmediateSubmit` 每次 `vkQueueWaitIdle`，stall 整条 graphics 队列 + 等当前 in-flight 帧。纹理/顶点批量加载时 N 次 stall。`[ENGINE]`

**设计（最小改动）**：批量化 API--调用方累积多个上传操作到一条 CB，一次 submit + fence。
```cpp
// 新增批量接口
void BeginUploadBatch();
void EndUploadBatch();  // 一次 submit + fence + wait
```
内部用同一 transient CB 录多份 copy，`EndUploadBatch` 统一 submit。

**中期**：专用 transfer 队列（若 `VK_QUEUE_TRANSFER_BIT` 独立）+ ownership transfer barrier + staging ring buffer + 按帧 fence 回收，彻底消除 stall。

**验证**：场景加载期 profiler 量 `vkQueueWaitIdle` 等待时长下降。

---

## 8. 验证路径汇总

> **编译验证 ✅ 通过**（2026-07-26）：A/C/D/F + Vulkan 后端编译链接通过，`dmge_tests` 5/5 通过。下表为**运行时** Validation 场景，需跑实际 Vulkan 渲染程序确认（待后续）。

启用 Validation Layer（Debug 即开，`DMGE_ENABLE_ASSERTS`）：

| 项 | 场景 | 预期 Validation |
|---|---|---|
| A | 拖拽窗口跨屏 / 换显示器 | 无 `VUID-vkCmdBeginRendering-None-06197` |
| C | 快速 resize 窗口边框 | 无 resize 后裁剪闪烁；无多余 OUT_OF_DATE |
| D | release 构建 + 构造 vkAllocate 失败 | 日志见 "Vulkan error ..." 而非静默崩溃 |
| F | shader 声明 2 sampler 只 Bind 1 | 无 "uninitialized binding" 报错 |
| B（落地后） | 加载纹理中途 reset() | 无 "resource destroyed while in use" |

工具：RenderDoc 逐帧捕获（查 descriptor set 是否每帧新建、texture 释放时 in-use 标记）；AGI 量 CPU 录制占比与 `vkQueueWaitIdle` 等待。

---

## 9. 准确性标签说明

`[SPEC]` Vulkan Spec 明确要求 · `[VUID]` Valid Usage ID · `[ENGINE]` 图形工程经验 · `[TOOL]` 可工具验证。

---

## 10. 阶段 0a 复核与 B 项落地（2026-10-03，agent/render-agent/vulkan-correctness）

> 背景：ROADMAP 阶段 0a 指派 render-agent 复核 A/C/D/F 并落地 B。对 worktree 代码逐项核对结果：
> **A/C/D/F 在 07-26 已落地且与本文件 §1-§4 描述一致**（代码核验：`OnSwapchainRecreate` 存在且销毁+m_Pipelines.clear() 保留 PipelineCache；`GraphicsContext::RequestResize` 虚函数 + WindowsWindow size callback 已接线；`VK_CHECK` release 分支已 `DMGE_LOG_ERROR`；dummy 1×1 纹理已建且 `WriteDescriptorSet` unbound 槽已绑 dummy）。本次仅对 A 做一处加固 + B 全量落地。E 项本波不做。

### 10.1 A 项加固 — RecreateSwapchain 前 device idle

- 问题：`VulkanGraphicsContext::BeginFrame` 中 `RecreateSwapchain()` 发生在 `vkWaitForFences` **之前**（与 §1"在 fence 之后调用"的设计描述不符）。旧 swapchain image 与按旧格式烧制 key 的缓存 pipeline 可能仍被 in-flight 提交引用，直接 destroy 属 use-while-in-flight。`[SPEC]`
- 修复：`RecreateSwapchain()` 开头 `vkDeviceWaitIdle`（resize 是低频事件，停顿可接受），再销毁重建。顺带使 `OnSwapchainRecreate` 的 pipeline 销毁严格安全。
- commit：`fix(engine): RecreateSwapchain 前等待 device idle，避免销毁 in-flight 引用的 swapchain/pipeline`

### 10.2 B 项落地 — per-frame deletion queue（§6 设计的实施版）

- 实施：按 §6 设计，但桶调度规则比设计稿更严格并附证明：
  - 2 桶对应 2 帧 in-flight 槽（`VulkanDeletionQueue.h`，纯逻辑、无 Vulkan 头依赖、可 headless 单测）。
  - **入桶规则** `DeletionBucketFor(frameStarted, currentFrame)`：录制中（frame started）入**当前槽**桶；帧间（frame not started）入**另一槽**桶（`(current+1)%2`）。设计稿的"入 currentFrame 桶"在帧间场景不安全：此时前后两帧（两槽）都可能 pending，当前槽桶的下次 flush 只等当前槽 fence，另一槽 pending 帧仍可引用该资源。
  - **flush 点**：`BeginFrame(slot b)` 在 `vkWaitForFences(b)` 之后 flush 桶 b。可证：两种入桶场景下，销毁闭包都在 `BeginFrame(f+2)`（f = 最后可能引用该资源的帧）执行，严格晚于所有引用帧 fence 信号（性质单测覆盖）。
  - `VulkanDevice`：`PushDeferDestroy/FlushDeletions/FlushAllDeletions` + `DeferDestroyTexture/DeferDestroyBuffer`（静态；`!IsInitialized()` 时安全 no-op，兜住 Shutdown 后静态析构）。`Shutdown` 在 `vmaDestroyAllocator` 前 FlushAll。互斥锁保护（资源析构可能来自非渲染线程）。
  - `VulkanGraphicsContext::BeginFrame` fence 等待后 flush 当前槽桶；`CurrentDeletionBucket()` 暴露入桶索引；`static_assert(kMaxFramesInFlight == kDeletionBucketCount)`。
- 改造资源：`VulkanTexture2D`（析构 + `Invalidate`）、`VulkanTexture2DArray`、`VulkanTextureCube`（析构 + `Invalidate`）、`VulkanVertexBuffer`、`VulkanIndexBuffer`（析构）→ defer + 清空句柄。
- 未纳入 defer：`VulkanShader::DestroyModules`（自带 `vkDeviceWaitIdle`，重但正确，记为性能待办——shader 卸载会全队列 stall）；`VulkanSwapchain`/pipeline 生命周期走 A 项路径（recreate 前已 device idle）。
- 回归测试：`engine/tests/test_deletion_queue.cpp`——调度性质（销毁点 × flush 时刻：flush 不早于 `BeginFrame(f+2)`、且一轮槽旋转内必 flush）+ 具体桶值 pin（防止退化成"总是当前桶"）。
- commit：`fix(engine): B 项——per-frame deletion queue，GPU 资源 fence 信号后销毁`

### 10.3 验证（2026-10-03）

- 构建环境：VS 18 Community (vcvars64) + Ninja + `DMGE_BUILD_SHARED=ON` + `DMGE_BUILD_TESTS=ON` + `DMGE_VULKAN_BACKEND=ON`。
- 结果见本节附注（构建日志由 render-agent 会话留存）：全量零 error、ctest 全绿、/W4 零新增警告；另以 `DMGE_VULKAN_BACKEND=OFF` 复测确认 OpenGL 默认路径未回归。
- 运行时 Validation 场景（§8 表）仍待人工跑 game/editor exe 确认，本波以编译+单测为准。

### 10.4 涉及文件

`VulkanDeletionQueue.h`（新）、`VulkanDevice.h/.cpp`、`VulkanGraphicsContext.h/.cpp`、`VulkanTexture2D.cpp`、`VulkanTexture2DArray.cpp`、`VulkanTextureCube.cpp`、`VulkanVertexBuffer.cpp`、`VulkanIndexBuffer.cpp`、`engine/tests/test_deletion_queue.cpp`（新）、`engine/tests/CMakeLists.txt`。