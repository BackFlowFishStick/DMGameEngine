/*
 * DMGameEngine - DirectX Backend Integration Hook
 *
 * Factories that bridge the D3D11 backend into the engine's abstraction
 * layer. The renderer resource factories live in files owned by the render
 * agent, so their API::DirectX branches (RendererAPI::Create and the
 * Shader/Texture/... switches) call into this file. Tests and tooling
 * construct the backend classes directly or through these factories.
 *
 * Exposed via DMGameEngine.h under the DMGE_D3D11 gate (R9).
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

namespace DMGameEngine {

class RendererAPI;       // forward declaration - no renderer headers leak here
class GraphicsContext;

namespace DirectX {

// Returns a freshly created, uninitialized D3D11RendererAPI. Call Init()
// (headless-capable: hardware adapter with WARP fallback) before use.
DMGE_API DM::Scope<RendererAPI> CreateDirectXRendererAPI();

// Returns a GraphicsContext bound to a native Win32 window handle (HWND,
// e.g. glfwGetWin32Window(...)). Call Init() after Window creation and
// BEFORE Renderer::Init() so the renderer adopts the window's device and
// swapchain. SwapBuffers() presents; RequestResize() recreates the buffers.
DMGE_API DM::Scope<GraphicsContext> CreateDirectXGraphicsContext(void* hwnd);

} // namespace DirectX
} // namespace DMGameEngine
