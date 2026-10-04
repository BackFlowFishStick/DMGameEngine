/*
 * DMGameEngine - Direct3D 11 Frame Buffer (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11FrameBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11Texture2D.h"

#include "DMGameEngine/Core/Log.h"

#include <atomic>
#include <cstring>

namespace DMGameEngine {

namespace {

uint32_t NextFrameBufferID()
{
    // Stand-in for the GL FBO name exposed via GetRendererID() (see
    // D3D11Texture2D.cpp for the same pattern).
    static std::atomic<uint32_t> s_Next{ 1 };
    return s_Next.fetch_add(1);
}

} // anonymous namespace


// ── Construction / destruction ───────────────────────────────────

D3D11FrameBuffer::D3D11FrameBuffer(const FramebufferSpecification& spec)
    : m_Spec(spec)
{
    for (auto& attachment : m_Spec.Attachments)
    {
        if (attachment.Format == TextureFormat::None)
            attachment.Format = TextureFormat::RGBA8;
    }

    Invalidate();
}

D3D11FrameBuffer::~D3D11FrameBuffer()
{
    Destroy();
}

void D3D11FrameBuffer::Destroy()
{
    // ComPtr members release themselves; clear the attachment Ref so the
    // adopted D3D11Texture2D drops its views with us.
    m_ColorAttachment.reset();
    m_ColorTexture.Reset();
    m_RTV.Reset();
    m_ColorSRV.Reset();
    m_DepthTexture.Reset();
    m_DSV.Reset();
    m_StagingTexture.Reset();
}

// ── Attachment creation ──────────────────────────────────────────

void D3D11FrameBuffer::Invalidate()
{
    ID3D11Device* device = D3D11Backend::Device();
    DMGE_CORE_ASSERT(device, "D3D11FrameBuffer created before D3D11RendererAPI::Init (no device)!");

    Destroy();

    const UINT width  = m_Spec.Width;
    const UINT height = m_Spec.Height;
    DMGE_CORE_ASSERT(width > 0 && height > 0, "Invalid framebuffer size!");

    // ── Color attachment 0 (stage A: single color attachment) ──
    const TextureFormat colorFormat = m_Spec.Attachments.empty()
                                          ? TextureFormat::RGBA8
                                          : m_Spec.Attachments[0].Format;

    D3D11_TEXTURE2D_DESC colorDesc{};
    colorDesc.Width            = width;
    colorDesc.Height           = height;
    colorDesc.MipLevels        = 1;
    colorDesc.ArraySize        = 1;
    colorDesc.Format           = D3D11Backend::TextureFormatToDXGI(colorFormat);
    colorDesc.SampleDesc.Count = 1; // backends allocate 1-sample storage (see FramebufferSpecification::Samples)
    colorDesc.Usage            = D3D11_USAGE_DEFAULT;
    colorDesc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&colorDesc, nullptr, &m_ColorTexture);
    DMGE_D3D_CHECK(hr, "CreateTexture2D (fb color)");
    if (FAILED(hr))
        return;

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format             = colorDesc.Format;
    rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    hr = device->CreateRenderTargetView(m_ColorTexture.Get(), &rtvDesc, &m_RTV);
    DMGE_D3D_CHECK(hr, "CreateRenderTargetView");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                    = colorDesc.Format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels       = 1;
    hr = device->CreateShaderResourceView(m_ColorTexture.Get(), &srvDesc, &m_ColorSRV);
    DMGE_D3D_CHECK(hr, "CreateShaderResourceView (fb color)");

    // Expose the attachment as a Texture2D (render-to-texture contract).
    if (m_ColorSRV)
    {
        Texture2DSpecification texSpec;
        texSpec.Width           = width;
        texSpec.Height          = height;
        texSpec.Format          = colorFormat;
        texSpec.GenerateMipmaps = false;
        m_ColorAttachment = DM::CreateRef<D3D11Texture2D>(m_ColorTexture, m_ColorSRV, texSpec);
    }

    // ── Depth/stencil attachment ────────────────────────────────
    if (m_Spec.DepthFormat != TextureFormat::None)
    {
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width            = width;
        depthDesc.Height           = height;
        depthDesc.MipLevels        = 1;
        depthDesc.ArraySize        = 1;
        depthDesc.Format           = D3D11Backend::DepthFormatToDXGI(m_Spec.DepthFormat);
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage            = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

        hr = device->CreateTexture2D(&depthDesc, nullptr, &m_DepthTexture);
        DMGE_D3D_CHECK(hr, "CreateTexture2D (fb depth)");

        if (SUCCEEDED(hr))
        {
            hr = device->CreateDepthStencilView(m_DepthTexture.Get(), nullptr, &m_DSV);
            DMGE_D3D_CHECK(hr, "CreateDepthStencilView");
        }
    }

    m_RendererID = NextFrameBufferID();
}

// ── Bind / Unbind ────────────────────────────────────────────────

void D3D11FrameBuffer::Bind()
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context)
        return;

    ID3D11RenderTargetView* rtv = m_RTV.Get();
    context->OMSetRenderTargets(1, &rtv, m_DSV.Get());
}

void D3D11FrameBuffer::Unbind()
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context)
        return;

    ID3D11RenderTargetView* nullRTV = nullptr;
    context->OMSetRenderTargets(1, &nullRTV, nullptr);
}

// ── Resize ───────────────────────────────────────────────────────

void D3D11FrameBuffer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        DMGE_LOG_WARN("[D3D11] FrameBuffer::Resize to {0}x{1} rejected", width, height);
        return;
    }

    m_Spec.Width  = width;
    m_Spec.Height = height;
    Invalidate();
}

// ── Attachments ──────────────────────────────────────────────────

DM::Ref<Texture2D> D3D11FrameBuffer::GetColorAttachment(uint32_t index) const
{
    (void)index; // stage A: single color attachment
    return m_ColorAttachment;
}

// ── CPU readback ─────────────────────────────────────────────────

bool D3D11FrameBuffer::CopyColorToStaging()
{
    ID3D11Device* device = D3D11Backend::Device();
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!device || !context || !m_ColorTexture)
        return false;

    if (!m_StagingTexture)
    {
        D3D11_TEXTURE2D_DESC rtDesc{};
        m_ColorTexture->GetDesc(&rtDesc);

        D3D11_TEXTURE2D_DESC stagingDesc = rtDesc;
        stagingDesc.Usage          = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags      = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MipLevels      = 1;

        HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &m_StagingTexture);
        DMGE_D3D_CHECK(hr, "CreateTexture2D (staging)");
        if (FAILED(hr))
            return false;
    }

    context->CopyResource(m_StagingTexture.Get(), m_ColorTexture.Get());
    return true;
}

bool D3D11FrameBuffer::ReadbackColor(void* dst, uint32_t dstSizeInBytes)
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context || !m_StagingTexture || !dst)
        return false;

    const uint32_t bpp = D3D11Backend::TextureFormatBytesPerPixel(
        m_Spec.Attachments.empty() ? TextureFormat::RGBA8 : m_Spec.Attachments[0].Format);
    const uint32_t rowBytes = m_Spec.Width * bpp;
    if (dstSizeInBytes < rowBytes * m_Spec.Height)
    {
        DMGE_LOG_WARN("[D3D11] ReadbackColor dst too small ({0} < {1})",
                      dstSizeInBytes, rowBytes * m_Spec.Height);
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context->Map(m_StagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    DMGE_D3D_CHECK(hr, "Map (staging readback)");
    if (FAILED(hr))
        return false;

    const auto* src = static_cast<const uint8_t*>(mapped.pData);
    auto* out = static_cast<uint8_t*>(dst);
    for (uint32_t row = 0; row < m_Spec.Height; ++row)
        std::memcpy(out + row * rowBytes, src + row * mapped.RowPitch, rowBytes);

    context->Unmap(m_StagingTexture.Get(), 0);
    return true;
}

} // namespace DMGameEngine
