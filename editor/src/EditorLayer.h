#pragma once
#include <DMGameEngine/DMGameEngine.h>
#include "EditorScene.h"
#include "LogPanel.h"
#include <string>
#include <vector>
#include <cstdint>

class EditorLayer : public DMGameEngine::Layer {
public:
    EditorLayer();
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(DMGameEngine::Timestep ts) override;
    void OnRender() override;
    void OnEvent(DMGameEngine::Event& e) override;
    void OnImGuiRender() override;

    EditorScene& GetScene() { return m_Scene; }
    DMGameEngine::Entity GetSelected() const { return m_Selected; }
    void SetSelected(DMGameEngine::Entity e) { m_Selected = e; }

private:
    void DrawDockspace();
    void DrawMenuBar();
    void DrawModalDialogs();
    void DrawViewport();
    void DrawHierarchy();
    void DrawEntityNode(DMGameEngine::Entity e);
    void DrawInspector();
    void DrawComponents(DMGameEngine::Entity e);
    void DrawSystems();
    void DrawAssetBrowser();

    // ── Play mode isolation (stage 4) ───────────────────────────
    // EnterPlay snapshots the selection (by UUID) so it can be restored on
    // the edit scene after Stop; EndPlay restores it.
    void BeginPlay();
    void EndPlay();

    // ── Scene management (stage 3) ──────────────────────────────
    void OpenSceneFromPath(const std::string& path);
    void SaveSceneToPath(const std::string& path);

    // ── Asset drag/drop (stage 3) ───────────────────────────────
    // Creates a new entity with a MeshComponent (default material) from a
    // model file. No-op during play mode (edits blocked).
    bool CreateEntityFromModel(const std::string& path);

    // ── Prefab (stage 3) ────────────────────────────────────────
    void SavePrefab(DMGameEngine::Entity e, const std::string& path);
    DMGameEngine::Entity InstantiatePrefab(const std::string& path);

    // ── Recent-files persistence (editor_config.ini) ────────────
    void LoadConfig();
    void SaveConfig();
    void PushRecentScene(const std::string& path);

    EditorScene m_Scene;
    DMGameEngine::Entity m_Selected = DMGameEngine::NullEntity;
    glm::vec2 m_ViewportSize{0.0f, 0.0f};
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;
    std::string m_ScenePath = "scene.scene";
    std::string m_SelectedAsset;
    LogPanel m_Log;
    int m_GizmoType = 0; // -1 off, 0 translate, 1 rotate, 2 scale

    // Play-mode selection preservation.
    uint64_t m_SelectedUUID = 0;

    // Modal dialogs (New Scene confirm / Open / Save As / Prefab paths).
    enum class Dialog { None, ConfirmNewScene, OpenScene, SaveSceneAs, SavePrefab, InstantiatePrefab };
    Dialog m_Dialog = Dialog::None;
    bool m_DialogOpenPending = false;
    char m_PathBuf[512] = {};
    DMGameEngine::Entity m_ContextEntity = DMGameEngine::NullEntity;

    // Scene management state.
    std::vector<std::string> m_RecentScenes;
};
