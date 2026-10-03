cmake_minimum_required(VERSION 3.20)
project(DMGameEditor VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EDITOR_SOURCES
    src/main.cpp
    src/EditorApplication.cpp
    src/EditorLayer.cpp
    src/EditorScene.cpp
    src/SceneDuplicator.cpp
    src/ProjectExporter.cpp
    src/LogPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/ImGuizmo/src/ImGuizmo.cpp
)
add_executable(DMGameEditor ${EDITOR_SOURCES})
target_link_libraries(DMGameEditor PRIVATE DMGameEngine)
# ImGui: use symbols exported by the engine DLL (IMGUI_API=dllexport there).
target_compile_definitions(DMGameEditor PRIVATE "IMGUI_API=__declspec(dllimport)")
target_include_directories(DMGameEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/engine/dependencies/imgui
    ${CMAKE_SOURCE_DIR}/engine/dependencies/imgui/backends
    ${CMAKE_SOURCE_DIR}/engine/dependencies/glm-master
    ${CMAKE_SOURCE_DIR}/engine/dependencies/spdlog-1.x/include
    ${CMAKE_SOURCE_DIR}/editor/dependencies/ImGuizmo/src)
if(MSVC)
    target_compile_options(DMGameEditor PRIVATE /W4 /utf-8)
endif()
add_custom_command(TARGET DMGameEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DMGameEngine> $<TARGET_FILE_DIR:DMGameEditor>
    COMMENT "Copying DMGameEngine.dll to editor runtime")