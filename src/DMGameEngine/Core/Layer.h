/*
 * DMGameEngine - Layer Base Class
 *
 * Defines the architectural layering system for the engine.
 * Every subsystem (Tool, Feature, Resource, Core, Platform)
 * derives from Layer and plugs into the LayerStack.
 *
 * Usage:
 *   class MyFeatureLayer : public DMGameEngine::Layer {
 *   public:
 *       MyFeatureLayer() : Layer("MyFeature") {}
 *       void OnUpdate(float dt) override { ... }
 *       void OnRender() override      { ... }
 *       void OnEvent(Event& e) override { ... }
 *   };
 */

#pragma once

#include "DMGameEngine/Core/Export.h"
#include "DMGameEngine/Core/Events/Event.h"

#include <string>

namespace DMGameEngine {

// ── Layer Type ───────────────────────────────────────────────────
// Hierarchical levels of the engine architecture, from low to high.
// Lower layers are initialized first and shut down last.
enum class LayerType : uint8_t {
    Platform = 0,   // Platform abstraction (OS, windowing, filesystem)
    Core,           // Memory, threading, math, containers
    Resource,       // Asset loading, caching, resource management
    Feature,        // Animation, physics, rendering, input, scripting
    Tool            // Editor tooling, debug overlays, profilers
};

inline const char* LayerTypeToString(LayerType type) {
    switch (type) {
        case LayerType::Platform: return "Platform";
        case LayerType::Core:     return "Core";
        case LayerType::Resource: return "Resource";
        case LayerType::Feature:  return "Feature";
        case LayerType::Tool:     return "Tool";
    }
    return "Unknown";
}

// ── Layer Base Class ─────────────────────────────────────────────

class DMGE_API Layer {
public:
    Layer(const std::string& name = "Layer",
          LayerType            type = LayerType::Feature);
    virtual ~Layer() = default;

    // ── Lifecycle hooks — override in derived classes ────
    virtual void OnAttach()    {}
    virtual void OnDetach()    {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender()    {}
    virtual void OnEvent(Event& event) {}

    // ── Optional ImGui hook — called during the UI pass ──
    //     Only call ImGui functions inside this method.
    virtual void OnImGuiRender() {}

    // ── Accessors ─────────────────────────────────────────
    const std::string& GetName() const { return m_name; }
    LayerType          GetType() const { return m_type; }

protected:
    std::string m_name;
    LayerType   m_type;
};

} // namespace DMGameEngine
