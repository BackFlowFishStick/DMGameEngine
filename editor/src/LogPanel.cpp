#include "LogPanel.h"
#include <DMGameEngine/Core/Log.h>
#include <imgui.h>
#include <algorithm>

void LogPanel::OnAttach() {
    m_Sink = std::make_shared<Sink>();
    m_Sink->buf = &m_Lines;
    auto sp = std::static_pointer_cast<spdlog::sinks::sink>(m_Sink);
    DMGameEngine::Log::GetCoreLogger()->sinks().push_back(sp);
    DMGameEngine::Log::GetClientLogger()->sinks().push_back(sp);
}

void LogPanel::OnDetach() {
    if (!m_Sink) return;
    auto sp = std::static_pointer_cast<spdlog::sinks::sink>(m_Sink);
    auto rm = [](DM::Ref<spdlog::logger>& logger, std::shared_ptr<spdlog::sinks::sink>& s) {
        if (!logger) return;
        auto& v = logger->sinks();
        v.erase(std::remove(v.begin(), v.end(), s), v.end());
    };
    rm(DMGameEngine::Log::GetCoreLogger(), sp);
    rm(DMGameEngine::Log::GetClientLogger(), sp);
    m_Sink.reset();
}

void LogPanel::OnImGuiRender() {
    ImGui::Begin("Log");
    if (ImGui::Button("Clear")) m_Lines.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu entries)", m_Lines.size());
    ImGui::Separator();
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& l : m_Lines) {
        ImVec4 c{ 1.0f, 1.0f, 1.0f, 1.0f };
        switch (l.level) {
            case spdlog::level::trace:    c = { 0.55f, 0.55f, 0.55f, 1.0f }; break;
            case spdlog::level::debug:    c = { 0.55f, 0.55f, 0.85f, 1.0f }; break;
            case spdlog::level::info:     c = { 1.0f, 1.0f, 1.0f, 1.0f }; break;
            case spdlog::level::warn:     c = { 1.0f, 0.90f, 0.20f, 1.0f }; break;
            case spdlog::level::err:      c = { 1.0f, 0.30f, 0.30f, 1.0f }; break;
            case spdlog::level::critical: c = { 1.0f, 0.20f, 0.60f, 1.0f }; break;
            default: break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, c);
        ImGui::TextUnformatted(l.text.c_str());
        ImGui::PopStyleColor();
    }
    if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}