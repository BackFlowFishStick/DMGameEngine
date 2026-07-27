/*
 * DMGameEngine - System base (ECS stage 1b)
 *
 * Pure logic: a System queries components by type and processes them in
 * bulk. It holds no state beyond a Scene& used to reach the registry.
 * Scene owns std::vector<Ref<System>>; registration order is execution
 * order (TransformSystem before MeshRenderSystem so world matrices are
 * ready before mesh submission). Subclasses override OnUpdate/OnRender/OnEvent
 * and reach the registry via m_Scene.GetRegistry().
 */
#pragma once
#include "DMGameEngine/Core/Timestep.h"
#include "DMGameEngine/Core/Events/Event.h"

namespace DMGameEngine {

class Scene;  // forward declaration - System holds a Scene& (defined in Scene.h)

class System
{
public:
    virtual ~System() = default;

    virtual void OnUpdate(Timestep ts) {}
    virtual void OnRender() {}
    virtual void OnEvent(Event& event) {}

protected:
    Scene& m_Scene;  // reach the registry through the owning Scene
    explicit System(Scene& scene) : m_Scene(scene) {}
};

} // namespace DMGameEngine