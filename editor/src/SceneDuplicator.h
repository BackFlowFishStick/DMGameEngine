#pragma once
/*
 * SceneDuplicator - editor-side deep copy of entity subtrees between Scenes.
 *
 * Two consumers:
 *  - Play-mode isolation (stage 4): EnterPlayMode copies the whole edit scene
 *    into a runtime copy; Stop discards the copy, so the edit scene object is
 *    never mutated during play (mainstream-engine semantics: changes made in
 *    play mode are not kept).
 *  - Prefab save / instantiate (stage 3): a temp Scene carries a single entity
 *    tree through SceneSerializer (Save As Prefab / Instantiate).
 *
 * Why a ref-sharing deep copy instead of a SceneSerializer JSON round-trip:
 * the serializer persists MeshComponent.Mesh only as an AssetUUID (see
 * SceneSerializer::SerializeMesh). Procedural meshes that were never
 * registered with the AssetManager (e.g. the default cube built in
 * EditorScene::SetupDefaultScene) have an invalid UUID and would be silently
 * dropped by a JSON snapshot. Copying components by value and sharing the
 * DM::Ref<Mesh> / DM::Ref<MaterialInstance> resources preserves everything;
 * the shared resources are treated as read-only during play.
 *
 * Lives entirely in the editor exe: no engine changes required (all touched
 * types have public copyable state; hierarchy handles are plain Entity ids).
 */
#include <DMGameEngine/DMGameEngine.h>

namespace EditorSceneCopy {

// Copy 'root' and its entire descendant subtree (FirstChild/NextSibling chain)
// from 'src' into 'dst'. New entities in 'dst' keep UUID/tag/components;
// hierarchy links inside the subtree are remapped, links to entities outside
// the subtree become roots (Parent = NullEntity). Returns the new root entity
// in 'dst'.
DMGameEngine::Entity CopyEntityTree(DMGameEngine::Scene& src,
                                    DMGameEngine::Entity root,
                                    DMGameEngine::Scene& dst);

// Copy every entity of 'src' (with full hierarchy) into 'dst'.
// 'dst' must not already contain entities (UUID collisions are not merged).
void CopyAllEntities(DMGameEngine::Scene& src, DMGameEngine::Scene& dst);

} // namespace EditorSceneCopy
