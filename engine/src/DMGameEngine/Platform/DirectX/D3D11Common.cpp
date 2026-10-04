/*
 * DMGameEngine - D3D11 Backend Common (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <array>
#include <cstring>

namespace DMGameEngine {
namespace D3D11Backend {

namespace {

ID3D11Device*        s_Device  = nullptr;
ID3D11DeviceContext* s_Context = nullptr;

// Non-owning unit table (SRVs are owned by D3D11Texture2D instances).
std::array<ID3D11ShaderResourceView*, kMaxTextureUnits> s_TextureUnits{};

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_WhiteDummySRV;

} // anonymous namespace

void SetDeviceContext(ID3D11Device* device, ID3D11DeviceContext* context)
{
    s_Device  = device;
    s_Context = context;
    // A new device invalidates any previously registered SRVs / dummy SRV.
    s_TextureUnits.fill(nullptr);
    s_WhiteDummySRV.Reset();
}

void ClearDeviceContext()
{
    s_TextureUnits.fill(nullptr);
    s_WhiteDummySRV.Reset();
    s_Device  = nullptr;
    s_Context = nullptr;
}

ID3D11Device* Device()        { return s_Device; }
ID3D11DeviceContext* Context(){ return s_Context; }

DXGI_FORMAT TextureFormatToDXGI(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:      return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RG8:     return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8:    return DXGI_FORMAT_R8G8B8A8_UNORM; // no 24-bit DXGI type; widened (documented deviation)
        case TextureFormat::RGBA8:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::R16F:    return DXGI_FORMAT_R16_FLOAT;
        case TextureFormat::RG16F:   return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::RGB16F:  return DXGI_FORMAT_R16G16B16A16_FLOAT; // widened
        case TextureFormat::RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::R32F:    return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::RG32F:   return DXGI_FORMAT_R32G32_FLOAT;
        case TextureFormat::RGB32F:  return DXGI_FORMAT_R32G32B32A32_FLOAT; // widened
        case TextureFormat::RGBA32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            DMGE_CORE_ASSERT(false, "TextureFormat has no DXGI color mapping!");
            return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT DepthFormatToDXGI(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::Depth:
        case TextureFormat::DepthStencil:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:
            DMGE_CORE_ASSERT(false, "TextureFormat is not a depth format!");
            return DXGI_FORMAT_UNKNOWN;
    }
}

uint32_t TextureFormatBytesPerPixel(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::R8:      return 1;
        case TextureFormat::RG8:     return 2;
        case TextureFormat::RGBA8:
        case TextureFormat::RGB8:    return 4; // RGB8 widened to RGBA8, see mapping above
        case TextureFormat::R16F:    return 2;
        case TextureFormat::RG16F:   return 4;
        case TextureFormat::RGBA16F:
        case TextureFormat::RGB16F:  return 8; // RGB16F widened
        case TextureFormat::R32F:    return 4;
        case TextureFormat::RG32F:   return 8;
        case TextureFormat::RGBA32F:
        case TextureFormat::RGB32F:  return 16; // RGB32F widened
        default:                     return 0;
    }
}

void BindSRVToUnit(uint32_t unit, ID3D11ShaderResourceView* srv)
{
    if (unit >= kMaxTextureUnits)
    {
        DMGE_LOG_WARN("[D3D11] Texture unit {0} out of range (max {1})", unit, kMaxTextureUnits);
        return;
    }
    s_TextureUnits[unit] = srv;
}

ID3D11ShaderResourceView* SRVForUnit(uint32_t unit)
{
    if (unit < kMaxTextureUnits && s_TextureUnits[unit])
        return s_TextureUnits[unit];
    return WhiteDummySRV();
}

ID3D11ShaderResourceView* WhiteDummySRV()
{
    if (s_WhiteDummySRV)
        return s_WhiteDummySRV.Get();

    if (!s_Device)
    {
        DMGE_LOG_WARN("[D3D11] WhiteDummySRV requested before device init");
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = 1;
    desc.Height           = 1;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    const uint8_t white[4] = { 255, 255, 255, 255 };
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = white;
    init.SysMemPitch = 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = s_Device->CreateTexture2D(&desc, &init, &texture);
    DMGE_D3D_CHECK(hr, "CreateTexture2D (white dummy)");

    if (SUCCEEDED(hr))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format                         = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels            = 1;
        hr = s_Device->CreateShaderResourceView(texture.Get(), &srvDesc, &s_WhiteDummySRV);
        DMGE_D3D_CHECK(hr, "CreateShaderResourceView (white dummy)");
    }

    return s_WhiteDummySRV.Get();
}

} // namespace D3D11Backend
} // namespace DMGameEngine
