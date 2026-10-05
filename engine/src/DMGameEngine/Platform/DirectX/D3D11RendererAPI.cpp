/*
 * DMGameEngine - Direct3D 11 Renderer API (implementation)
 */

#include "DMGameEngine/Platform/DirectX/D3D11RendererAPI.h"
#include "DMGameEngine/Platform/DirectX/D3D11FrameBuffer.h"
#include "DMGameEngine/Platform/DirectX/D3D11VertexArray.h"
#include "DMGameEngine/Platform/DirectX/D3D11Shader.h"
#include "DMGameEngine/Renderer/VertexArray.h" // full VertexArray / IndexBuffer types

#include "DMGameEngine/Core/Log.h"

#include <cstring>

namespace DMGameEngine {

namespace {

// ── enum mapping ─────────────────────────────────────────────────

D3D11_BLEND BlendFactorToD3D(BlendFactor factor)
{
    switch (factor)
    {
        case BlendFactor::Zero:                  return D3D11_BLEND_ZERO;
        case BlendFactor::One:                   return D3D11_BLEND_ONE;
        case BlendFactor::SrcColor:              return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:      return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::DstColor:              return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::OneMinusDstColor:      return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlpha:              return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:      return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha:              return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:      return D3D11_BLEND_INV_DEST_ALPHA;
        case BlendFactor::ConstantColor:
        case BlendFactor::ConstantAlpha:         return D3D11_BLEND_BLEND_FACTOR; // single float4 constant, no separate alpha factor (documented deviation)
        case BlendFactor::OneMinusConstantColor:
        case BlendFactor::OneMinusConstantAlpha: return D3D11_BLEND_INV_BLEND_FACTOR;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendFactor!");
    return D3D11_BLEND_ZERO;
}

D3D11_BLEND_OP BlendEquationToD3D(BlendEquation equation)
{
    switch (equation)
    {
        case BlendEquation::Add:             return D3D11_BLEND_OP_ADD;
        case BlendEquation::Subtract:        return D3D11_BLEND_OP_SUBTRACT;
        case BlendEquation::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendEquation::Min:             return D3D11_BLEND_OP_MIN;
        case BlendEquation::Max:             return D3D11_BLEND_OP_MAX;
    }
    DMGE_CORE_ASSERT(false, "Unknown BlendEquation!");
    return D3D11_BLEND_OP_ADD;
}

D3D11_COMPARISON_FUNC DepthFuncToD3D(DepthFunc func)
{
    switch (func)
    {
        case DepthFunc::Never:         return D3D11_COMPARISON_NEVER;
        case DepthFunc::Less:          return D3D11_COMPARISON_LESS;
        case DepthFunc::Equal:         return D3D11_COMPARISON_EQUAL;
        case DepthFunc::LessEqual:     return D3D11_COMPARISON_LESS_EQUAL;
        case DepthFunc::Greater:       return D3D11_COMPARISON_GREATER;
        case DepthFunc::NotEqual:      return D3D11_COMPARISON_NOT_EQUAL;
        case DepthFunc::GreaterEqual:  return D3D11_COMPARISON_GREATER_EQUAL;
        case DepthFunc::Always:        return D3D11_COMPARISON_ALWAYS;
    }
    DMGE_CORE_ASSERT(false, "Unknown DepthFunc!");
    return D3D11_COMPARISON_LESS;
}

D3D11_CULL_MODE CullModeToD3D(CullMode mode)
{
    switch (mode)
    {
        case CullMode::None: return D3D11_CULL_NONE;
        case CullMode::Front: return D3D11_CULL_FRONT;
        case CullMode::Back:  return D3D11_CULL_BACK;
        case CullMode::FrontAndBack:
            // D3D11 cannot cull both faces; degrade to no culling with a
            // warning (documented deviation, DIRECTX_BACKEND_DESIGN.md 3.1).
            DMGE_LOG_WARN("[D3D11] CullMode::FrontAndBack unsupported, falling back to CULL_NONE");
            return D3D11_CULL_NONE;
    }
    DMGE_CORE_ASSERT(false, "Unknown CullMode!");
    return D3D11_CULL_NONE;
}

// FNV-1a over a POD description for the state-object caches.
template <typename T>
uint32_t HashDesc(const T& desc)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(&desc);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

} // anonymous namespace


// ── Lifecycle ────────────────────────────────────────────────────

D3D11RendererAPI::~D3D11RendererAPI()
{
    D3D11Backend::ClearDeviceContext();
}

void D3D11RendererAPI::Init(const RendererAPIInitConfig& config)
{
    // ── Adopt an existing window-facing device ───────────────────
    // When a DirectXGraphicsContext already ran Init() (the normal
    // Application flow: window first, Renderer::Init() second), its device
    // and swapchain are registered process-wide and this backend must use
    // THAT device - a second device could not present to the swapchain.
    if (D3D11Backend::Device())
    {
        m_Device  = D3D11Backend::Device();
        m_Context = D3D11Backend::Context();
        std::memcpy(m_DriverTypeName, "Windowed", sizeof("Windowed"));
        DMGE_LOG_INFO("[D3D11] RendererAPI adopted the window's device (feature level 0x{0:x})",
                      static_cast<unsigned>(m_Device->GetFeatureLevel()));
    }
    else
    {
    // Headless-capable device creation: hardware adapter first, then the
    // software rasterizers, so CI/agent sandboxes without a GPU (or without
    // an interactive desktop session - see kb/KB-07 K-014 for why Vulkan
    // cannot do this) still get a fully functional device.
    static const D3D_DRIVER_TYPE kDriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    static const char* kDriverTypeNames[] = { "Hardware", "WARP", "Reference" };

    constexpr UINT kDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = E_FAIL;
    for (size_t i = 0; i < std::size(kDriverTypes); ++i)
    {
        hr = D3D11CreateDevice(nullptr, kDriverTypes[i], nullptr, kDeviceFlags,
                               nullptr, 0, D3D11_SDK_VERSION,
                               &m_Device, nullptr, &m_Context);
        if (SUCCEEDED(hr))
        {
            const size_t nameLen = std::strlen(kDriverTypeNames[i]);
            const size_t maxLen  = sizeof(m_DriverTypeName) - 1;
            std::memcpy(m_DriverTypeName, kDriverTypeNames[i],
                        (nameLen < maxLen) ? nameLen : maxLen);
            m_DriverTypeName[(nameLen < maxLen) ? nameLen : maxLen] = '\0';
            break;
        }
    }
    DMGE_D3D_CHECK(hr, "D3D11CreateDevice");
    if (FAILED(hr))
        return;

    DMGE_LOG_INFO("[D3D11] Device created (driver: {0}, feature level 0x{1:x})",
                  m_DriverTypeName,
                  static_cast<unsigned>(m_Device->GetFeatureLevel()));
    D3D11Backend::SetDeviceContext(m_Device.Get(), m_Context.Get());
    }

    // Apply the requested initial pipeline state through the virtual setters
    // (base implementation mirrors the OpenGL/Vulkan backends).
    RendererAPI::Init(config);
}


// ── Clear / viewport ─────────────────────────────────────────────

void D3D11RendererAPI::SetClearColor(const glm::vec4& color)
{
    m_ClearColor = color;
}

void D3D11RendererAPI::Clear()
{
    if (m_Context)
    {
        if (m_CurrentRTV)
            m_Context->ClearRenderTargetView(m_CurrentRTV, &m_ClearColor[0]);
        if (m_CurrentDSV)
            m_Context->ClearDepthStencilView(m_CurrentDSV,
                                             D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                             1.0f, 0);
    }
}

void D3D11RendererAPI::SetViewport(int x, int y, int width, int height)
{
    if (!m_Context || width <= 0 || height <= 0)
        return;

    m_Viewport.TopLeftX = static_cast<float>(x);
    m_Viewport.TopLeftY = static_cast<float>(y);
    m_Viewport.Width    = static_cast<float>(width);
    m_Viewport.Height   = static_cast<float>(height);
    m_Viewport.MinDepth = 0.0f;
    m_Viewport.MaxDepth = 1.0f;
    m_Context->RSSetViewports(1, &m_Viewport);
}


// ── Render pass / target ─────────────────────────────────────────

void D3D11RendererAPI::BeginRenderPass(FrameBuffer* target)
{
    // nullptr / swapchain target: bind the window-facing backbuffer + depth
    // view registered by DirectXGraphicsContext (stage B swapchain flow).
    // Headless runs (no window target registered) keep the stage-A behavior:
    // just drop the current targets.
    if (!target || target->GetSpecification().SwapChainTarget)
    {
        m_CurrentRTV = D3D11Backend::WindowRenderTargetView();
        m_CurrentDSV = D3D11Backend::WindowDepthStencilView();
        if (m_Context && m_CurrentRTV)
        {
            m_Context->OMSetRenderTargets(1, &m_CurrentRTV, m_CurrentDSV);
            // Mirror the OpenGL default-framebuffer loadOp=CLEAR semantics:
            // the host's ClearFrame() ran with nothing bound (no-op on
            // D3D11), so each pass opens clean here.
            Clear();
        }
        return;
    }

    auto* fb = dynamic_cast<D3D11FrameBuffer*>(target);
    if (!fb)
    {
        DMGE_LOG_ERROR("[D3D11] BeginRenderPass target is not a D3D11FrameBuffer");
        return;
    }

    m_CurrentRTV = fb->GetRenderTargetView();
    m_CurrentDSV = fb->GetDepthStencilView();
    if (m_Context)
        m_Context->OMSetRenderTargets(1, &m_CurrentRTV, m_CurrentDSV);

    // Mirror the OpenGL backend's loadOp=CLEAR semantics: a freshly bound
    // offscreen target starts the pass clean.
    Clear();
}

void D3D11RendererAPI::EndRenderPass()
{
    if (m_Context && m_CurrentRTV)
    {
        ID3D11RenderTargetView* nullRTV = nullptr;
        m_Context->OMSetRenderTargets(1, &nullRTV, nullptr);
    }
    m_CurrentRTV = nullptr;
    m_CurrentDSV = nullptr;
}


// ── Draw calls ───────────────────────────────────────────────────

void D3D11RendererAPI::DrawIndexed(const VertexArray& vertexArray)
{
    // GL glUniform timing: uniforms set after shader->Bind() take effect on
    // the next draw (engine contract, RenderQueue flush order).
    D3D11Shader::UploadBoundStaging();

    vertexArray.Bind();

    const auto& indexBuffer = vertexArray.GetIndexBuffer();
    DMGE_CORE_ASSERT(indexBuffer, "VertexArray has no IndexBuffer attached!");

    if (m_Context)
        m_Context->DrawIndexed(indexBuffer->GetCount(), 0, 0);
}

void D3D11RendererAPI::DrawIndexedInstanced(const VertexArray& vertexArray,
                                            uint32_t instanceCount,
                                            uint32_t baseInstance)
{
    D3D11Shader::UploadBoundStaging();

    vertexArray.Bind();

    const auto& indexBuffer = vertexArray.GetIndexBuffer();
    DMGE_CORE_ASSERT(indexBuffer, "VertexArray has no IndexBuffer attached!");

    if (m_Context)
        m_Context->DrawIndexedInstanced(indexBuffer->GetCount(), instanceCount,
                                        0, 0, baseInstance);
}


// ── Pipeline state ───────────────────────────────────────────────

void D3D11RendererAPI::ApplyBlendState()
{
    if (!m_Context)
        return;

    D3D11_BLEND_DESC desc{};
    desc.AlphaToCoverageEnable                 = FALSE;
    desc.IndependentBlendEnable                = FALSE;
    desc.RenderTarget[0].BlendEnable           = m_BlendEnabled ? TRUE : FALSE;
    desc.RenderTarget[0].SrcBlend              = BlendFactorToD3D(m_SrcBlend);
    desc.RenderTarget[0].DestBlend             = BlendFactorToD3D(m_DstBlend);
    desc.RenderTarget[0].BlendOp               = BlendEquationToD3D(m_BlendEquation);
    desc.RenderTarget[0].SrcBlendAlpha         = desc.RenderTarget[0].SrcBlend;
    desc.RenderTarget[0].DestBlendAlpha        = desc.RenderTarget[0].DestBlend;
    desc.RenderTarget[0].BlendOpAlpha          = desc.RenderTarget[0].BlendOp;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    const uint32_t key = HashDesc(desc);
    auto it = m_BlendStates.find(key);
    if (it == m_BlendStates.end())
    {
        Microsoft::WRL::ComPtr<ID3D11BlendState> state;
        HRESULT hr = m_Device->CreateBlendState(&desc, &state);
        DMGE_D3D_CHECK(hr, "CreateBlendState");
        it = m_BlendStates.emplace(key, std::move(state)).first;
    }

    static const float kBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_Context->OMSetBlendState(it->second.Get(), kBlendFactor, 0xffffffffu);
}

void D3D11RendererAPI::ApplyDepthStencilState()
{
    if (!m_Context)
        return;

    D3D11_DEPTH_STENCIL_DESC desc{};
    desc.DepthEnable      = m_DepthTest ? TRUE : FALSE;
    desc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc        = DepthFuncToD3D(m_DepthFunc);
    desc.StencilEnable    = FALSE;
    desc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
    desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

    const uint32_t key = HashDesc(desc);
    auto it = m_DepthStates.find(key);
    if (it == m_DepthStates.end())
    {
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
        HRESULT hr = m_Device->CreateDepthStencilState(&desc, &state);
        DMGE_D3D_CHECK(hr, "CreateDepthStencilState");
        it = m_DepthStates.emplace(key, std::move(state)).first;
    }

    m_Context->OMSetDepthStencilState(it->second.Get(), 0);
}

void D3D11RendererAPI::ApplyRasterizerState()
{
    if (!m_Context)
        return;

    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = CullModeToD3D(m_CullMode);
    // OpenGL default: CCW is front. D3D11 defaults to CW-front, so flip it
    // to keep vertex winding semantics identical across backends.
    desc.FrontCounterClockwise = TRUE;
    desc.DepthBias             = 0;
    desc.DepthBiasClamp        = 0.0f;
    desc.SlopeScaledDepthBias  = 0.0f;
    desc.DepthClipEnable       = TRUE;
    desc.MultisampleEnable     = FALSE;
    desc.ScissorEnable         = FALSE;
    desc.AntialiasedLineEnable = FALSE;

    const uint32_t key = HashDesc(desc);
    auto it = m_RasterStates.find(key);
    if (it == m_RasterStates.end())
    {
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> state;
        HRESULT hr = m_Device->CreateRasterizerState(&desc, &state);
        DMGE_D3D_CHECK(hr, "CreateRasterizerState");
        it = m_RasterStates.emplace(key, std::move(state)).first;
    }

    m_Context->RSSetState(it->second.Get());
}

void D3D11RendererAPI::SetBlendState(bool enable, BlendFactor srcFactor, BlendFactor dstFactor)
{
    m_BlendEnabled = enable;
    m_SrcBlend     = srcFactor;
    m_DstBlend     = dstFactor;
    ApplyBlendState();
}

void D3D11RendererAPI::SetBlendEquation(BlendEquation equation)
{
    m_BlendEquation = equation;
    ApplyBlendState();
}

void D3D11RendererAPI::SetDepthTest(bool enable)
{
    m_DepthTest = enable;
    ApplyDepthStencilState();
}

void D3D11RendererAPI::SetDepthFunc(DepthFunc func)
{
    m_DepthFunc = func;
    ApplyDepthStencilState();
}

void D3D11RendererAPI::SetCullMode(CullMode mode)
{
    m_CullMode = mode;
    ApplyRasterizerState();
}

} // namespace DMGameEngine
