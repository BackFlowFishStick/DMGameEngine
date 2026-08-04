#include "EditorApplication.h"
#include <DMGameEngine/Core/Log.h>

EditorApplication::EditorApplication()
    : Application(DMGameEngine::WindowProps("DMGameEngine - Editor", 1600, 900)) {}

void EditorApplication::OnInitialize() {
    DMGE_CLIENT_INFO("Editor initialized");
    GetWindow().SetVSync(true);
    PushLayer(DM::CreateScope<EditorLayer>());
}