/*
 * DMGameEngine - Direct3D 11 Shader (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11Shader.h"

#include "DMGameEngine/Core/Log.h"

#include <d3dcompiler.h>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <fstream>
#include <sstream>

namespace DMGameEngine {

D3D11Shader* D3D11Shader::s_Bound = nullptr;

namespace {

constexpr uint32_t kStageVS = 0;
constexpr uint32_t kStagePS = 1;

// ── `#type vertex` / `#type fragment` block splitting (filepath ctor) ──
// Mirrors the OpenGL backend's PreProcess convention (K-023: combined-stage
// sources only load through a filepath). Returns false when the source does
// not carry the expected blocks.
bool SplitTypedSources(std::string_view source,
                       std::string* vertexOut,
                       std::string* fragmentOut)
{
    constexpr std::string_view kToken = "#type";
    size_t pos = source.find(kToken, 0);
    if (pos == std::string::npos)
        return false;

    while (pos != std::string::npos)
    {
        const size_t eol = source.find_first_of("\r\n", pos);
        if (eol == std::string::npos)
            return false;

        const size_t begin = pos + kToken.size() + 1; // skip "#type "
        std::string_view type = source.substr(begin, eol - begin);

        const size_t nextLinePos = source.find_first_not_of("\r\n", eol);
        if (nextLinePos == std::string::npos)
            return false;

        pos = source.find(kToken, nextLinePos);
        const std::string block(pos == std::string::npos
                                    ? std::string(source.substr(nextLinePos))
                                    : std::string(source.substr(nextLinePos, pos - nextLinePos)));

        if (type == "vertex")
            *vertexOut = block;
        else if (type == "fragment" || type == "pixel")
            *fragmentOut = block;
        // unknown block types are ignored (GL backend behavior)
    }
    return !vertexOut->empty() && !fragmentOut->empty();
}

// ── Indexed array names: "u_PointLights_position[3]" ─────────────
// Splits into base name + index. Returns false for non-indexed names.
// baseOut may be null when only the index is wanted.
bool SplitIndex(std::string_view name, std::string* baseOut, uint32_t* indexOut)
{
    const size_t open = name.rfind('[');
    if (open == std::string::npos || name.empty() || name.back() != ']')
        return false;

    std::string_view indexStr = name.substr(open + 1, name.size() - open - 2);
    if (indexStr.empty())
        return false;

    uint32_t index = 0;
    for (const char c : indexStr)
    {
        if (c < '0' || c > '9')
            return false;
        index = index * 10u + static_cast<uint32_t>(c - '0');
    }
    *indexOut = index;
    if (baseOut)
        *baseOut = std::string(name.substr(0, open));
    return true;
}

} // anonymous namespace


// ── Construction / destruction ───────────────────────────────────

D3D11Shader::D3D11Shader(std::string_view filepath)
{
    std::ifstream in(std::string(filepath), std::ios::in | std::ios::binary);
    DMGE_CORE_ASSERT(in, "Could not open shader file: {0}", filepath);
    if (!in)
        return;

    std::ostringstream content;
    content << in.rdbuf();

    std::string vertexSrc, fragmentSrc;
    const auto lastSlash = std::string(filepath).find_last_of("/\\");
    m_Name = (lastSlash == std::string::npos)
                 ? std::string(filepath)
                 : std::string(filepath).substr(lastSlash + 1);

    if (!SplitTypedSources(content.str(), &vertexSrc, &fragmentSrc))
    {
        DMGE_LOG_ERROR("[D3D11] Shader file '{0}' has no #type vertex/#type fragment blocks",
                       filepath);
        return;
    }

    ID3D11Device* device = D3D11Backend::Device();
    DMGE_CORE_ASSERT(device, "D3D11Shader created before D3D11RendererAPI::Init (no device)!");

    CompileStage(&m_VSBlob, "VSMain", "vs_5_0", vertexSrc);
    CompileStage(&m_PSBlob, "PSMain", "ps_5_0", fragmentSrc);
    if (!m_VSBlob || !m_PSBlob)
        return;

    HRESULT hr = device->CreateVertexShader(m_VSBlob->GetBufferPointer(),
                                            m_VSBlob->GetBufferSize(), nullptr, &m_VS);
    DMGE_D3D_CHECK(hr, "CreateVertexShader");

    hr = device->CreatePixelShader(m_PSBlob->GetBufferPointer(),
                                   m_PSBlob->GetBufferSize(), nullptr, &m_PS);
    DMGE_D3D_CHECK(hr, "CreatePixelShader");

    ReflectStage(m_VSBlob.Get(), kStageVS);
    ReflectStage(m_PSBlob.Get(), kStagePS);

    // Shared sampler (identical to the two-source constructor; see it for
    // the description and the stage-B per-texture-state TODO).
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MipLODBias     = 0.0f;
    samplerDesc.MaxAnisotropy  = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD         = 0.0f;
    samplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&samplerDesc, &m_Sampler);
    DMGE_D3D_CHECK(hr, "CreateSamplerState");
}

D3D11Shader::D3D11Shader(std::string_view name,
                         std::string_view vertexSrc,
                         std::string_view fragmentSrc)
    : m_Name(name)
{
    ID3D11Device* device = D3D11Backend::Device();
    DMGE_CORE_ASSERT(device, "D3D11Shader created before D3D11RendererAPI::Init (no device)!");

    CompileStage(&m_VSBlob, "VSMain", "vs_5_0", vertexSrc);
    CompileStage(&m_PSBlob, "PSMain", "ps_5_0", fragmentSrc);

    if (!m_VSBlob || !m_PSBlob)
        return;

    HRESULT hr = device->CreateVertexShader(m_VSBlob->GetBufferPointer(),
                                            m_VSBlob->GetBufferSize(), nullptr, &m_VS);
    DMGE_D3D_CHECK(hr, "CreateVertexShader");

    hr = device->CreatePixelShader(m_PSBlob->GetBufferPointer(),
                                   m_PSBlob->GetBufferSize(), nullptr, &m_PS);
    DMGE_D3D_CHECK(hr, "CreatePixelShader");

    ReflectStage(m_VSBlob.Get(), kStageVS);
    ReflectStage(m_PSBlob.Get(), kStagePS);

    // Shared sampler for all texture bind points (LINEAR filter, WRAP -
    // matches the engine default Texture2DSpecification; per-texture
    // filter/wrap state is a stage B item).
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MipLODBias     = 0.0f;
    samplerDesc.MaxAnisotropy  = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD         = 0.0f;
    samplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&samplerDesc, &m_Sampler);
    DMGE_D3D_CHECK(hr, "CreateSamplerState");
}

D3D11Shader::~D3D11Shader()
{
    if (s_Bound == this)
        s_Bound = nullptr;
}

void D3D11Shader::CompileStage(ID3DBlob** blob, const char* entryPoint, const char* target,
                               std::string_view source)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DMGE_ENABLE_ASSERTS
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr,
                            entryPoint, target, flags, 0, blob, &errorBlob);

    if (errorBlob)
    {
        const char* msg = static_cast<const char*>(errorBlob->GetBufferPointer());
        DMGE_LOG_ERROR("[D3D11] {0} ({1}) compile output:\n{2}", m_Name, target, msg);
    }

    DMGE_D3D_CHECK(hr, "D3DCompile");
    if (FAILED(hr) && *blob)
    {
        (*blob)->Release();
        *blob = nullptr;
    }
}

void D3D11Shader::ReflectStage(ID3DBlob* blob, uint32_t stage)
{
    ID3D11Device* device = D3D11Backend::Device();

    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
    HRESULT hr = D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(),
                            IID_ID3D11ShaderReflection,
                            reinterpret_cast<void**>(reflector.GetAddressOf()));
    DMGE_D3D_CHECK(hr, "D3DReflect");
    if (FAILED(hr))
        return;

    D3D11_SHADER_DESC shaderDesc{};
    hr = reflector->GetDesc(&shaderDesc);
    DMGE_D3D_CHECK(hr, "GetDesc(ShaderReflection)");
    if (FAILED(hr))
        return;

    StageResources& resources = m_StageResources[stage];

    // ── Constant buffers: cbuffer object + staging + uniform offsets ──
    for (UINT cb = 0; cb < shaderDesc.ConstantBuffers; ++cb)
    {
        ID3D11ShaderReflectionConstantBuffer* cbuffer = reflector->GetConstantBufferByIndex(cb);
        if (!cbuffer)
            continue;

        D3D11_SHADER_BUFFER_DESC bufferDesc{};
        hr = cbuffer->GetDesc(&bufferDesc);
        DMGE_D3D_CHECK(hr, "GetDesc(ConstantBuffer)");
        if (FAILED(hr))
            continue;

        // Resolve the register slot (b#) this cbuffer is bound to.
        D3D11_SHADER_INPUT_BIND_DESC bindDesc{};
        hr = reflector->GetResourceBindingDescByName(bufferDesc.Name, &bindDesc);
        DMGE_D3D_CHECK(hr, "GetResourceBindingDescByName(cbuffer)");
        if (FAILED(hr))
            continue;
        const UINT slot = bindDesc.BindPoint;

        if (resources.CBuffers.size() <= slot)
        {
            resources.CBuffers.resize(slot + 1);
            resources.Staging.resize(slot + 1);
        }

        // GPU-side constant buffer (DEFAULT usage, updated via UpdateSubresource).
        D3D11_BUFFER_DESC gpuDesc{};
        gpuDesc.ByteWidth      = (bufferDesc.Size + 15u) & ~15u; // 16-byte aligned
        gpuDesc.Usage          = D3D11_USAGE_DEFAULT;
        gpuDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        gpuDesc.CPUAccessFlags = 0;

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        hr = device->CreateBuffer(&gpuDesc, nullptr, &buffer);
        DMGE_D3D_CHECK(hr, "CreateBuffer(cbuffer)");
        resources.CBuffers[slot] = std::move(buffer);

        // CPU staging copy, zero-initialized (unset uniforms read as 0).
        resources.Staging[slot].assign(bufferDesc.Size, 0);

        // Record each member as a uniform target.
        for (UINT v = 0; v < bufferDesc.Variables; ++v)
        {
            ID3D11ShaderReflectionVariable* variable = cbuffer->GetVariableByIndex(v);
            if (!variable)
                continue;

            D3D11_SHADER_VARIABLE_DESC varDesc{};
            hr = variable->GetDesc(&varDesc);
            DMGE_D3D_CHECK(hr, "GetDesc(Variable)");
            if (FAILED(hr))
                continue;

            // Array members: record the element stride so per-element writes
            // ("u_PointLights_position[3]") land on the packed offset (HLSL
            // packs float3 array elements at 16-byte steps).
            uint32_t elementStride = 0;
            if (ID3D11ShaderReflectionType* varType = variable->GetType())
            {
                D3D11_SHADER_TYPE_DESC typeDesc{};
                if (SUCCEEDED(varType->GetDesc(&typeDesc)) && typeDesc.Elements > 1)
                    elementStride = varDesc.Size / typeDesc.Elements;
            }

            m_UniformTargets[varDesc.Name].push_back(
                UniformTarget{ stage, static_cast<uint32_t>(slot),
                               static_cast<uint32_t>(varDesc.StartOffset),
                               static_cast<uint32_t>(varDesc.Size),
                               elementStride });
        }
    }

    // ── Texture (SRV) resources: sampler-name -> t# bind point ──
    for (UINT r = 0; r < shaderDesc.BoundResources; ++r)
    {
        D3D11_SHADER_INPUT_BIND_DESC bindDesc{};
        hr = reflector->GetResourceBindingDesc(r, &bindDesc);
        DMGE_D3D_CHECK(hr, "GetResourceBindingDesc");
        if (FAILED(hr))
            continue;

        if (bindDesc.Type == D3D_SIT_TEXTURE)
            m_TextureBindPoints[bindDesc.Name] = bindDesc.BindPoint;
    }
}


// ── Bind / Unbind ────────────────────────────────────────────────

void D3D11Shader::UploadStaging() const
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context)
        return;

    // Upload staging -> GPU constant buffers, then bind per stage.
    for (uint32_t stage = 0; stage < 2; ++stage)
    {
        const StageResources& resources = m_StageResources[stage];
        const UINT slotCount = static_cast<UINT>(resources.CBuffers.size());
        if (slotCount == 0)
            continue;

        std::vector<ID3D11Buffer*> rawBuffers;
        rawBuffers.reserve(slotCount);
        for (UINT slot = 0; slot < slotCount; ++slot)
        {
            ID3D11Buffer* buffer = resources.CBuffers[slot].Get();
            rawBuffers.push_back(buffer);
            if (buffer && !resources.Staging[slot].empty())
            {
                context->UpdateSubresource(buffer, 0, nullptr,
                                           resources.Staging[slot].data(), 0, 0);
            }
        }

        if (stage == kStageVS)
            context->VSSetConstantBuffers(0, slotCount, rawBuffers.data());
        else
            context->PSSetConstantBuffers(0, slotCount, rawBuffers.data());
    }
}

void D3D11Shader::Bind() const
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context)
        return;

    // Upload current staging (uniforms set before Bind) and bind the
    // program state. Uniforms set AFTER Bind are flushed at draw time via
    // UploadBoundStaging(), mirroring the engine's GL semantics.
    UploadStaging();

    context->VSSetShader(m_VS.Get(), nullptr, 0);
    context->PSSetShader(m_PS.Get(), nullptr, 0);

    // Resolve texture SRVs from the GL-style unit table; unregistered units
    // fall back to the 1x1 white dummy (never bind a dangling/null SRV).
    if (!m_TextureBindPoints.empty())
    {
        for (const auto& [texName, bindPoint] : m_TextureBindPoints)
        {
            const auto it = m_TextureSlots.find(bindPoint);
            const uint32_t unit = (it != m_TextureSlots.end()) ? it->second : 0;
            ID3D11ShaderResourceView* srv = D3D11Backend::SRVForUnit(unit);
            if (srv)
                context->PSSetShaderResources(bindPoint, 1, &srv);
        }
    }

    if (m_Sampler)
    {
        ID3D11SamplerState* sampler = m_Sampler.Get();
        context->PSSetSamplers(0, 1, &sampler);
    }

    s_Bound = const_cast<D3D11Shader*>(this);
}

void D3D11Shader::Unbind() const
{
    s_Bound = nullptr;
}

D3D11Shader* D3D11Shader::CurrentBound()
{
    return s_Bound;
}

void D3D11Shader::UploadBoundStaging()
{
    if (s_Bound)
        s_Bound->UploadStaging();
}


// ── Uniform staging ──────────────────────────────────────────────

bool D3D11Shader::WriteUniform(std::string_view name, const void* data, uint32_t size)
{
    // Base name: "u_PointLights_position[3]" -> "u_PointLights_position".
    std::string baseName(name.substr(0, name.find('[')));
    const auto targets = m_UniformTargets.find(baseName);
    if (targets == m_UniformTargets.end() || targets->second.empty())
        return false;

    // Indexed array element ("name[i]"): address the packed element
    // (HLSL packs float3 array elements at 16-byte strides - ElementStride
    // comes from reflection). A plain array name writes the whole array.
    std::string splitBase;
    uint32_t elementIndex = 0;
    const bool indexed = SplitIndex(name, &splitBase, &elementIndex);

    for (const UniformTarget& t : targets->second)
    {
        UniformTarget resolved = t;
        if (t.ElementStride != 0 && indexed)
        {
            const size_t wanted = static_cast<size_t>(elementIndex) * t.ElementStride;
            if (wanted + t.ElementStride > t.Size)
            {
                DMGE_LOG_WARN("[D3D11] Uniform '{0}' index {1} out of range "
                              "(array holds {2} elements)",
                              name, elementIndex, t.Size / t.ElementStride);
                continue;
            }
            resolved.Offset += static_cast<uint32_t>(wanted);
            resolved.Size    = t.ElementStride;
        }

        auto& staging = m_StageResources[resolved.Stage].Staging[resolved.BufferSlot];
        // Member size from reflection is authoritative; clamp for safety.
        const uint32_t bytes = (size < resolved.Size) ? size : resolved.Size;
        if (resolved.Offset + bytes > staging.size())
        {
            DMGE_LOG_WARN("[D3D11] Uniform '{0}' write out of range (offset {1} + {2} > {3})",
                          name, resolved.Offset, bytes, staging.size());
            continue;
        }
        std::memcpy(staging.data() + resolved.Offset, data, bytes);
    }
    return true;
}

void D3D11Shader::WarnOnceMissing(std::string_view name) const
{
    std::string key(name);
    auto it = m_MissingWarned.find(key);
    if (it == m_MissingWarned.end())
    {
        // Tolerant of missing uniforms, mirroring the OpenGL backend's
        // "location -1 is silently ignored" contract - but warn once.
        DMGE_LOG_WARN("[D3D11] Shader '{0}': uniform '{1}' not found (ignored)", m_Name, name);
        m_MissingWarned.emplace(std::move(key), true);
    }
}

bool D3D11Shader::HasUniform(std::string_view name) const
{
    // Plain member names, plus indexed array element names ("name[2]").
    std::string base;
    uint32_t index = 0;
    if (SplitIndex(name, &base, &index))
    {
        const auto it = m_UniformTargets.find(base);
        if (it == m_UniformTargets.end() || it->second.empty())
            return false;
        const UniformTarget& t = it->second.front();
        if (t.ElementStride == 0)
            return false; // member is not an array
        return static_cast<size_t>(index) * t.ElementStride + t.ElementStride <= t.Size;
    }
    return m_UniformTargets.count(std::string(name)) > 0;
}

bool D3D11Shader::HasTexture(std::string_view name) const
{
    return m_TextureBindPoints.count(std::string(name)) > 0;
}

bool D3D11Shader::ReadUniformStaging(std::string_view name, void* dst, uint32_t size) const
{
    if (!dst)
        return false;

    std::string base;
    uint32_t elementIndex = 0;
    const bool indexed = SplitIndex(name, &base, &elementIndex);
    const std::string& lookup = indexed ? base : std::string(name);

    const auto it = m_UniformTargets.find(lookup);
    if (it == m_UniformTargets.end() || it->second.empty())
        return false;

    const UniformTarget& t = it->second.front();
    UniformTarget resolved = t;
    if (indexed)
    {
        if (t.ElementStride == 0)
            return false;
        const size_t wanted = static_cast<size_t>(elementIndex) * t.ElementStride;
        if (wanted + t.ElementStride > t.Size)
            return false;
        resolved.Offset += static_cast<uint32_t>(wanted);
        resolved.Size    = t.ElementStride;
    }

    const auto& staging = m_StageResources[resolved.Stage].Staging[resolved.BufferSlot];
    const uint32_t bytes = (size < resolved.Size) ? size : resolved.Size;
    if (resolved.Offset + bytes > staging.size())
        return false;

    std::memcpy(dst, staging.data() + resolved.Offset, bytes);
    return true;
}


// ── Uniform setters ──────────────────────────────────────────────

void D3D11Shader::SetInt(std::string_view name, int value)
{
    // Sampler binding: engine convention is SetInt("u_AlbedoTexture", unit)
    // followed by texture->Bind(unit). Record the unit; SRVs resolve at
    // Bind() time (see D3D11Common unit table).
    const auto texIt = m_TextureBindPoints.find(std::string(name));
    if (texIt != m_TextureBindPoints.end())
    {
        m_TextureSlots[texIt->second] = static_cast<uint32_t>(value);
        return;
    }

    if (!WriteUniform(name, &value, sizeof(int)))
        WarnOnceMissing(name);
}

void D3D11Shader::SetIntArray(std::string_view name, const int* values, uint32_t count)
{
    if (!WriteUniform(name, values, count * sizeof(int)))
        WarnOnceMissing(name);
}

void D3D11Shader::SetFloat(std::string_view name, float value)
{
    if (!WriteUniform(name, &value, sizeof(float)))
        WarnOnceMissing(name);
}

void D3D11Shader::SetFloat2(std::string_view name, const glm::vec2& value)
{
    if (!WriteUniform(name, glm::value_ptr(value), sizeof(glm::vec2)))
        WarnOnceMissing(name);
}

void D3D11Shader::SetFloat3(std::string_view name, const glm::vec3& value)
{
    if (!WriteUniform(name, glm::value_ptr(value), sizeof(glm::vec3)))
        WarnOnceMissing(name);
}

void D3D11Shader::SetFloat4(std::string_view name, const glm::vec4& value)
{
    if (!WriteUniform(name, glm::value_ptr(value), sizeof(glm::vec4)))
        WarnOnceMissing(name);
}

void D3D11Shader::SetMat4(std::string_view name, const glm::mat4& value)
{
    // glm stores column-major; the HLSL cbuffers declare column_major
    // matrices, so the raw bytes match 1:1 (no transpose).
    if (!WriteUniform(name, glm::value_ptr(value), sizeof(glm::mat4)))
        WarnOnceMissing(name);
}

void D3D11Shader::SetMat4Array(std::string_view name, const glm::mat4* values, uint32_t count)
{
    (void)values; (void)count;
    // TODO(stage C): skeletal skinning (u_BoneMatrices palette). Needs either
    // a dedicated large cbuffer with dynamic offsets or an SSBO-equivalent
    // (structured buffer) path - same follow-up as the Vulkan backend (KB-03).
    // Warn once per (shader, name): RenderQueue calls this per skinned draw.
    std::string key = "mat4array:" + std::string(name);
    if (m_MissingWarned.find(key) == m_MissingWarned.end())
    {
        DMGE_LOG_WARN("[D3D11] Shader '{0}': SetMat4Array('{1}') not implemented yet (stage C)",
                      m_Name, name);
        m_MissingWarned.emplace(std::move(key), true);
    }
}

} // namespace DMGameEngine
