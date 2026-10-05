#include "EditorScene.h"
#include "SceneDuplicator.h"
#include <DMGameEngine/Scene/SceneSerializer.h>
#ifdef DMGE_ANIMATION
#include <DMGameEngine/Scene/Systems/AnimationSystem.h>
#include <DMGameEngine/Scene/Systems/SkinnedMeshRenderSystem.h>
#endif
#include <filesystem>
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

static DM::Ref<EditorCameraController> MakeCamera() {
    auto cam = DM::CreateRef<EditorCameraController>(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    cam->SetDistance(8.0f);
    return cam;
}

EditorScene::EditorScene() {
    FramebufferSpecification spec;
    spec.Width = 1280; spec.Height = 720;
    spec.Attachments.push_back({TextureFormat::RGBA8});
    spec.DepthFormat = TextureFormat::Depth;
    m_FB = FrameBuffer::Create(spec);
    // First tab: default demo scene (cube + directional light).
    auto tab = SceneTab{};
    tab.Name = "Untitled-1"; m_UntitledCounter = 1;
    tab.EditScene = DM::CreateRef<Scene>();
    RegisterSystems(*tab.EditScene);
    ApplyRenderState();
    SetupDefaultSceneContents(*tab.EditScene);
    tab.Camera = MakeCamera();
    tab.Active = tab.EditScene;
    m_Tabs.push_back(std::move(tab));
    m_Active = 0;
}

void EditorScene::RegisterSystems(Scene& s) {
    s.AddSystem(DM::CreateRef<TransformSystem>(s));
    s.AddSystem(DM::CreateRef<LightSystem>(s));
#ifdef DMGE_ANIMATION
    // AnimationSystem MUST be registered BEFORE MeshRenderSystem: it samples
    // the palette each update and the skin pass reads it at render time.
    // Registering it costs nothing when the scene holds no AnimatorComponent.
    // SkinnedMeshRenderSystem is likewise required: with DMGE_ANIMATION=ON
    // MeshRenderSystem EXCLUDES AnimatorComponent entities (double-draw
    // guard), so without the skin pass such entities would never be drawn.
    s.AddSystem(DM::CreateRef<AnimationSystem>(s));
#endif
    s.AddSystem(DM::CreateRef<MeshRenderSystem>(s));
#ifdef DMGE_ANIMATION
    s.AddSystem(DM::CreateRef<SkinnedMeshRenderSystem>(s));
#endif
}

void EditorScene::ApplyRenderState() {
    RenderCommand::SetClearColor({0.10f, 0.10f, 0.12f, 1.0f});
    RenderCommand::SetDepthTest(true);
    RenderCommand::SetDepthFunc(DepthFunc::Less);
    RenderCommand::SetCullMode(CullMode::Back);
}

void EditorScene::SetupDefaultSceneContents(Scene& s) {
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

    auto light = s.CreateEntity("Directional Light");
    auto& ltc = s.GetComponent<TransformComponent>(light);
    ltc.Translation = {4.0f, 6.0f, 3.0f}; ltc.Dirty = true;
    auto& lc = s.AddComponent<LightComponent>(light);
    lc.LightType = LightComponent::Type::Directional;
    lc.Color = {1.0f, 1.0f, 1.0f};
    lc.Intensity = 1.0f;
    lc.AmbientIntensity = 0.2f;

    auto cube = s.CreateEntity("Cube");
    s.GetComponent<TransformComponent>(cube).Dirty = true;
    auto& mc = s.AddComponent<MeshComponent>(cube);
    mc.Mesh = MakeCubeMesh();
    if (baseMat) {
        mc.MaterialOverrides.resize(1);
        mc.MaterialOverrides[0] = DM::CreateRef<MaterialInstance>(baseMat);
    }
}

// ── Tab management ──────────────────────────────────────────────

int EditorScene::AddUntitledTab() {
    SceneTab tab;
    tab.Name = "Untitled-" + std::to_string(++m_UntitledCounter);
    tab.EditScene = DM::CreateRef<Scene>();
    RegisterSystems(*tab.EditScene);
    ApplyRenderState();
    tab.Camera = MakeCamera();
    tab.Active = tab.EditScene;
    m_Tabs.push_back(std::move(tab));
    return static_cast<int>(m_Tabs.size()) - 1;
}

int EditorScene::AddTabFromFile(const std::string& path) {
    // Load into a fresh empty scene so opening never merges with existing entities.
    auto s = DM::CreateRef<Scene>();
    RegisterSystems(*s);
    if (!SceneSerializer::Load(*s, path))
        return -1;
    SceneTab tab;
    tab.Path = path;
    tab.Name = std::filesystem::path(path).stem().string();
    if (tab.Name.empty()) tab.Name = path;
    tab.EditScene = s;
    ApplyRenderState();
    tab.Camera = MakeCamera();
    tab.Active = tab.EditScene;
    m_Tabs.push_back(std::move(tab));
    return static_cast<int>(m_Tabs.size()) - 1;
}

int EditorScene::FindTabByPath(const std::string& path) const {
    if (path.empty()) return -1;
    for (int i = 0; i < static_cast<int>(m_Tabs.size()); ++i)
        if (m_Tabs[i].Path == path) return i;
    return -1;
}

void EditorScene::CloseTab(int index) {
    auto* tab = GetTab(index);
    if (!tab) return;
    // Discard any play copy on the closing tab.
    if (tab->Playing) {
        tab->PlayScene.reset();
        tab->Playing = false;
        tab->Paused = false;
        tab->Active = tab->EditScene;
    }
    m_Tabs.erase(m_Tabs.begin() + index);
    if (m_Active >= static_cast<int>(m_Tabs.size()))
        m_Active = static_cast<int>(m_Tabs.size()) - 1;
    // The editor is never scene-less: reopen an empty tab.
    if (m_Tabs.empty()) {
        SceneTab tab2;
        tab2.Name = "Untitled-" + std::to_string(++m_UntitledCounter);
        tab2.EditScene = DM::CreateRef<Scene>();
        RegisterSystems(*tab2.EditScene);
        ApplyRenderState();
        tab2.Camera = MakeCamera();
        tab2.Active = tab2.EditScene;
        m_Tabs.push_back(std::move(tab2));
        m_Active = 0;
    }
}

void EditorScene::SetActive(int i) {
    if (i >= 0 && i < static_cast<int>(m_Tabs.size()))
        m_Active = i;
}

int EditorScene::FindPlayingTab() const {
    for (int i = 0; i < static_cast<int>(m_Tabs.size()); ++i)
        if (m_Tabs[i].Playing) return i;
    return -1;
}

// ── Active-tab conveniences ─────────────────────────────────────

Scene* EditorScene::GetEditScene() const {
    const auto* t = GetActiveTab();
    return t ? t->EditScene.get() : nullptr;
}

Scene* EditorScene::GetScene() const {
    const auto* t = GetActiveTab();
    return t ? t->Active.get() : nullptr;
}

EditorCameraController* EditorScene::GetCamera() const {
    const auto* t = GetActiveTab();
    return t ? t->Camera.get() : nullptr;
}

Entity EditorScene::GetSelected() const {
    const auto* t = GetActiveTab();
    return t ? t->Selected : NullEntity;
}

void EditorScene::SetSelected(Entity e) {
    if (auto* t = GetActiveTab())
        t->Selected = e;
}

// ── Play mode (active tab) ──────────────────────────────────────

void EditorScene::EnterPlayMode() {
    auto* tab = GetActiveTab();
    if (!tab || tab->Playing || !tab->EditScene)
        return;
    if (FindPlayingTab() != -1) {
        // At most one runtime at a time; the caller surfaces a message.
        return;
    }
    // Snapshot isolation: the edit scene object stays untouched; we run on a
    // deep copy (shared read-only Mesh/Material refs - see SceneDuplicator.h
    // for why a ref-sharing copy beats a SceneSerializer JSON round-trip).
    tab->PlayScene = DM::CreateRef<Scene>();
    RegisterSystems(*tab->PlayScene);
    EditorSceneCopy::CopyAllEntities(*tab->EditScene, *tab->PlayScene);
    tab->Active = tab->PlayScene;
    tab->Playing = true;
    tab->Paused = false;
}

void EditorScene::ExitPlayMode() {
    auto* tab = GetActiveTab();
    if (!tab || !tab->Playing)
        return;
    tab->PlayScene.reset();       // discard every change made during play
    tab->Active = tab->EditScene; // restore the pristine edit scene
    tab->Playing = false;
    tab->Paused = false;
}

bool EditorScene::IsPlaying() const {
    const auto* t = GetActiveTab();
    return t && t->Playing;
}

bool EditorScene::IsPaused() const {
    const auto* t = GetActiveTab();
    return t && t->Paused;
}

void EditorScene::SetPaused(bool p) {
    if (auto* t = GetActiveTab())
        t->Paused = p;
}

// ── Frame update / render ───────────────────────────────────────

void EditorScene::OnUpdate(Timestep ts) {
    // Background play simulations keep running (multi-scene policy, see
    // EditorScene.h): tick every playing tab regardless of visibility.
    for (auto& tab : m_Tabs) {
        if (tab.Playing && !tab.Paused && tab.Active)
            tab.Active->OnUpdate(ts);
    }
    // Active tab edit mode: tick with dt=0 purely so the TransformSystem
    // recomputes dirty world matrices (gizmo/picking/render read
    // WorldMatrix); no time-dependent behavior is driven.
    if (auto* t = GetActiveTab(); t && !t->Playing && t->Active)
        t->Active->OnUpdate(Timestep(0.0f));
    if (auto* cam = GetCamera())
        cam->OnUpdate(ts);
}

void EditorScene::Render() {
    auto* t = GetActiveTab();
    if (!t || !t->Active || !t->Camera || !m_FB)
        return;
    Renderer::BeginScene(t->Camera->GetCamera(), m_FB);
    RenderCommand::SetViewport(0, 0, m_FB->GetWidth(), m_FB->GetHeight());
    t->Active->OnRender();
    Renderer::EndScene();
}

void EditorScene::Resize(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;
    if (m_FB) m_FB->Resize(w, h);
    if (auto* t = GetActiveTab()) {
        if (t->Camera)
            t->Camera->SetViewportSize(w, h);
    }
}
