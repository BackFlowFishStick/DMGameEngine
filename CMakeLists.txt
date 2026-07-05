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
    "src/DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLShader.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTexture.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexArray.cpp"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.cpp"

    # Platform / Windows
    "src/DMGameEngine/Platform/Windows/GLFWInput.cpp"
    "src/DMGameEngine/Platform/Windows/WindowsWindow.cpp"

    # Renderer
    "src/DMGameEngine/Renderer/IndexBuffer.cpp"
    "src/DMGameEngine/Renderer/Renderer.cpp"
    "src/DMGameEngine/Renderer/RendererAPI.cpp"
    "src/DMGameEngine/Renderer/Shader.cpp"
    "src/DMGameEngine/Renderer/Texture.cpp"
    "src/DMGameEngine/Renderer/VertexArray.cpp"
    "src/DMGameEngine/Renderer/VertexBuffer.cpp"
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
    "src/DMGameEngine/Core/Window.h"
    "src/DMGameEngine/Core/Events/ApplicationEvent.h"
    "src/DMGameEngine/Core/Events/Event.h"
    "src/DMGameEngine/Core/Events/KeyEvent.h"
    "src/DMGameEngine/Core/Events/MouseEvent.h"

    # Platform / OpenGL
    "src/DMGameEngine/Platform/OpenGL/OpenGLGraphicsContext.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLIndexBuffer.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLRendererAPI.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLShader.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLTexture.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexArray.h"
    "src/DMGameEngine/Platform/OpenGL/OpenGLVertexBuffer.h"

    # Platform / Windows
    "src/DMGameEngine/Platform/Windows/GLFWInput.h"
    "src/DMGameEngine/Platform/Windows/WindowsWindow.h"

    # Renderer
    "src/DMGameEngine/Renderer/Camera.h"
    "src/DMGameEngine/Renderer/GraphicsContext.h"
    "src/DMGameEngine/Renderer/IndexBuffer.h"
    "src/DMGameEngine/Renderer/Renderer.h"
    "src/DMGameEngine/Renderer/RendererAPI.h"
    "src/DMGameEngine/Renderer/Shader.h"
    "src/DMGameEngine/Renderer/Texture.h"
    "src/DMGameEngine/Renderer/VertexArray.h"
    "src/DMGameEngine/Renderer/VertexBuffer.h"

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

# ── Compiler Definitions ─────────────────────────────────────────
target_compile_definitions(DMGameEngine
    PRIVATE
        DMGE_BUILD_DLL
        $<$<CONFIG:Debug>:DMGE_ENABLE_ASSERTS>
)

if(MSVC)
    target_compile_options(DMGameEngine PRIVATE /W4 /utf-8)
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
if(DMGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
