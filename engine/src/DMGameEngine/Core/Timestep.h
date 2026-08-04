/*
 * DMGameEngine - Timestep
 *
 * A frame's delta time in seconds. Built from the elapsed wall-clock
 * time between two BeginFrame() boundaries and read via GetSeconds() /
 * GetMilliseconds(). Implicitly convertible to float so it drops into
 * existing math (e.g. position += speed * ts).
 *
 * The Application main loop will wrap each frame's wall-clock delta in a
 * Timestep before forwarding it to Layer::OnUpdate() / OnFixedUpdate().
 * Clamping and the fixed-step accumulator live in the Application, not
 * here - this type only holds and presents a duration.
 */

#pragma once

#include "DMGameEngine/Core/Export.h"

namespace DMGameEngine {

class DMGE_API Timestep
{
public:
    Timestep() = default;
    explicit Timestep(float seconds)
        : m_Time(seconds) {}

    // Implicit conversion so a Timestep can be used directly in
    // float arithmetic: pos += speed * ts.
    operator float() const { return m_Time; }

    float GetSeconds()      const { return m_Time; }
    float GetMilliseconds() const { return m_Time * 1000.0f; }

private:
    float m_Time = 0.0f;
};

} // namespace DMGameEngine