/*
 * DMGameEngine - AnimationSystem (animation stage 1)
 *
 * Samples every AnimatorComponent each frame: advance playback time, load the
 * Skeleton + active AnimationClip through AssetManager (dedup cache makes the
 * per-frame Load cheap), run AnimationMath::SampleLocalMatrices (keyframe ->
 * local TRS) and AnimationMath::ComputeSkinningPalette (hierarchy propagation
 * + inverse bind), and store the result in the component's Palette for
 * SkinnedMeshRenderSystem.
 *
 * Interpolation/hierarchy math is pure logic in AnimationMath (unit-tested);
 * this System is the thin ECS driver. Register it BEFORE MeshRenderSystem
 * (palette must be fresh when the skin pass runs); MeshRenderSystem excludes
 * animated entities so static + skinned paths never double-draw.
 *
 * Header-only, matching TransformSystem. Compiled only when DMGE_ANIMATION
 * is ON.
 */
#pragma once
#include "DMGameEngine/Scene/Systems/System.h"
#include "DMGameEngine/Scene/Scene.h"
#include "DMGameEngine/Scene/Components/AnimatorComponent.h"
#include "DMGameEngine/Asset/AssetManager.h"
#include "DMGameEngine/Animation/Skeleton.h"
#include "DMGameEngine/Animation/AnimationClip.h"
#include "DMGameEngine/Animation/AnimationMath.h"

#include <algorithm>
#include <cmath>

namespace DMGameEngine {

class AnimationSystem : public System
{
public:
    explicit AnimationSystem(Scene& scene) : System(scene) {}

    const char* GetName() const override { return "AnimationSystem"; }

    void OnUpdate(Timestep ts) override
    {
        auto& am   = AssetManager::Get();
        auto  view = m_Scene.GetRegistry().view<AnimatorComponent>();

        for (auto e : view)
        {
            auto& ac = view.get<AnimatorComponent>(e);
            if (!ac.SkeletonAsset.IsValid() || ac.Clips.empty()) continue;
            if (ac.ActiveClip < 0 || ac.ActiveClip >= static_cast<int>(ac.Clips.size()))
                continue;

            auto skeleton = am.Load<Skeleton>(ac.SkeletonAsset);
            if (!skeleton || skeleton->Joints.empty()) continue;
            auto clip = am.Load<AnimationClip>(ac.Clips[static_cast<size_t>(ac.ActiveClip)]);
            if (!clip) continue;

            // Advance playback (in clip ticks), then wrap/clamp.
            if (ac.Playing)
                ac.CurrentTime += ts.GetSeconds() * clip->TicksPerSecond * ac.PlaybackSpeed;

            if (clip->Duration > 0.0f)
            {
                if (ac.Loop)
                {
                    ac.CurrentTime = std::fmod(ac.CurrentTime, clip->Duration);
                    if (ac.CurrentTime < 0.0f) ac.CurrentTime += clip->Duration;
                }
                else
                {
                    ac.CurrentTime = std::clamp(ac.CurrentTime, 0.0f, clip->Duration);
                }
            }

            // Sample -> hierarchy -> palette (pure logic, unit-tested).
            std::vector<glm::mat4> local;
            AnimationMath::SampleLocalMatrices(*clip, *skeleton, ac.CurrentTime, local);
            AnimationMath::ComputeSkinningPalette(*skeleton, local, ac.Palette);
        }
    }
};

} // namespace DMGameEngine
