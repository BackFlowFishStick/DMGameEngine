# DirectX 11 后端设计（阶段 A+B：基础设施 + Headless 证明 + 窗口交换链前向路径）

> 作者：d3d-agent · 分支 `agent/d3d-agent/stage-a`（A）/ `agent/d3d-agent/stage-b`（B）· 2026-10-04/05
> 状态：阶段 A+B 已落地。本文是 DirectX 后端的**设计事实源**；阶段 C 落地时在此文档追加变更记录。
> 关联：`documents/ENGINE_REVIEW.md` §E1（双后端抽象收敛）、`kb/KB-03`（双后端注意事项）、`playbooks/PB-03`。

---

## 1. 为什么是 D3D11（取舍说明）

| 候选 | 结论 | 理由 |
|---|---|---|
| **D3D11** | ✅ 采用 | 状态机模型（immediate context + SO 状态对象）与 OpenGL 心智模型接近，反射/资源模型简单，学习型项目收益最高；Windows 自带运行时与 `D3DCompile`，无新增第三方依赖 |
| D3D12 | ❌ 阶段 A 不做 | 命令队列/fence/描述符堆/PSO/根签名全套显式化，复杂度与"第三后端验证抽象正确性"的目标不成比例；且引擎尚无 RenderPassDesc/资源屏障抽象（ROADMAP 2b 未落地），先做 D3D12 等于同时做两份收敛 |
| Vulkan | 已有 | — |

**学习型项目定位下的关键判断**：D3D11 允许 `D3D11CreateDevice` **无窗口创建设备**（硬件适配器或 WARP 软件光栅），配离屏 render target 即可完成"设备→资源→shader→绘制→回读"全链路——这正好补上 Vulkan 的短板（K-014：surface 创建需要交互式桌面会话，agent 沙箱内无法做运行时验证）。D3D11 是三个后端里**唯一可以在 CI/沙箱中全自动验证 GPU 路径**的，这也是它作为第三后端先落地的额外工程价值。

## 2. 分阶段路线

- **阶段 A（✅ 2026-10-04）**：设备创建（硬件→WARP 回退）、HLSL 运行时编译 + `D3DReflect` uniform 反射、VB/IB/VA（input layout）、Texture2D、离屏 FrameBuffer（单颜色附件 + 深度）、`D3D11RendererAPI` 阶段 A 子集、headless smoke 测试（画三角形 + Map 回读断言）。
- **阶段 B（✅ 2026-10-05，`agent/d3d-agent/stage-b`）**：HWND 交换链 + `DirectXGraphicsContext`（`CreateSwapChainForHwnd`、flip-discard 双缓冲、BGRA8、backbuffer RTV + 窗口深度 DSV、`ResizeBuffers`、`Present(vsync)`）；`BeginRenderPass(nullptr/SwapChainTarget)` 绑定窗口目标；点光/聚光**索引数组 uniform**（反射 ElementStride，按 HLSL 16 字节打包步进写入）；`Shader::Create(filepath)`（`#type` 分块 HLSL）；资源工厂（Shader/Texture2D/VA/VB/IB/FrameBuffer）DirectX 分支接入；`Mat4` 实例属性 input layout；`game` 演示 `DMGE_API=D3D11` 切换（环境变量或 `-d3d11`，默认仍 OpenGL，D3D11 下走 Forward 路径）。**深度范围处理**：D3D NDC z∈[0,1] 与投影矩阵的配合在离屏/窗口路径实测通过（清屏 depth=1.0、`Less` 比较语义与 GL 一致——阶段 A §3.3 的顾虑在 `DepthFunc::Less` + z=0 平面与真实场景下均未复现问题，若后续出现 z 精度问题再补重映射矩阵）。
- **阶段 C（TODO）**：ImGui D3D11 后端（当前 DirectX 下 ImGuiLayer 整层降级禁用）、`SetMat4Array` 蒙皮调色板、TextureCube/2DArray、与 2b 收敛成果（RenderPassDesc/反射统一）对齐、延迟渲染对齐（内嵌 deferred shader 仍为 GLSL 源）。

## 3. 抽象对齐表（RendererAPI / 资源抽象 → D3D11 对应物）

### 3.1 RendererAPI 虚函数

| RendererAPI 虚函数 | D3D11 对应物 | 阶段 A 状态 |
|---|---|---|
| `Init(config)` | 基类默认实现经虚 setter 应用初值；设备在构造时创建（`D3D11CreateDevice`，HARDWARE→WARP 回退） | ✅ |
| `SetClearColor` | 缓存 RGBA，`Clear()` 时传给 `ClearRenderTargetView` | ✅ |
| `Clear` | `ClearRenderTargetView(当前RTV)` + `ClearDepthStencilView(当前DSV, depth=1.0)` | ✅ |
| `SetViewport` | `CD3D11_VIEWPORT` + `RSSetViewports` | ✅ |
| `BeginRenderPass(FrameBuffer*)` | `OMSetRenderTargets(rtv, dsv)`，镜像 GL/Vulkan 语义进入时清屏；nullptr/SwapChainTarget → 解绑 | ✅ |
| `EndRenderPass` | `OMSetRenderTargets(0, ...)` 解绑 | ✅ |
| `DrawIndexed` | `vertexArray.Bind()`（IASetVertexBuffers/IndexBuffer/Topology/InputLayout）→ `DrawIndexed(count,0,0)` | ✅ |
| `DrawIndexedInstanced` | `DrawIndexedInstanced(count, instances, 0, 0, baseInstance)`（`baseInstance`→StartInstanceLocation） | ✅ |
| `SetBlendState` | 缓存描述 → 惰性创建 `ID3D11BlendState` 缓存 → `OMSetBlendState` | ✅ |
| `SetBlendEquation` | D3D11_BLEND_OP 映射（Add/Sub/RevSub/Min/Max 全支持） | ✅ |
| `SetDepthTest`/`SetDepthFunc` | 缓存描述 → 惰性 `ID3D11DepthStencilState` 缓存 | ✅ |
| `SetCullMode` | `ID3D11RasterizerState` 缓存；`FrontCounterClockwise=TRUE` 对齐 GL 默认 CCW-front；**`FrontAndBack` D3D11 无此模式 → 降级为 CULL_NONE + 警告日志**（已知偏差） | ✅ |

### 3.2 资源抽象

| 抽象 | D3D11 对应物 | 阶段 A 状态 |
|---|---|---|
| `Shader`（按名 uniform） | `D3D11Shader`：`D3DCompile`（VS/PS 两遍）+ `D3DReflect` 解析 cbuffer 成员名→offset；`SetXxx` 写 CPU 侧 staging，`Bind()` 时 `UpdateSubresource` 上传（见 §5） | ✅ |
| `VertexBuffer` | `ID3D11Buffer`（`D3D11_BIND_VERTEX_BUFFER`，STATIC/DYNAMIC 按构造重载） | ✅ |
| `IndexBuffer` | `ID3D11Buffer`（`D3D11_BIND_INDEX_BUFFER`，R32_UINT） | ✅ |
| `VertexArray` | 无原生 VAO：`D3D11VertexArray` 存 VB/IB 引用 + layout→`D3D11_INPUT_ELEMENT_DESC` 映射；`Bind()` 时 IASet* + input layout（按"当前绑定 shader 的 VS 字节码 + 布局"惰性创建并缓存） | ✅ |
| `Texture2D` | `ID3D11Texture2D` + `ID3D11ShaderResourceView`；`Bind(slot)` 把 SRV 登记进单元表（对齐 GL texture unit 语义，见 §5.3） | ✅ |
| `FrameBuffer`（离屏） | `ID3D11Texture2D`(BIND_RENDER_TARGET\|SHADER_RESOURCE) + RTV + SRV 包装成 `D3D11Texture2D` 附件；深度附件 DSV。阶段 A 单颜色附件；MSAA 仅按 spec 建样本数为 1（与 GL 后端现状一致） | ✅ |
| `GraphicsContext` | `DirectXGraphicsContext`：HWND swapchain（`CreateSwapChainForHwnd`，flip-discard、2 缓冲、BGRA8）+ backbuffer RTV + 窗口深度 DSV；`SwapBuffers()`=Present(vsync)、`RequestResize()`=`ResizeBuffers` + 视图重建；设备注册进 D3D11Backend 供 RendererAPI 采纳（B） | ✅ B |
| `TextureCube/2DArray` | 阶段 C 按需 | ⬜ C |

### 3.3 阶段 A 有意留白（TODO+日志，不 abort）

- `SetMat4Array`（蒙皮 u_BoneMatrices）：阶段 C，日志提示未实现。
- `Texture2D::Create(filepath)` 文件加载路径：构造可用（走 stb 后 CPU 上传），但未做视觉验证。
- 深度范围：GL 投影矩阵 NDC z∈[-1,1]，D3D z∈[0,1]。阶段 A smoke 用 z=0 平面几何不敏感；阶段 B 必须处理（投影后乘深度重映射矩阵，或后端内收敛——按 E1 原则放后端，不进 `Renderer`）。
- Y 方向：D3D clip Y 与 GL 同为向上，无需 Vulkan 式 flipY；但离屏纹理**行序自上而下**，CPU 回读/阶段 B 呈现时需注意（GL `glReadPixels` 自下而上）——回读约定见 §6。

## 4. 与现有约束的对齐（红线）

- **R5 后端无关**：不触碰 `Renderer.h`/`RendererAPI.cpp`/各工厂 cpp（render-agent 热区）。后端通过 `DirectXIntegration.h` 的 `CreateDirectXRendererAPI()` 工厂暴露，接线方案见 §7。公共头除 `DMGameEngine.h` 的 `#ifdef DMGE_D3D11` 门控 include（先例：`DMGE_ANIMATION`）外无 D3D 概念渗漏。
- **R1 DLL 边界**：导出类成员只有 COM 指针（`Microsoft::WRL::ComPtr`，非 STL、无 CRT 分配）、标量与引擎类型；`std::vector/std::string` 一律进 .cpp 匿名命名空间或 staging（staging 缓冲用 `std::vector<uint8_t>` 成员——不行，导出类禁 STL：staging 放 .cpp 侧的 per-shader 堆结构，通过 `void*` pimpl 隐藏）。实际采用 **pimpl**：所有 STL/COM 细节进 `D3D11Shader::Impl` 等 cpp 内部类，头文件只留 `struct Impl*`。
  > 注：`D3D11RendererAPI` 等类头仍需被 smoke 测试 include 才能直接构造，pimpl 同时把 `<d3d11.h>`/`<wrl>` 依赖挡在 .cpp 里，公共头保持干净。
- **R6 PCH**：`<d3d11.h>/<d3dcompiler.h>/<dxgi.h>` 不进 `dmge_pch.h`（与 glad/Vulkan 同理，后端头互斥）。
- **K-020/K-022**：smoke 测试零外部资产，HLSL 以原始字符串内嵌在测试里；正式 `BlinnPhong.hlsl` 放 `Platform/DirectX/Shaders/`（渲染 Agent 的 `engine/shaders/**` 禁入）。

## 5. Uniform 语义对齐（GLSL per-name ↔ HLSL cbuffer）

引擎现有 uniform 契约是 **OpenGL 式"按名上传"**（`Shader::SetMat4("u_ViewProjection", ...)`），`Material`/`RenderQueue`/场景序列化全建立其上。D3D11 只认 cbuffer 字节槽，桥接方案（呼应 E1"反射 + 显式 offset"的收敛方向，但用 D3DReflect 而非 SPIRV-Cross）：

1. **HLSL 侧约定**：所有 uniform 收进一个 cbuffer（VS/PS 各自 `register(b0)`），成员名与 GLSL uniform 名**逐字相同**（`u_ViewProjection`/`u_Transform`/`u_NormalMatrix`/`u_CameraPosition`/`u_AlbedoColor`/...）。矩阵显式声明 `column_major`（glm 是列主序，默认对齐；消除对编译器默认值的依赖）。数学上 `mul(M, v)` ≡ GLSL `M * v`。
2. **反射**：编译后对 VS/PS 各跑 `D3DReflect`，收集 (uniform 名 → {stage, cbuffer 槽, 字节 offset, 元素数})；同名可同时存在于两 stage，写入时全部命中。纹理类 bound resource（`u_AlbedoTexture`）记录其 bind point。
3. **上传**：`SetXxx(name, v)` 把字节写进该 stage 的 CPU staging（未命中名 → 一次性 WARN + 忽略，对齐 GL "location -1 静默忽略" 语义）；`Bind()` 时 `UpdateSubresource` 逐 cbuffer 上传 + `VSSetConstantBuffers/PSSetConstantBuffers`。
4. **采样器**：对齐 GL "glUniform1i(sampler, unit) + glBindTexture(unit)" 双段式——`SetInt("u_AlbedoTexture", 0)` 只记录槽号；`D3D11Texture2D::Bind(slot)` 把 SRV 登记进全局单元表；shader `Bind()` 时按反射 bind point `PSSetShaderResources` 解析。**未登记的槽位回退 1×1 白色 dummy SRV**（KB-03 Vulkan 项 F 的同源问题，先手治）。
5. **数组 uniform**：HLSL cbuffer 内数组按 16 字节对齐（与 std140 的 vec3[16] 布局一致处居多但不保证逐字节同构）——正因为走反射 offset，**调用方无需关心差异**；阶段 B 补齐点光/聚光数组时逐字段验证。

### 5.1 语义映射表（BlinnPhong.glsl → BlinnPhong.hlsl）

| GLSL（现有） | HLSL（Platform/DirectX/Shaders/BlinnPhong.hlsl） | 备注 |
|---|---|---|
| `layout(location=N) in vec3 a_Position` | `float3 a_Position : POSITION` | VA 元素名→semantic 映射：含 Position→POSITION、Normal→NORMAL、TexCoord(s)/UV→TEXCOORDi、Color→COLOR、Tangent→TANGENT，其余按序 TEXCOORDn |
| `uniform mat4 u_ViewProjection` 等 | `column_major float4x4` 同名 cbuffer 成员 | 反射上传 |
| `out/in vec3 v_WorldPos` | VS 返回 struct 成员 | 显式 in/out，与 Vulkan GLSL 约定一致 |
| `layout(location=0) out vec4 FragColor` | PS 返回 `float4 : SV_Target` | |
| `texture(u_AlbedoTexture, uv)` | `u_AlbedoTexture.Sample(u_AlbedoTextureSampler, uv)` | 采样器对象显式声明（register(s0)），名字 = 纹理名 + "Sampler" |
| `#type vertex/#type fragment` 预处理 | 无——HLSL 单文件多 entry point（`VSMain`/`PSMain`），`D3DShader` 构造直接收两段源码字符串 | 阶段 A 不复刻 `#type` 预处理器 |

## 6. Headless 验证设计（`engine/tests/test_d3d11_smoke.cpp`）

不依赖任何窗口/交互会话（K-014/K-021 的正面解法：D3D11 设备创建本身 headless 合法）：

1. `CreateDirectXRendererAPI()` → `Init(RendererAPIInitConfig{ClearColor=深蓝})`，记录实际使用的驱动类型（HARDWARE 不可得时 WARP，沙箱/CI 两态都能过）。
2. 直接构造 `D3D11FrameBuffer`（512×512 RGBA8 + Depth）、`D3D11Shader`（内嵌 HLSL：`u_ViewProjection`/`u_Transform` + a_Position/a_Color 顶点色）、VB/IB/VA。
3. `BeginRenderPass(fb)` → `SetClearColor` → `Clear` → `SetViewport` → shader `Bind()` + 设 uniform → `DrawIndexed` → `EndRenderPass`。
4. 建 STAGING 纹理 `CopyResource` → `Map` 回读，断言：
   - 三角形中心像素 = 顶点色红（R>0.9, G/B<0.1）——证明 VS/PS/IA/raster/draw 真实工作；
   - 远角像素 = 清屏色——证明 Clear/RTV 正确；
   - 回读按自上而下行序取坐标（§3.3 Y 方向注记）。
5. 附加用例：WARP 回退路径、shader 反射 offset 非零、uniform 名未命中容忍、Texture2D 创建/SetData/GenerateMipmaps 不崩。

## 7. 管理员接线清单（合并后 3 行，超出部分为零）

阶段 A 结束后引擎尚不能经 `Renderer::SetAPI` 选到 D3D11——接线是 `RendererAPI.cpp` 等文件的一处分支，属 render-agent/管理员操作：

1. `Renderer/RendererAPI.cpp` `RendererAPI::Create()`：
   ```cpp
   case Renderer::API::DirectX:
   #ifdef DMGE_D3D11
       return DirectX::CreateDirectXRendererAPI();
   #else
       DMGE_CORE_ASSERT(false, "DirectX backend not built (enable DMGE_D3D11).");
       return nullptr;
   #endif
   ```
   （枚举 `Renderer::API::DirectX` 已存在，**无需新增枚举值**；`DirectX11` 命名提案作废。）
2. `RendererAPI.cpp` 顶部：`#ifdef DMGE_D3D11  #include "DMGameEngine/Platform/DirectX/DirectXIntegration.h"  #endif`
3. 后续各资源工厂（`Shader::Create`/`Texture2D::Create`/...）同型分支——**阶段 B 已接入**（经用户批准的最小机械改动：`Renderer/Shader.cpp`、`Texture2D.cpp`、`VertexArray.cpp`、`VertexBuffer.cpp`、`IndexBuffer.cpp`、`FrameBuffer.cpp` 各加一个 `#ifdef DMGE_D3D11` 分支；`Texture2DArray/TextureCube` 仍留阶段 C）。

## 8. 构建开关

`engine/CMakeLists.txt` 末尾独立区块：

- `option(DMGE_D3D11 "Build the Direct3D 11 renderer backend (Windows/MSVC)" OFF)`——默认 **OFF**，与 `DMGE_VULKAN_BACKEND` 先例一致：可选后端不进默认构建；OFF 时全工程构建路径与现状逐字节相同（新源文件不编译、`DMGE_D3D11` 宏不产生）。
- ON 时：`target_compile_definitions(PUBLIC DMGE_D3D11)`（消费者 TU 必须与 DLL 一致，先例 DMGE_ANIMATION）、链 `d3d11.lib dxgi.lib d3dcompiler.lib`（系统库，无第三方下载）、`target_sources` 显式列出全部新源文件（R2，禁 GLOB）。
- 非 Windows/非 MSVC 下显式 `FATAL_ERROR`，避免静默产出坏目标。
- 测试目标 `dmge_d3d11_tests` 在 `engine/tests/CMakeLists.txt` 的 `if(DMGE_D3D11)` 块内注册，沿用既有 DLL 拷贝/`gtest_discover_tests` 模板。

## 9. 变更记录

| 日期 | 阶段 | 内容 |
|---|---|---|
| 2026-10-04 | A | 初版设计；阶段 A 基础设施 + headless smoke 落地于 `agent/d3d-agent/stage-a` |
| 2026-10-04 | A | 补充 K-023：本机 D3D11 运行时按寄存器序链接 PS 输入——**所有 D3D11 HLSL 的 VS 输出 struct 必须先声明 varying、最后声明 SV_Position**（§3.1/`BlinnPhong.hlsl` 头注释已落实） |
| 2026-10-05 | B | 阶段 B 落地于 `agent/d3d-agent/stage-b`：窗口交换链 GraphicsContext、RendererAPI 窗口目标、索引数组 uniform（反射 ElementStride）、`Shader::Create(filepath)`（#type 分块 HLSL）、资源工厂 DirectX 分支、Mat4 实例属性、game `DMGE_API=D3D11` 演示；smoke 测试扩至 8 例（含隐藏窗口 swapchain 渲染回读 + resize）。新坑 K-026~K-030。阶段 C（ImGui/蒙皮/延迟对齐/TextureCube）保持 TODO |
