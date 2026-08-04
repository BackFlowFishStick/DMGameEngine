/*
 * DMGameEngine - Mesh implementation (stage 1c+)
 */
#include "DMGameEngine/Asset/Mesh.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/IndexBuffer.h"

namespace DMGameEngine {

const DM::Ref<VertexArray>& Mesh::GetVertexArray() const
{
    if (m_VertexArray) return m_VertexArray;       // already uploaded
    if (Vertices.empty()) return m_VertexArray;    // nothing to upload -> stays null

    auto va = VertexArray::Create();
    auto vb = VertexBuffer::Create(Vertices.data(),
                                   static_cast<uint32_t>(Vertices.size() * sizeof(float)));
    vb->SetLayout(Layout);
    va->AddVertexBuffer(vb);

    if (!Indices.empty())
    {
        auto ib = IndexBuffer::Create(Indices.data(),
                                      static_cast<uint32_t>(Indices.size()));
        va->SetIndexBuffer(ib);
    }
    m_VertexArray = va;
    return m_VertexArray;
}

} // namespace DMGameEngine