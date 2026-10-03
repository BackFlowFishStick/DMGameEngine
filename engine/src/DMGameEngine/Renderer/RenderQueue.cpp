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
#include "DMGameEngine/Renderer/Material.h"
#include "DMGameEngine/Renderer/Shader.h"
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

// Upload all per-scene lighting uniforms to a shader. Called once per
// shader/material group at Flush time. Missing uniforms are silently
// skipped (location -1 -> glUniform no-op per GL spec).
void UploadSceneLighting(const DM::Ref<Shader>& shader,
                         const glm::vec3& cameraPosition,
                         const SceneLightData& light)
{
    // Camera
    shader->SetFloat3("u_CameraPosition", cameraPosition);

    // Ambient
    shader->SetFloat3("u_AmbientColor", light.AmbientColor);
    shader->SetFloat("u_AmbientIntensity", light.AmbientIntensity);

    // Directional light
    shader->SetFloat3("u_DirectionalLight_direction", light.DirectionalLight.Direction);
    shader->SetFloat3("u_DirectionalLight_color",
                      light.HasDirectional ? light.DirectionalLight.Color : glm::vec3(0.0f));
    shader->SetFloat("u_DirectionalLight_intensity",
                     light.HasDirectional ? light.DirectionalLight.Intensity : 0.0f);

    // Point lights
    shader->SetInt("u_PointLightCount", static_cast<int>(light.PointLightCount));
    for (uint32_t i = 0; i < light.PointLightCount && i < MAX_POINT_LIGHTS; ++i)
    {
        const auto& pl = light.PointLights[i];
        shader->SetFloat3("u_PointLights_position["     + std::to_string(i) + "]", pl.Position);
        shader->SetFloat3("u_PointLights_color["        + std::to_string(i) + "]", pl.Color);
        shader->SetFloat("u_PointLights_intensity["     + std::to_string(i) + "]", pl.Intensity);
        shader->SetFloat("u_PointLights_constant["      + std::to_string(i) + "]", pl.Constant);
        shader->SetFloat("u_PointLights_linear["        + std::to_string(i) + "]", pl.Linear);
        shader->SetFloat("u_PointLights_quadratic["     + std::to_string(i) + "]", pl.Quadratic);
    }

    // Spot lights
    shader->SetInt("u_SpotLightCount", static_cast<int>(light.SpotLightCount));
    for (uint32_t i = 0; i < light.SpotLightCount && i < MAX_SPOT_LIGHTS; ++i)
    {
        const auto& sl = light.SpotLights[i];
        shader->SetFloat3("u_SpotLights_position["       + std::to_string(i) + "]", sl.Position);
        shader->SetFloat3("u_SpotLights_direction["      + std::to_string(i) + "]", sl.Direction);
        shader->SetFloat3("u_SpotLights_color["          + std::to_string(i) + "]", sl.Color);
        shader->SetFloat("u_SpotLights_intensity["       + std::to_string(i) + "]", sl.Intensity);
        shader->SetFloat("u_SpotLights_constant["        + std::to_string(i) + "]", sl.Constant);
        shader->SetFloat("u_SpotLights_linear["          + std::to_string(i) + "]", sl.Linear);
        shader->SetFloat("u_SpotLights_quadratic["       + std::to_string(i) + "]", sl.Quadratic);
        shader->SetFloat("u_SpotLights_innerCutoffCos["  + std::to_string(i) + "]", sl.InnerCutoffCos);
        shader->SetFloat("u_SpotLights_outerCutoffCos["  + std::to_string(i) + "]", sl.OuterCutoffCos);
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

} // namespace DMGameEngine
