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
#include "DMGameEngine/Core/GamepadCodes.h"
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
#include "DMGameEngine/Core/Events/GamepadEvent.h"
#include "DMGameEngine/Core/Events/ApplicationEvent.h"

// ── Renderer ─────────────────────────────────────────────────────
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/Renderer/OrthographicCamera.h"
#include "DMGameEngine/Renderer/PerspectiveCamera.h"
#include "DMGameEngine/Renderer/CameraController.h"
#include "DMGameEngine/Renderer/GraphicsContext.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Renderer/RenderCommand.h"
#include "DMGameEngine/Renderer/RenderQueue.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/Texture.h"
#include "DMGameEngine/Renderer/Texture2D.h"
#include "DMGameEngine/Renderer/TextureCube.h"
#include "DMGameEngine/Renderer/Texture2DArray.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/IndexBuffer.h"
#include "DMGameEngine/Renderer/FrameBuffer.h"
#include "DMGameEngine/Renderer/Light.h"

// ── Scene ────────────────────────────────────────────────────────
#include "DMGameEngine/Scene/SceneCamera.h"
#include "DMGameEngine/Scene/OrthographicCameraController.h"
#include "DMGameEngine/Scene/EditorCameraController.h"
#include "DMGameEngine/Scene/DefaultSceneLayer.h"
#include "DMGameEngine/Scene/Entity.h"
#include "DMGameEngine/Scene/Components/Components.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/SceneSerializer.h"
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Systems/TransformSystem.h"
#include "DMGameEngine/Scene/Systems/MeshRenderSystem.h"
#include "DMGameEngine/Scene/Systems/LightSystem.h"
#ifdef DMGE_ANIMATION
// Skeletal animation subsystem (stage 1). Built when DMGE_ANIMATION=ON; see
// engine/CMakeLists.txt. AnimationSystem must be registered BEFORE
// MeshRenderSystem so palettes are fresh for SkinnedMeshRenderSystem.
#include "DMGameEngine/Animation/Skeleton.h"
#include "DMGameEngine/Animation/AnimationClip.h"
#include "DMGameEngine/Animation/AnimationMath.h"
#include "DMGameEngine/Scene/Systems/AnimationSystem.h"
#include "DMGameEngine/Scene/Systems/SkinnedMeshRenderSystem.h"
#endif

// ── Asset ───────────────────────────────────────────────────────
#include "DMGameEngine/Asset/AssetTypes.h"
#include "DMGameEngine/Asset/AssetHandle.h"
#include "DMGameEngine/Asset/AssetLoader.h"
#include "DMGameEngine/Asset/Mesh.h"
#include "DMGameEngine/Asset/AssetManager.h"

// ── ImGui ────────────────────────────────────────────────────────
#include "DMGameEngine/ImGui/ImGuiLayer.h"

// ── Debug ────────────────────────────────────────────────────────
#include "DMGameEngine/Debug/Profiler.h"
#include "DMGameEngine/Debug/ProfilerLayer.h"
#include "DMGameEngine/Debug/Console.h"
#include "DMGameEngine/Debug/ConsoleLayer.h"
