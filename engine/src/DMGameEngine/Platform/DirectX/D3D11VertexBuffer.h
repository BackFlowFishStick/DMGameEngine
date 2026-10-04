/*
 * DMGameEngine - Direct3D 11 Vertex Buffer
 *
 * D3D11 implementation of the VertexBuffer abstraction. Owns an
 * ID3D11Buffer with VERTEX_BUFFER bind flag. DYNAMIC usage for the
 * size-only constructor (streaming SetData), STATIC for the data
 * constructor - mirroring the OpenGL backend's glBufferData usage.
 *
 * Note: D3D11 has no ARRAY_BUFFER-style global binding; the actual
 * IASetVertexBuffers happens in D3D11VertexArray::Bind(). Bind()/Unbind()
 * are accepted as no-ops for interface parity.
 */

#pragma once

#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

namespace DMGameEngine {

// C4251 ("needs dll-interface for members") is intentionally suppressed for
// the D3D11 backend classes: members are COM pointers (no CRT state),
// header-only template types (ComPtr/glm), or private STL members that are
// only ever touched inside the engine DLL (same CRT by construction) - the
// pattern the OpenGL backend's exported classes already accept (kb/KB-02).
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

class DMGE_API D3D11VertexBuffer : public VertexBuffer
{
public:
    explicit D3D11VertexBuffer(uint32_t size);                            // DYNAMIC
    D3D11VertexBuffer(const void* vertices, uint32_t size);               // STATIC
    ~D3D11VertexBuffer() override;

    void Bind()   const override; // no-op (VA wires the input assembler)
    void Unbind() const override; // no-op

    void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
    const BufferLayout& GetLayout() const override { return m_Layout; }

    void SetData(const void* data, uint32_t size) override;

    ID3D11Buffer* GetBuffer() const { return m_Buffer.Get(); }

private:
    BufferLayout m_Layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_Buffer;
    uint32_t m_Size = 0;
    bool     m_Dynamic = false;
};

} // namespace DMGameEngine

#ifdef _MSC_VER
#pragma warning(pop)
#endif
