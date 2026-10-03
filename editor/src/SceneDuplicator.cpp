#include "SceneDuplicator.h"
#include <DMGameEngine/Scene/Components/Components.h>
#include <unordered_map>
#include <vector>

using namespace DMGameEngine;

namespace EditorSceneCopy {

namespace {

using EntityMap = std::unordered_map<Entity, Entity>;

// Collect 'root' and all descendants (guarded against malformed cycles).
void CollectSubtree(Scene& src, Entity root, std::vector<Entity>& out)
{
    std::vector<Entity> stack{ root };
    std::vector<bool> seen; // keyed by entity id; registry ids are dense enough for editor scenes
    auto markSeen = [&](Entity e) {
        if (e >= seen.size()) seen.resize(static_cast<size_t>(e) + 1, false);
        bool was = seen[e];
        seen[e] = true;
        return !was;
    };
    while (!stack.empty()) {
        Entity e = stack.back();
        stack.pop_back();
        if (!markSeen(e)) continue;
        out.push_back(e);
        if (src.HasComponent<TransformComponent>(e)) {
            Entity child = src.GetComponent<TransformComponent>(e).FirstChild;
            while (child != NullEntity) {
                stack.push_back(child);
                if (src.HasComponent<TransformComponent>(child))
                    child = src.GetComponent<TransformComponent>(child).NextSibling;
                else
                    break;
            }
        }
    }
}

Entity Remap(const EntityMap& map, Entity e)
{
    auto it = map.find(e);
    return it != map.end() ? it->second : NullEntity;
}

// Copy component payload for one mapped pair. Tag/Transform are attached by
// CreateEntity, so they are overwritten in place; the rest are added.
void CopyComponents(Scene& src, Entity from, Scene& dst, Entity to, const EntityMap& map)
{
    if (src.HasComponent<TagComponent>(from))
        dst.GetComponent<TagComponent>(to).Tag = src.GetComponent<TagComponent>(from).Tag;

    if (src.HasComponent<TransformComponent>(from)) {
        const auto& t = src.GetComponent<TransformComponent>(from);
        auto& tc = dst.GetComponent<TransformComponent>(to);
        tc.Translation   = t.Translation;
        tc.RotationEuler = t.RotationEuler;
        tc.Scale         = t.Scale;
        tc.Parent        = Remap(map, t.Parent);
        tc.FirstChild    = Remap(map, t.FirstChild);
        tc.NextSibling   = Remap(map, t.NextSibling);
        tc.Dirty         = true; // WorldMatrix recomputed by TransformSystem in dst
    }

    if (src.HasComponent<IDComponent>(from))
        dst.GetComponent<IDComponent>(to).UUID = src.GetComponent<IDComponent>(from).UUID;

    if (src.HasComponent<CameraComponent>(from))
        dst.AddComponent<CameraComponent>(to) = src.GetComponent<CameraComponent>(from);

    if (src.HasComponent<LightComponent>(from))
        dst.AddComponent<LightComponent>(to) = src.GetComponent<LightComponent>(from);

    // MeshComponent: share the Mesh / MaterialInstance resources (read-only
    // during play; AssetManager dedup means a re-load would return the same
    // Ref anyway). MeshAsset UUID is copied so prefab/save round-trips work.
    if (src.HasComponent<MeshComponent>(from)) {
        const auto& mc = src.GetComponent<MeshComponent>(from);
        auto& c = dst.AddComponent<MeshComponent>(to);
        c.MeshAsset         = mc.MeshAsset;
        c.Mesh              = mc.Mesh;
        c.MaterialOverrides = mc.MaterialOverrides;
    }
}

} // namespace

Entity CopyEntityTree(Scene& src, Entity root, Scene& dst)
{
    if (root == NullEntity || !src.GetRegistry().valid(static_cast<entt::entity>(root)))
        return NullEntity;

    std::vector<Entity> subtree;
    CollectSubtree(src, root, subtree);

    EntityMap map;
    for (Entity e : subtree) {
        std::string tag = src.HasComponent<TagComponent>(e)
                        ? src.GetComponent<TagComponent>(e).Tag
                        : std::string("Entity");
        map[e] = dst.CreateEntity(tag);
    }
    for (Entity e : subtree)
        CopyComponents(src, e, dst, map[e], map);

    return map[root];
}

void CopyAllEntities(Scene& src, Scene& dst)
{
    std::vector<Entity> all;
    src.GetRegistry().view<IDComponent>().each(
        [&](auto eh, IDComponent&) { all.push_back(static_cast<Entity>(eh)); });

    // Two passes so hierarchy links can be remapped regardless of iteration order.
    EntityMap map;
    for (Entity e : all) {
        std::string tag = src.HasComponent<TagComponent>(e)
                        ? src.GetComponent<TagComponent>(e).Tag
                        : std::string("Entity");
        map[e] = dst.CreateEntity(tag);
    }
    for (Entity e : all)
        CopyComponents(src, e, dst, map[e], map);
}

} // namespace EditorSceneCopy
