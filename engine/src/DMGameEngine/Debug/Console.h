/*
 * DMGameEngine - Runtime Developer Console (command registry)
 *
 * Lightweight, dependency-free console core. Holds a registry of
 * named commands (name + description + handler), a scrollback buffer
 * of input / output / error lines, and a command history.
 *
 * ConsoleLayer (a Tool overlay) renders the scrollback + input line
 * via ImGui, toggles on the GraveAccent (`) key, and registers the
 * engine's built-in commands (stat / profile / vsync / renderer /
 * quit / help / clear) against engine subsystems.
 *
 * Game layers and subsystems may register their own commands at any
 * time via Console::Get().Register(...).
 *
 * Access model: single-threaded (main thread). Command registration
 * runs during ConsoleLayer::OnAttach; Execute() and scrollback reads
 * run from the ImGui pass - all on the main thread, matching the
 * Profiler's single-threaded-render assumption.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace DMGameEngine {

// ── Scrollback line kinds (for coloring) ───────────────────────
enum class ConsoleLineKind : uint8_t {
    Input,    // echoed user input:  "> stat fps"
    Output,   // command return string
    Error,    // unknown command / bad args / handler exception
    System,   // console's own messages (welcome, completions, ...)
};

struct ConsoleLine {
    std::string       text;
    ConsoleLineKind   kind = ConsoleLineKind::Output;
};

// ── Command definition ───────────────────────────────────────────
struct ConsoleCommand {
    std::string name;
    std::string description;
    // Receives tokenized arguments; args[0] is the command name.
    // Returns the string to print as output (may be empty).
    std::function<std::string(const std::vector<std::string>&)> handler;
};

class DMGE_API Console {
public:
    using CommandHandler = std::function<std::string(const std::vector<std::string>&)>;

    static Console& Get();

    // ── Command registration ─────────────────────────────────
    // Register (or replace) a command. Re-registration is idempotent
    // so re-attaching ConsoleLayer does not duplicate entries.
    void Register(const std::string& name,
                  const std::string& description,
                  CommandHandler handler);
    void Unregister(const std::string& name);

    // ── Execution ────────────────────────────────────────────
    // Tokenize the raw input (whitespace-separated; a double-quoted
    // segment is kept as one argument, quotes stripped; '\' escapes
    // the next char), look up the command, invoke it, and push both
    // the echoed input line and the handler output into the
    // scrollback. Returns the handler output (empty on error / none).
    std::string Execute(const std::string& input);

    // ── Scrollback ───────────────────────────────────────────
    void Clear();
    void PrintSystem(const std::string& text);
    const std::vector<ConsoleLine>& GetScrollback() const { return m_lines; }

    // ── History ──────────────────────────────────────────────
    const std::vector<std::string>& GetHistory() const { return m_history; }

    // ── Autocomplete ─────────────────────────────────────────
    // Fills outMatches with every registered command name that
    // starts with prefix (sorted). Returns the match count.
    size_t AutoComplete(const std::string& prefix,
                        std::vector<std::string>& outMatches) const;

    // ── Inspection (for help / completion UI) ────────────────
    std::vector<std::string> GetCommandNames() const;
    const ConsoleCommand* FindCommand(const std::string& name) const;

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const         { return m_enabled; }

private:
    Console();
    ~Console() = default;
    Console(const Console&)            = delete;
    Console& operator=(const Console&) = delete;

    void PushLine(const std::string& text, ConsoleLineKind kind);

    std::unordered_map<std::string, ConsoleCommand> m_commands;
    std::vector<ConsoleLine>     m_lines;
    std::vector<std::string>     m_history;
    bool m_enabled = true;
};

} // namespace DMGameEngine