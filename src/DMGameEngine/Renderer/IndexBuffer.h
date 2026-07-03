/*
 * DMGameEngine - Index Buffer Abstraction
 *
 * Base class for all graphics API index buffer implementations.
 * Platform backends (OpenGL, Vulkan, DirectX) derive from this
 * and provide their own buffer creation, binding and data upload.
 *
 * Index buffers are created via the static Create() factory, which
 * selects the correct backend based on the active Renderer::API.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include <memory>
#include <cstdint>

namespace DMGameEngine {

// ── Index Buffer ─────────────────────────────────────────────────────

class DMGE_API IndexBuffer
{
public:
    virtual ~IndexBuffer() = default;

    virtual void Bind()   const = 0;
    virtual void Unbind() const = 0;

    virtual uint32_t GetCount() const = 0;

    // ── Factory ─────────────────────────────────────────────────
    static std::shared_ptr<IndexBuffer> Create(const uint32_t* indices, uint32_t count);
};

} // namespace DMGameEngine
