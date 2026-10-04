/*
 * DMGameEngine - Direct3D 11 Renderer API
 *
 * D3D11 implementation of the RendererAPI abstraction (stage A subset:
 * device creation, clear/viewport, pipeline state, render pass on an
 * offscreen D3D11FrameBuffer, indexed draws). Interfaces that stage A
 * intentionally leaves out log a TODO warning instead of aborting.
 *
 * Device lifecycle: created headless in Init() (hardware adapter with a
 * WARP/Reference fallback, see documents/DIRECTX_BACKEND_DESIGN.md §6),
 * registered into the backend's process-wide accessor and released in the
 * destructor. State objects (blend / depth-stencil / rasterizer) are
 * created lazily and cached per description hash.
 */

#pragma once

#include "DMGameEngine/Renderer/RendererAPI.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <glm/glm.hpp>
#include <unordered_map>
#include <cstdint>

namespace DMGameEngine {

class DMGE_API D3D11RendererAPI : public RendererAPI
{
public:
    D3D11RendererAPI() = default;
    ~D3D11RendererAPI() override;

    void Init(const RendererAPIInitConfig& config = {}) override;

    void SetClearColor(const glm::vec4& color) override;
    void Clear() override;
    void SetViewport(int x, int y, int width, int height) override;

    void BeginRenderPass(FrameBuffer* target) override;
    void EndRenderPass() override;

    void DrawIndexed(const VertexArray& vertexArray) override;
    void DrawIndexedInstanced(const VertexArray& vertexArray,
                              uint32_t instanceCount,
                              uint32_t baseInstance = 0) override;

    void SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor) override;
    void SetBlendEquation(BlendEquation equation) override;
    void SetDepthTest(bool enable) override;
    void SetDepthFunc(DepthFunc func) override;
    void SetCullMode(CullMode mode) override;

    // Driver type actually used by the last Init (e.g. "Hardware", "WARP").
    const char* GetDriverTypeName() const { return m_DriverTypeName; }

private:
    void ApplyBlendState();
    void ApplyDepthStencilState();
    void ApplyRasterizerState();

    Microsoft::WRL::ComPtr<ID3D11Device>        m_Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context;
    char m_DriverTypeName[16] = "Unknown";

    // Current render pass targets (non-owning; owned by the D3D11FrameBuffer).
    ID3D11RenderTargetView* m_CurrentRTV = nullptr;
    ID3D11DepthStencilView* m_CurrentDSV = nullptr;

    glm::vec4    m_ClearColor{ 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_VIEWPORT m_Viewport{};

    // Cached pipeline state (descriptions) + lazily created state objects.
    bool         m_BlendEnabled = false;
    BlendFactor  m_SrcBlend     = BlendFactor::SrcAlpha;
    BlendFactor  m_DstBlend     = BlendFactor::OneMinusSrcAlpha;
    BlendEquation m_BlendEquation = BlendEquation::Add;
    bool         m_DepthTest    = true;
    DepthFunc    m_DepthFunc    = DepthFunc::Less;
    CullMode     m_CullMode     = CullMode::None;

    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID3D11BlendState>>        m_BlendStates;
    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID3D11DepthStencilState>> m_DepthStates;
    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID3D11RasterizerState>>   m_RasterStates;
};

} // namespace DMGameEngine
