/*
 * DMGameEngine - Skeleton / AnimationClip loaders (animation stage 1)
 *
 * AssetLoader<Skeleton> / AssetLoader<AnimationClip> implementations.
 * Dispatch by path shape:
 *   "<model>.fbx|#skeleton"            -> assimp import (MeshImporterAssimp.cpp),
 *                                         the derived path the importer registers
 *   "<model>.fbx|#anim/<index>"        -> assimp clip import, same scheme
 *   *.skel.json / *.anim.json          -> minimal inline JSON format (test /
 *                                         hand-authored assets; KB-07 K-012 rule:
 *                                         programmatic assets must go through a
 *                                         loader path to be serializable)
 *
 * The specializations are exported (DMGE_API on the struct) per KB-05/K-002 so
 * AnimationSystem (header-only) can call AssetManager::Load<Skeleton> from any
 * TU. nlohmann/json stays PRIVATE like AssetLoader.cpp.
 */
#include "DMGameEngine/Asset/AssetLoader.h"
#include "DMGameEngine/Core/Log.h"

#include <nlohmann/json.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <string>
#include <vector>

namespace DMGameEngine {

// assimp entry points (defined in MeshImporterAssimp.cpp; keeps assimp out of
// this translation unit). Non-exported: used only inside the DLL.
DM::Ref<Skeleton> LoadSkeletonViaAssimp(const std::string& modelPath);
DM::Ref<AnimationClip> LoadAnimationClipViaAssimp(const std::string& modelPath,
                                                  uint32_t clipIndex);

namespace {

bool EndsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ── .skel.json ──────────────────────────────────────────────────────
// { "joints": [ { "name": "...", "parent": <int>, "inverseBind": [16 floats] } ] }
DM::Ref<Skeleton> LoadSkeletonFromJSON(const std::string& path)
{
    std::ifstream fin(path);
    if (!fin.is_open()) return nullptr;
    nlohmann::json j;
    try { fin >> j; } catch (...) { return nullptr; }

    if (!j.contains("joints")) return nullptr;

    auto skeleton = DM::CreateRef<Skeleton>();
    for (const auto& jj : j["joints"])
    {
        Joint joint;
        joint.Name = jj.value("name", "");
        joint.ParentIndex = jj.value("parent", -1);
        if (jj.contains("inverseBind") && jj["inverseBind"].size() == 16)
        {
            const auto m = jj["inverseBind"].get<std::vector<float>>();
            joint.InverseBindMatrix = glm::make_mat4(m.data());
        }
        skeleton->Joints.push_back(joint);
    }
    return skeleton;
}

// ── .anim.json ──────────────────────────────────────────────────────
// { "name": "...", "duration": <ticks>, "ticksPerSecond": <float>,
//   "channels": [ { "joint": <int>,
//       "translations": [ { "time": f, "value": [x,y,z] } ],
//       "rotations":    [ { "time": f, "value": [x,y,z,w] } ],
//       "scales":       [ { "time": f, "value": [x,y,z] } ] } ] }
DM::Ref<AnimationClip> LoadClipFromJSON(const std::string& path)
{
    std::ifstream fin(path);
    if (!fin.is_open()) return nullptr;
    nlohmann::json j;
    try { fin >> j; } catch (...) { return nullptr; }

    auto clip = DM::CreateRef<AnimationClip>();
    clip->Name           = j.value("name", "");
    clip->Duration       = j.value("duration", 0.0f);
    clip->TicksPerSecond = j.value("ticksPerSecond", 25.0f);

    if (!j.contains("channels")) return clip;
    for (const auto& cj : j["channels"])
    {
        AnimationChannel channel;
        channel.JointIndex = cj.value("joint", -1);

        const auto readVec3Keys = [](const nlohmann::json& arr) {
            std::vector<VectorKey> keys;
            for (const auto& k : arr)
                keys.push_back({ k.value("time", 0.0f),
                                 glm::vec3(k["value"][0], k["value"][1], k["value"][2]) });
            return keys;
        };
        const auto readQuatKeys = [](const nlohmann::json& arr) {
            std::vector<QuatKey> keys;
            for (const auto& k : arr)
                keys.push_back({ k.value("time", 0.0f),
                                 glm::quat(k["value"][3], k["value"][0],
                                           k["value"][1], k["value"][2]) });
            return keys;
        };

        if (cj.contains("translations")) channel.Translations = readVec3Keys(cj["translations"]);
        if (cj.contains("rotations"))    channel.Rotations    = readQuatKeys(cj["rotations"]);
        if (cj.contains("scales"))       channel.Scales       = readVec3Keys(cj["scales"]);
        clip->Channels.push_back(channel);
    }
    return clip;
}

} // namespace

DM::Ref<Skeleton> AssetLoader<Skeleton>::Load(const std::string& path)
{
    // Derived assimp path: "<model>#skeleton"
    const auto hash = path.rfind("#skeleton");
    if (hash != std::string::npos && hash + 9 == path.size())
        return LoadSkeletonViaAssimp(path.substr(0, hash));

    if (EndsWith(path, ".skel.json"))
        return LoadSkeletonFromJSON(path);

    DMGE_LOG_WARN("AssetLoader<Skeleton>: unsupported path '{}'", path);
    return nullptr;
}

DM::Ref<AnimationClip> AssetLoader<AnimationClip>::Load(const std::string& path)
{
    // Derived assimp path: "<model>#anim/<index>"
    const auto marker = path.find("#anim/");
    if (marker != std::string::npos)
    {
        const std::string modelPath = path.substr(0, marker);
        const std::string indexStr  = path.substr(marker + 6);
        try { return LoadAnimationClipViaAssimp(modelPath,
                    static_cast<uint32_t>(std::stoul(indexStr))); }
        catch (...) { return nullptr; }
    }

    if (EndsWith(path, ".anim.json"))
        return LoadClipFromJSON(path);

    DMGE_LOG_WARN("AssetLoader<AnimationClip>: unsupported path '{}'", path);
    return nullptr;
}

} // namespace DMGameEngine
