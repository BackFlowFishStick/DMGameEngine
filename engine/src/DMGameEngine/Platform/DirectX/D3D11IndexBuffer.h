/*
 * DMGameEngine - Direct3D 11 Index Buffer
 *
 * D3D11 implementation of the IndexBuffer abstraction: an ID3D11Buffer
 * with INDEX_BUFFER bind flag, 32-bit indices (R32_UINT), matching the
 * engine's DrawIndexed(GL_UNSIGNED_INT) contract.
 */

#pragma once

#include "DMGameEngine/Renderer/IndexBuffer.h"
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

class DMGE_API D3D11IndexBuffer : public IndexBuffer
{
public:
    D3D11IndexBuffer(const uint32_t* indices, uint32_t count);
    ~D3D11IndexBuffer() override;

    void Bind()   const override; // no-op (VA wires the input assembler)
    void Unbind() const override; // no-op

    uint32_t GetCount() const override { return m_Count; }

    ID3D11Buffer* GetBuffer() const { return m_Buffer.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_Buffer;
    uint32_t m_Count = 0;
};

} // namespace DMGameEngine

#ifdef _MSC_VER
#pragma warning(pop)
#endif
