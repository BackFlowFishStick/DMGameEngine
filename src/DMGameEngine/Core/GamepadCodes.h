/*
 * DMGameEngine - Gamepad Codes
 *
 * Platform-agnostic gamepad button and axis code enumerations.
 * Used by the polling input API (Input.h) and the gamepad events
 * (GamepadEvent.h). Integer values mirror the standard GLFW
 * gamepad mapping so they map 1:1 to the platform layer.
 */

#pragma once

namespace DMGameEngine {

// Maximum number of concurrently supported gamepads (player slots 0..3).
inline constexpr int kMaxGamepads = 4;

// ── Gamepad Buttons ───────────────────────────────────────────────
// Standard gamepad mapping. Values match GLFW_GAMEPAD_BUTTON_*.
enum class GamepadButton : int {
    A = 0, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DPadUp, DPadRight, DPadDown, DPadLeft,

    Count,  // number of buttons (= 15)
};

// ── Gamepad Axes ───────────────────────────────────────────────────
// Standard gamepad axes. Values match GLFW_GAMEPAD_AXIS_*.
//   LeftY / RightY are positive downwards (screen convention);
//   pushing up reports a negative value.
//   Triggers range [0, 1]; sticks range [-1, 1].
enum class GamepadAxis : int {
    LeftX = 0, LeftY, RightX, RightY,
    LeftTrigger, RightTrigger,

    Count,  // number of axes (= 6)
};

} // namespace DMGameEngine