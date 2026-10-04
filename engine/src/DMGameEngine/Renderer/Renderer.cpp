/*
 * DMGameEngine - Renderer Implementation
 *
 * Scene-level surface: the host clears the framebuffer once per frame
 * (ClearFrame); each scene layer brackets its own render pass with
 * BeginScene / EndScene, caching the camera view-projection and
 * enqueuing draws into a RenderQueue. The queue is sorted by
 * material/shader and flushed at EndScene / Flush, binding each group's
 * state only once per pass instead of once per Submit.
 *
 * Render path (3e stage 1, documents/DEFERRED_RENDERING_DESIGN.md):
 * with RenderPath::Deferred the bracket is re-orchestrated inside this
 * file - BeginScene opens a pass on the internal G-buffer FrameBuffer,
 * EndScene flushes the queue through RenderQueue::FlushDeferred (all
 * draws rewritten to the G-buffer shaders), ends that pass, then opens
 * a pass on the real target and draws a full-screen lighting pass that
 * reads the G-buffer. Scene systems (MeshRenderSystem / LightSystem)
 * are path-agnostic: they keep Submit()-ing exactly as before.
 */

#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/RenderCommand.h"
#include "DMGameEngine/Renderer/FrameBuffer.h"
#include "DMGameEngine/Renderer/Camera.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/VertexBuffer.h"
#include "DMGameEngine/Renderer/IndexBuffer.h"
#include "DMGameEngine/Core/Log.h"

namespace DMGameEngine {

Renderer::API Renderer::s_API = Renderer::API::OpenGL;
Renderer::SceneData Renderer::s_SceneData;
RenderQueue Renderer::s_Queue;
SceneLightData Renderer::s_LightData;
DeferredPathState Renderer::s_PathState;

// ── Deferred-path resource bundle (file-static: R1 keeps the exported
//    Renderer class free of STL/implementation members) ──────────────
namespace {

struct DeferredResources
{
    bool Initialized = false;

    // G-buffer: RT0 albedo+spec, RT1 normal+shininess, depth attachment.
    // Formats come from GBufferLayout (single source of truth).
    DM::Ref<FrameBuffer> GBuffer;

    DeferredShaderSet Shaders;

    // Full-screen triangle the lighting pass draws (no index buffer).
    DM::Ref<VertexArray> Quad;

    // The real output target of the current deferred frame (nullptr =
    // swapchain). Recorded by BeginScene, consumed by EndScene.
    FrameBuffer* Target = nullptr;
};

DeferredResources s_Deferred;

// Tracked window size, used when the deferred target is the swapchain
// (a SwapChainTarget FrameBuffer reports 0x0). Default mirrors the
// editor's initial 1280x720; Application keeps this current via
// Renderer::OnWindowResize.
uint32_t s_FrameWidth  = 1280;
uint32_t s_FrameHeight = 720;

} // anonymous namespace

void Renderer::Init(const RendererAPIInitConfig& config)
{
    RenderCommand::Init(config);

    const char* apiName = "Unknown";
    switch (s_API)
    {
        case API::OpenGL:  apiName = "OpenGL";  break;
        case API::Vulkan:  apiName = "Vulkan";   break;
        case API::DirectX: apiName = "DirectX";  break;
        case API::None:    apiName = "None";     break;
    }
    DMGE_LOG_INFO("Renderer initialized with API: {0}", apiName);
}

void Renderer::Shutdown()
{
    DestroyDeferredResources();
    RenderCommand::Shutdown();
}

void Renderer::ClearFrame()
{
    RenderCommand::Clear();
}

void Renderer::SetRenderPath(RenderPath path)
{
    s_PathState.Request(path);
    DMGE_LOG_INFO("Render path set to {0} (effective next BeginScene).",
                  path == RenderPath::Deferred ? "Deferred" : "Forward");
}

void Renderer::BeginScene(const DM::Ref<FrameBuffer>& target)
{
    s_PathState.OnBeginScene();
    s_SceneData.ViewProjectionMatrix = glm::mat4(1.0f);
    s_SceneData.CameraPosition = glm::vec3(0.0f);
    s_LightData.Clear();
    s_Queue.Clear();

    if (s_PathState.IsDeferredFrame())
    {
        // Identity view-projection cannot produce a meaningful G-buffer
        // (no camera, no projection): fall back to forward for this frame.
        DMGE_LOG_WARN("Deferred path requested without a camera - falling back to Forward for this frame.");
        s_PathState.Active = RenderPath::Forward;
    }
    RenderCommand::BeginRenderPass(target.get());
}

void Renderer::BeginScene(const Camera& camera)
{
    // Offscreen path "just configure the camera": render into the camera's
    // own render target when one is set, otherwise the swapchain (nullptr).
    BeginScene(camera, camera.GetRenderTarget());
}

void Renderer::BeginScene(const Camera& camera, const DM::Ref<FrameBuffer>& target)
{
    s_PathState.OnBeginScene();
    s_SceneData.ViewProjectionMatrix = camera.GetViewProjection();
    s_SceneData.CameraPosition = glm::vec3(glm::inverse(camera.GetView())[3]);
    if (s_API == API::Vulkan)
    {
        // Vulkan clip space Y points down (OpenGL Y points up), so an
        // OpenGL-convention projection renders upside down. Negate the
        // clip-space Y here; the matching frontFace flip in the Vulkan
        // pipeline keeps back-face culling behaving as under OpenGL.
        glm::mat4 flipY(1.0f);
        flipY[1][1] = -1.0f;
        s_SceneData.ViewProjectionMatrix = flipY * s_SceneData.ViewProjectionMatrix;
    }
    s_LightData.Clear();
    s_Queue.Clear();

    if (s_PathState.IsDeferredFrame())
    {
        // G-buffer size: the explicit target's size, or the tracked window
        // size when rendering to the swapchain (SwapChainTarget FrameBuffers
        // report 0x0).
        uint32_t w = 0, h = 0;
        if (target && !target->GetSpecification().SwapChainTarget)
        {
            w = target->GetWidth();
            h = target->GetHeight();
        }
        else
        {
            w = s_FrameWidth;
            h = s_FrameHeight;
        }

        if (w == 0 || h == 0)
        {
            DMGE_LOG_WARN("Deferred path requested with a 0x0 render target - "
                          "falling back to Forward for this frame.");
            s_PathState.Active = RenderPath::Forward;
        }
        else
        {
            EnsureDeferredResources(w, h);
            s_Deferred.Target = target.get();
            // G-buffer pass: the queue's draws are rewritten to the
            // G-buffer shaders at EndScene; the real target pass opens
            // there too (after the lighting input exists).
            RenderCommand::BeginRenderPass(s_Deferred.GBuffer.get());
            return;
        }
    }

    s_Deferred.Target = nullptr;
    RenderCommand::BeginRenderPass(target.get());
}

void Renderer::EndScene()
{
    if (s_PathState.IsDeferredFrame() && s_Deferred.Initialized && s_Deferred.GBuffer)
    {
        // ── 1) G-buffer flush ────────────────────────────────────
        // Explicit pipeline state: the G-buffer pass is opaque-only (MRT +
        // blending has no well-defined semantics; see design doc section 6)
        // and needs the depth test for depth prestorage.
        RenderCommand::SetDepthTest(true);
        RenderCommand::SetDepthFunc(DepthFunc::Less);
        RenderCommand::SetBlendState(false);
        s_Queue.FlushDeferred(s_SceneData.ViewProjectionMatrix, s_Deferred.Shaders);
        RenderCommand::EndRenderPass();   // end G-buffer pass (attachments -> sampleable)

        // ── 2) Lighting pass into the real target ───────────────
        RenderCommand::BeginRenderPass(s_Deferred.Target);
        DrawDeferredLighting();
        RenderCommand::EndRenderPass();
        s_Deferred.Target = nullptr;

        // Leave the depth state on for whoever renders next (other layers,
        // ImGui draw with their own state, but keep the default sane).
        RenderCommand::SetDepthTest(true);
        return;
    }

    Flush();
    RenderCommand::EndRenderPass();
}

// ── Deferred resources ────────────────────────────────────────────

void Renderer::EnsureDeferredResources(uint32_t width, uint32_t height)
{
    if (!s_Deferred.Initialized)
    {
        // G-buffer FrameBuffer: 2 color attachments + depth (GBufferLayout).
        FramebufferSpecification spec;
        spec.Width  = width;
        spec.Height = height;
        spec.Attachments.push_back({ GBufferLayout::AlbedoSpecFormat() });
        spec.Attachments.push_back({ GBufferLayout::NormalShininessFormat() });
        spec.DepthFormat = GBufferLayout::DepthFormat();
        s_Deferred.GBuffer = FrameBuffer::Create(spec);

        // Internal shaders: created from embedded sources (no CWD/filesystem
        // dependency for the engine DLL); engine/shaders/*.glsl are the
        // reference copies. Created AFTER the API backend exists - this runs
        // inside the first deferred frame, so the context is live.
        s_Deferred.Shaders.Static = Shader::Create(
            "DMGE_GBuffer",
            DeferredShaderSources::GBufferVS(),
            DeferredShaderSources::GBufferFS());
        s_Deferred.Shaders.Skinned = Shader::Create(
            "DMGE_GBufferSkinned",
            DeferredShaderSources::GBufferSkinnedVS(),
            DeferredShaderSources::GBufferSkinnedFS());
        s_Deferred.Shaders.Instanced = Shader::Create(
            "DMGE_GBufferInstanced",
            DeferredShaderSources::GBufferInstancedVS(),
            DeferredShaderSources::GBufferInstancedFS());
        s_Deferred.Shaders.Lighting = Shader::Create(
            "DMGE_DeferredLighting",
            DeferredShaderSources::DeferredLightingVS(),
            DeferredShaderSources::DeferredLightingFS());

        // Full-screen triangle (3 vertices, NDC, no index buffer needed by
        // the shader - but DrawIndexed requires one, so a trivial 0-1-2).
        s_Deferred.Quad = VertexArray::Create();
        auto vb = VertexBuffer::Create(kFullscreenTri,
                                       static_cast<uint32_t>(sizeof(kFullscreenTri)));
        vb->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float2, "a_TexCoords" },
        });
        s_Deferred.Quad->AddVertexBuffer(vb);
        constexpr uint32_t kQuadIndices[3] = { 0, 1, 2 };
        auto ib = IndexBuffer::Create(kQuadIndices, 3);
        s_Deferred.Quad->SetIndexBuffer(ib);

        s_Deferred.Initialized = true;
        DMGE_LOG_INFO("Deferred rendering resources created (G-buffer {}x{}, "
                      "RT0 RGBA8 albedo+spec, RT1 RGBA16F normal+shininess).",
                      width, height);
    }

    // Follow the target's size (framebuffer Resize re-creates attachments).
    if (ShouldRebuildGBuffer(s_Deferred.GBuffer->GetWidth(), s_Deferred.GBuffer->GetHeight(),
                             width, height))
    {
        s_Deferred.GBuffer->Resize(width, height);
    }
}

void Renderer::DrawDeferredLighting()
{
    const auto& lighting = s_Deferred.Shaders.Lighting;
    DMGE_CORE_ASSERT(lighting, "DrawDeferredLighting - lighting shader missing!");

    lighting->Bind();

    // Depth reprojection inputs: the inverse of the EXACT view-projection
    // the G-buffer pass used (self-consistent with the Vulkan flipY), and
    // the per-API NDC z minimum (GL -1 / Vulkan 0; 2b debt, see design doc).
    lighting->SetMat4("u_InverseViewProjection", glm::inverse(s_SceneData.ViewProjectionMatrix));
    lighting->SetFloat("u_NdcZMin", s_API == API::Vulkan ? 0.0f : -1.0f);
    lighting->SetFloat4("u_SkyColor", RenderCommand::GetClearColor());

    // Lighting uniforms: identical names to the forward path, shared upload.
    UploadSceneLighting(lighting, s_SceneData.CameraPosition, s_LightData);

    // Bind the G-buffer attachments as textures (slots from GBufferLayout).
    auto albedoSpec = s_Deferred.GBuffer->GetColorAttachment(GBufferLayout::kAlbedoSpecSlot);
    auto normalShininess = s_Deferred.GBuffer->GetColorAttachment(GBufferLayout::kNormalShininessSlot);
    auto depth = s_Deferred.GBuffer->GetDepthAttachment();
    DMGE_CORE_ASSERT(albedoSpec && normalShininess && depth,
                     "DrawDeferredLighting - G-buffer attachments missing!");

    albedoSpec->Bind(GBufferLayout::kAlbedoSpecSlot);
    lighting->SetInt(GBufferLayout::kAlbedoSpecSampler,
                     static_cast<int>(GBufferLayout::kAlbedoSpecSlot));
    normalShininess->Bind(GBufferLayout::kNormalShininessSlot);
    lighting->SetInt(GBufferLayout::kNormalShininessSampler,
                     static_cast<int>(GBufferLayout::kNormalShininessSlot));
    depth->Bind(GBufferLayout::kDepthSlot);
    lighting->SetInt(GBufferLayout::kDepthSampler,
                     static_cast<int>(GBufferLayout::kDepthSlot));

    // The quad covers the screen; depth testing is irrelevant (and the
    // G-buffer already holds the depth) - draw unconditionally.
    RenderCommand::SetDepthTest(false);
    RenderCommand::DrawIndexed(*s_Deferred.Quad);
    RenderCommand::SetDepthTest(true);
}

void Renderer::DestroyDeferredResources()
{
    // Also drains the target pointer: a stale FrameBuffer* across API
    // switches would dangle once the backend dies.
    s_Deferred.Target = nullptr;
    if (!s_Deferred.Initialized)
        return;

    // Drop the GPU objects in dependency order (shaders/VA/FBO). Callers
    // guarantee a live context (SetAPI / Shutdown happen while it exists).
    s_Deferred.Quad.reset();
    s_Deferred.Shaders = {};
    s_Deferred.GBuffer.reset();
    s_Deferred.Initialized = false;
    DMGE_LOG_INFO("Deferred rendering resources released.");
}

void Renderer::Submit(const DM::Ref<Shader>& shader,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(shader, "Renderer::Submit - shader is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::Submit - vertexArray is null!");
    s_Queue.Submit(shader, vertexArray, transform);
}

void Renderer::Submit(const DM::Ref<Material>& material,
                      const DM::Ref<VertexArray>& vertexArray,
                      const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(material, "Renderer::Submit - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::Submit - vertexArray is null!");
    s_Queue.Submit(material, vertexArray, transform);
}

void Renderer::SubmitSkinned(const DM::Ref<Material>& material,
                             const DM::Ref<VertexArray>& vertexArray,
                             const glm::mat4& transform,
                             const glm::mat4* bonePalette, uint32_t bonePaletteCount)
{
    DMGE_CORE_ASSERT(material, "Renderer::SubmitSkinned - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::SubmitSkinned - vertexArray is null!");
    DMGE_CORE_ASSERT(bonePalette && bonePaletteCount > 0,
                     "Renderer::SubmitSkinned - palette is empty!");
    s_Queue.Submit(material, vertexArray, transform, bonePalette, bonePaletteCount);
}

void Renderer::SubmitInstanced(const DM::Ref<Material>& material,
                                const DM::Ref<VertexArray>& vertexArray,
                                uint32_t instanceCount)
{
    DMGE_CORE_ASSERT(material, "Renderer::SubmitInstanced - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "Renderer::SubmitInstanced - vertexArray is null!");
    s_Queue.SubmitInstanced(material, vertexArray, instanceCount);
}

void Renderer::Flush()
{
    s_Queue.Flush(s_SceneData.ViewProjectionMatrix, s_SceneData.CameraPosition, s_LightData);
}

void Renderer::SubmitLightData(const SceneLightData& data)
{
    s_LightData = data;
}

void Renderer::OnWindowResize(int width, int height)
{
    s_FrameWidth  = (width  > 0) ? static_cast<uint32_t>(width)  : s_FrameWidth;
    s_FrameHeight = (height > 0) ? static_cast<uint32_t>(height) : s_FrameHeight;
    RenderCommand::SetViewport(0, 0, width, height);
}

} // namespace DMGameEngine
