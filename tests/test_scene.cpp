/*
 * DMGameEngine - SceneSerializer unit tests (stage 1c)
 *
 * Round-trip: Save -> Load -> field equality. Covers identity (UUID/Tag),
 * transform locals, hierarchy rebuild (parent UUID -> triple chain),
 * MeshComponent asset UUIDs, CameraComponent params, and version field.
 *
 * nlohmann/json is PRIVATE, so this test does not include <nlohmann/json.hpp>;
 * the version-field check reads the raw file text instead.
 */
#include <gtest/gtest.h>

#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/SceneSerializer.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Components/IDComponent.h"
#include "DMGameEngine/Scene/Components/TagComponent.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/MeshComponent.h"
#include "DMGameEngine/Scene/Components/CameraComponent.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Renderer/Shader.h"     // Shader base (NullShader test double)
#include "DMGameEngine/Renderer/Material.h"   // Material / MaterialInstance

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace DMGameEngine;

namespace {

// NullShader: no-op Shader test double so Material / MaterialInstance can be
// constructed headlessly (no OpenGL context). The serialization path only
// stores uniform values in a map and never calls the Shader setters, so the
// no-op implementations are sufficient. This is the "mock base" strategy noted
// in MATERIAL_OVERRIDE_SERIALIZATION.md section 6 - a real base Material
// (needs a live Shader + .mat asset) cannot be built under DMGE_TEST_HEADLESS.
class NullShader : public Shader
{
public:
    void Bind() const override {}
    void Unbind() const override {}
    const std::string& GetName() const override
    {
        static const std::string kName{"null"};
        return kName;
    }
    void SetInt(std::string_view, int) override {}
    void SetIntArray(std::string_view, const int*, uint32_t) override {}
    void SetFloat(std::string_view, float) override {}
    void SetFloat2(std::string_view, const glm::vec2&) override {}
    void SetFloat3(std::string_view, const glm::vec3&) override {}
    void SetFloat4(std::string_view, const glm::vec4&) override {}
    void SetMat4(std::string_view, const glm::mat4&) override {}
};

Entity FindByUUID(Scene& scene, uint64_t uuid)
{
    auto view = scene.GetRegistry().view<IDComponent>();
    for (auto e : view)
        if (view.get<IDComponent>(e).UUID == uuid)
            return static_cast<Entity>(e);
    return NullEntity;
}

const std::string kScenePath = "test_scene_serializer.json";
} // namespace

class SceneSerializerTest : public ::testing::Test
{
protected:
    void TearDown() override { std::filesystem::remove(kScenePath); }
};

TEST_F(SceneSerializerTest, RoundTripPreservesIdentity)
{
    Scene scene;
    Entity e = scene.CreateEntity("Player");
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    EXPECT_EQ(loaded.GetComponent<TagComponent>(le).Tag, std::string("Player"));
}

TEST_F(SceneSerializerTest, RoundTripPreservesTransform)
{
    Scene scene;
    Entity e = scene.CreateEntity();
    auto& tc = scene.GetComponent<TransformComponent>(e);
    tc.Translation   = { 1.5f, 2.5f, 3.5f };
    tc.RotationEuler  = { 0.1f, 0.2f, 0.3f };
    tc.Scale          = { 2.0f, 3.0f, 4.0f };
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    auto& ltc = loaded.GetComponent<TransformComponent>(le);
    EXPECT_FLOAT_EQ(ltc.Translation.x, 1.5f);
    EXPECT_FLOAT_EQ(ltc.Translation.y, 2.5f);
    EXPECT_FLOAT_EQ(ltc.Translation.z, 3.5f);
    EXPECT_FLOAT_EQ(ltc.RotationEuler.x, 0.1f);
    EXPECT_FLOAT_EQ(ltc.Scale.x, 2.0f);
    EXPECT_FLOAT_EQ(ltc.Scale.z, 4.0f);
}

TEST_F(SceneSerializerTest, RoundTripRebuildsHierarchy)
{
    Scene scene;
    Entity parent = scene.CreateEntity("Parent");
    Entity child  = scene.CreateEntity("Child");
    scene.SetParent(child, parent);
    uint64_t parentUUID = scene.GetComponent<IDComponent>(parent).UUID;
    uint64_t childUUID  = scene.GetComponent<IDComponent>(child).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity lp = FindByUUID(loaded, parentUUID);
    Entity lc = FindByUUID(loaded, childUUID);
    ASSERT_NE(lp, NullEntity);
    ASSERT_NE(lc, NullEntity);
    // parent rebuilt by UUID: child.Parent == parent
    EXPECT_EQ(loaded.GetComponent<TransformComponent>(lc).Parent, lp);
}

TEST_F(SceneSerializerTest, RoundTripPreservesMeshAssetUUID)
{
    Scene scene;
    Entity e = scene.CreateEntity();
    auto& mc = scene.AddComponent<MeshComponent>(e);
    mc.MeshAsset = AssetHandle(static_cast<uint64_t>(12345));
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    ASSERT_TRUE(loaded.HasComponent<MeshComponent>(le));
    auto& lmc = loaded.GetComponent<MeshComponent>(le);
    EXPECT_EQ(lmc.MeshAsset.GetUUID(), 12345u);
}

TEST_F(SceneSerializerTest, RoundTripPreservesCamera)
{
    Scene scene;
    Entity e = scene.CreateEntity();
    auto& cc = scene.AddComponent<CameraComponent>(e);
    cc.Primary = true;
    cc.Camera.SetPerspectiveVerticalFOV(60.0f);
    cc.Camera.SetProjectionType(SceneCamera::ProjectionType::Orthographic);
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    ASSERT_TRUE(loaded.HasComponent<CameraComponent>(le));
    auto& lcc = loaded.GetComponent<CameraComponent>(le);
    EXPECT_TRUE(lcc.Primary);
    EXPECT_FLOAT_EQ(lcc.Camera.GetPerspectiveVerticalFOV(), 60.0f);
    EXPECT_EQ(lcc.Camera.GetProjectionType(), SceneCamera::ProjectionType::Orthographic);
}

TEST_F(SceneSerializerTest, VersionFieldPresent)
{
    Scene scene;
    scene.CreateEntity();
    SceneSerializer::Save(scene, kScenePath);

    // Read raw file text; nlohmann/json is PRIVATE so we grep instead of parsing.
    std::ifstream fin(kScenePath);
    ASSERT_TRUE(fin.is_open());
    std::string content((std::istreambuf_iterator<char>(fin)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("\"version\""), std::string::npos);
    EXPECT_NE(content.find("\"entities\""), std::string::npos);
}

// ── Material overrides serialization (MATERIAL_OVERRIDE_SERIALIZATION.md) ──
//
// A MaterialInstance needs a live base Material (Shader + .mat asset) which
// cannot be built headlessly. The NullShader double lets us construct one and
// exercise the SAVE path (SerializeMesh -> UniformValueToJson). The LOAD path
// rebuilds the base from a Mesh asset via AssetManager, which is unavailable
// headless, so it is only verified to skip gracefully (see test below).

TEST_F(SceneSerializerTest, SaveWritesMaterialOverrides)
{
    auto shader = DM::CreateRef<NullShader>();
    auto base   = DM::CreateRef<Material>(shader);
    auto mi     = DM::CreateRef<MaterialInstance>(base);
    // Cover every UniformValue alternative so UniformValueToJson is exercised
    // for the whole closed variant, not just one branch.
    mi->SetFloat4 ("u_Color",   glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    mi->SetFloat3 ("u_Tint",    glm::vec3(0.5f, 0.5f, 0.5f));
    mi->SetFloat2 ("u_Offset",  glm::vec2(1.0f, 2.0f));
    mi->SetFloat  ("u_Tiling",  2.0f);
    mi->SetInt    ("u_Count",   3);
    mi->SetMat4   ("u_Model",   glm::mat4(1.0f));
    const int samplers[2] = { 7, 8 };
    mi->SetIntArray("u_Samplers", samplers, 2);

    Scene scene;
    Entity e = scene.CreateEntity();
    auto& mc = scene.AddComponent<MeshComponent>(e);
    mc.MeshAsset = AssetHandle(static_cast<uint64_t>(12345));
    mc.MaterialOverrides.resize(1);
    mc.MaterialOverrides[0] = mi;

    SceneSerializer::Save(scene, kScenePath);

    // nlohmann/json is PRIVATE, so verify the serialized structure by reading
    // the raw file text (same approach as VersionFieldPresent): the wrapper
    // field, the submesh index, every uniform name, and every type tag must be
    // present.
    std::ifstream fin(kScenePath);
    ASSERT_TRUE(fin.is_open());
    std::string content((std::istreambuf_iterator<char>(fin)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("\"materialOverrides\""), std::string::npos);
    EXPECT_NE(content.find("\"submesh\""),           std::string::npos);
    EXPECT_NE(content.find("\"uniforms\""),          std::string::npos);
    EXPECT_NE(content.find("\"u_Color\""),           std::string::npos);
    EXPECT_NE(content.find("\"u_Tint\""),            std::string::npos);
    EXPECT_NE(content.find("\"u_Offset\""),          std::string::npos);
    EXPECT_NE(content.find("\"u_Tiling\""),          std::string::npos);
    EXPECT_NE(content.find("\"u_Count\""),           std::string::npos);
    EXPECT_NE(content.find("\"u_Model\""),           std::string::npos);
    EXPECT_NE(content.find("\"u_Samplers\""),        std::string::npos);
    EXPECT_NE(content.find("\"Float4\""),            std::string::npos);
    EXPECT_NE(content.find("\"Float3\""),            std::string::npos);
    EXPECT_NE(content.find("\"Float2\""),            std::string::npos);
    EXPECT_NE(content.find("\"Float\""),             std::string::npos);
    EXPECT_NE(content.find("\"Int\""),               std::string::npos);
    EXPECT_NE(content.find("\"Mat4\""),              std::string::npos);
    EXPECT_NE(content.find("\"IntArray\""),          std::string::npos);
    // The base Material is NOT serialized (rebuilt on load) - the shader path
    // must not leak into materialOverrides.
    EXPECT_EQ(content.find("\"shader\""), std::string::npos);
}

TEST_F(SceneSerializerTest, LoadIsGracefulWhenMeshBaseUnavailable)
{
    // Save a scene carrying a material override (NullShader-backed instance),
    // then Load it back. The Mesh asset (12345) is not in the AssetManager
    // registry, so mc.Mesh stays null and the override-rebuild guard skips
    // instead of crashing. The MeshComponent's asset UUID is still preserved.
    auto shader = DM::CreateRef<NullShader>();
    auto mi = DM::CreateRef<MaterialInstance>(DM::CreateRef<Material>(shader));
    mi->SetFloat4("u_Color", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    Scene scene;
    Entity e = scene.CreateEntity();
    auto& mc = scene.AddComponent<MeshComponent>(e);
    mc.MeshAsset = AssetHandle(static_cast<uint64_t>(12345));
    mc.MaterialOverrides.resize(1);
    mc.MaterialOverrides[0] = mi;
    uint64_t uuid = scene.GetComponent<IDComponent>(e).UUID;

    SceneSerializer::Save(scene, kScenePath);

    Scene loaded;
    ASSERT_TRUE(SceneSerializer::Load(loaded, kScenePath));
    Entity le = FindByUUID(loaded, uuid);
    ASSERT_NE(le, NullEntity);
    ASSERT_TRUE(loaded.HasComponent<MeshComponent>(le));
    auto& lmc = loaded.GetComponent<MeshComponent>(le);
    EXPECT_EQ(lmc.MeshAsset.GetUUID(), 12345u);
    // Base could not be rebuilt headless -> overrides dropped (graceful skip),
    // never a dangling/null entry left behind.
    EXPECT_TRUE(lmc.MaterialOverrides.empty());
}