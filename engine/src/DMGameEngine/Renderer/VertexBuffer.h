/*
 * DMGameEngine - Vertex Buffer Abstraction
 *
 * Base class for all graphics API vertex buffer implementations.
 * Platform backends (OpenGL, Vulkan, DirectX) derive from this
 * and provide their own buffer creation, binding and data upload.
 *
 * Vertex buffers are created via the static Create() factory, which
 * selects the correct backend based on the active Renderer::API.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/Shader.h" // BufferLayout
#include <memory>
#include <cstdint>

namespace DMGameEngine {

// ── Vertex Buffer ────────────────────────────────────────────────────

class DMGE_API VertexBuffer
{
public:
    virtual ~VertexBuffer() = default;

    virtual void Bind()   const = 0;
    virtual void Unbind() const = 0;

    virtual void  SetLayout(const BufferLayout& layout) = 0;
    virtual const BufferLayout& GetLayout() const = 0;

    virtual void SetData(const void* data, uint32_t size) = 0;

    // ── Factory ─────────────────────────────────────────────────
    static DM::Ref<VertexBuffer> Create(uint32_t size);
    static DM::Ref<VertexBuffer> Create(const void* vertices, uint32_t size);
};

} // namespace DMGameEngine
