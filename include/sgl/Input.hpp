#pragma once

#include <array>
#include <string>

#include "Types.hpp"

namespace sgl {

enum class Key : i32 {
    Unknown = -1,
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,
    Num0 = 48, Num1 = 49, Num2 = 50, Num3 = 51, Num4 = 52,
    Num5 = 53, Num6 = 54, Num7 = 55, Num8 = 56, Num9 = 57,
    Semicolon = 59,
    Equal = 61,
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71,
    H = 72, I = 73, J = 74, K = 75, L = 76, M = 77, N = 78,
    O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84, U = 85,
    V = 86, W = 87, X = 88, Y = 89, Z = 90,
    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,
    World1 = 161,
    World2 = 162,
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
    F13 = 302, F14 = 303, F15 = 304, F16 = 305, F17 = 306, F18 = 307,
    F19 = 308, F20 = 309, F21 = 310, F22 = 311, F23 = 312, F24 = 313,
    F25 = 314,
    KP0 = 320, KP1 = 321, KP2 = 322, KP3 = 323, KP4 = 324,
    KP5 = 325, KP6 = 326, KP7 = 327, KP8 = 328, KP9 = 329,
    KPDecimal = 330,
    KPDivide = 331,
    KPMultiply = 332,
    KPSubtract = 333,
    KPAdd = 334,
    KPEnter = 335,
    KPEqual = 336,
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,
};

enum class MouseButton : i32 {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,
};

enum class KeyAction : i32 {
    Release = 0,
    Press = 1,
    Repeat = 2,
};

enum class Modifiers : i32 {
    None = 0,
    Shift = 0x0001,
    Control = 0x0002,
    Alt = 0x0004,
    Super = 0x0008,
    CapsLock = 0x0010,
    NumLock = 0x0020,
};

constexpr Modifiers operator|(Modifiers a, Modifiers b) {
    return static_cast<Modifiers>(static_cast<i32>(a) | static_cast<i32>(b));
}
constexpr Modifiers operator&(Modifiers a, Modifiers b) {
    return static_cast<Modifiers>(static_cast<i32>(a) & static_cast<i32>(b));
}
constexpr Modifiers& operator|=(Modifiers& a, Modifiers b) {
    a = a | b;
    return a;
}
constexpr bool HasModifier(Modifiers value, Modifiers flag) {
    return (static_cast<i32>(value) & static_cast<i32>(flag)) != 0;
}

std::string GetKeyName(Key key, i32 scancode);
i32 GetKeyScancode(Key key);
bool IsRawMouseMotionSupported();

enum class InputMode : i32 {
    Cursor = 0x00033001,
    StickyKeys = 0x00033002,
    StickyMouseButtons = 0x00033003,
    LockKeyMods = 0x00033004,
    RawMouseMotion = 0x00033005,
};

enum class CursorMode : i32 {
    Normal = 0x00034001,
    Hidden = 0x00034002,
    Disabled = 0x00034003,
    Captured = 0x00034004,
};

enum class JoystickHat : i32 {
    Centered = 0,
    Up = 1,
    Right = 2,
    Down = 4,
    Left = 8,
    RightUp = Up | Right,
    RightDown = Right | Down,
    LeftDown = Left | Down,
    LeftUp = Left | Up,
};

enum class GamepadButton : i32 {
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    LeftBumper = 4,
    RightBumper = 5,
    Back = 6,
    Start = 7,
    Guide = 8,
    LeftThumb = 9,
    RightThumb = 10,
    DPadUp = 11,
    DPadRight = 12,
    DPadDown = 13,
    DPadLeft = 14,
    Count = 15,
};

// Y-Achsen (LeftY/RightY) folgen der GLFW/SDL-Konvention:
// negativ = oben, positiv = unten. Das gilt auf allen Plattformen.
enum class GamepadAxis : i32 {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,
    RightTrigger = 5,
    Count = 6,
};

struct GamepadState {
    std::array<u8, static_cast<std::size_t>(GamepadButton::Count)> buttons{};
    std::array<f32, static_cast<std::size_t>(GamepadAxis::Count)> axes{};
};

} // namespace sgl
