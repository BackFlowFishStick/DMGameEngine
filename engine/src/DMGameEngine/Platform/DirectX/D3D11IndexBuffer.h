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
