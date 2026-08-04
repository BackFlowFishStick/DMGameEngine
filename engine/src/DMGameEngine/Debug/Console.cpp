#include "DMGameEngine/Debug/Console.h"

#include <algorithm>
#include <cctype>
#include <exception>

namespace DMGameEngine {

// ── Singleton ────────────────────────────────────────────────────
Console& Console::Get() {
    static Console instance;
    return instance;
}

Console::Console() {
    PrintSystem("Console ready. Type 'help' for commands.");
}

// ── Registration ─────────────────────────────────────────────────
void Console::Register(const std::string& name,
                       const std::string& description,
                       CommandHandler handler) {
    m_commands[name] = { name, description, std::move(handler) };
}

void Console::Unregister(const std::string& name) {
    m_commands.erase(name);
}

// ── Tokenizer ────────────────────────────────────────────────────
// Splits on whitespace; a double-quoted segment is kept as a single
// argument (quotes stripped). Backslash escapes the next char.
static std::vector<std::string> Tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes  = false;
    bool escape    = false;
    bool hasToken = false;

    for (char c : input) {
        if (escape) {
            current.push_back(c);
            hasToken = true;
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            hasToken = true;
            continue;
        }
        if (c == '"') {
            inQuotes = !inQuotes;
            hasToken = true;
            continue;
        }
        if (!inQuotes && std::isspace(static_cast<unsigned char>(c))) {
            if (hasToken) {
                tokens.push_back(std::move(current));
                current.clear();
                hasToken = false;
            }
            continue;
        }
        current.push_back(c);
        hasToken = true;
    }
    if (hasToken)
        tokens.push_back(std::move(current));
    return tokens;
}

// ── Execution ────────────────────────────────────────────────────
std::string Console::Execute(const std::string& input) {
    if (input.empty())
        return {};

    // Echo the raw input so the scrollback reads naturally.
    PushLine(std::string("> ") + input, ConsoleLineKind::Input);

    // Record in history (dedupe consecutive duplicates).
    if (m_history.empty() || m_history.back() != input)
        m_history.push_back(input);

    const auto args = Tokenize(input);
    if (args.empty())
        return {};

    const auto it = m_commands.find(args[0]);
    if (it == m_commands.end()) {
        PushLine("Unknown command: " + args[0] +
                 " (type 'help' for available commands)",
                 ConsoleLineKind::Error);
        return {};
    }

    if (!it->second.handler) {
        PushLine("Command '" + args[0] + "' has no handler",
                 ConsoleLineKind::Error);
        return {};
    }

    std::string output;
    try {
        output = it->second.handler(args);
    } catch (const std::exception& e) {
        PushLine(std::string("Command threw: ") + e.what(),
                 ConsoleLineKind::Error);
        return {};
    } catch (...) {
        PushLine("Command threw an unknown exception",
                 ConsoleLineKind::Error);
        return {};
    }

    if (!output.empty())
        PushLine(output, ConsoleLineKind::Output);
    return output;
}

// ── Scrollback ───────────────────────────────────────────────────
void Console::Clear() {
    m_lines.clear();
}

void Console::PrintSystem(const std::string& text) {
    PushLine(text, ConsoleLineKind::System);
}

void Console::PushLine(const std::string& text, ConsoleLineKind kind) {
    // Split multi-line text into separate scrollback entries so each
    // renders on its own line.
    std::string line;
    for (char c : text) {
        if (c == '\n') {
            m_lines.push_back({ std::move(line), kind });
            line.clear();
        } else {
            line.push_back(c);
        }
    }
    m_lines.push_back({ std::move(line), kind });
}

// ── Autocomplete ──────────────────────────────────────────────────
size_t Console::AutoComplete(const std::string& prefix,
                              std::vector<std::string>& outMatches) const {
    outMatches.clear();
    for (const auto& [name, cmd] : m_commands) {
        if (name.size() >= prefix.size() &&
            name.compare(0, prefix.size(), prefix) == 0)
            outMatches.push_back(name);
    }
    std::sort(outMatches.begin(), outMatches.end());
    return outMatches.size();
}

std::vector<std::string> Console::GetCommandNames() const {
    std::vector<std::string> names;
    names.reserve(m_commands.size());
    for (const auto& [name, cmd] : m_commands)
        names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}

const ConsoleCommand* Console::FindCommand(const std::string& name) const {
    const auto it = m_commands.find(name);
    return it == m_commands.end() ? nullptr : &it->second;
}

} // namespace DMGameEngine