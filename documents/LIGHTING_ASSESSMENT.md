# DMGameEngine - 光照系统评估

> 创建日期：2026-08-02
> 评估基准：ENGINE_SUMMARY.md + ENGINE_ROADMAP.md + 实际源码核验（全量阅读 Renderer/Scene/Asset/Platform 代码）
> 目的：评估当前渲染系统是否需要添加光照系统，明确现有基础、缺失项、前置依赖与分阶段建议

---

## 1. 当前渲染系统全景

### 1.1 架构分层

```
应用层 (DefaultSceneLayer)
    |  BeginScene(camera) / EndScene()
    v
高层渲染 (Renderer)              <- 场景提交：缓存 ViewProjection + 入队 + Flush
    |  Submit(material, va, transform)
    v
延迟队列 (RenderQueue)           <- 按 material/shader 排序分组，每 group 绑定一次
    |  Flush(viewProjection)
    v
命令层 (RenderCommand)           <- 持有活跃 RendererAPI 后端，委托调用
    v
后端抽象 (RendererAPI)           <- OpenGL / Vulkan 实现：DrawIndexed / 管线状态 / RenderPass
```

### 1.2 渲染一帧的完整流程

```
Application::Run()
  |- Renderer::ClearFrame()                     // 清屏（颜色+深度）
  |- Layer::OnUpdate(ts)
  |    |- CameraController::OnUpdate()          // 更新相机
  |    \_ Scene::OnUpdate()                     // TransformSystem 计算 WorldMatrix
  |- Layer::OnRender()
  |    |- Renderer::BeginScene(camera)          // 缓存 ViewProjection + 开始 RenderPass
  |    |    \_ RenderCommand::BeginRenderPass(target)
  |    |- Scene::OnRender()
  |    |    \_ MeshRenderSystem::OnRender()     // 遍历 Transform+Mesh，Submit(material, va, worldMatrix)
  |    |- Renderer::EndScene()
  |    |    \_ RenderQueue::Flush(vp)           // 排序分组 -> 绑定 shader -> 上传 uniform -> DrawIndexed
  |    |    \_ RenderCommand::EndRenderPass()
```

### 1.3 关键数据流

| 数据 | 来源 | 上传时机 | 当前 Uniform 名 |
|------|------|----------|----------------|
| ViewProjection 矩阵 | Camera::GetViewProjection() | 每个材质组首次绑定 | u_ViewProjection |
| World 变换矩阵 | TransformComponent::WorldMatrix | 每个 draw | u_Transform |
| 材质参数 | Material::m_Uniforms (variant) | 每个材质组绑定 | 按名设置 |
| 顶点数据 | Mesh::Vertices (interleaved) | 首次 GetVertexArray() | a_Position / a_Normal / a_TexCoords / a_Tangent |

---

## 2. 光照系统前置条件评估

### 2.1 已具备的基础设施

| 能力 | 现状 | 对光照的意义 |
|------|------|-------------|
| 顶点法线 | assimp 导入器已提取 a_Normal (Float3)，含 TransformDirection（逆转置法线矩阵变换） | 光照计算的核心输入已就绪--无需改导入管线 |
| 顶点切线 | assimp 已提取 a_Tangent (Float3)，aiProcess_CalcTangentSpace 已启用 | 法线贴图的前提数据已就绪 |
| 顶点 UV | assimp 已提取 a_TexCoords (Float2) | 可采样 albedo/normal/specular 贴图 |
| Texture::Bind(slot) | Texture2D/Cube/2DArray 均支持按 slot 绑定 | 着色器可采样多张纹理 |
| Material uniform 存储 | variant<int,float,vec2,vec3,vec4,mat4,int[]> | 可存储光照参数（颜色、强度、衰减） |
| FrameBuffer MRT | 支持多颜色附件 + depth-only | 延迟渲染 G-Buffer + 阴影贴图的技术基础 |
| 多 pass 渲染 | BeginScene(cam, target) + BeginRenderPass/EndRenderPass | 阴影 pass -> 主 pass 的多 pass 架构可用 |
| ECS 扩展性 | Scene/Entity/Component/System 框架完整 | 添加 LightComponent + LightSystem 成本低 |
| RenderQueue 分组 | 按 material/shader 排序，每组绑定一次 | 共享同一 lit shader 的对象只需上传一次光照数据 |
| TextureCube | 已实现，支持 6 面 cubemap | 天空盒 / 环境光 IBL 的基础 |

### 2.2 光照系统缺失项

| 缺失项 | 影响程度 | 说明 |
|--------|---------|------|
| 光照组件 | 致命 | 无 DirectionalLight / PointLight / SpotLight 组件--场景中无法描述光源 |
| 光照系统 | 致命 | 无 LightSystem 收集活跃光源并上传到着色器 |
| 光照着色器 | 致命 | 唯一着色器 texture.glsl 仅做纹理采样，无任何光照计算（连环境光都没有） |
| 场景光照数据上传 | 致命 | RenderQueue::Flush 只上传 u_ViewProjection + u_Transform，无光照 uniform 通道 |
| 相机世界坐标 | 重要 | SceneData 只缓存 ViewProjection 矩阵，无相机位置--镜面高光无法计算 |
| 法线矩阵 | 重要 | 只上传 u_Transform (Model 矩阵)，未上传 mat3(transpose(inverse(u_Transform)))--非均匀缩放时法线变换不正确 |
| Material 纹理绑定 | 重要 | Material 只存标量/向量 uniform，不持有 Ref<Texture> 引用--无法绑定 albedo/normal/specular 贴图 |
| UBO/SSBO 抽象 | 中等 | 当前 uniform 上传靠逐 draw glUniform* 调用；多光源数组用此方式效率低，但不阻塞基础光照 |
| 深度-only FrameBuffer 断言 | 中等 | 路线图已记录：离屏路径断言 GetColorAttachmentCount()>0，阴影贴图需放开 |
| 统一 RenderPassDesc | 中等 | 双后端抽象仍有 OpenGL-first 渗漏 (E1)，光照相关多 pass 代码可能需在 2b 后返工 |

---

## 3. 光照系统需求评估：是否需要添加？

### 3.1 判断依据

| 维度 | 评估 |
|------|------|
| 当前渲染效果 | 纯 unlit（无光照）--只能输出纹理原色或纯色，无明暗、无阴影、无法表现 3D 几何体的体积感 |
| 引擎定位 | 路线图目标是「可用游戏引擎」--unlit 渲染对 3D 游戏不可用 |
| 前置条件 | 顶点法线/切线/UV 已就绪（assimp 导入完成）；FrameBuffer MRT + 多 pass 已就绪--硬件数据层已准备好，软件层完全空白 |
| 投入产出比 | 基础前向光照（Blinn-Phong + 少量动态光源）工作量中等，但视觉提升巨大：从平面贴图到有立体感的 3D 场景 |
| 架构影响 | 需修改 RenderQueue（上传光照数据）、Material（绑定纹理）、Renderer（缓存相机位置）--改动范围可控，不破坏现有架构 |
| 路线图冲突 | 路线图将「高级渲染」放在 3e（远期），但 3e 指的是阴影/PBR/IBL/延迟渲染--基础前向光照不等于高级渲染，是 3e 的前置 |

### 3.2 结论

**需要添加光照系统。** 当前引擎处于「渲染管线完整但着色器无光照」的状态--顶点数据已包含法线，但从未被使用。添加基础前向光照是让引擎从「技术演示」变为「可视化 3D 引擎」的关键一步。

但不应一步到位实现 PBR/阴影/延迟渲染（这些属于路线图 3e，依赖 2b 后端收敛）。应分阶段推进。

---

## 4. 分阶段实施建议

### 阶段 A：前置基础设施（低风险，非破坏性）

**目标**：补齐光照计算所需的最小数据通道，不引入任何光照逻辑。

| 任务 | 改动文件 | 说明 |
|------|---------|------|
| 缓存相机世界坐标 | Renderer.h/cpp (SceneData 增加 CameraPosition) | BeginScene(camera) 时从 Camera 获取 |
| 上传法线矩阵 | RenderQueue.cpp (Flush 中每个 draw 额外上传 u_NormalMatrix) | mat3(transpose(inverse(mat3(transform)))) |
| 上传相机位置 | RenderQueue.cpp (每组首次绑定时上传 u_CameraPosition) | 镜面高光所需 |
| Material 纹理槽位 | Material.h/cpp (增加纹理引用 map + Bind() 中绑定纹理) | 让材质可持有 albedo/normal/specular 贴图引用 |
| .mat 格式扩展 | AssetLoader.cpp + UniformSerializer.h | 支持 "textures": {"u_Albedo": "<path>", ...} |
| 添加到公共 API | DMGameEngine.h + CMakeLists.txt | 新头文件注册 |

**预估**：1-2 天。纯增量，现有 unlit 着色器不受影响（不使用的 uniform 被驱动忽略）。

### 阶段 B：基础前向光照（核心价值）

**目标**：实现 Blinn-Phong 前向光照，支持 1 个方向光 + N 个点光源 + 1 个聚光灯。

| 任务 | 新增文件 | 说明 |
|------|---------|------|
| 光照组件 | Scene/Components/LightComponent.h | enum LightType { Directional, Point, Spot } + 颜色/强度/衰减/角度参数 |
| 光照系统 | Scene/Systems/LightSystem.h | 遍历 LightComponent + TransformComponent，收集到 SceneLightData 结构 |
| 光照数据结构 | Renderer/Light.h | struct DirectionalLight { vec3 direction; vec3 color; float intensity; } 等 + SceneLightData 容器 |
| 着色器 | assets/shaders/BlinnPhong.glsl | 顶点：传递世界坐标法线/位置；片段：环境光 + 漫反射 + 镜面反射 |
| RenderQueue 光照上传 | RenderQueue.cpp | Flush 时上传 u_LightCount / u_Lights[i].* / u_DirectionalLight.* |
| 组件注册 | Components.h + CMakeLists.txt + SceneSerializer.cpp | 序列化光照参数到 .scene |
| MeshRenderSystem 更新 | MeshRenderSystem.h | 无需改动（已通过 Material 绑定 shader） |

**数据流**：

```
Scene::OnRender()
  |- LightSystem::OnRender()       <- 收集光源到 Renderer::s_LightData
  |    \_ 遍历 LightComponent + TransformComponent -> 填充 SceneLightData
  \_ MeshRenderSystem::OnRender()  <- 提交网格 draw
       \_ Renderer::Submit(material, va, transform)
            \_ RenderQueue::Flush(vp, cameraPos, lightData)  <- 新增光照参数
                 \_ 每组绑定时上传 u_DirectionalLight / u_PointLights[...] / u_CameraPosition
```

**预估**：3-5 天。是引擎视觉表现的最大单步提升。

### 阶段 C：高级光照（遵循路线图 3e，延后）

**目标**：阴影、PBR、IBL、延迟渲染--依赖 2b（双后端抽象收敛）。

| 任务 | 前置依赖 | 说明 |
|------|---------|------|
| 阴影贴图 | 放开 FrameBuffer depth-only 断言 + 多 pass | 方向光 shadow map -> 采样比较 |
| PBR 材质 | Material 纹理槽位（阶段 A）+ 统一 BRDF | Cook-Torrance 微表面模型 |
| IBL 环境光 | TextureCube + 预滤波辐照度图 | 天空盒 cubemap 采样 |
| 延迟渲染 | FrameBuffer MRT + G-buffer 着色器 | 位置/法线/反照率 -> 光照 pass |

**预估**：1-2 周（每个子项独立）。

---

## 5. 技术设计要点

### 5.1 光照数据上传策略

```
当前：  per-draw glUniform* 调用（u_ViewProjection / u_Transform）
    |
阶段 B：per-group glUniform* 调用（光源数组）+ per-draw（transform/normalMatrix）
    |
阶段 C：UBO 全局光照块（一次绑定，所有 shader 共享）  <- 需新增 UBO 抽象
```

阶段 B 保持与现有 uniform 上传机制一致（不改 Shader 抽象），用 SetFloat3 / SetFloat4 数组上传光源。性能可接受（光源数量 < 32 时）。

### 5.2 着色器 uniform 命名约定

```glsl
// 阶段 A
uniform vec3  u_CameraPosition;        // 相机世界坐标
uniform mat3  u_NormalMatrix;          // 法线变换矩阵

// 阶段 B - 方向光
uniform vec3  u_DirectionalLight_direction;
uniform vec3  u_DirectionalLight_color;
uniform float u_DirectionalLight_intensity;

// 阶段 B - 点光源数组（展平命名，因 Shader 抽象不支持 struct 数组）
uniform int   u_PointLightCount;
uniform vec3  u_PointLights_position[MAX_LIGHTS];
uniform vec3  u_PointLights_color[MAX_LIGHTS];
uniform float u_PointLights_intensity[MAX_LIGHTS];
uniform float u_PointLights_constant[MAX_LIGHTS];
uniform float u_PointLights_linear[MAX_LIGHTS];
uniform float u_PointLights_quadratic[MAX_LIGHTS];

// 阶段 B - 环境光
uniform vec3  u_AmbientColor;
uniform float u_AmbientIntensity;
```

> 注意：当前 Shader 抽象使用按名 SetFloat3(name, value) 上传，不支持 struct 数组的 name.field 索引。
> 阶段 B 需展平为独立数组 uniform，或扩展 Shader 支持 UBO。

### 5.3 LightComponent 设计

```cpp
struct LightComponent
{
    enum class Type : uint8_t { Directional = 0, Point, Spot };

    Type       Type            = Type::Directional;
    glm::vec3  Color           = { 1.0f, 1.0f, 1.0f };
    float      Intensity       = 1.0f;

    // Point / Spot 衰减
    float      Range           = 10.0f;
    float      Constant        = 1.0f;
    float      Linear          = 0.09f;
    float      Quadratic       = 0.032f;

    // Spot 专用
    float      InnerConeAngle  = 12.5f;  // 度
    float      OuterConeAngle  = 17.5f;  // 度

    LightComponent() = default;
};
```

### 5.4 渲染排序考量

当前 RenderQueue 按 material/shader 排序。引入光照后：
- 所有 lit 材质共享同一套光照 uniform -> 同 shader 分组仍正确
- 光照数据在每组首次绑定时上传一次 -> 无 per-draw 光照开销
- 未来透明物体需在 lit 物体之后渲染（RenderQueue 需增加透明度排序 key）

---

## 6. 风险与注意事项

| 风险 | 等级 | 缓解措施 |
|------|------|---------|
| OpenGL-first 渗漏 (E1) 导致 Vulkan 光照代码返工 | 中 | 阶段 B 先只验证 OpenGL；Vulkan 验证留到 2b 收敛后 |
| 逐 draw uniform 上传光源数组性能差 | 中 | 阶段 B 限制 MAX_LIGHTS=16；阶段 C 引入 UBO |
| Material 纹理绑定改动影响 .mat 序列化 | 低 | 向后兼容：textures 字段可选，缺失时无纹理 |
| SceneSerializer 需支持 LightComponent | 低 | 照现有 TransformComponent 序列化模式即可 |
| Shader struct 数组 uniform 不支持 | 中高 | 展平命名 OR 扩展 Shader::SetFloat3Array |

---

## 7. 总结

```
当前状态：渲染管线完整 [OK] | 顶点法线就绪 [OK] | 光照逻辑完全缺失 [MISSING]

         +---------------------------------------------------+
         |  阶段 A（前置）  |  阶段 B（基础光照）  |  阶段 C（高级）  |
         |  1-2 天          |  3-5 天              |  1-2 周          |
         |  低风险          |  中风险              |  依赖 2b         |
         |  -----------     |  -----------         |  -----------     |
         |  相机位置        |  LightComponent      |  阴影贴图        |
         |  法线矩阵        |  LightSystem         |  PBR 材质        |
         |  Material 纹理   |  BlinnPhong.glsl     |  IBL 环境光      |
         |  .mat 格式扩展   |  光照 uniform 上传   |  延迟渲染        |
         +---------------------------------------------------+
                              ^
                     建议立即推进阶段 A + B
              （引擎从技术演示变为可视化 3D 引擎的关键一步）
```

**核心判断**：顶点数据层（法线/切线/UV）已通过 assimp 导入就绪，但着色器层完全空白--这是
「数据已到 GPU 却未被利用」的状态。添加基础前向光照的投入产出比极高，且不与路线图冲突
（3e 指的是 PBR/阴影/延迟等高级技术，基础 Blinn-Phong 是其前置）。
