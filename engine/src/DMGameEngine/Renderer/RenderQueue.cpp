/*
 * DMGameEngine - Render Queue Implementation
 *
 * Deferred submission: renderables are accumulated by Submit() and
 * committed once by Flush(), which sorts by material/shader so each
 * group binds its state only once and uploads the view-projection,
 * camera position, and lighting data once per shader instead of once
 * per draw.
 *
 * Lighting stage A+B: Flush also uploads per-draw u_NormalMatrix
 * (computed from the inverse-transpose of the model matrix) and
 * per-group lighting uniforms (camera position, directional/point/spot
 * lights, ambient). Shaders that don't declare these uniforms get -1
 * from glGetUniformLocation and the glUniform* calls are silently
 * ignored (see OpenGLShader::GetUniformLocation).
 *
 * Instanced path: SubmitInstanced enqueues a batch that Flush draws
 * with DrawIndexedInstanced. Uses a fixed-capacity array instead of
 * std::vector to avoid DLL-boundary vector state issues.
 */

#include "DMGameEngine/Renderer/RenderQueue.h"
#include "DMGameEngine/Renderer/DeferredRendering.h"  // DeferredShaderSet + shared UploadSceneLighting
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/Shader.h"
#include "DMGameEngine/Renderer/Texture.h"
#include "DMGameEngine/Renderer/VertexArray.h"
#include "DMGameEngine/Renderer/RenderCommand.h"
#include "DMGameEngine/Renderer/Light.h"
#include "DMGameEngine/Core/Log.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace DMGameEngine {

namespace {

// UploadSceneLighting moved to DeferredRendering.h (inline) so the deferred
// lighting pass in Renderer.cpp shares the exact same per-name uploads.

// Copy a queued Material's Blinn-Phong parameter values onto a G-buffer
// shader (deferred flush). Reads the well-known uniform names by value off
// the Material (Get() returns nullptr when unset -> keep the shader default).
// The albedo texture, when present, is bound to slot 0. 2b debt: no shader
// reflection - the names are the Blinn-Phong contract (see design doc §7).
void ApplyGBufferMaterial(const DM::Ref<Shader>& shader,
                          const DM::Ref<Material>& material)
{
    if (!material)
        return;

    auto setFloat = [&](std::string_view name) {
        if (const UniformValue* v = material->Get(name))
            if (const float* f = std::get_if<float>(v))
                shader->SetFloat(name, *f);
    };
    auto setFloat3 = [&](std::string_view name) {
        if (const UniformValue* v = material->Get(name))
            if (const glm::vec3* f = std::get_if<glm::vec3>(v))
                shader->SetFloat3(name, *f);
    };
    auto setInt = [&](std::string_view name) {
        if (const UniformValue* v = material->Get(name))
            if (const int* i = std::get_if<int>(v))
                shader->SetInt(name, *i);
    };

    setFloat3("u_AlbedoColor");
    setFloat("u_SpecularStrength");
    setFloat("u_Shininess");
    setInt("u_UseTexture");

    if (const TextureSlot* slot = material->GetTexture("u_AlbedoTexture"))
    {
        if (slot->Texture)
        {
            slot->Texture->Bind(0);
            shader->SetInt("u_AlbedoTexture", 0);
        }
    }
}

} // anonymous namespace

// ── Fixed-capacity instanced batch storage ──────────────────────
// A simple fixed array avoids std::vector DLL-boundary state issues
// (the vector object lives in the DLL but is modified from the exe
// via MeshRenderSystem; iterator debugging level mismatches can
// corrupt the vector's internal bookkeeping across the boundary).
static constexpr uint32_t kMaxInstancedBatches = 32;
static InstancedRenderable s_InstancedBatches[kMaxInstancedBatches];
static uint32_t s_InstancedBatchCount = 0;

void RenderQueue::Clear()
{
    m_Queue.clear();
    s_InstancedBatchCount = 0;
}

void RenderQueue::Submit(const DM::Ref<Material>& material,
                         const DM::Ref<VertexArray>& vertexArray,
                         const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(material, "RenderQueue::Submit - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "RenderQueue::Submit - vertexArray is null!");
    m_Queue.push_back({ material, nullptr, vertexArray, transform, nullptr, 0 });
}

void RenderQueue::Submit(const DM::Ref<Material>& material,
                         const DM::Ref<VertexArray>& vertexArray,
                         const glm::mat4& transform,
                         const glm::mat4* bonePalette, uint32_t bonePaletteCount)
{
    DMGE_CORE_ASSERT(material, "RenderQueue::Submit - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "RenderQueue::Submit - vertexArray is null!");
    m_Queue.push_back({ material, nullptr, vertexArray, transform,
                        bonePalette, bonePaletteCount });
}

void RenderQueue::Submit(const DM::Ref<Shader>& shader,
                         const DM::Ref<VertexArray>& vertexArray,
                         const glm::mat4& transform)
{
    DMGE_CORE_ASSERT(shader, "RenderQueue::Submit - shader is null!");
    DMGE_CORE_ASSERT(vertexArray, "RenderQueue::Submit - vertexArray is null!");
    m_Queue.push_back({ nullptr, shader, vertexArray, transform });
}

void RenderQueue::SubmitInstanced(const DM::Ref<Material>& material,
                                  const DM::Ref<VertexArray>& vertexArray,
                                  uint32_t instanceCount)
{
    DMGE_CORE_ASSERT(material, "RenderQueue::SubmitInstanced - material is null!");
    DMGE_CORE_ASSERT(vertexArray, "RenderQueue::SubmitInstanced - vertexArray is null!");
    DMGE_CORE_ASSERT(instanceCount > 0, "RenderQueue::SubmitInstanced - instanceCount is 0!");
    DMGE_CORE_ASSERT(s_InstancedBatchCount < kMaxInstancedBatches,
                     "RenderQueue::SubmitInstanced - too many instanced batches!");
    s_InstancedBatches[s_InstancedBatchCount] = { material, vertexArray, instanceCount };
    ++s_InstancedBatchCount;
}

void RenderQueue::Flush(const glm::mat4& viewProjection,
                        const glm::vec3& cameraPosition,
                        const SceneLightData& lightData)
{
    // Sort renderables so those sharing the same Material/Shader become
    // contiguous. Materials sort before shader-only draws (tag 0 vs 1),
    // then by object identity to keep each group together.
    auto sortKey = [](const Renderable& r) {
        const bool hasMaterial = static_cast<bool>(r.Material);
        const void* obj = hasMaterial
            ? static_cast<const void*>(r.Material.get())
            : static_cast<const void*>(r.Shader.get());
        return std::pair<uint32_t, uintptr_t>{
            hasMaterial ? 0u : 1u,
            reinterpret_cast<uintptr_t>(obj)
        };
    };

    std::sort(m_Queue.begin(), m_Queue.end(),
        [&](const Renderable& a, const Renderable& b) {
            return sortKey(a) < sortKey(b);
        });

    const Material* lastMaterial = nullptr;
    const Shader*   lastShader   = nullptr;

    // ── Per-draw queue ──────────────────────────────────────────
    for (const auto& r : m_Queue)
    {
        const DM::Ref<Shader>& shader = r.Material
            ? r.Material->GetShader()
            : r.Shader;

        DMGE_CORE_ASSERT(shader, "RenderQueue::Flush - renderable has no shader!");
        DMGE_CORE_ASSERT(r.VertexArray, "RenderQueue::Flush - vertexArray is null!");

        if (r.Material)
        {
            if (r.Material.get() != lastMaterial)
            {
                r.Material->Bind();
                shader->SetMat4("u_ViewProjection", viewProjection);
                UploadSceneLighting(shader, cameraPosition, lightData);
                lastMaterial = r.Material.get();
                lastShader   = shader.get();
            }
        }
        else
        {
            if (shader.get() != lastShader)
            {
                shader->Bind();
                shader->SetMat4("u_ViewProjection", viewProjection);
                UploadSceneLighting(shader, cameraPosition, lightData);
                lastShader = shader.get();
            }
        }

        // Per-draw uniforms: model transform + normal matrix.
        shader->SetMat4("u_Transform", r.Transform);

        glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(r.Transform)));
        glm::mat4 normalMat4(
            glm::vec4(normalMat3[0], 0.0f),
            glm::vec4(normalMat3[1], 0.0f),
            glm::vec4(normalMat3[2], 0.0f),
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        shader->SetMat4("u_NormalMatrix", normalMat4);

        // Skinning (animation stage 1): per-draw bone palette. Non-skinned
        // shaders don't declare u_BoneMatrices -> location -1 -> no-op, and
        // static draws carry a null palette anyway. Per-draw uniform (not a
        // descriptor/UBO) so it never touches the Vulkan descriptor cache
        // (kb/KB-07 K-009) on the future Vulkan path.
        if (r.BonePalette && r.BonePaletteCount > 0)
            shader->SetMat4Array("u_BoneMatrices", r.BonePalette, r.BonePaletteCount);

        RenderCommand::DrawIndexed(*r.VertexArray);
    }

    m_Queue.clear();

    // ── Instanced batches ──────────────────────────────────────
    // Each batch: bind material once, upload VP + lighting once,
    // then one DrawIndexedInstanced. No per-draw uniform uploads
    // (transforms are per-instance vertex attributes).
    const Material* lastInstancedMaterial = nullptr;
    for (uint32_t i = 0; i < s_InstancedBatchCount; ++i)
    {
        const auto& r = s_InstancedBatches[i];
        const DM::Ref<Shader>& shader = r.Material->GetShader();
        DMGE_CORE_ASSERT(shader, "RenderQueue::Flush - instanced renderable has no shader!");

        if (r.Material.get() != lastInstancedMaterial)
        {
            r.Material->Bind();
            shader->SetMat4("u_ViewProjection", viewProjection);
            UploadSceneLighting(shader, cameraPosition, lightData);
            lastInstancedMaterial = r.Material.get();
        }

        RenderCommand::DrawIndexedInstanced(*r.VertexArray, r.InstanceCount);
    }

    s_InstancedBatchCount = 0;
}

// ── Deferred-path flush (3e stage 1) ──────────────────────────────
// Rewrites every queued draw into the G-buffer: same material-grouped order
// as Flush, but the bound shader is always one of the internal G-buffer
// shaders and the material's Blinn-Phong parameters are copied onto it.
// No lighting uniforms here - the G-buffer pass stores geometry only; the
// lighting pass (Renderer::DrawDeferredLighting) evaluates the lights.
void RenderQueue::FlushDeferred(const glm::mat4& viewProjection,
                                const DeferredShaderSet& shaders)
{
    // Same sort key as Flush: material groups contiguous, shader-only draws
    // after materials.
    auto sortKey = [](const Renderable& r) {
        const bool hasMaterial = static_cast<bool>(r.Material);
        const void* obj = hasMaterial
            ? static_cast<const void*>(r.Material.get())
            : static_cast<const void*>(r.Shader.get());
        return std::pair<uint32_t, uintptr_t>{
            hasMaterial ? 0u : 1u,
            reinterpret_cast<uintptr_t>(obj)
        };
    };

    std::sort(m_Queue.begin(), m_Queue.end(),
        [&](const Renderable& a, const Renderable& b) {
            return sortKey(a) < sortKey(b);
        });

    const Shader*   lastShader   = nullptr;
    const Material* lastMaterial = nullptr;

    // ── Per-draw queue (static + skinned) ───────────────────────
    for (const auto& r : m_Queue)
    {
        DMGE_CORE_ASSERT(r.VertexArray, "RenderQueue::FlushDeferred - vertexArray is null!");

        // Skinned draws route to the skinned G-buffer shader so they can
        // receive the bone palette (u_BoneMatrices; OpenGL-only at this
        // stage - same restriction as the forward path).
        const DM::Ref<Shader>& shader =
            (r.BonePalette && r.BonePaletteCount > 0) ? shaders.Skinned
                                                      : shaders.Static;
        DMGE_CORE_ASSERT(shader, "RenderQueue::FlushDeferred - G-buffer shader missing!");

        if (shader.get() != lastShader)
        {
            shader->Bind();
            shader->SetMat4("u_ViewProjection", viewProjection);
            lastShader   = shader.get();
            lastMaterial = nullptr;   // new shader: reapply material params
        }

        if (r.Material)
        {
            // ApplyGBufferMaterial uploads every parameter (cheap per draw,
            // correct per material-instance). Track the pointer only to skip
            // repeats of the exact same material.
            if (r.Material.get() != lastMaterial)
            {
                ApplyGBufferMaterial(shader, r.Material);
                lastMaterial = r.Material.get();
            }
        }

        // Per-draw uniforms: model transform + normal matrix.
        shader->SetMat4("u_Transform", r.Transform);
        glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(r.Transform)));
        glm::mat4 normalMat4(
            glm::vec4(normalMat3[0], 0.0f),
            glm::vec4(normalMat3[1], 0.0f),
            glm::vec4(normalMat3[2], 0.0f),
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        shader->SetMat4("u_NormalMatrix", normalMat4);

        if (r.BonePalette && r.BonePaletteCount > 0)
            shader->SetMat4Array("u_BoneMatrices", r.BonePalette, r.BonePaletteCount);

        RenderCommand::DrawIndexed(*r.VertexArray);
    }

    m_Queue.clear();

    // ── Instanced batches ──────────────────────────────────────
    if (s_InstancedBatchCount == 0)
        return;
    DMGE_CORE_ASSERT(shaders.Instanced,
                     "RenderQueue::FlushDeferred - instanced G-buffer shader missing!");

    shaders.Instanced->Bind();
    shaders.Instanced->SetMat4("u_ViewProjection", viewProjection);

    const Material* lastInstancedMaterial = nullptr;
    for (uint32_t i = 0; i < s_InstancedBatchCount; ++i)
    {
        const auto& r = s_InstancedBatches[i];
        DMGE_CORE_ASSERT(r.Material, "RenderQueue::FlushDeferred - instanced renderable has no material!");
        DMGE_CORE_ASSERT(r.VertexArray, "RenderQueue::FlushDeferred - instanced vertexArray is null!");

        if (r.Material.get() != lastInstancedMaterial)
        {
            ApplyGBufferMaterial(shaders.Instanced, r.Material);
            lastInstancedMaterial = r.Material.get();
        }
        RenderCommand::DrawIndexedInstanced(*r.VertexArray, r.InstanceCount);
    }

    s_InstancedBatchCount = 0;
}

} // namespace DMGameEngine
