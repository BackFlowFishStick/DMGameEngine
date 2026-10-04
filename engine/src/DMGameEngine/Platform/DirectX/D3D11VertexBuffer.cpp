/*
 * DMGameEngine - Direct3D 11 Vertex Buffer (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11VertexBuffer.h"

#include "DMGameEngine/Core/Log.h"

#include <cstring>

namespace DMGameEngine {

D3D11VertexBuffer::D3D11VertexBuffer(uint32_t size)
    : m_Size(size), m_Dynamic(true)
{
    ID3D11Device* device = D3D11Backend::Device();
    DMGE_CORE_ASSERT(device, "D3D11VertexBuffer created before D3D11RendererAPI::Init (no device)!");

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth      = size;
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&desc, nullptr, &m_Buffer);
    DMGE_D3D_CHECK(hr, "CreateBuffer (dynamic VB)");
}

D3D11VertexBuffer::D3D11VertexBuffer(const void* vertices, uint32_t size)
    : m_Size(size), m_Dynamic(false)
{
    ID3D11Device* device = D3D11Backend::Device();
    DMGE_CORE_ASSERT(device, "D3D11VertexBuffer created before D3D11RendererAPI::Init (no device)!");

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth      = size;
    desc.Usage          = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = vertices;

    HRESULT hr = device->CreateBuffer(&desc, &init, &m_Buffer);
    DMGE_D3D_CHECK(hr, "CreateBuffer (static VB)");
}

D3D11VertexBuffer::~D3D11VertexBuffer() = default;

void D3D11VertexBuffer::Bind() const   { /* input assembler wiring happens in VertexArray::Bind */ }
void D3D11VertexBuffer::Unbind() const { }

void D3D11VertexBuffer::SetData(const void* data, uint32_t size)
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context || !m_Buffer)
        return;

    if (size > m_Size)
    {
        DMGE_LOG_WARN("[D3D11] VB SetData size {0} exceeds buffer size {1} (clamped)", size, m_Size);
        size = m_Size;
    }

    if (m_Dynamic)
    {
        // DISCARD map: rewrites the whole buffer without a GPU stall.
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        DMGE_D3D_CHECK(hr, "Map (VB)");
        if (SUCCEEDED(hr))
        {
            std::memcpy(mapped.pData, data, size);
            context->Unmap(m_Buffer.Get(), 0);
        }
    }
    else
    {
        context->UpdateSubresource(m_Buffer.Get(), 0, nullptr, data, 0, 0);
    }
}

} // namespace DMGameEngine
