#pragma once
#include <DMGameEngine/DMGameEngine.h>

/*
 * EditorScene - owns the editor's Scene + camera + offscreen target.
 *
 * Play-mode isolation (stage 4): the edit scene (m_EditScene) is the
 * authoritative editing state. EnterPlayMode() deep-copies it into
 * m_PlayScene (via SceneDuplicator, ref-sharing mesh/material resources) and
 * makes the copy the *active* scene (m_Scene): systems tick on the copy and
 * the viewport renders the copy. Any change made while playing therefore hits
 * only the copy; ExitPlayMode() discards it and restores m_EditScene
 * untouched - mainstream-engine semantics ("changes in play mode are lost").
 */
class EditorScene {
public:
    EditorScene();
    void OnUpdate(DMGameEngine::Timestep ts);
    void Render();
    void Resize(uint32_t w, uint32_t h);

    // ── Scene management (always operate on the edit scene) ─────
    void NewScene();                                        // back to default demo scene
    bool LoadSceneFromFile(const std::string& path);        // fresh edit scene from .scene
    DMGameEngine::Scene* GetEditScene() const { return m_EditScene.get(); }

    // ── Play mode (stage 4 isolation) ───────────────────────────
    void EnterPlayMode();   // snapshot edit scene -> runtime copy, run the copy
    void ExitPlayMode();    // discard runtime copy, back to untouched edit scene
    void SetPaused(bool p) { m_Paused = p; }
    bool IsPlaying() const { return m_Playing; }
    bool IsPaused()  const { return m_Paused; }

    // Active scene: the play copy while playing, else the edit scene.
    DMGameEngine::Scene* GetScene() const { return m_Scene.get(); }
    DMGameEngine::FrameBuffer* GetTarget() const { return m_FB.get(); }
    DMGameEngine::EditorCameraController* GetCamera() const { return m_Camera.get(); }
private:
    void CreateEmptyScene();      // systems + render state, no entities
    void SetupDefaultScene();     // CreateEmptyScene + demo entities
    void RegisterSystems(DMGameEngine::Scene& s);
    DM::Ref<DMGameEngine::Scene> m_EditScene;
    DM::Ref<DMGameEngine::Scene> m_PlayScene;
    DM::Ref<DMGameEngine::Scene> m_Scene;   // active = edit or play copy
    DM::Ref<DMGameEngine::EditorCameraController> m_Camera;
    DM::Ref<DMGameEngine::FrameBuffer> m_FB;
    bool m_Playing = false;
    bool m_Paused  = false;
};
