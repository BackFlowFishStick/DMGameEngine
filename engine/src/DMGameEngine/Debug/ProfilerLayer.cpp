#include "DMGameEngine/Debug/ProfilerLayer.h"

#include "DMGameEngine/Debug/Profiler.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/Log.h"

#include <imgui.h>

#include <algorithm>

namespace DMGameEngine {

ProfilerLayer::ProfilerLayer()
    : Layer("ProfilerLayer", LayerType::Tool) {}

void ProfilerLayer::OnAttach() {
    DMGE_LOG_INFO("ProfilerLayer attached (toggle with F2)");
}

void ProfilerLayer::OnDetach() {
    DMGE_LOG_INFO("ProfilerLayer detached");
}

void ProfilerLayer::OnEvent(Event& event) {
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
        // React only to the initial press, not auto-repeat.
        if (e.GetRepeatCount() == 0 && e.GetKeyCode() == KeyCode::F2)
            m_show = !m_show;
        return false;  // never consume - other layers may bind F2
    });
}

void ProfilerLayer::OnImGuiRender() {
    if (!m_show)
        return;

    ImGui::SetNextWindowSize(ImVec2(460.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Profiler (F2 to toggle)", &m_show)) {
        ImGui::End();
        return;
    }

    const auto& profiler = Profiler::Get();
    ImGui::Text("FPS:    %.1f", profiler.GetFPS());
    ImGui::SameLine(220.0f);
    ImGui::Text("Frame: %.3f ms", profiler.GetFrameTimeMs());
    ImGui::Text("Draws:  %u", profiler.GetDrawCalls());
    ImGui::SameLine(220.0f);
    ImGui::Text("Indices: %u", profiler.GetDrawIndices());

    // ── Frame-time graph ────────────────────────────────────────
    const auto& hist = profiler.GetFrameTimeHistory();
    float maxMs = 16.7f;
    if (!hist.empty())
        maxMs = (std::max)(maxMs, *std::max_element(hist.begin(), hist.end()));
    ImGui::PlotLines("Frame Time (ms)", hist.data(),
                     static_cast<int>(hist.size()), 0, nullptr,
                     0.0f, maxMs * 1.1f, ImVec2(0.0f, 70.0f));

    ImGui::Separator();

    // ── Per-scope table ──────────────────────────────────────────
    if (ImGui::BeginTable("Scopes", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Calls");
        ImGui::TableSetupColumn("Total");
        ImGui::TableSetupColumn("Mean");
        ImGui::TableSetupColumn("Min");
        ImGui::TableSetupColumn("Max");
        ImGui::TableHeadersRow();

        const auto& aggs = profiler.GetLastFrameAggregates();
        for (const auto& a : aggs) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(a.Name ? a.Name : "?");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", a.Count);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", a.Total / 1.0e6);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.4f", a.Count ? (a.Total / static_cast<double>(a.Count)) / 1.0e6 : 0.0);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.4f", a.Min / 1.0e6);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%.4f", a.Max / 1.0e6);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace DMGameEngine