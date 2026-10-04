/*
 * DMGameEngine - Direct3D 11 Vertex Array
 *
 * D3D11 implementation of the VertexArray abstraction. D3D11 has no native
 * VAO: Bind() wires the Input Assembler (vertex buffers, index buffer,
 * primitive topology) and selects an ID3D11InputLayout. Input layouts are
 * D3D11's shader-signature-bound replacement for GL vertex attrib pointers:
 * they must match the *currently bound vertex shader's* input signature, so
 * they are built lazily in Bind() from the bound D3D11Shader's VS blob
 * (D3D11Shader::CurrentBound) and cached per (vertex array, shader) pair.
 *
 * Semantic mapping (element name -> HLSL input semantic, case-insensitive):
 *   contains "position" -> POSITION   "normal" -> NORMAL
 *   contains "tangent"  -> TANGENT    "color"  -> COLOR
 *   everything else     -> TEXCOORD<n> (running index across all buffers)
 * The bundled HLSL shaders declare matching semantics (see
 * Platform/DirectX/Shaders/BlinnPhong.hlsl).
 */

#pragma once

#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <unordered_map>
#include <vector>

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

class DMGE_API D3D11VertexArray : public VertexArray
{
public:
    D3D11VertexArray();
    ~D3D11VertexArray() override;

    void Bind()   const override;
    void Unbind() const override;

    void AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer) override;
    void SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer) override;

    const std::vector<DM::Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
    const DM::Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }

private:
    // Builds D3D11_INPUT_ELEMENT_DESC entries for one buffer's layout,
    // extending the running TEXCOORD semantic index across buffers.
    void AppendInputElements(uint32_t slot, const BufferLayout& layout,
                             std::vector<D3D11_INPUT_ELEMENT_DESC>& out) const;

    std::vector<DM::Ref<VertexBuffer>> m_VertexBuffers;
    DM::Ref<IndexBuffer> m_IndexBuffer;

    // Input layout cache: key = shader raw pointer + layout signature hash.
    // Rebuilt if the same (array, shader) pair's layout changes (not
    // supported after AddVertexBuffer, same as the OpenGL backend).
    mutable std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID3D11InputLayout>> m_InputLayouts;
    mutable std::vector<D3D11_INPUT_ELEMENT_DESC> m_InputElements;
    mutable std::vector<DXGI_FORMAT> m_ElementFormats;
    mutable bool m_ElementsDirty = true;
};

} // namespace DMGameEngine

#ifdef _MSC_VER
#pragma warning(pop)
#endif
