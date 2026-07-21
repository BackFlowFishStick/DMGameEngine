/*
 * DMGameEngine - Profiler Layer (ImGui Overlay)
 *
 * A Tool-type overlay that renders the Profiler's data each frame via
 * the OnImGuiRender() pass: smoothed FPS, a rolling frame-time graph,
 * and a per-scope table (count / total / mean / min / max). Press F1
 * to toggle the overlay on or off.
 */

#pragma once

#include "DMGameEngine/Core/Layer.h"

namespace DMGameEngine {

class DMGE_API ProfilerLayer : public Layer {
public:
    ProfilerLayer();
    ~ProfilerLayer() override = default;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;
    void OnImGuiRender() override;

    void Toggle() { m_show = !m_show; }
    bool IsVisible() const { return m_show; }

private:
    bool m_show = true;
};

} // namespace DMGameEngine