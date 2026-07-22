/*
 * DMGameEngine - Render Queue Implementation
 *
 * Deferred submission: renderables are accumulated by Submit() and
 * committed once by Flush(), which sorts by material/shader so each
 * group binds its state only once and uploads the view-projection once
 * per shader instead of once per draw.
 */

#include "DMGameEngine/Renderer/RenderQueue.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/RenderCommand.h"
#include "DMGameEngine/Core/Log.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace DMGameEngine {

void RenderQueue::Clear()
{
    m_Queue.clear();
}

void RenderQueue::Submit(const DM::Ref<Material>& material,
                         const DM::Ref<VertexArray>& vertexArray,
                         const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(material, "RenderQueue::Submit - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "RenderQueue::Submit - vertexArray is null!");
    m_Queue.push_back({ material, nullptr, vertexArray, transform });
}

void RenderQueue::Submit(const DM::Ref<Shader>& shader,
                         const DM::Ref<VertexArray>& vertexArray,
                         const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(shader, "RenderQueue::Submit - shader is null!");
    DMGE_CORE_ASSERT(vertexArray, "RenderQueue::Submit - vertexArray is null!");
    m_Queue.push_back({ nullptr, shader, vertexArray, transform });
}

void RenderQueue::Flush(const glm::mat4& viewProjection)
{
    // Sort renderables so those sharing the same Material/Shader become
    // contiguous. Materials sort before shader-only draws (tag 0 vs 1),
    // then by object identity to keep each group together.
    auto sortKey = [](const Renderable& r) {
        const bool hasMaterial = static_cast<bool>(r.Material);
        const void* obj = hasMaterial
            ? static_cast<const void*>(r.Material.get())
            : static_cast<const void*>(r.Shader.get());
        return std::pair<uint32_t, uintptr_t>{
            hasMaterial ? 0u : 1u,
            reinterpret_cast<uintptr_t>(obj)
        };
    };

    std::sort(m_Queue.begin(), m_Queue.end(),
        [&](const Renderable& a, const Renderable& b) {
            return sortKey(a) < sortKey(b);
        });

    const Material* lastMaterial = nullptr;
    const Shader*   lastShader   = nullptr;

    for (const auto& r : m_Queue)
    {
        const DM::Ref<Shader>& shader = r.Material
            ? r.Material->GetShader()
            : r.Shader;

        DMGE_CORE_ASSERT(shader, "RenderQueue::Flush - renderable has no shader!");
        DMGE_CORE_ASSERT(r.VertexArray, "RenderQueue::Flush - vertexArray is null!");

        if (r.Material)
        {
            // New material group: rebind shader + uniforms, then upload
            // the per-frame view-projection once for this shader.
            if (r.Material.get() != lastMaterial)
            {
                r.Material->Bind();
                shader->SetMat4("u_ViewProjection", viewProjection);
                lastMaterial = r.Material.get();
                lastShader   = shader.get();
            }
        }
        else
        {
            if (shader.get() != lastShader)
            {
                shader->Bind();
                shader->SetMat4("u_ViewProjection", viewProjection);
                lastShader = shader.get();
            }
        }

        shader->SetMat4("u_Transform", r.Transform);
        RenderCommand::DrawIndexed(*r.VertexArray);
    }

    m_Queue.clear();
}

} // namespace DMGameEngine