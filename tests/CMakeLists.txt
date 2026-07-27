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
)

target_link_libraries(dmge_tests PRIVATE
    DMGameEngine
    gtest
    gtest_main
)

# Tests are pure logic; never instantiate a window / graphics context.
target_compile_definitions(dmge_tests PRIVATE DMGE_TEST_HEADLESS)

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