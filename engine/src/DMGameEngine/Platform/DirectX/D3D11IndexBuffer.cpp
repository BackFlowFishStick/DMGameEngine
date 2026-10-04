/*
 * DMGameEngine - Direct3D 11 Index Buffer (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11IndexBuffer.h"

#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

D3D11IndexBuffer::D3D11IndexBuffer(const uint32_t* indices, uint32_t count)
    : m_Count(count)
{
    ID3D11Device* device = D3D11Backend::Device();
    DMGE_CORE_ASSERT(device, "D3D11IndexBuffer created before D3D11RendererAPI::Init (no device)!");

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth      = count * sizeof(uint32_t);
    desc.Usage          = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = indices;

    HRESULT hr = device->CreateBuffer(&desc, &init, &m_Buffer);
    DMGE_D3D_CHECK(hr, "CreateBuffer (IB)");
}

D3D11IndexBuffer::~D3D11IndexBuffer() = default;

void D3D11IndexBuffer::Bind()   const { /* IASetIndexBuffer happens in VertexArray::Bind */ }
void D3D11IndexBuffer::Unbind() const { }

} // namespace DMGameEngine
