#include "DMGameEngine/Debug/Profiler.h"

#include <algorithm>
#include <functional>
#include <thread>
#include <unordered_map>

namespace DMGameEngine {

// ── Singleton ────────────────────────────────────────────────────
Profiler& Profiler::Get() {
    static Profiler instance;
    return instance;
}

// ── ProfilerScopeTimer ───────────────────────────────────────────
ProfilerScopeTimer::ProfilerScopeTimer(const char* name)
    : m_name(name)
    , m_start(std::chrono::steady_clock::now()) {}

ProfilerScopeTimer::~ProfilerScopeTimer() {
    if (!Profiler::Get().IsEnabled())
        return;

    const auto end = std::chrono::steady_clock::now();
    const auto startNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        m_start.time_since_epoch()).count();
    const auto endNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end.time_since_epoch()).count();
    const auto threadID = static_cast<uint32_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));

    Profiler::Get().WriteResult({ m_name, startNs, endNs, threadID });
}

// ── Frame lifecycle ─────────────────────────────────────────────
void Profiler::BeginFrame() {
    m_frameStart = Clock::now();
}

void Profiler::EndFrame() {
    m_lastFrameTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - m_frameStart).count();

    // Aggregate results gathered during this frame under the lock.
    std::unordered_map<const char*, ProfileAggregate> aggMap;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        aggMap.reserve(m_pending.size() + 8);
        for (const auto& r : m_pending) {
            const int64_t dur = r.End - r.Start;
            auto& a = aggMap[r.Name];
            if (a.Name == nullptr) {
                a.Name = r.Name;
                a.Min  = dur;
                a.Max  = dur;
            }
            a.Total += dur;
            if (dur < a.Min) a.Min = dur;
            if (dur > a.Max) a.Max = dur;
            ++a.Count;
        }
        m_pending.clear();
    }

    // Publish aggregates sorted by total time (desc), then call count.
    m_lastAggregates.clear();
    m_lastAggregates.reserve(aggMap.size());
    for (auto& [_, agg] : aggMap)
        m_lastAggregates.push_back(std::move(agg));
    std::sort(m_lastAggregates.begin(), m_lastAggregates.end(),
              [](const ProfileAggregate& a, const ProfileAggregate& b) {
                  if (a.Total != b.Total) return a.Total > b.Total;
                  return a.Count > b.Count;
              });

    // Smoothed FPS via exponential moving average.
    if (m_lastFrameTimeNs > 0) {
        const float instant = 1.0e9f / static_cast<float>(m_lastFrameTimeNs);
        m_fps = (m_fps <= 0.0f) ? instant : (m_fps * 0.9f + instant * 0.1f);
    }

    // Rolling frame-time history (ms).
    m_frameHistory.push_back(static_cast<float>(m_lastFrameTimeNs) / 1.0e6f);
    if (m_frameHistory.size() > kFrameHistorySize)
        m_frameHistory.erase(m_frameHistory.begin());
}

// ── Result collection (thread-safe) ─────────────────────────────
void Profiler::WriteResult(const ProfileResult& result) {
    if (!m_enabled)
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.push_back(result);
}

} // namespace DMGameEngine