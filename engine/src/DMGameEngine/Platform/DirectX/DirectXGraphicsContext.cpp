/*
 * DMGameEngine - Direct3D 11 Graphics Context (implementation)
 */

#include "DMGameEngine/Platform/DirectX/DirectXGraphicsContext.h"

#include "DMGameEngine/Core/Log.h"

#include <cstdio>
#include <cstring>

namespace DMGameEngine {

DirectXGraphicsContext::DirectXGraphicsContext(void* hwnd)
    : m_Hwnd(static_cast<HWND>(hwnd))
{
}

DirectXGraphicsContext::~DirectXGraphicsContext()
{
    // Release the window target views first (they reference swapchain
    // buffers), then let the swapchain go. The device registered in
    // D3D11Backend is only cleared if this context still owns it - a
    // D3D11RendererAPI that adopted it may have outlived the window.
    DestroyWindowTargets();
    if (m_Swapchain)
        m_Swapchain.Reset();

    // Our device ComPtr lived in the process-wide accessor (single-device
    // stage B): the D3D11RendererAPI owns the reference and clears it.
}

void DirectXGraphicsContext::Init()
{
    if (!m_Hwnd || !IsWindow(m_Hwnd))
    {
        DMGE_LOG_ERROR("[D3D11] DirectXGraphicsContext::Init - invalid HWND");
        return;
    }

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(m_Factory.GetAddressOf()));
    DMGE_D3D_CHECK(hr, "CreateDXGIFactory2");
    if (FAILED(hr))
        return;

    // Pick the adapter that owns the window's monitor (fall back to the
    // factory default, index 0, when the walk fails).
    {
        HMONITOR monitor = MonitorFromWindow(m_Hwnd, MONITOR_DEFAULTTONEAREST);
        IDXGIAdapter1* chosen = nullptr;
        for (UINT i = 0; m_Factory->EnumAdapters1(i, &chosen) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 desc{};
            chosen->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                chosen->Release();
                continue;
            }
            IDXGIOutput* output = nullptr;
            bool ownsMonitor = false;
            for (UINT o = 0; chosen->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o)
            {
                DXGI_OUTPUT_DESC od{};
                output->GetDesc(&od);
                if (od.Monitor == monitor)
                    ownsMonitor = true;
                output->Release();
                if (ownsMonitor)
                    break;
            }
            if (ownsMonitor)
                break; // `chosen` stays owned by us
            chosen->Release();
            chosen = nullptr;
        }
        m_Adapter = chosen; // may be null -> swapchain uses the default adapter
    }

    RECT clientRect{};
    if (!GetClientRect(m_Hwnd, &clientRect))
    {
        DMGE_LOG_ERROR("[D3D11] GetClientRect failed for the swapchain window");
        return;
    }
    m_Width  = static_cast<uint32_t>(clientRect.right  - clientRect.left);
    m_Height = static_cast<uint32_t>(clientRect.bottom - clientRect.top);
    if (m_Width == 0 || m_Height == 0)
    {
        DMGE_LOG_ERROR("[D3D11] Swapchain window has a zero client area ({}x{})",
                       m_Width, m_Height);
        return;
    }

    // A device is required before CreateSwapChainForHwnd. Stage B runs a
    // single process-wide device: if one is already registered (headless
    // Init ran first), adopt it; otherwise create the window-facing device
    // here (hardware -> WARP -> reference, same ladder as D3D11RendererAPI).
    Microsoft::WRL::ComPtr<ID3D11Device>        device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    if (D3D11Backend::Device())
    {
        device  = D3D11Backend::Device();
        context = D3D11Backend::Context();
        DMGE_LOG_INFO("[D3D11] GraphicsContext adopting the registered device");
    }
    else
    {
        // D3D11CreateDevice contract: when an adapter is given, DriverType
        // MUST be D3D_DRIVER_TYPE_UNKNOWN (any other type fails with
        // E_INVALIDARG). The type ladder only applies to the adapter-less
        // (headless) path.
        HRESULT devHr = E_FAIL;
        if (m_Adapter)
        {
            devHr = D3D11CreateDevice(m_Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                      nullptr, 0, D3D11_SDK_VERSION,
                                      &device, nullptr, &context);
        }
        else
        {
            static const D3D_DRIVER_TYPE kDriverTypes[] =
            {
                D3D_DRIVER_TYPE_HARDWARE,
                D3D_DRIVER_TYPE_WARP,
                D3D_DRIVER_TYPE_REFERENCE,
            };
            for (D3D_DRIVER_TYPE type : kDriverTypes)
            {
                devHr = D3D11CreateDevice(nullptr, type, nullptr,
                                          D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                          nullptr, 0, D3D11_SDK_VERSION,
                                          &device, nullptr, &context);
                if (SUCCEEDED(devHr))
                    break;
            }
        }
        DMGE_D3D_CHECK(devHr, "D3D11CreateDevice (swapchain)");
        if (FAILED(devHr))
            return;
    }

    // ── Swapchain (flip-discard, 2 buffers, BGRA8) ───────────────
    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.Width       = m_Width;
    scDesc.Height      = m_Height;
    scDesc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM; // device has BGRA_SUPPORT
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.Scaling     = DXGI_SCALING_STRETCH;

    hr = m_Factory->CreateSwapChainForHwnd(device.Get(), m_Hwnd, &scDesc,
                                           nullptr, nullptr,
                                           m_Swapchain.GetAddressOf());
    DMGE_D3D_CHECK(hr, "CreateSwapChainForHwnd");
    if (FAILED(hr))
        return;

    // Alt+Enter fullscreen toggle would need IDXGISwapChain association
    // handling the engine does not model; the window owns itself.
    m_Factory->MakeWindowAssociation(m_Hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Register the process-wide device/context (headless Init created none
    // in the fresh-window flow; the adopting flow re-registers the same
    // pointers, which is a no-op except for the unit-table reset).
    D3D11Backend::SetDeviceContext(device.Get(), context.Get());

    CreateBackbufferViews();
    CreateDepthBuffer();
    D3D11Backend::SetWindowTarget(m_BackbufferRTV.Get(), m_DepthDSV.Get());

    DMGE_LOG_INFO("[D3D11] Swapchain created ({}x{}, flip-discard, 2 buffers)",
                  m_Width, m_Height);
}

void DirectXGraphicsContext::CreateBackbufferViews()
{
    HRESULT hr = m_Swapchain->GetBuffer(0, IID_PPV_ARGS(m_BackbufferTexture.GetAddressOf()));
    DMGE_D3D_CHECK(hr, "GetBuffer(backbuffer)");
    if (FAILED(hr))
        return;

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    hr = D3D11Backend::Device()->CreateRenderTargetView(m_BackbufferTexture.Get(),
                                                        &rtvDesc,
                                                        m_BackbufferRTV.GetAddressOf());
    DMGE_D3D_CHECK(hr, "CreateRenderTargetView(backbuffer)");
}

void DirectXGraphicsContext::CreateDepthBuffer()
{
    ID3D11Device* device = D3D11Backend::Device();
    if (!device || m_Width == 0 || m_Height == 0)
        return;

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width            = m_Width;
    depthDesc.Height           = m_Height;
    depthDesc.MipLevels        = 1;
    depthDesc.ArraySize        = 1;
    depthDesc.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage            = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = device->CreateTexture2D(&depthDesc, nullptr, m_DepthTexture.GetAddressOf());
    DMGE_D3D_CHECK(hr, "CreateTexture2D (window depth)");
    if (FAILED(hr))
        return;

    hr = device->CreateDepthStencilView(m_DepthTexture.Get(), nullptr,
                                        m_DepthDSV.GetAddressOf());
    DMGE_D3D_CHECK(hr, "CreateDepthStencilView (window depth)");
}

void DirectXGraphicsContext::DestroyWindowTargets()
{
    // Detach anything the immediate context may still hold, then release.
    if (ID3D11DeviceContext* context = D3D11Backend::Context())
        context->OMSetRenderTargets(0, nullptr, nullptr);

    // The process-wide window-target accessors must not dangle (resize
    // recreates them right after; destruction leaves them null).
    D3D11Backend::ClearWindowTarget();

    m_BackbufferRTV.Reset();
    m_BackbufferTexture.Reset();
    m_DepthDSV.Reset();
    m_DepthTexture.Reset();
}

void DirectXGraphicsContext::ApplyPendingResize()
{
    if (!m_PendingResize || !m_Swapchain)
        return;
    m_PendingResize = false;

    const uint32_t w = m_PendingWidth;
    const uint32_t h = m_PendingHeight;
    if (w == 0 || h == 0 || (w == m_Width && h == m_Height))
        return;

    // ResizeBuffers requires every reference to the buffers released.
    DestroyWindowTargets();

    HRESULT hr = m_Swapchain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    DMGE_D3D_CHECK(hr, "ResizeBuffers");
    if (FAILED(hr))
        return;

    m_Width  = w;
    m_Height = h;

    CreateBackbufferViews();
    CreateDepthBuffer();
    D3D11Backend::SetWindowTarget(m_BackbufferRTV.Get(), m_DepthDSV.Get());

    DMGE_LOG_INFO("[D3D11] Swapchain resized to {0}x{1}", w, h);
}

void DirectXGraphicsContext::SwapBuffers()
{
    ApplyPendingResize();
    if (m_Swapchain)
    {
        HRESULT hr = m_Swapchain->Present(m_VSync ? 1 : 0, 0);
        if (FAILED(hr))
            DMGE_LOG_ERROR("[D3D11] Present failed: HRESULT 0x{0:08X}",
                           static_cast<uint32_t>(hr));
    }
}

void DirectXGraphicsContext::RequestResize(uint32_t width, uint32_t height)
{
    // Applied at the next SwapBuffers - the GLFW size callback runs before
    // the frame renders, so the present carries the new size while the
    // frame in flight targets the freshly resized backbuffer via the RTV
    // recreated here.
    m_PendingResize = true;
    m_PendingWidth  = width;
    m_PendingHeight = height;
    ApplyPendingResize();
}

void DirectXGraphicsContext::QueryAdapterInfo() const
{
    if (m_InfoQueried)
        return;
    m_InfoQueried = true;

    // Renderer/vendor: adapter description. Version: feature level.
    if (!m_Adapter)
    {
        // The swapchain's adapter works even when the monitor walk failed.
        if (m_Swapchain)
        {
            Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
            if (SUCCEEDED(m_Swapchain->GetDevice(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))))
            {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
                adapter.Get()->QueryInterface(IID_PPV_ARGS(m_Adapter.ReleaseAndGetAddressOf()));
            }
        }
    }
    if (m_Adapter)
    {
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(m_Adapter->GetDesc1(&desc)))
        {
            size_t i = 0;
            for (; desc.Description[i] != 0 && i < sizeof(m_Renderer) - 1; ++i)
                m_Renderer[i] = static_cast<char>(desc.Description[i]);
            m_Renderer[i] = '\0';
            std::snprintf(m_Vendor, sizeof(m_Vendor), "PCI 0x%04X", desc.VendorId);
        }
    }
    if (ID3D11Device* device = D3D11Backend::Device())
    {
        std::snprintf(m_Version, sizeof(m_Version), "Feature Level 0x%x",
                      static_cast<unsigned>(device->GetFeatureLevel()));
    }
}

const char* DirectXGraphicsContext::GetVendor() const
{
    QueryAdapterInfo();
    return m_Vendor;
}

const char* DirectXGraphicsContext::GetRenderer() const
{
    QueryAdapterInfo();
    return m_Renderer;
}

const char* DirectXGraphicsContext::GetVersion() const
{
    QueryAdapterInfo();
    return m_Version;
}

} // namespace DMGameEngine
