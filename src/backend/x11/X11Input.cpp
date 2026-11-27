#include "X11Internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <X11/Xutil.h>

namespace sgl::detail::x11 {

Key translateKeysym(KeySym keysym) {
    switch (keysym) {
    case XK_space: return Key::Space;
    case XK_apostrophe: return Key::Apostrophe;
    case XK_comma: return Key::Comma;
    case XK_minus: return Key::Minus;
    case XK_period: return Key::Period;
    case XK_slash: return Key::Slash;
    case XK_0: case XK_parenright: return Key::Num0;
    case XK_1: case XK_exclam: return Key::Num1;
    case XK_2: case XK_at: return Key::Num2;
    case XK_3: case XK_numbersign: return Key::Num3;
    case XK_4: case XK_dollar: return Key::Num4;
    case XK_5: case XK_percent: return Key::Num5;
    case XK_6: case XK_asciicircum: return Key::Num6;
    case XK_7: case XK_ampersand: return Key::Num7;
    case XK_8: case XK_asterisk: return Key::Num8;
    case XK_9: case XK_parenleft: return Key::Num9;
    case XK_semicolon: case XK_colon: return Key::Semicolon;
    case XK_equal: case XK_plus: return Key::Equal;
    case XK_a: case XK_A: return Key::A;
    case XK_b: case XK_B: return Key::B;
    case XK_c: case XK_C: return Key::C;
    case XK_d: case XK_D: return Key::D;
    case XK_e: case XK_E: return Key::E;
    case XK_f: case XK_F: return Key::F;
    case XK_g: case XK_G: return Key::G;
    case XK_h: case XK_H: return Key::H;
    case XK_i: case XK_I: return Key::I;
    case XK_j: case XK_J: return Key::J;
    case XK_k: case XK_K: return Key::K;
    case XK_l: case XK_L: return Key::L;
    case XK_m: case XK_M: return Key::M;
    case XK_n: case XK_N: return Key::N;
    case XK_o: case XK_O: return Key::O;
    case XK_p: case XK_P: return Key::P;
    case XK_q: case XK_Q: return Key::Q;
    case XK_r: case XK_R: return Key::R;
    case XK_s: case XK_S: return Key::S;
    case XK_t: case XK_T: return Key::T;
    case XK_u: case XK_U: return Key::U;
    case XK_v: case XK_V: return Key::V;
    case XK_w: case XK_W: return Key::W;
    case XK_x: case XK_X: return Key::X;
    case XK_y: case XK_Y: return Key::Y;
    case XK_z: case XK_Z: return Key::Z;
    case XK_bracketleft: case XK_braceleft: return Key::LeftBracket;
    case XK_backslash: case XK_bar: return Key::Backslash;
    case XK_bracketright: case XK_braceright: return Key::RightBracket;
    case XK_grave: case XK_asciitilde: return Key::GraveAccent;
    case XK_Escape: return Key::Escape;
    case XK_Return: return Key::Enter;
    case XK_Tab: case XK_ISO_Left_Tab: return Key::Tab;
    case XK_BackSpace: return Key::Backspace;
    case XK_Insert: return Key::Insert;
    case XK_Delete: return Key::Delete;
    case XK_Right: return Key::Right;
    case XK_Left: return Key::Left;
    case XK_Down: return Key::Down;
    case XK_Up: return Key::Up;
    case XK_Page_Up: return Key::PageUp;
    case XK_Page_Down: return Key::PageDown;
    case XK_Home: return Key::Home;
    case XK_End: return Key::End;
    case XK_Caps_Lock: return Key::CapsLock;
    case XK_Scroll_Lock: return Key::ScrollLock;
    case XK_Num_Lock: return Key::NumLock;
    case XK_Print: return Key::PrintScreen;
    case XK_Pause: return Key::Pause;
    case XK_F1: return Key::F1;
    case XK_F2: return Key::F2;
    case XK_F3: return Key::F3;
    case XK_F4: return Key::F4;
    case XK_F5: return Key::F5;
    case XK_F6: return Key::F6;
    case XK_F7: return Key::F7;
    case XK_F8: return Key::F8;
    case XK_F9: return Key::F9;
    case XK_F10: return Key::F10;
    case XK_F11: return Key::F11;
    case XK_F12: return Key::F12;
    case XK_F13: return Key::F13;
    case XK_F14: return Key::F14;
    case XK_F15: return Key::F15;
    case XK_F16: return Key::F16;
    case XK_F17: return Key::F17;
    case XK_F18: return Key::F18;
    case XK_F19: return Key::F19;
    case XK_F20: return Key::F20;
    case XK_F21: return Key::F21;
    case XK_F22: return Key::F22;
    case XK_F23: return Key::F23;
    case XK_F24: return Key::F24;
    case XK_F25: return Key::F25;
    case XK_KP_0: case XK_KP_Insert: return Key::KP0;
    case XK_KP_1: case XK_KP_End: return Key::KP1;
    case XK_KP_2: case XK_KP_Down: return Key::KP2;
    case XK_KP_3: case XK_KP_Next: return Key::KP3;
    case XK_KP_4: case XK_KP_Left: return Key::KP4;
    case XK_KP_5: case XK_KP_Begin: return Key::KP5;
    case XK_KP_6: case XK_KP_Right: return Key::KP6;
    case XK_KP_7: case XK_KP_Home: return Key::KP7;
    case XK_KP_8: case XK_KP_Up: return Key::KP8;
    case XK_KP_9: case XK_KP_Prior: return Key::KP9;
    case XK_KP_Decimal: case XK_KP_Delete: return Key::KPDecimal;
    case XK_KP_Divide: return Key::KPDivide;
    case XK_KP_Multiply: return Key::KPMultiply;
    case XK_KP_Subtract: return Key::KPSubtract;
    case XK_KP_Add: return Key::KPAdd;
    case XK_KP_Enter: return Key::KPEnter;
    case XK_KP_Equal: return Key::KPEqual;
    case XK_Shift_L: return Key::LeftShift;
    case XK_Control_L: return Key::LeftControl;
    case XK_Alt_L: return Key::LeftAlt;
    case XK_Super_L: return Key::LeftSuper;
    case XK_Shift_R: return Key::RightShift;
    case XK_Control_R: return Key::RightControl;
    case XK_Alt_R: return Key::RightAlt;
    case XK_Super_R: return Key::RightSuper;
    case XK_Menu: return Key::Menu;
    case XK_ISO_Level3_Shift: return Key::World1;
    case XK_ISO_Level5_Shift: return Key::World2;
    default: return Key::Unknown;
    }
}

KeySym keysymForKey(Key key) {
    if (key >= Key::A && key <= Key::Z) {
        return XK_a + (static_cast<int>(key) - static_cast<int>(Key::A));
    }
    if (key >= Key::Num0 && key <= Key::Num9) {
        return XK_0 + (static_cast<int>(key) - static_cast<int>(Key::Num0));
    }
    if (key >= Key::F1 && key <= Key::F25) {
        return XK_F1 + (static_cast<int>(key) - static_cast<int>(Key::F1));
    }
    if (key >= Key::KP0 && key <= Key::KP9) {
        return XK_KP_0 + (static_cast<int>(key) - static_cast<int>(Key::KP0));
    }
    switch (key) {
    case Key::Space: return XK_space;
    case Key::Apostrophe: return XK_apostrophe;
    case Key::Comma: return XK_comma;
    case Key::Minus: return XK_minus;
    case Key::Period: return XK_period;
    case Key::Slash: return XK_slash;
    case Key::Semicolon: return XK_semicolon;
    case Key::Equal: return XK_equal;
    case Key::LeftBracket: return XK_bracketleft;
    case Key::Backslash: return XK_backslash;
    case Key::RightBracket: return XK_bracketright;
    case Key::GraveAccent: return XK_grave;
    case Key::Escape: return XK_Escape;
    case Key::Enter: return XK_Return;
    case Key::Tab: return XK_Tab;
    case Key::Backspace: return XK_BackSpace;
    case Key::Insert: return XK_Insert;
    case Key::Delete: return XK_Delete;
    case Key::Right: return XK_Right;
    case Key::Left: return XK_Left;
    case Key::Down: return XK_Down;
    case Key::Up: return XK_Up;
    case Key::PageUp: return XK_Page_Up;
    case Key::PageDown: return XK_Page_Down;
    case Key::Home: return XK_Home;
    case Key::End: return XK_End;
    case Key::CapsLock: return XK_Caps_Lock;
    case Key::ScrollLock: return XK_Scroll_Lock;
    case Key::NumLock: return XK_Num_Lock;
    case Key::PrintScreen: return XK_Print;
    case Key::Pause: return XK_Pause;
    case Key::KPDecimal: return XK_KP_Decimal;
    case Key::KPDivide: return XK_KP_Divide;
    case Key::KPMultiply: return XK_KP_Multiply;
    case Key::KPSubtract: return XK_KP_Subtract;
    case Key::KPAdd: return XK_KP_Add;
    case Key::KPEnter: return XK_KP_Enter;
    case Key::KPEqual: return XK_KP_Equal;
    case Key::LeftShift: return XK_Shift_L;
    case Key::LeftControl: return XK_Control_L;
    case Key::LeftAlt: return XK_Alt_L;
    case Key::LeftSuper: return XK_Super_L;
    case Key::RightShift: return XK_Shift_R;
    case Key::RightControl: return XK_Control_R;
    case Key::RightAlt: return XK_Alt_R;
    case Key::RightSuper: return XK_Super_R;
    case Key::Menu: return XK_Menu;
    case Key::World1: return XK_ISO_Level3_Shift;
    case Key::World2: return XK_ISO_Level5_Shift;
    default: return NoSymbol;
    }
}

int scancodeForKey(Key key) {
    if (!state().display) {
        return -1;
    }
    const KeySym keysym = keysymForKey(key);
    if (keysym == NoSymbol) {
        return -1;
    }
    return static_cast<int>(XKeysymToKeycode(state().display, keysym));
}

std::string keyName(Key key, i32 scancode) {
    (void)scancode;
    static const char* const names[] = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        "backspace", "tab", nullptr, nullptr, "clear", "return", nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, "pause", nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        "space", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",",
        "-", ".", "/", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":",
        ";", "<", "=", ">", "?", "@", "a", "b", "c", "d", "e", "f", "g", "h",
        "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
        "w", "x", "y", "z", "[", "\\", "]", "^", "_", "`",
    };

    const int value = static_cast<int>(key);
    if (value >= 32 && value <= 96) {
        return names[value];
    }
    if (value == static_cast<int>(Key::Escape)) return "escape";
    if (value == static_cast<int>(Key::Enter)) return "enter";
    if (value == static_cast<int>(Key::Tab)) return "tab";
    if (value == static_cast<int>(Key::Backspace)) return "backspace";
    if (value == static_cast<int>(Key::Delete)) return "delete";
    if (value == static_cast<int>(Key::Insert)) return "insert";
    if (value == static_cast<int>(Key::Home)) return "home";
    if (value == static_cast<int>(Key::End)) return "end";
    if (value == static_cast<int>(Key::PageUp)) return "page up";
    if (value == static_cast<int>(Key::PageDown)) return "page down";
    if (value == static_cast<int>(Key::Left)) return "left";
    if (value == static_cast<int>(Key::Right)) return "right";
    if (value == static_cast<int>(Key::Up)) return "up";
    if (value == static_cast<int>(Key::Down)) return "down";
    if (value == static_cast<int>(Key::CapsLock)) return "caps lock";
    if (value == static_cast<int>(Key::ScrollLock)) return "scroll lock";
    if (value == static_cast<int>(Key::NumLock)) return "num lock";
    if (value == static_cast<int>(Key::PrintScreen)) return "print screen";
    if (value == static_cast<int>(Key::Pause)) return "pause";
    if (value >= static_cast<int>(Key::F1) && value <= static_cast<int>(Key::F25)) {
        static std::string buffer;
        buffer = "f" + std::to_string(value - static_cast<int>(Key::F1) + 1);
        return buffer;
    }
    if (value >= static_cast<int>(Key::KP0) && value <= static_cast<int>(Key::KP9)) {
        static std::string buffer;
        buffer = "keypad " + std::to_string(value - static_cast<int>(Key::KP0));
        return buffer;
    }
    if (value == static_cast<int>(Key::KPDecimal)) return "keypad decimal";
    if (value == static_cast<int>(Key::KPDivide)) return "keypad divide";
    if (value == static_cast<int>(Key::KPMultiply)) return "keypad multiply";
    if (value == static_cast<int>(Key::KPSubtract)) return "keypad subtract";
    if (value == static_cast<int>(Key::KPAdd)) return "keypad add";
    if (value == static_cast<int>(Key::KPEnter)) return "keypad enter";
    if (value == static_cast<int>(Key::KPEqual)) return "keypad equal";
    if (value == static_cast<int>(Key::LeftShift)) return "left shift";
    if (value == static_cast<int>(Key::LeftControl)) return "left control";
    if (value == static_cast<int>(Key::LeftAlt)) return "left alt";
    if (value == static_cast<int>(Key::LeftSuper)) return "left super";
    if (value == static_cast<int>(Key::RightShift)) return "right shift";
    if (value == static_cast<int>(Key::RightControl)) return "right control";
    if (value == static_cast<int>(Key::RightAlt)) return "right alt";
    if (value == static_cast<int>(Key::RightSuper)) return "right super";
    if (value == static_cast<int>(Key::Menu)) return "menu";
    return {};
}

void X11WindowBackend::updateModifiers(unsigned int xstate, Modifiers& mods) const {
    if (xstate & ShiftMask) mods |= Modifiers::Shift;
    if (xstate & ControlMask) mods |= Modifiers::Control;
    if (xstate & Mod1Mask) mods |= Modifiers::Alt;
    if (xstate & Mod4Mask) mods |= Modifiers::Super;
    if (xstate & LockMask) mods |= Modifiers::CapsLock;
    if (xstate & Mod2Mask) mods |= Modifiers::NumLock;

    if (!m_lockKeyMods) {
        if (hasModifier(mods, Modifiers::CapsLock)) {
            mods = static_cast<Modifiers>(static_cast<int>(mods) & ~static_cast<int>(Modifiers::CapsLock));
        }
        if (hasModifier(mods, Modifiers::NumLock)) {
            mods = static_cast<Modifiers>(static_cast<int>(mods) & ~static_cast<int>(Modifiers::NumLock));
        }
    }
}

void X11WindowBackend::warpCursorToCenter() {
    XWarpPointer(state().display, X11None, m_window, 0, 0, 0, 0, m_size.x / 2, m_size.y / 2);
    m_lastCursorPos = vec2(static_cast<f32>(m_size.x) * 0.5f, static_cast<f32>(m_size.y) * 0.5f);
}

void X11WindowBackend::updateCursorMode() {
    State& s = state();
    const ::Cursor cursor = m_cursor ? m_cursor->getCursor() : 0;

    switch (m_cursorMode) {
    case CursorMode::Normal:
        XUngrabPointer(s.display, CurrentTime);
        XDefineCursor(s.display, m_window, cursor);
        m_pointerConfined = false;
        break;
    case CursorMode::Hidden:
        XUngrabPointer(s.display, CurrentTime);
        XDefineCursor(s.display, m_window, s.invisibleCursor);
        m_pointerConfined = false;
        break;
    case CursorMode::Disabled:
    case CursorMode::Captured:
        XDefineCursor(s.display, m_window, s.invisibleCursor);
        XGrabPointer(s.display, m_window, True,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                     GrabModeAsync, m_window, s.invisibleCursor, CurrentTime);
        m_pointerConfined = true;
        warpCursorToCenter();
        break;
    }
}

void X11WindowBackend::updateCursorPositionFromEvent(int x, int y) {
    if (m_cursorMode == CursorMode::Disabled || m_cursorMode == CursorMode::Captured) {
        const vec2 center(static_cast<f32>(m_size.x) * 0.5f, static_cast<f32>(m_size.y) * 0.5f);
        if (x == static_cast<int>(center.x) && y == static_cast<int>(center.y)) {
            return;
        }
        if (!m_rawMouseMotion) {
            const vec2 delta(static_cast<f32>(x) - center.x, static_cast<f32>(y) - center.y);
            m_cursorPos += delta;
        }
        warpCursorToCenter();
    } else {
        m_cursorPos = vec2(static_cast<f32>(x), static_cast<f32>(y));
    }
    m_cursorTracked = true;
    if (m_events) {
        m_events->onCursorPos(m_cursorPos);
    }
}

void X11WindowBackend::handleRawMotion(XIRawEvent& event) {
    if (!m_rawMouseMotion || (m_cursorMode != CursorMode::Disabled &&
                              m_cursorMode != CursorMode::Captured)) {
        return;
    }
    vec2 delta(0.0f, 0.0f);
    if (XIMaskIsSet(event.valuators.mask, 0)) {
        delta.x = static_cast<f32>(event.raw_values[0]);
    }
    if (XIMaskIsSet(event.valuators.mask, 1)) {
        delta.y = static_cast<f32>(event.raw_values[XIMaskIsSet(event.valuators.mask, 0) ? 1 : 0]);
    }
    if (delta.x == 0.0f && delta.y == 0.0f) {
        return;
    }
    m_cursorPos += delta;
    m_cursorTracked = true;
    if (m_events) {
        m_events->onCursorPos(m_cursorPos);
    }
}

void X11WindowBackend::handleClientMessage(XClientMessageEvent& event) {
    State& s = state();
    if (event.message_type == s.wmProtocols && event.data.l[0] == static_cast<long>(s.wmDeleteWindow)) {
        m_closeRequested = true;
        if (m_events) {
            m_events->onClose();
        }
        return;
    }
    if (event.message_type == s.netWmState) {
        const long action = event.data.l[0];
        const Atom first = static_cast<Atom>(event.data.l[1]);
        const Atom second = static_cast<Atom>(event.data.l[2]);
        const bool add = action == 1 || action == 2;
        if (first == s.netWmStateMaximizedVert || second == s.netWmStateMaximizedVert ||
            first == s.netWmStateMaximizedHorz || second == s.netWmStateMaximizedHorz) {
            if (add != m_maximized) {
                m_maximized = add;
                if (m_events) {
                    m_events->onMaximize(add);
                }
            }
        }
        if (first == s.netWmStateFullscreen || second == s.netWmStateFullscreen) {
            m_fullscreen = add;
            if (!add && m_restoreWindowedGeometry) {
                XMoveResizeWindow(s.display, m_window, m_windowedPosition.x, m_windowedPosition.y,
                                  static_cast<unsigned int>(std::max(m_windowedSize.x, 1)),
                                  static_cast<unsigned int>(std::max(m_windowedSize.y, 1)));
                m_restoreWindowedGeometry = false;
            }
        }
        return;
    }
    if (event.message_type == s.xdndEnter || event.message_type == s.xdndPosition ||
        event.message_type == s.xdndLeave || event.message_type == s.xdndDrop) {
        handleXdnd(event);
    }
}

void X11WindowBackend::handleEvent(XEvent& event) {
    State& s = state();
    switch (event.type) {
    case ClientMessage:
        handleClientMessage(event.xclient);
        break;

    case ConfigureNotify: {
        const ivec2 newSize(event.xconfigure.width, event.xconfigure.height);
        if (newSize != m_size && newSize.x > 0 && newSize.y > 0) {
            m_size = newSize;
            if (m_events) {
                m_events->onSize(m_size);
                m_events->onFramebufferSize(m_size);
            }
        }
        const ivec2 newPosition(event.xconfigure.x, event.xconfigure.y);
        if (newPosition != m_position) {
            m_position = newPosition;
            if (m_events) {
                m_events->onPosition(newPosition);
            }
        }
        break;
    }

    case PropertyNotify: {
        if (event.xproperty.atom == s.netWmState) {
            Atom actualType = 0;
            int actualFormat = 0;
            unsigned long count = 0;
            unsigned long remaining = 0;
            unsigned char* data = nullptr;
            if (XGetWindowProperty(s.display, m_window, s.netWmState, 0, 64, False, XA_ATOM,
                                   &actualType, &actualFormat, &count, &remaining, &data) == Success &&
                data) {
                const auto* atoms = reinterpret_cast<Atom*>(data);
                bool maximized = false;
                bool fullscreen = false;
                for (unsigned long i = 0; i < count; ++i) {
                    if (atoms[i] == s.netWmStateMaximizedVert ||
                        atoms[i] == s.netWmStateMaximizedHorz) {
                        maximized = true;
                    }
                    if (atoms[i] == s.netWmStateFullscreen) {
                        fullscreen = true;
                    }
                }
                XFree(data);
                if (maximized != m_maximized) {
                    m_maximized = maximized;
                    if (m_events) {
                        m_events->onMaximize(maximized);
                    }
                }
                m_fullscreen = fullscreen;
                if (!fullscreen && m_restoreWindowedGeometry) {
                    XMoveResizeWindow(
                        s.display, m_window, m_windowedPosition.x, m_windowedPosition.y,
                        static_cast<unsigned int>(std::max(m_windowedSize.x, 1)),
                        static_cast<unsigned int>(std::max(m_windowedSize.y, 1)));
                    m_restoreWindowedGeometry = false;
                }
            }
        }
        break;
    }

    case FocusIn:
        if (!m_focused) {
            m_focused = true;
            if (m_ic) {
                XSetICFocus(m_ic);
            }
            if (m_events) {
                m_events->onFocus(true);
            }
        }
        break;

    case FocusOut:
        if (m_focused) {
            m_focused = false;
            if (m_ic) {
                XUnsetICFocus(m_ic);
            }
            if (m_events) {
                m_events->onFocus(false);
            }
        }
        break;

    case MapNotify:
        m_visible = true;
        m_mapped = true;
        if (m_focusPending) {
            focus();
        }
        break;

    case UnmapNotify: {
        m_visible = false;
        m_mapped = false;
        bool iconified = false;
        Atom actualType = 0;
        int actualFormat = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned char* data = nullptr;
        const Atom wmState = XInternAtom(s.display, "WM_STATE", False);
        if (XGetWindowProperty(s.display, m_window, wmState, 0, 2, False, wmState, &actualType,
                               &actualFormat, &count, &remaining, &data) == Success && data) {
            if (count >= 1) {
                const auto* values = reinterpret_cast<long*>(data);
                iconified = values[0] == IconicState;
            }
            XFree(data);
        }
        if (iconified != m_iconified) {
            m_iconified = iconified;
            if (m_events) {
                m_events->onIconify(iconified);
            }
        }
        break;
    }

    case EnterNotify:
        if (!m_hovered) {
            m_hovered = true;
            if (m_events) {
                m_events->onCursorEnter(true);
            }
        }
        m_cursorPos = vec2(static_cast<f32>(event.xcrossing.x), static_cast<f32>(event.xcrossing.y));
        m_cursorTracked = true;
        break;

    case LeaveNotify:
        if (m_hovered) {
            m_hovered = false;
            if (m_events) {
                m_events->onCursorEnter(false);
            }
        }
        m_cursorTracked = false;
        break;

    case MotionNotify:
        updateCursorPositionFromEvent(event.xmotion.x, event.xmotion.y);
        break;

    case ButtonPress:
    case ButtonRelease: {
        const bool pressed = event.type == ButtonPress;
        const unsigned int button = event.xbutton.button;

        if (button >= 4 && button <= 7) {
            if (pressed && m_events) {
                vec2 offset(0.0f, 0.0f);
                if (button == 4) offset.y = 1.0f;
                else if (button == 5) offset.y = -1.0f;
                else if (button == 6) offset.x = 1.0f;
                else offset.x = -1.0f;
                m_events->onScroll(offset);
            }
            break;
        }

        if ((m_cursorMode == CursorMode::Disabled || m_cursorMode == CursorMode::Captured) &&
            pressed) {
            focus();
        }

        MouseButton mouseButton = MouseButton::Left;
        std::size_t index = 0;
        if (button == 1) { mouseButton = MouseButton::Left; index = 0; }
        else if (button == 2) { mouseButton = MouseButton::Middle; index = 2; }
        else if (button == 3) { mouseButton = MouseButton::Right; index = 1; }
        else if (button >= 8 && button <= 11) {
            index = button - 8 + 3;
            mouseButton = static_cast<MouseButton>(index);
        } else {
            break;
        }

        m_pressedMouseButtons[index] = pressed;

        Modifiers mods = Modifiers::None;
        updateModifiers(event.xbutton.state, mods);
        if (m_stickyMouseButtons) {
            if (pressed) {
                const Modifiers flag = static_cast<Modifiers>(1 << index);
                m_stickyButtons = hasModifier(m_stickyButtons, flag)
                                      ? static_cast<Modifiers>(static_cast<int>(m_stickyButtons) &
                                                               ~static_cast<int>(flag))
                                      : m_stickyButtons | flag;
            }
            mods |= m_stickyButtons;
        }

        if (m_events) {
            m_events->onMouseButton(mouseButton, pressed ? KeyAction::Press : KeyAction::Release,
                                    mods);
        }
        break;
    }

    case KeyPress:
    case KeyRelease: {
        const bool pressed = event.type == KeyPress;
        KeySym keysym = XLookupKeysym(&event.xkey, 0);
        Key key = translateKeysym(keysym);
        if (key == Key::Unknown) {
            for (int level = 1; level <= 3 && key == Key::Unknown; ++level) {
                keysym = XLookupKeysym(&event.xkey, level);
                key = translateKeysym(keysym);
            }
        }

        KeyAction action = pressed ? KeyAction::Press : KeyAction::Release;
        if (pressed) {
            if (m_pressedKeys.contains(static_cast<int>(event.xkey.keycode))) {
                action = KeyAction::Repeat;
            } else {
                m_pressedKeys.insert(static_cast<int>(event.xkey.keycode));
            }
        } else {
            m_pressedKeys.erase(static_cast<int>(event.xkey.keycode));
        }

        Modifiers mods = Modifiers::None;
        updateModifiers(event.xkey.state, mods);
        if (m_stickyKeys) {
            if (pressed) {
                for (Modifiers flag : {Modifiers::Shift, Modifiers::Control, Modifiers::Alt,
                                       Modifiers::Super}) {
                    if (hasModifier(mods, flag)) {
                        m_stickyMods = hasModifier(m_stickyMods, flag)
                                           ? static_cast<Modifiers>(static_cast<int>(m_stickyMods) &
                                                                    ~static_cast<int>(flag))
                                           : m_stickyMods | flag;
                    }
                }
            }
            mods |= m_stickyMods;
        }

        if (m_events) {
            m_events->onKey(key, static_cast<i32>(event.xkey.keycode), action, mods);
        }

        if (pressed && action != KeyAction::Repeat && m_ic) {
            char buffer[64];
            KeySym symbol = NoSymbol;
            Status status = 0;
            const int count = Xutf8LookupString(m_ic, &event.xkey, buffer, sizeof(buffer),
                                                &symbol, &status);
            if ((status == XLookupChars || status == XLookupBoth) && count > 0) {
                int offset = 0;
                while (offset < count) {
                    const auto* bytes = reinterpret_cast<const unsigned char*>(buffer + offset);
                    u32 codepoint = 0;
                    int length = 0;
                    if (bytes[0] < 0x80) {
                        codepoint = bytes[0];
                        length = 1;
                    } else if ((bytes[0] & 0xE0) == 0xC0 && offset + 1 < count) {
                        codepoint = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
                        length = 2;
                    } else if ((bytes[0] & 0xF0) == 0xE0 && offset + 2 < count) {
                        codepoint = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) |
                                    (bytes[2] & 0x3F);
                        length = 3;
                    } else if ((bytes[0] & 0xF8) == 0xF0 && offset + 3 < count) {
                        codepoint = ((bytes[0] & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) |
                                    ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
                        length = 4;
                    } else {
                        break;
                    }
                    offset += length;
                    if (codepoint >= 32 || codepoint == 9 || codepoint == 10 || codepoint == 13) {
                        if (m_events) {
                            m_events->onChar(codepoint);
                            m_events->onCharMods(codepoint, mods);
                        }
                    }
                }
            }
        }
        break;
    }

    case SelectionNotify:
        handleSelectionNotify(event.xselection);
        break;

    case SelectionClear:
        handleSelectionClear(event.xselectionclear);
        break;

    case DestroyNotify:
        m_closeRequested = true;
        m_visible = false;
        break;

    default:
        break;
    }
}

i32 X11WindowBackend::getInputMode(InputMode mode) const {
    switch (mode) {
    case InputMode::Cursor: return static_cast<i32>(m_cursorMode);
    case InputMode::StickyKeys: return m_stickyKeys ? 1 : 0;
    case InputMode::StickyMouseButtons: return m_stickyMouseButtons ? 1 : 0;
    case InputMode::LockKeyMods: return m_lockKeyMods ? 1 : 0;
    case InputMode::RawMouseMotion: return m_rawMouseMotion ? 1 : 0;
    }
    return 0;
}

void X11WindowBackend::setInputMode(InputMode mode, i32 value) {
    switch (mode) {
    case InputMode::Cursor:
        m_cursorMode = static_cast<CursorMode>(value);
        updateCursorMode();
        break;
    case InputMode::StickyKeys:
        m_stickyKeys = value != 0;
        break;
    case InputMode::StickyMouseButtons:
        m_stickyMouseButtons = value != 0;
        break;
    case InputMode::LockKeyMods:
        m_lockKeyMods = value != 0;
        break;
    case InputMode::RawMouseMotion:
        m_rawMouseMotion = value != 0;
        break;
    }
}

bool X11WindowBackend::isRawMouseMotionSupported() const {
    return state().hasXi2;
}

std::string X11WindowBackend::getKeyName(Key key, i32 scancode) const {
    return keyName(key, scancode);
}

i32 X11WindowBackend::getKeyScancode(Key key) const {
    return scancodeForKey(key);
}

bool X11WindowBackend::isKeyPressed(Key key) const {
    const int keycode = scancodeForKey(key);
    return keycode > 0 && m_pressedKeys.contains(keycode);
}

bool X11WindowBackend::isMouseButtonPressed(MouseButton button) const {
    const auto index = static_cast<std::size_t>(button);
    return index < m_pressedMouseButtons.size() && m_pressedMouseButtons[index];
}

vec2 X11WindowBackend::getCursorPos() const {
    if (!m_cursorTracked) {
        ::Window rootReturn = 0;
        ::Window childReturn = 0;
        int rootX = 0;
        int rootY = 0;
        int windowX = 0;
        int windowY = 0;
        unsigned int mask = 0;
        if (XQueryPointer(state().display, m_window, &rootReturn, &childReturn, &rootX, &rootY,
                          &windowX, &windowY, &mask)) {
            m_cursorPos = vec2(static_cast<f32>(windowX), static_cast<f32>(windowY));
            m_cursorTracked = true;
        }
    }
    return m_cursorPos;
}

void X11WindowBackend::setCursorPos(vec2 position) {
    XWarpPointer(state().display, X11None, m_window, 0, 0, 0, 0,
                 static_cast<int>(std::lround(position.x)),
                 static_cast<int>(std::lround(position.y)));
    m_cursorPos = position;
    m_cursorTracked = true;
}

void X11WindowBackend::setCursor(CursorBackend* cursor) {
    m_cursor = dynamic_cast<X11CursorBackend*>(cursor);
    if (m_cursorMode == CursorMode::Normal) {
        XDefineCursor(state().display, m_window, m_cursor ? m_cursor->getCursor() : 0);
    }
}

} // namespace sgl::detail::x11
