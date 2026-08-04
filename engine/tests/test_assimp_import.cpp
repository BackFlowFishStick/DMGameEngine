/*
 * DMGameEngine - assimp mesh import headless tests (方式 A + B)
 *
 * Loads models through the Scene serialize/deserialize round-trip, which
 * triggers AssetManager::Load<Mesh> -> AssetLoader<Mesh>::Load (extension
 * dispatch) -> LoadMeshViaAssimp, all inside the engine DLL (the AssetLoader
 * specializations are not exported, so import is driven via the exported
 * SceneSerializer path). No GPU/window: assimp import is CPU-only and the
 * Mesh CPU data is inspectable without a graphics context.
 *
 * 方式 A: vendored assimp OBJ -> Mesh structure + negative material mapping.
 * 方式 B: self-authored .obj+.mtl (known material name) + sibling .mat ->
 *        positive material mapping (SubMesh.MaterialAsset resolves to the .mat).
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Asset/AssetTypes.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Asset/Mesh.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/SceneSerializer.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Components/IDComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"

using namespace DMGameEngine;

namespace {

// assimp test model already vendored in the dependency tree (read-only).
constexpr const char* kObjModelPath =
    "D:/CPPPractices/DMGameEngine/engine/dependencies/assimp/test/models/OBJ/box.obj";

Entity FindByUUID(Scene& scene, uint64_t uuid)
{
    auto view = scene.GetRegistry().view<IDComponent>();
    for (auto e : view)
        if (view.get<IDComponent>(e).UUID == uuid)
            return static_cast<Entity>(e);
    return NullEntity;
}

// Drives a model path through the Scene round-trip so AssetLoader<Mesh>::Load
// (assimp) runs inside the DLL. Returns the loaded Mesh (kept alive via the
// returned shared_ptr) or nullptr.
DM::Ref<Mesh> LoadMeshViaScene(const std::string& modelPath, const std::string& sceneFile)
{
    auto& am = AssetManager::Get();
    am.Clear();
    const AssetUUID uuid = am.Register(modelPath, AssetType::Mesh);

    Scene scene;
    Entity e = scene.CreateEntity("Mesh");
    auto& mc = scene.AddComponent<MeshComponent>(e);
    mc.MeshAsset = AssetHandle(uuid);
    const uint64_t idUUID = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, sceneFile);   // void
    Scene loaded;
    if (!SceneSerializer::Load(loaded, sceneFile)) return nullptr;

    const Entity le = FindByUUID(loaded, idUUID);
    if (le == NullEntity || !loaded.HasComponent<MeshComponent>(le)) return nullptr;
    return loaded.GetComponent<MeshComponent>(le).Mesh;
}

void WriteFile(const std::filesystem::path& p, const std::string& content)
{
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
}

} // namespace

class AssimpMeshImportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Import emits DMGE_LOG_WARN per material when no sibling .mat exists;
        // the logger must be initialized or the macro dereferences a null sink.
        static bool logReady = false;
        if (!logReady) { Log::Init(); logReady = true; }
        AssetManager::Get().Clear();
    }
};

// 方式 A: .obj -> assimp -> Mesh fill + index integrity + negative material.
TEST_F(AssimpMeshImportTest, ObjLoadsIntoMesh)
{
    const auto sceneFile = std::filesystem::temp_directory_path() / "dmge_assimp_a.scene";
    auto mesh = LoadMeshViaScene(kObjModelPath, sceneFile.string());
    std::filesystem::remove(sceneFile);

    ASSERT_NE(mesh, nullptr);
    EXPECT_FALSE(mesh->Vertices.empty());
    EXPECT_FALSE(mesh->Indices.empty());
    ASSERT_FALSE(mesh->SubMeshes.empty());

    uint32_t sum = 0;
    for (const auto& s : mesh->SubMeshes)
    {
        EXPECT_LE(s.IndexOffset + s.IndexCount, mesh->Indices.size());
        sum += s.IndexCount;
    }
    EXPECT_EQ(sum, mesh->Indices.size());

    EXPECT_EQ(mesh->Layout.GetElements().front().Name, "a_Position");
    // negative material mapping: the read-only assimp dir has no sibling .mat
    EXPECT_FALSE(mesh->SubMeshes.front().MaterialAsset.IsValid());
}

// 方式 B: positive material mapping - .obj+.mtl (material "TriMat") + TriMat.mat
TEST_F(AssimpMeshImportTest, MaterialMatLookupResolvesHandle)
{
    const auto dir = std::filesystem::temp_directory_path() / "dmge_assimp_b";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    WriteFile(dir / "tri.obj", R"(# tri.obj
mtllib tri.mtl
v -1 -1 0
v 1 -1 0
v 0 1 0
vn 0 0 1
vt 0 0
vt 1 0
vt 0 1
usemtl TriMat
f 1/1/1 2/2/1 3/3/1
)");
    WriteFile(dir / "tri.mtl", R"(newmtl TriMat
Kd 1.0 0.0 0.0
)");
    // The shader path is irrelevant here: the importer only Register()s the
    // .mat (UUID only); the Material itself is not loaded during import.
    WriteFile(dir / "TriMat.mat", R"({ "shader": "assets/shaders/mesh.glsl", "uniforms": {} }
)");

    const auto sceneFile = std::filesystem::temp_directory_path() / "dmge_assimp_b.scene";
    auto mesh = LoadMeshViaScene((dir / "tri.obj").string(), sceneFile.string());

    ASSERT_NE(mesh, nullptr);
    ASSERT_FALSE(mesh->SubMeshes.empty());

    const AssetHandle handle = mesh->SubMeshes.front().MaterialAsset;
    EXPECT_TRUE(handle.IsValid());   // .mat found -> registered as a Material asset

    const auto* meta = AssetManager::Get().GetMetadata(handle.GetUUID());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->Type, AssetType::Material);
    EXPECT_EQ(std::filesystem::path(meta->Path).filename().string(), "TriMat.mat");

    std::filesystem::remove(sceneFile);
    std::filesystem::remove_all(dir);
}
