#include "DMGameEngine/Core/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <vector>

namespace DMGameEngine {

DM::Ref<spdlog::logger> Log::s_coreLogger;
DM::Ref<spdlog::logger> Log::s_clientLogger;

void Log::Init() {
    // ── Console sink (colored) ──────────────────────────────────
    auto consoleSink = DM::CreateRef<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] %n: %v%$");

    // ── File sink ───────────────────────────────────────────────
    auto fileSink = DM::CreateRef<spdlog::sinks::basic_file_sink_mt>(
        "logs/DMGameEngine.log", true);
    fileSink->set_pattern("[%Y-%m-%d %T] [%l] %n: %v");

    std::vector<spdlog::sink_ptr> sinks = { consoleSink, fileSink };

    // ── Core logger ─────────────────────────────────────────────
    s_coreLogger = DM::CreateRef<spdlog::logger>(
        "DMEngine", sinks.begin(), sinks.end());
    spdlog::register_logger(s_coreLogger);
    s_coreLogger->set_level(spdlog::level::trace);
    s_coreLogger->flush_on(spdlog::level::trace);

    // ── Client logger ───────────────────────────────────────────
    s_clientLogger = DM::CreateRef<spdlog::logger>(
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

DM::Ref<spdlog::logger>& Log::GetCoreLogger() {
    return s_coreLogger;
}

DM::Ref<spdlog::logger>& Log::GetClientLogger() {
    return s_clientLogger;
}

} // namespace DMGameEngine
