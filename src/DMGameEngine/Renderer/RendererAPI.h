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
#include "glm/glm.hpp"
#include <memory>
#include <cstdint>

namespace DMGameEngine {

class VertexArray; // forward declaration

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

    virtual void DrawIndexed(const VertexArray& vertexArray) = 0;

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