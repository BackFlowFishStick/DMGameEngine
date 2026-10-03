#pragma once
#include <DMGameEngine/DMGameEngine.h>
#include <string>
#include <vector>

/*
 * SceneTab - one open scene in the editor (stage 3 multi-scene tabs).
 *
 * Each tab is fully independent: its own edit scene, play copy, camera and
 * selection. Play-mode isolation (stage 4) is per tab: EnterPlayMode()
 * deep-copies the tab's edit scene into PlayScene (via SceneDuplicator,
 * ref-sharing mesh/material resources) and ticks the copy; ExitPlayMode()
 * discards it. See EditorScene for the cross-tab play policy.
 */
struct SceneTab {
    std::string Name;   // display name: file stem or "Untitled-N"
    std::string Path;   // empty = never saved
    bool        Dirty = false;  // unsaved edits since last save

    DM::Ref<DMGameEngine::Scene> EditScene;   // authoritative editing state
    DM::Ref<DMGameEngine::Scene> PlayScene;   // runtime copy while playing
    DM::Ref<DMGameEngine::Scene> Active;      // = PlayScene while playing, else EditScene

    DM::Ref<DMGameEngine::EditorCameraController> Camera;
    DMGameEngine::Entity Selected = DMGameEngine::NullEntity;   // per-tab selection

    bool Playing = false;
    bool Paused  = false;

    // Play-mode selection preservation: the selection (by UUID) is remembered
    // here when Play starts so it can be re-resolved on the edit scene after
    // Stop. Per tab because tabs can be switched while another one plays.
    uint64_t SelectedUUIDBeforePlay = 0;
};

/*
 * EditorScene - owns ALL open scene tabs + the shared offscreen target.
 *
 * Multi-scene policy (stage 3):
 *  - Every tab has its own play state, but AT MOST ONE tab may be in play
 *    mode at any time (a "runtime" is global, like launching the game).
 *    EnterPlayMode() refuses if another tab is already playing.
 *  - While a background tab is playing, its simulation KEEPS RUNNING
 *    (ticked every frame, not rendered). Rationale: pausing an invisible
 *    simulation silently would surprise the user when switching back
 *    (time-dependent motion freezes/resumes unpredictably), and an
 *    unrendered ECS tick is cheap. Switching tabs never touches play state.
 *  - Only the ACTIVE tab is rendered (into the single shared FrameBuffer)
 *    and only its camera receives input.
 */
class EditorScene {
public:
    EditorScene();   // opens the first tab with the default demo scene
    ~EditorScene() = default;

    void OnUpdate(DMGameEngine::Timestep ts);
    void Render();
    void Resize(uint32_t w, uint32_t h);

    // ── Tab management ──────────────────────────────────────────
    int  AddUntitledTab();                         // empty scene, "Untitled-N"; returns index
    int  AddTabFromFile(const std::string& path);  // new tab from .scene; -1 on failure
    int  FindTabByPath(const std::string& path) const;
    void CloseTab(int index);   // discards play copy on that tab; keeps >= 1 tab open
    int  GetTabCount() const { return static_cast<int>(m_Tabs.size()); }
    SceneTab* GetTab(int i)   { return (i >= 0 && i < static_cast<int>(m_Tabs.size())) ? &m_Tabs[i] : nullptr; }
    const SceneTab* GetTab(int i) const { return (i >= 0 && i < static_cast<int>(m_Tabs.size())) ? &m_Tabs[i] : nullptr; }

    int  GetActive() const { return m_Active; }
    void SetActive(int i);
    SceneTab* GetActiveTab() { return GetTab(m_Active); }
    const SceneTab* GetActiveTab() const { return GetTab(m_Active); }

    int FindPlayingTab() const;   // -1 if none (at most one exists)

    // ── Active-tab conveniences (null-safe) ─────────────────────
    DMGameEngine::Scene* GetEditScene() const;   // active tab's edit scene
    DMGameEngine::Scene* GetScene() const;       // active tab's active scene (play copy while playing)
    DMGameEngine::FrameBuffer* GetTarget() const { return m_FB.get(); }
    DMGameEngine::EditorCameraController* GetCamera() const;
    DMGameEngine::Entity GetSelected() const;    // active tab's selection
    void SetSelected(DMGameEngine::Entity e);

    // ── Play mode (operates on the ACTIVE tab) ──────────────────
    void EnterPlayMode();   // refuses if any tab is already playing
    void ExitPlayMode();    // discards the active tab's play copy
    bool IsPlaying() const; // active tab
    bool IsPaused()  const;
    void SetPaused(bool p);

private:
    void RegisterSystems(DMGameEngine::Scene& s);
    void ApplyRenderState();                      // clear color / depth / cull (global GL state)
    void SetupDefaultSceneContents(DMGameEngine::Scene& s);   // demo cube + light
    void ResizeTabCamera(SceneTab& tab, uint32_t w, uint32_t h);

    std::vector<SceneTab> m_Tabs;
    int m_Active = -1;
    int m_UntitledCounter = 0;
    DM::Ref<DMGameEngine::FrameBuffer> m_FB;      // one offscreen target, active tab renders into it
};
