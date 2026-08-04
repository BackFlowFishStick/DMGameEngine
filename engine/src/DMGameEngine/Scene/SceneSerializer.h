/*
 * DMGameEngine - SceneSerializer (stage 1c)
 *
 * Saves/loads a Scene to a .scene JSON file. Component serialization is via an
 * internal type-name -> (serialize, deserialize) registry (hand-written per
 * Component, Hazel-style - see SCENE_DESIGN.md). Public API exposes only
 * Save/Load (path-based); json types stay internal so nlohmann/json remains
 * a PRIVATE dependency.
 *
 * Hierarchy is rebuilt across two passes on Load: first create all entities
 * (UUID -> Entity map), then re-link parents by their serialized UUID
 * (runtime Entity IDs change each load).
 */
#pragma once
#include "DMGameEngine/Core/Export.h"
#include <string>

namespace DMGameEngine {

class Scene;

class DMGE_API SceneSerializer
{
public:
    static void Save(const Scene& scene, const std::string& path);
    static bool Load(Scene& scene, const std::string& path);
};

} // namespace DMGameEngine