/*
 * DMGameEngine - Layer Stack
 *
 * Owns and manages all engine layers in insertion order.
 * Layers are updated first-to-last; overlays are rendered last-to-first
 * (e.g. for UI on top of gameplay).
 *
 * Usage:
 *   LayerStack stack;
 *   stack.PushLayer(new GameLayer());
 *   stack.PushOverlay(new DebugOverlay());
 *
 *   for (Layer* layer : stack)
 *       layer->OnUpdate(ts);
 */

#pragma once

#include "DMGameEngine/Core/Layer.h"

#include <vector>
#include <memory>

namespace DMGameEngine {

class DMGE_API LayerStack {
public:
    LayerStack() = default;
    ~LayerStack();

    // Non-copyable, movable
    LayerStack(const LayerStack&) = delete;
    LayerStack& operator=(const LayerStack&) = delete;
    LayerStack(LayerStack&&) = default;
    LayerStack& operator=(LayerStack&&) = default;

    // ── Push / Pop ──────────────────────────────────────────
    // Layers are inserted at m_layerInsertIndex so that
    // overlays always sit at the back of the container.
    void PushLayer(DM::Scope<Layer> layer);
    void PushOverlay(DM::Scope<Layer> overlay);

    // Pop removes the layer from the stack; ownership
    // is transferred back to the caller (usually for deletion).
    DM::Scope<Layer> PopLayer(Layer* layer);
    DM::Scope<Layer> PopOverlay(Layer* layer);

    // ── Teardown ───────────────────────────────────────────
    // Detaches every layer in reverse order (OnDetach) while it is
    // still alive, then drops ownership. Safe on an empty stack and
    // idempotent - the destructor delegates here, so a later destroy
    // of an already-cleared stack is a no-op.
    void Clear();

    // ── Iterators ───────────────────────────────────────────
    // Forward: layers first, then overlays
    // Reverse: overlays first (render order), then layers

    using container_type = std::vector<DM::Scope<Layer>>;

    container_type::iterator       begin()       { return m_layers.begin(); }
    container_type::iterator       end()         { return m_layers.end(); }
    container_type::reverse_iterator rbegin()   { return m_layers.rbegin(); }
    container_type::reverse_iterator rend()     { return m_layers.rend(); }

    container_type::const_iterator begin() const { return m_layers.begin(); }
    container_type::const_iterator end()   const { return m_layers.end(); }
    container_type::const_reverse_iterator rbegin() const { return m_layers.rbegin(); }
    container_type::const_reverse_iterator rend()   const { return m_layers.rend(); }

private:
    container_type m_layers;
    unsigned int   m_layerInsertIndex = 0;  // insertion point for regular layers
};

} // namespace DMGameEngine
