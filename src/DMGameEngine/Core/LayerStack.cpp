#include "DMGameEngine/Core/LayerStack.h"

#include <algorithm>

namespace DMGameEngine {

// ── Destructor ───────────────────────────────────────────────────

LayerStack::~LayerStack() {
    Clear();
}

// ── Clear ────────────────────────────────────────────────────────

void LayerStack::Clear() {
    // Detach layers in reverse order (top layers detached first).
    // Layers are still alive while their OnDetach() runs; ownership
    // is released only afterwards when the vector is cleared.
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        (*it)->OnDetach();
    }
    m_layers.clear();
    m_layerInsertIndex = 0;
}

// ── PushLayer ────────────────────────────────────────────────────

void LayerStack::PushLayer(DM::Scope<Layer> layer) {
    layer->OnAttach();
    m_layers.emplace(m_layers.begin() + m_layerInsertIndex,
                     std::move(layer));
    ++m_layerInsertIndex;
}

// ── PushOverlay ──────────────────────────────────────────────────

void LayerStack::PushOverlay(DM::Scope<Layer> overlay) {
    overlay->OnAttach();
    m_layers.emplace_back(std::move(overlay));
    // m_layerInsertIndex unchanged — overlays always at the back
}

// ── PopLayer ─────────────────────────────────────────────────────

DM::Scope<Layer> LayerStack::PopLayer(Layer* layer) {
    auto it = std::find_if(m_layers.begin(),
                           m_layers.begin() + m_layerInsertIndex,
                           [layer](const DM::Scope<Layer>& ptr) {
                               return ptr.get() == layer;
                           });

    if (it != m_layers.begin() + m_layerInsertIndex) {
        (*it)->OnDetach();
        auto result = std::move(*it);
        m_layers.erase(it);
        --m_layerInsertIndex;
        return result;
    }

    return nullptr;
}

// ── PopOverlay ───────────────────────────────────────────────────

DM::Scope<Layer> LayerStack::PopOverlay(Layer* overlay) {
    auto it = std::find_if(m_layers.begin() + m_layerInsertIndex,
                           m_layers.end(),
                           [overlay](const DM::Scope<Layer>& ptr) {
                               return ptr.get() == overlay;
                           });

    if (it != m_layers.end()) {
        (*it)->OnDetach();
        auto result = std::move(*it);
        m_layers.erase(it);
        return result;
    }

    return nullptr;
}

} // namespace DMGameEngine
