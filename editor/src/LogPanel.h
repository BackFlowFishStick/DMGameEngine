#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <vector>
#include <string>
#include <memory>

// A spdlog sink that buffers formatted log lines for the editor's Log panel.
// Attached to the engine's core + client loggers so DMGE_LOG_*/DMGE_CLIENT_*
// output appears live in the editor UI.
class LogPanel {
public:
    void OnAttach();
    void OnDetach();
    void OnImGuiRender();

private:
    struct Line { spdlog::level::level_enum level; std::string text; };

    class Sink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        std::vector<Line>* buf = nullptr;
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            if (buf)
                buf->push_back({ msg.level,
                                 std::string(msg.payload.data(), msg.payload.size()) });
        }
        void flush_() override {}
    };

    std::vector<Line>      m_Lines;
    std::shared_ptr<Sink>  m_Sink;
    bool                   m_AutoScroll = true;
};