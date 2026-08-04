#pragma once
#include <DMGameEngine/DMGameEngine.h>
#include "EditorLayer.h"

class EditorApplication : public DMGameEngine::Application {
public:
    EditorApplication();
    void OnInitialize() override;
};