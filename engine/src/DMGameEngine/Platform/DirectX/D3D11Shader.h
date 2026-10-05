/*
 * DMGameEngine - Direct3D 11 Shader
 *
 * D3D11 implementation of the Shader abstraction. Compiles HLSL at runtime
 * via D3DCompile (VS/PS entry points) and bridges the engine's OpenGL-style
 * per-name uniform contract onto cbuffer byte offsets using D3DReflect:
 *
 *   SetXxx(name, v)  -> memcpy into the CPU staging copy of every cbuffer
 *                       (VS and/or PS) that has a member with that name, at
 *                       the reflection-derived offset.
 *   Bind()           -> UpdateSubresource each cbuffer, bind constant
 *                       buffers/shaders/samplers, and resolve texture SRVs
 *                       from the GL-style unit table (D3D11Common).
 *
 * Uniform names are expected to match the GLSL shaders verbatim (e.g.
 * u_ViewProjection); see documents/DIRECTX_BACKEND_DESIGN.md §5 for the
 * full GLSL <-> HLSL semantic mapping.
 */

#pragma once

#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>

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

class DMGE_API D3D11Shader : public Shader
{
public:
    // From two source strings (HLSL, VSMain/PSMain entry points) - the
    // Shader::Create(name, vert, frag) factory path.
    D3D11Shader(std::string_view name,
                std::string_view vertexSrc,
                std::string_view fragmentSrc);
    // From a file using the engine's `#type vertex` / `#type fragment`
    // (or `pixel`) block convention - the Shader::Create(filepath) factory
    // path. The blocks contain HLSL (cbuffers, VSMain/PSMain), NOT GLSL.
    explicit D3D11Shader(std::string_view filepath);
    ~D3D11Shader() override;

    void Bind()   const override;
    void Unbind() const override;

    const std::string& GetName() const override { return m_Name; }

    // ── Uniform setters (per-name, OpenGL semantics) ────────────
    void SetInt(std::string_view name, int value) override;
    void SetIntArray(std::string_view name, const int* values, uint32_t count) override;
    void SetFloat(std::string_view name, float value) override;
    void SetFloat2(std::string_view name, const glm::vec2& value) override;
    void SetFloat3(std::string_view name, const glm::vec3& value) override;
    void SetFloat4(std::string_view name, const glm::vec4& value) override;
    void SetMat4(std::string_view name, const glm::mat4& value) override;
    // Stage A TODO: skinning palette upload (requires a dynamic-offset cbuffer
    // path; see Shader::SetMat4Array base note / kb/KB-03 Vulkan analog).
    void SetMat4Array(std::string_view name, const glm::mat4* values, uint32_t count) override;

    // ── Diagnostics / test hooks ────────────────────────────────
    // True when a cbuffer member (or a sampler/texture resource) with the
    // given name was found by reflection. Indexed array element names
    // ("u_PointLights_position[3]") resolve against the array member.
    bool HasUniform(std::string_view name) const;
    bool HasTexture(std::string_view name) const;
    // Copies a uniform's current CPU staging bytes into dst (must hold
    // `size` bytes; indexed array element names supported). Diagnostics /
    // test hook: verifies what the next Bind() would upload without a GPU
    // readback (cbuffers are GPU-private DEFAULT resources). Returns false
    // when the name is unknown.
    bool ReadUniformStaging(std::string_view name, void* dst, uint32_t size) const;

    ID3DBlob* GetVertexBlob() const { return m_VSBlob.Get(); }

    // The shader currently bound via Bind(). D3D11VertexArray needs the VS
    // blob to build matching ID3D11InputLayout objects (D3D11 has no VAO).
    static D3D11Shader* CurrentBound();

    // Flushes the currently bound shader's uniform staging into its GPU
    // cbuffers (no-op if nothing is bound). D3D11RendererAPI calls this at
    // DRAW time: the engine contract is shader->Bind() -> SetXxx(...)* ->
    // DrawIndexed(...) (RenderQueue), mirroring GL glUniform timing.
    static void UploadBoundStaging();

private:
    // Where one uniform name lives: which stage, which cbuffer slot, at what
    // byte offset inside that cbuffer. A name can appear in several places.
    struct UniformTarget
    {
        uint32_t Stage;         // 0 = VS, 1 = PS
        uint32_t BufferSlot;    // cbuffer register (b#)
        uint32_t Offset;        // byte offset inside the cbuffer
        uint32_t Size;          // member size in bytes (arrays: total)
        uint32_t ElementStride; // array element stride in bytes (HLSL packs
                                // float3 arrays at 16-byte steps); 0 = scalar
    };

    struct StageResources
    {
        // Indexed by cbuffer register slot (sparse-safe: vector sized to max slot + 1).
        std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> CBuffers;
        std::vector<std::vector<uint8_t>>                 Staging;
    };

    void CompileStage(ID3DBlob** blob, const char* entryPoint, const char* target,
                      std::string_view source);
    void ReflectStage(ID3DBlob* blob, uint32_t stage);
    void UploadStaging() const;
    bool WriteUniform(std::string_view name, const void* data, uint32_t size);
    void WarnOnceMissing(std::string_view name) const;

    std::string m_Name;

    Microsoft::WRL::ComPtr<ID3DBlob>          m_VSBlob;
    Microsoft::WRL::ComPtr<ID3DBlob>          m_PSBlob;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_PS;

    StageResources m_StageResources[2]; // [0]=VS, [1]=PS
    std::unordered_map<std::string, std::vector<UniformTarget>> m_UniformTargets;

    // Sampler-name -> t# bind point (from reflection); the sampler object
    // itself is a shared LINEAR/WRAP sampler bound at its s# slot.
    std::unordered_map<std::string, uint32_t> m_TextureBindPoints;
    std::unordered_map<uint32_t, uint32_t>    m_TextureSlots;      // t# bind point -> requested texture unit
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_Sampler;

    mutable std::unordered_map<std::string, bool> m_MissingWarned;

    static D3D11Shader* s_Bound;
};

} // namespace DMGameEngine

#ifdef _MSC_VER
#pragma warning(pop)
#endif
