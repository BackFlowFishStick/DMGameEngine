# 可配置延迟渲染设计（ROADMAP 3e 阶段 1）

> 状态：**已实现（最小可行版）**，2026-10-04，agent/render-agent/deferred。
> 范围：OpenGL 与 Vulkan 双后端的可配置 Forward/Deferred 渲染路径，默认仍为 Forward。
> 与 2b 收敛的关系：本波是**最小可行版**，刻意不引入 RenderPassDesc / SPIR-V 反射 / 资源屏障抽象；
> 统一收敛仍归 2b。本文在"与 2b 的关系"一节列出本波欠下的债。

---

## 1. 目标与非目标

### 目标
1. `Renderer` 增加 `RenderPath`（Forward / Deferred），默认 Forward，运行时可切换。
2. G-buffer MRT pass（双后端）+ 全屏 quad 光照 pass（Blinn-Phong，光源常量沿用 `Light.h` 的 MAX_*）。
3. 深度复用：光照 pass 从 G-buffer 的 depth attachment 反投影世界坐标，不新增位置 RT。
4. editor viewport（离屏 FB）与 game（swapchain）两条 RTT 路径在 Deferred 下保持正确。
5. 不破坏：现有前向路径、实例化、K-009 descriptor 缓存、§12 PipelineCache、editor/game 两个 exe。

### 非目标（阶段 1 明确不做）
- 透明物体专用 G-buffer / 深度剥离（阶段 1 无透明标记，全部走不透明，见 §6）。
- 阴影 / PBR / IBL（3e 其余内容）。
- RenderPassDesc 统一、SPIR-V 反射（2b）。
- 光源数据 UBO 化（沿用 per-name uniform 数组上传，见 §5）。
- DirectX 后端（d3d-agent 并行开发中，公共接口未增加任何后端专属概念）。

---

## 2. MRT 现状核验（动手前调研结论）

| 后端 | MRT 差多少 | 结论 |
|---|---|---|
| **OpenGL** | `OpenGLFrameBuffer::Invalidate()` 已实现多颜色附件：逐附件 `glFramebufferTexture2D` + 附件数 >1 时 `glDrawBuffers`（=0 时 `glDrawBuffer(GL_NONE)`）。`BeginRenderPass` 的 `glClear(GL_COLOR\|GL_DEPTH)` 天然清空全部 draw buffer。 | **差 0，已支持**。"pipeline blend 假设单颜色附件"在 GL 侧不成立——`glBlendFunc` 是全局状态，对所有 draw buffer 一致生效；G-buffer pass 要求 blend 关闭，由延迟路径显式 `SetBlendState(false)` 保证。 |
| **Vulkan** | 三处单附件硬编码：① `BeginRenderPass` 只构造 1 个 `VkRenderingAttachmentInfo`、`colorAttachmentCount=1`；② `GetOrCreatePipeline` 的 `VkPipelineColorBlendStateCreateInfo::attachmentCount=1`；③ `VkPipelineRenderingCreateInfo` 同样为 1 且 PipelineKey 只存一个 `colorFormat`。`EndRenderPass` 的 SHADER_READ_ONLY 转移已循环全部颜色附件（含深度）。 | **差 3 处**，本波全部补齐：多附件 rendering info、按附件数复制 blend attachment state、PipelineKey 增加附件数 + 全部颜色格式。 |

注意：Vulkan pipeline 的 colorAttachmentCount 必须与 `vkCmdBeginRendering` 的附件数一致，否则 validation 层报 attachment count mismatch——所以 PipelineKey 必须把附件数纳入缓存键，同一个 shader 在前向（1 附件）与延迟（2 附件）pass 下会得到两个 pipeline。

---

## 3. G-buffer 布局与格式表

常量表实现在 `Renderer/DeferredRendering.h`（`GBufferLayout`），是纯逻辑、可 headless 单测的单一事实源：

| RT | 格式（TextureFormat） | 内容 | 选用理由 |
|---|---|---|---|
| RT0 | `RGBA8` | rgb = albedo（线性），a = specular strength (0-1) | 材质高光参数打包进 alpha，省一个 RT；specular strength 本身就是 0-1 标量，8bit 足够 |
| RT1 | `RGBA16F` | xyz = 世界空间法线（单位向量），w = shininess | 法线分量有负值，UNORM 会截断，必须浮点格式；shininess 范围可达数百，16F 无压力 |
| Depth | `Depth`（GL 深度纹理 / Vulkan D32 或设备支持的深度格式） | 深度预存 + 光照 pass 反投影 | 不为世界坐标单独开 RT；深度纹理两后端都已是可采样 Texture2D |

- MRT 数量：**2 个颜色附件 + 1 个深度附件**。`GBufferLayout::kColorAttachmentCount = 2`。
- Vulkan 管线侧的 `kMaxColorAttachments = 4`（预留余量，本波最多用到 2）。
- 采样器约定（光照 pass）：slot 0 = RT0，slot 1 = RT1，slot 2 = depth。

### 为什么不用 glClipControl / 投影矩阵改造统一深度
GL 默认 NDC z ∈ [-1,1]、窗口 z = (ndc+1)/2；Vulkan NDC z ∈ [0,1]、窗口 z = ndc。为避免改共享投影矩阵（风险外溢到相机系统），反投影公式统一为：

```glsl
vec3 ndc;
ndc.xy = uv * 2.0 - 1.0;                  // 两后端一致（各自纹理约定下成立）
float depth = texture(u_GDepth, uv).r;
ndc.z    = mix(u_NdcZMin, 1.0, depth);    // GL: u_NdcZMin=-1, Vulkan: 0
vec4 world = u_InverseViewProjection * vec4(ndc, 1.0);
vec3 worldPos = world.xyz / world.w;
```

`u_InverseViewProjection` 是 G-buffer pass 实际使用的 ViewProjection 的逆（含 Vulkan 的 flipY，自洽）；`u_NdcZMin` 由 Renderer 按 `GetAPI()` 设置一个 float——这是已知的 2b 债务（与 BeginScene 的 Y 翻转同类，见 §7）。

---

## 4. 帧编排（Deferred 下一帧的完整流程）

```
Application: ClearFrame()                      // GL 清默认 FB；Vulkan BeginFrame 开 swapchain pass（loadOp=CLEAR）
Layer: Renderer::BeginScene(camera, target)    // Deferred 分支：
    - 记录真实输出 target（nullptr = swapchain）
    - EnsureDeferredResources()（惰性创建 G-buffer FB + 4 个 shader + 全屏 quad VA；尺寸随 target，变化时 Resize）
    - RenderCommand::BeginRenderPass(GBuffer)  // GL 绑 FBO+清屏；Vulkan 结束 swapchain pass、转布局、开 2 附件 dynamic rendering（loadOp=CLEAR）
    - s_SceneData / s_LightData / s_Queue 重置（与前向一致）
Scene::OnRender()                              // LightSystem 填光源数据；MeshRenderSystem/游戏代码照常 Submit——不感知路径
Renderer::EndScene()                           // Deferred 分支：
    1) 显式管线状态：DepthTest(true)/Less、Blend(false)
    2) RenderQueue::FlushDeferred(...)         // 全部排队 draw 改写为 G-buffer shader 输出 MRT
    3) RenderCommand::EndRenderPass()          // 结束 G-buffer pass（Vulkan：全部附件→SHADER_READ_ONLY，含深度）
    4) RenderCommand::BeginRenderPass(target)  // 真实输出（GL 绑 FBO+清 / 默认 FB no-op；Vulkan 重开对应 pass）
    5) 光照 pass：绑定 G-buffer 纹理 slot 0/1/2，上传 u_InverseViewProjection / u_NdcZMin /
       u_CameraPosition / u_SkyColor / 全部光源 uniform（复用 UploadSceneLighting），
       深度测试关闭，DrawIndexed(全屏三角)
    6) RenderCommand::EndRenderPass()          // 恢复与前向一致的 pass 语义（Vulkan：editor FB→SHADER_READ_ONLY，swapchain pass 重开 loadOp=LOAD）
ImGui / 后续层 / SwapBuffers                    // 与前向完全一致
```

设计要点：
- **MeshRenderSystem / LightSystem / 场景代码零改动**。分流收口在 `Renderer::EndScene → Flush`：前向走 `RenderQueue::Flush`，延迟走 `RenderQueue::FlushDeferred`。这是"RenderQueue 侧分流"与"Renderer 侧编排"的组合，游戏与编辑器无需感知路径。
- **切换立即生效于下一帧的 BeginScene**；帧内不响应切换。切换不做 GPU 资源销毁重建——G-buffer 资源惰性常驻（内存 ≈ 1.5×目标分辨率 的 RGBA8+RGBA16F+Depth），`SetAPI` 与 `Shutdown` 时释放（跨 API 的 shader/FBO 不可复用）。
- **Resize**：每次 deferred BeginScene 比对 target 尺寸与 G-buffer FB 尺寸，不同则 `Resize()`（复用 FrameBuffer 重建路径）。swapchain 目标的尺寸取 `Renderer::OnWindowResize` 记录的窗口尺寸。

## 5. 光照数据上传（沿用 per-name uniform）

光照 pass 的 shader（DeferredLighting）声明与前向 Blinn-Phong **完全同名**的光源 uniform（`u_DirectionalLight_*`、`u_PointLights_*[16]`、`u_SpotLights_*[8]`、`u_Ambient*`、`u_PointLightCount`、`u_SpotLightCount`）。`UploadSceneLighting` 从 RenderQueue.cpp 的匿名命名空间提为 `DeferredRendering.h` 的 inline 函数，两处共用——不引入 UBO 抽象（任务约束），Vulkan 侧继续走合成 UBO + dynamic offset 的既有机制（K-009 缓存语义不变，光照 uniform 进 UBO 块，纹理进 binding 1..N）。

## 6. 透明物体策略（阶段 1）

当前引擎没有透明标记（无 per-material blend 标志；blend 状态由应用手动开闭）。阶段 1 的处理：
- **全部物体按不透明走 G-buffer**：G-buffer pass 强制 `SetBlendState(false)`（MRT + blend 混写没有定义良好的语义），光照 pass 全屏覆盖。
- 限制注明：若应用在帧内手动开启了 blend，延迟路径会忽略它（等价于把混合物体当不透明画）。真正的透明前向回退（"光照 pass 后按前向补画"）设计上预留了位置——在步骤 5 之后按 RenderPath 分流补一段前向 flush 即可——但需要先有透明标记数据（属场景 Agent 的 Component 域），阶段 1 不做。

## 7. 与 2b 收敛的关系（本波欠债清单）

| 债 | 说明 | 归还时机 |
|---|---|---|
| `u_NdcZMin` per-API 常量 | 深度反投影的深度范围差异以 uniform 注入，与 BeginScene Y 翻转同属 E1 渗漏 | 2b RenderPassDesc / 投影统一 |
| 延迟 shader 内嵌于 DeferredRendering.h | 引擎内部 shader 用 `Shader::Create(name, src...)` 内嵌源码（DLL 无 CWD 依赖）；`engine/shaders/*.glsl` 为同内容的规范副本 | 2b 资产管线统一 shader 路径 |
| 光照 pass 直接编排 RenderCommand | 未走 RenderPassDesc 抽象 | 2b |
| 硬编码 Blinn-Phong 材质 uniform 名 | FlushDeferred 从 Material 按知名名取值（u_AlbedoColor 等），非反射 | 2b SPIR-V 反射 |
| 管线 blend 状态按附件复制 | Vulkan key 里存全部分格式；blend state 仍是全局单份复制到各附件（与 GL 语义一致） | 2b per-target blend |

## 8. 测试与验证

- 纯逻辑（headless 单测，`engine/tests/test_deferred.cpp`）：G-buffer 格式表不变量、路径状态机（默认 Forward / 往返切换 / 帧内不生效语义）、G-buffer 重建判定（尺寸变化才重建、非法尺寸拒绝）、全屏三角覆盖不变量、DeferredShaderSet 默认值。
- 运行时（人工，见报告"人工验证步骤"）：前向/延迟视觉一致性对比、editor viewport、Vulkan Validation Layer。
- Vulkan 无 surface 限制（K-014）：agent 会话内无法建 swapchain，pass 结构以"纯逻辑单测 + 双后端全量构建 + ctest"兜底，运行时验证留给人工。
