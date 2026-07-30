/*
 * DMGameEngine - SceneSerializer implementation (stage 1c)
 */
#include "DMGameEngine/Scene/SceneSerializer.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Components/IDComponent.h"
#include "DMGameEngine/Scene/Components/TagComponent.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"
#include "DMGameEngine/Scene/Components/CameraComponent.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Asset/AssetManager.h"  // Load<Material> in DeserializeMesh

#include <nlohmann/json.hpp>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace DMGameEngine {

namespace {

using json = nlohmann::json;
using SerializeFn   = std::function<void(const Scene&, Entity, json&)>;
using DeserializeFn = std::function<void(Scene&, Entity, const json&)>;

std::unordered_map<std::string, std::pair<SerializeFn, DeserializeFn>>& Registry()
{
    static std::unordered_map<std::string, std::pair<SerializeFn, DeserializeFn>> r;
    return r;
}

// ── TransformComponent ─────────────────────────────────────────
void SerializeTransform(const Scene& scene, Entity e, json& j)
{
    auto& s = const_cast<Scene&>(scene);
    if (!s.HasComponent<TransformComponent>(e)) return;
    auto& tc = s.GetComponent<TransformComponent>(e);

    json tj;
    tj["translation"] = { tc.Translation.x, tc.Translation.y, tc.Translation.z };
    tj["rotation"]    = { tc.RotationEuler.x, tc.RotationEuler.y, tc.RotationEuler.z };
    tj["scale"]       = { tc.Scale.x, tc.Scale.y, tc.Scale.z };

    // parent stored as the parent entity's IDComponent UUID (0 = root), not the
    // runtime Entity ID (which changes each load).
    uint64_t parentUUID = 0;
    if (tc.Parent != NullEntity && s.HasComponent<IDComponent>(tc.Parent))
        parentUUID = s.GetComponent<IDComponent>(tc.Parent).UUID;
    tj["parent"] = parentUUID;

    j["Transform"] = tj;
}

void DeserializeTransform(Scene& scene, Entity e, const json& ej)
{
    if (!ej.contains("Transform")) return;
    const auto& tj = ej["Transform"];

    auto& tc = scene.GetComponent<TransformComponent>(e);  // CreateEntity already attached
    tc.Translation   = { tj["translation"][0], tj["translation"][1], tj["translation"][2] };
    tc.RotationEuler  = { tj["rotation"][0],    tj["rotation"][1],    tj["rotation"][2] };
    tc.Scale          = { tj["scale"][0],        tj["scale"][1],        tj["scale"][2] };
    tc.Dirty = true;
    // parent re-linking happens in Load's second pass (UUID -> Entity map).
}

// ── MeshComponent ───────────────────────────────────────────────
void SerializeMesh(const Scene& scene, Entity e, json& j)
{
    auto& s = const_cast<Scene&>(scene);
    if (!s.HasComponent<MeshComponent>(e)) return;
    auto& mc = s.GetComponent<MeshComponent>(e);

    json mj;
    mj["meshAsset"] = mc.MeshAsset.GetUUID();
    mj["material"]  = mc.MaterialAsset.GetUUID();
    j["Mesh"] = mj;
}

void DeserializeMesh(Scene& scene, Entity e, const json& ej)
{
    if (!ej.contains("Mesh")) return;
    const auto& mj = ej["Mesh"];
    auto& mc = scene.AddComponent<MeshComponent>(e);
    mc.MeshAsset     = AssetHandle(mj.value("meshAsset",  uint64_t(0)));
    mc.MaterialAsset = AssetHandle(mj.value("material",   uint64_t(0)));
    // Load Material via AssetManager (AssetLoader<Material> reads .mat JSON).
    // Requires the Material UUID to be in the AssetManager registry (LoadRegistry)
    // mapping to a .mat file path; returns null if not registered.
    if (mc.MaterialAsset.IsValid())
        mc.Material = AssetManager::Get().Load<Material>(mc.MaterialAsset);
    // Load mesh (VertexArray) via AssetManager (AssetLoader<VertexArray> reads .mesh JSON).
    if (mc.MeshAsset.IsValid())
        mc.VAO = AssetManager::Get().Load<VertexArray>(mc.MeshAsset);
}

// ── CameraComponent ─────────────────────────────────────────────
void SerializeCamera(const Scene& scene, Entity e, json& j)
{
    auto& s = const_cast<Scene&>(scene);
    if (!s.HasComponent<CameraComponent>(e)) return;
    auto& cc = s.GetComponent<CameraComponent>(e);

    json cj;
    cj["primary"]          = cc.Primary;
    cj["fixedAspectRatio"] = cc.FixedAspectRatio;
    cj["projectionType"]   = static_cast<int>(cc.Camera.GetProjectionType());
    cj["perspective"]      = { cc.Camera.GetPerspectiveVerticalFOV(),
                               cc.Camera.GetPerspectiveNearClip(),
                               cc.Camera.GetPerspectiveFarClip() };
    cj["orthographic"]     = { cc.Camera.GetOrthographicSize(),
                               cc.Camera.GetOrthographicNearClip(),
                               cc.Camera.GetOrthographicFarClip() };
    j["Camera"] = cj;
}

void DeserializeCamera(Scene& scene, Entity e, const json& ej)
{
    if (!ej.contains("Camera")) return;
    const auto& cj = ej["Camera"];
    auto& cc = scene.AddComponent<CameraComponent>(e);
    cc.Primary          = cj.value("primary", false);
    cc.FixedAspectRatio = cj.value("fixedAspectRatio", false);
    cc.Camera.SetProjectionType(
        static_cast<SceneCamera::ProjectionType>(cj.value("projectionType", 0)));
    cc.Camera.SetPerspectiveVerticalFOV(cj["perspective"][0]);
    cc.Camera.SetPerspectiveNearClip(cj["perspective"][1]);
    cc.Camera.SetPerspectiveFarClip(cj["perspective"][2]);
    cc.Camera.SetOrthographicSize(cj["orthographic"][0]);
    cc.Camera.SetOrthographicNearClip(cj["orthographic"][1]);
    cc.Camera.SetOrthographicFarClip(cj["orthographic"][2]);
}

// Static registration of built-in components (runs at program start).
bool s_Registered = []() {
    Registry()["Transform"] = { SerializeTransform, DeserializeTransform };
    Registry()["Mesh"]      = { SerializeMesh,      DeserializeMesh };
    Registry()["Camera"]    = { SerializeCamera,    DeserializeCamera };
    return true;
}();

} // namespace

// ── Save ───────────────────────────────────────────────────────
void SceneSerializer::Save(const Scene& scene, const std::string& path)
{
    json j;
    j["version"] = 1;
    json entities = json::array();

    auto& reg = const_cast<Scene&>(scene).GetRegistry();
    reg.view<IDComponent>().each([&](auto ent, auto& idc) {
        Entity e = static_cast<Entity>(ent);
        json ej;
        ej["uuid"] = idc.UUID;
        auto& s = const_cast<Scene&>(scene);
        ej["tag"] = s.HasComponent<TagComponent>(e)
                  ? s.GetComponent<TagComponent>(e).Tag
                  : std::string("Entity");
        for (const auto& [name, fns] : Registry())
            fns.first(scene, e, ej);  // SerializeFn checks HasComponent internally
        entities.push_back(ej);
    });
    j["entities"] = entities;

    std::ofstream fout(path);
    fout << j.dump(2);
}

// ── Load ───────────────────────────────────────────────────────
bool SceneSerializer::Load(Scene& scene, const std::string& path)
{
    std::ifstream fin(path);
    if (!fin.is_open()) return false;
    json j;
    fin >> j;

    if (!j.contains("version") || !j.contains("entities")) return false;
    (void)j["version"].get<int>();  // reserved for future version checks

    // Pass 1: create entities + UUID -> Entity map + deserialize component locals.
    std::unordered_map<uint64_t, Entity> uuidToEntity;
    std::unordered_map<Entity, uint64_t> entityToParentUUID;

    for (const auto& ej : j["entities"]) {
        uint64_t uuid = ej["uuid"].get<uint64_t>();
        std::string tag = ej.value("tag", "Entity");
        Entity e = scene.CreateEntity(tag);  // attaches ID + Tag + Transform
        scene.GetComponent<IDComponent>(e).UUID = uuid;  // overwrite random UUID
        uuidToEntity[uuid] = e;

        for (const auto& [name, fns] : Registry())
            fns.second(scene, e, ej);  // DeserializeFn (AddComponent + fill)

        // stash parent UUID for pass 2 (Transform's parent).
        if (ej.contains("Transform"))
            entityToParentUUID[e] = ej["Transform"].value("parent", uint64_t(0));
    }

    // Pass 2: re-link hierarchy by parent UUID.
    for (const auto& [e, parentUUID] : entityToParentUUID) {
        if (parentUUID != 0) {
            auto it = uuidToEntity.find(parentUUID);
            if (it != uuidToEntity.end())
                scene.SetParent(e, it->second);
        }
    }

    return true;
}

} // namespace DMGameEngine