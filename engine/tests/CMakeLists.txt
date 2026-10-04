# DMGameEngine - unit tests (GoogleTest)
#
# Pure-logic tests that do not touch the GPU (Timestep conversion, BufferLayout
# stride/offset math, ...). They link the engine target so DMGE_API-exported
# symbols are available, but they never create a window or graphics context, so
# they run headless on CI.
#
# Enable with: cmake -DDMGE_BUILD_TESTS=ON

include(FetchContent)
# CMake 3.24+ (CMP0135): NEW behavior uses extraction timestamps for URL
# downloads so a URL change triggers a rebuild (avoids the CMP0135 warning and
# stale downloaded content). Safe here - gtest is a leaf dependency we never
# patch locally.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(dmge_tests
    test_core.cpp
    test_ecs.cpp
    test_asset.cpp
    test_scene.cpp
    test_assimp_import.cpp
    test_deletion_queue.cpp
    test_deferred.cpp
)

# D3D11 backend headless smoke tests (device -> resources -> shader -> draw
# -> readback, no window needed; see documents/DIRECTX_BACKEND_DESIGN.md §6
# and kb/KB-07 K-014/K-021 for why this is possible for D3D11 specifically).
# Gated on the backend switch (DMGE_D3D11 gate in engine/CMakeLists.txt) -
# the tests construct backend classes that only exist when it is built.
if(DMGE_D3D11)
    add_executable(dmge_d3d11_tests
        test_d3d11_smoke.cpp
    )
    target_link_libraries(dmge_d3d11_tests PRIVATE
        DMGameEngine
        gtest
        gtest_main
    )
    # No PCH reuse (same dual-glm reasoning as dmge_tests, PRECOMPILED_HEADER.md).
    target_compile_definitions(dmge_d3d11_tests PRIVATE DMGE_TEST_HEADLESS)
    if(MSVC)
        target_compile_options(dmge_d3d11_tests PRIVATE /utf-8 /W4)
    endif()
    if(DMGE_BUILD_SHARED)
        add_custom_command(TARGET dmge_d3d11_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:DMGameEngine> $<TARGET_FILE_DIR:dmge_d3d11_tests>)
    endif()
    include(GoogleTest)
    gtest_discover_tests(dmge_d3d11_tests)
endif()

# Animation subsystem tests (pure logic + loaders + serialization round-trip).
# Only built when the subsystem itself is compiled (DMGE_ANIMATION gate in
# engine/CMakeLists.txt) - the tests exercise DLL-exported animation symbols.
if(DMGE_ANIMATION)
    add_executable(dmge_animation_tests
        test_animation_math.cpp
        test_animation_asset.cpp
        test_animation_serialize.cpp
        test_animation_render.cpp
    )
    target_link_libraries(dmge_animation_tests PRIVATE
        DMGameEngine
        gtest
        gtest_main
    )
    target_compile_definitions(dmge_animation_tests PRIVATE DMGE_TEST_HEADLESS
    DMGE_TEST_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    if(MSVC)
        target_compile_options(dmge_animation_tests PRIVATE /utf-8 /W4)
    endif()
    if(DMGE_BUILD_SHARED)
        add_custom_command(TARGET dmge_animation_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:DMGameEngine> $<TARGET_FILE_DIR:dmge_animation_tests>)
    endif()
    include(GoogleTest)
    gtest_discover_tests(dmge_animation_tests)
endif()

target_link_libraries(dmge_tests PRIVATE
    DMGameEngine
    gtest
    gtest_main
)

# NOTE: tests intentionally do NOT reuse the engine PCH (REUSE_FROM). When the
# Vulkan backend is enabled, the engine PCH is built against the Vulkan SDK's
# bundled glm (which shadows the vendored glm); the test target does not inherit
# the PRIVATE Vulkan include dirs, so it resolves <glm/glm.hpp> to the vendored
# copy and clashes with the PCH glm (C2011/C2995 redefinition). The 5 test TUs
# are small, so compiling them without a PCH sidesteps the dual-glm conflict.
# The engine PCH itself is unaffected. See PRECOMPILED_HEADER.md.

# Tests are pure logic; never instantiate a window / graphics context.
target_compile_definitions(dmge_tests PRIVATE DMGE_TEST_HEADLESS
    DMGE_TEST_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# spdlog (transitively pulled in via DMGameEngine/Core/Log.h -> Shader.h)
# bundles fmt, which static_asserts under MSVC unless /utf-8 is set. The
# engine target sets /utf-8 PRIVATE (it does not propagate to consumers), so
# mirror it here. Also fixes C4819 (GBK codepage vs UTF-8 source).
if(MSVC)
    target_compile_options(dmge_tests PRIVATE /utf-8)
endif()

# dmge_tests links DMGameEngine. When the engine is a shared library
# (DMGE_BUILD_SHARED=ON, the default), the DLL is output next to the engine
# target, not next to the test exe; copy it next to dmge_tests.exe so the
# loader finds it at runtime. gtest_discover_tests runs the exe at build time
# to enumerate cases, so without this discovery fails with 0xc0000135
# (STATUS_DLL_NOT_FOUND). No-op when the engine is built static.
if(DMGE_BUILD_SHARED)
    add_custom_command(TARGET dmge_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:DMGameEngine> $<TARGET_FILE_DIR:dmge_tests>)
endif()

include(GoogleTest)
gtest_discover_tests(dmge_tests)
