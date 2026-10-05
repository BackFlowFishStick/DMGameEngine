/*
 * DMGameEngine - Renderer API Abstraction
 *
 * Low-level backend interface for graphics-API-specific rendering
 * commands (draw calls, clear, viewport, ...). Platform backends
 * (OpenGL, Vulkan, DirectX) derive from this and provide their own
 * implementations.
 *
 * The active backend is created via the static Create() factory, which
 * selects the correct implementation based on Renderer::GetAPI().
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Renderer/RenderPassDesc.h"
#include "glm/glm.hpp"
#include <memory>
#include <cstdint>

namespace DMGameEngine {

class VertexArray; // forward declaration
class FrameBuffer; // forward declaration (render-target pass)

// ── Blend Factor ─────────────────────────────────────────────
enum class BlendFactor : uint8_t
{
    Zero = 0,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
};

// ── Blend Equation ───────────────────────────────────────────
enum class BlendEquation : uint8_t
{
    Add = 0,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

// ── Depth Function ───────────────────────────────────────────
enum class DepthFunc : uint8_t
{
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

// ── Cull Mode ────────────────────────────────────────────────
enum class CullMode : uint8_t
{
    None = 0,
    Front,
    Back,
    FrontAndBack,
};

// -- Init Configuration ------------------------------------------
// Bundle of initial pipeline-state values handed to RendererAPI::Init().
// Defaults mirror the previous hardcoded baseline, so call sites that
// pass no argument keep the same behavior.
struct RendererAPIInitConfig
{
    // Clear color written by Clear() (RGBA). Matches the GL default.
    glm::vec4 ClearColor{0.0f, 0.0f, 0.0f, 0.0f};

    // Depth test.
    bool DepthTestEnabled = true;
    DepthFunc DepthFunction = DepthFunc::Less;

    // Face culling.
    CullMode Culling = CullMode::None;

    // Blending.
    bool BlendEnabled = false;
    BlendFactor SrcBlendFactor = BlendFactor::SrcAlpha;
    BlendFactor DstBlendFactor = BlendFactor::OneMinusSrcAlpha;
    BlendEquation BlendEquationMode = BlendEquation::Add;
};

class DMGE_API RendererAPI
{
public:
    virtual ~RendererAPI() = default;

    virtual void Init(const RendererAPIInitConfig& config = {});

    virtual void SetClearColor(const glm::vec4& color) = 0;
    virtual void Clear() = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;

    // -- Render pass / target ----------------------------------------
    // Begins a render pass targeting target (nullptr = default /
    // swapchain). For Vulkan dynamic rendering this begins the pass;
    // for OpenGL it binds the framebuffer. EndRenderPass ends/unbinds.
    // A scene layer brackets its draws so it can render into an offscreen
    // FrameBuffer (render-to-texture, multi-pass).
    //
    // Legacy form, KEPT for call-site compatibility (2b stage 1): backends
    // route it through the desc form below (MakeRenderPassDescForTarget),
    // so each backend has exactly ONE consumption path. The desc form is
    // the primary entry: attachment formats/load ops, clear values,
    // viewport/scissor and the depth attachment come from the description
    // instead of backend-internal defaults. Backends that have not
    // migrated (DirectX stage B) fall back to the legacy form via the
    // default implementation.
    virtual void BeginRenderPass(FrameBuffer* target) = 0;
    virtual void BeginRenderPass(const RenderPassDesc& desc)
    {
        // Degrade to the legacy form: sufficient for backends whose pass
        // begin derives everything from the target alone (DirectX stage B).
        BeginRenderPass(desc.Target);
    }
    virtual void EndRenderPass() = 0;

    // The desc as ANNOTATED by the backend for the pass currently open
    // (or the last one begun): NdcZMin is filled per the backend's clip
    // convention (OpenGL -1 / Vulkan 0 / DirectX-family 0), everything
    // else mirrors what was passed to BeginRenderPass. Pure readback for
    // consumers like the deferred lighting pass's depth reprojection -
    // replaces Renderer-layer API branches (E1).
    virtual RenderPassDesc GetActiveRenderPassDesc() const
    {
        // Default: DirectX-family depth convention (backend-independent
        // neutral choice; OpenGL/Vulkan override with real snapshots).
        RenderPassDesc desc;
        desc.NdcZMin = 0.0f;
        return desc;
    }

    virtual void DrawIndexed(const VertexArray& vertexArray) = 0;
    // Instanced indexed draw: renders instanceCount copies, advancing
    // per-instance vertex attributes (BufferElement PerInstance) once per
    // instance. baseInstance offsets the starting instance index (ignored on
    // backends without base-instance support).
    virtual void DrawIndexedInstanced(const VertexArray& vertexArray,
                                      uint32_t instanceCount,
                                      uint32_t baseInstance = 0) = 0;

    // ── Pipeline state ───────────────────────────────────────
    //  Blend: toggles GL_BLEND and sets the RGB/alpha blend function.
    virtual void SetBlendState(bool enable,
                               BlendFactor srcFactor = BlendFactor::SrcAlpha,
                               BlendFactor dstFactor = BlendFactor::OneMinusSrcAlpha) = 0;
    virtual void SetBlendEquation(BlendEquation equation) = 0;

    //  Depth test: toggles GL_DEPTH_TEST and sets the depth comparison.
    virtual void SetDepthTest(bool enable) = 0;
    virtual void SetDepthFunc(DepthFunc func) = 0;

    //  Face culling: None disables culling; Front/Back/FrontAndBack
    //  enable GL_CULL_FACE and select the culled face(s).
    virtual void SetCullMode(CullMode mode) = 0;

    // ── Factory ─────────────────────────────────────────────────
    static DM::Scope<RendererAPI> Create();
};

} // namespace DMGameEngine