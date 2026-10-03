#pragma once
#include <string>

namespace DMGameEngine {
class Scene;
}

/*
 * ProjectExporter (stage 4) - generates a minimal runnable game project from
 * the active scene, standalone from the editor:
 *
 *   <out>/CMakeLists.txt          consumer CMake (add_subdirectory engine source)
 *   <out>/src/main.cpp            Application + scene runtime layer
 *   <out>/README.md               3-line build instructions
 *   <out>/assets/scene/main.scene SceneSerializer dump of the EDIT scene
 *   <out>/assets/registry.json    AssetManager UUID registry snapshot
 *   <out>/assets/models/...       model files referenced by the scene
 *   <out>/shaders/...             engine/shaders copy (relative shader paths)
 *
 * Why add_subdirectory instead of find_package(DMGameEngine CONFIG):
 * the engine's install rules (engine/CMakeLists.txt) only install an
 * EXPORT set (DMGameEngineTargets.cmake) and no DMGameEngineConfig.cmake,
 * and the export set itself cannot even be generated - configure of a
 * standalone engine build fails with
 *   install(EXPORT "DMGameEngineTargets" ...) includes target "DMGameEngine"
 *   which requires target "glm" that is not in any export set.
 * (PUBLIC link against the add_subdirectory 'glm' / 'EnTT::EnTT' targets is
 * not exportable as-is.) Consumers therefore build the engine from source.
 */
namespace ProjectExporter {

struct ExportResult {
    bool        Ok               = false;
    std::string OutputDir;       // normalized path on success
    std::string Error;           // reason on failure
    size_t      ModelsCopied     = 0;
    size_t      ProceduralMeshes = 0;   // in-scene procedural meshes (cannot survive serialization, K-012)
};

// Exports 'editScene' (the EDIT state - never the play copy) into 'outputDir'
// (created if missing). Never builds the generated project.
ExportResult Export(const std::string& outputDir, const DMGameEngine::Scene& editScene);

} // namespace ProjectExporter
