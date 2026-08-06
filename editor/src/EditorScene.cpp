#include "EditorScene.h"
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
    v( 1,-1,-1,  0, 0,-1, 0,0); v(-1,-1,-1,  0, 0,-1, 1,0); v(-1, 1,-1,  0, 0,-1, 1,1); v( 1, 1,-1,  0, 0,-1, 0,1);
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

void EditorScene::SetupDefaultScene() {
    m_Scene = DM::CreateRef<Scene>();
    m_Scene->AddSystem(DM::CreateRef<TransformSystem>(*m_Scene));
    m_Scene->AddSystem(DM::CreateRef<LightSystem>(*m_Scene));
    m_Scene->AddSystem(DM::CreateRef<MeshRenderSystem>(*m_Scene));

    RenderCommand::SetClearColor({0.10f, 0.10f, 0.12f, 1.0f});
    RenderCommand::SetDepthTest(true);
    RenderCommand::SetDepthFunc(DepthFunc::Less);
    RenderCommand::SetCullMode(CullMode::Back);

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

    auto light = m_Scene->CreateEntity("Directional Light");
    auto& ltc = m_Scene->GetComponent<TransformComponent>(light);
    ltc.Translation = {4.0f, 6.0f, 3.0f}; ltc.Dirty = true;
    auto& lc = m_Scene->AddComponent<LightComponent>(light);
    lc.LightType = LightComponent::Type::Directional;
    lc.Color = {1.0f, 1.0f, 1.0f};
    lc.Intensity = 1.0f;
    lc.AmbientIntensity = 0.2f;

    auto cube = m_Scene->CreateEntity("Cube");
    m_Scene->GetComponent<TransformComponent>(cube).Dirty = true;
    auto& mc = m_Scene->AddComponent<MeshComponent>(cube);
    mc.Mesh = MakeCubeMesh();
    if (baseMat) {
        mc.MaterialOverrides.resize(1);
        mc.MaterialOverrides[0] = DM::CreateRef<MaterialInstance>(baseMat);
    }
}

void EditorScene::OnUpdate(Timestep ts) {
    if (m_Scene)
        m_Scene->OnUpdate(ts);
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

void EditorScene::NewScene() {
    SetupDefaultScene();
}