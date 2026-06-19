#include "DMGameEngine/Core/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <vector>

namespace DMGameEngine {

std::shared_ptr<spdlog::logger> Log::s_coreLogger;
std::shared_ptr<spdlog::logger> Log::s_clientLogger;

void Log::Init() {
    // ── Console sink (colored) ──────────────────────────────────
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] %n: %v%$");

    // ── File sink ───────────────────────────────────────────────
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/DMGameEngine.log", true);
    fileSink->set_pattern("[%Y-%m-%d %T] [%l] %n: %v");

    std::vector<spdlog::sink_ptr> sinks = { consoleSink, fileSink };

    // ── Core logger ─────────────────────────────────────────────
    s_coreLogger = std::make_shared<spdlog::logger>(
        "DMEngine", sinks.begin(), sinks.end());
    spdlog::register_logger(s_coreLogger);
    s_coreLogger->set_level(spdlog::level::trace);
    s_coreLogger->flush_on(spdlog::level::trace);

    // ── Client logger ───────────────────────────────────────────
    s_clientLogger = std::make_shared<spdlog::logger>(
        "APP", sinks.begin(), sinks.end());
    spdlog::register_logger(s_clientLogger);
    s_clientLogger->set_level(spdlog::level::trace);
    s_clientLogger->flush_on(spdlog::level::trace);
}

void Log::Shutdown() {
    s_coreLogger->flush();
    s_clientLogger->flush();
    spdlog::shutdown();
    s_coreLogger.reset();
    s_clientLogger.reset();
}

std::shared_ptr<spdlog::logger>& Log::GetCoreLogger() {
    return s_coreLogger;
}

std::shared_ptr<spdlog::logger>& Log::GetClientLogger() {
    return s_clientLogger;
}

} // namespace DMGameEngine
