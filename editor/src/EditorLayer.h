#pragma once
#include <DMGameEngine/DMGameEngine.h>
#include "EditorScene.h"
#include "LogPanel.h"
#include <string>

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
    void DrawViewport();
    void DrawHierarchy();
    void DrawEntityNode(DMGameEngine::Entity e);
    void DrawInspector();
    void DrawComponents(DMGameEngine::Entity e);
    void DrawSystems();
    void DrawAssetBrowser();

    EditorScene m_Scene;
    DMGameEngine::Entity m_Selected = DMGameEngine::NullEntity;
    glm::vec2 m_ViewportSize{0.0f, 0.0f};
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;
    std::string m_ScenePath = "scene.scene";
    std::string m_SelectedAsset;
    LogPanel m_Log;
    int m_GizmoType = 0; // -1 off, 0 translate, 1 rotate, 2 scale
};