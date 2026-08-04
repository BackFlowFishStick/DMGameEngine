/*
 * DMGameEngine - LightSystem (ECS, lighting stage B)
 *
 * Collects all LightComponent + TransformComponent entities each frame,
 * converts them to backend-agnostic light data structs (SceneLightData),
 * and uploads the aggregated state to the Renderer via SubmitLightData.
 *
 * Direction convention:
 *   Directional / Spot lights emit along the entity's local -Z axis
 *   (OpenGL forward). The world-space forward vector is extracted from
 *   the rotation part of the entity's WorldMatrix (column 2, negated).
 *
 * Ordering:
 *   LightSystem must be registered BEFORE MeshRenderSystem in the Scene's
 *   system list. Both run inside DefaultSceneLayer's BeginScene/EndScene
 *   bracket. LightSystem fills s_LightData; MeshRenderSystem enqueues
 *   draws; EndScene -> RenderQueue::Flush reads s_LightData and uploads
 *   it per shader group.
 *
 * Ambient:
 *   The first directional light in the scene drives the ambient term
 *   (AmbientColor = light color, AmbientIntensity = light's ambient
 *   intensity). If no directional light exists, a default dim ambient
 *   is used.
 */
#pragma once

#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/TransformComponent.h"
#include "DMGameEngine/Scene/Components/LightComponent.h"
#include "DMGameEngine/Renderer/Renderer.h"
#include "DMGameEngine/Renderer/Light.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <cmath>

namespace DMGameEngine {

class LightSystem : public System
{
public:
    explicit LightSystem(Scene& scene) : System(scene) {}

    const char* GetName() const override { return "LightSystem"; }

    void OnRender() override
    {
        auto& reg  = m_Scene.GetRegistry();
        auto  view = reg.view<LightComponent, TransformComponent>();

        SceneLightData light;

        for (auto e : view)
        {
            auto [lc, tc] = view.get<LightComponent, TransformComponent>(e);

            // Extract world-space position from the world matrix translation.
            glm::vec3 position = glm::vec3(tc.WorldMatrix[3]);

            // Extract world-space forward direction (-Z in OpenGL convention).
            // Normalize column 2 of the rotation part to remove scale.
            glm::mat3 rotMat(tc.WorldMatrix);
            glm::vec3 forward = glm::normalize(rotMat[2]);
            // Guard against degenerate (zero) forward from zero-scaled entities.
            if (glm::any(glm::isnan(forward)) || glm::length(forward) < 0.001f)
                forward = { 0.0f, 0.0f, -1.0f };

            switch (lc.LightType)
            {
            case LightComponent::Type::Directional:
                if (!light.HasDirectional)
                {
                    // Light travels along -forward (the entity's -Z axis).
                    light.DirectionalLight.Direction = -forward;
                    light.DirectionalLight.Color     = lc.Color;
                    light.DirectionalLight.Intensity = lc.Intensity;
                    light.HasDirectional = true;

                    // The first directional light drives ambient.
                    light.AmbientColor     = lc.Color;
                    light.AmbientIntensity = lc.AmbientIntensity;
                }
                break;

            case LightComponent::Type::Point:
                if (light.PointLightCount < MAX_POINT_LIGHTS)
                {
                    auto& pl = light.PointLights[light.PointLightCount++];
                    pl.Position  = position;
                    pl.Color     = lc.Color;
                    pl.Intensity = lc.Intensity;
                    pl.Constant  = lc.Constant;
                    pl.Linear    = lc.Linear;
                    pl.Quadratic = lc.Quadratic;
                }
                break;

            case LightComponent::Type::Spot:
                if (light.SpotLightCount < MAX_SPOT_LIGHTS)
                {
                    auto& sl = light.SpotLights[light.SpotLightCount++];
                    sl.Position      = position;
                    sl.Direction     = -forward;  // cone opens along -Z (forward)
                    sl.Color         = lc.Color;
                    sl.Intensity     = lc.Intensity;
                    sl.Constant      = lc.Constant;
                    sl.Linear        = lc.Linear;
                    sl.Quadratic     = lc.Quadratic;
                    sl.InnerCutoffCos = std::cos(glm::radians(lc.InnerConeAngle));
                    sl.OuterCutoffCos = std::cos(glm::radians(lc.OuterConeAngle));
                }
                break;
            }
        }

        Renderer::SubmitLightData(light);
    }
};

} // namespace DMGameEngine
