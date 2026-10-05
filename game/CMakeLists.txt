cmake_minimum_required(VERSION 3.20)
project(DMGameDemo
    VERSION 0.1.0
    DESCRIPTION "Demo game using DMGameEngine"
    LANGUAGES CXX
)

# ── C++ Standard ─────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Game Executable ──────────────────────────────────────────────
set(GAME_SOURCES
    "src/main.cpp"
)
add_executable(DMGameDemo ${GAME_SOURCES})

# DMGameEngine target is provided by top-level CMakeLists.txt
target_link_libraries(DMGameDemo PRIVATE DMGameEngine)

# Shader asset directories, injected at configure time so the demo works from
# any checkout/worktree location (main.cpp previously hardcoded an absolute
# main-checkout path). main.cpp provides fallback defaults.
target_compile_definitions(DMGameDemo PRIVATE
    DMGE_GAME_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../engine/shaders"
    DMGE_GAME_HLSL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/DMGameEngine/Platform/DirectX/Shaders"
)

# ── Copy DMGameEngine DLL to runtime output directory ────────────
add_custom_command(TARGET DMGameDemo POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:DMGameEngine>"
        "$<TARGET_FILE_DIR:DMGameDemo>"
    COMMENT "Copying DMGameEngine.dll to runtime directory"
)

# ── MSVC Options ─────────────────────────────────────────────────
if(MSVC)
    target_compile_options(DMGameDemo PRIVATE /W4 /utf-8)
endif()
