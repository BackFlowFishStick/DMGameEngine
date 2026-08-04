#pragma once
#include <DMGameEngine/DMGameEngine.h>

class EditorScene {
public:
    EditorScene();
    void OnUpdate(DMGameEngine::Timestep ts);
    void Render();
    void Resize(uint32_t w, uint32_t h);
    void NewScene();
    void SetPlaying(bool p) { m_Playing = p; }
    bool IsPlaying() const { return m_Playing; }
    DMGameEngine::Scene* GetScene() const { return m_Scene.get(); }
    DMGameEngine::FrameBuffer* GetTarget() const { return m_FB.get(); }
    DMGameEngine::EditorCameraController* GetCamera() const { return m_Camera.get(); }
private:
    void SetupDefaultScene();
    DM::Ref<DMGameEngine::Scene> m_Scene;
    DM::Ref<DMGameEngine::EditorCameraController> m_Camera;
    DM::Ref<DMGameEngine::FrameBuffer> m_FB;
    bool m_Playing = false;
};