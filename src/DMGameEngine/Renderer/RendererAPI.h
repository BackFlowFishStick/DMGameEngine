/*
 * DMGameEngine - Renderer API Abstraction
 *
 * Low-level backend interface for graphics-API-specific rendering
 * commands (draw calls, clear, viewport, ...). Platform backends
 * (OpenGL, Vulkan, DirectX) derive from this and provide their own
 * implementations.
 *
 * The active backend is created via the static Create() factory, which
 * selects the correct implementation based on Renderer::GetAPI().
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include <memory>

namespace DMGameEngine {

class VertexArray; // forward declaration

class DMGE_API RendererAPI
{
public:
    virtual ~RendererAPI() = default;

    virtual void DrawIndexed(const VertexArray& vertexArray) = 0;

    // ── Factory ─────────────────────────────────────────────────
    static std::unique_ptr<RendererAPI> Create();
};

} // namespace DMGameEngine