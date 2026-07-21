/*
 * DMGameEngine - Profiler (CPU Scope Timing)
 *
 * Lightweight, dependency-free frame profiler. RAII scope timers
 * (ProfilerScopeTimer, created via the DMGE_PROFILE_* macros) record
 * {name, start, end, thread} into the Profiler singleton. At each
 * EndFrame() the results are aggregated per scope name and published
 * for the ProfilerLayer to draw as an ImGui overlay (FPS, frame-time
 * graph, per-scope min/mean/max/count).
 *
 * Collection of scope samples is gated by the DMGE_PROFILE compile
 * switch (CMake option DMGE_PROFILE): when undefined the
 * DMGE_PROFILE_* macros expand to nothing (zero overhead), while the
 * frame-time / FPS counters driven by Application's BeginFrame /
 * EndFrame keep running unconditionally.
 *
 * Thread safety: samples are pushed through a mutex-guarded buffer so
 * scope timers on worker threads (e.g. async asset loaders) are safe;
 * aggregation runs on the main thread at EndFrame().
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

namespace DMGameEngine {

// ── Single scope measurement ────────────────────────────────────
struct ProfileResult {
    const char* Name;   // static string from the macro call site
    int64_t     Start;  // ns since the steady-clock epoch
    int64_t     End;    // ns since the steady-clock epoch
    uint32_t    ThreadID;
};

// ── Per-scope aggregate over the last completed frame ───────────
struct ProfileAggregate {
    const char* Name   = nullptr;
    int64_t     Total  = 0;  // ns, summed across occurrences
    int64_t     Min    = 0;
    int64_t     Max    = 0;
    uint32_t    Count  = 0;
};

class DMGE_API Profiler {
public:
    static Profiler& Get();

    // Called by Application around each frame.
    void BeginFrame();
    void EndFrame();

    // Called by ProfilerScopeTimer on scope exit (thread-safe).
    void WriteResult(const ProfileResult& result);

    // ── Read access for ProfilerLayer ────────────────────────────
    const std::vector<ProfileAggregate>& GetLastFrameAggregates() const { return m_lastAggregates; }
    float GetFPS() const         { return m_fps; }
    float GetFrameTimeMs() const { return m_lastFrameTimeNs / 1.0e6f; }
    const std::vector<float>& GetFrameTimeHistory() const { return m_frameHistory; }

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const         { return m_enabled; }

private:
    Profiler() = default;
    ~Profiler() = default;
    Profiler(const Profiler&)            = delete;
    Profiler& operator=(const Profiler&) = delete;

    using Clock = std::chrono::steady_clock;

    std::mutex                     m_mutex;
    std::vector<ProfileResult>     m_pending;        // gathered this frame
    std::vector<ProfileAggregate>  m_lastAggregates; // published at EndFrame

    Clock::time_point m_frameStart;
    int64_t           m_lastFrameTimeNs = 0;
    float             m_fps = 0.0f;

    std::vector<float> m_frameHistory;  // ms, rolling window
    static constexpr size_t kFrameHistorySize = 240;

    bool m_enabled = true;
};

// ── RAII scope timer (created by DMGE_PROFILE_*) ─────────────────
class DMGE_API ProfilerScopeTimer {
public:
    explicit ProfilerScopeTimer(const char* name);
    ~ProfilerScopeTimer();

    ProfilerScopeTimer(const ProfilerScopeTimer&)            = delete;
    ProfilerScopeTimer& operator=(const ProfilerScopeTimer&) = delete;

private:
    const char*                              m_name;
    std::chrono::steady_clock::time_point    m_start;
};

} // namespace DMGameEngine

// ── Profiling Macros ─────────────────────────────────────────────
// Define DMGE_PROFILE via CMake (option DMGE_PROFILE, default ON) to
// time individual scopes. When undefined the macros are no-ops while
// the frame-time / FPS counters still run.
#ifdef DMGE_PROFILE
    #define DMGE_PROFILE_SCOPE(name) \
        ::DMGameEngine::ProfilerScopeTimer _dmge_profile_scope(name)
    #define DMGE_PROFILE_FUNCTION() \
        DMGE_PROFILE_SCOPE(__FUNCTION__)
#else
    #define DMGE_PROFILE_SCOPE(name)
    #define DMGE_PROFILE_FUNCTION()
#endif