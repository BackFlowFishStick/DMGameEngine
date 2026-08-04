/*
 * DMGameEngine - ImGui Layer
 *
 * Owns the Dear ImGui context and drives the per-frame ImGui pass.
 * The Application attaches it as an overlay: OnAttach() brings up the
 * ImGui GLFW + OpenGL3 backends, Begin() opens a new ImGui frame just
 * before the LayerStack's OnImGuiRender() pass, and End() finalizes
 * and renders the accumulated draw data afterwards.
 *
 * Editor and debug UI layers override Layer::OnImGuiRender() to submit
 * their panels; they must never call ImGui::NewFrame / Render themselves
 * - that is this layer's responsibility.
 *
 * Input: the ImGui GLFW backend installs its own GLFW callbacks and
 * chains the engine's existing callbacks, so ImGui receives input
 * transparently. OnEvent() additionally marks mouse / keyboard events
 * handled while ImGui wants to capture them (cursor over an ImGui
 * window, focus in a text box) so they do not reach lower layers.
 */

#pragma once

#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Layer.h"

namespace DMGameEngine {

// ── ImGui Layer ──────────────────────────────────────────────────
class DMGE_API ImGuiLayer : public Layer {
public:
    ImGuiLayer();
    ~ImGuiLayer() override = default;

    // ── Lifecycle ────────────────────────────────────────────────
    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;
    void OnImGuiRender() override;

    // ── Per-frame ImGui pass ─────────────────────────────────────
    //     Begin() starts a new ImGui frame; End() renders the
    //     accumulated draw data via the OpenGL3 backend. The
    //     Application calls these around the OnImGuiRender() pass.
    void Begin();
    void End();

    // ── Built-in demo window (verify setup) ──────────────────────
    void ShowDemoWindow(bool show) { m_ShowDemo = show; }
    bool IsDemoWindowShown() const { return m_ShowDemo; }

private:
    bool m_ShowDemo = false;
};

} // namespace DMGameEngine