# Vulkan Renderer Backend

A Vulkan backend mirroring `src/DMGameEngine/Platform/OpenGL/` lives under
`src/DMGameEngine/Platform/Vulkan/`. It implements the same renderer
abstractions (GraphicsContext, RendererAPI, Shader, VertexBuffer,
IndexBuffer, VertexArray, Texture2D / 2DArray / Cube) on top of Vulkan 1.3
(dynamic rendering) + VMA + shaderc.

## Dependencies (install the Vulkan SDK once)

The Vulkan SDK (https://vulkan.lunarg.com/) provides all three:

- **Vulkan headers + loader** (`vulkan.h`, `vulkan-1.lib`) — `find_package(Vulkan)`.
- **VulkanMemoryAllocator** — single header `vk_mem_alloc.h` (in the SDK `Include`).
- **shaderc** — runtime GLSL → SPIR-V (`shaderc/shaderc.hpp`, `shaderc_combined.lib`).

Set the `VULKAN_SDK` environment variable (the SDK installer does this).

## Enabling the backend

Add this block to `CMakeLists.txt` (before the `target_compile_definitions`
section) and configure with `-DDMGE_VULKAN_BACKEND=ON`:

```cmake
option(DMGE_VULKAN_BACKEND "Build the Vulkan renderer backend" OFF)
if(DMGE_VULKAN_BACKEND)
    find_package(Vulkan REQUIRED)
    find_path(VMA_INCLUDE_DIR NAMES vk_mem_alloc.h
        PATHS "$ENV{VULKAN_SDK}/Include" "$ENV{VULKAN_SDK}/Include/vulkan" REQUIRED)
    find_path(SHADERC_INCLUDE_DIR NAMES shaderc/shaderc.hpp
        PATHS "$ENV{VULKAN_SDK}/Include" REQUIRED)
    find_library(SHADERC_LIBRARY NAMES shaderc_combined shaderc
        PATHS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/Lib/shaderc" REQUIRED)

    target_compile_definitions(DMGameEngine PRIVATE DMGE_VULKAN)
    target_include_directories(DMGameEngine PRIVATE
        "${VMA_INCLUDE_DIR}" "${SHADERC_INCLUDE_DIR}")
    target_link_libraries(DMGameEngine PRIVATE Vulkan::Vulkan "${SHADERC_LIBRARY}")

    target_sources(DMGameEngine PRIVATE
        "src/DMGameEngine/Platform/Vulkan/VulkanDevice.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanSwapchain.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanRendererAPI.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanShader.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanVertexBuffer.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanIndexBuffer.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanVertexArray.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanTexture2D.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanTexture2DArray.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanTextureCube.cpp")
endif()
```

OpenGL remains the default; Vulkan is opt-in.

## Selecting Vulkan at runtime

Call `Renderer::SetAPI(Renderer::API::Vulkan)` **before** `Application::Run()`
(the window / context are created during `Initialize()`, so the API must be
chosen first):

```cpp
DMGameEngine::Renderer::SetAPI(DMGameEngine::Renderer::API::Vulkan);
app.Run();
```

`WindowsWindow` then requests a `GLFW_NO_API` window and builds a
`VulkanGraphicsContext`.

## How OpenGL semantics map to Vulkan

- **Per-draw pipeline state** (blend/depth/cull) is baked into `VkPipeline`s,
  cached by a key of `(shader, vertex layout, blend, depth, cull)` so the
  OpenGL `glEnable`-style state changes are reproduced.
- **Name-based uniforms** (`Shader::SetMat4("u_ViewProjection", ...)`,
  `Material::Bind()`) are supported by rewriting each stage's GLSL at load
  time: standalone `uniform` declarations are folded into one synthesized
  `layout(std140, set=0, binding=0)` UBO block (offsets known without SPIR-V
  reflection), and `uniform sampler*` get explicit bindings. At draw time the
  staging block is committed into a per-frame dynamic UBO slot, one descriptor
  set per draw.
- **Frame lifecycle**: `Renderer::BeginScene()` → `Clear()` acquires the
  swapchain image and begins dynamic rendering (clears color + depth). Draws
  are recorded into the frame's command buffer. `Window::SwapBuffers()` ends
  rendering, submits and presents. The frame stays open across `EndScene()`/
  ImGui so a future Vulkan ImGui backend can record its draws.

## Known limitations / assumptions

- **GLSL**: shaders must use modern desktop GLSL (`#version` is bumped to
  450 core; `out vec4`/explicit locations are injected where missing). Legacy
  `gl_FragColor` or pre-declared `uniform` blocks are not auto-handled —
  prefer standalone `uniform type name;` and explicit `in`/`out` locations.
  Sampler arrays (`uniform sampler2D u[36]`) are not fully supported (use
  individually named samplers).
- **ImGui**: the GL render backend is skipped under Vulkan, so panels are not
  drawn until a `imgui_impl_vulkan` path is added to `ImGuiLayer`.
- **Resource lifetime**: GPU resources (shaders/textures/buffers) must be
  released before `Renderer::Shutdown()` / window destruction (same
  constraint as the OpenGL backend).
- **Validation layers** are enabled in Debug builds.
