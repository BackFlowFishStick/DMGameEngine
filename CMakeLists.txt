cmake_minimum_required(VERSION 3.20)
project(DMGameEngine
    VERSION 0.1.0
    DESCRIPTION "DMGameEngine - A C++ game engine library"
    LANGUAGES CXX
)

# ── C++ Standard ─────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build Options ────────────────────────────────────────────────
option(DMGE_BUILD_SHARED "Build DMGameEngine as a shared library (DLL)" ON)
option(DMGE_BUILD_EXAMPLES "Build example projects" OFF)

# ── Source Files ─────────────────────────────────────────────────
file(GLOB_RECURSE DMGE_SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE DMGE_HEADERS
    "src/*.h"
    "src/*.hpp"
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

# ── Dependencies (external static libraries) ─────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
    file(GLOB DMGE_DEP_LIBS "dependencies/lib/*.lib" "dependencies/lib/*.a")
    if(DMGE_DEP_LIBS)
        target_link_libraries(DMGameEngine PRIVATE ${DMGE_DEP_LIBS})
    endif()

    file(GLOB DMGE_DEP_INCLUDE_DIRS LIST_DIRECTORIES true "dependencies/include/*")
    foreach(DEP_DIR ${DMGE_DEP_INCLUDE_DIRS})
        if(IS_DIRECTORY "${DEP_DIR}")
            target_include_directories(DMGameEngine PRIVATE "${DEP_DIR}")
        endif()
    endforeach()
endif()

# ── Compiler Definitions ─────────────────────────────────────────
target_compile_definitions(DMGameEngine
    PRIVATE
        DMGE_BUILD_DLL
)

if(MSVC)
    target_compile_options(DMGameEngine PRIVATE /W4)
endif()

set_target_properties(DMGameEngine PROPERTIES
    LINKER_LANGUAGE CXX
    EXPORT_NAME DMGameEngine
)

# ── Install Rules ────────────────────────────────────────────────
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

# ── Examples (optional) ──────────────────────────────────────────
if(DMGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
