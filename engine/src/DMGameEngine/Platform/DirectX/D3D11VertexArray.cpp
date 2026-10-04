/*
 * DMGameEngine - Direct3D 11 Vertex Array (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11VertexArray.h"
#include "DMGameEngine/Platform/DirectX/D3D11Shader.h"
#include "DMGameEngine/Platform/DirectX/D3D11VertexBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11IndexBuffer.h"

#include "DMGameEngine/Core/Log.h"

#include <cstring>

namespace DMGameEngine {

namespace {

// ── element-name -> HLSL semantic mapping (see header) ───────────

bool NameContains(const std::string& name, const char* keyword)
{
    const size_t keywordLen = std::strlen(keyword);
    const size_t nameLen = name.size();
    if (nameLen < keywordLen)
        return false;
    for (size_t i = 0; i + keywordLen <= nameLen; ++i)
    {
        size_t j = 0;
        while (j < keywordLen &&
               std::tolower(static_cast<unsigned char>(name[i + j])) ==
               std::tolower(static_cast<unsigned char>(keyword[j])))
            ++j;
        if (j == keywordLen)
            return true;
    }
    return false;
}

DXGI_FORMAT ElementFormat(ShaderDataType type, uint32_t components)
{
    switch (type)
    {
        case ShaderDataType::Float: case ShaderDataType::Float2:
        case ShaderDataType::Float3: case ShaderDataType::Float4:
            switch (components)
            {
                case 1: return DXGI_FORMAT_R32_FLOAT;
                case 2: return DXGI_FORMAT_R32G32_FLOAT;
                case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
                case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            break;
        case ShaderDataType::Int: case ShaderDataType::Int2:
        case ShaderDataType::Int3: case ShaderDataType::Int4:
            switch (components)
            {
                case 1: return DXGI_FORMAT_R32_SINT;
                case 2: return DXGI_FORMAT_R32G32_SINT;
                case 3: return DXGI_FORMAT_R32G32B32_SINT;
                case 4: return DXGI_FORMAT_R32G32B32A32_SINT;
            }
            break;
        case ShaderDataType::Bool:
            return DXGI_FORMAT_R8_UINT;
        default:
            break;
    }
    DMGE_CORE_ASSERT(false, "Unsupported ShaderDataType for D3D11 input element!");
    return DXGI_FORMAT_UNKNOWN;
}

// FNV-1a over the input element descriptions (incl. semantic strings) for
// the per-shader input layout cache key.
uint32_t HashElements(const std::vector<D3D11_INPUT_ELEMENT_DESC>& elements)
{
    uint32_t hash = 2166136261u;
    auto mix = [&hash](const void* data, size_t size)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 16777619u;
        }
    };
    for (const auto& e : elements)
    {
        mix(e.SemanticName, std::strlen(e.SemanticName));
        mix(&e.SemanticIndex, sizeof(uint32_t));
        mix(&e.Format, sizeof(DXGI_FORMAT));
        mix(&e.InputSlot, sizeof(uint32_t));
        mix(&e.AlignedByteOffset, sizeof(uint32_t));
        mix(&e.InputSlotClass, sizeof(D3D11_INPUT_CLASSIFICATION));
        mix(&e.InstanceDataStepRate, sizeof(uint32_t));
    }
    return hash;
}

} // anonymous namespace


// ── Constructor / Destructor ─────────────────────────────────────

D3D11VertexArray::D3D11VertexArray() = default;

D3D11VertexArray::~D3D11VertexArray() = default;

// ── Vertex / Index Buffer attachment ─────────────────────────────

void D3D11VertexArray::AddVertexBuffer(const DM::Ref<VertexBuffer>& vertexBuffer)
{
    DMGE_CORE_ASSERT(!vertexBuffer->GetLayout().GetElements().empty(),
                     "Vertex buffer has no layout!");
    m_VertexBuffers.push_back(vertexBuffer);
    m_ElementsDirty = true;
}

void D3D11VertexArray::SetIndexBuffer(const DM::Ref<IndexBuffer>& indexBuffer)
{
    m_IndexBuffer = indexBuffer;
}

// ── Input element construction ───────────────────────────────────

void D3D11VertexArray::AppendInputElements(uint32_t slot, const BufferLayout& layout,
                                           std::vector<D3D11_INPUT_ELEMENT_DESC>& out) const
{
    // Running TEXCOORD semantic index across all buffers (POSITION/NORMAL/
    // COLOR/TANGENT are fixed at index 0 - one attribute of each per shader).
    uint32_t texcoordIndex = 0;
    for (const auto& existing : out)
    {
        if (std::strcmp(existing.SemanticName, "TEXCOORD") == 0 &&
            existing.SemanticIndex >= texcoordIndex)
            texcoordIndex = existing.SemanticIndex + 1;
    }

    for (const auto& element : layout)
    {
        const char* semantic = "TEXCOORD";
        uint32_t semanticIndex = texcoordIndex;

        if (NameContains(element.Name, "position"))      { semantic = "POSITION"; semanticIndex = 0; }
        else if (NameContains(element.Name, "normal"))   { semantic = "NORMAL";   semanticIndex = 0; }
        else if (NameContains(element.Name, "tangent"))  { semantic = "TANGENT";  semanticIndex = 0; }
        else if (NameContains(element.Name, "color"))    { semantic = "COLOR";    semanticIndex = 0; }
        else                                             { ++texcoordIndex; }        // TEXCOORD<n>

        const bool perInstance = element.PerInstance;
        const bool isMatrix = (element.Type == ShaderDataType::Mat3 ||
                               element.Type == ShaderDataType::Mat4);
        // Matrices expand into consecutive 4-component rows (mirrors the
        // OpenGL backend's glVertexAttribPointer expansion); they are
        // per-instance attributes there, so keep the same rate here.
        const uint8_t rows = (element.Type == ShaderDataType::Mat4) ? 4
                           : (element.Type == ShaderDataType::Mat3) ? 3 : 1;

        for (uint8_t r = 0; r < rows; ++r)
        {
            D3D11_INPUT_ELEMENT_DESC desc{};
            desc.SemanticName         = semantic;
            desc.SemanticIndex        = semanticIndex;
            desc.Format               = ElementFormat(element.Type, rows > 1 ? 4 : element.GetComponentCount());
            desc.InputSlot            = slot;
            desc.AlignedByteOffset    = element.Offset + 16u * r;
            desc.InputSlotClass       = (perInstance || isMatrix)
                                            ? D3D11_INPUT_PER_INSTANCE_DATA
                                            : D3D11_INPUT_PER_VERTEX_DATA;
            desc.InstanceDataStepRate = (perInstance || isMatrix) ? 1 : 0;
            out.push_back(desc);

            if (rows > 1 || (!perInstance && !isMatrix))
                ++semanticIndex; // matrix rows and TEXCOORD attributes use consecutive indices
        }
    }
}

// ── Bind / Unbind (Input Assembler wiring) ───────────────────────

void D3D11VertexArray::Bind() const
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context)
        return;

    if (m_ElementsDirty)
    {
        m_InputElements.clear();
        for (uint32_t slot = 0; slot < m_VertexBuffers.size(); ++slot)
            AppendInputElements(slot, m_VertexBuffers[slot]->GetLayout(), m_InputElements);
        m_ElementsDirty = false;
    }

    // ── Vertex buffers + strides ─────────────────────────────────
    std::vector<ID3D11Buffer*> rawBuffers;
    std::vector<UINT> strides;
    std::vector<UINT> offsets;
    rawBuffers.reserve(m_VertexBuffers.size());
    strides.reserve(m_VertexBuffers.size());
    offsets.reserve(m_VertexBuffers.size());
    for (const auto& vb : m_VertexBuffers)
    {
        rawBuffers.push_back(static_cast<D3D11VertexBuffer*>(vb.get())->GetBuffer());
        strides.push_back(vb->GetLayout().GetStride());
        offsets.push_back(0);
    }
    context->IASetVertexBuffers(0, static_cast<UINT>(rawBuffers.size()),
                                rawBuffers.data(), strides.data(), offsets.data());

    // ── Index buffer + primitive topology ────────────────────────
    if (m_IndexBuffer)
        context->IASetIndexBuffer(static_cast<D3D11IndexBuffer*>(m_IndexBuffer.get())->GetBuffer(),
                                  DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ── Input layout (shader-signature dependent, cached) ────────
    D3D11Shader* shader = D3D11Shader::CurrentBound();
    if (!shader)
    {
        DMGE_LOG_WARN("[D3D11] VertexArray::Bind with no bound D3D11Shader - "
                      "input layout cannot be selected (D3D11 layouts match the VS signature)");
        return;
    }
    if (m_InputElements.empty())
    {
        DMGE_LOG_WARN("[D3D11] VertexArray::Bind with no input elements");
        return;
    }

    ID3DBlob* vsBlob = shader->GetVertexBlob();
    if (!vsBlob)
        return;

    const uint64_t cacheKey =
        (static_cast<uint64_t>(HashElements(m_InputElements)) << 32) ^
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(shader));

    auto it = m_InputLayouts.find(cacheKey);
    if (it == m_InputLayouts.end())
    {
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        HRESULT hr = D3D11Backend::Device()->CreateInputLayout(
            m_InputElements.data(), static_cast<UINT>(m_InputElements.size()),
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout);
        DMGE_D3D_CHECK(hr, "CreateInputLayout");
        it = m_InputLayouts.emplace(cacheKey, std::move(layout)).first;
    }

    context->IASetInputLayout(it->second.Get());
}

void D3D11VertexArray::Unbind() const { /* nothing to unbind on D3D11 */ }

} // namespace DMGameEngine
