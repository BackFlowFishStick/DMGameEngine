# DMGameEngine — 项目开发总结

> 最后更新：2026-08-03（更新）  
> 目标：C++23 动态链接库游戏引擎，供游戏工程通过 CMake `find_package` 引用

---

## 1. 目录结构

```
engine/
├── CMakeLists.txt
├── dependencies/
│   ├── glad/
│   │   ├── include/glad/glad.h         # OpenGL loader header
│   │   ├── include/KHR/khrplatform.h
│   │   └── src/glad.c                  # 单文件源码，直接编译进引擎
│   ├── glm-master/
│   │   ├── glm/glm.hpp                  # OpenGL Mathematics 头文件库
│   │   ├── glm/gtc/...                  # GTC 扩展（矩阵变换等）
│   │   ├── glm/gtx/...                  # GTX 扩展（实验性）
│   │   └── CMakeLists.txt               # 官方 CMake，通过 add_subdirectory 集成
│   ├── spdlog-1.x/
│   │   └── include/spdlog/...            # 头文件库，自带 bundled fmt
│   └── glfw-3.4/
│       ├── include/GLFW/glfw3.h          # 窗口库头文件
│       ├── src/...                       # 源码，由 CMake add_subdirectory 编译
│       └── CMakeLists.txt
├── ENGINE_SUMMARY.md                     # 本文件
├── PRECOMPILED_HEADER.md                 # 预编译头指南（原理/方法/注意事项/本项目决策）
└── src/DMGameEngine/
    ├── DMGameEngine.h                    # 公共 API 入口头文件
    ├── dmge_pch.h                        # 预编译头内容（stdlib+glm+spdlog），/FI 强制包含进每个 C++ TU
    ├── Core/
    │   ├── Export.h                      # DLL 导出宏 DMGE_API + 智能指针别名 Scope/Ref/CreateScope/CreateRef
    │   ├── Application.h / .cpp          # 引擎生命周期基类
    │   ├── Layer.h                       # 分层架构基类 + LayerType 枚举
    │   ├── LayerStack.h / .cpp           # 层栈管理器（Layer/Overlay 插入顺序）
    │   ├── EntryPoint.h                  # 引擎 main() 入口点
    │   ├── Log.h / .cpp                  # spdlog 日志封装
    │   ├── Input.h                       # 输入抽象接口（平台无关轮询：键鼠 + 手柄 + 逐帧鼠标位移）
    │   ├── KeyCodes.h                   # 键盘按键码枚举（KeyCode）
    │   ├── MouseCodes.h                 # 鼠标按键码枚举（MouseCode）
    │   ├── GamepadCodes.h               # 手柄按键/轴码枚举（GamepadButton / GamepadAxis / kMaxGamepads）
    │   ├── Timestep.h                   # 帧时间步抽象（秒数 + GetSeconds/GetMilliseconds，隐式转 float）
    │   ├── Window.h                      # 窗口抽象接口（含 CursorMode 光标模式 + raw mouse motion）
    │   └── Events/
    │       ├── Event.h                   # 事件核心：EventType/Category 枚举、Event 基类、EventDispatcher
    │       ├── KeyEvent.h                # 键盘事件（依赖 KeyCodes.h）
    │       ├── MouseEvent.h              # 鼠标事件（依赖 MouseCodes.h）
    │       ├── GamepadEvent.h            # 手柄热插拔事件（GamepadConnected / Disconnected）
    │       └── ApplicationEvent.h        # 窗口/应用生命周期事件
    ├── Renderer/
    │   ├── Camera.h                     # 相机基类，持有投影 + 视图矩阵（glm::mat4，header-only）
    │   ├── OrthographicCamera.h         # 正交相机（位置/旋转 → 视图矩阵，header-only）
    │   ├── PerspectiveCamera.h          # 透视相机（位置/目标 → 视图矩阵，header-only）
    │   ├── GraphicsContext.h            # 渲染上下文抽象接口（平台无关）
    │   ├── Renderer.h / .cpp            # 高层场景渲染 API（BeginScene / EndScene / Submit 场景提交 + API 枚举；底层命令委托 RenderCommand）
│   ├── RenderCommand.h / .cpp       # 渲染命令门面（静态类，持有 RendererAPI 后端实例，转发 clear/viewport/blend/depth/cull/draw）
│   ├── RenderQueue.h / .cpp          # 渲染队列（延迟提交 + 按 material/shader 排序分组 + 每组上传光照/相机/法线矩阵，同组只 bind 一次）
    │   ├── RendererAPI.h / .cpp        # 渲染后端抽象基类（DrawIndexed 纯虚 + Create() 工厂）
    │   ├── Shader.h / .cpp              # Shader 基类 + Create() 工厂（抽象不同图形 API）
    │   ├── Material.h / .cpp            # Material（持有 Shader + uniform 值 + 纹理槽位，Bind() 一次上传 uniform + 绑定纹理，后端无关）
    │   ├── UniformSerializer.h          # 内部头：uniform<->JSON（UniformValueToJson + ApplyUniform，.mat 与 scene 覆盖共用；含 nlohmann/json，仅 .cpp 包含）
    │   ├── Texture.h                    # Texture 抽象基类（header-only）+ Format/Filter/Wrap 枚举
    │   ├── Texture2D.h / .cpp           # Texture2D（派生 Texture）+ Texture2DSpecification + Create() 工厂
    │   ├── TextureCube.h / .cpp         # TextureCube（派生 Texture）+ CubeFace 枚举 + Specification + Create() 工厂
    │   ├── Texture2DArray.h / .cpp      # Texture2DArray（派生 Texture）+ Layers + Specification + Create() 工厂
    │   ├── VertexArray.h / .cpp         # VertexArray 基类 + Create() 工厂（捆绑 VertexBuffer + IndexBuffer 的 VAO 抽象）
    │   ├── VertexBuffer.h / .cpp        # VertexBuffer 基类 + Create() 工厂（顶点缓存抽象）
    │   ├── IndexBuffer.h / .cpp         # IndexBuffer 基类 + Create() 工厂（下标缓存抽象）
    │   ├── FrameBuffer.h / .cpp         # FrameBuffer 基类 + Create() 工厂（渲染目标 / RTT 抽象）
    │   └── Light.h                      # 光照数据结构（Directional/Point/Spot LightData + SceneLightData）
    ├── Scene/
    │   ├── SceneCamera.h
    │   ├── OrthographicCameraController.h / .cpp
    │   ├── EditorCameraController.h
    │   ├── DefaultSceneLayer.h          # 场景层基类（持 CameraController + Scene，bracket BeginScene/EndScene）
    │   ├── Entity.h                    # ECS Entity = uint32_t（ID，非对象）+ NullEntity
    │   ├── Scene.h / .cpp              # ECS Scene（entt::registry + System 列表 + Transform 层级 dirty 传播/剪枝/环检测/孤儿子节点）
    │   ├── Components/                 # ECS Component（纯数据 POD）
    │   │   ├── IDComponent.h           # UUID（序列化身份）
    │   │   ├── TagComponent.h          # 名字
    │   │   ├── TransformComponent.h    # 本地变换 + 三叉链层级（Parent/FirstChild/NextSibling）+ world 缓存 + dirty
    │   │   ├── MeshComponent.h         # VAO + Material（对接 Renderer::Submit）
    │   │   ├── CameraComponent.h       # 包装 SceneCamera + Primary
    │   │   ├── LightComponent.h         # 光源组件（Directional / Point / Spot + 颜色/强度/衰减/锥角/环境光）
    │   │   └── Components.h            # 聚合 include
    │   └── Systems/                    # ECS System（纯逻辑）
    │       ├── System.h                # 基类 OnUpdate/OnRender/OnEvent + Scene&
    │       ├── TransformSystem.h       # dirty 传播 + 拓扑序 world matrix（!dirty 短路）
    │       ├── MeshRenderSystem.h      # view<Transform,Mesh> -> Renderer::Submit（零适配对接）
    │       └── LightSystem.h          # view<Light,Transform> -> 收集光源 -> Renderer::SubmitLightData
    ├── Asset/                          # 资源管理（1a UUID 方案）
    │   ├── AssetTypes.h                # AssetUUID(uint64) + AssetType 枚举 + AssetMetadata(path/type/dependencies) + AssetTypeOf<T> 特化
    │   ├── AssetHandle.h              # 纯身份令牌（只存 UUID，不存路径）
    │   ├── AssetLoader.h / .cpp       # AssetLoader<T> 模板 + Shader/Texture2D/Material/VertexArray/Mesh 特化
    │   ├── Mesh.h / .cpp                # Mesh 中间类（Vertices/Indices/SubMeshes/Layout + GetVertexArray 懒上传）
    │   └── AssetManager.h / .cpp      # 单例 + 三表（Registry/PathToUUID/Cache）+ Load<T>(uuid)/Register/LoadRegistry/SaveRegistry                # 运行时可切换正交/透视的相机（含宽高比，header-only）
    ├── ImGui/
    │   └── ImGuiLayer.h / .cpp          # Dear ImGui 集成层（上下文 + GLFW/OpenGL3 后端 + 每帧 UI 通道）
    ├── Debug/
    │   ├── Profiler.h / .cpp            # CPU 帧级性能分析器（Profiler 单例 + RAII ScopeTimer + DMGE_PROFILE_SCOPE 宏 + 帧末聚合）
    │   └── ProfilerLayer.h / .cpp       # ProfilerLayer（LayerType::Tool overlay，F1 切换，ImGui 绘制 FPS / 帧时间图 / scope 表）
    └── Platform/
        ├── OpenGL/
        │   ├── OpenGLDebug.h                   # OpenGL 错误侦测宏 DMGE_GL_CALL（Debug glGetError 检查 + 断言；Release 零开销）
        │   ├── OpenGLGraphicsContext.h / .cpp  # GraphicsContext 的 OpenGL 实现（GLAD 加载 / SwapBuffers）
        │   ├── OpenGLRendererAPI.h / .cpp     # RendererAPI 的 OpenGL 实现（glDrawElements）
        │   ├── OpenGLShader.h / .cpp           # Shader 的 OpenGL 实现（GLSL 编译/链接/Uniform）
        │   ├── OpenGLTextureUtils.h           # GL 枚举映射共享工具（Format/Filter/Wrap -> GL，inline）
        │   ├── OpenGLTexture2D.h / .cpp       # Texture2D 的 OpenGL 实现（GL_TEXTURE_2D）
        │   ├── OpenGLTextureCube.h / .cpp     # TextureCube 的 OpenGL 实现（GL_TEXTURE_CUBE_MAP，6 面）
        │   ├── OpenGLTexture2DArray.h / .cpp  # Texture2DArray 的 OpenGL 实现（GL_TEXTURE_2D_ARRAY，glTexImage3D）
        │   ├── OpenGLVertexArray.h / .cpp     # VertexArray 的 OpenGL 实现（GL_VERTEX_ARRAY_OBJECT + 顶点属性绑定）
        │   ├── OpenGLVertexBuffer.h / .cpp    # VertexBuffer 的 OpenGL 实现（GL_ARRAY_BUFFER）
        │   ├── OpenGLIndexBuffer.h / .cpp     # IndexBuffer 的 OpenGL 实现（GL_ELEMENT_ARRAY_BUFFER）
        │   └── OpenGLFrameBuffer.h / .cpp     # FrameBuffer 的 OpenGL 实现（GL_FRAMEBUFFER）
        ├── Vulkan/                            # Vulkan 1.3 渲染后端（可选，CMake DMGE_VULKAN_BACKEND=ON）
        │   ├── VulkanDebug.h                  # Vulkan 调试 messenger + VkResult 字符串化（VK_EXT_debug_utils 回调）
        │   ├── VulkanDevice.h / .cpp          # Vulkan 实例 + 物理设备选择 + 逻辑设备（1.3 dynamicRendering/sync2）+ VMA 分配器 + ImmediateSubmit
        │   ├── VulkanSwapchain.h / .cpp       # 交换链 + 深度/模板（acquire/present/recreate）
        │   ├── VulkanGraphicsContext.h / .cpp # GraphicsContext 的 Vulkan 实现（dynamic rendering 帧生命周期 BeginFrame/EndFrame，static Get()）
        │   ├── VulkanRendererAPI.h / .cpp    # RendererAPI 的 Vulkan 实现（管线缓存 + 每帧 scratch UBO + 每绘制描述符池）
        │   ├── VulkanShader.h / .cpp          # Shader 的 Vulkan 实现（shaderc GLSL->SPIR-V + GLSL 重写桥接）
        │   ├── VulkanTexture.h                # Texture Vulkan 共享基类（描述符互操作，header-only）
        │   ├── VulkanTextureUtils.h          # VK 枚举映射共享工具（Format/Filter/Wrap -> Vk，inline）
        │   ├── VulkanTextureHelpers.h        # 纹理上传 / 布局转换共享助手（inline）
        │   ├── VulkanTexture2D.h / .cpp      # Texture2D 的 Vulkan 实现
        │   ├── VulkanTexture2DArray.h / .cpp # Texture2DArray 的 Vulkan 实现
        │   ├── VulkanTextureCube.h / .cpp    # TextureCube 的 Vulkan 实现
        │   ├── VulkanVertexArray.h / .cpp    # VertexArray 的 Vulkan 实现（顶点属性 -> VkVertexInput）
        │   ├── VulkanVertexBuffer.h / .cpp   # VertexBuffer 的 Vulkan 实现（VMA 上传缓冲）
        │   ├── VulkanIndexBuffer.h / .cpp    # IndexBuffer 的 Vulkan 实现
        │   └── VulkanFrameBuffer.h / .cpp    # FrameBuffer 的 Vulkan 实现
        └── Windows/
            ├── WindowsWindow.h           # GLFW 窗口实现（通过 GraphicsContext 抽象管理 OpenGL 上下文）
            ├── WindowsWindow.cpp         # GLFW 窗口实现（事件回调映射 + SwapBuffers 委托）
            ├── GLFWInput.h                 # GLFW 轮询式输入实现
            └── GLFWInput.cpp              # glfwGetKey/glfwGetMouseButton/glfwGetCursorPos 直接轮询
```

---

## 2. 构建系统（CMakeLists.txt）

| 配置项 | 值 |
|---|---|
| CMake 最低版本 | 3.20 |
| C++ 标准 | C++23（强制，无扩展） |
| 项目名 | DMGameEngine，版本 0.1.0 |
| 构建类型 | `DMGE_BUILD_SHARED=ON`（DLL）/ OFF（静态库） |
| 示例项目 | `DMGE_BUILD_EXAMPLES=OFF`（默认关闭） |
| MSVC 编译选项 | `/W4 /utf-8` |
| Debug 断言 | `$<$<CONFIG:Debug>:DMGE_ENABLE_ASSERTS>`（Debug 构建自动注入） |
| 预编译头 | `DMGE_USE_PCH=ON`（默认开启；`/FI` 强制包含 stdlib+glm+spdlog 缓存解析加速编译，OFF 关闭） |

**源文件管理：** ⚠️ 已改为显式列出源文件（`set(DMGE_SOURCES ...)`），不再使用 `file(GLOB_RECURSE ...)`。新增 `.cpp` / `.h` 时必须在 `CMakeLists.txt` 的对应 `set(...)` 中手动添加条目，避免增量构建时的文件遗漏问题。

**依赖集成策略：**
- `dependencies/*/include` → 自动 glob 加入 PUBLIC include 路径
- `dependencies/*/lib/*.lib` / `*.a` → 自动 glob 链接
- GLM：通过 `add_subdirectory` 引入官方 CMake，生成 `glm` target（INTERFACE 库），`PUBLIC` 链接到引擎（因 Shader.h 公共头文件暴露了 glm 类型）
- GLFW：检测到 `glfw-3.4/CMakeLists.txt` 时通过 `add_subdirectory` 从源码编译（关闭 docs/tests/examples）
- GLAD 单文件集成：检测到 `glad/src/glad.c` 时通过 `target_sources` 直接编译进引擎（include 由 glob 自动覆盖）

**安装规则：**
- `cmake --install` 生成 `DMGameEngineTargets.cmake`
- 游戏工程可通过 `find_package(DMGameEngine REQUIRED)` 引用
- `target_link_libraries(MyGame PRIVATE DMGameEngine::DMGameEngine)`

---

## 3. 核心模块

### 3.1 Export.h — DLL 导出宏

```cpp
// 构建引擎时 CMake 定义 DMGE_BUILD_DLL → __declspec(dllexport)
// 消费引擎时 _WIN32 定义 → __declspec(dllimport)
// 其他情况（静态库 / IDE 解析）→ 空
#define DMGE_API  // 根据编译环境自动展开
```

所有需要导出的类/函数前加 `DMGE_API`。

**智能指针别名（namespace DM）：**

```cpp
namespace DM
{
    template<typename T> using Scope = std::unique_ptr<T>;
    template<typename T> using Ref  = std::shared_ptr<T>;

    template<typename T, typename... Args>
    Scope<T> CreateScope(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

    template<typename T, typename... Args>
    Ref<T>  CreateRef(Args&&... args)  { return std::make_shared<T>(std::forward<Args>(args)...); }
}
```

引擎代码位于 `namespace DMGameEngine`，通过 `Export.h` 末尾的 `using DM::Scope` / `using DM::Ref` / `using DM::CreateScope` / `using DM::CreateRef` 将别名引入。全工程统一使用 `DM::Scope<T>` / `DM::Ref<T>`（类型）与 `DM::CreateScope<T>(...)` / `DM::CreateRef<T>(...)`（工厂调用）替代裸 `std::unique_ptr` / `std::shared_ptr` / `std::make_unique` / `std::make_shared`。

### 3.2 Application — 生命周期基类

```cpp
class DMGE_API Application {
public:
    Application();                              // 默认窗口：1280×720, "DMGameEngine"
    explicit Application(const WindowProps&);   // 自定义窗口属性

    int Run();           // 入口：Initialize → MainLoop → Shutdown
    void Quit();         // 请求退出主循环
    bool IsRunning();

    Window& GetWindow(); // 获取当前窗口引用
    Camera* GetActiveCamera();          // 激活场景相机（空则 BeginScene 用 identity VP）
    void SetActiveCamera(const DM::Ref<Camera>& camera); // 设置激活相机，驱动 BeginScene 视图投影

    static Application& Get(); // 全局单例访问器

    // ── Layer 管理 ────────────────────────
    void PushLayer(DM::Scope<Layer> layer);
    void PushOverlay(DM::Scope<Layer> overlay);
    DM::Scope<Layer> PopLayer(Layer* layer);
    DM::Scope<Layer> PopOverlay(Layer* overlay);

protected:
    virtual void OnInitialize();   // 覆写实现初始化
    virtual void OnUpdate(Timestep ts); // 覆写实现逐帧逻辑，ts 为帧时间步（秒）
    virtual void OnRender();       // 覆写实现渲染
    virtual void OnShutdown();     // 覆写实现清理

    virtual void OnEvent(Event& e); // 覆写处理窗口事件（需调用基类保留关闭行为）
};
```

- 主循环使用 `std::chrono::high_resolution_clock` 计算 deltaTime，钳制上限 0.1s 后包进 `Timestep` 再分发
- 五个虚函数均为空/默认实现，派生类按需覆写
- **窗口自动管理**：`Initialize()` 中通过 `Window::Create(m_windowProps)` 自动创建窗口
- **事件绑定**：窗口 GLFW 回调固定转发到 `OnEvent(e)`，事件先逆序遍历 LayerStack 分发（Overlay 优先消费），未被处理的才进入 Application 级兜底（`WindowCloseEvent` → `Quit()`）
- **内置 LayerStack**：Application 持有 `LayerStack` 实例，通过 `PushLayer` / `PushOverlay` / `PopLayer` / `PopOverlay` 管理各子系统层级
- `Shutdown()` 中先调用 `OnShutdown()`，再显式清空 `m_layerStack`（触发各层 `OnDetach`），最后销毁窗口
- **激活场景相机**：`SetActiveCamera(DM::Ref<Camera>)` 设置激活相机；`MainLoop` 渲染阶段若已设则 `Renderer::BeginScene(*camera)` 缓存其视图投影，否则 `BeginScene()` 以 identity 视图投影开始

**主循环五阶段结构：**

```
每帧循环:
┌─ Stage 1: Event Pump ───────────────────────────┐
│ m_window->PollEvents()                           │
│ 从 OS 拉取所有输入/窗口事件，触发回调链分发至各层   │
│ 必须最先执行：为整帧提供最新的输入状态             │
├─ Stage 2: Update ────────────────────────────────┤
│ LayerStack 正向迭代 → layer->OnUpdate(ts)         │
│ Application::OnUpdate(ts)  // 无层时的兜底路径   │
│ 正向 = Platform → Core → Resource → Feature → Tool│
├─ Stage 3: Render ────────────────────────────────┤
│ Renderer::BeginScene()  // 清屏（颜色+深度）       │
│ LayerStack 正向迭代 → layer->OnRender()           │
│ Application::OnRender()                           │
│ Renderer::EndScene()   // 帧结束占位（预留提交）  │
├─ Stage 4: ImGui ─────────────────────────────────┤
│ LayerStack 正向迭代 → layer->OnImGuiRender()      │
├─ Stage 5: Swap ──────────────────────────────────┤
│ m_window->SwapBuffers()                           │
│ 呈现渲染结果到显示器（双缓冲翻页）                  │
└──────────────────────────────────────────────────┘
```

**PollEvents 与 Platform Layer 的分工：**

| | `m_window->PollEvents()` | Platform Layer |
|---|---|---|
| 本质 | OS 事件泵，引擎与外部世界的桥梁 | 平台服务提供者 |
| 执行时机 | 每帧最先，在任何层之前 | 在 LayerStack 正向迭代中 |
| 职责 | 从 OS 拉取输入/窗口事件，注入引擎事件系统 | 文件 I/O、线程池、内存映射、定时器等平台级服务 |
| 为什么不能合并 | 事件必须先收集再分发——放在层内会导致前面的层拿不到本帧事件 | Platform Layer 是事件的消费者之一，PollEvents 是生产者 |

- 派生类覆写 `OnEvent` 追加自定义事件处理时，必须调用 `Application::OnEvent(e)` 保留 LayerStack 事件传播和关闭行为

### 3.3 EntryPoint.h — 引擎入口点

```cpp
extern Application* CreateApplication();  // 游戏工程实现

int main(int, char**) {
    Log::Init();
    auto* app = CreateApplication();
    int result = app->Run();
    delete app;
    Log::Shutdown();
    return result;
}
```

- `EntryPoint.h` 包含 `main()` 定义，**只能被一个翻译单元包含**
- `DMGameEngine.h` 不包含它（避免 ODR 违规）

### 3.4 Log — 日志系统

基于 **spdlog 1.17.0**，控制台（彩色）+ 文件（`logs/DMGameEngine.log`）双 sink。

| 宏 | 说明 |
|---|---|
| `DMGE_LOG_TRACE/INFO/WARN/ERROR/CRITICAL(...)` | 引擎核心日志 |
| `DMGE_CLIENT_TRACE/INFO/WARN/ERROR/CRITICAL(...)` | 客户端/游戏日志 |

- Core logger 名称：`DMEngine`
- Client logger 名称：`APP`
- 日志格式：`[HH:MM:SS] DMEngine: message`（控制台）

### 3.5 事件系统

#### EventType 枚举（15 种）
| 分类 | 事件 |
|---|---|
| Window | Close, Resize, Focus, LostFocus, Moved |
| App | Tick, Update, Render |
| Key | Pressed, Released, Typed |
| Mouse | ButtonPressed, ButtonReleased, Moved, Scrolled |

#### EventCategory 枚举（位掩码，可组合）
`Application | Input | Keyboard | Mouse | MouseButton`

#### 宏减少样板代码
```cpp
EVENT_CLASS_TYPE(KeyPressed)      // 生成 GetStaticType() / GetEventType() / GetName()
EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)
```

#### EventDispatcher — 模板化事件分发
```cpp
EventDispatcher dispatcher(event);
dispatcher.Dispatch<KeyPressedEvent>([](KeyPressedEvent& e) {
    if (e.GetKeyCode() == KeyCode::Escape) return true; // handled
    return false;
});
```

### 3.6 Window — 窗口抽象

```cpp
class DMGE_API Window {
public:
    using EventCallbackFn = std::function<void(Event&)>;

    virtual void PollEvents() = 0;
    virtual void SetEventCallback(const EventCallbackFn&) = 0;
    virtual void SetVSync(bool) = 0;

    static DM::Scope<Window> Create(const WindowProps& props = {});
};

struct WindowProps {
    std::string title = "DMGameEngine";
    unsigned int width = 1280, height = 720;
};
```

- 工厂方法 `Window::Create()` 返回平台实例
- 当前唯一实现：`Platform/Windows/WindowsWindow`（基于 GLFW）

### 3.7 WindowsWindow — GLFW 平台窗口

- GLFW 初始化：`glfwInit()` + 创建窗口，配置 OpenGL 4.6 Core Profile 上下文
- **图形上下文抽象**：窗口创建后，通过 `DM::CreateScope<OpenGLGraphicsContext>(m_window)` 创建 OpenGL 后端，再调用 `m_context->Init()` 完成 `glfwMakeContextCurrent` + GLAD 加载 + GPU 信息日志
- 持有 `DM::Scope<GraphicsContext> m_context`（抽象类型），SDK/资源加载失败时通过 `DMGE_CORE_ASSERT` 触发断点
- `SwapBuffers()` 委托给 `m_context->SwapBuffers()` → `glfwSwapBuffers`
- `Shutdown()` 中先行 `m_context.reset()` 再 `glfwDestroyWindow`，确保图形资源先于窗口释放
- 需在 glad 前定义 `GLFW_INCLUDE_NONE` 防止 GLFW 默认引入系统 OpenGL 头文件与 glad 冲突
- 完整事件回调映射：

| GLFW 回调 | DMGE 事件 |
|---|---|
| `SetWindowSizeCallback` | `WindowResizeEvent` |
| `SetWindowCloseCallback` | `WindowCloseEvent` |
| `SetWindowFocusCallback` | `WindowFocusEvent` / `WindowLostFocusEvent` |
| `SetKeyCallback` | `KeyPressedEvent` / `KeyReleasedEvent` |
| `SetCharCallback` | `KeyTypedEvent` |
| `SetMouseButtonCallback` | `MouseButtonPressedEvent` / `MouseButtonReleasedEvent` |
| `SetCursorPosCallback` | `MouseMovedEvent` |
| `SetScrollCallback` | `MouseScrolledEvent` |

- `PollEvents()` 调用 `glfwPollEvents()`
- `SwapBuffers()` 委托 `m_context->SwapBuffers()` → `glfwSwapBuffers`
- `SetVSync()` 调用 `glfwSwapInterval()`

### 3.8 Layer — 分层架构基类

引擎采用分层架构，所有子系统派生自 `Layer`。`LayerType` 枚举定义了 5 个架构层级（由低到高：底层先初始化、最后销毁）：

| 枚举值 | 层级 | 职责 |
|---|---|---|
| `LayerType::Platform` | 平台层 | OS 抽象、窗口、文件系统 |
| `LayerType::Core` | 核心层 | 内存分配、线程管理、数学运算 |
| `LayerType::Resource` | 资源层 | 资源加载、缓存、资产管理 |
| `LayerType::Feature` | 功能层 | 动画、物理、渲染、输入、脚本 |
| `LayerType::Tool` | 工具层 | 编辑器工具、调试覆盖层、性能分析器 |

```cpp
class DMGE_API Layer {
public:
    Layer(const std::string& name = "Layer",
          LayerType            type = LayerType::Feature);
    virtual ~Layer() = default;

    // ── 生命周期钩子 ──────────────────────
    virtual void OnAttach()    {}      // 层被推入栈时调用
    virtual void OnDetach()    {}      // 层被弹出栈时调用
    virtual void OnUpdate(Timestep ts) {} // 逐帧逻辑，ts 为帧时间步（秒）
    virtual void OnRender()    {}      // 渲染调用
    virtual void OnEvent(Event&) {}    // 事件分发

    // ── UI 钩子（预留 ImGui 集成）────────
    virtual void OnImGuiRender() {}

    const std::string& GetName() const; // 调试名称
    LayerType          GetType() const; // 架构层级
};
```

- 派生类按需覆写钩子，空实现保证最小编译开销
- 辅助函数 `LayerTypeToString()` 用于日志/调试输出
- 默认 `LayerType::Feature`，派生类构造函数中传入实际层级

### 3.9 LayerStack — 层栈管理器

`LayerStack` 管理所有 `Layer` 的生命周期与迭代顺序：

```cpp
class DMGE_API LayerStack {
public:
    LayerStack();
    ~LayerStack();  // 委托 Clear()，逆序 Detach 所有层
    void Clear();   // 显式收尾：逆序 OnDetach() 后清空栈（幂等；析构复用）

    // Non-copyable
    LayerStack(const LayerStack&) = delete;
    LayerStack& operator=(const LayerStack&) = delete;

    void PushLayer(DM::Scope<Layer> layer);
    void PushOverlay(DM::Scope<Layer> overlay);
    DM::Scope<Layer> PopLayer(Layer* layer);
    DM::Scope<Layer> PopOverlay(Layer* overlay);

    // 正向迭代：普通层 → Overlay（更新顺序）
    // 反向迭代：Overlay → 普通层（渲染顺序，UI 在顶层）
    begin() / end() / rbegin() / rend();
};
```

**设计要点：**
- 使用 `DM::Scope<Layer>` 独占所有权，Push 时转移
- `m_layerInsertIndex` 始终指向普通层与 Overlay 的分界点
  - `PushLayer()` 在索引处插入，索引递增
  - `PushOverlay()` 始终追加到末尾，索引不变
- 普通层按插入顺序更新；Overlay 在渲染时逆序（后入栈的 Overlay 渲染在更上层）
- `OnDetach()` 逆序调用逻辑封装在 `Clear()` 中；析构函数复用 `Clear()`，对已清空栈幂等（多次调用安全）
- ⚠️ 移动赋值 `operator=(LayerStack&&) = default` 仅做成员级赋值、不执行析构体，故不会触发 `OnDetach()`；`Shutdown()` 须显式 `Clear()`，不可用 `m_layerStack = LayerStack{}`（否则 `OnDetach()` 被静默跳过）
- 显式声明不可拷贝（`= delete`），防止 `unique_ptr` 误复制

### 3.10 Application 与 LayerStack 集成

Application 内置 `LayerStack m_layerStack` 成员，每帧自动驱动所有层的生命周期：

**层管理接口：** Application 暴露的 `PushLayer` / `PushOverlay` / `PopLayer` / `PopOverlay` 直接委托给内部 `m_layerStack`，所有权通过 `DM::Scope` 转移。

**事件传播（逆序）：**

```cpp
void Application::OnEvent(Event& e) {
    // Overlay（UI/Tool）优先消费，已被处理则停止传播
    for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
        (*it)->OnEvent(e);
        if (e.Handled)
            return;
    }
    // Application 级兜底：WindowClose → Quit
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&) {
        Quit();
        return true;
    });
}
```

**每帧驱动（正向）：**

```cpp
// Stage 2 — Update: Platform → Core → Resource → Feature → Tool
for (auto& layer : m_layerStack)
    layer->OnUpdate(ts);

// Stage 3 — Render: Renderer 包裹帧，层正向提交绘制
Renderer::BeginScene();
for (auto& layer : m_layerStack)
    layer->OnRender();
Renderer::EndScene();

// Stage 4 — ImGui: Begin/End 包裹 OnImGuiRender() 通道
m_ImGuiLayer->Begin();
for (auto& layer : m_layerStack)
    layer->OnImGuiRender();
m_ImGuiLayer->End();
```

**清理顺序：** `Shutdown()` 中先调用用户 `OnShutdown()`，再 `m_layerStack.Clear()` 显式逆序调用各层 `OnDetach()` 并清空栈（位于 `m_window.reset()` 之前、GL 上下文仍存活，故层可在 `OnDetach()` 安全释放 GL 资源），最后 `m_window.reset()` 销毁窗口。

---

### 3.11 Input — 轮询式输入管理（含边沿检测 + GLFW_REPEAT 显式处理）

Input 为**抽象基类**，定义平台无关的轮询接口。具体实现通过 `Input::Get()` 返回平台单例。

**两种查询模式：**

| 方法 | 语义 | 实现 |
|---|---|---|
| `IsKeyPressed(KeyCode)` | **连续** — 按住期间每帧返回 true | `glfwGetKey()` 直接轮询 |
| `IsKeyJustPressed(KeyCode)` | **上升沿** — 仅在首次按下帧返回 true，repeat 不触发 | GLFW 状态 ⊕ `m_keyJustPressed` 标记 |
| `IsMouseButtonPressed(MouseCode)` | 连续 | `glfwGetMouseButton()` |
| `IsMouseButtonJustPressed(MouseCode)` | 上升沿 | GLFW ⊕ `m_mouseButtonJustPressed` 标记 |

**GLFW_REPEAT 显式处理：**

```
OnEvent 收到 KeyPressedEvent:
  ├─ repeatCount == 0  (GLFW_PRESS)   → m_keyJustPressed[key] = true   // 设置上升沿标记
  └─ repeatCount >= 1  (GLFW_REPEAT)  → 不设置，忽略重复触发

BeginFrame():
  m_keyJustPressed.clear()   // 每帧清空，确保标记仅当帧有效

IsKeyJustPressed(key):
  return glfwGetKey(key) == GLFW_PRESS   // GLFW 权威当前状态
      && m_keyJustPressed[key]            // 仅 repeatCount==0 时为 true
```

**每帧行为：**

| 帧 | GLFW 回调 | `m_keyJustPressed` | `IsKeyPressed` | `IsKeyJustPressed` |
|---|---|---|---|---|
| N | `GLFW_PRESS` (repeat=0) | `{W: true}` | `true` | `true` |
| N+1 | `GLFW_REPEAT` (repeat=1) | 被 BeginFrame 清空 | `true` | `false` |
| N+2 | `GLFW_REPEAT` (repeat=2) | 被 BeginFrame 清空 | `true` | `false` |
| N+3 | `GLFW_RELEASE` | — | `false` | `false` |

**数据流：**

```
Application::MainLoop():
  1. Input::Get().BeginFrame()           // 快照 + 清空 m_*JustPressed
  2. m_window->PollEvents()              // GLFW 回调 → OnEvent
     → Application::OnEvent(e)
       → Input::Get().OnEvent(e)          // 更新 m_keyState / m_keyJustPressed
       → LayerStack 逆序分发
  3. Layer::OnUpdate(ts)
     if (Input::Get().IsKeyPressed(W))     // glfwGetKey → GLFW_PRESS (连续)
       MoveForward();
     if (Input::Get().IsKeyJustPressed(Space)) // PRESS && repeat==0 (上升沿)
       Jump();
```

### 3.12 Renderer — 渲染抽象层

**GraphicsContext（`Renderer/GraphicsContext.h`）：**

```cpp
class DMGE_API GraphicsContext
{
public:
    virtual void Init() = 0;
    virtual void SwapBuffers() = 0;

    virtual const char* GetVendor() const = 0;
    virtual const char* GetRenderer() const = 0;
    virtual const char* GetVersion() const = 0;
};
```

**设计要点：**
- **职责窄化** — 仅管理单个图形 API 会话的生命周期（上下文创建 → 帧呈现 → 查询），不涉及绘制调用或材质
- **平台无关** — 纯虚接口，`Platform/OpenGL/`、`Platform/Vulkan/` 等目录各自实现
- **Init()** — 窗口就绪后绑定图形 API（OpenGL: `glfwMakeContextCurrent` + GLAD 动态加载；Vulkan: 创建 `VkSurface/Swapchain`）
- **SwapBuffers()** — 每帧末尾双缓冲翻页，VSync 开启时在此阻塞等待
- **GPU 指纹** — `GetVendor/GetRenderer/GetVersion` 用于日志记录、厂商 workaround、特性版本门控

**Renderer（`Renderer/Renderer.h`）：**

```cpp
class DMGE_API Renderer
{
public:
    enum class API { None = 0, OpenGL, Vulkan, DirectX };

    static void Init();
    static void Shutdown();
    static void BeginScene();
    static void BeginScene(const Camera& camera);
    static void EndScene();
    static void SetClearColor(const glm::vec4& color);
    static void Clear();
    static void Submit(const DM::Ref<Shader>& shader,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform = glm::mat4(1.0f));
    static void Flush();
    static void OnWindowResize(int width, int height);
    static API GetAPI() { return s_API; }
private:
    struct SceneData { glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f); };
    static API s_API;
    static SceneData s_SceneData;
};
```

**设计要点：**
- **静态门面** - 无实例，全局单例语义，`BeginScene/EndScene/Flush` 管理帧生命周期；`BeginScene()` 清屏并将场景视图投影重置为 identity
- **场景数据流** - `BeginScene(const Camera&)` 缓存 `GetViewProjection()` 至私有 `SceneData`；`Submit(shader, va, transform)` 绑定 Shader、写入 `u_ViewProjection`/`u_Transform` 后委托 `DrawIndexed`，MVP 由 Shader 在 GLSL 内派生
- **后端选择** — 通过 `s_API` 枚举在编译/启动时决定用 OpenGL / Vulkan / DirectX
- **职责分离** — `Renderer` 管"画什么"（场景提交），`GraphicsContext` 管"画到哪"（上下文管理）

**OpenGLGraphicsContext（`Platform/OpenGL/OpenGLGraphicsContext.h/.cpp`）：**

```cpp
class DMGE_API OpenGLGraphicsContext : public GraphicsContext
{
public:
    explicit OpenGLGraphicsContext(GLFWwindow* windowHandle);

    void Init() override;        // glfwMakeContextCurrent + gladLoadGLLoader
    void SwapBuffers() override; // glfwSwapBuffers
    const char* GetVendor() const override;   // glGetString(GL_VENDOR)
    const char* GetRenderer() const override; // glGetString(GL_RENDERER)
    const char* GetVersion() const override;  // glGetString(GL_VERSION)

private:
    GLFWwindow* m_WindowHandle = nullptr;
};
```

- 持有 `GLFWwindow*` 但不负责内存释放（生命周期由 `Window` 管理）
- `Init()` 中 GLAD 加载失败触发 `DMGE_CORE_ASSERT(status, ...)` → Debug 断点 / Release 零开销

### Shader 系统（`Renderer/Shader.h/.cpp` + `Platform/OpenGL/OpenGLShader.h/.cpp`）

**Shader 基类（`Renderer/Shader.h`）：**

```cpp
class DMGE_API Shader {
public:
    virtual ~Shader() = default;

    virtual void Bind()   const = 0;
    virtual void Unbind() const = 0;
    virtual const std::string& GetName() const = 0;

    // Uniform 设置接口
    virtual void SetInt(std::string_view name, int value) = 0;
    virtual void SetFloat(std::string_view name, float value) = 0;
    virtual void SetFloat2(std::string_view name, const glm::vec2& value) = 0;
    virtual void SetFloat3(std::string_view name, const glm::vec3& value) = 0;
    virtual void SetFloat4(std::string_view name, const glm::vec4& value) = 0;
    virtual void SetMat4(std::string_view name, const glm::mat4& value) = 0;

    // 工厂方法 — 根据 Renderer::GetAPI() 分发到对应后端
    static DM::Ref<Shader> Create(std::string_view filepath);
    static DM::Ref<Shader> Create(std::string_view name,
                                          std::string_view vertexSrc,
                                          std::string_view fragmentSrc);
};
```

**配套类型：**
- `ShaderDataType` — 枚举（Float / Float2~4 / Int / Int2~4 / Bool / Mat3 / Mat4），含 `ShaderDataTypeSize()` 辅助函数
- `BufferElement` — 单个顶点属性的布局描述（名称、类型、大小、偏移、是否归一化、分量数）
- `BufferLayout` — 一组 `BufferElement` 的布局，自动计算 stride 和 offset，支持 `std::initializer_list` 构造和 range-for 遍历

**OpenGL 后端（`Platform/OpenGL/OpenGLShader.h/.cpp`）：**

```cpp
class DMGE_API OpenGLShader : public Shader {
public:
    OpenGLShader(std::string_view name, std::string_view vertexSrc, std::string_view fragmentSrc);
    explicit OpenGLShader(std::string_view filepath);
    ~OpenGLShader() override;   // glDeleteProgram

    void Bind()   const override;   // glUseProgram(m_RendererID)
    void Unbind() const override;   // glUseProgram(0)

    // 所有 Uniform setter 通过 glUniform* 实现
    // ...
};
```

**设计要点：**
- **单文件多阶段** — 支持 `#type vertex` / `#type fragment` 标记，一个 `.glsl` 文件可包含多个 shader stage
- **编译错误日志** — 每个 shader 和 program link 失败时通过 `glGetShaderInfoLog` / `glGetProgramInfoLog` 输出完整错误信息
- **Uniform 缓存** — `m_UniformLocationCache` 缓存 `glGetUniformLocation` 结果，避免每帧重复查询
- **工厂分发** — `Shader::Create()` 在 `Renderer/Shader.cpp` 中通过 `switch (Renderer::GetAPI())` 返回对应后端实例

**使用示例：**
```cpp
// 从文件加载
auto shader = DMGameEngine::Shader::Create("assets/shaders/FlatColor.glsl");

// 或从源码创建
auto shader = DMGameEngine::Shader::Create("MyShader", vertexSrc, fragmentSrc);

shader->Bind();
shader->SetFloat4("u_Color", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
shader->SetMat4("u_MVP", viewProjection * model);
// ... draw calls ...
shader->Unbind();
```

---

**抽象接口（`Core/Input.h`）：**

```cpp
class DMGE_API Input {
public:
    virtual void BeginFrame() = 0;
    virtual bool IsKeyPressed(KeyCode keycode) const = 0;
    virtual bool IsKeyJustPressed(KeyCode keycode) const = 0;
    virtual bool IsMouseButtonPressed(MouseCode button) const = 0;
    virtual bool IsMouseButtonJustPressed(MouseCode button) const = 0;
    virtual float GetMouseX() const = 0;
    virtual float GetMouseY() const = 0;
    virtual float GetMouseDeltaX() const = 0;
    virtual float GetMouseDeltaY() const = 0;
    virtual bool IsGamepadPresent(int index) const = 0;
    virtual std::string GetGamepadName(int index) const = 0;
    virtual bool IsGamepadButtonPressed(int index, GamepadButton button) const = 0;
    virtual bool IsGamepadButtonJustPressed(int index, GamepadButton button) const = 0;
    virtual float GetGamepadAxis(int index, GamepadAxis axis) const = 0;
    virtual void OnEvent(Event& e) = 0;
    static Input& Get();
};
```

**设计要点：**
- **GLFW 权威轮询** — `IsKeyPressed` 直接读 GLFW 内部状态，不受 Layer 事件消费影响
- **GLFW_REPEAT 显式区分** — `OnEvent` 中检查 `GetRepeatCount()`，仅 repeatCount==0（`GLFW_PRESS`）设置 `m_keyJustPressed` 标记；repeatCount≥1（`GLFW_REPEAT`）不触发
- **双校验上升沿** — `IsKeyJustPressed` 同时检查 GLFW 当前状态（防止事件丢失）和 `m_keyJustPressed` 标记（排除 repeat）
- **BeginFrame 清空** — 每帧首将 `m_keyJustPressed` / `m_mouseButtonJustPressed` 清空，确保标记仅当帧有效
- **惰性窗口缓存** — 首次查询时通过 `Application::Get().GetWindow().GetNativeWindow()` 获取 `GLFWwindow*`

**使用示例：**

```cpp
// OnUpdate 中：
if (Input::Get().IsKeyPressed(KeyCode::W))            // 按住持续移动
    camera.MoveForward(ts);
if (Input::Get().IsKeyJustPressed(KeyCode::Space))     // 仅按下首帧跳跃（repeat 不触发）
    player.Jump();
if (Input::Get().IsMouseButtonJustPressed(MouseCode::Left))
    FireWeapon();
```

---

### Material 系统（`Renderer/Material.h/.cpp`）

`Material` 将一个 `Shader` 与其 uniform 参数值打包：调用方按名称存入 uniform 值，`Bind()` 时绑定 shader 并用 `std::visit` 一次性上传全部已存 uniform，免每帧逐个 `SetXxx`。`MaterialInstance` 派生自 `Material`，引用共享的基材质并叠加自身的 uniform 覆盖，使共享同一 `Material` 的多个实例可各自调参而互不影响。二者均后端无关，仅经 `Shader` 抽象操作，无平台工厂与平台子类（与 `Camera` 一致）。

```cpp
using UniformValue = std::variant<
    int, float, glm::vec2, glm::vec3, glm::vec4, glm::mat4, std::vector<int>>;

class DMGE_API Material
{
public:
    explicit Material(DM::Ref<Shader> shader);
    virtual ~Material() = default;

    virtual void Bind() const;                          // 绑定 shader + 上传全部已存 uniform
    const DM::Ref<Shader>& GetShader() const;

    virtual void SetInt(std::string_view name, int value);
    virtual void SetIntArray(std::string_view name, const int* values, uint32_t count);
    virtual void SetFloat(std::string_view name, float value);
    virtual void SetFloat2(std::string_view name, const glm::vec2& value);
    virtual void SetFloat3(std::string_view name, const glm::vec3& value);
    virtual void SetFloat4(std::string_view name, const glm::vec4& value);
    virtual void SetMat4(std::string_view name, const glm::mat4& value);

    virtual bool Has(std::string_view name) const;
    virtual const UniformValue* Get(std::string_view name) const;
};

// 共享基 Material（其 Shader + 默认 uniform），叠加私有 override；
// Bind() 先绑基材质、再上传本实例覆盖，兄弟实例互不影响。
class DMGE_API MaterialInstance : public Material
{
public:
    explicit MaterialInstance(DM::Ref<Material> baseMaterial);
    void Bind() const override;                        // 基 Bind() + 本实例 override
    const DM::Ref<Material>& GetBaseMaterial() const;

    void SetInt(std::string_view name, int value) override;           // 仅写 override
    void SetIntArray(std::string_view name, const int* values, uint32_t count) override;
    void SetFloat(std::string_view name, float value) override;
    void SetFloat2(std::string_view name, const glm::vec2& value) override;
    void SetFloat3(std::string_view name, const glm::vec3& value) override;
    void SetFloat4(std::string_view name, const glm::vec4& value) override;
    void SetMat4(std::string_view name, const glm::mat4& value) override;

    bool Has(std::string_view name) const override;                    // 先 override 再基
    const UniformValue* Get(std::string_view name) const override;
};
```

**设计要点：**
- **类型擦除存储** - `UniformValue` 为 `std::variant`，每个备选对应 `Shader` 的一个 uniform setter；`int` 数组存为 `std::vector<int>` 使其自持数据，`Bind()` 可反复重传
- **setter 镜像 Shader API** - 命名 `SetInt/SetFloat/SetFloat2-4/SetMat4/SetIntArray` 与 `Shader` 一致，避免重载 `Set` 在 `double` 字面量下歧义
- **`Bind()` 为 `const` + virtual** - 与 `Shader::Bind()` 一致；经 `shared_ptr` 调用 shader 非 const setter（同 `Renderer::Submit`）；虚函数使 `MaterialInstance` 可叠加覆盖
- **后端无关 / 无平台工厂** - 纯经 `Shader` 抽象，无 `OpenGLMaterial` 等平台派生；`Material` 构造即 `DM::CreateRef<Material>(shader)`
- **职责划分** - Material 持材质参数（颜色、强度等），场景级 `u_ViewProjection`/`u_Transform` 仍由 `Renderer::Submit` 经 `material->GetShader()` 上传
- **已纳入公共 API** - `DMGameEngine.h`、`CMakeLists.txt`（`DMGE_SOURCES` + `DMGE_HEADERS`）已登记

**MaterialInstance（材质实例）：**
- 共享基材质（`shared_ptr<Material>`，含 Shader + 默认 uniform）+ 私有 `m_Overrides`；`Bind()` 先 `m_BaseMaterial->Bind()`（绑 shader + 上传基 uniform），再用共享 `UploadUniforms()` 上传本实例覆盖，覆盖叠于基之上，**兄弟实例互不影响**
- setter 覆盖仅写入 `m_Overrides`，绝不触碰基的 `m_Uniforms`；`Has`/`Get` 先查覆盖再回落基
- 派生自 `Material`，`shared_ptr<MaterialInstance>` 隐式转 `shared_ptr<Material>`，故 `Renderer::Submit(shared_ptr<Material>&)` 经虚 `Bind()` 直接适用于实例，**无需新增 Submit 重载**
- `Material` 补 `virtual ~Material() = default;` 保障多态析构；`MaterialInstance` 构造 `DM::CreateRef<MaterialInstance>(baseMaterial)`
- `Material.cpp` 抽出匿名 `UploadUniforms(shader, map)`，`Material::Bind` 与 `MaterialInstance::Bind` 复用，visitor 不重复

---

### VertexArray 系统（`Renderer/VertexArray.h/.cpp` + `Platform/OpenGL/OpenGLVertexArray.h/.cpp`）

**VertexArray 基类（`Renderer/VertexArray.h`）：**

```cpp
class DMGE_API VertexArray
{
public:
    virtual ~VertexArray() = default;

    virtual void Bind()   const = 0;
    virtual void Unbind() const = 0;

    virtual void AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer) = 0;
    virtual void SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer)   = 0;

    virtual const std::vector<DM::Ref<VertexBuffer>>& GetVertexBuffers() const = 0;
    virtual const DM::Ref<IndexBuffer>& GetIndexBuffer() const = 0;

    // ── Factory ─────────────────────────────────────────────────
    static DM::Ref<VertexArray> Create();
};
```

**设计要点：**
- **职责** — 将一个或多个 `VertexBuffer`（各自携带 `BufferLayout` 描述顶点属性）与一个可选 `IndexBuffer` 打包，使一次 `Bind()` 即可接通一次绘制所需的全部顶点输入
- **平台无关** — 纯虚接口，`Platform/OpenGL/`、`Platform/Vulkan/` 等目录各自实现
- **工厂分发** — `Create()` 在 `Renderer/VertexArray.cpp` 中 `switch (Renderer::GetAPI())`，OpenGL → `OpenGLVertexArray`，其余 API → `DMGE_CORE_ASSERT` 断言；游戏侧只持有 `DM::Ref<VertexArray>` 基类指针
- **资源语义** — `shared_ptr` 持有，便于在多个提交点共享同一几何

**OpenGL 后端（`Platform/OpenGL/OpenGLVertexArray.h/.cpp`）：**

```cpp
class DMGE_API OpenGLVertexArray : public VertexArray
{
public:
    OpenGLVertexArray();
    ~OpenGLVertexArray() override;   // glDeleteVertexArrays

    void Bind()   const override;   // glBindVertexArray(m_RendererID)
    void Unbind() const override;   // glBindVertexArray(0)

    void AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer) override;
    void SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer)   override;

    const std::vector<DM::Ref<VertexBuffer>>& GetVertexBuffers() const override;
    const DM::Ref<IndexBuffer>& GetIndexBuffer() const override;

private:
    uint32_t m_RendererID = 0;
    uint32_t m_VertexBufferIndex = 0;   // 下一个可用属性槽位
    std::vector<DM::Ref<VertexBuffer>> m_VertexBuffers;
    DM::Ref<IndexBuffer> m_IndexBuffer;
};
```

- **VAO 生命周期** — 构造 `glGenVertexArrays`，析构 `glDeleteVertexArrays`，`m_RendererID` 标识一个 `GL_VERTEX_ARRAY_OBJECT`
- **AddVertexBuffer** — 绑定 VAO + VBO 后遍历 `BufferLayout` 各 `BufferElement`，按 `ShaderDataType` 选择 `glVertexAttribPointer`（Float 系）或 `glVertexAttribIPointer`（Int/Bool 系），启用属性槽 `m_VertexBufferIndex` 并自增；矩阵（Mat3/Mat4）拆成多列分别挂载并设 `glVertexAttribDivisor(.., 1)`；最后断言布局非空并保存 VBO
- **SetIndexBuffer** — 绑定 VAO 后绑定 IBO（`GL_ELEMENT_ARRAY_BUFFER`），VAO 随之记录该绑定并保存
- **DrawIndexed 协作** — `OpenGLRendererAPI::DrawIndexed` 绑定该 VAO 后 `glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr)`，顶点属性与索引绑定均由 VAO 状态提供

**使用示例：**

```cpp
auto vertexArray = DMGameEngine::VertexArray::Create();

float vertices[] = { /* ... */ };
auto vbo = DMGameEngine::VertexBuffer::Create(vertices, sizeof(vertices));
vbo->SetLayout({ { DMGameEngine::ShaderDataType::Float3, "a_Position" } });
vertexArray->AddVertexBuffer(vbo);

uint32_t indices[] = { 0, 1, 2 };
auto ibo = DMGameEngine::IndexBuffer::Create(indices, 3);
vertexArray->SetIndexBuffer(ibo);

// Submit 内部绑定 Shader 与 VAO，并上传 u_ViewProjection / u_Transform
DMGameEngine::Renderer::Submit(shader, vertexArray);
```

---

### Texture 系统（`Renderer/Texture.h` + `Renderer/Texture2D.h/.cpp` + `Renderer/TextureCube.h/.cpp` + `Renderer/Texture2DArray.h/.cpp` + `Platform/OpenGL/OpenGLTexture*.h/.cpp`）

纹理按「类型」分层：`Texture` 为纯抽象基类（header-only），仅暴露 `Bind/Unbind/GetWidth/GetHeight/GetRendererID` + `operator==`；各具体纹理类型派生自它并自带专属 `Specification` 与 `Create()` 工厂。`TextureFormat` / `TextureFilter` / `TextureWrap` 三个枚举定义于 `Texture.h`，三种类型共享。

**Texture 基类（`Renderer/Texture.h`，header-only）：**

```cpp
enum class TextureFormat : uint8_t { None, R8, RG8, RGB8, RGBA8, R16F, RG16F, RGB16F, RGBA16F,
                                     R32F, RG32F, RGB32F, RGBA32F, Depth, DepthStencil };
enum class TextureFilter : uint8_t { None, Nearest, Linear };
enum class TextureWrap   : uint8_t { None, Repeat, ClampToEdge, ClampToBorder, MirroredRepeat };

class DMGE_API Texture
{
public:
    virtual ~Texture() = default;
    virtual uint32_t GetWidth()      const = 0;
    virtual uint32_t GetHeight()     const = 0;
    virtual uint32_t GetRendererID() const = 0;
    virtual void Bind(uint32_t slot = 0) const = 0;
    virtual void Unbind()                 const = 0;
    virtual bool operator==(const Texture& other) const
    { return GetRendererID() == other.GetRendererID(); }

    // 从基层级重新生成 mipmap 链；存储未分配 mip 层时为空操作。
    virtual void GenerateMipmaps() = 0;
};
```

**三种具体类型（各自 `: public Texture`）：**
- **Texture2D**（`Renderer/Texture2D.h/.cpp`）- 2D 纹理，`Texture2DSpecification`（Width/Height/Format/Min/MagFilter/WrapS/WrapT/GenerateMipmaps）；`SetData(void* data, uint32_t size)` 上传整张；`Create(spec)` / `Create(filepath)` 工厂（文件加载经 stb_image 强制 RGBA8 + 垂直翻转）
- **TextureCube**（`Renderer/TextureCube.h/.cpp`）- 立方体贴图，6 个正方形面；`CubeFace` 枚举（Right/Left/Top/Bottom/Front/Back，值与 `GL_TEXTURE_CUBE_MAP_POSITIVE_X` 偏移对齐）、`CubeFaceCount=6`、`TextureCubeSpecification`（Size/Format/...，WrapR/S/T 默认 `ClampToEdge` 避免接缝）；`SetData(data, size, face)` 按面上传；`Create(spec)` / `Create({右,左,上,下,前,后})` 工厂
- **Texture2DArray**（`Renderer/Texture2DArray.h/.cpp`）- 2D 纹理数组，单 GPU 资源持多层同尺寸 2D，着色器以 `(u, v, layer)` 寻址；`Texture2DArraySpecification`（Width/Height/Layers/...，含 WrapR）；`SetData(data, size, layer)` 按层上传、`GetLayerCount()`；`Create(spec)` 工厂

**设计要点：**
- **工厂下放** - 原 `Texture::Create()` 拆分到各子类（`Texture2D::Create` / `TextureCube::Create` / `Texture2DArray::Create`），各自 `switch (Renderer::GetAPI())` 分发；基类不再持工厂与 `Specification`，2D / 立方体 / 数组的尺寸、面、层语义相互隔离
- **SetData 类型相关** - 2D 传 `(data, size)`、立方体传 `(data, size, face)`、数组传 `(data, size, layer)`，故 `SetData` 下放到子类而非基类
- **资源语义** - `DM::Ref<TextureXxx>` 持有，`operator==` 比对 `GetRendererID()`
- **不可变存储 + 子区域更新** - OpenGL 后端 `Invalidate()` 用 `glTexStorage2D/3D` 一次性分配不可变存储（`GenerateMipmaps` 为真时预留全部 mip 层）；`SetData` 经 `glTexSubImage2D/3D` 仅写入已分配存储、不重指定/重分配，适合视频帧、动态小地图、Canvas 等高频更新
- **mipmap 解耦** - `SetData` 不再自动重算 mipmap；新增基类 `GenerateMipmaps()` 显式重生成（文件加载构造在首张上传后调一次）；流式更新可关闭 `GenerateMipmaps` 规避每帧 mipmap 开销

**GL 枚举映射共享工具（`Platform/OpenGL/OpenGLTextureUtils.h`，header-only inline）：**
- 原 `OpenGLTexture.cpp` 中的 static 成员（`TextureFormatToGLInternal/Data/Type`、`TextureFilterToGL`、`TextureWrapToGL`、`FormatChannels`、`MipLevelCount`）抽为 `DMGameEngine::Detail` 内联自由函数，三个 OpenGL 后端共用，避免重复；`MipLevelCount(w, h)` 按 `floor(log2(max(w,h)))+1` 计算不可变存储的 mip 层数

**OpenGL 后端：**
- **OpenGLTexture2D**（`Platform/OpenGL/OpenGLTexture2D.h/.cpp`）- 由原 `OpenGLTexture` 重命名而来，`GL_TEXTURE_2D`；`Invalidate()` 建 tex + 设过滤/包裹 + `glTexStorage2D` 分配不可变存储（`GenerateMipmaps` 为真时预留全部 mip 层）；`SetData` 经 `glTexSubImage2D` 更新基层级（不重指定存储、不重算 mipmap）；文件加载构造上传后调一次 `GenerateMipmaps()`
- **OpenGLTextureCube**（`Platform/OpenGL/OpenGLTextureCube.h/.cpp`）- `GL_TEXTURE_CUBE_MAP`；`Invalidate()` 经 `glTexStorage2D` 一次分配 6 面 + mip 链；`SetData(face)` 经 `glTexSubImage2D` 更新单面；文件加载构造按首面尺寸分配后逐面 `SetData`，末尾调一次 `GenerateMipmaps()`（立方体面不翻转）
- **OpenGLTexture2DArray**（`Platform/OpenGL/OpenGLTexture2DArray.h/.cpp`）- `GL_TEXTURE_2D_ARRAY`；`Invalidate()` 经 `glTexStorage3D` 分配 `Width×Height×Layers` 不可变存储 + mip 链；`SetData(layer)` 经 `glTexSubImage3D`（depth=1）更新单层；无文件加载构造，需显式 `GenerateMipmaps()`
- 三个后端统一经 `DMGE_GL_CALL` 包裹 GL 调用

**使用示例：**

```cpp
// 2D（文件 / 规格）
auto tex2d = DMGameEngine::Texture2D::Create("assets/textures/brick.png");
DM::Ref<DMGameEngine::Texture2D> rt =
    DMGameEngine::Texture2D::Create({ 1024, 1024, DMGameEngine::TextureFormat::RGBA16F });

// 立方体贴图（天空盒）
auto skybox = DMGameEngine::TextureCube::Create({
    "assets/skybox/right.jpg", "assets/skybox/left.jpg",
    "assets/skybox/top.jpg",   "assets/skybox/bottom.jpg",
    "assets/skybox/front.jpg", "assets/skybox/back.jpg" });

// 2D 数组
DMGameEngine::Texture2DArraySpecification spec{ 512, 512, 8 };
spec.Format = DMGameEngine::TextureFormat::RGBA8;
auto array = DMGameEngine::Texture2DArray::Create(spec);
array->SetData(layerPixels, layerPixelsSize, 0);   // 上传第 0 层
```

---
### 3.12 RendererAPI — 渲染后端抽象

所有底层图形 API 调用（glDrawElements 等）被封装在 `RendererAPI` 纯虚接口之后，由 `RenderCommand` 静态门面持有后端实例并转发命令（见 3.13），`Renderer` 不再直接持有或调用后端。

```cpp
class DMGE_API RendererAPI {
public:
    virtual ~RendererAPI() = default;

    virtual void Init(const RendererAPIInitConfig& config = {});
    virtual void SetClearColor(const glm::vec4& color) = 0;
    virtual void Clear() = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    virtual void DrawIndexed(const VertexArray& vertexArray) = 0;

    static DM::Scope<RendererAPI> Create();
};
```

**工厂分发：** `Create()` 按 `Renderer::GetAPI()` 分发 — OpenGL → `OpenGLRendererAPI`，其余 API → `DMGE_CORE_ASSERT` 断言。

**OpenGLRendererAPI 实现：**
- `Init(config)`：基类 `RendererAPI::Init` 模板方法，按 `RendererAPIInitConfig` 经各虚 setter 应用初始管线状态；`OpenGLRendererAPI` 继承该实现，不再重写
- `SetClearColor(color)`：`glClearColor(color.r, color.g, color.b, color.a)`
- `Clear()`：`glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)` 清颜色+深度缓存
- `DrawIndexed()`：绑定 VAO → 断言 IndexBuffer → `glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr)`

**Init 可配置化：** `RendererAPI::Init(const RendererAPIInitConfig& config = {})` 为模板方法，按 config 经各虚 setter（`SetClearColor` / `SetDepthTest` / `SetDepthFunc` / `SetCullMode` / `SetBlendState` / `SetBlendEquation`）应用初始管线状态，由后端派发到各自 API 调用。`RendererAPIInitConfig`（`RendererAPI.h`）聚合清屏色、深度测试开关与比较函数、面剔除模式、混合开关与源/目标因子及方程，默认值对齐原硬编码基线（深度测试开 + `Less`、剔除关、混合关），无参调用行为不变。`Renderer::Init(config)` 转发至后端；`OpenGLRendererAPI` 不再重写 `Init`，直接继承基类实现。

### 3.13 RenderCommand / Renderer - 命令门面与场景层

渲染底层与场景之间拆为三层：`RendererAPI`（纯虚后端接口，见 3.12）-> `RenderCommand`（静态门面，拥有后端实例并转发原子 GPU 命令）-> `Renderer`（场景编排，缓存相机视图投影、提交绘制）。`Renderer` 不再直接持有 `RendererAPI` 实例，所有底层调用经 `RenderCommand` 派发，使场景无关代码（ImGui 层、调试渲染器）可不经 `Renderer` 直接发命令。

**RenderCommand（`Renderer/RenderCommand.h / .cpp`）** - 静态门面，持有 `static DM::Scope<RendererAPI> s_RendererAPI`：

| 方法 | 职责 |
|---|---|
| `Init(config = {})` | `RendererAPI::Create()` 创建后端（按 `Renderer::GetAPI()` 分发）-> 断言非空 -> `s_RendererAPI->Init(config)`（按 `RendererAPIInitConfig` 应用初始管线状态） |
| `Shutdown()` | `s_RendererAPI.reset()` 销毁后端 |
| `SetClearColor` / `Clear` / `SetViewport` / `DrawIndexed` | 转发至后端，各带初始化守卫 assert |
| `SetBlendState` / `SetBlendEquation` / `SetDepthTest` / `SetDepthFunc` / `SetCullMode` | 转发管线状态至后端 |

**Renderer（`Renderer/Renderer.h / .cpp`）** - 静态场景编排层，缓存 `SceneData::ViewProjectionMatrix`：

| 方法 | 职责 |
|---|---|
| `Init(config = {})` | `RenderCommand::Init(config)`；记录当前 API 名称日志 |
| `Shutdown()` | `RenderCommand::Shutdown()` |
| `BeginScene()` | `RenderCommand::Clear()` 清屏，场景视图投影重置为 identity |
| `BeginScene(camera)` | `RenderCommand::Clear()` 并缓存 `camera.GetViewProjection()` 至 `SceneData`；Vulkan 下左乘 `flipY` 翻转 clip-space Y |
| `EndScene()` | 帧结束占位（预留交换/提交） |
| `Submit(shader, vertexArray, transform)` | 绑定 Shader -> 写入 `u_ViewProjection`/`u_Transform` -> `RenderCommand::DrawIndexed(*vertexArray)` |
| `Submit(material, vertexArray, transform)` | 绑定 Material（其 Shader + 存储 uniform）-> 经 `GetShader()` 写入 `u_ViewProjection`/`u_Transform` -> `RenderCommand::DrawIndexed` |
| `OnWindowResize(w, h)` | `RenderCommand::SetViewport(0, 0, w, h)`；`WindowResizeEvent` 时由 `Application::OnEvent` 调用 |
| `GetAPI()` / `SetAPI(api)` | 读取/设置当前图形 API 枚举（默认 `OpenGL`），`RenderCommand::Init` 经 `RendererAPI::Create()` 据此选后端 |

**生命周期集成（Application）：**
- `Initialize()`：建窗 -> 设回调 -> **`Renderer::Init()`** -> **`Renderer::OnWindowResize(w, h)`**（同步初始视口）-> `OnInitialize()`
- `Shutdown()`：`OnShutdown()` -> **`Renderer::Shutdown()`** -> 销毁窗口
- `MainLoop()` 渲染阶段：**`Renderer::BeginScene()`**（清屏；若设了激活相机则 `BeginScene(*camera)` 缓存其视图投影，否则用 identity）-> 层 `OnRender()` / `OnRender()` 提交 -> **`Renderer::EndScene()`**
- `OnEvent()`：收到 **`WindowResizeEvent`** 时调用 **`Renderer::OnWindowResize(w, h)`** 更新视口（在层传播之前分发，不可被层抑制），并经 `m_ActiveCamera->OnViewportResize()` 同步激活相机宽高比（若已设相机）
- 时序保证：`Renderer::Init()` 调用时 `OpenGLGraphicsContext::Init()` 已完成 GLAD 加载

**设计要点：**
- 三层职责分离：`RendererAPI`=纯虚接口、`RenderCommand`=命令门面（拥有后端实例、转发原子命令）、`Renderer`=场景编排；`Renderer` 不再 `#include` 后端、不再直接调 `RendererAPI`
- `Renderer.h` 仅需 `<memory>`、`glm/glm.hpp`、`RendererAPI.h`（`RendererAPIInitConfig`）及前向声明 `Camera`/`Shader`/`Material`/`VertexArray`
- 现有 `IndexBuffer` / `VertexBuffer` / `Shader` / `Texture` 资源工厂模式保持不变

### 3.14 Camera — 相机基类

`Camera` 是所有相机类型的共同基类，同时持有投影矩阵与视图矩阵（以及缓存的 `projection * view` 乘积），将世界空间映射到裁剪空间。投影矩阵由派生类通过 `SetProjection()` 写入（如 `glm::ortho` / `glm::perspective`），视图矩阵由派生类通过 `SetView()` 写入（如 `glm::lookAt` 或相机世界变换的逆）；任一矩阵更新都会自动重算缓存，渲染器经 `GetViewProjection()` 读取以提交 view-projection uniform。

```cpp
class DMGE_API Camera
{
public:
    Camera() = default;
    explicit Camera(const glm::mat4& projection)
        : m_Projection(projection)
    {
        RecalculateViewProjection();
    }

    virtual ~Camera() = default;

    // 视口 resize 钩子（默认 no-op）：派生类按需重写以刷新宽高比相关投影
    virtual void OnViewportResize(uint32_t width, uint32_t height) {}

    const glm::mat4& GetProjection() const { return m_Projection; }
    void SetProjection(const glm::mat4& projection)
    {
        m_Projection = projection;
        RecalculateViewProjection();
    }

    const glm::mat4& GetView() const { return m_View; }
    void SetView(const glm::mat4& view)
    {
        m_View = view;
        RecalculateViewProjection();
    }

    const glm::mat4& GetViewProjection() const { return m_ViewProjection; }

protected:
    void RecalculateViewProjection()
    {
        m_ViewProjection = m_Projection * m_View;
    }

    glm::mat4 m_Projection     = glm::mat4(1.0f);
    glm::mat4 m_View           = glm::mat4(1.0f);
    glm::mat4 m_ViewProjection  = glm::mat4(1.0f);
};
```

**设计要点：**
- **header-only** — 仅 inline 访问器，无 `.cpp`、无工厂；非后端抽象，与平台无关
- **投影 + 视图双矩阵** — 基类同时存储 `m_Projection` / `m_View` 及缓存 `m_ViewProjection = m_Projection * m_View`（列主序：clip = P·V·World）；`SetProjection()` / `SetView()` 均自动触发重算
- **视图由派生类负责** — 基类仅提供 `SetView()` 通道；具体视图（位置 / 旋转 / 目标驱动）由派生类型按需计算后写入
- **默认单位矩阵** — 三个矩阵均初始化为 `glm::mat4(1.0f)`，避免未初始化读取
- **已纳入公共 API** — `DMGameEngine.h` 与 `CMakeLists.txt`（`DMGE_HEADERS`）已登记
- **视口 resize 钩子** — `OnViewportResize(w, h)` 虚方法（默认 no-op）供宿主在 `WindowResizeEvent` 时回调；`PerspectiveCamera`/`SceneCamera` 重写以刷新宽高比投影，`OrthographicCamera` 用显式边界保持 no-op

### 3.15 OrthographicCamera — 正交相机

`OrthographicCamera`（`Renderer/OrthographicCamera.h`，header-only）派生自 `Camera`，持有正交投影（left / right / bottom / top / near / far）与一个 2D 相机变换（位置 + 绕 Z 轴的 roll 角）。视图矩阵取相机世界变换的逆（`glm::translate`·`glm::rotate` 后 `glm::inverse`）。修改任一参数即时重算受影响矩阵。

```cpp
class DMGE_API OrthographicCamera : public Camera
{
public:
    OrthographicCamera(float left, float right, float bottom, float top,
                       float nearClip = -1.0f, float farClip = 1.0f);

    void SetProjection(float left, float right, float bottom, float top,
                       float nearClip = -1.0f, float farClip = 1.0f);

    void SetPosition(const glm::vec3& position);
    const glm::vec3& GetPosition() const;
    void SetRotation(float rotation);   // roll about Z, degrees
    float GetRotation() const;
};
```

**设计要点：**
- **2D 友好** — 位置 + 单一 roll 角即可描述 2D / HUD / 等距相机；视图 = `inverse(translate(position) · rotate(z, roll))`
- **投影可热改** — `SetProjection(L,R,B,T,n,f)` 重算投影；`SetPosition` / `SetRotation` 仅重算视图
- **header-only** — 仅 inline 实现，依赖 `glm/gtc/matrix_transform.hpp` 与 `glm/gtc/matrix_inverse.hpp`

### 3.16 PerspectiveCamera — 透视相机

`PerspectiveCamera`（`Renderer/PerspectiveCamera.h`，header-only）派生自 `Camera`，持有透视投影（垂直 FOV / 宽高比 / near / far）与 look-at 相机变换（位置 / 目标点 / 上方向）。视图矩阵由 `glm::lookAt` 计算。提供 `SetAspectRatio()` 仅更新宽高比并重算投影；另提供 `SetViewportSize(w, h)` 由像素尺寸安全换算宽高比（零尺寸忽略），`OnViewportResize()` 重写委托 `SetViewportSize`，窗口缩放时由 `Application::OnEvent` 经基类虚方法回调同步。

```cpp
class DMGE_API PerspectiveCamera : public Camera
{
public:
    PerspectiveCamera(float fov, float aspectRatio,
                      float nearClip = 0.1f, float farClip = 1000.0f);

    void SetProjection(float fov, float aspectRatio, float nearClip, float farClip);
    void SetAspectRatio(float aspectRatio);   // 保持 FOV/near/far，仅改宽高比
    float GetAspectRatio() const;
    void SetViewportSize(uint32_t width, uint32_t height);  // 由像素尺寸算宽高比（零尺寸忽略）
    void OnViewportResize(uint32_t width, uint32_t height) override;  // resize 钩子 -> SetViewportSize

    void SetPosition(const glm::vec3& position);
    const glm::vec3& GetPosition() const;
    void SetTarget(const glm::vec3& target);
    const glm::vec3& GetTarget() const;
    void SetUp(const glm::vec3& up);
    const glm::vec3& GetUp() const;
};
```

**设计要点：**
- **3D 友好** — 位置 + 目标 + 上方向 → `glm::lookAt`；FOV 以度数表示（内部 glm::radians 换算）
- **宽高比可改** — `SetAspectRatio()` 保留 FOV / near / far 仅刷新比例；`SetViewportSize(w, h)` 由像素尺寸安全换算（零尺寸忽略），`OnViewportResize()` 重写委托 `SetViewportSize`，窗口缩放同步走单一链路
- **header-only** — 仅 inline 实现，依赖 `glm/gtc/matrix_transform.hpp`

### 3.17 SceneCamera — 运行时可切换相机

`SceneCamera`（`Scene/SceneCamera.h`，header-only）派生自 `Camera`，同时维护正交与透视两套投影参数，通过 `ProjectionType` 枚举在运行时切换。单一宽高比驱动两种投影，`SetViewportSize()` / `SetAspectRatio()` 一次调用即可让两种投影都保持正确。视图矩阵不由本类管理——它面向场景图驱动（如 TransformComponent），由外部经继承的 `SetView()` 写入。

```cpp
class DMGE_API SceneCamera : public Camera
{
public:
    enum class ProjectionType : uint8_t { Perspective = 0, Orthographic = 1 };

    SceneCamera();
    ~SceneCamera() override = default;

    ProjectionType GetProjectionType() const;
    void SetProjectionType(ProjectionType type);   // 切换并重算投影

    // 透视参数（FOV 单位为度数，内部 glm::radians 换算）
    float GetPerspectiveVerticalFOV() const;  void SetPerspectiveVerticalFOV(float);
    float GetPerspectiveNearClip() const;     void SetPerspectiveNearClip(float);
    float GetPerspectiveFarClip() const;      void SetPerspectiveFarClip(float);

    // 正交参数（size 为可见高度，宽度 = size * 宽高比）
    float GetOrthographicSize() const;        void SetOrthographicSize(float);
    float GetOrthographicNearClip() const;   void SetOrthographicNearClip(float);
    float GetOrthographicFarClip() const;    void SetOrthographicFarClip(float);

    // 宽高比 / 视口
    float GetAspectRatio() const;            void SetAspectRatio(float);
    void SetViewportSize(uint32_t width, uint32_t height);   // 零尺寸忽略，防 NaN
    void OnViewportResize(uint32_t width, uint32_t height) override;  // resize 钩子 -> SetViewportSize
};
```

**设计要点：**
- **运行时切换** — `SetProjectionType()` 在 `Perspective` / `Orthographic` 间翻转并即时重算 `m_Projection`
- **单一宽高比** — `SetViewportSize(w,h)` / `SetAspectRatio()` 同时驱动两种投影，窗口缩放无需逐投影重配；`OnViewportResize()` 重写委托 `SetViewportSize`，与 `PerspectiveCamera` 调用链统一
- **正交尺寸语义** — `OrthographicSize` 为可见高度（世界单位），水平范围由 `size * aspectRatio` 推导
- **正交裁剪 3D 友好** — 正交 near/far 默认与透视一致（`0.01 / 1000`），3D 场景切到正交不丢远物；区别于 2D 取向的独立 `OrthographicCamera`（默认 `-1 / 1`）
- **零尺寸保护** — `SetViewportSize(0,0)` 静默忽略，避免 `glm::perspective` 产生 NaN
- **视图外部驱动** — 视图矩阵由场景系统经继承的 `SetView()` 注入，基类不参与相机变换计算
- **header-only** — 仅 inline 实现，依赖 `glm/gtc/matrix_transform.hpp`

---

### 3.18 ImGuiLayer — Dear ImGui 集成层

`ImGuiLayer`（`ImGui/ImGuiLayer.h` / `.cpp`）派生自 `Layer`（`LayerType::Tool`），拥有 Dear ImGui 上下文并驱动每帧 UI 通道，作为编辑器与调试 UI 的基础依赖。由 `Application` 在 `Initialize()` 中作为 overlay 自动挂载。

**职责：**
- **生命周期** — `OnAttach()`：`ImGui::CreateContext()` + `ImGui_ImplGlfw_InitForOpenGL(window, true)` + `ImGui_ImplOpenGL3_Init()`；`OnDetach()`：逆序关闭后端并 `ImGui::DestroyContext()`
- **帧驱动** — `Begin()`（`NewFrame` 三件套）/ `End()`（`ImGui::Render` + `RenderDrawData`），由 `Application::MainLoop` Stage 4 在 `OnImGuiRender()` 通道前后调用
- **输入捕获** — `OnEvent()` 在 `ImGuiIO::WantCaptureMouse` / `WantCaptureKeyboard` 为真时，按 `EventCategory::Mouse` / `Keyboard` 标记事件已处理，阻止向下游层传播
- **后端** — GLFW 后端安装自有回调并链式调用引擎既有回调（双方均收输入）；OpenGL3 后端使用 ImGui 自带 GL 加载器，与引擎 glad 并存
- **演示** — `ShowDemoWindow(bool)` 开关内置 demo（默认关闭）；`ImGuiConfigFlags_NavEnableKeyboard` 已启用

**版本：** Dear ImGui 1.92.6（docking 分支；multi-viewport 经 DMGE_IMGUI_VIEWPORTS 编译期开关启用，详见下方 Profiler 节）

**涉及文件：** `ImGui/ImGuiLayer.h`（新增）、`ImGui/ImGuiLayer.cpp`（新增）、`Core/Application.h`、`Core/Application.cpp`、`CMakeLists.txt`、`DMGameEngine.h`、`ENGINE_SUMMARY.md`

---

### Profiler - CPU 帧级性能分析器

自研轻量、零外部依赖的 CPU 帧级 profiler，由 `Profiler` 单例（`Debug/Profiler.h` / `.cpp`）与 `ProfilerLayer` ImGui overlay（`Debug/ProfilerLayer.h` / `.cpp`）组成，贴合现有 `Layer` / `ImGui` 架构。`Application::MainLoop` 五阶段各以 `DMGE_PROFILE_SCOPE` 插桩，帧首尾调 `Profiler::BeginFrame/EndFrame`。

**职责：**
- **插桩层** - `ProfilerScopeTimer`（RAII）经 `DMGE_PROFILE_SCOPE(name)` / `DMGE_PROFILE_FUNCTION()` 宏在作用域进出口打点；析构时以 `steady_clock` 纳秒记录 `{name, start, end, thread}` 推入 `Profiler`。宏受 `DMGE_PROFILE` 编译期开关（CMake `option(DMGE_PROFILE)`）控制，关闭时展开为空、零开销。
- **核心层** - `Profiler` 单例（Meyers，`Get()`）：`WriteResult` 经 mutex 保护写入 `m_pending`（可跨 worker 线程）；`EndFrame` 在锁内按 `const char*`（调用点字面量地址）聚合成 `count/total/min/max`，锁外按 Total 降序排序后发布到 `m_lastAggregates`；帧时间用指数移动平均算 `m_fps`，维护 240 帧滚动历史。
- **帧管理** - `BeginFrame`（Stage 1 前）/`EndFrame`（Stage 5 SwapBuffers 后）独立于 scope 采集：即便 `DMGE_PROFILE=OFF`（scope 表为空），FPS 与帧时间图仍由这两者直接驱动、始终可用。
- **绘制调用统计** - `Profiler` 维护每帧 draw call / index 计数：`BeginFrame` 清零累计器 `m_drawCalls` / `m_drawIndices`，`RenderCommand::DrawIndexed` 每次调用经 `AddDrawCall(indexCount)` 累加（index 数取自 `VertexArray::GetIndexBuffer()->GetCount()`），`EndFrame` 发布到 `m_lastDrawCalls` / `m_lastDrawIndices` 供 UI 读取。仅统计经 `RenderCommand::DrawIndexed` 的场景绘制，ImGui 经自身后端绘制不计入；不随 `DMGE_PROFILE` 开关（默认启用，可经 `m_drawCallTracking` / Console `stat draws` 启停），单线程渲染无需原子。
- **呈现层** - `ProfilerLayer`（`LayerType::Tool` overlay，`Application::Initialize` 自动挂载）只读 `Profiler`：`OnImGuiRender` 绘制 FPS / 帧时间折线图（`PlotLines`，y 轴自适应）/ 每帧 Draw Calls + Index 计数（`Draws` / `Indices` 文本）/ per-scope 表格（`BeginTable`，Scope+Calls+Total+Mean+Min+Max，按 Total 降序）。`OnEvent` 监听 F1（`KeyPressedEvent`，过滤 auto-repeat）切换显隐，不消费事件。

**设计：** 三层解耦（插桩宏 / 数据管线 / UI），`ProfilerLayer` 崩溃不影响计时、换数据结构不影响调用点。`const char*` 作 map key = 按调用点聚合、零字符串拷贝；锁内只做读+清空、排序在锁外，临界区最小。mutex 而非 thread_local：规避 DLL 边界 thread_local 风险，单线程渲染下无竞争、worker 线程低频写入语义正确。`DMGE_PROFILE_SCOPE` 为唯一入口，后端可切换（换宏体即可接 Tracy，调用点零改动）。GPU 计时不在范围内（CPU-only；已新增 draw call / index 计数，系 CPU 端提交量聚合、非 GPU 侧计时）。

**多视口集成：** profiler 窗口经 ImGui multi-viewport（`ImGuiConfigFlags_ViewportsEnable`）可拖出主窗口成独立 OS 平台窗口。该能力受 `DMGE_IMGUI_VIEWPORTS` 编译期开关控制--CMake `file(STRINGS)` 自动检测 `imgui.h` 是否含 `ImGuiConfigFlags_ViewportsEnable`（即是否 docking 分支），仅在检测到时定义宏并编译 `ImGuiLayer` 的视口代码（避免 master 分支下"未声明标识符"编译失败）。`ImGuiLayer::End` 在主窗口 `RenderDrawData` 后调 `UpdatePlatformWindows` + `RenderPlatformWindowsDefault`（OpenGL 保存/恢复 GL context）。`file(STRINGS)` 不自带依赖，故 `CMakeLists.txt` 额外以 `CMAKE_CONFIGURE_DEPENDS` 指向 `imgui.h`，换 ImGui 分支后自动触发重新检测。

**涉及文件：** `Debug/Profiler.h`（新增）、`Debug/Profiler.cpp`（新增）、`Debug/ProfilerLayer.h`（新增）、`Debug/ProfilerLayer.cpp`（新增）、`Core/Application.cpp`、`ImGui/ImGuiLayer.cpp`、`CMakeLists.txt`、`DMGameEngine.h`、`ENGINE_SUMMARY.md`

---

### 3.19 OpenGLDebug — OpenGL 错误侦测宏 DMGE_GL_CALL

`OpenGLDebug.h`（`Platform/OpenGL/`，header-only）提供 Debug 专用的 OpenGL 错误检查宏 `DMGE_GL_CALL`，弥补 OpenGL 后端此前无错误侦测的问题。包裹一条 GL 调用后：先排空残留错误、执行调用、再以 `glGetError` 逐条核对；任一错误即以 ERROR 级日志打印错误码（符号名 + 十六进制 + 调用文本 + 文件:行号），并在**调用处**经 `DMGE_CORE_ASSERT` 触发断点。

**机制：**
- **`DMGE_GL_CALL(x)`** — Debug 下 `GLClearErrors() -> x -> DMGE_CORE_ASSERT(GLCheckErrors(#x, __FILE__, __LINE__), ...)`；Release（未定义 `DMGE_ENABLE_ASSERTS`）下直接展开为 `x`，零开销
- **`GLClearErrors()`** — 循环 `glGetError` 排空先前残留错误，确保本次检查只反映当前调用
- **`GLCheckErrors()`** — 逐条读取错误，`GLDecodeError()` 转符号名（`GL_INVALID_ENUM` 等），返回是否无错；断言置于宏内使 `__debugbreak()` 落在 `DMGE_GL_CALL` 调用处

**使用约定：**
- 仅包裹 fire-and-forget 的 void 调用（`glBind*` / `glDraw*` / `glBufferData` / `glTexParameteri` / `glUniform*` 等）
- 不包裹返回值被使用的调用：`glCreate*` / `glGetString` / `glGetUniformLocation`，及状态查询 `glGetShaderiv` / `glGetProgramiv` / `glGet*InfoLog`
- 已应用于全部 6 个 OpenGL 后端 .cpp（共 70 处）；`OpenGLGraphicsContext` 无 fire-and-forget 调用故未引入

**涉及文件：** `Platform/OpenGL/OpenGLDebug.h`（新增）、`Platform/OpenGL/OpenGL*.cpp`（6 个）、`CMakeLists.txt`、`ENGINE_SUMMARY.md`

---

## 4. 已处理的 IDE / 编译问题

| 问题 | 解决方案 |
|---|---|
| CLion 报 `class DMGE_API` 未展开 | Export.h 添加 `#else` 空定义回退 + Reload CMake |
| `INTERFACE_INCLUDE_DIRECTORIES contains source path` | 保持 `$<BUILD_INTERFACE:...>` 不直接用裸路径 |
| 捆绑 MinGW g++.exe 损坏 | 切换到 VS2026 工具链（修改 `toolchains.xml`：`C:\Program Files\Microsoft Visual Studio\18\Community`） |
| spdlog 编译报 `C2338: /utf-8` | CMakeLists.txt MSVC 编译选项添加 `/utf-8` |
| LNK1104: 无法打开 kernel32.lib | CMake 在非 VS 开发环境下配置导致 LIB/INCLUDE 路径缺失 → 删除 `cmake-build-debug`，**在 CLion 中用 `Ctrl+F9` 构建**（内置构建会自动初始化 VS 环境），或手动在 vcvars64.bat 下 `cmake -B ...`。**不要用终端跑 cmake/ninja** |
| C2280: dllexport 类含 unique_ptr 成员导致隐式拷贝构造/赋值被删除 | LayerStack.h 显式 `= delete` 拷贝构造和拷贝赋值运算符 |
| LNK2019: Layer::Layer 构造函数未定义 | 创建 `Layer.cpp` 实现构造函数（GLOB_RECURSE 后需重新 cmake configure） |
| LNK2001: Renderer::s_API 未解析的外部符号 | `Renderer.h` 声明了 `static API s_API` 但缺少 `.cpp` 定义 → 创建 `Renderer.cpp`，定义 `Renderer::API Renderer::s_API = Renderer::API::OpenGL` 并提供所有静态方法 stub 实现 |
| 新增 .cpp 后 CMake 未识别（GLOB_RECURSE 缓存） | `engine/CMakeLists.txt` 和 `game/CMakeLists.txt` 已改为 **显式列出源文件**（`set(DMGE_SOURCES ...)` / `set(GAME_SOURCES ...)`），不再使用 `file(GLOB_RECURSE ...)`。新增文件时需手动添加到对应列表中 |
| C2039: IsHandled 不是 Event 的成员 | `Application.cpp:76` 将 `e.IsHandled()` 改为 `e.Handled`（Event 使用公开成员而非方法） |
| `CMAKE_C_COMPILE_OBJECT` not set | `project()` 中 `LANGUAGES CXX` → `LANGUAGES C CXX`，启用 C 编译器以编译 glad.c |
| C1189: OpenGL header already included | `GLFW/glfw3.h` 默认包含系统 OpenGL 头文件与 glad 冲突 → include glad 前定义 `GLFW_INCLUDE_NONE` |
| C2065/C3861: KeyPressedEvent 等事件类型未声明 | `GLFWInput.cpp` 缺少 `Core/Events/KeyEvent.h` 和 `Core/Events/MouseEvent.h` → 补充 include |
| CMake Error: install(EXPORT) 要求 glm 在导出集合中 | `target_link_libraries(DMGameEngine PUBLIC glm)` 后 `install(EXPORT ...)` 报错 → 将 install 规则用 `if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)` 包裹，仅在 engine 为顶层项目时执行 |
| game 目标编译报 `fatal error C1083: glm/glm.hpp: No such file or directory` | `target_link_libraries(DMGameEngine PRIVATE glm)` 导致 GLM include 路径未传播到下游 → 改为 `PUBLIC`（Shader.h 公共头文件暴露了 `glm::vec2/3/4/mat4` 类型） |
| CLion IntelliSense 报 "在搜索路径中无法找到目录'glm'" | CMake 配置正确但 CLion 索引缓存过期 → `File` → `Invalidate Caches...` → `Invalidate and Restart` |
| OpenGLGraphicsContext.cpp 使用未定义宏 `DMGE_CORE_ASSERT` / `DMGE_CORE_INFO` | Log.h 新增 `DMGE_CORE_ASSERT` 宏（`#ifdef DMGE_ENABLE_ASSERTS` → `__debugbreak()`）；CMakeLists.txt 添加 `$<$<CONFIG:Debug>:DMGE_ENABLE_ASSERTS>`；`.cpp` 中 `DMGE_CORE_INFO` → `DMGE_LOG_INFO` |
| `OpenGLGraphicsContext.cpp` 缺失 `GLFW_INCLUDE_NONE`，glad 与 GLFW 默认 OpenGL 头冲突 | 在 `#include <GLFW/glfw3.h>` 前补充 `#define GLFW_INCLUDE_NONE` |
| WindowsWindow 直接操作 GLAD/GLFW context，未通过 GraphicsContext 抽象 | 移除 `#include <glad/glad.h>` 和裸 `glfwMakeContextCurrent`/`gladLoadGLLoader`；改为创建 `OpenGLGraphicsContext` 并调用 `Init()`；`SwapBuffers()` 委托给 `m_context->SwapBuffers()` |
| 主循环缺少 SwapBuffers 调用，渲染结果未呈现 | `Window` 基类新增 `virtual SwapBuffers()`；`Application::MainLoop` 末尾新增 Stage 5 Swap → `m_window->SwapBuffers()` |
| OpenGLShader 编译失败时已编译 shader 和 program 泄漏，Release 模式下代码继续执行 | 编译失败分支新增 `glDeleteShader` 清理所有已编译 shader + `glDeleteProgram` + `return`；链接失败分支同样加 `return` 防止 fallthrough |
| 关闭窗口后进程不退出（打印 `DemoGame shutting down` 后挂起） | `EntryPoint.h` 的 `main` 在 `app->Run()` 返回后才 `delete app`，而 `Run()` 内 `Application::Shutdown()` 已 `m_window.reset()` -> `glfwTerminate` 销毁 GL 上下文；持有 GL 对象（VAO / Shader 等）的 `Application` 派生类成员要到 `delete app` 才析构，`glDelete*` 在已销毁上下文上执行导致挂起。修复：游戏侧 GL 资源须在 `OnShutdown()`（上下文仍存活）中显式 `reset()` 释放，与引擎在 `Shutdown()` 内手动 reset `m_layerStack` / `m_window` 的模式一致；勿依赖派生类析构释放 GL 对象（`game/src/main.cpp`） |
| `LNK2019`：游戏侧实例化相机（`DM::CreateRef<OrthographicCamera>`）时无法解析构造 / 析构（标记为 `dllimport`） | 4 个相机类（`Camera` / `OrthographicCamera` / `PerspectiveCamera` / `SceneCamera`）为 header-only 却标了 `DMGE_API`，消费侧展开为 `dllimport`，但这些类无 .cpp、从未编进 DLL，符号不存在 -> 链接失败（此前游戏未实例化相机，故未暴露）。修复：移除这 4 个 header-only 类的 `DMGE_API`（其代码直接编进各消费方，不应走 DLL 导入导出），并删除随之失效的 `#include "DMGameEngine/Core/Export.h"`；有 .cpp 的类仍保留 `DMGE_API` |

---

## 5. 游戏工程使用示例

**main.cpp（游戏侧）：**
```cpp
#include <DMGameEngine/DMGameEngine.h>
#include <DMGameEngine/Core/EntryPoint.h>

class MyGame : public DMGameEngine::Application {
public:
    MyGame() : Application({"My Game", 1920, 1080}) {}

    void OnInitialize() override {
        // 窗口已自动创建，可直接配置
        GetWindow().SetVSync(true);
    }

    void OnEvent(DMGameEngine::Event& e) override {
        DMGameEngine::EventDispatcher d(e);
        d.Dispatch<DMGameEngine::KeyPressedEvent>([](auto& e) {
            if (e.GetKeyCode() == DMGameEngine::KeyCode::Escape)
                /* quit */;
            return false;
        });
        // 保留基类 WindowCloseEvent → Quit() 的关闭行为
        DMGameEngine::Application::OnEvent(e);
    }

    void OnUpdate(Timestep ts) override { /* 游戏逻辑 */ }
    void OnRender() override          { /* 渲染 */ }
    void OnShutdown() override        { /* 清理 */ }
};

DMGameEngine::Application* DMGameEngine::CreateApplication() {
    return new MyGame();
}
```

---

## 6. 构建最佳实践

### ⚠️ 始终使用 CLion 内置构建，不要用终端

| 方式 | 操作 | 说明 |
|---|---|---|
| **CLion 内置构建（推荐）** | `Ctrl+F9` 或工具栏 🔨 Build 按钮 | 自动初始化 VS 开发环境（vcvars），无需手动设置 PATH/LIB/INCLUDE |
| **CMake 重新配置** | CMake 工具窗口 → Reload CMake Project（循环箭头） | 新增文件后需要 Reload 才能被 CMake 识别 |
| **终端 cmake/ninja** | ❌ 不推荐 | 终端缺少 VS 环境变量，导致 `LNK1104: kernel32.lib` / `C1083: stddef.h` 等错误 |

### 新增源文件的正确流程

1. 创建 `.cpp` / `.h` 文件
2. 在 `engine/CMakeLists.txt` 的 `set(DMGE_SOURCES ...)` 或 `set(DMGE_HEADERS ...)` 中添加路径条目
3. CMake 工具窗口 → **Reload CMake Project**
4. `Ctrl+F9` 构建

---

## 7. 后续工作建议

1. ~~**Application 集成 Window**~~ ✅ 已完成 — Application 自动创建/管理窗口生命周期，默认关闭→退出回调，主循环集成窗口事件轮询
2. ~~**Layer / LayerStack**~~ ✅ 已完成 — 分层架构基类 + 5 级 LayerType 枚举 + 层栈管理器，支持 Layer/Overlay 双轨管理
3. ~~**Input 系统**~~ ✅ 已完成 — 抽象基类 `Input` + Windows GLFW 轮询实现（`glfwGetKey`/`glfwGetMouseButton`/`glfwGetCursorPos`），通过 `Application::Get()` → `Window::GetNativeWindow()` → `GLFWwindow*` 链路获取输入
4. ~~**渲染器抽象**~~ ✅ 已完成 — `GraphicsContext` 抽象接口 + `Renderer` 高层 API + **`RendererAPI` 后端抽象**（pimpl 解耦，`DrawIndexed` 纯虚 + `Create()` 工厂，`OpenGLRendererAPI` OpenGL 后端）+ `OpenGLGraphicsContext` OpenGL 后端 + **Shader 系统**（基类 + 工厂分发 + OpenGL GLSL 后端）+ **Texture 系统**（基类 + 工厂分发 + OpenGL 后端）+ **VertexBuffer / IndexBuffer 系统**（基类 + 工厂分发 + OpenGL 后端）；`Renderer::Init/Shutdown` 已接入 `Application::Initialize/Shutdown` 生命周期，后续扩展 Vulkan/DX 后端；**清屏与深度测试已补全**（`RendererAPI` 新增 `Init/SetClearColor/Clear`，`OpenGLRendererAPI` 实现 `glEnable(GL_DEPTH_TEST)`/`glClearColor`/`glClear`，`Renderer::BeginScene()` 每帧清屏，详见第 8 节）
5. **ImGui 集成** — 编辑器/调试 UI
6. **CMake 配置优化** — 将 GLFW/spdlog 的 CMake 配置独立为 `cmake/Dependencies.cmake`

## 8. 变更记录

### 2026-07-05 — 渲染清屏与深度测试

为渲染后端抽象补全每帧清屏能力并开启深度测试，使 `glClear(GL_DEPTH_BUFFER_BIT)` 真正生效。

**新增接口（`RendererAPI`）：**
- `virtual void Init() {}` — 后端生命周期钩子，默认空实现
- `virtual void SetClearColor(const glm::vec4& color) = 0;` — 设定每帧清屏色
- `virtual void Clear() = 0;` — 清颜色 + 深度缓存

**OpenGL 实现（`OpenGLRendererAPI`）：**
- `Init()`：`glEnable(GL_DEPTH_TEST)`（`Renderer::Init()` 时上下文已就绪，安全调用）
- `SetClearColor()`：`glClearColor(color.r, color.g, color.b, color.a)`
- `Clear()`：`glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)`

**高层门面（`Renderer`）：**
- 新增静态 `SetClearColor` / `Clear`，委托后端并附 `s_RendererAPI` 非空断言
- `Init()` 创建后端后调用 `s_RendererAPI->Init()` 初始化后端状态
- `BeginScene()` 内部调 `Clear()`，每帧按设定颜色刷新屏幕并清深度缓存

**使用方式：**

```cpp
// Application::OnInitialize()（Renderer::Init() 之后）
Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });

// 每帧渲染前
Renderer::BeginScene();   // 自动清屏（颜色 + 深度）
Renderer::Submit(vertexArray);
Renderer::EndScene();
```

**涉及文件：** `Renderer/RendererAPI.h`、`Platform/OpenGL/OpenGLRendererAPI.h/.cpp`、`Renderer/Renderer.h/.cpp`

### 2026-07-05 — Renderer 执行接入主循环

将 Renderer 的每帧执行流程接入 `Application::MainLoop()` 渲染阶段，使清屏与帧边界由引擎统一驱动，不再依赖游戏侧在 `OnRender()` 中手动调用。

**变更（`Core/Application.cpp` — Stage 3 Render）：**
- 渲染阶段首部新增 `Renderer::BeginScene()`（内部 `Clear()`，按设定颜色清颜色+深度缓存）
- 层 `OnRender()` 与 `Application::OnRender()` 在两者之间提交绘制命令
- 渲染阶段尾部新增 `Renderer::EndScene()`（帧结束占位，预留交换/提交）
- 帧时序：`BeginScene` → 层渲染 → `EndScene` → Stage 4 ImGui → Stage 5 `SwapBuffers`

**影响：**
- 游戏侧 `OnRender()` 现在只需 `Renderer::Submit(vertexArray)`，无需手动 `BeginScene/EndScene`
- 默认清屏色为 OpenGL 默认 `(0,0,0,0)`，可在 `OnInitialize()` 中通过 `Renderer::SetClearColor(...)` 自定义

**涉及文件：** `Core/Application.cpp`、`ENGINE_SUMMARY.md`（主循环五阶段图、每帧驱动示例与生命周期集成段同步更新）

### 2026-07-05 — 实现 Renderer::OnWindowResize（视口同步）

补全窗口尺寸变化时的视口同步，使渲染目标区域随窗口 drawable 大小更新，避免拉伸/裁剪。

**新增接口（`RendererAPI`）：**
- `virtual void SetViewport(int x, int y, int width, int height) = 0;` — 设置渲染视口区域（纯虚）

**OpenGL 实现（`OpenGLRendererAPI`）：**
- `SetViewport()`：`glViewport(x, y, width, height)`

**高层门面（`Renderer`）：**
- `OnWindowResize(width, height)`：断言 `s_RendererAPI` 非空 → `s_RendererAPI->SetViewport(0, 0, width, height)`（全窗口视口）

**Application 接入：**
- `OnEvent()`：新增 `WindowResizeEvent` 分发 → `Renderer::OnWindowResize(w, h)`；在层传播之前分发，返回 `false` 不标记 handled，层仍可响应（如相机改宽高比）
- `Initialize()`：`Renderer::Init()` 之后调用 `Renderer::OnWindowResize(GetWidth, GetHeight)` 同步初始视口
- 类型安全：窗口尺寸为 `unsigned int`，调用处以 `static_cast<int>` 显式转换以契合 `/W4`

**涉及文件：** `Renderer/RendererAPI.h`、`Platform/OpenGL/OpenGLRendererAPI.h/.cpp`、`Renderer/Renderer.cpp`、`Core/Application.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-05 — 新增 Camera 相机基类

在 `Renderer/` 下新增相机基类 `Camera`，集中持有投影矩阵，作为后续 Orthographic / Perspective / SceneCamera 等相机类型的共同基类。

**新增（`Renderer/Camera.h`，header-only）：**
- `Camera` 基类：默认构造 / `explicit Camera(const glm::mat4& projection)` / 虚析构
- `GetProjection()` / `SetProjection()` inline 访问器
- `protected glm::mat4 m_Projection = glm::mat4(1.0f)`（默认单位矩阵）

**设计要点：**
- **header-only** — 仅 inline 访问器，无 `.cpp`、无工厂；非后端抽象，与图形 API 无关
- **投影由派生类计算** — 基类只存投影矩阵；视图矩阵与逐帧重算交由派生类型（如 `glm::ortho` / `glm::perspective` 计算后 `SetProjection`）
- **登记** — 已加入 `CMakeLists.txt`（`DMGE_HEADERS`）与公共头 `DMGameEngine.h`

**涉及文件：** `Renderer/Camera.h`（新增）、`CMakeLists.txt`、`DMGameEngine.h`、`ENGINE_SUMMARY.md`

### 2026-07-06 — 补全公共头 DMGameEngine.h 缺失的渲染类型

公共入口头 `DMGameEngine.h` 此前仅暴露 `Camera` / `GraphicsContext` / `Renderer` / `Shader`，缺失其余渲染资源抽象，游戏侧无法仅凭单头构建几何与材质。补全 Renderer 段，并加入新增相机派生类头。

**变更（`DMGameEngine.h` ─ Renderer 段）：**
- 新增 `VertexArray.h` / `VertexBuffer.h` / `IndexBuffer.h` / `Texture.h` / `RendererAPI.h`（此前已存在于 `Renderer/` 但未在公共头登记）
- 新增 `OrthographicCamera.h` / `PerspectiveCamera.h` / `SceneCamera.h`（随相机体系补全一并纳入公共 API）

**影响：** 游戏工程 `#include <DMGameEngine/DMGameEngine.h>` 后即可直接创建 VAO / VBO / IBO / Texture、提交 `VertexArray`，无需手动罗列各头

**涉及文件：** `DMGameEngine.h`、`ENGINE_SUMMARY.md`

### 2026-07-06 — Camera 体系补全：视图矩阵 + 正交 / 透视 / SceneCamera

`Camera` 基类此前仅持有投影矩阵，无视图矩阵，且无可改宽高比的能力。补全为完整相机体系：基类增视图矩阵与缓存，并派生正交、透视相机及运行时可切换的 SceneCamera。

**Camera 基类（`Renderer/Camera.h`，header-only）：**
- 新增 `m_View` 与缓存 `m_ViewProjection = m_Projection * m_View`（列主序 clip = P·V·World）
- 新增 `GetView()` / `SetView()` / `GetViewProjection()`；`SetProjection()` / `SetView()` 均自动触发 `RecalculateViewProjection()`
- 受保护 `RecalculateViewProjection()`；三个矩阵默认单位矩阵

**新增派生类（`Renderer/`，均 header-only）：**
- **OrthographicCamera** — 正交投影（L/R/B/T/near/far）+ 位置 + Z 轴 roll；视图 = `inverse(translate · rotate)`
- **PerspectiveCamera** — 透视投影（FOV / aspect / near / far）+ position / target / up；视图 = `glm::lookAt`；`SetAspectRatio()` 仅改比例并重算投影
- **SceneCamera** — `ProjectionType { Perspective, Orthographic }` 运行时切换；单一宽高比 `SetViewportSize()` / `SetAspectRatio()` 驱动两种投影；正交 `size` 为可见高度、水平范围 = `size * aspectRatio`；零尺寸静默忽略防 NaN；视图由场景系统经继承的 `SetView()` 注入

**登记：** 三个新头加入 `CMakeLists.txt`（`DMGE_HEADERS`）与公共头 `DMGameEngine.h`

**涉及文件：** `Renderer/Camera.h`、`Renderer/OrthographicCamera.h`（新增）、`Renderer/PerspectiveCamera.h`（新增）、`Renderer/SceneCamera.h`（新增）、`CMakeLists.txt`、`DMGameEngine.h`、`ENGINE_SUMMARY.md`

### 2026-07-06 — 补全 VertexArray / OpenGLVertexArray 文档

`VertexArray` 抽象与 `OpenGLVertexArray` 实现已存在，但 `ENGINE_SUMMARY.md` 中缺失对应说明，目录树亦未登记。补全文档。

**变更（`ENGINE_SUMMARY.md`）：**
- 目录树补登 `Renderer/VertexArray.h / .cpp` 与 `Platform/OpenGL/OpenGLVertexArray.h / .cpp`
- 新增「VertexArray 系统」章节：基类纯虚接口（`Bind` / `AddVertexBuffer` / `SetIndexBuffer` / `Create` 工厂）+ OpenGL 后端实现（VAO 生命周期、按 `ShaderDataType` 选择 `glVertexAttribPointer` / `glVertexAttribIPointer`、矩阵拆列 + `glVertexAttribDivisor`、与 `DrawIndexed` 协作）+ 使用示例

**涉及文件：** `ENGINE_SUMMARY.md`

### 2026-07-06 — SceneCamera 迁移至 Scene/ 目录

为后续场景相关类型（Entity、Component、Scene 等）预留专属目录，将运行时可切换相机 `SceneCamera` 从 `Renderer/` 迁移至新建的 `Scene/` 目录。`SceneCamera` 仍派生自 `Renderer/Camera`，仅文件归属变更，API 与行为不变。

**变更：**
- 新建目录 `src/DMGameEngine/Scene/`，`SceneCamera.h` 由 `Renderer/` 移入（`Renderer/SceneCamera.h` → `Scene/SceneCamera.h`）
- 公共头 `DMGameEngine.h`：`#include "DMGameEngine/Renderer/SceneCamera.h"` → `#include "DMGameEngine/Scene/SceneCamera.h"`，并新增独立的「Scene」段落
- `engine/CMakeLists.txt`：`DMGE_HEADERS` 中将该头由 `# Renderer` 段移出，新增 `# Scene` 段登记 `src/DMGameEngine/Scene/SceneCamera.h`
- `SceneCamera.h` 内部 include 为工程根相对路径（`DMGameEngine/Renderer/Camera.h` 等），移动后无需改动

**说明：** `target_include_directories` 公共根仍为 `src/`，`install(DIRECTORY src/ ... FILES_MATCHING *.h)` 自动覆盖新头，无额外安装规则改动。

**涉及文件：** `Scene/SceneCamera.h`（迁移）、`DMGameEngine.h`、`CMakeLists.txt`、`ENGINE_SUMMARY.md`

### 2026-07-06 — 新增 ImGuiLayer（Dear ImGui 集成层）

新增 `ImGuiLayer` 作为编辑器与调试 UI 的基础依赖，补全引擎的即时模式 UI 能力。`Application` 现自动挂载该 overlay，并在主循环 Stage 4 用 `Begin()` / `End()` 包裹 `LayerStack` 的 `OnImGuiRender()` 通道——此前该通道无 ImGui 上下文，调用 ImGui API 会崩溃。

**新增（`ImGui/ImGuiLayer.h` / `.cpp`）：**
- `ImGuiLayer : Layer`（`LayerType::Tool`）：`OnAttach`/`OnDetach` 管理 ImGui 上下文与 GLFW + OpenGL3 后端
- `Begin()` / `End()` 驱动每帧 ImGui 通道；`OnImGuiRender()` 提供可选 demo（`ShowDemoWindow`，默认关闭）
- `OnEvent()` 在 ImGui 捕获鼠标 / 键盘时按事件类别标记已处理，阻断下游传播

**Application 集成：**
- `Application` 新增 `ImGuiLayer* m_ImGuiLayer` 成员与 `GetImGuiLayer()` 访问器
- `Initialize()` 在窗口创建与渲染器初始化后、`OnInitialize()` 前以 overlay 挂载 `ImGuiLayer`
- `MainLoop` Stage 4：`m_ImGuiLayer->Begin()` → 各层 `OnImGuiRender()` → `m_ImGuiLayer->End()`
- `Shutdown()`：层栈销毁触发 `OnDetach`（关闭后端），随后置空 `m_ImGuiLayer`

**登记：** `ImGuiLayer.h` / `.cpp` 加入 `CMakeLists.txt`（`DMGE_SOURCES` / `DMGE_HEADERS`）；`DMGameEngine.h` 新增「ImGui」段

**说明：** ImGui 1.92.6（master 分支），GLFW 后端链式调用引擎既有 GLFW 回调；OpenGL3 后端使用自带 GL 加载器，与引擎 glad 并存，无需额外配置

**涉及文件：** `ImGui/ImGuiLayer.h`（新增）、`ImGui/ImGuiLayer.cpp`（新增）、`Core/Application.h`、`Core/Application.cpp`、`CMakeLists.txt`、`DMGameEngine.h`、`ENGINE_SUMMARY.md`

### 2026-07-07 — SceneCamera 正交模式 near/far 默认值改为 3D 友好

`SceneCamera` 正交投影的 near/far 默认值原为 `-1.0 / 1.0`（2D 取向，与独立的 `OrthographicCamera` 一致），在 3D 场景中从透视切到正交时远物会被裁掉。改为与透视模式一致的 `0.01 / 1000.0`，使两种投影覆盖相同的可视纵深，切换不再丢几何。

**变更：**
- `Scene/SceneCamera.h`：`m_OrthographicNear` `-1.0f` → `0.01f`、`m_OrthographicFar` `1.0f` → `1000.0f`
- 仅改默认值；`SetOrthographicNearClip()` / `SetOrthographicFarClip()` 行为不变，调用方仍可按需覆盖

**说明：** 正交投影为线性深度，无透视的 z-fighting 精度问题，大范围安全；独立的 2D 相机 `OrthographicCamera` 仍保留 `-1 / 1` 默认，不受影响

**涉及文件：** `Scene/SceneCamera.h`、`ENGINE_SUMMARY.md`

### 2026-07-07 — 新增 OpenGL 错误侦测宏 DMGE_GL_CALL

OpenGL 后端此前无任何 GL 错误检查，调用失误（无效枚举 / 错误状态等）被静默吞掉。新增 `Platform/OpenGL/OpenGLDebug.h`（header-only）提供 `DMGE_GL_CALL` 宏：Debug 下包裹 GL 调用，先排空残留错误、执行调用、再以 `glGetError` 逐条核对，任一错误即打印错误码与调用文本 / 文件 / 行号并在调用处触发 `DMGE_CORE_ASSERT` 断点；Release 下零开销展开为裸调用。

**新增（`Platform/OpenGL/OpenGLDebug.h`）：**
- `DMGE_GL_CALL(x)`：Debug 下 `GLClearErrors() -> x -> GLCheckErrors(#x, __FILE__, __LINE__)` + `DMGE_CORE_ASSERT`；Release 下 `x`
- `GLClearErrors()` / `GLCheckErrors()` / `GLDecodeError()`：排空错误、逐条核对并解码（`GL_INVALID_ENUM` 等）

**改造：** 6 个 OpenGL 后端 .cpp 全部接入，共 70 处包裹（RendererAPI 5 / VertexBuffer 11 / IndexBuffer 6 / VertexArray 13 / Shader 21 / Texture 14）
- 仅包裹 fire-and-forget 的 void 调用；`glCreate*` / `glGetString` / `glGetUniformLocation` / `glGet*iv` / `glGet*InfoLog` 等返回值或查询调用保持不包裹
- `OpenGLGraphicsContext` 无此类调用，未引入

**登记：** `OpenGLDebug.h` 加入 `CMakeLists.txt`（`DMGE_HEADERS`，Platform/OpenGL 段）

**涉及文件：** `Platform/OpenGL/OpenGLDebug.h`（新增）、`Platform/OpenGL/OpenGL*.cpp`（6 个）、`CMakeLists.txt`、`ENGINE_SUMMARY.md`

### 2026-07-08 - Renderer 接入 Shader 与相机场景数据流

此前 `Renderer::Submit` 仅接收 `VertexArray` 并委托 `DrawIndexed`，Shader 完全游离于渲染管线之外（调用方须自行 `Bind()`），`Camera::GetViewProjection()` 注释承诺的"提交 view-projection uniform"亦从未兑现。本次将 Shader 与相机视图投影纳入 Renderer，形成 Hazel 式场景数据流。

**变更（`Renderer/Renderer.h`）：**
- `Submit` 签名由 `Submit(const VertexArray&)` 改为 `Submit(const std::shared_ptr<Shader>&, const std::shared_ptr<VertexArray>&, const glm::mat4& transform = glm::mat4(1.0f))`：绑定 Shader、写入 `u_ViewProjection`（来自 `BeginScene` 缓存的相机）与 `u_Transform`（模型矩阵）后委托后端 `DrawIndexed`；MVP 由 Shader 在 GLSL 内以 `u_ViewProjection * u_Transform` 派生
- 新增 `BeginScene(const Camera&)`：清屏并缓存 `camera.GetViewProjection()`；原无参 `BeginScene()` 保留，改为清屏并将场景视图投影重置为 identity（防跨帧泄漏）
- 新增私有嵌套 `SceneData { glm::mat4 ViewProjectionMatrix }` 与静态实例 `s_SceneData`，承载帧内场景状态
- 前向声明 `Camera`/`Shader`/`VertexArray`，`Renderer.h` 仍不依赖 GL 头

**变更（`Renderer/Renderer.cpp`）：**
- 新增 `#include` Camera/Shader/VertexArray 头，定义 `Renderer::s_SceneData`
- `BeginScene()` / `BeginScene(const Camera&)` / `Submit(...)` 实现如上；`Submit` 对 `shader`/`vertexArray` 空指针附断言

**影响：**
- `Application::MainLoop` 仍调用无参 `BeginScene()`（清屏 + 重置场景）；持有相机的层在其 `OnRender()` 内调用 `BeginScene(camera)` 再 `Submit` 提交
- 旧 `Submit(vertexArray)` 已移除（引擎内无调用方）；若外部游戏工程使用需迁移为新签名

**涉及文件：** `Renderer/Renderer.h`、`Renderer/Renderer.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-09 - Application 接入激活场景相机

此前 `Application::MainLoop` 渲染阶段只调用无参 `Renderer::BeginScene()`，引擎主循环中没有使用相机的位置，上一版的相机场景数据流无从消费。本次为 Application 增加可设置的激活相机，使主循环真正驱动相机视图投影。

**变更（`Core/Application.h`）：**
- 新增前向声明 `class Camera;` 与私有成员 `std::shared_ptr<Camera> m_ActiveCamera`
- 新增公有 `SetActiveCamera(const std::shared_ptr<Camera>&)` 与 `Camera* GetActiveCamera() const`（非内联，定义于 .cpp）

**变更（`Core/Application.cpp`）：**
- 新增 `#include "DMGameEngine/Renderer/Camera.h"`
- `MainLoop` Stage 3 改为：若 `m_ActiveCamera` 非空则 `Renderer::BeginScene(*m_ActiveCamera)` 缓存其视图投影，否则回退无参 `BeginScene()`（清屏 + identity VP），向后兼容
- 实现 `GetActiveCamera` / `SetActiveCamera`

**影响：**
- 默认无相机时行为不变；游戏/层在 `OnInitialize` 或 `OnUpdate` 调用 `SetActiveCamera(myCamera)` 即可让 `Submit` 拿到正确的 `u_ViewProjection`
- 相机仍由调用方拥有与驱动（视图矩阵等）；Application 仅在帧首读取其 `GetViewProjection()`

**涉及文件：** `Core/Application.h`、`Core/Application.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-09 - header-only 相机类移除 DMGE_API

`Camera` / `OrthographicCamera` / `PerspectiveCamera` / `SceneCamera` 四个相机类全部内联于头文件（无 .cpp），却沿用了 `DMGE_API` 导出标注。引擎构建时 `DMGE_API` = `dllexport`，但这些类从未在任何 .cpp 中实例化，故 DLL 中不存在其构造 / 析构 / 虚表符号；游戏消费时 `DMGE_API` = `dllimport`，链接器向 DLL 索取这些符号而不得 -> `LNK2019`。此前游戏一直未实例化相机，隐患未暴露；接入激活相机后 `std::make_shared<OrthographicCamera>` 才触发。

**根因：** header-only 类的代码直接编译进每个消费方，不应标 `DMGE_API`（`dllexport` / `dllimport`）。只有"有 .cpp、编译进 DLL 并导出符号"的类才需要 `DMGE_API`。

**变更：**
- 4 个相机头文件：`class DMGE_API X` -> `class X`（`Renderer/Camera.h`、`Renderer/OrthographicCamera.h`、`Renderer/PerspectiveCamera.h`、`Scene/SceneCamera.h`）
- 顺带删除这 4 个头中已失效的 `#include "DMGameEngine/Core/Export.h"`（`DMGE_API` 不再被引用）

**影响：**
- 相机的构造 / 析构 / 虚表改由各消费方直接内联编译，不再跨 DLL 导入；`shared_ptr<Camera>` 跨边界时不存在"虚表归 DLL、对象归 exe"的错配
- 引擎 DLL 不受影响（本就不含相机符号，亦无 .cpp 实例化它们）；有 .cpp 的导出类（`Application` / `Renderer` / `Shader` 等）仍保留 `DMGE_API`，API 不变
- `game/src/main.cpp` 无需改动

**涉及文件：** `Renderer/Camera.h`、`Renderer/OrthographicCamera.h`、`Renderer/PerspectiveCamera.h`、`Scene/SceneCamera.h`、`ENGINE_SUMMARY.md`

### 2026-07-09 - 修复 OrthographicCamera 旋转未换算弧度

`OrthographicCamera::RecalculateViewMatrix()` 此前直接把 `m_Rotation` 传入 `glm::rotate()`，而 GLM 的 `glm::rotate` 角度参数须为**弧度**；`SetRotation()` / `m_Rotation` 的语义对使用者而言是度数（`SetRotation(90.f)` 期望旋转 90°），却未做度 -> 弧度换算，导致实际旋转角度远大于预期（90 被当作 90 弧度 ≈ 5156°）。

**修复（`Renderer/OrthographicCamera.h`）：**
- `RecalculateViewMatrix()`：`glm::rotate(glm::mat4(1.0f), m_Rotation, ...)` -> `glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), ...)`
- `SetRotation()` / `m_Rotation` 约定改为**度数**，注释同步更新（`in radians` -> `in degrees（内部经 glm::radians 换算）`）

**全相机旋转排查（结果：仅此一处）：**
- `Camera`（基类）- 无旋转字段，无此问题
- `OrthographicCamera` - **已修复**（上）
- `PerspectiveCamera` - 视图由 `glm::lookAt(position, target, up)` 推导，无欧拉旋转角，无此问题
- `SceneCamera`（`Scene/`）- 仅投影参数（正交无角度），无旋转字段，无此问题

**说明：** 透视 FOV（`PerspectiveCamera::m_Fov`、`SceneCamera::m_PerspectiveFOV`）当时以**弧度**存储（默认 `glm::radians(45.0f)`，与 `glm::perspective` 直接对接），与正交相机 roll 的度数约定不同；已统一为度数（见下条「透视 FOV 统一为度数」）。

**涉及文件：** `Renderer/OrthographicCamera.h`、`ENGINE_SUMMARY.md`

### 2026-07-09 - 透视 FOV 统一为度数

承接上一条：正交相机 roll 已改为度数，但透视 FOV（`PerspectiveCamera::m_Fov`、`SceneCamera::m_PerspectiveFOV`）仍以弧度存储，与 roll 约定不一致。现将全相机角度统一为「API 传度数、内部 `glm::radians` 换算」。

**修复：**
- `PerspectiveCamera`（`Renderer/PerspectiveCamera.h`）：`m_Fov` 默认 `glm::radians(45.0f)` -> `45.0f`；`SetProjection()` 与 `SetAspectRatio()` 的 `glm::perspective(...)` 改传 `glm::radians(fov)` / `glm::radians(m_Fov)`；注释改为度数
- `SceneCamera`（`Scene/SceneCamera.h`）：`m_PerspectiveFOV` 默认 `glm::radians(45.0f)` -> `45.0f`；`RecalculateProjection()` 的 `glm::perspective(...)` 改传 `glm::radians(m_PerspectiveFOV)`；注释改为度数

**约定（全相机一致）：** 所有用户面向的角度参数（roll、FOV）一律以**度数**传入，内部在调用 `glm::rotate` / `glm::perspective` 前用 `glm::radians` 换算；正交投影的 L/R/B/T/near/far 为线性范围，不涉及角度。

**涉及文件：** `Renderer/PerspectiveCamera.h`、`Scene/SceneCamera.h`、`ENGINE_SUMMARY.md`

### 2026-07-10 - 修复 Shutdown 未触发各层 OnDetach()

`Application::Shutdown()` 此前以 `m_layerStack = LayerStack{}` 收尾层栈。该语句调用的是 `LayerStack` 的默认移动赋值 `operator=(LayerStack&&) = default`，仅做成员级赋值，不执行 `~LayerStack()` 析构体，而逆序调用各层 `OnDetach()` 的逻辑正位于析构体内。结果：旧栈的 `unique_ptr<Layer>` 在移动赋值中被销毁（执行 `~Layer()`），但 `OnDetach()` 被静默跳过；其后 `delete app` 触发 `~Application() -> ~LayerStack()` 时，`m_layerStack` 已被替换为空栈，循环遍历空容器，`OnDetach()` 始终不被调用。`TestLayer::OnDetach()` 不执行（`TestLayer shutting down` 日志不打印），`ImGuiLayer::OnDetach()` 同理。

**根因：** 误以为移动赋值会触发析构。析构仅在对象生命周期结束（离开作用域 / `delete`）时发生，移动赋值不触发；原注释「LayerStack destructor automatically calls OnDetach()」与实际语句相矛盾。

**修复：**
- `Core/LayerStack.h` / `Core/LayerStack.cpp`：新增 `void Clear()`，逆序遍历调用各层 `OnDetach()`（此时层仍存活），随后 `m_layers.clear()` 并重置 `m_layerInsertIndex`；`~LayerStack()` 改为委托 `Clear()`，对已清空栈幂等
- `Core/Application.cpp`：`Shutdown()` 中 `m_layerStack = LayerStack{};` 改为 `m_layerStack.Clear();`，置于 `Renderer::Shutdown()` 之后、`m_window.reset()` 之前（GL 上下文仍存活，层可在 `OnDetach()` 中安全释放 GL 资源）

**影响：**
- 关闭应用时各层 `OnDetach()` 现被正确触发（`TestLayer` 打印日志并释放 VAO / Shader，`ImGuiLayer` 关闭 ImGui 后端）
- 析构函数复用 `Clear()`，`delete app` 时对空栈幂等，不会重复 detach
- `LayerStack` 移动语义（`= default`）保持不变；移动赋值不 detach 为常规语义，故仍须显式 `Clear()`

**涉及文件：** `Core/LayerStack.h`、`Core/LayerStack.cpp`、`Core/Application.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-11 - 窗口 resize 同步活动相机宽高比（OnViewportResize 钩子）

`Application::OnEvent` 处理 `WindowResizeEvent` 时仅调用 `Renderer::OnWindowResize`（设视口），未更新 `m_ActiveCamera` 的宽高比，导致缩放后 `PerspectiveCamera`/`SceneCamera` 的投影保持旧比例、画面拉伸。

**修复：**
- `Renderer/Camera.h`：基类新增虚方法 `virtual void OnViewportResize(uint32_t, uint32_t)`（默认 no-op，补 `#include <cstdint>`），作为宿主 resize 回调钩子
- `Renderer/PerspectiveCamera.h`：新增 `SetViewportSize(uint32_t, uint32_t)`（零尺寸守卫 + 安全 float 除法，委托 `SetAspectRatio`）；`OnViewportResize()` 重写委托 `SetViewportSize`（补 `#include <cstdint>`）
- `Scene/SceneCamera.h`：`OnViewportResize()` 重写委托已有的 `SetViewportSize`
- `Core/Application.cpp`：`OnEvent` 的 `WindowResizeEvent` lambda 改捕获 `[this]`，设视口后调用 `m_ActiveCamera->OnViewportResize(w, h)`（若已设相机），仍返回 `false` 让图层继续处理

**设计：** 两相机 resize 调用链统一为 `OnViewportResize -> SetViewportSize -> SetAspectRatio -> Recalculate*`；`OrthographicCamera` 投影用显式边界、不依赖宽高比，保持基类 no-op。`SetViewportSize` 封装「先转 float 再除」避免调用方整数除法踩坑，零尺寸守卫防 NaN。

**涉及文件：** `Renderer/Camera.h`、`Renderer/PerspectiveCamera.h`、`Scene/SceneCamera.h`、`Core/Application.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-11 - Timestep 抽象集成 Application + deltaTime 钳制

`Core/Timestep.h` 此前已定义帧时间步类型（持有秒数 + `GetSeconds()`/`GetMilliseconds()` + 隐式 `float` 转换），但 `Application` 主循环仍内联 `chrono` 算 `deltaTime` 并以 `float` 传入 `OnUpdate`，未使用该类型，且无上限钳制——窗口拖拽/断点/系统挂起时 `deltaTime` 可飙至 0.5s，导致物理/动画积分爆炸。

**修复：**
- `Core/Layer.h`：`OnUpdate(float deltaTime)` 改为 `OnUpdate(Timestep ts)`；补 `#include "DMGameEngine/Core/Timestep.h"`
- `Core/Application.h` / `Core/Application.cpp`：`OnUpdate` 签名同步改为 `OnUpdate(Timestep ts)`；补 Timestep.h include
- `Core/Application.cpp` `MainLoop`：`deltaTime` 改用 `std::clamp(elapsed.count(), 0.0f, 0.1f)` 钳制上限 0.1s，再 `Timestep ts(deltaTime)` 包装后分发至 `layer->OnUpdate(ts)` 与 `OnUpdate(ts)`；补 `#include <algorithm>`
- `Core/LayerStack.h`：注释 `OnUpdate(dt)` 同步为 `OnUpdate(ts)`
- `DMGameEngine.h`：公共头补 `#include "DMGameEngine/Core/Timestep.h"`
- `CMakeLists.txt`：`DMGE_HEADERS` 补 `src/DMGameEngine/Core/Timestep.h`
- `game/src/main.cpp`：`DemoGame::OnUpdate` 改为 `OnUpdate(DMGameEngine::Timestep ts)`；`m_elapsedTime += ts` 走隐式 `float` 转换，`fmt` 格式化改用 `ts.GetSeconds()`

**设计：** 钳制位于 Application 层（符合 `Timestep.h` 设计注释——该类型只持有/展示时长，不负责钳制）。`std::clamp(elapsed.count(), 0.0f, 0.1f)` 同时防负值（时钟回拨）与超大值。`Timestep` 的隐式 `operator float()` 使 `pos += speed * ts` 等既有浮点运算无需改写；`fmt` 需显式 `GetSeconds()` 因无 `fmt::formatter<Timestep>` 特化。

**涉及文件：** `Core/Layer.h`、`Core/Application.h`、`Core/Application.cpp`、`Core/LayerStack.h`、`DMGameEngine.h`、`CMakeLists.txt`、`game/src/main.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-11 - 新增 Material 类与 Renderer::Submit 材质重载

新增 `Renderer/Material.h/.cpp`：`Material` 持有 `std::shared_ptr<Shader>` 与按名称存取的 uniform 值（`std::unordered_map<std::string, UniformValue>`，`UniformValue` 为覆盖 Shader 全部 uniform 类型的 `std::variant`）。`Bind()` 绑定 shader 后用 `std::visit` 一次性上传所有已存 uniform，免每帧逐个 `SetXxx`。后端无关，无平台工厂/子类。

**新增：**
- `Renderer/Material.h`：声明 `UniformValue` 类型别名 + `Material` 类（构造持 shader、`Bind() const`、`SetInt/SetFloat/SetFloat2-4/SetMat4/SetIntArray` 镜像 Shader API、`Has/Get` 查询）
- `Renderer/Material.cpp`：实现；`Bind()` 用匿名 `overloaded` + `std::visit` 派发各 uniform 类型至对应 `Shader` setter；`SetIntArray` 拷贝为 `std::vector<int>` 自持

**Renderer::Submit 材质重载（非破坏性）：**
- `Renderer/Renderer.h`：补前向声明 `class Material;` + `Submit(const std::shared_ptr<Material>&, const std::shared_ptr<VertexArray>&, const glm::mat4& transform = glm::mat4(1.0f))` 重载声明
- `Renderer/Renderer.cpp`：补 `#include "DMGameEngine/Renderer/Material.h"` + 重载定义：`material->Bind()` 后经 `material->GetShader()` 补传 `u_ViewProjection`/`u_Transform` 再 `DrawIndexed`

**设计：** Material 管材质参数（颜色、强度等），Renderer 管场景/物体参数（`u_ViewProjection`/`u_Transform`），二者解耦避免 Material 每帧重复承担全局状态。setter 镜像 Shader 命名避免重载歧义。`int` 数组存 `vector<int>` 使 Material 自持数据、`Bind()` 可反复重传。

**涉及文件：** `Renderer/Material.h`、`Renderer/Material.cpp`、`Renderer/Renderer.h`、`Renderer/Renderer.cpp`、`DMGameEngine.h`、`CMakeLists.txt`、`ENGINE_SUMMARY.md`

### 2026-07-11 - 新增 MaterialInstance（材质实例）+ Material 多态化

新增 `MaterialInstance`：派生自 `Material`，引用共享基材质（`shared_ptr<Material>`，含 Shader + 默认 uniform）并叠加私有 `m_Overrides`。`Bind()` 先 `m_BaseMaterial->Bind()`（绑 shader + 上传基 uniform），再用共享 `UploadUniforms()` 上传本实例覆盖，故共享同一 `Material` 的多个实例可各自调参而互不影响；setter 仅写覆盖、`Has`/`Get` 先覆盖后基。

**改动：**
- `Renderer/Material.h`：`Material` 的 `Bind`/`SetInt`.../`Has`/`Get` 改 `virtual`、补 `virtual ~Material() = default;`；新增 `MaterialInstance : public Material`（`m_BaseMaterial` + `m_Overrides`，覆盖 `Bind`/setter/`Has`/`Get`，`GetBaseMaterial` 访问器）
- `Renderer/Material.cpp`：抽出匿名 `UploadUniforms(shader, map)` 供 `Material::Bind` 与 `MaterialInstance::Bind` 复用；`Material::Bind` 改委托该 helper（行为不变）；实现 `MaterialInstance`（构造 `Material(base->GetShader())`、`Bind` 叠加覆盖、setter 写 `m_Overrides`、`Has`/`Get` 先覆盖后基）

**设计：** 派生自 `Material` 使 `shared_ptr<MaterialInstance>` 隐式转 `shared_ptr<Material>`，现有 `Renderer::Submit(shared_ptr<Material>&)` 经虚 `Bind()` 直接适用于实例，无需新增 Submit 重载。`virtual` 改动向后兼容（对具体 `Material` 行为不变）。`MaterialInstance` 与 `Material` 同处 `Material.h/.cpp`，故 CMake/公共头无需改动。

**涉及文件：** `Renderer/Material.h`、`Renderer/Material.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-13 - 引入智能指针别名 Scope/Ref/CreateScope/CreateRef，全工程替换 std::unique_ptr / std::shared_ptr

在 Core/Export.h 的 namespace DM 中新增智能指针类型别名与工厂函数：

- Scope<T> = std::unique_ptr<T>，Ref<T> = std::shared_ptr<T>
- CreateScope<T>(args...) = std::make_unique<T>(args...)，CreateRef<T>(args...) = std::make_shared<T>(args...)

引擎代码位于 namespace DMGameEngine，通过 Export.h 末尾的 using DM::Scope / using DM::Ref / using DM::CreateScope / using DM::CreateRef 将别名引入。全工程统一使用 DM::Scope<T> / DM::Ref<T>（类型）与 DM::CreateScope<T>(...) / DM::CreateRef<T>(...)（工厂调用）替代裸 std::unique_ptr / std::shared_ptr / std::make_unique / std::make_shared。

**改动：**
- Core/Export.h：新增 #include <utility>；namespace DM 内补 CreateScope / CreateRef 函数模板；新增 namespace DMGameEngine 块以 using 引入四个别名
- 全工程 26 个文件（Core / Renderer / Platform）中所有 std::unique_ptr<T> -> DM::Scope<T>、std::shared_ptr<T> -> DM::Ref<T>、std::make_unique<T>(...) -> DM::CreateScope<T>(...)、std::make_shared<T>(...) -> DM::CreateRef<T>(...)
- Core/LayerStack.h、Core/Window.h：补 #include "DMGameEngine/Core/Export.h"（原先通过 <memory> 直接使用 std 智能指针，现改为别名需显式引入）

**设计：** 别名集中于 Export.h（已被全工程普遍包含），一处定义、全局可用。DM:: 前缀显式限定命名空间，避免在 DMGameEngine 命名空间内裸用 Scope/Ref 产生歧义。工厂函数 CreateScope/CreateRef 保持与 std::make_unique/std::make_shared 完全一致的语义，仅替换调用名，不改行为。

**涉及文件：** Core/Export.h、Core/Application.h、Core/Application.cpp、Core/LayerStack.h、Core/LayerStack.cpp、Core/Log.h、Core/Log.cpp、Core/Window.h、Platform/Windows/WindowsWindow.h、Platform/Windows/WindowsWindow.cpp、Platform/OpenGL/OpenGLVertexArray.h、Platform/OpenGL/OpenGLVertexArray.cpp、Renderer/Renderer.h、Renderer/Renderer.cpp、Renderer/RendererAPI.h、Renderer/RendererAPI.cpp、Renderer/Shader.h、Renderer/Shader.cpp、Renderer/Texture.h、Renderer/Texture.cpp、Renderer/VertexArray.h、Renderer/VertexArray.cpp、Renderer/VertexBuffer.h、Renderer/VertexBuffer.cpp、Renderer/IndexBuffer.h、Renderer/IndexBuffer.cpp、Renderer/Material.h、Renderer/Material.cpp、ENGINE_SUMMARY.md

### 2026-07-13 - Texture 体系重构：拆分 Texture2D / TextureCube / Texture2DArray

将单一 `Texture` + `OpenGLTexture` 重构为「抽象基类 + 三种具体类型 + 各自 OpenGL 后端」的纹理类型层次，支持立方体贴图与 2D 纹理数组等后续扩展。

**改动：**
- `Renderer/Texture.h` 改为纯抽象基类（header-only），仅保留 `TextureFormat`/`TextureFilter`/`TextureWrap` 三个共享枚举与 `Bind/Unbind/GetWidth/GetHeight/GetRendererID/operator==` 公共接口；移除 `TextureSpecification` 与 `Texture::Create()` 工厂
- 新增 `Renderer/Texture2D.h/.cpp`：`Texture2D : Texture` + `Texture2DSpecification` + `SetData(data,size)` + `Create(spec)/Create(filepath)` 工厂
- 新增 `Renderer/TextureCube.h/.cpp`：`TextureCube : Texture` + `CubeFace` 枚举 / `CubeFaceCount` / `TextureCubeSpecification`（WrapR/S/T 默认 `ClampToEdge`）+ `SetData(data,size,face)` + `Create(spec)/Create({6 面})` 工厂
- 新增 `Renderer/Texture2DArray.h/.cpp`：`Texture2DArray : Texture` + `Texture2DArraySpecification`（含 `Layers`/`WrapR`）+ `SetData(data,size,layer)` + `GetLayerCount()` + `Create(spec)` 工厂
- 原 `OpenGLTexture.h/.cpp` 重命名为 `OpenGLTexture2D.h/.cpp`（`GL_TEXTURE_2D`，行为不变）
- 新增 `OpenGLTextureCube.h/.cpp`（`GL_TEXTURE_CUBE_MAP`，6 面分配 / `glTexSubImage2D` 单面更新 / 文件加载按首面尺寸分配）与 `OpenGLTexture2DArray.h/.cpp`（`GL_TEXTURE_2D_ARRAY`，`glTexImage3D` 分配 / `glTexSubImage3D` 单层更新）
- 原 `OpenGLTexture.cpp` 中的 GL 枚举映射 static 成员抽出到 `OpenGLTextureUtils.h`（`DMGameEngine::Detail` 内联自由函数），三个 OpenGL 后端共用
- `DMGameEngine.h` 新增 `Texture2D/TextureCube/Texture2DArray` 公共头；`CMakeLists.txt` 源/头列表同步更新；删除 `Renderer/Texture.cpp`（工厂下放、基类 header-only）

**设计：** 工厂从 `Texture::Create()` 下放到各子类，使 2D / 立方体 / 数组的尺寸、面、层语义各自隔离；`SetData` 签名随类型变化（2D 传 `size`、立方体传 `face`、数组传 `layer`），故下放到子类而非基类。GL 枚举映射抽为共享内联工具避免三处重复。API 变更：原 `Texture::Create(...)` / `TextureSpecification` 改为 `Texture2D::Create(...)` / `Texture2DSpecification`，新建立方体用 `TextureCube::Create`、2D 数组用 `Texture2DArray::Create` 后按层 `SetData`。当前工程无其他处引用旧 API，故不破坏现有代码。

**涉及文件：** Renderer/Texture.h、Renderer/Texture2D.h/.cpp、Renderer/TextureCube.h/.cpp、Renderer/Texture2DArray.h/.cpp、Platform/OpenGL/OpenGLTextureUtils.h、Platform/OpenGL/OpenGLTexture2D.h/.cpp、Platform/OpenGL/OpenGLTextureCube.h/.cpp、Platform/OpenGL/OpenGLTexture2DArray.h/.cpp、DMGameEngine.h、CMakeLists.txt、ENGINE_SUMMARY.md（删除 Renderer/Texture.cpp、Platform/OpenGL/OpenGLTexture.h/.cpp）

### 2026-07-14 - RendererAPI::Init 可配置化（RendererAPIInitConfig）

将 `RendererAPI::Init()` 由硬编码、无参的虚函数改造为可配置的模板方法，初始管线状态不再写死于 `OpenGLRendererAPI::Init()`。

**改动：**
- `Renderer/RendererAPI.h` 新增 `RendererAPIInitConfig` 聚合结构：清屏色 `ClearColor`（默认 `{0,0,0,0}`，对齐 GL 默认）、深度测试开关 `DepthTestEnabled` + 比较函数 `DepthFunction`（默认开 + `Less`）、面剔除 `Culling`（默认 `None`）、混合开关 `BlendEnabled` + 源/目标因子 `SrcBlendFactor`/`DstBlendFactor` + 方程 `BlendEquationMode`（默认关 + `SrcAlpha`/`OneMinusSrcAlpha`/`Add`）。默认值与原硬编码基线一致，无参调用行为不变。
- `RendererAPI::Init(const RendererAPIInitConfig& config = {})` 改为模板方法：经各虚 setter（`SetClearColor`/`SetDepthTest`/`SetDepthFunc`/`SetCullMode`/`SetBlendState`/`SetBlendEquation`）应用 config，由后端派发到各自 API 调用；实现移至 `RendererAPI.cpp`。
- `Renderer::Init(const RendererAPIInitConfig& config = {})` 转发 config 至 `s_RendererAPI->Init(config)`；`Application::Initialize()` 仍以无参 `Renderer::Init()` 调用（走默认 config），行为不变。
- `OpenGLRendererAPI` 移除原硬编码 `Init()` 重写（`glEnable(GL_DEPTH_TEST)` + 固定 blend/cull），直接继承基类模板方法；其虚 setter 实现不变。

**设计：** 初始管线状态从「OpenGL 后端写死」上提为「基类按 config 经虚 setter 应用」的模板方法，消除后端重复，并使初始状态可由调用方（如 `Application`）按需定制，如 `Renderer::Init({ .Culling = CullMode::Back })`。`RendererAPIInitConfig` 复用既有 `BlendFactor`/`BlendEquation`/`DepthFunc`/`CullMode` 枚举。残留：线框模式（polygon mode / `glPolygonMode`）尚未暴露，留待后续。

**涉及文件：** Renderer/RendererAPI.h、Renderer/RendererAPI.cpp、Renderer/Renderer.h、Renderer/Renderer.cpp、Platform/OpenGL/OpenGLRendererAPI.h、Platform/OpenGL/OpenGLRendererAPI.cpp、ENGINE_SUMMARY.md、ENGINE_REVIEW.md

### 2026-07-14 - Texture 后端改为不可变存储 + glTexSubImage 更新，mipmap 与 SetData 解耦

针对纹理高频更新场景（视频帧、动态小地图、Canvas 画布）优化 OpenGL 后端：避免每次 `SetData` 重新指定存储与重算 mipmap 的开销。

**改动：**
- `Renderer/Texture.h`：基类新增纯虚 `virtual void GenerateMipmaps() = 0;`，将 mipmap 重生成显式化（原先由 `SetData` 自动触发）
- `OpenGLTextureUtils.h`：新增 `Detail::MipLevelCount(w, h)`（`floor(log2(max(w,h)))+1`）用于计算不可变存储的 mip 层数
- `OpenGLTexture2D` / `OpenGLTextureCube` / `OpenGLTexture2DArray`：
  - `Invalidate()` 由 `glTexImage2D` / `glTexImage3D`（mutable，逐层/逐面分配）改为 `glTexStorage2D` / `glTexStorage3D`（不可变存储，一次性分配全部面/层 + mip 链，`GenerateMipmaps` 为真时预留全部 mip 层）；移除分配后的 `glGenerateMipmap`
  - `SetData` 由 `glTexImage2D`（重指定存储 + 自动 `glGenerateMipmap`）改为 `glTexSubImage2D` / `glTexSubImage3D`（仅写已分配存储、不重指定/重分配、不重算 mipmap）
  - 新增 `GenerateMipmaps() override`（`!m_RendererID || !m_Spec.GenerateMipmaps` 时直接返回，安全）；`OpenGLTexture2D` / `OpenGLTextureCube` 的文件加载构造在首张上传后调一次 `GenerateMipmaps()`

**设计：** 不可变存储一次性分配，后续更新只能走 `glTexSubImage*`（`glTexImage*` 对不可变存储为 GL 错误），驱动无需每次重分配/重校验存储，流式更新更高效；mipmap 与 `SetData` 解耦后，流式场景关闭 `GenerateMipmaps` 即可彻底跳过每帧 mipmap 重算，需要时显式调用 `GenerateMipmaps()`。引擎请求 GL 4.6 core（`WindowsWindow.cpp`），`glTexStorage2D` / `glTexStorage3D` 运行时可用。带宽与同步停顿的进一步优化（PBO 异步上传、按区域 `SetSubData`）留待后续。

**涉及文件：** Renderer/Texture.h、Platform/OpenGL/OpenGLTextureUtils.h、Platform/OpenGL/OpenGLTexture2D.h/.cpp、Platform/OpenGL/OpenGLTextureCube.h/.cpp、Platform/OpenGL/OpenGLTexture2DArray.h/.cpp、ENGINE_SUMMARY.md

### 2026-07-15 - 相机控制器（CameraController 基类 + 2D / 3D 编辑器）

为现有相机体系补充输入驱动层：以轮询式输入（`Input::Get()`）与事件路由（`OnEvent`）驱动 `Camera` 的视图（及必要时投影）矩阵，每帧由 `OnUpdate(Timestep)` 推进。

**改动：**
- `Renderer/CameraController.h`：新增抽象基类 `CameraController`，定义控制器接口 `OnUpdate(Timestep)` / `OnEvent(Event&)` / `GetCamera()`，并提供 `SetEnabled` / `IsEnabled` 开关（禁用时忽略输入、保留上次相机状态）。协变 `GetCamera()` 允许派生类暴露具体相机类型。header-only，与 Camera 家族一致。
- `Scene/OrthographicCameraController.h`：2D 控制器，持有 `OrthographicCamera`。WASD / 方向键平移、Q / E 绕视图 Z 轴翻滚（构造时 `rotation` 开关启用）、鼠标滚轮缩放（缩放级别缩放正交边界）、`WindowResizeEvent` 同步宽高比。平移速度随缩放级别缩放以保持手感一致。
- `Scene/EditorCameraController.h`：3D 编辑器轨道控制器，持有 `PerspectiveCamera`。以 target 为中心的球面坐标（yaw / pitch / distance）定位相机：左键拖拽轨道旋转、右键 / 中键拖拽平移 target、滚轮改变轨道距离（dolly）、WASD / Space / LeftShift 沿视图前 / 右 / 上轴移动 target（`OnUpdate` 轮询）。pitch 限制在 ±89° 避免 up 翻转；平移与 dolly 速度随距离缩放。
- `DMGameEngine.h`：公共 API 头新增 `Renderer/CameraController.h`、`Scene/OrthographicCameraController.h`、`Scene/EditorCameraController.h`。
- `CMakeLists.txt`：将三个新头文件加入 `DMGE_HEADERS`（IDE 分组）。安装由既有 `install(DIRECTORY src/ ... *.h)` 覆盖，无需额外规则。
- `Core/Application.h` / `Core/Application.cpp`：将活动相机由 `DM::Ref<Camera> m_ActiveCamera` 改为 `DM::Ref<CameraController> m_ActiveController`（`SetActiveCameraController` / `GetActiveCameraController`）。主循环 Stage 2 先 `m_ActiveController->OnUpdate(ts)` 再更新各层；`OnEvent` 在视口同步后把事件转发给控制器；Stage 3 `Renderer::BeginScene(m_ActiveController->GetCamera())`。控制器自带相机按值持有，`GetCamera()` 返回 `Camera&` 供 BeginScene 借引用，相机无 shared_ptr 牵连。

**设计：** 控制器刻意不是 `Layer`，而是暴露与 `Layer` 相同的 `OnUpdate` / `OnEvent` 表面，便于被 Layer / Scene / Application 持有并转发每帧时间步与事件。键盘连续移动走轮询（`OnUpdate`），鼠标离散交互（拖拽轨道 / 平移、滚轮缩放、窗口缩放）走事件（`OnEvent` 经 `EventDispatcher`），事件处理函数返回 `false` 以让输入继续向其它层传播。控制器为 header-only，无平台相关代码，复用 `Camera` 已有的 `SetView` / `SetProjection` 接口推进矩阵。

**涉及文件：** Renderer/CameraController.h、Scene/OrthographicCameraController.h、Scene/EditorCameraController.h、Core/Application.h、Core/Application.cpp、DMGameEngine.h、CMakeLists.txt、ENGINE_SUMMARY.md

### 2026-07-16 - 输入扩展：手柄抽象 + 鼠标逐帧位移 + 光标模式 + raw mouse motion

补齐 `ENGINE_REVIEW.md` D3「Input 覆盖面不足」全部四项：手柄/控制器轮询与热插拔、逐帧鼠标位移 `GetMouseDelta()`、光标模式控制 `SetCursorMode`、raw mouse motion。帧序 `BeginFrame() -> PollEvents() -> OnUpdate()` 已天然适配逐帧位移（清零于 BeginFrame、累积于 PollEvents、读取于 OnUpdate）。

**改动：**
- `Core/GamepadCodes.h`（新）：`GamepadButton` / `GamepadAxis` 枚举（值与 GLFW 标准 gamepad mapping 一一对应）+ `kMaxGamepads=4`。
- `Core/Events/GamepadEvent.h`（新）：`GamepadConnectedEvent`（带 jid + name）/ `GamepadDisconnectedEvent`。
- `Core/Events/Event.h`：新增 `GamepadConnected/Disconnected` 事件类型、`EventCategory::Gamepad`、`ToString` 分支。
- `Core/Window.h`：新增 `enum class CursorMode { Normal, Hidden, Disabled }` + `SetCursorMode/GetCursorMode/SetRawMouseMotion/IsRawMouseMotion`。
- `Core/Input.h`：新增 `GetMouseDeltaX/Y()`、手柄轮询 `IsGamepadPresent/GetGamepadName/IsGamepadButtonPressed/IsGamepadButtonJustPressed/GetGamepadAxis`；include `GamepadCodes.h` + `<string>`。
- `Platform/Windows/WindowsWindow.h/.cpp`：`SetCursorMode` 经 `glfwSetInputMode(GLFW_CURSOR, ...)`；`SetRawMouseMotion` 经 `glfwRawMouseMotionSupported()` + `GLFW_RAW_MOUSE_MOTION`；`glfwSetJoystickCallback` 热插拔回调派发 `GamepadConnected/DisconnectedEvent`（GLFW 回调无 window 参数，经文件作用域 `s_eventWindow` 桥接到窗口回调链，`Shutdown` 清空）。
- `Platform/Windows/GLFWInput.h/.cpp`：`GetMouseDeltaX/Y` 由 `OnEvent` 累积 `MouseMovedEvent`、`BeginFrame` 清零（`m_lastMouseX/Y` 跟踪连续位置避免跳变）；手柄经 `glfwGetGamepadState`（仅 present 且有标准 mapping 成功），`IsGamepadButtonJustPressed` 用 `BeginFrame` 轮询的上一帧快照 + 查询时实时轮询做双快照边沿检测；`BeginFrame` 顺带为 4 个槽轮询上一帧按钮状态。
- `DMGameEngine.h` / `CMakeLists.txt`：登记 `GamepadCodes.h`、`GamepadEvent.h`。

**设计：** 鼠标位移走事件累积而非 `glfwGetCursorPos`，在 `CursorMode::Disabled`（FPS 锁鼠）下也准确。手柄无 press/release 事件，边沿检测用「`BeginFrame` 轮询上一帧 + 查询时实时轮询」双快照（与键鼠的事件式 `m_keyJustPressed` 机制不同但同效）。光标模式与 raw motion 故意解耦：raw motion 仅 `Disabled` 下生效、需平台支持，FPS 锁鼠需二者组合，由调用方决定，引擎不施加策略。手柄索引 `[0, kMaxGamepads)` 越界一律安全返回 false / 0。GLFW 回调对私有 `WindowData` 的访问因 lambda 定义于 `Init` 成员函数上下文而合法（与既有窗口回调一致）。

**涉及文件：** Core/GamepadCodes.h、Core/Events/GamepadEvent.h、Core/Events/Event.h、Core/Window.h、Core/Input.h、Platform/Windows/WindowsWindow.h、Platform/Windows/WindowsWindow.cpp、Platform/Windows/GLFWInput.h、Platform/Windows/GLFWInput.cpp、DMGameEngine.h、CMakeLists.txt、ENGINE_SUMMARY.md、ENGINE_REVIEW.md

### 2026-07-18 - Vulkan 渲染后端（镜像 OpenGL 抽象，可选启用）

新增 `Platform/Vulkan/` 后端，与既有 `Platform/OpenGL/` 实现同一套渲染抽象（`GraphicsContext` / `RendererAPI` / `Shader` / `VertexBuffer` / `IndexBuffer` / `VertexArray` / `Texture2D` / `Texture2DArray` / `TextureCube`），基于 Vulkan 1.3（dynamic rendering + sync2）+ VulkanMemoryAllocator + shaderc。OpenGL 仍为默认后端；Vulkan 经 CMake 选项 `DMGE_VULKAN_BACKEND=ON` 选入、编译期 `DMGE_VULKAN` 宏切换，运行时由 `Renderer::SetAPI(Renderer::API::Vulkan)` 在 `Application::Run()` 之前切换。详细说明见 `VULKAN_BACKEND.md`。

**改动：**
- `Platform/Vulkan/`（新增 26 文件）：
  - `VulkanDebug.h`（header-only）：Vulkan debug messenger + `VkResult` 字符串化，Debug 下经 `VK_EXT_debug_utils` 回调打印验证层消息。
  - `VulkanDevice.h / .cpp`：Vulkan 实例创建 + 物理设备选择（图形队列）+ 逻辑设备（启用 `dynamicRendering` / `sync2`，API 1.3）+ VMA 分配器（`#define VMA_IMPLEMENTATION` 单点实现）+ `ImmediateSubmit` 即时命令提交助手。
  - `VulkanSwapchain.h / .cpp`：surface + 交换链 + 深度/模板附件，`AcquireImage` / `Present` / `Recreate`（窗口尺寸变化重建）。
  - `VulkanGraphicsContext.h / .cpp`：`GraphicsContext` 的 Vulkan 实现，帧生命周期经 dynamic rendering（无 render pass / framebuffer）；`BeginFrame` / `EndFrame`、静态 `Get()`；深度布局 `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`；提交时重置 fence 以避免 swapchain-out-of-date 死锁。
  - `VulkanRendererAPI.h / .cpp`：`RendererAPI` 的 Vulkan 实现；管线缓存以 shader + 顶点布局 + blend + depth + cull 为键；每帧 scratch UBO + 每绘制描述符池；`SetCurrentShader(VulkanShader*)` 指针直传；未绑定 sampler 跳过。
  - `VulkanShader.h / .cpp`：shaderc 运行时 GLSL -> SPIR-V；GLSL 重写桥接 GL 风格着色器——升至 `#version 450 core`、把独立 uniform 折叠进合成 std140 UBO（binding 0）、为 sampler 分配 binding 1..N、补全缺失的 `layout(location)` / `out`；基于名称的 `Set*` 写入 staging 缓冲。
  - `VulkanVertexBuffer.h / .cpp` / `VulkanIndexBuffer.h / .cpp` / `VulkanVertexArray.h / .cpp`：对应缓冲抽象的 Vulkan 实现（VMA 上传、顶点属性 -> `VkVertexInputBindingDescription` / `attribute`）。
  - `VulkanTexture.h`（header-only）：Texture Vulkan 共享基类，供描述符互操作。
  - `VulkanTextureUtils.h` / `VulkanTextureHelpers.h`（header-only）：VK 枚举映射（Format/Filter/Wrap -> Vk）+ 纹理上传 / 布局转换 inline 助手。
  - `VulkanTexture2D.h / .cpp` / `VulkanTexture2DArray.h / .cpp` / `VulkanTextureCube.h / .cpp`：三种纹理抽象的 Vulkan 实现。
- `CMakeLists.txt`：`target_compile_definitions` 之前新增 `option(DMGE_VULKAN_BACKEND ...)` 块——`find_package(Vulkan)` + `VMA_INCLUDE_DIR`（`vk_mem_alloc.h`）+ `SHADERC_INCLUDE_DIR` / `SHADERC_LIBRARY`（`shaderc_combined`）经 `$ENV{VULKAN_SDK}` 定位；启用时定义 `DMGE_VULKAN`、加入 11 个 Vulkan `.cpp` 源。依赖全部来自 Vulkan SDK，不入 `dependencies/`。
- 工厂派发（`#ifdef DMGE_VULKAN`）：`Renderer/RendererAPI.cpp` / `VertexBuffer.cpp` / `IndexBuffer.cpp` / `VertexArray.cpp` / `Shader.cpp` / `Texture2D.cpp` / `Texture2DArray.cpp` / `TextureCube.cpp` 共 8 个 `Create()` 在 Vulkan 下派发到对应 `Vulkan*` 实现。
- `Renderer/Renderer.h`：新增 `static void SetAPI(API)`（运行时切换后端，须在 `Application::Run()` 前调用）；`Renderer.cpp`：Init 日志打印实际后端。
- `Platform/Windows/WindowsWindow.cpp`：窗口提示按后端分支（Vulkan 用 `GLFW_NO_API`、不建 GL 上下文）；上下文创建在 `DMGE_VULKAN` 下走 `VulkanGraphicsContext`；VSync 在 Vulkan 下受保护（无 GL 上下文）。
- `ImGui/ImGuiLayer.cpp`：Vulkan 下跳过 OpenGL3 后端，改用 `ImGui_ImplGlfw_InitForVulkan`（面板暂不绘制，待补 `imgui_impl_vulkan` 后端）。
- `VULKAN_BACKEND.md`（新增）：后端文档（依赖、启用方式、运行时切换、已知限制）。

**设计：** 后端刻意复用既有抽象层，零侵入 OpenGL 路径——所有切换集中在编译期 `DMGE_VULKAN` 宏与运行期 `Renderer::SetAPI`。Vulkan 1.3 dynamic rendering 免去 render pass / framebuffer 显式管理；管线按 shader+顶点布局+blend+depth+cull 缓存避免逐绘制建管线；GLSL 由 shaderc 运行时编译并重写（升 450 core、合成 UBO / 自动 binding / 补 location）以兼容既有 GL 风格着色器。VMA 单点 `VMA_IMPLEMENTATION` 于 `VulkanDevice.cpp`。后端为可选构建，默认不参与编译，不影响 OpenGL 用户。已知限制（见 `VULKAN_BACKEND.md`）：GLSL 须为现代桌面风格（`gl_FragColor` / 预声明 uniform 块未自动处理）、sampler 数组支持不全、Vulkan ImGui 面板待补、GPU 资源须在 `Renderer::Shutdown()` / 窗口销毁前释放。

**涉及文件：** Platform/Vulkan/*（26 新增）、CMakeLists.txt、Renderer/Renderer.h、Renderer/Renderer.cpp、Renderer/RendererAPI.cpp、Renderer/VertexBuffer.cpp、Renderer/IndexBuffer.cpp、Renderer/VertexArray.cpp、Renderer/Shader.cpp、Renderer/Texture2D.cpp、Renderer/Texture2DArray.cpp、Renderer/TextureCube.cpp、Platform/Windows/WindowsWindow.cpp、ImGui/ImGuiLayer.cpp、VULKAN_BACKEND.md、ENGINE_SUMMARY.md

### 2026-07-18 - Vulkan 后端打通：编译/链接修复 + ImGui Vulkan 后端接入 + Y 翻转

Vulkan 后端首次完整构建暴露的编译、链接与运行期问题逐一修复，使 Vulkan 路径可编译链接并以正确朝向渲染场景与 ImGui 面板。

**改动：**
- 编译修复：
  - `VulkanShader.cpp`：shaderc 目标环境枚举 `shaderc_target_vulkan` -> `shaderc_target_env_vulkan`（旧枚举名已废弃）；`DestroyModules` 内 `VK_NULL_HANDLE` 链式赋值拆为两行。
  - `VulkanSwapchain.cpp`：`Cleanup` 内 `m_DepthView = m_DepthImage = VK_NULL_HANDLE` 链式赋值拆为两行。
  - `VulkanTextureHelpers.h`：`FinalizeAfterUpload` 新增 `baseLayer` 参数（供数组/立方体指定起始层）；`VulkanTexture2D.cpp` 调用补 `0`。
  - `VulkanRendererAPI.cpp`：`vkCmdBindDescriptorSets` 动态偏移 `dynamicOffset` 显式转 `uint32_t` 入栈局部 `dynOffset`，修正类型不匹配。
- 链接修复（`CMakeLists.txt`）：shaderc 按 Configuration 选库--`SHADERC_LIBRARY_DEBUG`（`shaderc_combinedd`）/ `SHADERC_LIBRARY_RELEASE`（`shaderc_combined`）经 `$<CONFIG:Debug>` 生成器表达式链接，解决 Debug 下链 `shaderc_combined` 缺符号；补充 VMA 头文件搜索路径 `Include/vma`。
- ImGui Vulkan 后端接入（接续上一条目「面板待补」限制）：
  - `CMakeLists.txt`：将 `imgui_impl_vulkan.cpp` 加入 Vulkan `target_sources`。
  - `VulkanGraphicsContext.h`：新增 `VkInstance GetInstance() const` 访问器。
  - `ImGuiLayer.cpp`：`OnAttach` 在 Vulkan 下以 dynamic rendering 初始化 `ImGui_ImplVulkan_InitInfo`（`ApiVersion=1.3`、`DescriptorPoolSize=1000`、`UseDynamicRendering=true`、`PipelineRenderingCreateInfo.pColorAttachmentFormats=交换链颜色格式`、`MSAASamples=1`）；`Begin` 调 `ImGui_ImplVulkan_NewFrame`，`End` 调 `ImGui_ImplVulkan_RenderDrawData` 录入 `VulkanGraphicsContext::Get().GetCurrentCommandBuffer()`（帧内 dynamic-rendering pass 仍开启），`OnDetach` 调 `ImGui_ImplVulkan_Shutdown`；附 `ImGuiVkCheckResult` 将 `VkResult` 错误转 `DMGE_LOG_ERROR`。
- Vulkan Y 翻转修复：Vulkan NDC Y 向下、framebuffer 原点在左上（OpenGL 为 Y 向上、原点左下），沿用 OpenGL 投影矩阵导致画面上下颠倒。
  - `Renderer.cpp` `BeginScene(const Camera&)`：Vulkan 下左乘 `flipY`（`flipY[1][1] = -1`）翻转 clip-space Y。
  - `VulkanRendererAPI.cpp`：`rasterizer.frontFace` 由 `VK_FRONT_FACE_COUNTER_CLOCKWISE` 改 `VK_FRONT_FACE_CLOCKWISE`，使投影翻转后绕序仍与 OpenGL 一致（默认剔除关闭，无回归；culling 开启后行为正确）。

**设计：** 投影翻转集中在 `Renderer::BeginScene`（`GetViewProjection()` 仅此处使用），相机保持后端无关；ImGui 使用自身投影故不受影响。ImGui 复用 `VulkanGraphicsContext` 已开启的 dynamic-rendering pass（跨 `EndScene`/ImGui 直到 `SwapBuffers`），无需独立 render pass / framebuffer。shaderc 库选择用生成器表达式按 Configuration 切换，避免 Debug 链 release 库的符号缺失。

**涉及文件：** `CMakeLists.txt`、`ImGui/ImGuiLayer.cpp`、`Platform/Vulkan/VulkanGraphicsContext.h`、`Platform/Vulkan/VulkanRendererAPI.cpp`、`Platform/Vulkan/VulkanShader.cpp`、`Platform/Vulkan/VulkanSwapchain.cpp`、`Platform/Vulkan/VulkanTexture2D.cpp`、`Platform/Vulkan/VulkanTextureHelpers.h`、`Renderer/Renderer.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-19 - 渲染层职责拆分：新增 RenderCommand 命令门面层

参照 Hazel，将原由 `Renderer` 兼任的「底层命令转发」职责独立为 `RenderCommand` 静态门面层，渲染抽象由两层（`RendererAPI` / `Renderer`）变为三层（`RendererAPI` / `RenderCommand` / `Renderer`）。后端实例所有权从 `Renderer` 迁至 `RenderCommand`。

**改动：**
- 新增 `Renderer/RenderCommand.h` / `.cpp`：静态门面类，持有 `static DM::Scope<RendererAPI> s_RendererAPI`；`Init(config)` 经 `RendererAPI::Create()`（仍按 `Renderer::GetAPI()` 分发）创建后端并 `Init`，`Shutdown()` 释放；转发 `SetClearColor`/`Clear`/`SetViewport`/`DrawIndexed` 及 `SetBlendState`/`SetBlendEquation`/`SetDepthTest`/`SetDepthFunc`/`SetCullMode`，每条命令带初始化守卫 assert。
- `Renderer/Renderer.h`：移除命令转发方法（`SetClearColor`/`Clear`/`SetViewport`/`SetBlendState`/`SetBlendEquation`/`SetDepthTest`/`SetDepthFunc`/`SetCullMode`）及 `s_RendererAPI` 成员；保留 `API` 枚举、`Init`/`Shutdown`/`BeginScene`/`EndScene`/`Submit`/`Flush`/`OnWindowResize`/`SceneData`；更新文件头注释。
- `Renderer/Renderer.cpp`：不再直接持有/调用 `RendererAPI`；`Init`->`RenderCommand::Init`、`Shutdown`->`RenderCommand::Shutdown`、`BeginScene`->`RenderCommand::Clear`、`Submit`->`RenderCommand::DrawIndexed`、`OnWindowResize`->`RenderCommand::SetViewport`。Vulkan clip-space Y 翻转逻辑原样保留于 `BeginScene(camera)`。
- `CMakeLists.txt`：`DMGE_SOURCES`/`DMGE_HEADERS` 紧邻 `Renderer.cpp`/`Renderer.h` 注册 `RenderCommand.cpp`/`.h`。

**设计：** `RendererAPI`=纯虚接口、`RenderCommand`=静态门面（拥有后端实例、转发原子命令）、`Renderer`=场景编排（缓存相机视图投影、`Submit` 提交绘制）。拆分后场景无关代码（ImGui 层、调试渲染器）可经 `RenderCommand` 发底层命令，无需依赖 `Renderer` 的场景状态；`Renderer` 不再 `#include` 后端头文件。行为零回归：`Application.cpp` 调用点（`Renderer::Init`/`OnWindowResize`/`Shutdown`/`BeginScene`/`Submit`）签名与语义不变；初始化守卫从 `Renderer` 各方法迁至 `RenderCommand` 各命令。

**涉及文件：** `Renderer/RenderCommand.h`（新增）、`Renderer/RenderCommand.cpp`（新增）、`Renderer/Renderer.h`、`Renderer/Renderer.cpp`、`CMakeLists.txt`、`ENGINE_SUMMARY.md`

### 2026-07-20 - 新增 CPU 性能分析器（Profiler）+ ImGui multi-viewport

自研轻量、零依赖的 CPU 帧级 profiler，经 `DMGE_PROFILE_SCOPE` 宏插桩、`Profiler` 单例帧末聚合、`ProfilerLayer` ImGui overlay 呈现；并启用 ImGui multi-viewport 使 profiler 窗口可拖出主窗口成独立平台窗口。

**改动：**
- 新增 `Debug/Profiler.h` / `.cpp`：`Profiler` 单例（Meyers）+ RAII `ProfilerScopeTimer`；`steady_clock` 纳秒计时，`WriteResult` 经 mutex 保护写入 `m_pending`（可跨线程）；`EndFrame` 锁内按 `const char*` 聚合 `count/total/min/max`、锁外按 Total 降序排序发布到 `m_lastAggregates`；帧时间用指数移动平均算 `m_fps` + 240 帧滚动历史。`BeginFrame`/`EndFrame` 独立于 scope 采集（`DMGE_PROFILE=OFF` 时 FPS/帧时间仍可用）。
- 新增 `Debug/ProfilerLayer.h` / `.cpp`：`LayerType::Tool` overlay，`Application::Initialize` 自动挂载；`OnImGuiRender` 绘制 FPS / 帧时间折线图（`PlotLines`，y 轴自适应）/ per-scope 表格（`BeginTable`，按 Total 降序）；`OnEvent` 监听 F1（过滤 auto-repeat）切换显隐，不消费事件。
- `Core/Application.cpp`：include；`Initialize` 末尾 `PushOverlay(ProfilerLayer)`；`MainLoop` 帧首 `Profiler::Get().BeginFrame()`、帧尾 `EndFrame()`，五阶段（Event Pump/Update/Render/ImGui/Swap）各加 `DMGE_PROFILE_SCOPE`（`{ }` 限定 RAII 生命周期）。
- `CMakeLists.txt`：`option(DMGE_PROFILE ... ON)` + `$<$<BOOL:DMGE_PROFILE>:DMGE_PROFILE>` 编译定义；`DMGE_SOURCES`/`DMGE_HEADERS` 注册 Debug 四文件。
- `DMGameEngine.h`：公共头接入 `Profiler.h` / `ProfilerLayer.h`。
- ImGui multi-viewport：`ImGui/ImGuiLayer.cpp` `OnAttach` 设 `ImGuiConfigFlags_ViewportsEnable` + 视口样式（`WindowRounding=0`、`WindowBg` 不透明）；`End` 在 `RenderDrawData` 后调 `UpdatePlatformWindows` + `RenderPlatformWindowsDefault`（OpenGL 保存/恢复 GL context，Vulkan 按同模式）。新增 `#include <GLFW/glfw3.h>`。
- `CMakeLists.txt` ImGui 检测块：`file(STRINGS imgui.h REGEX "ImGuiConfigFlags_ViewportsEnable")` 自动判断是否 docking 分支，仅当时定义 `DMGE_IMGUI_VIEWPORTS` 并编译视口代码；附 `CMAKE_CONFIGURE_DEPENDS` 指向 `imgui.h`，换分支后自动重新检测（`file(STRINGS)` 不自带依赖）。

**设计：** 三层解耦（插桩宏 / 数据管线 / UI）：`ProfilerLayer` 只读、崩溃不影响计时；换数据结构不影响调用点。`const char*` 作 map key = 按调用点聚合、零拷贝；锁内只读+清空、锁外排序，临界区最小。mutex 而非 thread_local 规避 DLL 边界风险（单线程渲染无竞争、worker 低频写入）。`DMGE_PROFILE_SCOPE` 为唯一入口，后端可切换（换宏体接 Tracy，调用点零改动）。视口能力经 `DMGE_IMGUI_VIEWPORTS` 开关保护，master 分支下不编译视口代码以避免"未声明标识符 `ImGuiConfigFlags_ViewportsEnable`"编译失败。CPU-only（GPU 计时不在范围）。

**涉及文件：** `Debug/Profiler.h`（新增）、`Debug/Profiler.cpp`（新增）、`Debug/ProfilerLayer.h`（新增）、`Debug/ProfilerLayer.cpp`（新增）、`Core/Application.cpp`、`ImGui/ImGuiLayer.cpp`、`CMakeLists.txt`、`DMGameEngine.h`、`ENGINE_SUMMARY.md`
### 2026-07-22 - 新增运行时开发者控制台（Console + ConsoleLayer）

自研轻量、零依赖的运行时开发者控制台，按反引号键（GraveAccent）唤出，输入命令查询 / 控制引擎内部状态。沿用 Profiler 的子系统模式：单例数据层 + Tool overlay。

**改动：**
- 新增 `Debug/Console.h` / `.cpp`：`Console` 单例（Meyers）+ `ConsoleCommand`（name / description / handler）注册表 + 滚动缓冲（`ConsoleLine` + `ConsoleLineKind`：Input / Output / Error / System）+ 命令历史 + `Execute`（空白分词，支持双引号整段参数与 `\` 转义，未知命令 / 异常分别走 Error 行）+ `AutoComplete`（前缀匹配、返回排序匹配表）。主线程访问，无锁（注册在 `OnAttach`、执行 / UI 均在主线程，匹配 Profiler 的单线程渲染假设）。
- 新增 `Debug/ConsoleLayer.h` / `.cpp`：`LayerType::Tool` overlay，`Application::Initialize` 自动挂载（紧随 ProfilerLayer）。`OnEvent` 监听 `KeyPressedEvent`，`GetRepeatCount()==0` 过滤自动重复，`KeyCode::GraveAccent` 切换显隐（默认隐藏）并消费该键；切换时清空输入缓冲 + 重置历史游标。`OnImGuiRender` 绘制滚动区（按 `ConsoleLineKind` 着色 + 自动滚底）+ 输入行（`InputText` + `EnterReturnsTrue|CallbackCompletion|CallbackHistory|NoHorizontalScroll`）：回车执行、Tab 前缀补全（多匹配打印清单 + 公共前缀）、Up / Down 历史。
- 内置命令（`ConsoleLayer::OnAttach` 注册，lambda 调单例、零状态）：`help`（列命令 + 描述）、`clear`、`echo`、`stat <fps|frame>`（读 `Profiler::Get()`）、`profile <on|off|status>`（`Profiler::SetEnabled`）、`vsync <on|off|status>`（`Window::SetVSync`）、`renderer api`（`Renderer::GetAPI`）、`quit`（`Application::Quit`）。游戏层可经 `Console::Get().Register(...)` 注册自定义命令。
- `Core/Application.cpp`：include；`Initialize` 在 ProfilerLayer 后 `PushOverlay(ConsoleLayer)`。
- `CMakeLists.txt`：`DMGE_SOURCES` / `DMGE_HEADERS` 注册 Debug 四文件。无 CMake option（Console 无插桩宏可门控，参照 Profiler 的数据 / UI 层无条件编译与挂载）。
- `DMGameEngine.h`：公共头接入 `Console.h` / `ConsoleLayer.h`。

**设计：** 三层解耦（命令注册表 / 滚动缓冲 + 执行 / ImGui UI），`Console` 不依赖任何引擎子系统（纯字符串进、字符串出），`ConsoleLayer` 负责把内置命令接到 Profiler / Renderer / Window / Application —— 与 Profiler / ProfilerLayer 的数据 vs UI 拆分一致。输入聚焦屏蔽由既有 `ImGuiLayer::OnEvent`（`WantCaptureKeyboard` 时标记 Handled）负责，控制台打字时游戏层不收按键。GraveAccent 键先于 ImGuiLayer 收到（overlay 反序遍历），开关可靠；切换清缓冲规避 ImGui 可能入队的杂散反引号字符。`Console` 主线程访问无需锁（与 Profiler 的「单线程渲染、worker 低频写」前提一致；命令注册预期在 `OnAttach`）。未含日志回显（方案 B 的 spdlog sink）与 cvar 持久化（方案 C），留作后续演进。

**涉及文件：** `Debug/Console.h`（新增）、`Debug/Console.cpp`（新增）、`Debug/ConsoleLayer.h`（新增）、`Debug/ConsoleLayer.cpp`（新增）、`Core/Application.cpp`、`CMakeLists.txt`、`DMGameEngine.h`、`ENGINE_SUMMARY.md`

### 2026-07-22 - Profiler 新增 GPU draw call / index 计数

为 Profiler 增加每帧 GPU 绘制提交统计：draw call 数与消耗的 index 数，经 `RenderCommand::DrawIndexed` 统一累计，`ProfilerLayer` UI 展示 `Draws` / `Indices`。弥补 profiler 此前仅 CPU 计时的不足，提供最基础的 GPU 提交量观测。

**改动：**
- `Debug/Profiler.h`：新增每帧累计器 `m_drawCalls` / `m_drawIndices` 与发布快照 `m_lastDrawCalls` / `m_lastDrawIndices`；内联 `AddDrawCall(indexCount=0)` 累加方法；`GetDrawCalls()` / `GetDrawIndices()` 只读 getter。不随 `DMGE_PROFILE` 开关，始终启用。
- `Debug/Profiler.cpp`：`BeginFrame` 清零 `m_drawCalls` / `m_drawIndices`；`EndFrame` 将其发布到 `m_lastDrawCalls` / `m_lastDrawIndices` 供 UI 读取（与 FPS / scope 聚合的发布时机并列）。
- `Renderer/RenderCommand.cpp`：`#include` Profiler.h / VertexArray.h；`DrawIndexed` 在转发后端前读 `VertexArray::GetIndexBuffer()->GetCount()` 调 `Profiler::Get().AddDrawCall(indexCount)`。
- `Debug/ProfilerLayer.cpp`：UI 增加 `Draws: %u` / `Indices: %u` 文本行（`SameLine` 对齐）。

**设计：** 仅统计经 `RenderCommand::DrawIndexed` 的场景绘制，ImGui 经自身后端绘制不计入（故不计 profiler 自身 UI 渲染开销）。累计器在 `BeginFrame` 清零、`EndFrame` 发布，UI 读 `m_lastDraw*`（上一帧稳定快照），与 FPS / scope 表的读取时机一致。draw call 计数为无条件启用，提供最基础的 GPU 提交量观测；真正的 GPU 侧计时（GL query / timestamp）仍留作后续。index 计数可推算三角形数（`/3`），但未统计 vertex 数（`VertexBuffer` 缺 `GetSize` / `GetVertexCount` 接口）。单线程渲染路径无需原子。

**涉及文件：** `Debug/Profiler.h`、`Debug/Profiler.cpp`、`Debug/ProfilerLayer.cpp`、`Renderer/RenderCommand.cpp`、`ENGINE_SUMMARY.md`
### 2026-07-22 - 新增 RenderQueue（延迟提交 + 按 material/shader 排序分组）

渲染批处理第一层：把逐物体立即式提交改为帧末统一排序提交，消除冗余的 shader/material 绑定与每物体重复上传 `u_ViewProjection`。draw call 数量不变，但每 draw 的状态绑定与 uniform 上传从「每物体」降到「每组」。

**改动：**
- 新增 `Renderer/RenderQueue.h`：`Renderable` 结构体（`Ref<Material>` 或 `Ref<Shader>` 二选一 + `Ref<VertexArray>` + `Transform`）与 `RenderQueue` 类（`Clear` / 两个 `Submit` 重载 / `Flush(viewProjection)` / `GetCount`）。`Flush` 末尾自动清空队列。
- 新增 `Renderer/RenderQueue.cpp`：`Flush` 按排序键（material 优先 tag 0、shader-only tag 1，再按对象地址）使同组连续；遍历时跟踪 `lastMaterial` / `lastShader`，仅在组切换时 `Bind` + 设一次 `u_ViewProjection`，每个 renderable 只设 `u_Transform` 再 `RenderCommand::DrawIndexed`。
- `CMakeLists.txt`：`DMGE_SOURCES` / `DMGE_HEADERS` 紧邻 `RenderCommand.cpp` / `.h` 注册 `RenderQueue.cpp` / `.h`。
- `src/DMGameEngine/DMGameEngine.h`：公共头在 `RenderCommand.h` 后接入 `RenderQueue.h`。

**设计：** 第一层批处理（状态排序分组），不合并几何、不实例化，draw call 数不变。`Renderable` 兼容 `Renderer::Submit` 的两个重载（material 版优先、shader-only 兜底）；以 `material->Bind()` 为最小重绑单位（一次含 shader + 全部 uniform），同组内 `u_ViewProjection` 只设一次，消除现有 `Submit` 逐物体重复上传 VP 的冗余。`Flush` 自动清空，调用方每帧 `Clear` + 多次 `Submit` + 末尾 `Flush`。尚未接入 `Renderer`（`Submit` 仍立即式），为后续把 `Renderer` 改为延迟提交预留。

**涉及文件：** `Renderer/RenderQueue.h`（新增）、`Renderer/RenderQueue.cpp`（新增）、`CMakeLists.txt`、`src/DMGameEngine/DMGameEngine.h`、`ENGINE_SUMMARY.md`

### 2026-07-22 - Console stat 命令新增 draws 分支（draw call 查看 + 开关）

运行时控制台新增查看 / 启停 draw call 统计的入口，复用上一条 `Profiler` draw call / index 计数。

**改动：**
- `Debug/ConsoleLayer.cpp`：`stat` 命令增加 `draws` / `drawcalls` 子分支，usage 改为 `stat <fps|frame|draws>`；`p` 由 const 引用改为非 const 以支持开关。无参或 `status` 显示 `Draw calls: N | Indices: M (tracking: on/off)`；`on` / `off` 调 `Profiler::SetDrawCallTracking` 启停。
- `Debug/Profiler.h`：`AddDrawCall` 增加 `m_drawCallTracking` 门控（禁用时提前 return）；新增 `SetDrawCallTracking` / `IsDrawCallTracking` 与成员 `m_drawCallTracking`（默认 `true`，行为不变）。

**设计：** draw call 计数本无条件启用（开销极小，类比 FPS），tracking 开关仅作调试观测：关闭后 `AddDrawCall` 提前返回、`GetDrawCalls()` 停在 0，重新开启恢复。开关与 `Profiler::m_enabled`（控制 scope timer）解耦，互不影响。计数链路沿用上一条已就绪的 `RenderCommand::DrawIndexed` → `AddDrawCall` → `EndFrame` 快照 → `stat draws` 读取。

**涉及文件：** `Debug/ConsoleLayer.cpp`、`Debug/Profiler.h`、`ENGINE_SUMMARY.md`

### 2026-07-23 - RenderQueue 接入 Renderer（立即式改为延迟提交）

把 Renderer 的逐物体立即式 Submit 改为入队、帧末统一排序提交，衔接上一条已就绪但未接入的 RenderQueue。Submit 签名不变，调用方零改动。

**改动：**
- `Renderer/Renderer.h`：include `RenderQueue.h`；私有新增 `static RenderQueue s_Queue`；注释更新为「Submit 入队、EndScene/Flush 提交」。
- `Renderer/Renderer.cpp`：定义 `s_Queue`；两个 `BeginScene` 重载末尾 `s_Queue.Clear()`；两个 `Submit` 重载改为 `s_Queue.Submit(...)` 入队，移除原有 `Bind` / `SetMat4("u_ViewProjection"/"u_Transform")` / `DrawIndexed`；`EndScene` 委托 `Flush()`；`Flush` 调 `s_Queue.Flush(s_SceneData.ViewProjectionMatrix)`。`OnWindowResize` 的 `SetViewport` 等不变。

**设计：** 绘制从立即式变延迟式——`Submit` 只入队，实际 `Bind` + uniform 上传 + `DrawIndexed` 统一在 `EndScene`（经 `Flush`）排序分组后执行。调用方工作流不变（`BeginScene` -> `Submit…` -> `EndScene`）。绘制顺序由「Submit 调用顺序」变为「material/shader 分组」，半透明物体依赖的 back-to-front 会被打乱，需透明物体单独通道（不入队、按距离排序提交）。`Flush` 亦可中途显式提交（多 pass）。`EndScene` 委托 `Flush` 是组合而非重复——`Flush` 为提交原语，`EndScene` 为帧生命周期边界，留帧级收尾（计数重置 / debug marker / 帧结束事件）扩展点。draw call 数量不变，每帧 shader bind 与 `u_ViewProjection` 上传从 N 次降到「每组 1 次」。

**涉及文件：** `Renderer/Renderer.h`、`Renderer/Renderer.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-23 - RendererAPI 实例化绘制扩展（DrawIndexedInstanced + per-instance 属性）

渲染批处理第二层：为后端加实例化绘制能力，同一 mesh 一次 draw 画 N 个有 per-instance 差异的实例。含 draw 命令与 per-instance 顶点属性两套改动，覆盖 OpenGL / Vulkan 双后端。

**改动：**
- `Renderer/RendererAPI.h`：新增纯虚 `DrawIndexedInstanced(const VertexArray&, uint32_t instanceCount, uint32_t baseInstance = 0)`。
- `Renderer/RenderCommand.h` / `.cpp`：门面 `DrawIndexedInstanced` 转发；`.cpp` 计入 profiler（`AddDrawCall(indexCount * instanceCount)`，与 `DrawIndexed` 的 `AddDrawCall(indexCount)` 风格一致：1 draw call、indices 按 instanceCount 缩放）。
- `Platform/OpenGL/OpenGLRendererAPI.h` / `.cpp`：override + `glDrawElementsInstanced`（core 3.3）；`baseInstance` 暂不发出（需 GL 4.2 `glDrawElementsInstancedBaseInstance`），以兼容 core 3.3 loader。
- `Platform/Vulkan/VulkanRendererAPI.h` / `.cpp`：override + 提取私有 `DrawIndexedCommon`，`DrawIndexed`（1 实例）与 `DrawIndexedInstanced`（N 实例）共用之，末行 `vkCmdDrawIndexed(cmd, indexCount, instanceCount, 0, 0, firstInstance)`，完整支持 baseInstance。
- `Renderer/Shader.h`：`BufferElement` 新增 `bool PerInstance`（默认 false）+ 构造参数，向后兼容现有 `BufferElement(type, name, normalized)` 用法。
- `Platform/OpenGL/OpenGLVertexArray.cpp`：`AddVertexBuffer` 中 Float/Int 属性若 `element.PerInstance` 则 `glVertexAttribDivisor(index, 1)`；矩阵类型保持原有 divisor=1 不变。
- `Platform/Vulkan/VulkanVertexArray.cpp`：`RebuildLayout` 检测 buffer 内是否含 `PerInstance` 元素，该 binding 设 `VK_VERTEX_INPUT_RATE_INSTANCE`，并断言禁止同一 buffer 内混合 per-vertex / per-instance（实例数据必须独立 VBO）；`binding.inputRate` 计入 layout hash 使 pipeline 缓存区分两类 rate。

**设计：** 实例化需 draw 命令 + per-instance 数据通道两套配合，缺一则画出的 N 个实例无差异。per-instance 数据（如 transform）放独立 instance VBO、`BufferElement` 标 `PerInstance=true`：OpenGL 走 attribute 级 `glVertexAttribDivisor(1)`，Vulkan 走 binding 级 `inputRate=INSTANCE`（Vulkan rate 是 binding 粒度，故 per-vertex / per-instance 不能混在同一 buffer）。`baseInstance`：Vulkan 经 `firstInstance` 完整支持；OpenGL 暂忽略以保兼容（接口保留，后续接 GL 4.2 BaseInstance 版本）。profiler 计数把一次 instanced draw 记为 1 draw call、indices 计 `indexCount * instanceCount`。实现中 Vulkan 的 `DrawIndexed` 曾因 single-quoted 跨行拼接撇号把函数头写坏，已改用 here-string（撇号字面）+ IndexOf 定位重建为 `DrawIndexed` / `DrawIndexedInstanced` / `DrawIndexedCommon` 三函数结构。draw 命令未接入 `Renderer`（`Submit` 仍非实例化入口），待上层 `Renderer::SubmitInstanced` 提供后再面向调用方。

**涉及文件：** `Renderer/RendererAPI.h`、`Renderer/RenderCommand.h`、`Renderer/RenderCommand.cpp`、`Platform/OpenGL/OpenGLRendererAPI.h`、`Platform/OpenGL/OpenGLRendererAPI.cpp`、`Platform/Vulkan/VulkanRendererAPI.h`、`Platform/Vulkan/VulkanRendererAPI.cpp`、`Renderer/Shader.h`、`Platform/OpenGL/OpenGLVertexArray.cpp`、`Platform/Vulkan/VulkanVertexArray.cpp`、`ENGINE_SUMMARY.md`
### 2026-07-24 - 解耦 Application 与 CameraController（引入 ClearFrame / DefaultSceneLayer）

**背景：** Application（Core 层）此前持有 `DM::Ref<CameraController> m_ActiveController`，在 `OnEvent` / `OnUpdate` / Stage 3 渲染三处转发并以其调用 `Renderer::BeginScene(camera)`。这把"单相机"模型烧进帧编排器、让 Core 反向依赖 Renderer（CameraController）、并与正在成型的 Scene 系统（SceneCamera 由场景 transform 驱动）竞争相机所有权；而调用方（game）本就自持 controller Ref 再注册一份，持有权冗余。

**改动：**
- `Renderer/Renderer.h` / `.cpp`：新增 `static void ClearFrame()`（仅 `RenderCommand::Clear()`，每帧清帧缓冲一次）；`BeginScene()` / `BeginScene(camera)` **不再清帧缓冲**，只缓存 view-projection 并 `s_Queue.Clear()`。清屏与相机 pass 解耦，使多场景层各自 `BeginScene/EndScene` 多 pass 渲染时不互相清屏。
- `Core/Application.h`：删除 `class CameraController;` 前向声明、`SetActiveCameraController` / `GetActiveCameraController`、`m_ActiveController` 成员；顶部文档新增 "Rendering model" 段说明 Application 不再持相机、不再调 BeginScene。
- `Core/Application.cpp`：删除 `#include CameraController.h`、getter/setter 实现、`OnEvent` 中 controller 转发、Stage 2 的 `controller->OnUpdate`；Stage 3 由 `BeginScene(...) / 层渲染 / EndScene` 改为 `ClearFrame()` + 层 `OnRender()`（不再 App 级 BeginScene/EndScene）。
- `Scene/DefaultSceneLayer.h`（新）：Layer 子类，持 `DM::Ref<CameraController>`；`OnRender` 以 `BeginScene(camera) / EndScene()` 括起 `OnSceneRender()` 钩子（无 controller 时用无参 `BeginScene()`）；`OnUpdate` / `OnEvent` 转发给 controller。作为沙箱 / 玩法场景渲染的推荐基类，可派生并 override `OnSceneRender` 提交绘制。
- `DMGameEngine.h` / `CMakeLists.txt`：登记 `DefaultSceneLayer.h`。
- `Renderer/CameraController.h`：文档注释将持有者由 "a Layer (or Scene / Application)" 改为 "a Layer or Scene (e.g. DefaultSceneLayer)"。
- `game/src/main.cpp`：`TestLayer` -> `DemoScene : DefaultSceneLayer`，相机 controller 在 `OnAttach` 内创建并 `SetCameraController`，三角形 submit 移入 `OnSceneRender`；`DemoGame` 不再 `SetActiveCameraController`、移除 `m_Camera` / `m_CameraController` 成员。

**设计：** Application 退化为帧编排器（窗口 / 事件泵 / 层栈 / 帧计时 / ImGui），不再知道相机。相机所有权与绘制提交同处一个场景层，符合"场景拥有相机"的方向，并为多相机（分屏 / 小地图 RTT / 阴影相机等多 pass）留出空间：每场景层各自 `BeginScene(cam) / EndScene`，`ClearFrame` 仅每帧清一次屏以保留前序 pass 输出。事件顺序变化：controller 现经场景层 `OnEvent` 在层栈中接收事件（overlay 在前），而非 App 级在所有层之前——更灵活且符合 UI 先消费输入的惯例。沙箱便利由 `DefaultSceneLayer` 基类提供（替代原先烧进 Application 的 `SetActiveCameraController`）。`OnRender()` Application 回调保留，但世界绘制应放在场景层内自带 `BeginScene/EndScene` 括号。

**涉及文件：** `Renderer/Renderer.h`、`Renderer/Renderer.cpp`、`Core/Application.h`、`Core/Application.cpp`、`Scene/DefaultSceneLayer.h`（新）、`DMGameEngine.h`、`CMakeLists.txt`、`Renderer/CameraController.h`、`game/src/main.cpp`、`ENGINE_SUMMARY.md`
### 2026-07-24 - 新增 FrameBuffer 渲染目标抽象（OpenGL / Vulkan 后端）

**背景：** 此前渲染目标写死在交换链：Vulkan 的 `VulkanGraphicsContext::BeginFrame()` 把 `VkRenderingAttachmentInfo` 硬编码指向 swapchain 颜色 / 深度 view，OpenGL 隐式画到默认 framebuffer（0）。输入端（VertexBuffer / IndexBuffer / Texture）已有完整的后端无关抽象 + 工厂，输出端缺这层对称抽象，导致无法离屏渲染到纹理（RTT），阴影贴图 / 后处理 / 编辑器视口 / 反射等多 pass 功能无从落地。本次补齐 FrameBuffer 类型及其 OpenGL / Vulkan 后端，作为后续「BeginScene 接受可选 target」多 pass 接入的基础（类型先行，不重接帧生命周期）。

**改动：**
- `Renderer/FrameBuffer.h`（新）：FrameBuffer 抽象基类 + `FramebufferSpecification`（Width/Height + `std::vector<FramebufferTextureAttachment>` 多颜色附件支持 MRT + `DepthFormat` 可 `None` 关闭深度做纯深度阴影图 + `Samples` 预留 + `SwapChainTarget` 标记默认帧缓冲）；`GetColorAttachment(i)` 返回 `Ref<Texture2D>` 供画后采样 / 喂 ImGui。
- `Renderer/FrameBuffer.cpp`（新）：`Create()` 工厂按 `Renderer::GetAPI()` 派发，Vulkan 包在 `#ifdef DMGE_VULKAN`。
- `Platform/OpenGL/OpenGLFrameBuffer.h / .cpp`（新）：FBO（`glGenFramebuffers` + `glFramebufferTexture2D`）；颜色 / 深度附件用 `Texture2D`（`glTexStorage2D` 不可变存储）承载以便采样；`Bind()` 存 `GL_DRAW_FRAMEBUFFER_BINDING`、`Unbind()` 还原，使各场景层切 FBO 不互踩；深度 `Depth`/`DepthStencil` 映射 `GL_DEPTH_ATTACHMENT`/`GL_DEPTH_STENCIL_ATTACHMENT`；无颜色附件时 `glDrawBuffer/glReadBuffer(GL_NONE)`（纯深度阴影图）；`glCheckFramebufferStatus` 断言完整。
- `Platform/Vulkan/VulkanFrameBuffer.h / .cpp`（新）：因后端走 VK_KHR_dynamic_rendering，**不创建 VkFramebuffer**；改为拥有 `VulkanTexture2D` 附件（带 `COLOR_ATTACHMENT`/`DEPTH_STENCIL_ATTACHMENT` usage）并对外暴露 `GetColorImageView`/`GetColorFormat`/`GetDepthImageView`/`GetDepthFormat`/`HasDepth`，供将来 `vkCmdBeginRendering` 的 `VkRenderingAttachmentInfo` 取用。`Bind/Unbind` 为空操作（与 `VulkanVertexBuffer` 一致的 per-draw 绑定哲学）。
- `Platform/Vulkan/VulkanTexture2D.h / .cpp`：新增渲染目标构造 `VulkanTexture2D(const Texture2DSpecification&, VkImageUsageFlags extraUsage)` + 成员 `m_ExtraUsage`，`Invalidate()` 把它 OR 进 `VkImage` usage——让附件既可作渲染目标又保持 `SAMPLED` 可采样（否则 VulkanTexture2D 只建 `TRANSFER_DST|SAMPLED` 无法被 attachment 使用）。既有纹理 `m_ExtraUsage=0` 行为不变。
- `DMGameEngine.h` / `CMakeLists.txt`：登记 6 个新文件；Vulkan 段追加 `VulkanFrameBuffer.cpp`。

**设计：** FrameBuffer 是 draw 写出的"画布"，与 VertexBuffer/IndexBuffer（喂入管线的几何数据）属流水线两端的对偶资源；附件用 Texture2D 承载使画完即可采样，是 RTT 成立的根基。富 Specification 选多颜色附件 + 可选深度，为 MRT（G-buffer）/ 阴影图（纯深度）/ 后处理留好空间。Vulkan 后端不复刻 OpenGL 的"Bind 即切目标"模型（dynamic rendering 无全局 FBO 绑定），转而持有附件 view 供接入层填 `VkRenderingAttachmentInfo`，与现有 per-draw 绑定哲学一致。本次刻意不重接帧生命周期：`BeginFrame` 仍硬编码 swapchain、`VulkanRendererAPI::GetOrCreatePipeline` 的 pipeline 格式仍取自 swapchain——离屏 FBO 真正参与渲染需把 `BeginScene` 接受可选 target 且把 FBO 格式纳入 `PipelineKey`（否则 HDR `RGBA16F` 等与 swapchain 格式不匹配触发 validation），列为后续单独议题；当前 FBO 已备好正确 usage/view/format，接入时直接取用即可。

**涉及文件：** `Renderer/FrameBuffer.h`（新）、`Renderer/FrameBuffer.cpp`（新）、`Platform/OpenGL/OpenGLFrameBuffer.h`（新）、`Platform/OpenGL/OpenGLFrameBuffer.cpp`（新）、`Platform/Vulkan/VulkanFrameBuffer.h`（新）、`Platform/Vulkan/VulkanFrameBuffer.cpp`（新）、`Platform/Vulkan/VulkanTexture2D.h`、`Platform/Vulkan/VulkanTexture2D.cpp`、`DMGameEngine.h`、`CMakeLists.txt`、`ENGINE_SUMMARY.md`

### 2026-07-25 - SceneCamera 渲染目标字段 + BeginScene(cam) 便利重载（离屏路径「只需配置摄像机」）

**背景：** 2026-07-24 补齐了 FrameBuffer 渲染目标抽象，但渲染入口仍是「显式传 target」：`Renderer::BeginScene(camera, target)` 的 target 默认 nullptr（swapchain），离屏渲染须每次调用显式传入 FrameBuffer，调用点与目标耦合，且相机自身无法表达「我画到哪儿」。本次把渲染目标变为相机的一个属性，使离屏路径退化为「只配置相机」——`sceneCam.SetRenderTarget(fb)` 后 `BeginScene(sceneCam)` 即画入该 FBO；显式 `BeginScene(cam, target)` 作为底层实现保留，显式 target 仍可覆盖相机自带目标。

**改动：**
- `Renderer/Camera.h`：相机基类新增虚函数 `GetRenderTarget() const -> DM::Ref<FrameBuffer>`（默认返回 nullptr = swapchain），为此 include `Core/Export.h`（取 `DM::Ref`）并前向声明 `FrameBuffer`；任何相机类型可藉此携带渲染目标，基类默认无目标。
- `Scene/SceneCamera.h`：新增 `DM::Ref<FrameBuffer> m_RenderTarget` 成员 + `SetRenderTarget(const DM::Ref<FrameBuffer>&)` / `GetRenderTarget() const override`，SceneCamera 成为首个能携带离屏目标的相机（成员为 shared_ptr，前向声明的 FrameBuffer 即足，无需 include FrameBuffer.h）。
- `Renderer/Renderer.h`：相机入口拆为两个重载——便利 `BeginScene(const Camera& camera)`（画入相机自带目标）与底层 `BeginScene(const Camera& camera, const DM::Ref<FrameBuffer>& target)`（去掉原 `= nullptr` 默认，使 `BeginScene(cam)` 解析到便利重载）；无相机的 `BeginScene(const Ref<FrameBuffer>& = nullptr)` 不变。
- `Renderer/Renderer.cpp`：便利重载实现为 `BeginScene(camera, camera.GetRenderTarget())`，直接委托底层重载；底层重载（含 Vulkan 裁剪空间 Y 翻转）原样保留。

**设计：** 在 Camera 基类开一个返回 `Ref<FrameBuffer>` 的虚钩子（与既有 `OnViewportResize` 虚钩子同构），把「相机画到哪儿」下沉为相机属性而非每次调用显式传参；SceneCamera 持有存储并 override，其余相机（Orthographic/Perspective）继承基类 nullptr 默认，行为不变。重载解析无歧义：`BeginScene()` / `BeginScene(fb)` 走无相机重载，`BeginScene(cam)` 走便利重载取相机目标，`BeginScene(cam, fb)` 走底层重载以显式 target 覆盖相机目标。DefaultSceneLayer 既有 `BeginScene(GetCamera())` 自动解析到便利重载，对 OrthographicCamera（目标 nullptr）仍画 swapchain，向后兼容。本次只动 API 表层与相机属性，未触及 `RenderCommand::BeginRenderPass` / 后端 FBO 绑定（属 2026-07-24 标注的后续接入议题）。

**涉及文件：** `Renderer/Camera.h`、`Scene/SceneCamera.h`、`Renderer/Renderer.h`、`Renderer/Renderer.cpp`、`ENGINE_SUMMARY.md`

### 2026-07-25 - 接入 FrameBuffer 帧生命周期（后端 FBO 绑定 + PipelineKey 格式），解锁离屏渲染

**背景：** 2026-07-24 补齐 FrameBuffer 抽象时将「帧生命周期接入」明确列为后续议题：`BeginFrame` 仍硬编码指向 swapchain、`GetOrCreatePipeline` 的 pipeline 格式仍取自 swapchain，FBO 虽已备好正确的 usage/view/format 却无法参与渲染；2026-07-25 的相机渲染目标字段亦标注「未触及 `RenderCommand::BeginRenderPass` / 后端 FBO 绑定」。本次落地这块后端接入，使离屏 FrameBuffer 真正进入渲染--相机自带目标或显式传 target 的离屏 pass 可作为多 pass RTT（阴影贴图/后处理/视口/反射）落地。

**改动：**
- `Renderer/RendererAPI.h`：前向声明 `FrameBuffer`；新增纯虚 `BeginRenderPass(FrameBuffer* target)` / `EndRenderPass()` 跨后端接口（target 为 nullptr 或 `SwapChainTarget` = 默认/swapchain，离屏 FrameBuffer 则开始针对它的 pass）。
- `Renderer/RenderCommand.h` / `.cpp`：门面层转发 `BeginRenderPass` / `EndRenderPass`（带初始化断言）。
- `Renderer/Renderer.h` / `.cpp`：底层 `BeginScene(const Camera&, const Ref<FrameBuffer>& target)` 在缓存 view-projection + 清队列后调 `BeginRenderPass(target.get())`；`EndScene()` 改为 `Flush()` 后再 `EndRenderPass()`（绘制先记入当前 pass 再结束）。便利重载 `BeginScene(cam)` 经 `GetRenderTarget()` 委托到底层重载，离屏相机即由此走离屏 pass；全 swapchain 路径（目标 nullptr）行为不变。
- `Platform/Vulkan/VulkanRendererAPI.h` / `.cpp`：采用「swapchain pass 全帧打开 + 离屏临时替换」策略--swapchain 目标仅记录活动格式并保持 `BeginFrame` 开启的 pass 不动（ImGui 仍在其内记录，全 swapchain 路径字节级不变）；离屏目标 `vkCmdEndRendering` 结束 swapchain pass、把 FBO 颜色/深度附件 `UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL / DEPTH_STENCIL_ATTACHMENT_OPTIMAL`、以 `loadOp=CLEAR` 开启针对 FBO 的 pass 并设视口为 FBO 尺寸；`EndRenderPass` 结束 FBO pass、附件转回 `SHADER_READ_ONLY_OPTIMAL`（供后续采样）后以 `loadOp=LOAD` 重启 swapchain pass 并恢复视口，保留先前输出。新增 `m_ActiveColorFormat` / `m_ActiveDepthFormat` / `m_InOffscreenPass` / `m_ActiveFrameBuffer` 状态，`Clear()` 设 swapchain 格式为基线。
- 同上两文件：`PipelineKey` 增加 `VkFormat colorFormat` / `depthFormat`（含 `operator==` 与 `PipelineKeyHash`）；`GetOrCreatePipeline` 写入两者并改用 `m_ActiveColorFormat` / `m_ActiveDepthFormat` 填 `VkPipelineRenderingCreateInfo`（不再取 swapchain 格式）--HDR `RGBA16F` 等离屏目标由此获独立缓存 pipeline，避免与 swapchain 格式不匹配触发 validation。
- `Platform/OpenGL/OpenGLRendererAPI.h` / `.cpp`：`BeginRenderPass` 绑定 FBO 并 `glClear`（镜像 Vulkan `loadOp=CLEAR`，ClearFrame 只清了默认帧缓冲），`EndRenderPass` 解绑还原；nullptr/swapchain 目标为 no-op。新增 `m_ActiveTarget` 成员。
- `Platform/Vulkan/VulkanTexture2D.h`：暴露 `GetVkImage()`（布局屏障 / FBO 附件需原始 VkImage）。
- `Platform/Vulkan/VulkanFrameBuffer.h` / `.cpp`：新增 `GetColorImage(i)` / `GetDepthImage()`（委托附件纹理 `GetVkImage`）供离屏 pass 布局转换取用。
- `Platform/Vulkan/VulkanGraphicsContext.h` / `.cpp`：把文件局部 `TransitionImageLayout` 提升为 `VulkanGraphicsContext::TransitionImageLayout` 公开静态（原匿名命名空间助手改名 `TransitionImageLayoutImpl` 并由其转发），供 `VulkanRendererAPI` 过渡 FBO 附件布局。

**设计：** 选「swapchain pass 全帧打开、离屏 pass 临时替换」而非「把 pass 生命周期整体移入 BeginScene」，是为了不动 ImGui--其主视口路径（`ImGui_ImplVulkan_RenderDrawData`）不自开/自结 dynamic-rendering pass，依赖 swapchain pass 在 Stage 4 仍开启；本方案让 ImGui 与全 swapchain 路径不变，只在离屏 `BeginScene(target)` 做「结束 swapchain pass -> 开 FBO pass -> 结束 FBO pass -> 以 loadOp=LOAD 重启 swapchain pass」的对称 juggle，可叠加多个串行离屏 pass。pipeline 格式纳入 `PipelineKey` 是「FBO 附件格式与 swapchain 不同（如 HDR RGBA16F）致 pipeline 复用错配、触发 validation」的根因修复。与 2026-07-25 相机渲染目标形成「显式底层 primitive（`BeginScene(cam, target)`，覆盖无相机场景如阴影贴图/全屏后处理）+ 相机便利层（`BeginScene(cam)`，覆盖视口/小地图等稳定视图->目标绑定）」的分层。

**涉及文件：** `Renderer/RendererAPI.h`、`Renderer/RenderCommand.h`、`Renderer/RenderCommand.cpp`、`Renderer/Renderer.h`、`Renderer/Renderer.cpp`、`Platform/Vulkan/VulkanRendererAPI.h`、`Platform/Vulkan/VulkanRendererAPI.cpp`、`Platform/Vulkan/VulkanGraphicsContext.h`、`Platform/Vulkan/VulkanGraphicsContext.cpp`、`Platform/Vulkan/VulkanFrameBuffer.h`、`Platform/Vulkan/VulkanFrameBuffer.cpp`、`Platform/Vulkan/VulkanTexture2D.h`、`Platform/OpenGL/OpenGLRendererAPI.h`、`Platform/OpenGL/OpenGLRendererAPI.cpp`、`ENGINE_SUMMARY.md`

**范围：** 单颜色附件 + 可选深度（匹配引擎现有单颜色 blend 状态）；depth-only（0 颜色，纯阴影贴图）与 MRT（>1 颜色）由 `FramebufferSpecification` 支持但 pipeline blend 仍假设 1 颜色附件，离屏路径对 0 颜色断言 `GetColorAttachmentCount() > 0`，留作后续。

### 2026-07-26 - Vulkan 后端正确性修复（阶段 0：A/C/D/F）+ 测试/CI 骨架（E2）

Vulkan 后端 `VulkanBackendReview.md` 列出的正确性隐患 A/C/D/F 全部落地修复，并首次建立单元测试 + CI 基线。修复后 Vulkan 后端 + 全部改动编译链接通过（`dmge_tests` 链接含 Vulkan 后端的 `DMGameEngine.dll`），5 个纯逻辑单元测试通过。详见 `VULKAN_FIXES.md`。

**改动：**
- **A - pipeline 缓存随 swapchain recreate 清空**：`VulkanRendererAPI` 新增 `OnSwapchainRecreate()`（销毁全部缓存 `VkPipeline`、清空 `m_Pipelines`、保留 `VkPipelineCache`）；`VulkanGraphicsContext::RecreateSwapchain()` 末尾调用它；`VulkanGraphicsContext.cpp` include `VulkanRendererAPI.h`。
- **C - RequestResize 接线**：`GraphicsContext` 基类新增 `virtual void RequestResize(uint32_t, uint32_t) {}`（默认 no-op，OpenGL 隐式 resize 无需实现）；`VulkanGraphicsContext::RequestResize` 标 `override`；`RecreateSwapchain` 改为始终 `glfwGetFramebufferSize` 取真实像素（RequestResize 仅作触发标志，避免 DPI 逻辑/像素不一致）；`WindowsWindow::WindowData` 加 `GraphicsContext* context`，Init 设 `m_data.context = m_context.get()`，`glfwSetWindowSizeCallback` 调 `data.context->RequestResize(w,h)`。
- **D - VK_CHECK release 不再静默**：`VulkanDebug.h` 的 `#else` 分支由 `(void)(x)` 改为检查 `VkResult != VK_SUCCESS` 并 `DMGE_LOG_ERROR`（不断言，避免 release 崩溃）。
- **F - 未绑定 sampler 槽绑定 dummy texture**：`VulkanRendererAPI` 新增全局 1×1 dummy（`VkImage`/`VmaAllocation`/`VkImageView`/`VkSampler`）+ `CreateDummyResources()`/`DestroyDummyResources()`（Init 创建经 `ImmediateSubmit` 转 `UNDEFINED->SHADER_READ_ONLY_OPTIMAL`，析构销毁）；`WriteDescriptorSet` 中 unbound sampler 槽改写 dummy 的 sampler/view（消除 "uninitialized binding" validation + 未定义采样）。
- **E2 - 测试 + CI 骨架**：新增 `tests/CMakeLists.txt`（GoogleTest via `FetchContent` v1.14.0，`gtest_discover_tests`）+ `tests/test_core.cpp`（5 个纯逻辑 case：Timestep 转换 / ShaderDataTypeSize / BufferLayout stride-offset）；顶层 `CMakeLists.txt` 加 `option(DMGE_BUILD_TESTS)` + `add_subdirectory(tests)` + `enable_testing()`；新增 `.github/workflows/ci.yml`（Windows+Ubuntu 矩阵 + ctest + Vulkan 编译 job）。
- **测试落地踩坑修复**（`tests/CMakeLists.txt`）：CMP0135 设 NEW 消除 FetchContent 时间戳警告；dmge_tests 补 `/utf-8`（spdlog fmt bundled 在 MSVC 下 static_assert 要求，引擎 `/utf-8` 是 PRIVATE 不传播给消费者）；`DMGE_BUILD_SHARED=ON` 时 POST_BUILD `copy_if_different` 把 `DMGameEngine.dll` 拷到 `dmge_tests.exe` 同目录（解决 `gtest_discover_tests` 运行 exe 时的 `0xc0000135 STATUS_DLL_NOT_FOUND`）。

**设计：** 阶段 0 目标是堵 Vulkan 后端正确性漏洞 + 建回归基线，让后续重构（ECS / 抽象收敛）「改得动、改得对」。A 项经 07-25 离屏改动（`PipelineKey` 已含 `colorFormat/depthFormat` + `Clear()` 每帧刷新 `m_ActiveColorFormat`）后，实际从「device lost 高风险」降为「旧 format pipeline 小泄漏」，`OnSwapchainRecreate` 是防御性清理 + 双保险。C 项把 resize 触发从「等 acquire 返回 OUT_OF_DATE」提前到「window 回调即设 `m_NeedsResize`」，消除 resize 后 1..N 帧 viewport/extent 不匹配闪烁。D 项让 release 失败有诊断而非静默崩。F 项用 dummy 让 descriptor 始终完整写入。测试仅覆盖纯逻辑（header-only，headless 可跑）；渲染冒烟测试（offscreen Init->BeginScene->Submit->EndScene->Shutdown）留后续。测试编译验证顺带确认 Vulkan 后端 + A/C/D/F 改动可编译链接。

**涉及文件：** `Platform/Vulkan/VulkanDebug.h`、`Platform/Vulkan/VulkanRendererAPI.h`、`Platform/Vulkan/VulkanRendererAPI.cpp`、`Platform/Vulkan/VulkanGraphicsContext.h`、`Platform/Vulkan/VulkanGraphicsContext.cpp`、`Renderer/GraphicsContext.h`、`Platform/Windows/WindowsWindow.h`、`Platform/Windows/WindowsWindow.cpp`、`CMakeLists.txt`、`tests/CMakeLists.txt`（新增）、`tests/test_core.cpp`（新增）、`.github/workflows/ci.yml`（新增）、`VULKAN_FIXES.md`（新增）、`ENGINE_SUMMARY.md`

**验证：** CLion（Ninja + MSVC 19.51 + Vulkan ON + DLL）配置构建，`dmge_tests` 5/5 通过；引擎含 Vulkan 后端 + A/C/D/F 改动编译链接通过。运行时 Validation Layer 场景验证（A/C/D/F 场景）待后续跑实际渲染程序确认。

---

### 2026-07-26 - 阶段 1b ECS 核心（entt + Entity/Component/Scene/System）

引入 ECS 架构：Entity（ID）+ Component（纯数据 POD）+ System（纯逻辑），Scene 持 entt::registry + System 列表 + Transform 层级（dirty 传播/剪枝/环检测/孤儿子节点）。详见 `ECS_DESIGN.md` / `ECS_CONCEPTS.md`。

**改动：**
- 引入 entt v3.13.2（FetchContent，header-only，PUBLIC link `EnTT::EnTT`，方案 A Hazel 风格暴露）
- 新增 `Scene/Entity.h`：`Entity = uint32_t` + `NullEntity`
- 新增 `Scene/Components/`：`IDComponent`（UUID）/ `TagComponent`（名字）/ `TransformComponent`（本地变换 + 三叉链 Parent/FirstChild/NextSibling + world 缓存 + dirty）/ `MeshComponent`（VAO + Material）/ `CameraComponent`（SceneCamera + Primary）+ `Components.h` 聚合
- 新增 `Scene/Systems/`：`System` 基类（OnUpdate/OnRender/OnEvent + Scene&）/ `TransformSystem`（拓扑序深度优先 + dirty 短路）/ `MeshRenderSystem`（`view<Transform,Mesh>` -> `Renderer::Submit`，零适配对接）
- 新增 `Scene/Scene.h/.cpp`：entt::registry + System 列表 + `CreateEntity`（挂 ID+Tag+Transform + 随机 UUID）/ `DestroyEntity`（孤儿子节点）/ `SetParent`（环检测）/ `MarkSubtreeDirty`（剪枝）/ `OnUpdate`/`OnRender`
- 改 `Scene/DefaultSceneLayer.h`：加 `Ref<Scene>` + `SetScene()`，`OnUpdate` tick Scene，`OnSceneRender` 默认调 `Scene::OnRender`（向后兼容，无 Scene 时行为不变）
- 接入 `CMakeLists.txt`（DMGE_SOURCES/HEADERS）+ `DMGameEngine.h`
- 新增 `tests/test_ecs.cpp`：11 个 ECS 逻辑单元测试（CreateEntity 组件 / world matrix / 父子累乘 / dirty 传播 / 环检测 / 孤儿 / 兄弟链），全绿

**设计：** ECS 把身份/数据/行为分离，数据导向（DOD）+ cache 友好。Transform 层级用三叉链（存 Entity ID 非指针，POD 友好），dirty propagation（父变标子树 dirty，TransformSystem 拓扑序重算，`!Dirty` 短路跳过静止子树）。`MeshRenderSystem` 调 `Renderer::Submit(Material, VAO, WorldMatrix)` 签名零适配。`DefaultSceneLayer` 仍管相机 bracket（方案 A，`CameraComponent` 接管留后续）。glm 用 `mat4_cast`（GTC 稳定 API，非 GTX `toMat4`）。

**涉及文件：** `Scene/Entity.h`、`Scene/Scene.h`、`Scene/Scene.cpp`、`Scene/Components/*`、`Scene/Systems/*`、`Scene/DefaultSceneLayer.h`、`CMakeLists.txt`、`DMGameEngine.h`、`tests/test_ecs.cpp`、`tests/CMakeLists.txt`

**验证：** CLion（Ninja + MSVC 19.51）配置构建，`dmge_tests` 11/11 通过；引擎编译链接通过。

---

### 2026-07-27 - 阶段 1a AssetManager（UUID 方案）+ ShaderLibrary 整合 + nlohmann/json

引入统一资源管理体系：资源身份（UUID）与位置（path）分离，资源移动只更新元数据 path，UUID 不变，引用不断。详见 `ASSET_DESIGN.md` / `ASSET_UUID_CONCEPTS.md`。

**改动：**
- 新增 `Asset/` 目录：`AssetTypes.h`（`AssetUUID = uint64` + `AssetType` 枚举 + `AssetMetadata{path/type/dependencies}` + `AssetTypeOf<T>` 特化 Shader/Texture2D）/ `AssetHandle.h`（纯身份令牌，只存 UUID，不存路径）/ `AssetLoader.h`（模板 + Shader/Texture2D 特化，复用 `Shader::Create`/`Texture2D::Create`）/ `AssetManager.h/.cpp`（单例 + 三表：`m_Registry` UUID->元数据 / `m_PathToUUID` 路径->UUID / `m_Cache` UUID->`DM::WeakRef<void>`；`Load<T>(uuid)`/`Load<T>(path)`/`Register`/`GetUUID`/`GetMetadata`/`LoadRegistry`/`SaveRegistry`/`CleanUnused`/`Clear`）
- 引入 nlohmann/json v3.11.3（FetchContent，PRIVATE；registry 持久化 JSON + 1c 序列化复用）
- `Core/Export.h`：加 `DM::WeakRef<T> = std::weak_ptr<T>`（与 `DM::Scope`/`DM::Ref` 风格统一）
- `Renderer/Shader.cpp`：`ShaderLibrary::Load(name, filepath)` 委托 `AssetManager::Load<Shader>(filepath)`（去重缓存归 AssetManager，名字索引 `m_Shaders` 保留）
- 接入 `CMakeLists.txt`（DMGE_SOURCES/HEADERS + nlohmann/json FetchContent）+ `DMGameEngine.h`
- 新增 `tests/test_asset.cpp`：6 个 AssetManager 单元测试（Register / 幂等 / GetUUID / registry 往返 / CleanUnused / Clear），headless 不需资源文件，全绿

**设计：** UUID 方案三张表：`UUID->AssetMetadata`（path/type/dependencies）、`path->UUID`（反向查找）、`UUID->DM::WeakRef<void>`（缓存去重 + 自动释放）。`AssetHandle` 只存 UUID（序列化存 UUID 短且稳定，资源移动引用不断）。registry JSON 持久化（`LoadRegistry`/`SaveRegistry`）让 UUID 跨重启稳定。`ShaderLibrary` 整合：`Load` 委托 `AssetManager`（资源去重归 AssetManager），名字索引保留。`DM::WeakRef` 统一智能指针风格。异步加载 / 热重载 / 延迟释放队列留后续（依赖 3d Job System / 0a-B deletion queue）。

**涉及文件：** `Asset/AssetTypes.h`、`Asset/AssetHandle.h`、`Asset/AssetLoader.h`、`Asset/AssetManager.h`、`Asset/AssetManager.cpp`、`Core/Export.h`、`Renderer/Shader.cpp`、`CMakeLists.txt`、`DMGameEngine.h`、`tests/test_asset.cpp`、`tests/CMakeLists.txt`

**验证：** CLion（Ninja + MSVC 19.51）配置构建，`dmge_tests` 17/17 通过（11 ECS + 6 Asset）；引擎编译链接通过。

---

### 2026-07-30 - `AssetLoader<Material>` + `AssetLoader<VertexArray>`（.mat/.mesh 资源加载）

1c 资源加载补全：Material 从 .mat JSON 加载，VertexArray（mesh 资源）从 .mesh JSON 加载。MeshComponent 反序列化后 VAO + Material 真正加载，MeshRenderSystem 可渲染。详见 `ASSET_DESIGN.md` / `SCENE_DESIGN.md`（SVN 文档）。

**改动：**
- `AssetLoader<Material>`（`AssetLoader.cpp`）：读 .mat JSON（shader + uniforms）-> `AssetManager::Load<Shader>` -> `DM::CreateRef<Material>(shader)` -> 按 type 调 `Set*`（Int/Float/Float2/Float3/Float4/Mat4/IntArray，对应 `UniformValue` variant）。Material 无 Create 工厂（直接构造），不持 texture（.mat 无 textures 字段）。
- `AssetLoader<VertexArray>`（`AssetLoader.cpp`）：读 .mesh JSON（layout + vertices + indices）-> 动态构造 `BufferLayout` -> `VertexBuffer` + `IndexBuffer` -> `VertexArray`。引擎无 Mesh 类，Mesh 资源 = VertexArray。.mesh 自定义 JSON 格式（layout type/name + float vertices + uint32 indices）。标准 .obj/.gltf 需解析库（assimp/tinygltf），留后续。
- `Renderer/Shader.h`：`BufferLayout` 加 `AddElement(BufferElement)`（动态构造 layout，原只有 `initializer_list` 构造，.mesh 解析需动态）。
- `Scene/SceneSerializer.cpp`：`DeserializeMesh` 集成 `Load<Material>` + `Load<VertexArray>`（MeshComponent 反序列化后 VAO/Material 填充，需 AssetManager registry 预加载 UUID->path）。
- `CMakeLists.txt`：DMGE_SOURCES 加 `AssetLoader.cpp`。
- nlohmann/json 保持 PRIVATE（AssetLoader.h 只声明特化，实现在 .cpp）。

**设计：** `AssetLoader<T>` 模板特化覆盖 Shader/Texture2D/Material/VertexArray 四类资源。Material/VertexArray 实现在 .cpp（用 nlohmann/json），AssetLoader.h 只声明特化，保持 json PRIVATE。Material 用实际 API（无 Create 工厂，`CreateRef<Material>(shader)` 直接构造；uniform `Set*` 按 type tag 分发；不持 texture）。VertexArray 用 .mesh JSON（layout + vertices + indices），`BufferLayout` 加 `AddElement` 支持动态构造。MeshComponent 反序列化调 `Load<Material>/Load<VertexArray>`，需 AssetManager registry 预加载（LoadRegistry）。标准 mesh 格式（FBX/OBJ/GLTF）需解析库 + 引入 Mesh 中间类，留后续。

**涉及文件：** `Asset/AssetLoader.h`、`Asset/AssetLoader.cpp`、`Renderer/Shader.h`、`Scene/SceneSerializer.cpp`、`CMakeLists.txt`

**验证：** CLion（Ninja + MSVC）配置构建编译通过；运行时资源加载验证（.mat/.mesh + registry）待 demo。

---

### 2026-07-31 - Mesh 类 + AssetLoader&lt;Mesh&gt; + MaterialOverrides（per-instance 材质覆盖）

引入 Mesh 中间类（CPU 数据 + 懒上传 VertexArray），分离文件解析与 GPU 上传。MeshComponent 改持 `Ref<Mesh>`，材质下沉到 SubMesh（资源级，多材质支持）；MaterialOverrides 补 per-instance 材质覆盖（MaterialInstance base + overrides）。详见 `ASSET_DESIGN.md` / `SCENE_DESIGN.md` / `MATERIAL_OVERRIDE_SERIALIZATION.md`（SVN 文档）。

**改动：**
- 新增 `Asset/Mesh.h/.cpp`：`Mesh`（Vertices/Indices/SubMeshes/Layout + `GetVertexArray()` 懒上传缓存）+ `SubMesh`（IndexOffset/IndexCount/MaterialAsset）。Mesh 作 CPU 端中间表示，解析（AssetLoader）与 GPU 上传（GetVertexArray）解耦。
- `AssetLoader<Mesh>`（`AssetLoader.h/.cpp`）：读 .mesh JSON（layout + vertices + indices + submeshes）-&gt; Mesh，复用 ParseShaderType。.mesh 无 submeshes 时默认单子网格覆盖全部索引。
- `MeshComponent` 改：持 `Ref<Mesh>`（去 VAO/Material/MaterialAsset，材质在 SubMesh）+ `std::vector<DM::Ref<MaterialInstance>> MaterialOverrides`（per-instance 覆盖，null = 用 Mesh 默认材质）。
- `MeshRenderSystem` 改：用 `mc.Mesh->GetVertexArray()` + `MaterialOverrides[0]`（有则用，MaterialInstance is-a Material）否则 `Load<Material>(SubMeshes[0].MaterialAsset)`。
- `SceneSerializer.cpp`：`DeserializeMesh` 改 `Load<Mesh>`（去 Load&lt;Material&gt;/Load&lt;VertexArray&gt;，材质在 Mesh 资源）；`SerializeMesh` 去 material 字段（材质在 SubMesh）。
- `tests/test_scene.cpp`：`RoundTripPreservesMeshAssetUUID` 去 MaterialAsset。
- 接入 `CMakeLists.txt`（Mesh.cpp/.h）+ `DMGameEngine.h`。

**设计：** Mesh 作 CPU 中间表示（解析结果 + 懒上传 VertexArray），材质在 SubMesh（资源级，多材质模型支持），MaterialOverrides 补 per-instance 覆盖（MaterialInstance base + overrides，不覆盖时用 base Material 零开销）。这是 Unity/Unreal 标准模式（Mesh 资源自带材质 + MaterialInstance per-instance 覆盖）。多子网格 draw range（Renderer::Submit range 重载）+ MaterialOverrides 序列化（见 `MATERIAL_OVERRIDE_SERIALIZATION.md`）+ assimp（FBX/OBJ/GLTF 导入）留后续。

**涉及文件：** `Asset/Mesh.h`、`Asset/Mesh.cpp`、`Asset/AssetLoader.h`、`Asset/AssetLoader.cpp`、`Scene/Components/MeshComponent.h`、`Scene/Systems/MeshRenderSystem.h`、`Scene/SceneSerializer.cpp`、`tests/test_scene.cpp`、`CMakeLists.txt`、`DMGameEngine.h`

**验证：** CLion（Ninja + MSVC）配置构建编译通过；`dmge_tests` 现有测试全绿（MaterialOverrides 不参与序列化，运行时设）；运行时 Mesh 加载 + MaterialOverrides per-instance 覆盖验证待 demo。

### 2026-07-31 - MaterialOverrides 序列化（.scene materialOverrides + 共享 UniformSerializer）

实现 `MATERIAL_OVERRIDE_SERIALIZATION.md` 路线：per-instance 材质覆盖（`MaterialInstance` 的 overrides）持久化到 `.scene`，补齐上一条「MaterialOverrides 序列化留后续」的缺口。base Material 不存（从 Mesh 资源重建），只存 overrides（uniform name + type + value）+ submesh 索引。

**改动：**
- `Renderer/Material.h`：`MaterialInstance` 加 `public GetOverrides()`（序列化只读访问 `m_Overrides`）。
- 新增 `Renderer/UniformSerializer.h`（**内部头**，含 nlohmann/json，仅被 .cpp 包含）：`UniformValueToJson`（`UniformValue` variant -> JSON `{type,value}`，`std::visit`+`if constexpr`，覆盖 Int/Float/Float2/Float3/Float4/Mat4/IntArray 全部 7 种）+ `ApplyUniform`（JSON -> `Material::Set*` 按 type tag 派发）。.mat 加载与 scene 覆盖反序列化共用一份 type dispatch，避免重复。
- `Asset/AssetLoader.cpp`：`AssetLoader<Material>::Load` 的 uniform 派发循环改用 `ApplyUniform`，去重 type-tag 分支。
- `Scene/SceneSerializer.cpp`：`SerializeMesh` 写 `materialOverrides` 数组（每元素 submesh 索引 + uniforms map，null/空跳过，base 不存）；`DeserializeMesh` 从 `Mesh->SubMeshes[i].MaterialAsset` 经 `AssetManager::Load<Material>` 重建 base -> `CreateRef<MaterialInstance>(base)` -> `ApplyUniform` 还原 overrides，Mesh/base 不可加载时优雅跳过（不留悬空项）。
- `tests/test_scene.cpp`：新增 `NullShader` 桩（无 GPU 也能构造 `Material`/`MaterialInstance`）+ `SaveWritesMaterialOverrides`（覆盖全部 variant 类型的序列化结构，grep 验证）+ `LoadIsGracefulWhenMeshBaseUnavailable`（base 不可用时 Load 不崩、优雅丢弃）。
- `CMakeLists.txt`：`DMGE_HEADERS` 加 `UniformSerializer.h`。

**设计：** base Material 不序列化（从 `Mesh.SubMeshes[i].MaterialAsset` 重建，避免重复存 + 不一致），只存 overrides。复用 `.mat` 的 `{type,value}` uniform 格式，`UniformValueToJson`/`ApplyUniform` 提取为共享内部头 `UniformSerializer.h`，`.mat` 加载（`AssetLoader<Material>`）与 scene 覆盖反序列化（`SceneSerializer`）共用。`UniformValueToJson` 用 `std::visit`+`if constexpr`（variant 闭合，新增类型编译期报错保证穷尽）。测试难点：`MaterialInstance` 需 base Material（GPU Shader + .mat 资源），headless 无法构造，故按文档「简化策略」——Save 侧用 `NullShader` 桩验序列化结构，Load 侧只验优雅跳过；`ApplyUniform`（Load 方向）留 demo 验证（与现有 asset 测试一致）。nlohmann/json 保持 PRIVATE（`UniformSerializer.h` 不进 `DMGameEngine.h`，仅被 `.cpp` 包含）。

**涉及文件：** `Renderer/Material.h`、`Renderer/UniformSerializer.h`（新增）、`Asset/AssetLoader.cpp`、`Scene/SceneSerializer.cpp`、`tests/test_scene.cpp`、`CMakeLists.txt`

**验证：** CLion（Ninja + MSVC）配置构建编译通过（`DMGameEngine.dll` + `dmge_tests.exe` 链接成功；已清除 `UniformSerializer.h` 的 C4702 unreachable 告警）；新增 2 个 headless 测试随 `dmge_tests` 运行（用户执行）；运行时 MaterialOverrides 往返（含真实 base 重建）待 demo。


### 2026-08-01 - assimp 网格导入（FBX/OBJ/GLTF -> Mesh）+ headless 测试

实现 Mesh 资产按文件扩展名加载：`.mesh` 走原 JSON，`.fbx/.obj/.gltf/.glb` 走 assimp 导入。补齐 `AssetLoader<Mesh>` 此前只读 `.mesh` 的缺口，使 Mesh 资产能直接从常见模型格式加载。

**改动：**
- `Asset/AssetLoader.cpp`：`AssetLoader<Mesh>::Load` 改为按（小写）扩展名分发——`.mesh` 走原 JSON（重构为内部 `LoadMeshFromMeshJSON`，行为不变），`.fbx/.obj/.gltf/.glb` 调 `LoadMeshViaAssimp`，未知扩展名返回 `nullptr`。新增 `GetLowerExtension` 工具，加 `<algorithm>`/`<cctype>`。
- 新增 `Asset/MeshImporterAssimp.cpp`（内部 TU，assimp include 局限于此）：`LoadMeshViaAssimp` 用 `Assimp::Importer` 读文件 -> `aiScene` -> 递归遍历节点树（累积变换烘焙进顶点：位置用 4×4，法线/切线用逆转置 3×3）-> 展平所有 `aiMesh` 进单个 `Mesh`（一个 `SubMesh` 一个 `aiMesh`，索引重映射进共享 VB）。顶点布局取所有 `aiMesh` 通道的**超集**（`a_Position` 恒有，`a_Normal`/`a_TexCoords`/`a_Tangent` 任一 mesh 有则含，缺失通道补零保统一步长）。导入标志 `Triangulate|GenSmoothNormals|JoinIdenticalVertices|CalcTangentSpace|ValidateDataStructure`（不用 `FlipUVs`——GL 纹理加载器已垂直翻转）。材质映射：每个 `aiMaterial` 按 `<模型目录>/<材质名>.mat` 查找，找到则 `AssetManager::Register(...,AssetType::Material)` 得 `AssetHandle`（仅 UUID，实际材质延迟到渲染时加载），找不到则置空 + `DMGE_LOG_WARN`。
- `Asset/AssetTypes.h`：补 `AssetTypeOf<Material>()`/`AssetTypeOf<Mesh>()` 特化（含前向声明）。此前只有 Shader/Texture2D，`AssetManager::Load<Mesh/Material>` 类型校验会失败。
- `Asset/AssetLoader.h`：`AssetLoader<Mesh>` 特化加 `DMGE_API`，导出 `Load` 符号，使消费者（exe，如 demo）能链接调用 `AssetManager::Load<Mesh>`（特化原先未导出，exe 直接调用会 `LNK2019`）。
- `CMakeLists.txt`：`DMGE_SOURCES` 加 `MeshImporterAssimp.cpp`（assimp 本已链接，无新依赖）。
- 新增 `tests/test_assimp_import.cpp` + `tests/CMakeLists.txt` 接入：两个 headless 测试（无 GPU）。通过 Scene 序列化往返触发 DLL 内部 `AssetLoader<Mesh>::Load`（`Material`/`VertexArray` 特化未导出，故经已导出的 `SceneSerializer` 路径驱动导入，避免 exe 直接调用的链接问题）：
  - `ObjLoadsIntoMesh`：用 assimp 自带 `box.obj`，断言 Mesh 非空、顶点/索引/子网格非空、`Σ SubMesh.IndexCount == Indices.size()`、布局首元素 `a_Position`、负向材质映射（只读目录无 `.mat` -> `MaterialAsset` 无效）。
  - `MaterialMatLookupResolvesHandle`：临时目录自写 `tri.obj`+`tri.mtl`（材质名 `TriMat`）+ 同目录 `TriMat.mat`，断言 `SubMesh.MaterialAsset` 有效、元数据 `Type==Material`、路径文件名为 `TriMat.mat`。

**设计：** Mesh 是 CPU 中间表示（解析结果 + 懒上传 VertexArray）。assimp 导入把整个场景展平为单 VB/IB + 多 SubMesh（每个 = 索引区间 + 材质 AssetHandle），契合现有 `Mesh::GetVertexArray()` 与 `MeshRenderSystem`。材质走外部 `.mat` 查找（不生成材质、不绑纹理——`Material` 尚无纹理绑定），缺失则置空由渲染系统优雅跳过。导入标志无 `FlipUVs`（与现有翻转纹理加载器一致）。`AssetLoader<Mesh>` 加 `DMGE_API` 是本次关键修复——让 Mesh 资产管线对 exe 消费者可用。

**涉及文件：** `Asset/AssetLoader.cpp`、`Asset/AssetLoader.h`、`Asset/AssetTypes.h`、`Asset/MeshImporterAssimp.cpp`（新增）、`CMakeLists.txt`、`tests/test_assimp_import.cpp`（新增）、`tests/CMakeLists.txt`

**验证：** CLion（Ninja + MSVC，VS 开发者环境）构建通过——`DMGameEngine.dll` + `dmge_tests.exe` 链接成功，新 TU 在 `/W4` 下无新增告警（仅引擎公共头既有 C4251/C4100）。两个 headless 测试随 `dmge_tests` 运行通过（OBJ 结构 + 索引完整性 + 负向/正向材质映射）。消费者侧（demo）已端到端验证 `AssetManager::Load<Mesh>("box.obj")` 经 assimp 加载可用（`DMGE_API` 导出生效，48 顶点数据/36 索引/1 子网格，~110 FPS 渲染 3000 实例）。


### 2026-08-02 - 预编译头（PCH）加速引擎编译

新增预编译头 `dmge_pch.h`，经 CMake `target_precompile_headers` 以 `/FI` 强制包含进 `DMGameEngine` 目标的每个 C++ TU，缓存 stdlib + glm + spdlog（~2 MB 模板）的解析结果，避免 52+ 个 TU 重复解析。`PRIVATE` + `$<$<COMPILE_LANGUAGE:CXX>:...>` 守卫（避开 C 源 `glad.c`）。`DMGE_USE_PCH` 选项默认开启，可一键关闭。

**改动：**
- 新增 `src/DMGameEngine/dmge_pch.h`：PCH 内容--14 个标准库高频头 + glm（glm.hpp + matrix_transform/matrix_inverse/quaternion/type_ptr）+ spdlog（spdlog.h + stdout_color_sinks/basic_file_sink）。范围严格限定为稳定第三方 + 标准库，**不含**引擎自身头、glad/GLFW/imgui/stb/nlohmann_json/assimp/Vulkan（平台/后端专属或仅少数 TU 使用，会膨胀 `.pch` 并污染无关 TU；引擎活跃头进 PCH 会触发全量重编）。
- `CMakeLists.txt`：新增 `option(DMGE_USE_PCH ... ON)` + `target_precompile_headers(DMGameEngine PRIVATE $<$<COMPILE_LANGUAGE:CXX>:...dmge_pch.h>)`。CMake 生成 `cmake_pch.hxx` 包装器（`#include dmge_pch.h`），用 `/Yc` 编译一次生成 `cmake_pch.cxx.pch` 快照，给每个 C++ `.cpp` 加 `/FI`+`/Yu`+`/Fp`。
- `tests/CMakeLists.txt`：**未**使用 `REUSE_FROM`。实测 Vulkan 后端开启时引擎 PCH 解析 Vulkan SDK 自带 glm，测试目标解析 vendored glm，`REUSE_FROM` 把引擎那份 glm 注入测试 TU 与测试自身 vendored glm 冲突（`C2011`/`C2955` 重定义）。测试目标（5 个小 TU）不使用 PCH 回到改动前状态。根因是项目存在两份 glm（vendored + Vulkan SDK 自带）的既有问题。
- 新增 `PRECOMPILED_HEADER.md`：PCH 原理、制作方法（CMake `/FI` vs 手动 `#include` vs MSVC `/Yc`/`/Yu`）、9 条注意事项、本项目设计决策与实测冲突记录。

**设计：** 选「强制包含」（`/FI` 经编译参数注入）而非「手动 `#include`」--零源码改动、全部 C++ TU 自动覆盖、一个开关可关闭。PCH 只加速不替代：源文件原有 `#include` 照旧保留，PCH 只把最重的公共头预解析缓存。PCH 不含引擎自身头，使其与 `DMGE_BUILD_DLL`/`DMGE_PROFILE` 等宏解耦。

**涉及文件：** `src/DMGameEngine/dmge_pch.h`（新增）、`CMakeLists.txt`、`tests/CMakeLists.txt`、`PRECOMPILED_HEADER.md`（新增）

**验证：** CLion（Ninja + MSVC）配置构建通过--`cmake_pch.cxx.pch`（266 MB）生成，61/61 C++ 源带 `/Yu` 使用 PCH，`DMGameEngined.dll` 链接成功；`glad.c`（C 源）被 CXX 守卫正确排除；`dmge_tests.exe` 移除 `REUSE_FROM` 后构建通过。


### 2026-08-03 - 光照系统（阶段 A+B：前置基础设施 + 基础前向 Blinn-Phong 光照）

从「unlit 纯纹理采样」升级为「前向 Blinn-Phong 光照」--支持 1 方向光 + 16 点光源 + 8 聚光灯 + 环境光。顶点法线已由 assimp 导入就绪但此前从未使用，本次补齐着色器层光照管线，引擎从「技术演示」变为「可视化 3D 引擎」。

**阶段 A - 前置基础设施（非破坏性增量）：**
- `Renderer.h/cpp`：`SceneData` 新增 `CameraPosition`（`BeginScene` 时从 `inverse(view)[3]` 提取）；新增 `static SceneLightData s_LightData` + `SubmitLightData()`；`BeginScene` 每帧 `s_LightData.Clear()`。
- `RenderQueue.h/cpp`：`Flush` 签名扩展为 `(viewProjection, cameraPosition, lightData)`；每组材质绑定时上传 `u_CameraPosition` + 全部光照 uniform（方向光/点光源数组/聚光灯数组/环境光）；每个 draw 额外上传 `u_NormalMatrix`（`transpose(inverse(mat3(model)))`，以 mat4 适配现有 `SetMat4` API）。
- `Material.h/cpp`：新增 `TextureSlot` 结构（`Ref<Texture>` + `uint32_t slot`）+ `m_Textures` / `m_TextureOverrides` 映射；`Bind()` 在上传 uniform 后绑定纹理并设置 sampler uniform；`SetTexture` / `HasTexture` / `GetTexture` 虚方法（MaterialInstance 覆写为本地覆盖）。
- `AssetLoader.cpp`：`.mat` 格式新增 `"textures"` 字段（`{ "u_AlbedoTexture": {"path": "...", "slot": 0} }`），通过 `AssetManager::Load<Texture2D>` 加载并 `SetTexture`。
- `OpenGLShader.cpp`：`GetUniformLocation` 改为容错--未声明的 uniform 返回 -1（`glUniform*` 按 GL 规范静默忽略），不再触发断言。这让 RenderQueue 可以无条件上传光照 uniform，不使用光照的着色器自动跳过。

**阶段 B - 基础前向光照：**
- 新增 `Renderer/Light.h`：光照数据结构（`DirectionalLightData` / `PointLightData` / `SpotLightData` / `SceneLightData`），纯数据 POD，后端无关。`MAX_POINT_LIGHTS=16` / `MAX_SPOT_LIGHTS=8`。
- 新增 `Scene/Components/LightComponent.h`：ECS 光源组件（`enum Type { Directional, Point, Spot }` + 颜色/强度/衰减系数/锥角/环境光强度）。
- 新增 `Scene/Systems/LightSystem.h`：ECS 系统，遍历 `LightComponent` + `TransformComponent`，从 `WorldMatrix` 提取光源位置/方向，填充 `SceneLightData` 并调 `Renderer::SubmitLightData`。方向约定：方向光/聚光灯沿实体 -Z 轴发射（OpenGL forward），首个方向光驱动环境光。
- 新增 `shaders/BlinnPhong.glsl`：Blinn-Phong 前向光照着色器（环境光 + 漫反射 + 镜面反射 + 衰减 + 聚光灯锥角），支持反照率纹理（`u_UseTexture` 开关）。
- `SceneSerializer.cpp`：新增 `SerializeLight` / `DeserializeLight`，注册为 `"Light"` 组件。
- `Components.h` / `DMGameEngine.h` / `CMakeLists.txt`：注册新头文件到公共 API + 构建。

**附带修复：**
- `LightSystem.h` / `MeshRenderSystem.h`：`using System::System;` 改为显式 `public` 构造函数--MSVC 的 `std::make_shared` 经 `std::construct_at` 内部路径无法访问 protected 继承构造函数（`C2248`）。
- `AssetLoader.h`：`AssetLoader<Material>` 和 `AssetLoader<VertexArray>` 补 `DMGE_API`--此前仅 `AssetLoader<Mesh>` 有导出标记，消费者 exe 直接 `Load<Material>` 会 `LNK2019`。

**设计：** 光照 uniform 按「展平数组」命名（`u_PointLights_position[0]` 等）而非 GLSL struct 数组（`u_PointLights[0].position`），因当前 Shader 抽象仅支持按名 `SetFloat3(name, value)` 单值上传，不支持 struct 成员访问。每组材质绑定时上传一次光照数据（per-group），每个 draw 仅额外上传 `u_Transform` + `u_NormalMatrix`（per-draw）。容错 uniform 查找让 unlit 着色器与 lit 着色器共存于同一 RenderQueue Flush 而不崩溃。PBR/阴影/IBL/延迟渲染属于路线图 3e（远期），本次不涉及。

**涉及文件：** `Renderer/Light.h`（新增）、`Scene/Components/LightComponent.h`（新增）、`Scene/Systems/LightSystem.h`（新增）、`shaders/BlinnPhong.glsl`（新增）、`Renderer/Renderer.h` / `.cpp`、`Renderer/RenderQueue.h` / `.cpp`、`Renderer/Material.h` / `.cpp`、`Platform/OpenGL/OpenGLShader.cpp`、`Asset/AssetLoader.h` / `.cpp`、`Scene/SceneSerializer.cpp`、`Scene/Components/Components.h`、`Scene/Systems/MeshRenderSystem.h`、`DMGameEngine.h`、`CMakeLists.txt`

**验证：** `DMGameEngine` 库 + `dmge_tests` 编译通过；30/30 测试通过（零回归）；`DMGameDemo` 游戏工程编译通过并运行--1500 个 ECS 方块在 Blinn-Phong 光照下渲染（1 方向光 + 4 彩色点光源 + 可见光源体），方块分散漂浮+旋转，光照 uniform 每材质组上传一次。

### 2026-08-03（更新）- 实例化渲染（双路径）+ DLL STL 边界修复

在光照系统基础上新增实例化渲染路径，将 1500 个方块的 draw call 从 1500 降到 1。修复过程中发现并解决了 STL 容器跨 DLL 边界的崩溃问题。

**实例化双路径渲染：**
- 新增 `RenderQueue::SubmitInstanced` + `Renderer::SubmitInstanced`：支持按 (Material, Mesh) 分组，大组（≥8 实例）走 `DrawIndexedInstanced`（1 draw call/组），小组走原有 per-draw 路径。
- `MeshRenderSystem` 重写为双阶段：Phase 1 按 (Material*, Mesh*) 分组实体；Phase 2 按组大小选择路径。instanced VA 缓存于 `m_InstancedVAs`（Mesh* -> {VA, InstanceVB}），跨帧复用，仅 instance buffer 数据每帧 `SetData` 更新。
- 新增 `shaders/BlinnPhongInstanced.glsl`：per-instance `a_InstanceModel`（mat4, locations 0-3）替代 `u_Transform` uniform；mesh 属性移至 locations 4+；法线矩阵在 GPU 顶点着色器内从 instance model 实时计算。
- instanced VA 创建顺序：instance buffer FIRST（locations 0-3）-> mesh vertex buffer SECOND（locations 4+），与 `OpenGLVertexArray` 的 `m_VertexBufferIndex` 递增分配机制匹配。

**DLL STL 边界修复：**
- **问题**：`std::vector<InstancedRenderable> m_InstancedQueue` 作为 `DMGE_API` 类 `RenderQueue` 的成员，跨 DLL 边界时内部状态损坏。C4251 警告已提示风险。第二帧 `push_back` 在 `cap=1, size=0` 的已有缓冲区上崩溃（0xC0000005 access violation）。
- **根因**：exe 和 DLL 各自编译 `RenderQueue.h`，对 `std::vector` 的 sizeof/布局可能因 `_ITERATOR_DEBUG_LEVEL` 差异而不一致。exe 编译 `MeshRenderSystem.h`（header-only，包含 `RenderQueue.h`）时需要 `RenderQueue` 完整布局，布局错位导致 `m_InstancedQueue` 的内部簿记（data/size/capacity/proxy 指针）损坏。
- **修复**：将 `m_InstancedQueue` 从 `RenderQueue` 类中移除，改为 `RenderQueue.cpp` 内部的 `static InstancedRenderable s_InstancedBatches[32]`（Pimpl 思想简化版）。exe 完全看不到这个成员，布局不一致问题消除。
- **诊断过程**：逐步 `fprintf(stderr, ...)` + `fflush(stderr)` 定位。Frame 1 全流程成功，Frame 2 在 `push_back` 崩溃。`reserve(16)` 预分配无效。逐步赋值（`r.Material = material; ...`）成功但 `push_back(std::move(r))` 崩溃。`emplace_back()` 同位置崩溃。改用固定数组后立即修复。
- 新增 `DLL_STL_BOUNDARY.md`：完整记录问题原理、诊断过程、解决方案。

**涉及文件：** `Renderer/RenderQueue.h` / `.cpp`、`Renderer/Renderer.h` / `.cpp`、`Scene/Systems/MeshRenderSystem.h`、`shaders/BlinnPhongInstanced.glsl`（新增）、`DLL_STL_BOUNDARY.md`（新增）

**验证：** 30/30 测试通过；DMGameDemo 运行稳定（~52 FPS，1500 方块 + 5 光源，1 instanced draw call + 5 per-draw draw calls = 6 total，较此前 1505 draw call 降幅 99.6%）。
### 2026-10-03 - Vulkan 正确性阶段 0a：B 项 per-frame deletion queue 落地 + A 项销毁时序加固

复核 `documents/VulkanBackendReview.md` A-F：A/C/D/F 已于 07-26 落地（`VULKAN_FIXES.md` §1-§4 与代码一致），本波落地 B 项并加固 A 项销毁时序。分支 `agent/render-agent/vulkan-correctness`。

- **B 项（本次主体）**：新增 per-frame deletion queue（2 桶 = 2 帧 in-flight）。`VulkanTexture2D/2DArray/Cube` 析构与 `Invalidate`、`VulkanVertexBuffer/IndexBuffer` 析构不再直接 `vkDestroy/vmaDestroy`，改为捕获句柄入桶；`BeginFrame` 在 `vkWaitForFences` 后 flush 当前槽桶。入桶规则为非对称的 `DeletionBucketFor(frameStarted, currentFrame)`（录制中入当前槽、帧间入另一槽——"总入当前槽"在帧间场景另一槽 pending 帧仍引用资源，不安全，证明与性质测试见 `VulkanDeletionQueue.h` / `test_deletion_queue.cpp`）。`VulkanDevice` 新增 `PushDeferDestroy/FlushDeletions/FlushAllDeletions/DeferDestroyTexture/DeferDestroyBuffer`，`Shutdown` 前统一 flush。
- **A 项加固**：`RecreateSwapchain` 开头补 `vkDeviceWaitIdle`——原实现发生在 `vkWaitForFences` 之前，销毁旧 swapchain/旧格式 pipeline 时 in-flight 提交可能仍引用。
- 新坑沉淀：kb/KB-07 K-009（删除桶入桶非对称规则）、K-010（低频销毁路径 device idle 原则）。
- E 项（regex UBO → SPIR-V 反射）本波不做，仍留 2b。

**涉及文件：** `Platform/Vulkan/VulkanDeletionQueue.h`（新增）、`VulkanDevice.h/.cpp`、`VulkanGraphicsContext.h/.cpp`、`VulkanTexture2D.cpp`、`VulkanTexture2DArray.cpp`、`VulkanTextureCube.cpp`、`VulkanVertexBuffer.cpp`、`VulkanIndexBuffer.cpp`、`engine/tests/test_deletion_queue.cpp`（新增）、`engine/tests/CMakeLists.txt`

**验证：** 全量构建（engine + editor + game，Vulkan ON）零 error、/W4 零新增警告；ctest 全绿（含新增 DeletionQueue 4 例）；另以 `DMGE_VULKAN_BACKEND=OFF` 复测 OpenGL 默认路径无回归。运行时 Validation Layer 场景（resize/纹理中途释放）待人工跑 editor/game exe 确认。

### 2026-10-03 - 编辑器阶段 3/4 补完：Play 隔离 + 拖拽建实体 + Scene 管理 + Prefab

分支 `agent/editor-agent/stage3-4`（worktree `.worktrees/editor-agent`）。四个独立 commit：

- **Play mode 快照隔离（阶段 4，commit 22abb0a）**：编辑态/运行态分离。`EnterPlayMode` 用 `SceneDuplicator`（新增 `editor/src/SceneDuplicator.{h,cpp}`，跨 Scene 实体子树深拷贝 + 层级重映射）把编辑态 Scene 拷为运行态副本，Play 期间模拟跑副本、Inspector/Entity CRUD/gizmo 只读禁用；Stop 丢弃副本恢复编辑态，选中项按 UUID 回映射。未走 SceneSerializer JSON 快照——它只存 MeshAsset UUID，程序化网格会被丢（新坑 K-012）。`EditorScene` 拆 m_EditScene/m_PlayScene/m_Scene；`LoadSceneFromFile` 载入空场景不再与默认实体合并；编辑态 dt=0 tick 仅驱动脏变换。
- **Asset Browser 拖模型创建实体（阶段 3，commit 56dac01）**：拖 `.fbx/.obj/.gltf/.glb/.mesh` 到 Viewport 或 Hierarchy（`BeginDragDropTargetCustom` 全窗口目标）新建带 `MeshComponent` 实体；走 `AssetManager::Load<Mesh>` 公共 API（K-002 不触碰），材质编辑器自建默认 Blinn-Phong（helper 提取），回填 MeshAsset UUID 支持序列化往返；Play 期间拒绝。
- **Scene 管理（阶段 3，commit f9a0f60）**：File 菜单重做——New Scene 确认弹窗防丢、Open（路径输入 + 存在性校验）、Save、Save As、最近场景子菜单；最近文件持久化到自管 `editor_config.ini`（已入 .gitignore）。无新依赖，对话框 ImGui 自绘。
- **Prefab 最小版（阶段 3，commit 2273405）**：Hierarchy 右键 Save As Prefab（临时 Scene 承载单实体树 → `SceneSerializer::Save`，引擎零改动）/ Instantiate（Load 后深拷贝进当前场景）；Asset Browser 增加 `.prefab` 图标并浏览 `prefabs/`。
- 另补录 `editor/GIZMO_HITTEST_FIX.md`（K-007 关联，commit dd5b933）；新坑沉淀 K-012/K-013。

**涉及文件：** `editor/src/SceneDuplicator.h/.cpp`（新增）、`editor/src/EditorScene.h/.cpp`、`editor/src/EditorLayer.h/.cpp`、`editor/CMakeLists.txt`、`.gitignore`、`editor/GIZMO_HITTEST_FIX.md`（补录）、kb/KB-07

**验证：** 全量构建（engine+editor+game+tests，build-agent）零 error、editor 侧零新增警告（还消掉 2 个既有 strncpy C4996）；ctest --test-dir build-agent/engine 37/37 全绿。GUI 行为（Play 隔离/拖拽/对话框/Prefab 往返）待人工验证。

### 2026-10-03 - K-011 正解：enable_testing() 挪至根 CMakeLists.txt，ctest 指回构建根目录

分支 `agent/test-agent/enable-testing-root`（worktree `.worktrees/test-agent`）。把 `enable_testing()` 从 `engine/CMakeLists.txt`（L430）挪到根 `CMakeLists.txt`（`add_subdirectory(engine)` 之前），根 build 目录由此生成 `CTestTestfile.cmake`，`ctest --test-dir <build>` 可直接发现全部用例，K-011 的假绿根因消除。engine 侧保留独立构建兜底：仅当 engine 被当作顶层工程构建时（`CMAKE_SOURCE_DIR == CMAKE_CURRENT_SOURCE_DIR`）才自行 `enable_testing()`。`gtest_discover_tests` 无作用域问题——`include(GoogleTest)` 与发现调用都在 `engine/tests/CMakeLists.txt`，由根级 `enable_testing()` 的"目录及子目录"覆盖语义正常工作。CI（`.github/workflows/ci.yml`）ctest 步骤改回 `ctest --test-dir build`（保留一行 K-011 历史注释）；另按 CI 规划补 `clang-tidy` 报告型 job 骨架（Ninja + msvc-dev-cmd 导出 compile_commands.json，`continue-on-error: true` 只出报告不拦截）。`.github/CI.md`、`AGENTS.md` §4 本地等价命令同步改为指向构建根目录；KB-07 K-011 条目末尾追加修复状态行（原条目内容未改写）。

**涉及文件：** `CMakeLists.txt`、`engine/CMakeLists.txt`、`.github/workflows/ci.yml`、`.github/CI.md`、`AGENTS.md`、`kb/KB-07-已知问题与陷阱清单.md`

**验证：** 全新构建目录 `build-agent`（Ninja + vcvars，FetchContent 离线复用主 checkout `_deps`）全流程 configure → build → `ctest --test-dir build-agent`：37/37 全绿（此前根目录 ctest 为 "No tests were found" 假绿）；根 `CTestTestfile.cmake` 含 `subdirs("engine")`，`engine/tests` 生成 37 条测试命令。全量构建零 error；本改动为纯构建编排，无新增 /W4 警告（日志中 engine 侧 C4251/C4100 与 ImGuizmo C4273/C4245 均为存量基线）。无新坑入 KB-07（首过时 dmge_tests.exe 链接遇一次瞬时 LNK1104，重跑即过，判断为文件锁，未立条）。
