/*
 * DMGameEngine - Direct3D 11 Graphics Context
 *
 * D3D11 implementation of the GraphicsContext abstraction (stage B): owns
 * the window-facing device state - an ID3D11Device created against a Win32
 * HWND, an IDXGISwapChain1 (flip-discard, 2 buffers, BGRA8), the backbuffer
 * render-target view and a matching depth/stencil view.
 *
 * Lifecycle contract (mirrors the OpenGL backend):
 *  1. WindowsWindow creates this context with the native HWND and calls
 *     Init() BEFORE Renderer::Init() runs. Init() registers the device /
 *     immediate context into the backend's process-wide accessors
 *     (D3D11Common), so resources and D3D11RendererAPI::Init() adopt this
 *     device instead of creating a headless one.
 *  2. Each frame ends with SwapBuffers() -> IDXGISwapChain1::Present.
 *  3. Window resizes arrive via RequestResize() (GLFW size callback, before
 *     the WindowResizeEvent re-sets the viewport): the backbuffer views are
 *     released, ResizeBuffers recreates the color buffers and the depth
 *     buffer follows.
 *
 * Backend-only: nothing in here leaks into the public abstraction (R5).
 */

#pragma once

#include "DMGameEngine/Renderer/GraphicsContext.h"
#include "DMGameEngine/Platform/DirectX/D3D11Common.h"

#include <dxgi1_3.h>  // IDXGIFactory2 / IDXGISwapChain1 / CreateDXGIFactory2
#include <cstdint>

namespace DMGameEngine {

// C4251: same rationale as the other D3D11 backend classes (COM pointers /
// header-only members only) - see D3D11RendererAPI.h.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

class DMGE_API DirectXGraphicsContext : public GraphicsContext
{
public:
    // hwnd: Win32 top-level window handle (HWND). GLFW windows hand theirs
    // over via glfwGetWin32Window (WindowsWindow). A null/invalid HWND fails
    // Init() with an error log (headless paths must not construct this class).
    explicit DirectXGraphicsContext(void* hwnd);
    ~DirectXGraphicsContext() override;

    void Init() override;
    void SwapBuffers() override;
    void RequestResize(uint32_t width, uint32_t height) override;

    const char* GetVendor() const override;
    const char* GetRenderer() const override;
    const char* GetVersion() const override;

    // ── Window render target (consumed by D3D11RendererAPI) ─────
    // Non-owning views into the current backbuffer + window depth buffer.
    // The same pair is registered into D3D11Backend::SetWindowTarget, so
    // BeginRenderPass(nullptr / SwapChainTarget) binds the screen.
    ID3D11RenderTargetView* GetBackbufferRTV() const { return m_BackbufferRTV.Get(); }
    ID3D11DepthStencilView* GetDepthDSV()      const { return m_DepthDSV.Get(); }
    ID3D11Texture2D*        GetBackbufferTexture() const { return m_BackbufferTexture.Get(); }

    uint32_t GetWidth()  const { return m_Width;  }
    uint32_t GetHeight() const { return m_Height; }

    // True when Init() completed successfully.
    bool IsValid() const { return m_Swapchain != nullptr; }

private:
    void CreateBackbufferViews();
    void CreateDepthBuffer();
    void DestroyWindowTargets();
    void ApplyPendingResize();

    HWND m_Hwnd = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory2>    m_Factory;
    mutable Microsoft::WRL::ComPtr<IDXGIAdapter1> m_Adapter; // lazily resolved in GetRenderer()
    Microsoft::WRL::ComPtr<IDXGISwapChain1>  m_Swapchain;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_BackbufferTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_BackbufferRTV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_DepthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_DepthDSV;

    uint32_t m_Width  = 0;
    uint32_t m_Height = 0;
    bool     m_PendingResize = false;
    uint32_t m_PendingWidth  = 0;
    uint32_t m_PendingHeight = 0;

    // Stage B: vsync always on (Present(1)); the GraphicsContext interface
    // has no vsync carrier yet, and WindowsWindow::SetVSync skips the GLFW
    // call for this backend (documented stage-B limitation).
    bool m_VSync = true;

    // Adapter / feature-level description strings (GetVendor & co).
    mutable char m_Vendor[128]   = "Unknown";
    mutable char m_Renderer[128] = "Unknown";
    mutable char m_Version[32]   = "Unknown";
    mutable bool m_InfoQueried = false;
    void QueryAdapterInfo() const;
};

} // namespace DMGameEngine

#ifdef _MSC_VER
#pragma warning(pop)
#endif
