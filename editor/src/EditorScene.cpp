#include "EditorScene.h"
#include "SceneDuplicator.h"
#include <DMGameEngine/Scene/SceneSerializer.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace DMGameEngine;

static DM::Ref<Mesh> MakeCubeMesh() {
    auto mesh = DM::CreateRef<Mesh>();
    mesh->Layout = BufferLayout{
        {ShaderDataType::Float3, "a_Position"},
        {ShaderDataType::Float3, "a_Normal"},
        {ShaderDataType::Float2, "a_TexCoords"},
    };
    auto v = [&](float px, float py, float pz, float nx, float ny, float nz, float u, float w) {
        mesh->Vertices.push_back(px); mesh->Vertices.push_back(py); mesh->Vertices.push_back(pz);
        mesh->Vertices.push_back(nx); mesh->Vertices.push_back(ny); mesh->Vertices.push_back(nz);
        mesh->Vertices.push_back(u);  mesh->Vertices.push_back(w);
    };
    v( 1,-1, 1,  1, 0, 0, 0,0); v( 1,-1,-1,  1, 0, 0, 1,0); v( 1, 1,-1,  1, 0, 0, 1,1); v( 1, 1, 1,  1, 0, 0, 0,1);
    v(-1,-1,-1, -1, 0, 0, 0,0); v(-1,-1, 1, -1, 0, 0, 1,0); v(-1, 1, 1, -1, 0, 0, 1,1); v(-1, 1,-1, -1, 0, 0, 0,1);
    v(-1, 1, 1,  0, 1, 0, 0,0); v( 1, 1, 1,  0, 1, 0, 1,0); v( 1, 1,-1,  0, 1, 0, 1,1); v(-1, 1,-1,  0, 1, 0, 0,1);
    v(-1,-1,-1,  0,-1, 0, 0,0); v( 1,-1,-1,  0,-1, 0, 1,0); v( 1,-1, 1,  0,-1, 0, 1,1); v(-1,-1, 1,  0,-1, 0, 0,1);
    v(-1,-1, 1,  0, 0, 1, 0,0); v( 1,-1, 1,  0, 0, 1, 1,0); v( 1, 1, 1,  0, 0, 1, 1,1); v(-1, 1, 1,  0, 0, 1, 0,1);
    v( 1,-1,-1,  0, 0,-1, 0,0); v(-1,-1,-1,  0, 0,-1, 1,0); v(-1, 1,-1,  0, 0,-1, 1,1); v(-1, 1,-1,  0, 0,-1, 0,1);
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t base = face * 4;
        mesh->Indices.push_back(base + 0); mesh->Indices.push_back(base + 1); mesh->Indices.push_back(base + 2);
        mesh->Indices.push_back(base + 0); mesh->Indices.push_back(base + 2); mesh->Indices.push_back(base + 3);
    }
    SubMesh sub; sub.IndexOffset = 0; sub.IndexCount = 36;
    mesh->SubMeshes.push_back(sub);
    return mesh;
}

EditorScene::EditorScene() {
    FramebufferSpecification spec;
    spec.Width = 1280; spec.Height = 720;
    spec.Attachments.push_back({TextureFormat::RGBA8});
    spec.DepthFormat = TextureFormat::Depth;
    m_FB = FrameBuffer::Create(spec);
    m_Camera = DM::CreateRef<EditorCameraController>(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    m_Camera->SetDistance(8.0f);
    SetupDefaultScene();
}

void EditorScene::RegisterSystems(Scene& s) {
    s.AddSystem(DM::CreateRef<TransformSystem>(s));
    s.AddSystem(DM::CreateRef<LightSystem>(s));
    s.AddSystem(DM::CreateRef<MeshRenderSystem>(s));
}

void EditorScene::CreateEmptyScene() {
    m_EditScene = DM::CreateRef<Scene>();
    RegisterSystems(*m_EditScene);

    RenderCommand::SetClearColor({0.10f, 0.10f, 0.12f, 1.0f});
    RenderCommand::SetDepthTest(true);
    RenderCommand::SetDepthFunc(DepthFunc::Less);
    RenderCommand::SetCullMode(CullMode::Back);
}

void EditorScene::SetupDefaultScene() {
    CreateEmptyScene();

    auto shader = AssetManager::Get().Load<Shader>(
        "D:/CPPPractices/DMGameEngine/engine/shaders/BlinnPhong.glsl");
    DM::Ref<Material> baseMat;
    if (shader) {
        baseMat = DM::CreateRef<Material>(shader);
        baseMat->SetFloat3("u_AlbedoColor", {0.8f, 0.8f, 0.85f});
        baseMat->SetFloat("u_SpecularStrength", 0.5f);
        baseMat->SetFloat("u_Shininess", 64.0f);
        baseMat->SetInt("u_UseTexture", 0);
    }

    auto light = m_EditScene->CreateEntity("Directional Light");
    auto& ltc = m_EditScene->GetComponent<TransformComponent>(light);
    ltc.Translation = {4.0f, 6.0f, 3.0f}; ltc.Dirty = true;
    auto& lc = m_EditScene->AddComponent<LightComponent>(light);
    lc.LightType = LightComponent::Type::Directional;
    lc.Color = {1.0f, 1.0f, 1.0f};
    lc.Intensity = 1.0f;
    lc.AmbientIntensity = 0.2f;

    auto cube = m_EditScene->CreateEntity("Cube");
    m_EditScene->GetComponent<TransformComponent>(cube).Dirty = true;
    auto& mc = m_EditScene->AddComponent<MeshComponent>(cube);
    mc.Mesh = MakeCubeMesh();
    if (baseMat) {
        mc.MaterialOverrides.resize(1);
        mc.MaterialOverrides[0] = DM::CreateRef<MaterialInstance>(baseMat);
    }

    m_Scene = m_EditScene;
}

void EditorScene::NewScene() {
    SetupDefaultScene();
}

bool EditorScene::LoadSceneFromFile(const std::string& path) {
    // Load into a fresh empty scene so opening never merges with existing entities.
    auto s = DM::CreateRef<Scene>();
    RegisterSystems(*s);
    if (!SceneSerializer::Load(*s, path))
        return false;
    m_EditScene = s;
    if (!m_Playing)
        m_Scene = m_EditScene;
    return true;
}

void EditorScene::EnterPlayMode() {
    if (m_Playing || !m_EditScene)
        return;
    // Snapshot isolation: the edit scene object stays untouched; we run on a
    // deep copy (shared read-only Mesh/Material refs - see SceneDuplicator.h
    // for why a ref-sharing copy beats a SceneSerializer JSON round-trip).
    m_PlayScene = DM::CreateRef<Scene>();
    RegisterSystems(*m_PlayScene);
    EditorSceneCopy::CopyAllEntities(*m_EditScene, *m_PlayScene);
    m_Scene = m_PlayScene;
    m_Playing = true;
    m_Paused = false;
}

void EditorScene::ExitPlayMode() {
    if (!m_Playing)
        return;
    m_PlayScene.reset();          // discard every change made during play
    m_Scene = m_EditScene;        // restore the pristine edit scene
    m_Playing = false;
    m_Paused = false;
}

void EditorScene::OnUpdate(Timestep ts) {
    if (m_Scene) {
        if (m_Playing) {
            // Simulation ticks on the play copy; Pause freezes it.
            if (!m_Paused)
                m_Scene->OnUpdate(ts);
        } else {
            // Edit mode: tick with dt=0 purely so the TransformSystem
            // recomputes dirty world matrices (gizmo/picking/render read
            // WorldMatrix); no time-dependent behavior is driven.
            m_Scene->OnUpdate(Timestep(0.0f));
        }
    }
    if (m_Camera)
        m_Camera->OnUpdate(ts);
}

void EditorScene::Render() {
    if (!m_Scene || !m_Camera || !m_FB)
        return;
    Renderer::BeginScene(m_Camera->GetCamera(), m_FB);
    RenderCommand::SetViewport(0, 0, m_FB->GetWidth(), m_FB->GetHeight());
    m_Scene->OnRender();
    Renderer::EndScene();
}

void EditorScene::Resize(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;
    if (m_FB) m_FB->Resize(w, h);
    if (m_Camera) m_Camera->SetViewportSize(w, h);
}
