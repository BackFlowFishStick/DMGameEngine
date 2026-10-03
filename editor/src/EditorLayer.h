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
    // Selection lives per tab inside EditorScene (multi-scene, stage 3).
    DMGameEngine::Entity GetSelected() const { return m_Scene.GetSelected(); }
    void SetSelected(DMGameEngine::Entity e) { m_Scene.SetSelected(e); }

private:
    void DrawDockspace();
    void DrawMenuBar();
    void DrawSceneTabs();     // stage 3: one tab per open scene
    void DrawModalDialogs();
    void DrawViewport();
    void DrawHierarchy();
    void DrawEntityNode(DMGameEngine::Entity e);
    void DrawInspector();
    void DrawComponents(DMGameEngine::Entity e);
    void DrawSystems();
    void DrawAssetBrowser();

    // ── Play mode isolation (stage 4, per active tab) ───────────
    // EnterPlay snapshots the active tab's selection (by UUID) into the tab
    // so it can be re-resolved on the edit scene after Stop.
    void BeginPlay();
    void EndPlay();

    // ── Scene management (stage 3, multi-tab) ───────────────────
    void OpenSceneFromPath(const std::string& path);   // opens a NEW tab (or focuses an already-open one)
    void SaveSceneToPath(const std::string& path);     // saves the ACTIVE tab
    void RequestCloseTab(int index);                   // dirty tabs get a confirm dialog
    void MarkActiveDirty();

    // ── Asset drag/drop (stage 3) ───────────────────────────────
    // Creates a new entity with a MeshComponent (default material) from a
    // model file. No-op during play mode (edits blocked).
    bool CreateEntityFromModel(const std::string& path);

    // ── Prefab (stage 3) ────────────────────────────────────────
    void SavePrefab(DMGameEngine::Entity e, const std::string& path);
    DMGameEngine::Entity InstantiatePrefab(const std::string& path);

    // ── Runnable project export (stage 4) ───────────────────────
    void ExportRunnableProject(const std::string& outputDir);

    // ── Recent-files persistence (editor_config.ini) ────────────
    void LoadConfig();
    void SaveConfig();
    void PushRecentScene(const std::string& path);

    EditorScene m_Scene;
    glm::vec2 m_ViewportSize{0.0f, 0.0f};
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;
    std::string m_SelectedAsset;
    LogPanel m_Log;
    int m_GizmoType = 0; // -1 off, 0 translate, 1 rotate, 2 scale

    // Modal dialogs (Open / Save As / Prefab paths / Close confirm / Export).
    enum class Dialog { None, ConfirmCloseTab, OpenScene, SaveSceneAs, SavePrefab, InstantiatePrefab, ExportProject };
    Dialog m_Dialog = Dialog::None;
    bool m_DialogOpenPending = false;
    char m_PathBuf[512] = {};
    char m_ExportDirBuf[512] = {};
    DMGameEngine::Entity m_ContextEntity = DMGameEngine::NullEntity;
    int m_PendingCloseTab = -1;   // tab awaiting close confirmation

    // Scene management state.
    std::vector<std::string> m_RecentScenes;
};
