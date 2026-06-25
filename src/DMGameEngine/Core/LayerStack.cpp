#include "DMGameEngine/Core/LayerStack.h"

#include <algorithm>

namespace DMGameEngine {

// ── Destructor ───────────────────────────────────────────────────

LayerStack::~LayerStack() {
    // Detach layers in reverse order (top layers detached first)
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        (*it)->OnDetach();
    }
}

// ── PushLayer ────────────────────────────────────────────────────

void LayerStack::PushLayer(std::unique_ptr<Layer> layer) {
    layer->OnAttach();
    m_layers.emplace(m_layers.begin() + m_layerInsertIndex,
                     std::move(layer));
    ++m_layerInsertIndex;
}

// ── PushOverlay ──────────────────────────────────────────────────

void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay) {
    overlay->OnAttach();
    m_layers.emplace_back(std::move(overlay));
    // m_layerInsertIndex unchanged — overlays always at the back
}

// ── PopLayer ─────────────────────────────────────────────────────

std::unique_ptr<Layer> LayerStack::PopLayer(Layer* layer) {
    auto it = std::find_if(m_layers.begin(),
                           m_layers.begin() + m_layerInsertIndex,
                           [layer](const std::unique_ptr<Layer>& ptr) {
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

std::unique_ptr<Layer> LayerStack::PopOverlay(Layer* overlay) {
    auto it = std::find_if(m_layers.begin() + m_layerInsertIndex,
                           m_layers.end(),
                           [overlay](const std::unique_ptr<Layer>& ptr) {
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
