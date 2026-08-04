#include <DMGameEngine/DMGameEngine.h>
#include <DMGameEngine/Core/EntryPoint.h>
#include "EditorApplication.h"

DMGameEngine::Application* DMGameEngine::CreateApplication() {
    DMGameEngine::Renderer::SetAPI(DMGameEngine::Renderer::API::OpenGL);
    return new EditorApplication();
}