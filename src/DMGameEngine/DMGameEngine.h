/*
 * DMGameEngine - Public API Entry Header
 *
 * Game projects should include this single header to access
 * the full engine API:
 *
 *   #include <DMGameEngine/DMGameEngine.h>
 */

#pragma once

// ── Core ─────────────────────────────────────────────────────────
#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/KeyCodes.h"
#include "DMGameEngine/Core/MouseCodes.h"
#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Core/Layer.h"
#include "DMGameEngine/Core/LayerStack.h"
#include "DMGameEngine/Core/Application.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Core/Input.h"
#include "DMGameEngine/Core/Window.h"

// ── Events ───────────────────────────────────────────────────────
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/Events/MouseEvent.h"
#include "DMGameEngine/Core/Events/ApplicationEvent.h"

// ── Renderer ─────────────────────────────────────────────────────
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/Renderer/OrthographicCamera.h"
#include "DMGameEngine/Renderer/PerspectiveCamera.h"
#include "DMGameEngine/Renderer/GraphicsContext.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/Texture.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/IndexBuffer.h"

// ── Scene ────────────────────────────────────────────────────────
#include "DMGameEngine/Scene/SceneCamera.h"

// ── ImGui ────────────────────────────────────────────────────────
#include "DMGameEngine/ImGui/ImGuiLayer.h"
