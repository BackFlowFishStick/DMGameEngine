/*
 * DMGameEngine - Mouse Codes
 *
 * Platform-agnostic mouse button code enumeration.
 * Used by both the event system (MouseEvent.h) and the polling input API (Input.h).
 */

#pragma once

namespace DMGameEngine {

enum class MouseCode : int {
    Button0 = 0,
    Button1 = 1,
    Button2 = 2,
    Button3 = 3,
    Button4 = 4,
    Button5 = 5,
    Button6 = 6,
    Button7 = 7,

    Left   = Button0,
    Right  = Button1,
    Middle = Button2,
};

} // namespace DMGameEngine
