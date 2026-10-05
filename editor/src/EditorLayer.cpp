#include "EditorLayer.h"
#include "EditorScene.h"
#include "ProjectExporter.h"
#include "SceneDuplicator.h"
#include <DMGameEngine/Core/Log.h>
#include <DMGameEngine/Scene/SceneSerializer.h>
#include <DMGameEngine/Scene/Components/Components.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

using namespace DMGameEngine;

struct AABBd { glm::vec3 min, max; };

static bool MeshLocalBounds(const Mesh& mesh, AABBd& out) {
    const auto& elems = mesh.Layout.GetElements();
    const uint32_t stride = mesh.Layout.GetStride();
    uint32_t posOffset = UINT32_MAX;
    for (const auto& el : elems)
        if (el.Name == "a_Position") { posOffset = el.Offset; break; }
    if (posOffset == UINT32_MAX || stride == 0 || mesh.Vertices.empty()) return false;
    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
    const float* data = mesh.Vertices.data();
    const uint32_t fStride = stride / 4;
    const uint32_t fPos = posOffset / 4;
    const uint32_t count = static_cast<uint32_t>(mesh.Vertices.size()) / fStride;
    for (uint32_t i = 0; i < count; ++i) {
        const float* p = data + i * fStride + fPos;
        glm::vec3 v(p[0], p[1], p[2]);
        mn = glm::min(mn, v);
        mx = glm::max(mx, v);
    }
    out = { mn, mx };
    return true;
}

static bool RayAABB(const glm::vec3& o, const glm::vec3& d,
                    const glm::vec3& bmin, const glm::vec3& bmax, float& tHit) {
    float tmin = 0.0f, tmax = FLT_MAX;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-6f) {
            if (o[i] < bmin[i] || o[i] > bmax[i]) return false;
        } else {
            float inv = 1.0f / d[i];
            float t1 = (bmin[i] - o[i]) * inv;
            float t2 = (bmax[i] - o[i]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    tHit = tmin;
    return true;
}

// ── Default material helper ─────────────────────────────────────
// assimp imports carry no material; assign a default Blinn-Phong so the mesh
// actually renders. (AssetLoader<Material> is not exported - K-002 - so the
// editor builds the material itself instead of loading a .mat file.)
static DM::Ref<Material> CreateDefaultMaterial() {
    auto shader = AssetManager::Get().Load<Shader>(
        "D:/CPPPractices/DMGameEngine/engine/shaders/BlinnPhong.glsl");
    if (!shader) return nullptr;
    auto mat = DM::CreateRef<Material>(shader);
    mat->SetFloat3("u_AlbedoColor", {0.8f, 0.8f, 0.85f});
    mat->SetFloat("u_SpecularStrength", 0.5f);
    mat->SetFloat("u_Shininess", 64.0f);
    mat->SetInt("u_UseTexture", 0);
    return mat;
}

static void AssignDefaultMaterial(MeshComponent& mc) {
    auto baseMat = CreateDefaultMaterial();
    if (!baseMat) return;
    mc.MaterialOverrides.clear();
    mc.MaterialOverrides.resize(mc.Mesh ? mc.Mesh->SubMeshes.size() : 0);
    for (auto& ov : mc.MaterialOverrides)
        ov = DM::CreateRef<MaterialInstance>(baseMat);
}

static bool IsModelExtension(std::string ext) {
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".mesh";
}

static std::string SanitizeFilename(const std::string& name) {
    std::string out;
    for (char c : name) {
        bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.';
        out.push_back(ok ? c : '_');
    }
    return out.empty() ? std::string("Entity") : out;
}

static constexpr const char* kConfigPath = "editor_config.ini";
static constexpr size_t kMaxRecentScenes = 8;

EditorLayer::EditorLayer()
    : Layer("EditorLayer", LayerType::Tool) {}

void EditorLayer::OnAttach() {
    ImGuiIO& io = ImGui::GetIO();
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    // The engine enables multi-viewport (ViewportsEnable) which makes
    // io.MousePos global-screen while some ImGui window rects use a different
    // origin -> ImGuizmo hit-testing breaks (mouse vs rect mismatch). The
    // editor docks everything in the main window, so disable multi-viewport.
    // Docking still works; only OS-level detached windows are turned off.
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    DMGE_CLIENT_INFO("EditorLayer attached");
    m_Log.OnAttach();
    LoadConfig();
}

void EditorLayer::OnDetach() {
    SaveConfig();
    m_Log.OnDetach();
}

void EditorLayer::OnUpdate(Timestep ts) {
    if (auto* cam = m_Scene.GetCamera())
        cam->SetEnabled(m_ViewportFocused && m_ViewportHovered);
    m_Scene.OnUpdate(ts);
}

void EditorLayer::OnEvent(Event& e) {
    if (m_Scene.GetCamera())
        m_Scene.GetCamera()->OnEvent(e);
}

void EditorLayer::OnRender() {
    m_Scene.Render();
}

void EditorLayer::OnImGuiRender() {
    // Global shortcuts first: polled against this frame's ImGui IO (after
    // NewFrame), so ImGui::IsKeyPressed is valid here (same as the gizmo
    // 1-4 keys in DrawViewport).
    ProcessShortcuts();
    ImGuizmo::BeginFrame();
    DrawModalDialogs();
    DrawDockspace();
    DrawMenuBar();
    DrawSceneTabs();
    DrawViewport();
    DrawHierarchy();
    DrawInspector();
    DrawSystems();
    DrawAssetBrowser();
    m_Log.OnImGuiRender();
}

// ── Play mode isolation (stage 4, per active tab) ───────────────

void EditorLayer::BeginPlay() {
    auto* tab = m_Scene.GetActiveTab();
    if (!tab || tab->Playing) return;
    if (m_Scene.FindPlayingTab() != -1) {
        // Multi-scene policy: at most one runtime at a time.
        auto* playing = m_Scene.GetTab(m_Scene.FindPlayingTab());
        DMGE_CLIENT_WARN("Scene '{0}' is already playing - stop it before playing another scene.",
                         playing ? playing->Name : "?");
        return;
    }
    // Remember the selection by UUID so it can be re-resolved on the edit
    // scene after Stop (runtime entity ids differ between the two scenes).
    uint64_t uuid = 0;
    Scene* edit = tab->EditScene.get();
    const Entity sel = tab->Selected;
    if (edit && sel != NullEntity && edit->HasComponent<IDComponent>(sel))
        uuid = edit->GetComponent<IDComponent>(sel).UUID;
    tab->SelectedUUIDBeforePlay = uuid;
    m_Scene.EnterPlayMode();
    // Re-resolve the selection onto the play copy so the read-only inspector
    // keeps showing the same logical entity while playing.
    if (tab->Playing && uuid != 0) {
        Scene* play = tab->PlayScene.get();
        if (play) {
            play->GetRegistry().view<IDComponent>().each([&](auto eh, IDComponent& idc) {
                if (idc.UUID == uuid)
                    m_Scene.SetSelected(static_cast<Entity>(eh));
            });
        }
    }
    DMGE_CLIENT_INFO("Play mode entered on '{0}' - edit state snapshotted, changes during play will be discarded on Stop",
                     tab->Name);
}

void EditorLayer::EndPlay() {
    auto* tab = m_Scene.GetActiveTab();
    if (!tab || !tab->Playing) return;
    const uint64_t uuid = tab->SelectedUUIDBeforePlay;
    m_Scene.ExitPlayMode();   // discards the play copy, restores the edit scene
    m_Scene.SetSelected(NullEntity);
    Scene* edit = tab->EditScene.get();
    if (edit && uuid != 0) {
        edit->GetRegistry().view<IDComponent>().each([&](auto eh, IDComponent& idc) {
            if (idc.UUID == uuid)
                m_Scene.SetSelected(static_cast<Entity>(eh));
        });
    }
    tab->SelectedUUIDBeforePlay = 0;
    DMGE_CLIENT_INFO("Play mode stopped on '{0}' - edit state restored", tab->Name);
}

// ── Global keyboard shortcuts + shared menu actions ─────────────

void EditorLayer::NewSceneInTab() {
    int idx = m_Scene.AddUntitledTab();
    m_Scene.SetActive(idx);
    m_Scene.SetSelected(NullEntity);
}

void EditorLayer::OpenSceneDialog() {
    auto* activeTab = m_Scene.GetActiveTab();
    std::snprintf(m_PathBuf, sizeof(m_PathBuf), "%s",
                  activeTab ? activeTab->Path.c_str() : "");
    m_Dialog = Dialog::OpenScene;
    m_DialogOpenPending = true;
}

void EditorLayer::SaveActiveScene() {
    auto* activeTab = m_Scene.GetActiveTab();
    if (!activeTab) return;
    if (activeTab->Path.empty()) {
        m_PathBuf[0] = '\0';
        m_Dialog = Dialog::SaveSceneAs;   // never-saved scene: ask where
        m_DialogOpenPending = true;
    } else {
        SaveSceneToPath(activeTab->Path);
    }
}

void EditorLayer::CreateEmptyEntity() {
    if (auto* s = m_Scene.GetScene()) {
        m_Scene.SetSelected(s->CreateEntity("Entity"));
        MarkActiveDirty();
    }
}

void EditorLayer::DeleteSelectedEntity() {
    const Entity sel = m_Scene.GetSelected();
    if (sel == NullEntity) return;
    if (auto* s = m_Scene.GetScene()) {
        s->DestroyEntity(sel);
        m_Scene.SetSelected(NullEntity);
        MarkActiveDirty();
    }
}

void EditorLayer::ProcessShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    // Text input owns the keyboard (e.g. Log filter box, path dialogs,
    // UUID fields): every shortcut yields so typing cannot trigger actions.
    if (io.WantTextInput)
        return;
    // Modal dialogs open (close-confirm, open/save/export): route keys to
    // the dialog, not to the scene behind it. (Path dialogs additionally
    // hold the keyboard via WantTextInput above.)
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        return;

    const bool playing = m_Scene.IsPlaying();

    // Play controls: F5 = Play (edit) / Pause-Resume toggle (playing),
    // F6 = Stop. The popup guard above also yields these to modal dialogs.
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (playing)
            m_Scene.SetPaused(!m_Scene.IsPaused());   // Play <-> Pause toggle
        else
            BeginPlay();   // warns itself if another tab owns the runtime slot
    }
    if (playing && ImGui::IsKeyPressed(ImGuiKey_F6, false))
        EndPlay();

    // Edit-type shortcuts: inert during play mode, matching the menu bar's
    // disabled state for the same actions.
    if (playing)
        return;

    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_N, false))
        NewSceneInTab();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_O, false))
        OpenSceneDialog();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
        SaveActiveScene();
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_A, false))
        CreateEmptyEntity();
    if (!ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        DeleteSelectedEntity();
}

// ── Scene management (stage 3, multi-tab) ───────────────────────

void EditorLayer::OpenSceneFromPath(const std::string& path) {
    if (path.empty()) return;
    // Already open? Just focus its tab (no duplicate tabs for one file).
    if (int existing = m_Scene.FindTabByPath(path); existing != -1) {
        m_Scene.SetActive(existing);
        DMGE_CLIENT_INFO("Scene already open - focused tab '{0}'", m_Scene.GetTab(existing)->Name);
        return;
    }
    int idx = m_Scene.AddTabFromFile(path);
    if (idx != -1) {
        m_Scene.SetActive(idx);
        PushRecentScene(path);
        DMGE_CLIENT_INFO("Scene loaded into new tab: {0}", path);
    } else {
        DMGE_CLIENT_WARN("Failed to load scene: {0}", path);
    }
}

void EditorLayer::SaveSceneToPath(const std::string& path) {
    auto* tab = m_Scene.GetActiveTab();
    Scene* s = m_Scene.GetEditScene();
    if (!s || !tab || path.empty()) return;
    // Always serialize the EDIT scene - never the play copy - so a save during
    // play cannot capture transient play-mode state.
    SceneSerializer::Save(*s, path);
    tab->Path = path;
    auto stem = std::filesystem::path(path).stem().string();
    if (!stem.empty()) tab->Name = stem;
    tab->Dirty = false;
    PushRecentScene(path);
    DMGE_CLIENT_INFO("Scene saved: {0}", path);
}

void EditorLayer::RequestCloseTab(int index) {
    auto* tab = m_Scene.GetTab(index);
    if (!tab) return;
    if (tab->Dirty) {
        m_PendingCloseTab = index;
        m_Dialog = Dialog::ConfirmCloseTab;
        m_DialogOpenPending = true;
    } else {
        m_Scene.CloseTab(index);
    }
}

void EditorLayer::MarkActiveDirty() {
    if (auto* tab = m_Scene.GetActiveTab())
        tab->Dirty = true;
}

// ── Runnable project export (stage 4) ───────────────────────────

void EditorLayer::ExportRunnableProject(const std::string& outputDir) {
    // Always export the EDIT scene - never the play copy.
    Scene* edit = m_Scene.GetEditScene();
    if (!edit || outputDir.empty()) return;
    auto res = ProjectExporter::Export(outputDir, *edit);
    if (res.Ok) {
        DMGE_CLIENT_INFO("Runnable project exported to '{0}'", res.OutputDir);
        DMGE_CLIENT_INFO("  Scene: assets/scene/main.scene, models copied: {0}, shaders: shaders/", res.ModelsCopied);
        if (res.ProceduralMeshes > 0)
            DMGE_CLIENT_WARN("  {0} procedural mesh entity/entities were NOT exported (in-editor meshes cannot be serialized, see KB-07 K-012) - they will not render in the exported game.", res.ProceduralMeshes);
        DMGE_CLIENT_INFO("  Build it yourself (we do not build automatically): "
                         "cmake -B build -DDMGE_ENGINE_DIR=\"<repo>/engine\" && cmake --build build - see README.md in the output directory.");
    } else {
        DMGE_CLIENT_ERROR("Export failed: {0}", res.Error);
    }
}

// ── Recent-files persistence ────────────────────────────────────

void EditorLayer::PushRecentScene(const std::string& path) {
    std::erase(m_RecentScenes, path);
    m_RecentScenes.insert(m_RecentScenes.begin(), path);
    if (m_RecentScenes.size() > kMaxRecentScenes)
        m_RecentScenes.resize(kMaxRecentScenes);
    SaveConfig();
}

void EditorLayer::LoadConfig() {
    m_RecentScenes.clear();
    std::ifstream in(kConfigPath);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        constexpr const char* kPrefix = "recent=";
        if (line.rfind(kPrefix, 0) == 0 && line.size() > std::strlen(kPrefix))
            m_RecentScenes.push_back(line.substr(std::strlen(kPrefix)));
    }
}

void EditorLayer::SaveConfig() {
    std::ofstream out(kConfigPath);
    if (!out.is_open()) return;
    for (const auto& p : m_RecentScenes)
        out << "recent=" << p << "\n";
}

// ── Modal dialogs ───────────────────────────────────────────────

void EditorLayer::DrawModalDialogs() {
    if (m_DialogOpenPending) {
        ImGui::OpenPopup("EditorDialog");
        m_DialogOpenPending = false;
    }
    if (!ImGui::BeginPopupModal("EditorDialog", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize))
        return;

    switch (m_Dialog) {
    case Dialog::ConfirmCloseTab: {
        const SceneTab* tab = m_Scene.GetTab(m_PendingCloseTab);
        ImGui::Text("Close scene '%s'?", tab ? tab->Name.c_str() : "?");
        ImGui::TextDisabled("Unsaved changes will be lost.");
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            const int idx = m_PendingCloseTab;
            m_Dialog = Dialog::None;
            m_PendingCloseTab = -1;
            ImGui::CloseCurrentPopup();
            m_Scene.CloseTab(idx);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_Dialog = Dialog::None;
            m_PendingCloseTab = -1;
            ImGui::CloseCurrentPopup();
        }
        break;
    }
    case Dialog::OpenScene: {
        ImGui::Text("Open scene file (.scene) in a NEW tab:");
        ImGui::InputText("Path", m_PathBuf, sizeof(m_PathBuf));
        bool exists = m_PathBuf[0] && std::filesystem::exists(m_PathBuf);
        if (m_PathBuf[0] && !exists)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "File not found.");
        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (exists) {
                OpenSceneFromPath(m_PathBuf);
                m_Dialog = Dialog::None;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_Dialog = Dialog::None;
            ImGui::CloseCurrentPopup();
        }
        break;
    }
    case Dialog::SaveSceneAs: {
        ImGui::Text("Save scene as (.scene):");
        ImGui::InputText("Path", m_PathBuf, sizeof(m_PathBuf));
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (m_PathBuf[0]) {
                SaveSceneToPath(m_PathBuf);
                m_Dialog = Dialog::None;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_Dialog = Dialog::None;
            ImGui::CloseCurrentPopup();
        }
        break;
    }
    case Dialog::SavePrefab: {
        ImGui::Text("Save prefab as (.prefab):");
        ImGui::InputText("Path", m_PathBuf, sizeof(m_PathBuf));
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (m_PathBuf[0] && m_ContextEntity != NullEntity) {
                SavePrefab(m_ContextEntity, m_PathBuf);
                m_Dialog = Dialog::None;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_Dialog = Dialog::None;
            ImGui::CloseCurrentPopup();
        }
        break;
    }
    case Dialog::InstantiatePrefab: {
        ImGui::Text("Instantiate prefab (.prefab):");
        ImGui::InputText("Path", m_PathBuf, sizeof(m_PathBuf));
        bool exists = m_PathBuf[0] && std::filesystem::exists(m_PathBuf);
        if (m_PathBuf[0] && !exists)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "File not found.");
        if (ImGui::Button("Instantiate", ImVec2(120, 0))) {
            if (exists && InstantiatePrefab(m_PathBuf) != NullEntity) {
                m_Dialog = Dialog::None;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_Dialog = Dialog::None;
            ImGui::CloseCurrentPopup();
        }
        break;
    }
    case Dialog::ExportProject: {
        ImGui::Text("Export runnable project to directory:");
        ImGui::InputText("Directory", m_ExportDirBuf, sizeof(m_ExportDirBuf));
        ImGui::TextDisabled("Generates CMakeLists.txt + main.cpp + README + assets");
        ImGui::TextDisabled("(scene / shaders / referenced models). Not built automatically.");
        if (ImGui::Button("Export", ImVec2(120, 0))) {
            if (m_ExportDirBuf[0]) {
                ExportRunnableProject(m_ExportDirBuf);
                m_Dialog = Dialog::None;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_Dialog = Dialog::None;
            ImGui::CloseCurrentPopup();
        }
        break;
    }
    case Dialog::None:
        ImGui::CloseCurrentPopup();
        break;
    }
    ImGui::EndPopup();
}

// ── Dockspace ───────────────────────────────────────────────────

void EditorLayer::DrawDockspace() {
#ifdef IMGUI_HAS_DOCK
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##EditorDockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);
    ImGuiID dockspaceId = ImGui::GetID("DMEditorDock");
    ImGui::DockSpace(dockspaceId, ImVec2(0, 0),
                     ImGuiDockNodeFlags_PassthruCentralNode);
    // Unity-style default layout on first launch: scene tabs above the
    // viewport, left hierarchy, center viewport, right inspector, bottom
    // asset browser. ImGui persists the layout to imgui.ini afterwards, so
    // the user can rearrange freely.
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);
        ImGuiID dockMain = dockspaceId;
        ImGuiID dockTop   = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up,    0.06f, nullptr, &dockMain);
        ImGuiID dockLeft  = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left,  0.18f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.24f, nullptr, &dockMain);
        ImGuiID dockDown  = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down,  0.28f, nullptr, &dockMain);
        ImGui::DockBuilderDockWindow("Scenes",           dockTop);
        ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector",        dockRight);
        ImGui::DockBuilderDockWindow("Systems",          dockRight);
        ImGui::DockBuilderDockWindow("Asset Browser",    dockDown);
        ImGui::DockBuilderDockWindow("Log",               dockDown);
        ImGui::DockBuilderDockWindow("Viewport",         dockMain);
        ImGui::DockBuilderFinish(dockspaceId);
    }
    ImGui::End();
#endif
}

// ── Menu bar ────────────────────────────────────────────────────

void EditorLayer::DrawMenuBar() {
    auto* activeTab = m_Scene.GetActiveTab();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // New = fresh tab: nothing is replaced, so no confirm needed.
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                NewSceneInTab();
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                OpenSceneDialog();
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                SaveActiveScene();
            }
            if (ImGui::MenuItem("Save Scene As...", nullptr, false, !m_Scene.IsPlaying())) {
                std::snprintf(m_PathBuf, sizeof(m_PathBuf), "%s",
                              activeTab ? activeTab->Path.c_str() : "");
                m_Dialog = Dialog::SaveSceneAs;
                m_DialogOpenPending = true;
            }
            if (!m_RecentScenes.empty() && ImGui::BeginMenu("Recent Scenes")) {
                for (const auto& p : m_RecentScenes)
                    if (ImGui::MenuItem(p.c_str()))
                        OpenSceneFromPath(p);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close Tab")) {
                RequestCloseTab(m_Scene.GetActive());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Runnable Project...", nullptr, false, !m_Scene.IsPlaying())) {
                m_ExportDirBuf[0] = '\0';
                m_Dialog = Dialog::ExportProject;
                m_DialogOpenPending = true;
            }
            ImGui::Separator();
            // Alt+F4 is handled by the OS window manager (closes the window);
            // the label documents the system behavior, no editor handling.
            if (ImGui::MenuItem("Quit", "Alt+F4"))
                Application::Get().Quit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Entity")) {
            // Play-mode isolation: entity CRUD only in edit mode.
            if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+A", false, !m_Scene.IsPlaying())) {
                CreateEmptyEntity();
            }
            const Entity sel = m_Scene.GetSelected();
            if (sel != NullEntity && ImGui::MenuItem("Delete", "Del", false, !m_Scene.IsPlaying())) {
                DeleteSelectedEntity();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Play")) {
            auto* playingTab = m_Scene.GetTab(m_Scene.FindPlayingTab());
            if (m_Scene.IsPlaying()) {
                if (ImGui::MenuItem(m_Scene.IsPaused() ? "Resume" : "Pause", "F5"))
                    m_Scene.SetPaused(!m_Scene.IsPaused());
                if (ImGui::MenuItem("Stop", "F6"))
                    EndPlay();
            } else if (playingTab) {
                // Another tab owns the single runtime slot.
                ImGui::MenuItem(playingTab->Name.c_str(), nullptr, false, false);
                ImGui::TextDisabled("is playing - stop it first");
            } else if (ImGui::MenuItem("Play", "F5")) {
                BeginPlay();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rendering")) {
            // Render path toggle (ROADMAP 3e): takes effect from the next
            // frame; the current frame always finishes on its own path.
            auto cur = Renderer::GetRenderPath();
            if (ImGui::MenuItem("Forward", nullptr, cur == RenderPath::Forward))
            {
                if (cur != RenderPath::Forward) {
                    Renderer::SetRenderPath(RenderPath::Forward);
                    DMGE_CLIENT_INFO("Render path -> Forward");
                }
            }
            if (ImGui::MenuItem("Deferred", nullptr, cur == RenderPath::Deferred))
            {
                if (cur != RenderPath::Deferred) {
                    Renderer::SetRenderPath(RenderPath::Deferred);
                    DMGE_CLIENT_INFO("Render path -> Deferred");
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled("Active: %s",
                Renderer::GetActiveRenderPath() == RenderPath::Deferred ? "Deferred" : "Forward");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// ── Scene tabs (stage 3) ────────────────────────────────────────

void EditorLayer::DrawSceneTabs() {
    ImGui::Begin("Scenes");
    int closeRequested = -1;
    if (ImGui::BeginTabBar("SceneTabBar", ImGuiTabBarFlags_Reorderable)) {
        for (int i = 0; i < m_Scene.GetTabCount(); ++i) {
            auto* tab = m_Scene.GetTab(i);
            if (!tab) continue;
            std::string label = tab->Name;
            if (tab->Dirty) label += " *";
            if (tab->Playing) label += " (Playing)";
            bool open = true;
            ImGuiTabItemFlags flags = (i == m_Scene.GetActive())
                ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
                // Clicking a tab activates its scene; play/camera/selection
                // state is untouched (background play keeps running).
                if (m_Scene.GetActive() != i)
                    m_Scene.SetActive(i);
                ImGui::EndTabItem();
            }
            if (!open)   // close button ('x') on the tab item
                closeRequested = i;
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    // Handle close after the loop: closing mutates the tab list.
    if (closeRequested != -1)
        RequestCloseTab(closeRequested);
}

// ── Viewport ────────────────────────────────────────────────────

void EditorLayer::DrawViewport() {
    ImGui::Begin("Viewport");
    // Gizmo mode toolbar (screen buttons + 1/2/3/4 keys; does not conflict
    // with camera WASD/QE dolly).
    {
        auto modeBtn = [&](const char* label, int v) {
            bool active = (m_GizmoType == v);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label)) m_GizmoType = v;
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        modeBtn("Translate", 0);
        modeBtn("Rotate", 1);
        modeBtn("Scale", 2);
        modeBtn("None", -1);
        ImGui::NewLine();
        if (ImGui::IsKeyPressed(ImGuiKey_1)) m_GizmoType = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_2)) m_GizmoType = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_3)) m_GizmoType = 2;
        if (ImGui::IsKeyPressed(ImGuiKey_4)) m_GizmoType = -1;
    }
    m_ViewportFocused = ImGui::IsWindowFocused();
    m_ViewportHovered = ImGui::IsWindowHovered();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_ViewportSize = glm::vec2(avail.x, avail.y);

    // avail can be NEGATIVE while the dock layout is still settling (first
    // frames / tab switches); casting that straight to uint32 produces a
    // ~4-billion size that used to reach glTexStorage2D and die on the GL
    // assert (kb/KB-07 K-017). Guard the float BEFORE the cast.
    if (avail.x >= 1.0f && avail.y >= 1.0f && avail.x <= 8192.0f && avail.y <= 8192.0f)
        m_Scene.Resize(static_cast<uint32_t>(avail.x), static_cast<uint32_t>(avail.y));

    // Camera control via ImGui mouse state. ImGuiLayer intercepts engine mouse
    // events over the viewport (io.WantCaptureMouse) so EditorCameraController::OnEvent
    // never receives them - drive the camera directly here instead.
    // Right-drag = orbit, Middle-drag = pan, Wheel = zoom. Left = pick/gizmo.
    if (m_ViewportHovered && m_ViewportFocused && !ImGuizmo::IsUsing()) {
        auto* cam = m_Scene.GetCamera();
        if (cam) {
            ImGuiIO& io = ImGui::GetIO();
            const glm::vec2 d(io.MouseDelta.x, io.MouseDelta.y);
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                cam->SetYaw(cam->GetYaw() - d.x * cam->GetRotateSpeed());
                cam->SetPitch(cam->GetPitch() + d.y * cam->GetRotateSpeed());
            } else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                glm::vec3 fwd = glm::normalize(cam->GetTarget() - cam->GetPosition());
                glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
                glm::vec3 up  = glm::normalize(glm::cross(rgt, fwd));
                const float s = cam->GetPanSpeed() * cam->GetDistance();
                glm::vec3 t = cam->GetTarget();
                t -= rgt * d.x * s;
                t += up  * d.y * s;
                cam->SetTarget(t);
            }
            if (io.MouseWheel != 0.0f)
                cam->SetDistance(cam->GetDistance() - io.MouseWheel * cam->GetZoomSpeed());
        }
    }

    if (avail.x > 0.0f && avail.y > 0.0f && m_Scene.GetTarget() &&
        m_Scene.GetTarget()->GetColorAttachmentCount() > 0) {
        ImTextureID texID = (ImTextureID)(uintptr_t)
            m_Scene.GetTarget()->GetColorAttachment(0)->GetRendererID();
        ImGui::Image(texID, avail, ImVec2(0, 1), ImVec2(1, 0));

        // Drag-drop target: drop a model asset on the viewport to create a
        // new entity with a MeshComponent.
        if (ImGui::BeginDragDropTarget()) {
            if (auto* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string path((const char*)pl->Data);  // to null terminator
                CreateEntityFromModel(path);
            }
            ImGui::EndDragDropTarget();
        }

        // Mouse pick: click the viewport to select the nearest mesh entity.
        if (m_ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
            ImVec2 imMin = ImGui::GetItemRectMin();
            ImVec2 imMax = ImGui::GetItemRectMax();
            ImVec2 mp = ImGui::GetMousePos();
            if (mp.x >= imMin.x && mp.x <= imMax.x && mp.y >= imMin.y && mp.y <= imMax.y) {
                const float x = (mp.x - imMin.x) / (imMax.x - imMin.x);
                const float y = (mp.y - imMin.y) / (imMax.y - imMin.y);
                const float ndcX = x * 2.0f - 1.0f;
                const float ndcY = 1.0f - y * 2.0f;
                if (auto* cam = m_Scene.GetCamera()) {
                    glm::mat4 invVP = glm::inverse(cam->GetCamera().GetViewProjection());
                    glm::vec4 np = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                    glm::vec4 fp = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
                    np /= np.w; fp /= fp.w;
                    glm::vec3 ro(np), rd = glm::normalize(glm::vec3(fp) - ro);
                    Scene* sc = m_Scene.GetScene();
                    float bestT = FLT_MAX;
                    Entity best = NullEntity;
                    auto& reg = sc->GetRegistry();
                    reg.view<MeshComponent>().each([&](auto eh, MeshComponent&) {
                        Entity e = static_cast<Entity>(eh);
                        if (!sc->HasComponent<TransformComponent>(e)) return;
                        auto& mc = sc->GetComponent<MeshComponent>(e);
                        auto& tc = sc->GetComponent<TransformComponent>(e);
                        if (!mc.Mesh) return;
                        AABBd lb;
                        if (!MeshLocalBounds(*mc.Mesh, lb)) return;
                        glm::vec3 corners[8] = {
                            {lb.min.x,lb.min.y,lb.min.z}, {lb.max.x,lb.min.y,lb.min.z},
                            {lb.min.x,lb.max.y,lb.min.z}, {lb.max.x,lb.max.y,lb.min.z},
                            {lb.min.x,lb.min.y,lb.max.z}, {lb.max.x,lb.min.y,lb.max.z},
                            {lb.min.x,lb.max.y,lb.max.z}, {lb.max.x,lb.max.y,lb.max.z}
                        };
                        glm::vec3 wmn(FLT_MAX), wmx(-FLT_MAX);
                        for (const auto& c : corners) {
                            glm::vec3 w = glm::vec3(tc.WorldMatrix * glm::vec4(c, 1.0f));
                            wmn = glm::min(wmn, w); wmx = glm::max(wmx, w);
                        }
                        float t;
                        if (RayAABB(ro, rd, wmn, wmx, t) && t < bestT) { bestT = t; best = e; }
                    });
                    m_Scene.SetSelected(best);
                }
            }
        }

        // Gizmo editing only in edit mode: writing transforms during play
        // would desync the play copy from what gets discarded on Stop anyway,
        // and the inspector is read-only then - keep them consistent.
        if (!m_Scene.IsPlaying() && m_Scene.GetSelected() != NullEntity && m_GizmoType >= 0) {

            Scene* sc = m_Scene.GetScene();
            const Entity sel = m_Scene.GetSelected();
            if (sc && sc->HasComponent<TransformComponent>(sel)) {
                // Draw into the Viewport window's own draw list. ImGuizmo's
                // IsHoveringWindow() resolves the owning window from
                // gContext.mDrawList->_OwnerName to set mbMouseOver (hit-testing);
                // ForegroundDrawList has no owning window, so mbMouseOver was always
                // false and IsOver()/IsUsing() silently failed. Drawing here (after
                // ImGui::Image) also keeps the gizmo above the viewport image.
                ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                ImGuizmo::SetOrthographic(false);
                ImVec2 rmin = ImGui::GetItemRectMin();
                ImVec2 rmax = ImGui::GetItemRectMax();
                ImGuizmo::SetRect(rmin.x, rmin.y, rmax.x - rmin.x, rmax.y - rmin.y);
                auto* cam = m_Scene.GetCamera();
                glm::mat4 view = cam->GetCamera().GetView();
                glm::mat4 proj = cam->GetCamera().GetProjection();
                auto& tc = sc->GetComponent<TransformComponent>(sel);
                glm::mat4 matrix = tc.WorldMatrix;
                ImGuizmo::OPERATION op = (m_GizmoType == 0) ? ImGuizmo::TRANSLATE
                                         : (m_GizmoType == 1) ? ImGuizmo::ROTATE
                                                               : ImGuizmo::SCALE;
                ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                     op, ImGuizmo::LOCAL, glm::value_ptr(matrix));
                float t[3], rotDeg[3], sca[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), t, rotDeg, sca);
                tc.Translation = glm::vec3(t[0], t[1], t[2]);
                tc.RotationEuler = glm::radians(glm::vec3(rotDeg[0], rotDeg[1], rotDeg[2]));
                tc.Scale = glm::vec3(sca[0], sca[1], sca[2]);
                tc.Dirty = true;
                sc->MarkSubtreeDirty(sel);
                MarkActiveDirty();
            }
        }
    }
    ImGui::End();
}

// ── Hierarchy ───────────────────────────────────────────────────

void EditorLayer::DrawHierarchy() {
    ImGui::Begin("Scene Hierarchy");
    Scene* s = m_Scene.GetScene();
    if (s) {
        auto& reg = s->GetRegistry();
        reg.view<TagComponent>().each([&](auto eHandle, auto&) {
            Entity e = static_cast<Entity>(eHandle);
            bool isRoot = true;
            if (s->HasComponent<TransformComponent>(e))
                isRoot = s->GetComponent<TransformComponent>(e).Parent == NullEntity;
            if (isRoot)
                DrawEntityNode(e);
        });
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()
            && !ImGui::IsAnyItemHovered())
            m_Scene.SetSelected(NullEntity);
    }
    // Window-wide drop target: dropping a model asset into the hierarchy
    // creates a new root entity with a MeshComponent.
    {
        ImVec2 wpos = ImGui::GetWindowPos();
        ImVec2 rmin = ImGui::GetWindowContentRegionMin();
        ImVec2 rmax = ImGui::GetWindowContentRegionMax();
        ImRect bb(ImVec2(wpos.x + rmin.x, wpos.y + rmin.y),
                  ImVec2(wpos.x + rmax.x, wpos.y + rmax.y));
        if (ImGui::BeginDragDropTargetCustom(bb, ImGui::GetID("HierarchyDropTarget"))) {
            if (auto* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string path((const char*)pl->Data);
                CreateEntityFromModel(path);
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::End();
}

void EditorLayer::DrawEntityNode(Entity e) {
    Scene* s = m_Scene.GetScene();
    if (!s || !s->HasComponent<TagComponent>(e))
        return;
    auto& tag = s->GetComponent<TagComponent>(e).Tag;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    bool hasChildren = false;
    if (s->HasComponent<TransformComponent>(e))
        hasChildren = s->GetComponent<TransformComponent>(e).FirstChild != NullEntity;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (e == m_Scene.GetSelected())
        flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>((uintptr_t)e),
        flags, "%s", tag.c_str());
    if (ImGui::IsItemClicked())
        m_Scene.SetSelected(e);
    // Right-click context menu: prefab save / instantiate (stage 3).
    if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
        m_ContextEntity = e;
        const bool editOk = !m_Scene.IsPlaying();  // edits blocked during play
        if (ImGui::MenuItem("Save As Prefab...", nullptr, false, editOk)) {
            std::string def = std::string("prefabs/") + SanitizeFilename(tag) + ".prefab";
            std::snprintf(m_PathBuf, sizeof(m_PathBuf), "%s", def.c_str());
            m_Dialog = Dialog::SavePrefab;
            m_DialogOpenPending = true;
        }
        if (ImGui::MenuItem("Instantiate Prefab...", nullptr, false, editOk)) {
            m_PathBuf[0] = '\0';
            m_Dialog = Dialog::InstantiatePrefab;
            m_DialogOpenPending = true;
        }
        ImGui::EndPopup();
    }
    if (open) {
        if (s->HasComponent<TransformComponent>(e)) {
            Entity child = s->GetComponent<TransformComponent>(e).FirstChild;
            while (child != NullEntity) {
                DrawEntityNode(child);
                if (s->HasComponent<TransformComponent>(child))
                    child = s->GetComponent<TransformComponent>(child).NextSibling;
                else
                    break;
            }
        }
        ImGui::TreePop();
    }
}

// ── Inspector ───────────────────────────────────────────────────

void EditorLayer::DrawInspector() {
    ImGui::Begin("Inspector");
    Scene* s = m_Scene.GetScene();
    const Entity sel = m_Scene.GetSelected();
    if (sel != NullEntity && s &&
        s->GetRegistry().valid(static_cast<entt::entity>(sel))) {
        DrawComponents(sel);
    } else {
        ImGui::TextDisabled("No entity selected");
    }
    ImGui::End();
}

void EditorLayer::DrawComponents(Entity e) {
    Scene* s = m_Scene.GetScene();

    // Play-mode isolation: the entity under inspection belongs to the play
    // copy, which is discarded on Stop - make everything read-only.
    const bool readOnly = m_Scene.IsPlaying();
    if (readOnly) {
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.3f, 1.0f),
            "Play mode - read-only (changes are discarded on Stop)");
        ImGui::Separator();
        ImGui::BeginDisabled(true);
    }

    if (s->HasComponent<TagComponent>(e)) {
        auto& tag = s->GetComponent<TagComponent>(e).Tag;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", tag.c_str());
        if (ImGui::InputText("Tag", buf, 256)) {
            tag = buf;
            MarkActiveDirty();
        }
    }
    if (s->HasComponent<IDComponent>(e)) {
        auto& id = s->GetComponent<IDComponent>(e);
        ImGui::Text("UUID: %llu", static_cast<unsigned long long>(id.UUID));
    }
    if (s->HasComponent<TransformComponent>(e)) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& tc = s->GetComponent<TransformComponent>(e);
            bool changed = false;
            changed |= ImGui::DragFloat3("Translation", glm::value_ptr(tc.Translation), 0.1f);
            glm::vec3 rotDeg = glm::degrees(tc.RotationEuler);
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotDeg), 0.5f)) {
                tc.RotationEuler = glm::radians(rotDeg);
                changed = true;
            }
            changed |= ImGui::DragFloat3("Scale", glm::value_ptr(tc.Scale), 0.1f, 0.01f, 100.0f);
            if (changed) {
                tc.Dirty = true;
                s->MarkSubtreeDirty(e);
                MarkActiveDirty();
            }
        }
    }
    if (s->HasComponent<CameraComponent>(e)) {
        if (ImGui::CollapsingHeader("Camera")) {
            auto& cc = s->GetComponent<CameraComponent>(e);
            ImGui::Checkbox("Primary", &cc.Primary);
            ImGui::Checkbox("Fixed Aspect Ratio", &cc.FixedAspectRatio);
            auto& cam = cc.Camera;
            const char* items[] = {"Perspective", "Orthographic"};
            int idx = static_cast<int>(cam.GetProjectionType());
            if (ImGui::Combo("Projection", &idx, items, 2))
                cam.SetProjectionType(static_cast<SceneCamera::ProjectionType>(idx));
            if (cam.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
                float fov = cam.GetPerspectiveVerticalFOV();
                if (ImGui::DragFloat("Vertical FOV", &fov, 0.5f, 1.0f, 179.0f))
                    cam.SetPerspectiveVerticalFOV(fov);
            }
        }
    }
    if (s->HasComponent<LightComponent>(e)) {
        if (ImGui::CollapsingHeader("Light")) {
            auto& lc = s->GetComponent<LightComponent>(e);
            const char* types[] = {"Directional", "Point", "Spot"};
            int idx = static_cast<int>(lc.LightType);
            if (ImGui::Combo("Type", &idx, types, 3))
                lc.LightType = static_cast<LightComponent::Type>(idx);
            ImGui::ColorEdit3("Color", glm::value_ptr(lc.Color));
            ImGui::DragFloat("Intensity", &lc.Intensity, 0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Ambient", &lc.AmbientIntensity, 0.01f, 0.0f, 5.0f);
        }
    }
    if (s->HasComponent<MeshComponent>(e)) {
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& mc = s->GetComponent<MeshComponent>(e);
            ImGui::Text("Mesh: %s", mc.Mesh ? "loaded" : "(null)");
            if (mc.Mesh)
                ImGui::Text("SubMeshes: %llu",
                    static_cast<unsigned long long>(mc.Mesh->SubMeshes.size()));
            ImGui::Text("Material overrides: %llu",
                static_cast<unsigned long long>(mc.MaterialOverrides.size()));
            ImGui::Button("Drop [mesh] asset here", ImVec2(-1, 0));
            if (ImGui::BeginDragDropTarget()) {
                if (auto* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    std::string path((const char*)pl->Data);  // to null terminator
                    if (IsModelExtension(std::filesystem::path(path).extension().string())) {
                        auto mesh = AssetManager::Get().Load<Mesh>(path);
                        if (mesh) {
                            mc.Mesh = mesh;
                            mc.MeshAsset = AssetHandle(AssetManager::Get().GetUUID(path));
                            AABBd lb;
                            if (MeshLocalBounds(*mesh, lb)) {
                                // fbx models vary wildly in scale (e.g. this one is 200 units).
                                // Normalize so max extent ~= 2 units -> visible at default camera.
                                float maxExt = std::max({lb.max.x - lb.min.x, lb.max.y - lb.min.y, lb.max.z - lb.min.z});
                                if (maxExt > 0.0001f && s->HasComponent<TransformComponent>(e)) {
                                    float sc = 2.0f / maxExt;
                                    auto& tc = s->GetComponent<TransformComponent>(e);
                                    tc.Scale = {sc, sc, sc};
                                    tc.Dirty = true;
                                    s->MarkSubtreeDirty(e);
                                }
                            }
                            AssignDefaultMaterial(mc);
                            MarkActiveDirty();
                            DMGE_CLIENT_INFO("Loaded mesh into entity: {0}", path);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }

#ifdef DMGE_ANIMATION
    if (s->HasComponent<AnimatorComponent>(e)) {
        if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& ac = s->GetComponent<AnimatorComponent>(e);

            // ── Skeleton / clip asset display ───────────────────
            // Asset references are UUID handles; readable names come from
            // the AssetManager registry (path metadata). There is no
            // asset-picker UI yet (Asset Browser carries paths, not UUIDs),
            // so references are authored in .scene files and shown
            // read-only here.
            auto displayName = [](AssetHandle h) {
                if (!h.IsValid()) return std::string("(none)");
                if (const auto* meta = AssetManager::Get().GetMetadata(h.GetUUID());
                    meta && !meta->Path.empty())
                    return std::filesystem::path(meta->Path).filename().string();
                return std::string("UUID ") + std::to_string(h.GetUUID());
            };
            ImGui::Text("Skeleton: %s", displayName(ac.SkeletonAsset).c_str());
            ImGui::TextDisabled("  UUID: %llu (assign via .scene; no asset picker yet)",
                static_cast<unsigned long long>(ac.SkeletonAsset.GetUUID()));

            // ── Clip selection: combo over the component's own Clips list ──
            // Rationale: the AssetManager public API has no registry
            // enumeration, so "clips associated with this skeleton" cannot
            // be listed from the consumer side; the component's Clips
            // vector IS the authoritative list, so switch among those.
            if (ac.Clips.empty()) {
                ImGui::TextDisabled("No clips (add by UUID below)");
            } else {
                int idx = ac.ActiveClip;
                if (idx < 0 || idx >= static_cast<int>(ac.Clips.size())) idx = 0;
                std::string preview = displayName(ac.Clips[static_cast<size_t>(idx)]);
                if (ImGui::BeginCombo("Active Clip", preview.c_str())) {
                    for (int i = 0; i < static_cast<int>(ac.Clips.size()); ++i) {
                        std::string label = displayName(ac.Clips[static_cast<size_t>(i)]);
                        if (ImGui::Selectable(label.c_str(), i == ac.ActiveClip)) {
                            ac.ActiveClip = i;
                            ac.CurrentTime = 0.0f;   // restart on clip switch
                            MarkActiveDirty();
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            // Manual clip UUID input: appends to the Clips list (the only
            // way to author a reference from the UI until an asset picker
            // exists). UUIDs are listed by the Log when assets register /
            // can be read from the .scene JSON.
            static uint64_t s_ClipUuidInput = 0;   // scratch; one inspector at a time
            ImGui::PushID(static_cast<int>(e));
            ImGui::InputScalar("##ClipUUID", ImGuiDataType_U64, &s_ClipUuidInput,
                               nullptr, nullptr, "%llu");
            ImGui::SameLine();
            if (ImGui::Button("Add Clip") && s_ClipUuidInput != 0) {
                const uint64_t uuid = s_ClipUuidInput;
                ac.Clips.emplace_back(uuid);
                ac.ActiveClip = static_cast<int>(ac.Clips.size()) - 1;
                ac.CurrentTime = 0.0f;
                s_ClipUuidInput = 0;
                MarkActiveDirty();
                DMGE_CLIENT_INFO("Animator: clip UUID {0} added", uuid);
            }
            ImGui::PopID();

            // ── Playback controls ────────────────────────────────
            // Edit mode ticks scenes with dt=0 (EditorScene::OnUpdate), so
            // Playing only takes effect in play mode; here it configures
            // what the play copy will do. CurrentTime is scrubbable when
            // not auto-playing: AnimationSystem still samples at dt=0, so
            // scrubbing previews the exact pose in the editor viewport.
            if (ImGui::Button(ac.Playing ? "Pause" : "Play")) {
                ac.Playing = !ac.Playing;
                MarkActiveDirty();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Loop", &ac.Loop))
                MarkActiveDirty();
            if (ImGui::DragFloat("Speed", &ac.PlaybackSpeed, 0.05f, 0.0f, 10.0f, "%.2f"))
                MarkActiveDirty();
            if (ac.Playing) {
                ImGui::Text("Time: %.2f ticks (auto)", ac.CurrentTime);
            } else {
                if (ImGui::DragFloat("Time (ticks)", &ac.CurrentTime, 0.05f, 0.0f, FLT_MAX, "%.2f"))
                    MarkActiveDirty();
            }
            ImGui::Text("Joints in palette: %d", static_cast<int>(ac.Palette.size()));
        }
    }
#endif

    // Add / Remove Component (runtime attach/detach).
    ImGui::Separator();
    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponentPopup");
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!s->HasComponent<CameraComponent>(e) && ImGui::MenuItem("Camera")) {
            s->AddComponent<CameraComponent>(e);
            MarkActiveDirty();
        }
        if (!s->HasComponent<LightComponent>(e) && ImGui::MenuItem("Light")) {
            s->AddComponent<LightComponent>(e);
            MarkActiveDirty();
        }
        if (!s->HasComponent<MeshComponent>(e) && ImGui::MenuItem("Mesh")) {
            s->AddComponent<MeshComponent>(e);
            MarkActiveDirty();
        }
#ifdef DMGE_ANIMATION
        if (!s->HasComponent<AnimatorComponent>(e) && ImGui::MenuItem("Animator")) {
            s->AddComponent<AnimatorComponent>(e);
            MarkActiveDirty();
        }
#endif
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove..."))
        ImGui::OpenPopup("RemoveComponentPopup");
    if (ImGui::BeginPopup("RemoveComponentPopup")) {
        if (s->HasComponent<CameraComponent>(e) && ImGui::MenuItem("Camera")) {
            s->RemoveComponent<CameraComponent>(e);
            MarkActiveDirty();
        }
        if (s->HasComponent<LightComponent>(e) && ImGui::MenuItem("Light")) {
            s->RemoveComponent<LightComponent>(e);
            MarkActiveDirty();
        }
        if (s->HasComponent<MeshComponent>(e) && ImGui::MenuItem("Mesh")) {
            s->RemoveComponent<MeshComponent>(e);
            MarkActiveDirty();
        }
#ifdef DMGE_ANIMATION
        if (s->HasComponent<AnimatorComponent>(e) && ImGui::MenuItem("Animator")) {
            s->RemoveComponent<AnimatorComponent>(e);
            MarkActiveDirty();
        }
#endif
        ImGui::EndPopup();
    }

    if (readOnly)
        ImGui::EndDisabled();
}

// ── Systems panel ───────────────────────────────────────────────

void EditorLayer::DrawSystems() {
    ImGui::Begin("Systems");
    Scene* s = m_Scene.GetScene();
    if (s) {
        const auto& systems = s->GetSystems();
        ImGui::Text("Registered systems: %llu",
            static_cast<unsigned long long>(systems.size()));
        ImGui::Separator();
        int i = 0;
        for (const auto& sys : systems) {
            ImGui::Text("%d. %s", i++, sys ? sys->GetName() : "(null)");
        }
        ImGui::Separator();
        auto* playingTab = m_Scene.GetTab(m_Scene.FindPlayingTab());
        if (m_Scene.IsPlaying()) {
            ImGui::Text("Play mode ('%s'): %s",
                m_Scene.GetActiveTab() ? m_Scene.GetActiveTab()->Name.c_str() : "?",
                m_Scene.IsPaused() ? "PAUSED" : "PLAYING");
            if (ImGui::Button(m_Scene.IsPaused() ? "Resume##sys" : "Pause##sys"))
                m_Scene.SetPaused(!m_Scene.IsPaused());
            ImGui::SameLine();
            if (ImGui::Button("Stop##sys"))
                EndPlay();
        } else if (playingTab) {
            ImGui::Text("Play mode ('%s'): PLAYING (background)",
                        playingTab->Name.c_str());
            ImGui::TextDisabled("Switch to that tab to control it.");
        } else if (ImGui::Button("Play##sys")) {
            BeginPlay();
        }
    }
    ImGui::End();
}

// ── Asset Browser ───────────────────────────────────────────────

void EditorLayer::DrawAssetBrowser() {
    ImGui::Begin("Asset Browser");
    namespace fs = std::filesystem;
    std::error_code ec;

    auto drawDir = [&](const char* root, const char* title) {
        if (!fs::exists(root, ec)) return;
        ImGui::Separator();
        ImGui::TextDisabled("%s", title);
        ImGui::Separator();
        // recursive: lists files in subdirectories too (e.g. assets/models/*.fbx).
        for (auto& it : fs::recursive_directory_iterator(root, ec)) {
            if (!it.is_regular_file()) continue;
            auto path = it.path().string();
            auto rel = fs::relative(it.path(), root, ec).string();
            auto ext = it.path().extension().string();
            const char* icon = "[file]";
            if (ext == ".mesh" || ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") icon = "[mesh]";
            else if (ext == ".glsl") icon = "[shader]";
            else if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds") icon = "[tex]";
            else if (ext == ".mat") icon = "[mat]";
            else if (ext == ".prefab") icon = "[prefab]";
            else if (ext == ".scene") icon = "[scene]";
            std::string label = std::string(icon) + "  " + rel;
            ImGui::PushID(path.c_str());
            // full-width row; clear, not cramped.
            if (ImGui::Selectable(label.c_str(), m_SelectedAsset == path))
                m_SelectedAsset = path;
            // Drag source: carries the asset path to drop targets (MeshComponent
            // slot, Viewport, Hierarchy). Model files create mesh entities.
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_PATH", path.c_str(), path.size() + 1);
                ImGui::Text("%s %s", icon, rel.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
    };

    drawDir("D:/CPPPractices/DMGameEngine/editor/assets", "Assets (editor/assets)");
    drawDir("D:/CPPPractices/DMGameEngine/engine/shaders", "Shaders (engine/shaders)");
    drawDir("prefabs", "Prefabs (prefabs/)");

    ImGui::Separator();
    if (!m_SelectedAsset.empty())
        ImGui::TextWrapped("Selected: %s", m_SelectedAsset.c_str());
    else
        ImGui::TextDisabled("Drag a [mesh] onto the Viewport or Hierarchy to create an entity.");
    ImGui::End();
}

// ── Model drop -> new entity ────────────────────────────────────

bool EditorLayer::CreateEntityFromModel(const std::string& path) {
    // Play-mode isolation: entity creation is an edit; block it.
    if (m_Scene.IsPlaying()) {
        DMGE_CLIENT_WARN("Cannot create entities during play mode (changes would be discarded on Stop).");
        return false;
    }
    Scene* s = m_Scene.GetScene();
    if (!s) return false;
    if (!IsModelExtension(std::filesystem::path(path).extension().string())) {
        DMGE_CLIENT_WARN("Not a model file: {0}", path);
        return false;
    }
    auto mesh = AssetManager::Get().Load<Mesh>(path);
    if (!mesh) {
        DMGE_CLIENT_WARN("Failed to load model: {0}", path);
        return false;
    }
    std::string name = std::filesystem::path(path).stem().string();
    Entity e = s->CreateEntity(name);
    auto& mc = s->AddComponent<MeshComponent>(e);
    mc.Mesh = mesh;
    // Record the registry UUID (Load<Mesh> by path registers the file) so the
    // entity round-trips through scene save / prefab save.
    mc.MeshAsset = AssetHandle(AssetManager::Get().GetUUID(path));
    AssignDefaultMaterial(mc);
    AABBd lb;
    if (MeshLocalBounds(*mesh, lb)) {
        float maxExt = std::max({lb.max.x - lb.min.x, lb.max.y - lb.min.y, lb.max.z - lb.min.z});
        if (maxExt > 0.0001f && s->HasComponent<TransformComponent>(e)) {
            float sc = 2.0f / maxExt;
            auto& tc = s->GetComponent<TransformComponent>(e);
            tc.Scale = {sc, sc, sc};
            tc.Dirty = true;
            s->MarkSubtreeDirty(e);
        }
    }
    m_Scene.SetSelected(e);
    MarkActiveDirty();
    DMGE_CLIENT_INFO("Created entity '{0}' from model {1}", name, path);
    return true;
}

// ── Prefab (stage 3) ────────────────────────────────────────────

void EditorLayer::SavePrefab(Entity e, const std::string& path) {
    Scene* s = m_Scene.GetEditScene();   // prefabs always capture the edit state
    if (!s || e == NullEntity || path.empty()) return;
    // Temp Scene carrying the single entity tree through SceneSerializer:
    // no engine changes needed (editor-side single-entity serialization).
    Scene tmp;
    EditorSceneCopy::CopyEntityTree(*s, e, tmp);
    std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());
    SceneSerializer::Save(tmp, path);
    DMGE_CLIENT_INFO("Prefab saved: {0}", path);
}

Entity EditorLayer::InstantiatePrefab(const std::string& path) {
    if (m_Scene.IsPlaying()) {
        DMGE_CLIENT_WARN("Cannot instantiate prefabs during play mode.");
        return NullEntity;
    }
    Scene* s = m_Scene.GetScene();
    if (!s) return NullEntity;
    Scene tmp;
    if (!SceneSerializer::Load(tmp, path)) {
        DMGE_CLIENT_WARN("Failed to load prefab: {0}", path);
        return NullEntity;
    }
    // Copy every root of the prefab scene into the current edit scene.
    Entity firstNew = NullEntity;
    tmp.GetRegistry().view<TransformComponent>().each([&](auto eh, TransformComponent& tc) {
        if (tc.Parent != NullEntity) return;   // children come with their root
        Entity root = static_cast<Entity>(eh);
        Entity ne = EditorSceneCopy::CopyEntityTree(tmp, root, *s);
        if (firstNew == NullEntity)
            firstNew = ne;
    });
    if (firstNew != NullEntity) {
        m_Scene.SetSelected(firstNew);
        MarkActiveDirty();
        DMGE_CLIENT_INFO("Prefab instantiated: {0}", path);
    }
    return firstNew;
}
