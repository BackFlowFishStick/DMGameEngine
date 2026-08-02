cmake_minimum_required(VERSION 3.20)
project(DMGameEngine
    VERSION 0.1.0
    DESCRIPTION "DMGameEngine - A C++ game engine library"
    LANGUAGES C CXX
)

# ── C++ Standard ─────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build Options ────────────────────────────────────────────────
option(DMGE_BUILD_SHARED "Build DMGameEngine as a shared library (DLL)" ON)
option(DMGE_BUILD_EXAMPLES "Build example projects" OFF)
option(DMGE_BUILD_TESTS "Build unit tests (GoogleTest via FetchContent; needs network)" ON)
option(DMGE_PROFILE "Enable engine CPU profiling instrumentation" ON)
option(DMGE_USE_PCH "Enable precompiled header for faster builds (MSVC/GCC/Clang)" ON)

# ── ImGui multi-viewport: auto-detect the docking branch ─────────
# The vendored master-branch ImGui does not define
# ImGuiConfigFlags_ViewportsEnable; only the docking branch does.
# Detect it so the multi-viewport code in ImGuiLayer.cpp is compiled
# only when the vendored ImGui actually supports it (avoids the
# "undeclared identifier" build break seen with the master branch).
set(DMGE_IMGUI_VIEWPORTS OFF CACHE BOOL "ImGui multi-viewport (auto-detected; requires docking branch)" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.h")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.h" _dmge_imgui_vp REGEX "ImGuiConfigFlags_ViewportsEnable")
    # Re-run auto-detection when the vendored ImGui header changes.
    # file(STRINGS) does not register a build-system dependency on its own,
    # so swapping the ImGui branch would otherwise leave DMGE_IMGUI_VIEWPORTS
    # stale (OFF) until the cache is wiped.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.h")
    if(_dmge_imgui_vp)
        set(DMGE_IMGUI_VIEWPORTS ON CACHE BOOL "ImGui multi-viewport (auto-detected; requires docking branch)" FORCE)
    endif()
    unset(_dmge_imgui_vp)
endif()
if(DMGE_IMGUI_VIEWPORTS)
    message(STATUS "ImGui: multi-viewport ENABLED (docking branch detected)")
else()
    message(STATUS "ImGui: multi-viewport disabled (master branch; swap to docking branch to enable)")
endif()

# ── Source Files ─────────────────────────────────────────────────
set(DMGE_SOURCES
    # Core
    "src/DMGameEngine/Core/Application.cpp"
    "src/DMGameEngine/Core/Layer.cpp"
    "src/DMGameEngine/Core/LayerStack.cpp"
    "src/DMGameEngine/Core/Log.cpp"

    # Platform / OpenGL
    "src/DMGameEngine/Platform/OpenGL/OpenGLGraphicsContext.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLIndexBuffer.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLFrameBuffer.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLShader.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTexture2D.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTextureCube.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTexture2DArray.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexArray.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.cpp"

    # Platform / Windows
    "src/DMGameEngine/Platform/Windows/GLFWInput.cpp"
    "src/DMGameEngine/Platform/Windows/WindowsWindow.cpp"

    # Renderer
    "src/DMGameEngine/Renderer/IndexBuffer.cpp"
    "src/DMGameEngine/Renderer/FrameBuffer.cpp"
    "src/DMGameEngine/Renderer/Renderer.cpp"
    "src/DMGameEngine/Renderer/RenderCommand.cpp"
    "src/DMGameEngine/Renderer/RenderQueue.cpp"
    "src/DMGameEngine/Renderer/RendererAPI.cpp"
    "src/DMGameEngine/Renderer/Shader.cpp"
    "src/DMGameEngine/Renderer/Material.cpp"
    "src/DMGameEngine/Renderer/Texture2D.cpp"
    "src/DMGameEngine/Renderer/TextureCube.cpp"
    "src/DMGameEngine/Renderer/Texture2DArray.cpp"
    "src/DMGameEngine/Renderer/VertexArray.cpp"
    "src/DMGameEngine/Renderer/VertexBuffer.cpp"

    # Scene
    "src/DMGameEngine/Scene/Scene.cpp"
    "src/DMGameEngine/Scene/SceneSerializer.cpp"

    # Asset
    "src/DMGameEngine/Asset/AssetManager.cpp"
    "src/DMGameEngine/Asset/AssetLoader.cpp"
    "src/DMGameEngine/Asset/Mesh.cpp"
    "src/DMGameEngine/Asset/MeshImporterAssimp.cpp"

    # ImGui
    "src/DMGameEngine/ImGui/ImGuiLayer.cpp"

    # Debug
    "src/DMGameEngine/Debug/Profiler.cpp"
    "src/DMGameEngine/Debug/ProfilerLayer.cpp"
    "src/DMGameEngine/Debug/Console.cpp"
    "src/DMGameEngine/Debug/ConsoleLayer.cpp"
)

set(DMGE_HEADERS
    # Core
    "src/DMGameEngine/Core/Application.h"
    "src/DMGameEngine/Core/EntryPoint.h"
    "src/DMGameEngine/Core/Export.h"
    "src/DMGameEngine/Core/Input.h"
    "src/DMGameEngine/Core/KeyCodes.h"
    "src/DMGameEngine/Core/Layer.h"
    "src/DMGameEngine/Core/LayerStack.h"
    "src/DMGameEngine/Core/Log.h"
    "src/DMGameEngine/Core/MouseCodes.h"
    "src/DMGameEngine/Core/GamepadCodes.h"
    "src/DMGameEngine/Core/Timestep.h"
    "src/DMGameEngine/Core/Window.h"
    "src/DMGameEngine/Core/Events/ApplicationEvent.h"
    "src/DMGameEngine/Core/Events/Event.h"
    "src/DMGameEngine/Core/Events/KeyEvent.h"
    "src/DMGameEngine/Core/Events/MouseEvent.h"
    "src/DMGameEngine/Core/Events/GamepadEvent.h"

    # Platform / OpenGL
    "src/DMGameEngine/Platform/OpenGL/OpenGLDebug.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLGraphicsContext.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLIndexBuffer.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLFrameBuffer.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLShader.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTextureUtils.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTexture2D.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTextureCube.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTexture2DArray.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexArray.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.h"

    # Platform / Windows
    "src/DMGameEngine/Platform/Windows/GLFWInput.h"
    "src/DMGameEngine/Platform/Windows/WindowsWindow.h"

    # Renderer
    "src/DMGameEngine/Renderer/Camera.h"
    "src/DMGameEngine/Renderer/CameraController.h"
    "src/DMGameEngine/Renderer/OrthographicCamera.h"
    "src/DMGameEngine/Renderer/PerspectiveCamera.h"
    "src/DMGameEngine/Renderer/GraphicsContext.h"
    "src/DMGameEngine/Renderer/IndexBuffer.h"
    "src/DMGameEngine/Renderer/FrameBuffer.h"
    "src/DMGameEngine/Renderer/Light.h"
    "src/DMGameEngine/Renderer/Renderer.h"
    "src/DMGameEngine/Renderer/RenderCommand.h"
    "src/DMGameEngine/Renderer/RenderQueue.h"
    "src/DMGameEngine/Renderer/RendererAPI.h"
    "src/DMGameEngine/Renderer/Shader.h"
    "src/DMGameEngine/Renderer/Material.h"
    "src/DMGameEngine/Renderer/UniformSerializer.h"  # internal: uniform<->JSON (shared .mat + scene override)
    "src/DMGameEngine/Renderer/Texture.h"
    "src/DMGameEngine/Renderer/Texture2D.h"
    "src/DMGameEngine/Renderer/TextureCube.h"
    "src/DMGameEngine/Renderer/Texture2DArray.h"
    "src/DMGameEngine/Renderer/VertexArray.h"
    "src/DMGameEngine/Renderer/VertexBuffer.h"

    # Scene
    "src/DMGameEngine/Scene/SceneCamera.h"
    "src/DMGameEngine/Scene/OrthographicCameraController.h"
    "src/DMGameEngine/Scene/EditorCameraController.h"
    "src/DMGameEngine/Scene/DefaultSceneLayer.h"
    "src/DMGameEngine/Scene/Entity.h"
    "src/DMGameEngine/Scene/Components/Components.h"
    "src/DMGameEngine/Scene/Components/IDComponent.h"
    "src/DMGameEngine/Scene/Components/TagComponent.h"
    "src/DMGameEngine/Scene/Components/TransformComponent.h"
    "src/DMGameEngine/Scene/Components/MeshComponent.h"
    "src/DMGameEngine/Scene/Components/LightComponent.h"
    "src/DMGameEngine/Scene/Components/CameraComponent.h"
    "src/DMGameEngine/Scene/Scene.h"
    "src/DMGameEngine/Scene/SceneSerializer.h"
    "src/DMGameEngine/Scene/Systems/System.h"
    "src/DMGameEngine/Scene/Systems/TransformSystem.h"
    "src/DMGameEngine/Scene/Systems/MeshRenderSystem.h"
    "src/DMGameEngine/Scene/Systems/LightSystem.h"

    # Asset
    "src/DMGameEngine/Asset/AssetTypes.h"
    "src/DMGameEngine/Asset/AssetHandle.h"
    "src/DMGameEngine/Asset/AssetLoader.h"
    "src/DMGameEngine/Asset/Mesh.h"
    "src/DMGameEngine/Asset/AssetManager.h"

    # ImGui
    "src/DMGameEngine/ImGui/ImGuiLayer.h"

    # Debug
    "src/DMGameEngine/Debug/Profiler.h"
    "src/DMGameEngine/Debug/ProfilerLayer.h"
    "src/DMGameEngine/Debug/Console.h"
    "src/DMGameEngine/Debug/ConsoleLayer.h"

    # Public entry point
    "src/DMGameEngine/DMGameEngine.h"
)

# ── Engine Library Target ────────────────────────────────────────
if(DMGE_BUILD_SHARED)
    add_library(DMGameEngine SHARED ${DMGE_SOURCES} ${DMGE_HEADERS})
else()
    add_library(DMGameEngine STATIC ${DMGE_SOURCES} ${DMGE_HEADERS})
endif()

target_include_directories(DMGameEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# ── Dependencies ─────────────────────────────────────────────────
# Structure: dependencies/<libname>/include/   (e.g. dependencies/spdlog-1.x/include/)
#            dependencies/<libname>/lib/       (e.g. dependencies/somelib/lib/foo.lib)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
    file(GLOB DMGE_DEP_INCLUDE_DIRS LIST_DIRECTORIES true
        "dependencies/*/include")
    foreach(DEP_DIR ${DMGE_DEP_INCLUDE_DIRS})
        if(IS_DIRECTORY "${DEP_DIR}")
            target_include_directories(DMGameEngine PUBLIC "$<BUILD_INTERFACE:${DEP_DIR}>")
        endif()
    endforeach()

    file(GLOB DMGE_DEP_LIBS
        "dependencies/*/lib/*.lib"
        "dependencies/*/lib/*.a")
    if(DMGE_DEP_LIBS)
        target_link_libraries(DMGameEngine PRIVATE ${DMGE_DEP_LIBS})
    endif()
endif()

# ── GLFW (built from source) ─────────────────────────────────────
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw-3.4/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw-3.4"
                     "${CMAKE_BINARY_DIR}/glfw")
    target_link_libraries(DMGameEngine PRIVATE glfw)
endif()

# ── GLM (OpenGL Mathematics, header-only) ─────────────────────────
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm-master/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm-master"
                     "${CMAKE_BINARY_DIR}/glm")
    target_link_libraries(DMGameEngine PUBLIC glm)
endif()

# ── GLAD (OpenGL loader, single-file) ─────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glad/src/glad.c")
    target_sources(DMGameEngine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glad/src/glad.c"
    )
endif()

# ── stb_image (image loading, single-file) ────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/stb/stb_image.cpp")
    target_include_directories(DMGameEngine PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependencies/stb>"
    )
    target_sources(DMGameEngine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/stb/stb_image.cpp"
    )
endif()

# ── ImGui (source-based) ───────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.h")
    target_include_directories(DMGameEngine PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends>"
    )
    target_sources(DMGameEngine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_draw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_tables.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_widgets.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_demo.cpp"
        # GLFW + OpenGL3 backends
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_glfw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_opengl3.cpp"
    )
endif()

# ── entt (ECS, header-only via FetchContent) ───────────────────────
# Entity-Component-System library used by the Scene module (ECS redesign,
# see ECS_DESIGN.md). Header-only: FetchContent_MakeAvailable provides the
# EnTT::EnTT interface target. PUBLIC because Scene/Entity.h exposes entt
# types in the public API (see ECS_DESIGN.md §12 - strategy A, Hazel-style).
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(
    entt
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.13.2.zip
)
FetchContent_MakeAvailable(entt)
target_link_libraries(DMGameEngine PUBLIC EnTT::EnTT)

# ── nlohmann/json (JSON serialization, header-only via FetchContent) ────
# Used by AssetManager registry persistence + stage 1c scene serialization.
# PRIVATE: only AssetManager.cpp includes <nlohmann/json.hpp>; the public
# API (AssetManager.h) does not expose json types.
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(DMGameEngine PRIVATE nlohmann_json::nlohmann_json)

# ── assimp (mesh import: FBX/OBJ/GLTF/..., source in dependencies/assimp) ──
# Compiled from source via add_subdirectory. Disable tests/tools/samples/export
# to cut build time; static link to avoid a separate assimp DLL. assimp ships
# its own zlib (contrib/) so no external zlib is required.
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)   # import-only
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # static assimp
add_subdirectory(dependencies/assimp)
target_link_libraries(DMGameEngine PRIVATE assimp)

# ── Compiler Definitions ─────────────────────────────────────────
# ── Vulkan backend (optional) ────────────────────────────────────
option(DMGE_VULKAN_BACKEND "Build the Vulkan renderer backend" OFF)
if(DMGE_VULKAN_BACKEND)
    find_package(Vulkan REQUIRED)
    find_path(VMA_INCLUDE_DIR NAMES vk_mem_alloc.h PATHS "$ENV{VULKAN_SDK}/Include" "$ENV{VULKAN_SDK}/Include/vulkan" "$ENV{VULKAN_SDK}/Include/vma" REQUIRED)
    find_path(SHADERC_INCLUDE_DIR NAMES shaderc/shaderc.hpp PATHS "$ENV{VULKAN_SDK}/Include" REQUIRED)
    find_library(SHADERC_LIBRARY_RELEASE NAMES shaderc_combined PATHS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/Lib/shaderc" REQUIRED)
    find_library(SHADERC_LIBRARY_DEBUG   NAMES shaderc_combinedd PATHS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/Lib/shaderc" REQUIRED)
    target_compile_definitions(DMGameEngine PRIVATE DMGE_VULKAN)
    target_include_directories(DMGameEngine PRIVATE "${VMA_INCLUDE_DIR}" "${SHADERC_INCLUDE_DIR}")
    target_link_libraries(DMGameEngine PRIVATE Vulkan::Vulkan
        "$<$<CONFIG:Debug>:${SHADERC_LIBRARY_DEBUG}>"
        "$<$<NOT:$<CONFIG:Debug>>:${SHADERC_LIBRARY_RELEASE}>")
    target_sources(DMGameEngine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_vulkan.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanDevice.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanSwapchain.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanGraphicsContext.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanRendererAPI.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanShader.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanVertexBuffer.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanIndexBuffer.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanFrameBuffer.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanVertexArray.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanTexture2D.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanTexture2DArray.cpp"
        "src/DMGameEngine/Platform/Vulkan/VulkanTextureCube.cpp")
endif()
target_compile_definitions(DMGameEngine
    PRIVATE
        DMGE_BUILD_DLL
        $<$<CONFIG:Debug>:DMGE_ENABLE_ASSERTS>
        $<$<BOOL:DMGE_PROFILE>:DMGE_PROFILE>
        $<$<BOOL:${DMGE_IMGUI_VIEWPORTS}>:DMGE_IMGUI_VIEWPORTS>
)

if(MSVC)
    target_compile_options(DMGameEngine PRIVATE /W4 /utf-8)
endif()

# -- Precompiled Header --------------------------------------------
# Caches the parse of stable, heavy, universally-included headers
# (std lib + glm + spdlog) so every C++ TU reuses one snapshot instead
# of re-parsing ~2 MB of templates per file. PRIVATE: build-time only,
# never exported to consumers. The CXX generator-expression guard keeps
# the C++ PCH off the C source (glad.c compiles as C).
# Scope: only std lib + 3rd-party libs here - NO engine headers, NO
# glad/GLFW/imgui/stb/Vulkan (platform/backend-specific). See
# PRECOMPILED_HEADER.md for the full rationale.
if(DMGE_USE_PCH)
    target_precompile_headers(DMGameEngine PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/DMGameEngine/dmge_pch.h>
    )
endif()

set_target_properties(DMGameEngine PROPERTIES
    LINKER_LANGUAGE CXX
    EXPORT_NAME DMGameEngine
)

# ── Install Rules (only when engine is the top-level project) ──────
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(GNUInstallDirs)

    install(TARGETS DMGameEngine
        EXPORT DMGameEngineTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY src/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DMGameEngine
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
    )

    install(EXPORT DMGameEngineTargets
        FILE DMGameEngineTargets.cmake
        NAMESPACE DMGameEngine::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DMGameEngine
    )
endif()

# ── Examples (optional) ──────────────────────────────────────────
if(DMGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(DMGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
