/*
 * DMGameEngine - Vertex Array Abstraction
 *
 * Base class for all graphics API vertex array object (VAO)
 * implementations. A VertexArray bundles one or more VertexBuffers
 * (whose BufferLayouts define the vertex attributes) together with an
 * optional IndexBuffer, so a single Bind() call wires up all vertex
 * inputs for a draw.
 *
 * Vertex arrays are created via the static Create() factory, which
 * selects the correct backend based on the active Renderer::API.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/IndexBuffer.h"
#include <memory>
#include <vector>

namespace DMGameEngine {

class DMGE_API VertexArray
{
public:
    virtual ~VertexArray() = default;

    virtual void Bind()   const = 0;
    virtual void Unbind() const = 0;

    virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) = 0;
    virtual void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)   = 0;

    virtual const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const = 0;
    virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const = 0;

    // ── Factory ─────────────────────────────────────────────────
    static std::shared_ptr<VertexArray> Create();
};

} // namespace DMGameEngine