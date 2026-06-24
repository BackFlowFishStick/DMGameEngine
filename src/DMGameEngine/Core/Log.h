/*
 * DMGameEngine - Logging System
 *
 * Wraps spdlog to provide engine-wide logging macros.
 * Call Log::Init() once at engine startup.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <memory>
#include <string>

namespace DMGameEngine {

class DMGE_API Log {
public:
    static void Init();
    static void Shutdown();

    static std::shared_ptr<spdlog::logger>& GetCoreLogger();
    static std::shared_ptr<spdlog::logger>& GetClientLogger();

private:
    static std::shared_ptr<spdlog::logger> s_coreLogger;
    static std::shared_ptr<spdlog::logger> s_clientLogger;
};

} // namespace DMGameEngine

// ── Logging Macros ───────────────────────────────────────────────

// Core engine log
#define DMGE_LOG_TRACE(...)    ::DMGameEngine::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define DMGE_LOG_INFO(...)     ::DMGameEngine::Log::GetCoreLogger()->info(__VA_ARGS__)
#define DMGE_LOG_WARN(...)     ::DMGameEngine::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define DMGE_LOG_ERROR(...)    ::DMGameEngine::Log::GetCoreLogger()->error(__VA_ARGS__)
#define DMGE_LOG_CRITICAL(...) ::DMGameEngine::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client / game log
#define DMGE_CLIENT_TRACE(...)    ::DMGameEngine::Log::GetClientLogger()->trace(__VA_ARGS__)
#define DMGE_CLIENT_INFO(...)     ::DMGameEngine::Log::GetClientLogger()->info(__VA_ARGS__)
#define DMGE_CLIENT_WARN(...)     ::DMGameEngine::Log::GetClientLogger()->warn(__VA_ARGS__)
#define DMGE_CLIENT_ERROR(...)    ::DMGameEngine::Log::GetClientLogger()->error(__VA_ARGS__)
#define DMGE_CLIENT_CRITICAL(...) ::DMGameEngine::Log::GetClientLogger()->critical(__VA_ARGS__)
