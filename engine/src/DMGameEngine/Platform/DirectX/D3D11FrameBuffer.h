/*
 * DMGameEngine - Direct3D 11 Frame Buffer
 *
 * D3D11 implementation of the FrameBuffer abstraction (stage A: one color
 * attachment + optional depth/stencil). The color attachment is an
 * ID3D11Texture2D with RENDER_TARGET | SHADER_RESOURCE bind flags, exposed
 * as a D3D11Texture2D (adopting constructor) so it can be sampled after
 * rendering - the render-to-texture contract of the abstraction.
 *
 * CPU readback (test/diagnostic path): CopyColorToStaging() copies the
 * color attachment into a CPU_READ staging texture; ReadbackColor() then
 * Maps and copies it out row by row (GPU row pitch may exceed
 * width * bytesPerPixel). Rows are top-down (D3D11 origin), unlike
 * glReadPixels which reads bottom-up.
 */

#pragma once

#include "DMGameEngine/Renderer/FrameBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <cstdint>

namespace DMGameEngine {

class DMGE_API D3D11FrameBuffer : public FrameBuffer
{
public:
    explicit D3D11FrameBuffer(const FramebufferSpecification& spec);
    ~D3D11FrameBuffer() override;

    void Bind()   override; // OMSetRenderTargets(rtv, dsv)
    void Unbind() override; // OMSetRenderTargets(null)

    void Resize(uint32_t width, uint32_t height) override;

    uint32_t GetWidth()      const override { return m_Spec.Width;  }
    uint32_t GetHeight()     const override { return m_Spec.Height; }
    uint32_t GetRendererID() const override { return m_RendererID; }

    DM::Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const override;
    size_t             GetColorAttachmentCount()        const override { return 1; }

    const FramebufferSpecification& GetSpecification() const override { return m_Spec; }

    // Raw views for D3D11RendererAPI::BeginRenderPass (non-owning).
    ID3D11RenderTargetView* GetRenderTargetView() const { return m_RTV.Get(); }
    ID3D11DepthStencilView* GetDepthStencilView() const { return m_DSV.Get(); }

    // ── CPU readback (stage-A test/diagnostic path) ──────────────
    // Copies color attachment 0 into the internal staging texture.
    bool CopyColorToStaging();
    // Maps the staging texture and copies rows into dst (must hold
    // width*height*4 bytes for RGBA8). Returns false on failure.
    bool ReadbackColor(void* dst, uint32_t dstSizeInBytes);

private:
    void Invalidate(); // (re)create all attachments from m_Spec
    void Destroy();

    FramebufferSpecification m_Spec;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_ColorTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_RTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ColorSRV;
    DM::Ref<Texture2D>                               m_ColorAttachment;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_DepthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_DSV;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_StagingTexture;

    uint32_t m_RendererID = 0;
};

} // namespace DMGameEngine
