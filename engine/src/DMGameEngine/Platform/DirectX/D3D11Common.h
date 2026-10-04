/*
 * DMGameEngine - D3D11 Backend Common
 *
 * Backend-internal shared plumbing for the Direct3D 11 renderer backend:
 * process-wide device/context accessors (D3D11 has no global context like
 * OpenGL's; resources need the device at construction time, mirroring how
 * the OpenGL backend calls into glad directly), DXGI format mapping, the
 * GL-style "texture unit" table used to bridge sampler uniforms, and the
 * HRESULT check macro.
 *
 * Backend-only: nothing in here may leak into the public abstraction
 * headers (R5). Consumed by Platform/DirectX/*.cpp and the D3D11 smoke test.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Renderer/Texture.h" // TextureFormat

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>

namespace DMGameEngine {
namespace D3D11Backend {

// ── HRESULT check ────────────────────────────────────────────────
// Logs on failure in all configs (VK_CHECK lesson, kb/KB-07 K-008) and
// breaks in debug builds.
#define DMGE_D3D_CHECK(hr, what)                                                        \
    do                                                                                  \
    {                                                                                   \
        if (FAILED(hr))                                                                 \
        {                                                                               \
            DMGE_LOG_ERROR("[D3D11] {0} failed: HRESULT 0x{1:08X}", what,               \
                           static_cast<uint32_t>(hr));                                  \
            DMGE_CORE_ASSERT(false, "[D3D11] " what " failed");                         \
        }                                                                               \
    } while (0)

// ── Process-wide device / immediate context ──────────────────────
// Registered by D3D11RendererAPI::Init, cleared by its destructor.
// Stage A is single-device, single immediate context (same lifecycle
// assumption as the OpenGL backend's shared GL state).
// DMGE_API: the headless smoke test exe consumes these through the DLL.
DMGE_API void SetDeviceContext(ID3D11Device* device, ID3D11DeviceContext* context);
DMGE_API void ClearDeviceContext();
DMGE_API ID3D11Device*        Device();
DMGE_API ID3D11DeviceContext* Context();

// ── Format mapping ───────────────────────────────────────────────
// Color formats usable for both SRV (sampling) and RTV (render target).
DXGI_FORMAT TextureFormatToDXGI(TextureFormat format);
// Depth / depth-stencil formats for the DSV attachment.
DXGI_FORMAT DepthFormatToDXGI(TextureFormat format);
// Bytes per pixel for CPU-side upload/readback math (0 = unsupported).
uint32_t TextureFormatBytesPerPixel(TextureFormat format);

// ── GL-style texture unit table ──────────────────────────────────
// Bridges the engine's OpenGL texture-unit contract ("Bind(slot)" +
// "SetInt(samplerName, slot)") to D3D11's explicit SRV binding. Textures
// register their SRV here; D3D11Shader::Bind() resolves the table against
// reflection-derived bind points. Unregistered units resolve to a 1x1
// white dummy SRV (same defense as Vulkan's global dummy texture,
// kb/KB-03 item F).
constexpr uint32_t kMaxTextureUnits = 32;

DMGE_API void BindSRVToUnit(uint32_t unit, ID3D11ShaderResourceView* srv);
DMGE_API ID3D11ShaderResourceView* SRVForUnit(uint32_t unit);
// Never null once a device is registered; 1x1 white fallback SRV.
DMGE_API ID3D11ShaderResourceView* WhiteDummySRV();

} // namespace D3D11Backend
} // namespace DMGameEngine
