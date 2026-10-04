/*
 * DMGameEngine - Direct3D 11 Shader (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11Shader.h"

#include "DMGameEngine/Core/Log.h"

#include <d3dcompiler.h>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>

namespace DMGameEngine {

D3D11Shader* D3D11Shader::s_Bound = nullptr;

namespace {

constexpr uint32_t kStageVS = 0;
constexpr uint32_t kStagePS = 1;

} // anonymous namespace


// ── Construction / destruction ───────────────────────────────────

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

            m_UniformTargets[varDesc.Name].push_back(
                UniformTarget{ stage, static_cast<uint32_t>(slot),
                               static_cast<uint32_t>(varDesc.StartOffset),
                               static_cast<uint32_t>(varDesc.Size) });
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

void D3D11Shader::Bind() const
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


// ── Uniform staging ──────────────────────────────────────────────

bool D3D11Shader::WriteUniform(std::string_view name, const void* data, uint32_t size)
{
    const auto it = m_UniformTargets.find(std::string(name));
    if (it == m_UniformTargets.end())
        return false;

    for (const UniformTarget& target : it->second)
    {
        auto& staging = m_StageResources[target.Stage].Staging[target.BufferSlot];
        // Member size from reflection is authoritative; clamp for safety.
        const uint32_t bytes = (size < target.Size) ? size : target.Size;
        if (target.Offset + bytes > staging.size())
        {
            DMGE_LOG_WARN("[D3D11] Uniform '{0}' write out of range (offset {1} + {2} > {3})",
                          name, target.Offset, bytes, staging.size());
            continue;
        }
        std::memcpy(staging.data() + target.Offset, data, bytes);
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
    return m_UniformTargets.count(std::string(name)) > 0;
}

bool D3D11Shader::HasTexture(std::string_view name) const
{
    return m_TextureBindPoints.count(std::string(name)) > 0;
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
    (void)name; (void)values; (void)count;
    // TODO(stage C): skeletal skinning (u_BoneMatrices palette). Needs either
    // a dedicated large cbuffer with dynamic offsets or an SSBO-equivalent
    // (structured buffer) path - same follow-up as the Vulkan backend (KB-03).
    DMGE_LOG_WARN("[D3D11] SetMat4Array('{0}') not implemented yet (stage C)", name);
}

} // namespace DMGameEngine
