/*
 * DMGameEngine - Direct3D 11 Texture2D (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11Texture2D.h"

#include "DMGameEngine/Core/Log.h"

#include <stb_image.h>

#include <atomic>
#include <cstring>

namespace DMGameEngine {

namespace {

uint32_t NextTextureID()
{
    // GL textures are identified by a GLuint name; the abstraction exposes
    // GetRendererID() as uint32_t. D3D11 COM pointers have no such id, so a
    // process-wide counter stands in for cross-backend identity checks.
    static std::atomic<uint32_t> s_Next{ 1 };
    return s_Next.fetch_add(1);
}

} // anonymous namespace


// ── Construction / destruction ───────────────────────────────────

D3D11Texture2D::D3D11Texture2D(const Texture2DSpecification& spec)
    : m_Spec(spec)
{
    Create(nullptr);
}

D3D11Texture2D::D3D11Texture2D(std::string_view filepath)
    : m_FilePath(filepath)
{
    int width = 0, height = 0, channels = 0;
    // D3D11's origin is top-left: no vertical flip on load (the OpenGL
    // backend flips because its origin is bottom-left).
    stbi_uc* data = stbi_load(m_FilePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!data)
    {
        DMGE_CORE_ASSERT(false, "Failed to load texture from file: {0}", m_FilePath);
        return;
    }

    m_Spec.Width  = static_cast<uint32_t>(width);
    m_Spec.Height = static_cast<uint32_t>(height);
    m_Spec.Format = TextureFormat::RGBA8;

    Create(data);
    stbi_image_free(data);

    if (m_Spec.GenerateMipmaps)
        GenerateMipmaps();
}

D3D11Texture2D::D3D11Texture2D(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                               Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv,
                               const Texture2DSpecification& spec)
    : m_Spec(spec)
    , m_Texture(std::move(texture))
    , m_SRV(std::move(srv))
{
    m_RendererID = NextTextureID();
}

D3D11Texture2D::~D3D11Texture2D() = default;

// ── GPU resource creation ────────────────────────────────────────

void D3D11Texture2D::Create(const void* initialData)
{
    ID3D11Device* device = D3D11Backend::Device();
    DMGE_CORE_ASSERT(device, "D3D11Texture2D created before D3D11RendererAPI::Init (no device)!");

    const DXGI_FORMAT format = D3D11Backend::TextureFormatToDXGI(m_Spec.Format);
    DMGE_CORE_ASSERT(format != DXGI_FORMAT_UNKNOWN, "Unsupported texture format for D3D11");

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = m_Spec.Width;
    desc.Height           = m_Spec.Height;
    desc.MipLevels        = m_Spec.GenerateMipmaps ? 0 : 1; // 0 = full chain
    desc.ArraySize        = 1;
    desc.Format           = format;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    // RENDER_TARGET: required for attachment use and GenerateMips; SHADER_RESOURCE
    // for sampling. Depth formats are handled by D3D11FrameBuffer, not here.
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags   = 0;

    D3D11_SUBRESOURCE_DATA init{};
    D3D11_SUBRESOURCE_DATA* initPtr = nullptr;
    if (initialData)
    {
        init.pSysMem     = initialData;
        init.SysMemPitch = m_Spec.Width * D3D11Backend::TextureFormatBytesPerPixel(m_Spec.Format);
        initPtr = &init;
    }

    HRESULT hr = device->CreateTexture2D(&desc, initPtr, &m_Texture);
    DMGE_D3D_CHECK(hr, "CreateTexture2D");
    if (FAILED(hr))
        return;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format              = format;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels       = m_Spec.GenerateMipmaps ? 0xFFFFFFFFu : 1u; // 0xFFFFFFFF = full chain
    hr = device->CreateShaderResourceView(m_Texture.Get(), &srvDesc, &m_SRV);
    DMGE_D3D_CHECK(hr, "CreateShaderResourceView");

    m_RendererID = NextTextureID();
}

// ── Bind / Unbind ────────────────────────────────────────────────

void D3D11Texture2D::Bind(uint32_t slot) const
{
    // Register in the unit table (resolved by D3D11Shader::Bind against the
    // reflection-derived bind points) and issue an immediate PS bind.
    D3D11Backend::BindSRVToUnit(slot, m_SRV.Get());

    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (context && m_SRV)
    {
        ID3D11ShaderResourceView* srv = m_SRV.Get();
        context->PSSetShaderResources(slot, 1, &srv);
    }
}

void D3D11Texture2D::Unbind() const
{
    D3D11Backend::BindSRVToUnit(0, nullptr);
}

// ── Data upload ──────────────────────────────────────────────────

void D3D11Texture2D::SetData(void* data, uint32_t size)
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context || !m_Texture)
        return;

    const uint32_t pitch = m_Spec.Width * D3D11Backend::TextureFormatBytesPerPixel(m_Spec.Format);
    const uint32_t expected = pitch * m_Spec.Height;
    if (size < expected)
    {
        DMGE_LOG_WARN("[D3D11] Texture SetData size {0} < expected {1} (ignored)", size, expected);
        return;
    }

    context->UpdateSubresource(m_Texture.Get(), 0, nullptr, data, pitch, 0);
}

// ── Mipmaps ──────────────────────────────────────────────────────

void D3D11Texture2D::GenerateMipmaps()
{
    ID3D11DeviceContext* context = D3D11Backend::Context();
    if (!context || !m_SRV || !m_Spec.GenerateMipmaps)
        return;

    context->GenerateMips(m_SRV.Get());
}

} // namespace DMGameEngine
