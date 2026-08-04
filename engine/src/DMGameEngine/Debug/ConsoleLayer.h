/*
 * DMGameEngine - Console Layer (ImGui developer console)
 *
 * A Tool-type overlay that renders the Console singleton's scrollback
 * + input line via the OnImGuiRender() pass. Toggle with the
 * GraveAccent (`) key - hidden by default.
 *
 * OnAttach() registers the engine's built-in commands against engine
 * subsystems (Profiler / Renderer / Window / Application). Game layers
 * may register additional commands at any time via Console::Get().
 *
 * Input capture while typing is handled by ImGuiLayer (it marks
 * keyboard events handled when a text input is focused), so gameplay
 * layers do not receive keystrokes typed into the console.
 */

#pragma once

#include "DMGameEngine/Core/Layer.h"

struct ImGuiInputTextCallbackData;  // forward decl - defined by <imgui.h>

namespace DMGameEngine {

class DMGE_API ConsoleLayer : public Layer {
public:
    ConsoleLayer();
    ~ConsoleLayer() override = default;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& event) override;
    void OnImGuiRender() override;

    void Toggle() { m_show = !m_show; }
    bool IsVisible() const { return m_show; }

private:
    // ImGui InputText callback trampoline (static so it can be passed
    // as a C function pointer and still reach the private members).
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data);
    int  TextEditCallback(ImGuiInputTextCallbackData* data);

    void RegisterBuiltins();
    void DrawScrollback();
    void DrawInputLine();

    bool m_show            = false;  // hidden by default (toggle with `)
    bool m_reclaimFocus    = false; // focus the input the frame after open / submit
    bool m_scrollToBottom  = true;  // snap scrollback to bottom on new output
    char m_inputBuf[256]   = {};
    int  m_historyPos      = -1;     // -1 = not navigating history
};

} // namespace DMGameEngine