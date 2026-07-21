#include "DMGameEngine/Debug/ConsoleLayer.h"

#include "DMGameEngine/Core/Application.h"
#include "DMGameEngine/Core/Events/Event.h"
#include "DMGameEngine/Core/Events/KeyEvent.h"
#include "DMGameEngine/Core/KeyCodes.h"
#include "DMGameEngine/Core/Log.h"
#include "DMGameEngine/Core/Window.h"
#include "DMGameEngine/Debug/Console.h"
#include "DMGameEngine/Debug/Profiler.h"
#include "DMGameEngine/Renderer/Renderer.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace DMGameEngine {

ConsoleLayer::ConsoleLayer()
    : Layer("ConsoleLayer", LayerType::Tool) {}

// ── Lifecycle ────────────────────────────────────────────────────

void ConsoleLayer::OnAttach() {
    RegisterBuiltins();
    DMGE_LOG_INFO("ConsoleLayer attached (toggle with `)");
}

void ConsoleLayer::OnDetach() {
    DMGE_LOG_INFO("ConsoleLayer detached");
}

// ── Event handling ───────────────────────────────────────────────

void ConsoleLayer::OnEvent(Event& event) {
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
        // Toggle on the initial GraveAccent press only (no auto-repeat).
        if (e.GetRepeatCount() == 0 && e.GetKeyCode() == KeyCode::GraveAccent) {
            m_show = !m_show;
            // Reset the input line on every toggle: covers a stray
            // backtick char that ImGui may have queued this poll, and
            // gives a fresh prompt when reopening.
            m_inputBuf[0] = '\0';
            m_historyPos  = -1;
            if (m_show) {
                m_reclaimFocus   = true;
                m_scrollToBottom = true;
            }
            return true;  // claim the console key
        }
        return false;
    });
}

// ── ImGui ────────────────────────────────────────────────────────

void ConsoleLayer::OnImGuiRender() {
    if (!m_show)
        return;

    ImGui::SetNextWindowSize(ImVec2(720.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console (` to toggle)", &m_show,
                      ImGuiWindowFlags_NoNavInputs)) {
        ImGui::End();
        return;
    }

    DrawScrollback();
    ImGui::Separator();
    DrawInputLine();

    ImGui::End();
}

void ConsoleLayer::DrawScrollback() {
    // Reserve room for the input line + hint beneath.
    const float reserve = ImGui::GetFrameHeightWithSpacing() * 2.0f + 4.0f;
    const float avail   = ImGui::GetContentRegionAvail().y;
    const float scrollH = avail > reserve ? avail - reserve : 0.0f;

    ImGui::BeginChild("ConsoleScrollback", ImVec2(0, scrollH),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    const auto& lines = Console::Get().GetScrollback();

    for (const auto& line : lines) {
        switch (line.kind) {
            case ConsoleLineKind::Input:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.90f, 0.60f, 1.0f)); break;
            case ConsoleLineKind::Output:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f)); break;
            case ConsoleLineKind::Error:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.45f, 0.45f, 1.0f)); break;
            case ConsoleLineKind::System:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.00f, 1.0f)); break;
        }
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }

    if (m_scrollToBottom && !lines.empty()) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBottom = false;
    }

    ImGui::EndChild();
}

void ConsoleLayer::DrawInputLine() {
    // Reclaim keyboard focus the frame after opening / submitting.
    if (m_reclaimFocus) {
        ImGui::SetKeyboardFocusHere(0);
        m_reclaimFocus = false;
    }

    const ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackCompletion |
        ImGuiInputTextFlags_CallbackHistory |
        ImGuiInputTextFlags_NoHorizontalScroll;

    const bool submitted = ImGui::InputText("##ConsoleInput", m_inputBuf,
                                            sizeof(m_inputBuf), flags,
                                            &ConsoleLayer::TextEditCallbackStub,
                                            this);

    ImGui::SameLine();
    ImGui::TextDisabled("(enter to run, tab to complete, up/down for history)");

    if (submitted) {
        const std::string input(m_inputBuf);
        m_inputBuf[0] = '\0';
        if (!input.empty()) {
            Console::Get().Execute(input);
            m_scrollToBottom = true;
        }
        m_historyPos   = -1;
        m_reclaimFocus  = true;  // keep focus for the next command
    }
}

// ── ImGui InputText callback ─────────────────────────────────────

int ConsoleLayer::TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
    auto* layer = static_cast<ConsoleLayer*>(data->UserData);
    return layer->TextEditCallback(data);
}

int ConsoleLayer::TextEditCallback(ImGuiInputTextCallbackData* data) {
    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackCompletion: {
            // Tab - complete the word before the cursor.
            const char* wordEnd   = data->Buf + data->CursorPos;
            const char* wordStart = wordEnd;
            while (wordStart > data->Buf) {
                const char prev = *(wordStart - 1);
                if (prev == ' ' || prev == '\t')
                    break;
                --wordStart;
            }
            const std::string prefix(wordStart, wordEnd);

            std::vector<std::string> matches;
            const size_t count = Console::Get().AutoComplete(prefix, matches);

            if (count == 0) {
                Console::Get().PrintSystem("No command starts with '" + prefix + "'");
            } else if (count == 1) {
                const std::string replacement = matches[0] + " ";
                data->DeleteChars(static_cast<int>(wordStart - data->Buf),
                                  static_cast<int>(wordEnd - wordStart));
                data->InsertChars(static_cast<int>(wordStart - data->Buf),
                                  replacement.c_str());
            } else {
                // Multiple matches: list them and complete the common prefix.
                std::string line = "Commands: ";
                for (size_t i = 0; i < matches.size(); ++i) {
                    if (i) line += ", ";
                    line += matches[i];
                }
                Console::Get().PrintSystem(line);
                m_scrollToBottom = true;

                std::string common = matches[0];
                for (size_t i = 1; i < matches.size(); ++i) {
                    const size_t m = std::min(common.size(), matches[i].size());
                    size_t n = 0;
                    while (n < m && common[n] == matches[i][n]) ++n;
                    common.resize(n);
                }
                if (common.size() > prefix.size()) {
                    data->DeleteChars(static_cast<int>(wordStart - data->Buf),
                                      static_cast<int>(wordEnd - wordStart));
                    data->InsertChars(static_cast<int>(wordStart - data->Buf),
                                      common.c_str());
                }
            }
            break;
        }
        case ImGuiInputTextFlags_CallbackHistory: {
            // Up / Down - navigate command history.
            const auto& history = Console::Get().GetHistory();
            if (history.empty())
                break;

            const int prevPos = m_historyPos;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (m_historyPos == -1)
                    m_historyPos = static_cast<int>(history.size()) - 1;
                else if (m_historyPos > 0)
                    --m_historyPos;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (m_historyPos != -1) {
                    if (m_historyPos < static_cast<int>(history.size()) - 1)
                        ++m_historyPos;
                    else
                        m_historyPos = -1;  // past newest -> clear
                }
            }

            if (m_historyPos != prevPos) {
                const char* entry =
                    (m_historyPos == -1) ? "" : history[m_historyPos].c_str();
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, entry);
            }
            break;
        }
    }
    return 0;
}

// ── Built-in commands ────────────────────────────────────────────

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

const char* RendererApiName(Renderer::API api) {
    switch (api) {
        case Renderer::API::None:    return "None";
        case Renderer::API::OpenGL:   return "OpenGL";
        case Renderer::API::Vulkan:   return "Vulkan";
        case Renderer::API::DirectX:  return "DirectX";
    }
    return "Unknown";
}

} // namespace

void ConsoleLayer::RegisterBuiltins() {
    auto& console = Console::Get();

    console.Register("help", "List available commands",
        [](const std::vector<std::string>&) -> std::string {
            std::string out = "Available commands:";
            const auto& c = Console::Get();
            for (const auto& name : c.GetCommandNames()) {
                const ConsoleCommand* cmd = c.FindCommand(name);
                out += "\n  " + name;
                if (cmd && !cmd->description.empty())
                    out += "  -  " + cmd->description;
            }
            return out;
        });

    console.Register("clear", "Clear the console scrollback",
        [](const std::vector<std::string>&) -> std::string {
            Console::Get().Clear();
            return {};
        });

    console.Register("echo", "Print arguments back",
        [](const std::vector<std::string>& args) -> std::string {
            std::string out;
            for (size_t i = 1; i < args.size(); ++i) {
                if (i > 1) out += ' ';
                out += args[i];
            }
            return out;
        });

    console.Register("stat", "stat <fps|frame>",
        [](const std::vector<std::string>& args) -> std::string {
            const auto& p = Profiler::Get();
            const std::string what = (args.size() >= 2) ? ToLower(args[1]) : std::string{};
            if (what == "fps")
                return "FPS: " + std::to_string(p.GetFPS());
            if (what == "frame")
                return "Frame time: " + std::to_string(p.GetFrameTimeMs()) + " ms";
            return "usage: stat <fps|frame>";
        });

    console.Register("profile", "profile <on|off|status>",
        [](const std::vector<std::string>& args) -> std::string {
            auto& p = Profiler::Get();
            const std::string what = (args.size() >= 2) ? ToLower(args[1]) : std::string{};
            if (what == "on")  { p.SetEnabled(true);  return "Profiler enabled"; }
            if (what == "off") { p.SetEnabled(false); return "Profiler disabled"; }
            if (what == "status" || what.empty())
                return std::string("Profiler: ") + (p.IsEnabled() ? "enabled" : "disabled");
            return "usage: profile <on|off|status>";
        });

    console.Register("vsync", "vsync <on|off|status>",
        [](const std::vector<std::string>& args) -> std::string {
            auto& window = Application::Get().GetWindow();
            const std::string what = (args.size() >= 2) ? ToLower(args[1]) : std::string{};
            if (what == "on")  { window.SetVSync(true);  return "VSync enabled"; }
            if (what == "off") { window.SetVSync(false); return "VSync disabled"; }
            if (what == "status" || what.empty())
                return std::string("VSync: ") + (window.IsVSync() ? "enabled" : "disabled");
            return "usage: vsync <on|off|status>";
        });

    console.Register("renderer", "renderer <api>",
        [](const std::vector<std::string>& args) -> std::string {
            const std::string what = (args.size() >= 2) ? ToLower(args[1]) : std::string{};
            if (what == "api" || what.empty())
                return std::string("Renderer API: ") + RendererApiName(Renderer::GetAPI());
            return "usage: renderer <api>";
        });

    console.Register("quit", "Quit the application",
        [](const std::vector<std::string>&) -> std::string {
            Application::Get().Quit();
            return "Quitting...";
        });
}

} // namespace DMGameEngine