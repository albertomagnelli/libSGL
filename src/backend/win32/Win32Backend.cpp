#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <mmsystem.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "backend/Backend.hpp"
#include "backend/BackendFactory.hpp"
#include "common/GamepadMapping.hpp"
#include "common/MonitorAccess.hpp"

namespace sgl::detail::win32 {
namespace {

constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
constexpr int WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002;
constexpr int WGL_CONTEXT_FLAGS_ARB = 0x2094;
constexpr int WGL_CONTEXT_DEBUG_BIT_ARB = 0x00000001;
constexpr int WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB = 0x00000002;
constexpr int WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB = 0x00000004;
constexpr int WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB = 0x8256;
constexpr int WGL_NO_RESET_NOTIFICATION_ARB = 0x8261;
constexpr int WGL_LOSE_CONTEXT_ON_RESET_ARB = 0x8252;

using CreateContextAttribsProc = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
using SwapIntervalProc = BOOL(WINAPI*)(int);

struct XInputGamepad {
    WORD buttons;
    BYTE leftTrigger;
    BYTE rightTrigger;
    SHORT leftThumbX;
    SHORT leftThumbY;
    SHORT rightThumbX;
    SHORT rightThumbY;
};

struct XInputState {
    DWORD packetNumber;
    XInputGamepad gamepad;
};

using XInputGetStateProc = DWORD(WINAPI*)(DWORD, XInputState*);

struct XInputVibration {
    WORD leftMotorSpeed;
    WORD rightMotorSpeed;
};

struct XInputCapabilities {
    BYTE type;
    BYTE subType;
    WORD flags;
    XInputGamepad gamepad;
    XInputVibration vibration;
};

using XInputGetCapabilitiesProc = DWORD(WINAPI*)(DWORD, DWORD, XInputCapabilities*);

constexpr WORD XINPUT_GAMEPAD_DPAD_UP = 0x0001;
constexpr WORD XINPUT_GAMEPAD_DPAD_DOWN = 0x0002;
constexpr WORD XINPUT_GAMEPAD_DPAD_LEFT = 0x0004;
constexpr WORD XINPUT_GAMEPAD_DPAD_RIGHT = 0x0008;
constexpr WORD XINPUT_GAMEPAD_START = 0x0010;
constexpr WORD XINPUT_GAMEPAD_BACK = 0x0020;
constexpr WORD XINPUT_GAMEPAD_LEFT_THUMB = 0x0040;
constexpr WORD XINPUT_GAMEPAD_RIGHT_THUMB = 0x0080;
constexpr WORD XINPUT_GAMEPAD_LEFT_SHOULDER = 0x0100;
constexpr WORD XINPUT_GAMEPAD_RIGHT_SHOULDER = 0x0200;
constexpr WORD XINPUT_GAMEPAD_A = 0x1000;
constexpr WORD XINPUT_GAMEPAD_B = 0x2000;
constexpr WORD XINPUT_GAMEPAD_X = 0x4000;
constexpr WORD XINPUT_GAMEPAD_Y = 0x8000;

constexpr BYTE XINPUT_DEVSUBTYPE_GAMEPAD = 0x01;
constexpr BYTE XINPUT_DEVSUBTYPE_WHEEL = 0x02;
constexpr BYTE XINPUT_DEVSUBTYPE_ARCADE_STICK = 0x03;
constexpr BYTE XINPUT_DEVSUBTYPE_FLIGHT_STICK = 0x04;
constexpr BYTE XINPUT_DEVSUBTYPE_DANCE_PAD = 0x05;
constexpr BYTE XINPUT_DEVSUBTYPE_GUITAR = 0x06;
constexpr BYTE XINPUT_DEVSUBTYPE_DRUM_KIT = 0x08;
constexpr WORD XINPUT_CAPS_WIRELESS = 0x0002;

constexpr const char* kGamepadPlatform = "Windows";

struct XInputRaw {
    std::array<f32, 6> axes{};
    std::array<u8, 10> buttons{};
    JoystickHat hat = JoystickHat::Centered;

    XInputRaw() { axes.fill(0.0f); buttons.fill(0); }
};

std::string Utf8FromWide(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(),
                        length, nullptr, nullptr);
    return result;
}

std::wstring WideFromUtf8(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                        length);
    return result;
}

void ReportWin32Error(ErrorCode code, const char* context) {
    const DWORD value = GetLastError();
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s (Win32-Fehler %lu)", context,
                  static_cast<unsigned long>(value));
    ReportError(code, buffer);
}

struct State {
    HINSTANCE instance = nullptr;
    ATOM windowClass = 0;
    std::unordered_map<HWND, class Win32WindowBackend*> windows;
    MonitorCallback monitorCallback;
    std::vector<HMONITOR> monitors;
    bool initialized = false;

    XInputGetStateProc xinputGetState = nullptr;
    XInputGetCapabilitiesProc xinputGetCapabilities = nullptr;
    std::array<bool, 4> joystickConnected{};
    std::function<void(i32, bool)> joystickCallback;

    std::array<bool, 8> pressedKeys{};
    UINT timerResolution = 1;
};

State g_state;

UINT VirtualKeyForKey(Key key);

Key TranslateVirtualKey(WPARAM vk, LPARAM lp) {
    const UINT scancode = static_cast<UINT>((lp >> 16) & 0xFF);
    const bool extended = (lp & (1 << 24)) != 0;

    if (vk == VK_SHIFT) {
        return MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT ? Key::RightShift
                                                                        : Key::LeftShift;
    }
    if (vk == VK_CONTROL) {
        return extended ? Key::RightControl : Key::LeftControl;
    }
    if (vk == VK_MENU) {
        return extended ? Key::RightAlt : Key::LeftAlt;
    }
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<Key>(static_cast<i32>(Key::A) + static_cast<i32>(vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        return static_cast<Key>(static_cast<i32>(Key::Num0) + static_cast<i32>(vk - '0'));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return static_cast<Key>(static_cast<i32>(Key::F1) + static_cast<i32>(vk - VK_F1));
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return static_cast<Key>(static_cast<i32>(Key::KP0) + static_cast<i32>(vk - VK_NUMPAD0));
    }

    switch (vk) {
    case VK_SPACE: return Key::Space;
    case VK_OEM_7: return Key::Apostrophe;
    case VK_OEM_COMMA: return Key::Comma;
    case VK_OEM_MINUS: return Key::Minus;
    case VK_OEM_PERIOD: return Key::Period;
    case VK_OEM_2: return Key::Slash;
    case VK_OEM_1: return Key::Semicolon;
    case VK_OEM_PLUS: return Key::Equal;
    case VK_OEM_4: return Key::LeftBracket;
    case VK_OEM_5: return Key::Backslash;
    case VK_OEM_6: return Key::RightBracket;
    case VK_OEM_3: return Key::GraveAccent;
    case VK_ESCAPE: return Key::Escape;
    case VK_RETURN: return extended ? Key::KPEnter : Key::Enter;
    case VK_TAB: return Key::Tab;
    case VK_BACK: return Key::Backspace;
    case VK_INSERT: return Key::Insert;
    case VK_DELETE: return Key::Delete;
    case VK_RIGHT: return Key::Right;
    case VK_LEFT: return Key::Left;
    case VK_DOWN: return Key::Down;
    case VK_UP: return Key::Up;
    case VK_PRIOR: return Key::PageUp;
    case VK_NEXT: return Key::PageDown;
    case VK_HOME: return Key::Home;
    case VK_END: return Key::End;
    case VK_CAPITAL: return Key::CapsLock;
    case VK_SCROLL: return Key::ScrollLock;
    case VK_NUMLOCK: return Key::NumLock;
    case VK_SNAPSHOT: return Key::PrintScreen;
    case VK_PAUSE: return Key::Pause;
    case VK_DECIMAL: return Key::KPDecimal;
    case VK_DIVIDE: return Key::KPDivide;
    case VK_MULTIPLY: return Key::KPMultiply;
    case VK_SUBTRACT: return Key::KPSubtract;
    case VK_ADD: return Key::KPAdd;
    case VK_SEPARATOR: return Key::KPEqual;
    case VK_LSHIFT: return Key::LeftShift;
    case VK_RSHIFT: return Key::RightShift;
    case VK_LCONTROL: return Key::LeftControl;
    case VK_RCONTROL: return Key::RightControl;
    case VK_LMENU: return Key::LeftAlt;
    case VK_RMENU: return Key::RightAlt;
    case VK_LWIN: return Key::LeftSuper;
    case VK_RWIN: return Key::RightSuper;
    case VK_APPS: return Key::Menu;
    default: return Key::Unknown;
    }
}

Modifiers CurrentModifiers() {
    Modifiers mods = Modifiers::None;
    if (GetKeyState(VK_SHIFT) < 0) mods |= Modifiers::Shift;
    if (GetKeyState(VK_CONTROL) < 0) mods |= Modifiers::Control;
    if (GetKeyState(VK_MENU) < 0) mods |= Modifiers::Alt;
    if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) mods |= Modifiers::Super;
    if ((GetKeyState(VK_CAPITAL) & 1) != 0) mods |= Modifiers::CapsLock;
    if ((GetKeyState(VK_NUMLOCK) & 1) != 0) mods |= Modifiers::NumLock;
    return mods;
}

class Win32BackendImpl;

class Win32CursorBackend final : public CursorBackend {
public:
    explicit Win32CursorBackend(HCURSOR cursor, bool owned) : m_cursor(cursor), m_owned(owned) {}
    ~Win32CursorBackend() override {
        if (m_owned && m_cursor) {
            DestroyCursor(m_cursor);
        }
    }
    [[nodiscard]] void* getNativeHandle() const override { return m_cursor; }
    [[nodiscard]] HCURSOR getCursor() const { return m_cursor; }

private:
    HCURSOR m_cursor = nullptr;
    bool m_owned = true;
};

class Win32WindowBackend final : public WindowBackend {
public:
    Win32WindowBackend(const WindowHints& hints, Error& error);
    ~Win32WindowBackend() override;

    void setEvents(WindowEvents* events) override { m_events = events; }
    void makeContextCurrent() override;
    void swapBuffers() override;
    void* getProcAddress(const char* name) override;
    [[nodiscard]] NativeHandle getNativeHandle() const override;
    [[nodiscard]] bool isCloseRequested() const override { return m_closeRequested; }
    void setCloseRequested(bool requested) override { m_closeRequested = requested; }
    [[nodiscard]] ivec2 getSize() const override { return m_size; }
    [[nodiscard]] ivec2 getFramebufferSize() const override { return m_size; }
    void setSwapInterval(i32 interval) override;
    [[nodiscard]] i32 getSwapInterval() const override { return m_swapInterval; }
    [[nodiscard]] bool isExtensionSupported(const char* name) override;
    [[nodiscard]] std::string getTitle() const override { return m_title; }
    void setTitle(std::string_view title) override;
    void setSize(ivec2 size) override;
    void setPosition(ivec2 position) override;
    [[nodiscard]] ivec2 getPosition() const override;
    [[nodiscard]] ivec4 getFrameSize() const override;
    [[nodiscard]] vec2 getContentScale() const override;
    void show() override;
    void hide() override;
    void focus() override;
    void requestAttention() override;
    void iconify() override;
    void restore() override;
    void maximize() override;
    [[nodiscard]] i32 getAttrib(WindowAttrib attrib) const override;
    void setAttrib(WindowAttrib attrib, bool value) override;
    [[nodiscard]] f32 getOpacity() const override { return m_opacity; }
    void setOpacity(f32 opacity) override;
    void setSizeLimits(ivec2 minimum, ivec2 maximum) override;
    void setAspectRatio(i32 numerator, i32 denominator) override;
    void setIcon(std::span<const WindowIcon> images) override;
    [[nodiscard]] std::vector<WindowIcon> getIcon() const override { return m_icon; }
    void setMonitor(const Monitor& monitor, ivec2 position, ivec2 size, i32 refreshRate) override;
    [[nodiscard]] Monitor getMonitor() const override;
    void setInputMode(InputMode mode, i32 value) override;
    [[nodiscard]] i32 getInputMode(InputMode mode) const override;
    [[nodiscard]] bool isRawMouseMotionSupported() const override { return true; }
    [[nodiscard]] std::string getKeyName(Key key, i32 scancode) const override;
    [[nodiscard]] i32 getKeyScancode(Key key) const override;
    [[nodiscard]] bool isKeyPressed(Key key) const override;
    [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const override;
    [[nodiscard]] vec2 getCursorPos() const override;
    void setCursorPos(vec2 position) override;
    void setCursor(CursorBackend* cursor) override;
    [[nodiscard]] std::string getClipboardString() const override;
    void setClipboardString(std::string_view text) override;

    LRESULT handleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void destroy();

private:
    void createContext(Error& error);
    void updateCursorMode();
    void updateStyle();
    Modifiers modifiersFromMessage(WPARAM wparam) const;

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;

    ivec2 m_size{0, 0};
    ivec2 m_position{0, 0};
    std::string m_title;
    WindowEvents* m_events = nullptr;
    WindowHints m_hints;

    bool m_closeRequested = false;
    bool m_visible = false;
    bool m_iconified = false;
    bool m_maximized = false;
    bool m_focused = false;
    bool m_hovered = false;
    bool m_resizable = true;
    bool m_decorated = true;
    bool m_floating = false;
    bool m_mousePassthrough = false;
    bool m_sizeMoveActive = false;

    i32 m_swapInterval = 0;
    f32 m_opacity = 1.0f;
    CursorMode m_cursorMode = CursorMode::Normal;
    bool m_stickyKeys = false;
    bool m_stickyMouseButtons = false;
    bool m_lockKeyMods = true;
    bool m_rawMouseMotion = false;
    bool m_rawMouseRegistered = false;

    std::unordered_set<WPARAM> m_pressedKeys;
    std::array<bool, 8> m_pressedButtons{};
    Modifiers m_stickyMods = Modifiers::None;
    Modifiers m_stickyButtons = Modifiers::None;

    vec2 m_cursorPos{0.0f, 0.0f};
    vec2 m_rawDelta{0.0f, 0.0f};
    bool m_cursorTracked = false;

    ivec2 m_sizeLimitsMin{0, 0};
    ivec2 m_sizeLimitsMax{0, 0};
    i32 m_aspectNumerator = 0;
    i32 m_aspectDenominator = 0;

    Win32CursorBackend* m_cursor = nullptr;
    HCURSOR m_savedCursor = nullptr;
    std::vector<WindowIcon> m_icon;

    bool m_fullscreen = false;
    Monitor m_fullscreenMonitor;
    RECT m_windowedRect{};
    DWORD m_windowedStyle = 0;
    DWORD m_windowedExStyle = 0;
    u32 m_pendingHighSurrogate = 0;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* window = reinterpret_cast<Win32WindowBackend*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!window) {
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            window = static_cast<Win32WindowBackend*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
    }
    if (window) {
        return window->handleMessage(message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM) {
    g_state.monitors.push_back(monitor);
    return TRUE;
}

bool LoadXInput() {
    if (g_state.xinputGetState) {
        return true;
    }
    for (const wchar_t* name : {L"XInput1_4.dll", L"XInput1_3.dll", L"XInput9_1_0.dll"}) {
        if (HMODULE module = LoadLibraryW(name)) {
            g_state.xinputGetState =
                reinterpret_cast<XInputGetStateProc>(GetProcAddress(module, "XInputGetState"));
            g_state.xinputGetCapabilities = reinterpret_cast<XInputGetCapabilitiesProc>(
                GetProcAddress(module, "XInputGetCapabilities"));
            if (g_state.xinputGetState) {
                return true;
            }
            FreeLibrary(module);
        }
    }
    return false;
}

void PollJoysticks() {
    if (!LoadXInput()) {
        return;
    }
    for (DWORD index = 0; index < 4; ++index) {
        XInputState state{};
        const bool connected = g_state.xinputGetState(index, &state) == ERROR_SUCCESS;
        if (connected != g_state.joystickConnected[index]) {
            g_state.joystickConnected[index] = connected;
            if (g_state.joystickCallback) {
                g_state.joystickCallback(static_cast<i32>(index), connected);
            }
        }
    }
}

XInputRaw ReadXInput(i32 jid) {
    XInputRaw raw;
    if (jid < 0 || jid >= 4 || !LoadXInput()) {
        return raw;
    }
    XInputState native{};
    if (g_state.xinputGetState(static_cast<DWORD>(jid), &native) != ERROR_SUCCESS) {
        return raw;
    }

    const auto& pad = native.gamepad;
    raw.axes[0] = std::clamp(static_cast<f32>(pad.leftThumbX) / 32767.0f, -1.0f, 1.0f);
    // GLFW/SDL-Konvention: Y-Achsen sind negativ nach oben.
    raw.axes[1] = std::clamp(-static_cast<f32>(pad.leftThumbY) / 32767.0f, -1.0f, 1.0f);
    raw.axes[2] = std::clamp(static_cast<f32>(pad.rightThumbX) / 32767.0f, -1.0f, 1.0f);
    raw.axes[3] = std::clamp(-static_cast<f32>(pad.rightThumbY) / 32767.0f, -1.0f, 1.0f);
    raw.axes[4] = static_cast<f32>(pad.leftTrigger) / 127.5f - 1.0f;
    raw.axes[5] = static_cast<f32>(pad.rightTrigger) / 127.5f - 1.0f;

    const std::array<WORD, 10> buttonMasks = {
        XINPUT_GAMEPAD_A,          XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_X,          XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_BACK,       XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
    };
    for (std::size_t i = 0; i < buttonMasks.size(); ++i) {
        raw.buttons[i] = (pad.buttons & buttonMasks[i]) != 0 ? 1 : 0;
    }

    int hat = 0;
    if ((pad.buttons & XINPUT_GAMEPAD_DPAD_UP) != 0) hat |= static_cast<int>(JoystickHat::Up);
    if ((pad.buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0) hat |= static_cast<int>(JoystickHat::Right);
    if ((pad.buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0) hat |= static_cast<int>(JoystickHat::Down);
    if ((pad.buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0) hat |= static_cast<int>(JoystickHat::Left);
    raw.hat = static_cast<JoystickHat>(hat);
    return raw;
}

XInputCapabilities ReadXInputCapabilities(i32 jid) {
    XInputCapabilities capabilities{};
    capabilities.subType = XINPUT_DEVSUBTYPE_GAMEPAD;
    if (jid >= 0 && jid < 4 && LoadXInput() && g_state.xinputGetCapabilities) {
        g_state.xinputGetCapabilities(static_cast<DWORD>(jid), 0, &capabilities);
    }
    return capabilities;
}

std::string XinputGuid(i32 jid) {
    const XInputCapabilities capabilities = ReadXInputCapabilities(jid);
    char guid[33];
    std::snprintf(guid, sizeof(guid), "78696e707574%02x000000000000000000",
                  static_cast<unsigned>(capabilities.subType));
    return guid;
}

std::string XinputName(i32 jid) {
    const XInputCapabilities capabilities = ReadXInputCapabilities(jid);
    switch (capabilities.subType) {
    case XINPUT_DEVSUBTYPE_WHEEL: return "XInput Wheel";
    case XINPUT_DEVSUBTYPE_ARCADE_STICK: return "XInput Arcade Stick";
    case XINPUT_DEVSUBTYPE_FLIGHT_STICK: return "XInput Flight Stick";
    case XINPUT_DEVSUBTYPE_DANCE_PAD: return "XInput Dance Pad";
    case XINPUT_DEVSUBTYPE_GUITAR: return "XInput Guitar";
    case XINPUT_DEVSUBTYPE_DRUM_KIT: return "XInput Drum Kit";
    case XINPUT_DEVSUBTYPE_GAMEPAD:
    default:
        return (capabilities.flags & XINPUT_CAPS_WIRELESS) != 0 ? "Wireless Xbox Controller"
                                                                : "Xbox Controller";
    }
}

} // namespace

class Win32BackendImpl final : public Backend {
public:
    bool init(const InitOptions&, Error& error) override;
    void terminate() override;
    [[nodiscard]] Platform getPlatform() const override { return Platform::Win32; }

    std::unique_ptr<WindowBackend> createWindow(const WindowHints& hints, Error& error) override {
        auto window = std::make_unique<Win32WindowBackend>(hints, error);
        if (error.isError()) {
            return nullptr;
        }
        return window;
    }

    std::unique_ptr<CursorBackend> createCursor(const WindowIcon& image, ivec2 hotspot,
                                                Error& error) override;
    std::unique_ptr<CursorBackend> createStandardCursor(StandardCursor shape, Error& error) override;

    [[nodiscard]] std::vector<Monitor> getMonitors() override {
        std::vector<Monitor> monitors;
        monitors.reserve(g_state.monitors.size());
        for (HMONITOR monitor : g_state.monitors) {
            monitors.push_back(MakeMonitor(monitor));
        }
        return monitors;
    }

    [[nodiscard]] Monitor getPrimaryMonitor() override {
        POINT point{0, 0};
        const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
        return MakeMonitor(monitor);
    }

    void pollEvents() override {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        PollJoysticks();
    }

    void waitEvents(f64 timeout) override {
        const DWORD milliseconds = timeout < 0.0 ? INFINITE : static_cast<DWORD>(timeout * 1000.0);
        MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        pollEvents();
    }

    void postEmptyEvent() override {
        PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
    }

    void setMonitorCallback(MonitorCallback callback) override {
        g_state.monitorCallback = std::move(callback);
    }

    [[nodiscard]] bool isMonitorConnected(const Monitor& monitor) override {
        const HMONITOR handle = static_cast<HMONITOR>(MonitorAccess::Native(monitor));
        return std::find(g_state.monitors.begin(), g_state.monitors.end(), handle) !=
               g_state.monitors.end();
    }

    [[nodiscard]] ivec2 getMonitorPosition(const Monitor& monitor) override {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            return ivec2(info.rcMonitor.left, info.rcMonitor.top);
        }
        return {0, 0};
    }

    [[nodiscard]] Monitor::Workarea getMonitorWorkarea(const Monitor& monitor) override {
        Monitor::Workarea area;
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            area.position = ivec2(info.rcWork.left, info.rcWork.top);
            area.size = ivec2(info.rcWork.right - info.rcWork.left,
                              info.rcWork.bottom - info.rcWork.top);
        }
        return area;
    }

    [[nodiscard]] ivec2 getMonitorPhysicalSize(const Monitor& monitor) override {
        i32 dpiX = 96;
        i32 dpiY = 96;
        GetMonitorDpi(monitor, dpiX, dpiY);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            const i32 width = info.rcMonitor.right - info.rcMonitor.left;
            const i32 height = info.rcMonitor.bottom - info.rcMonitor.top;
            return ivec2(static_cast<i32>(std::lround(width * 25.4 / dpiX)),
                         static_cast<i32>(std::lround(height * 25.4 / dpiY)));
        }
        return {0, 0};
    }

    [[nodiscard]] vec2 getMonitorContentScale(const Monitor& monitor) override {
        i32 dpiX = 96;
        i32 dpiY = 96;
        GetMonitorDpi(monitor, dpiX, dpiY);
        return {static_cast<f32>(dpiX) / 96.0f, static_cast<f32>(dpiY) / 96.0f};
    }

    [[nodiscard]] std::string getMonitorName(const Monitor& monitor) override {
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            return {};
        }
        DISPLAY_DEVICEW device{};
        device.cb = sizeof(device);
        for (DWORD index = 0; EnumDisplayDevicesW(info.szDevice, index, &device, 0); ++index) {
            if ((device.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0) {
                return Utf8FromWide(device.DeviceString);
            }
        }
        return Utf8FromWide(info.szDevice);
    }

    [[nodiscard]] std::vector<VideoMode> getVideoModes(const Monitor& monitor) override {
        std::vector<VideoMode> modes;
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            return modes;
        }
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        for (DWORD index = 0; EnumDisplaySettingsExW(info.szDevice, index, &mode, 0); ++index) {
            VideoMode videoMode;
            videoMode.size = ivec2(static_cast<i32>(mode.dmPelsWidth),
                                   static_cast<i32>(mode.dmPelsHeight));
            videoMode.redBits = static_cast<i32>(mode.dmBitsPerPel >= 24 ? 8 : 5);
            videoMode.greenBits = videoMode.redBits;
            videoMode.blueBits = videoMode.redBits;
            videoMode.refreshRate = static_cast<i32>(mode.dmDisplayFrequency);
            if (videoMode.refreshRate <= 1) {
                videoMode.refreshRate = 0;
            }
            modes.push_back(videoMode);
        }
        return modes;
    }

    [[nodiscard]] VideoMode getVideoMode(const Monitor& monitor) override {
        VideoMode result;
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            return result;
        }
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsExW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0)) {
            result.size = ivec2(static_cast<i32>(mode.dmPelsWidth),
                                static_cast<i32>(mode.dmPelsHeight));
            result.refreshRate = static_cast<i32>(mode.dmDisplayFrequency);
            if (result.refreshRate <= 1) {
                result.refreshRate = 0;
            }
        }
        return result;
    }

    void setGamma(const Monitor& monitor, f32 gamma) override {
        if (gamma <= 0.0f) {
            ReportError(ErrorCode::InvalidValue, "Gamma muss groesser als 0 sein.");
            return;
        }
        GammaRamp ramp;
        for (int i = 0; i < 256; ++i) {
            const double value = std::pow(static_cast<double>(i) / 255.0, 1.0 / gamma);
            ramp.red[static_cast<std::size_t>(i)] = static_cast<u16>(std::lround(value * 65535.0));
            ramp.green[static_cast<std::size_t>(i)] = ramp.red[static_cast<std::size_t>(i)];
            ramp.blue[static_cast<std::size_t>(i)] = ramp.red[static_cast<std::size_t>(i)];
        }
        setGammaRamp(monitor, ramp);
    }

    [[nodiscard]] GammaRamp getGammaRamp(const Monitor& monitor) override {
        GammaRamp result;
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            return result;
        }
        HDC dc = CreateDCW(info.szDevice, nullptr, nullptr, nullptr);
        if (!dc) {
            return result;
        }
        std::array<WORD, 768> ramp{};
        if (GetDeviceGammaRamp(dc, ramp.data())) {
            for (std::size_t i = 0; i < 256; ++i) {
                result.red[i] = ramp[i];
                result.green[i] = ramp[256 + i];
                result.blue[i] = ramp[512 + i];
            }
        }
        DeleteDC(dc);
        return result;
    }

    void setGammaRamp(const Monitor& monitor, const GammaRamp& ramp) override {
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            return;
        }
        HDC dc = CreateDCW(info.szDevice, nullptr, nullptr, nullptr);
        if (!dc) {
            return;
        }
        std::array<WORD, 768> native{};
        for (std::size_t i = 0; i < 256; ++i) {
            native[i] = ramp.red[i];
            native[256 + i] = ramp.green[i];
            native[512 + i] = ramp.blue[i];
        }
        SetDeviceGammaRamp(dc, native.data());
        DeleteDC(dc);
    }

    [[nodiscard]] NativeHandle getMonitorNativeHandle(const Monitor& monitor) override {
        NativeHandle handle;
        handle.platform = NativePlatform::Win32;
        handle.window = MonitorAccess::Native(monitor);
        return handle;
    }

    [[nodiscard]] bool isJoystickPresent(i32 jid) override {
        if (jid < 0 || jid >= 4 || !LoadXInput()) {
            return false;
        }
        XInputState state{};
        return g_state.xinputGetState(static_cast<DWORD>(jid), &state) == ERROR_SUCCESS;
    }

    [[nodiscard]] std::string getJoystickName(i32 jid) override {
        if (!isJoystickPresent(jid)) {
            return {};
        }
        return XinputName(jid);
    }

    [[nodiscard]] std::string getJoystickGUID(i32 jid) override {
        return isJoystickPresent(jid) ? XinputGuid(jid) : std::string{};
    }

    [[nodiscard]] std::span<const f32> getJoystickAxes(i32 jid) override {
        if (!isJoystickPresent(jid)) {
            return {};
        }
        const XInputRaw raw = ReadXInput(jid);
        static std::array<f32, 6> axes{};
        axes = raw.axes;
        return axes;
    }

    [[nodiscard]] std::span<const u8> getJoystickButtons(i32 jid) override {
        if (!isJoystickPresent(jid)) {
            return {};
        }
        const XInputRaw raw = ReadXInput(jid);
        static std::array<u8, 10> buttons{};
        buttons = raw.buttons;
        return buttons;
    }

    [[nodiscard]] std::span<const JoystickHat> getJoystickHats(i32 jid) override {
        if (!isJoystickPresent(jid)) {
            return {};
        }
        const XInputRaw raw = ReadXInput(jid);
        static std::array<JoystickHat, 1> hats{};
        hats[0] = raw.hat;
        return hats;
    }

    [[nodiscard]] bool isGamepad(i32 jid) override {
        if (!isJoystickPresent(jid)) {
            return false;
        }
        return GamepadMappingDatabase::Instance()
            .find(XinputGuid(jid), kGamepadPlatform)
            .has_value();
    }

    [[nodiscard]] std::string getGamepadName(i32 jid) override {
        if (!isJoystickPresent(jid)) {
            return {};
        }
        const auto mapping =
            GamepadMappingDatabase::Instance().find(XinputGuid(jid), kGamepadPlatform);
        return mapping ? mapping->name : std::string{};
    }

    [[nodiscard]] std::optional<GamepadState> getGamepadState(i32 jid) override {
        if (!isJoystickPresent(jid)) {
            return std::nullopt;
        }
        const auto mapping =
            GamepadMappingDatabase::Instance().find(XinputGuid(jid), kGamepadPlatform);
        if (!mapping) {
            return std::nullopt;
        }
        const XInputRaw raw = ReadXInput(jid);
        return ApplyGamepadMapping(*mapping, raw.axes, raw.buttons,
                                   std::span<const JoystickHat>(&raw.hat, 1));
    }

    bool updateGamepadMappings(std::string_view mappings) override {
        return GamepadMappingDatabase::Instance().addMappings(mappings);
    }

    void setJoystickCallback(std::function<void(i32, bool)> callback) override {
        g_state.joystickCallback = std::move(callback);
    }

private:
    static Monitor MakeMonitor(HMONITOR monitor) {
        return MonitorAccess::Make(monitor, Platform::Win32);
    }

    static void GetMonitorDpi(const Monitor& monitor, i32& dpiX, i32& dpiY) {
        using GetDpiForMonitorProc = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
        static auto getDpiForMonitor = [] {
            if (HMODULE module = LoadLibraryW(L"Shcore.dll")) {
                return reinterpret_cast<GetDpiForMonitorProc>(
                    GetProcAddress(module, "GetDpiForMonitor"));
            }
            return static_cast<GetDpiForMonitorProc>(nullptr);
        }();
        dpiX = 96;
        dpiY = 96;
        if (getDpiForMonitor) {
            UINT x = 96;
            UINT y = 96;
            if (SUCCEEDED(getDpiForMonitor(
                    static_cast<HMONITOR>(MonitorAccess::Native(monitor)), 0, &x, &y))) {
                dpiX = static_cast<i32>(x);
                dpiY = static_cast<i32>(y);
            }
        }
    }
};

bool Win32BackendImpl::init(const InitOptions&, Error& error) {
    if (g_state.initialized) {
        return true;
    }
    g_state.instance = GetModuleHandleW(nullptr);
    if (!g_state.instance) {
        error = MakeError(ErrorCode::PlatformError, "Win32-Instanz konnte nicht ermittelt werden.");
        return false;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = g_state.instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = L"sglWindowClass";
    g_state.windowClass = RegisterClassExW(&windowClass);
    if (!g_state.windowClass) {
        ReportWin32Error(ErrorCode::PlatformError,
                         "Win32-Fensterklasse konnte nicht registriert werden");
        error = GetError();
        return false;
    }

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 0);

    timeBeginPeriod(g_state.timerResolution);
    g_state.initialized = true;
    return true;
}

void Win32BackendImpl::terminate() {
    if (!g_state.initialized) {
        return;
    }
    std::vector<Win32WindowBackend*> windows;
    for (auto& [hwnd, window] : g_state.windows) {
        (void)hwnd;
        windows.push_back(window);
    }
    for (auto* window : windows) {
        window->destroy();
    }
    g_state.windows.clear();
    g_state.monitors.clear();
    g_state.monitorCallback = nullptr;
    g_state.joystickCallback = nullptr;
    timeEndPeriod(g_state.timerResolution);
    g_state.initialized = false;
}

std::unique_ptr<CursorBackend> Win32BackendImpl::createCursor(const WindowIcon& image, ivec2 hotspot,
                                                              Error& error) {
    if (image.size.x <= 0 || image.size.y <= 0) {
        error = MakeError(ErrorCode::InvalidValue, "Cursor-Bild hat eine ungueltige Groesse.");
        return nullptr;
    }
    const auto pixels = static_cast<std::size_t>(image.size.x) *
                        static_cast<std::size_t>(image.size.y);
    if (image.rgba8.size() < pixels * 4) {
        error = MakeError(ErrorCode::InvalidValue, "Cursor-Bild enthaelt zu wenige Pixel.");
        return nullptr;
    }

    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = image.size.x;
    header.bV5Height = -image.size.y;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;

    HDC screen = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header),
                                     DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color || !bits) {
        error = MakeError(ErrorCode::OutOfMemory, "Cursor-Bitmap konnte nicht erstellt werden.");
        return nullptr;
    }

    auto* destination = static_cast<u8*>(bits);
    for (std::size_t i = 0; i < pixels; ++i) {
        destination[i * 4 + 0] = image.rgba8[i * 4 + 2];
        destination[i * 4 + 1] = image.rgba8[i * 4 + 1];
        destination[i * 4 + 2] = image.rgba8[i * 4 + 0];
        destination[i * 4 + 3] = image.rgba8[i * 4 + 3];
    }

    HBITMAP mask = CreateBitmap(image.size.x, image.size.y, 1, 1, nullptr);
    ICONINFO info{};
    info.fIcon = FALSE;
    info.xHotspot = static_cast<DWORD>(std::max(hotspot.x, 0));
    info.yHotspot = static_cast<DWORD>(std::max(hotspot.y, 0));
    info.hbmMask = mask;
    info.hbmColor = color;
    HCURSOR cursor = CreateIconIndirect(&info);
    DeleteObject(color);
    DeleteObject(mask);
    if (!cursor) {
        error = MakeError(ErrorCode::PlatformError, "Cursor konnte nicht erstellt werden.");
        return nullptr;
    }
    return std::make_unique<Win32CursorBackend>(cursor, true);
}

std::unique_ptr<CursorBackend> Win32BackendImpl::createStandardCursor(StandardCursor shape,
                                                                      Error& error) {
    LPCWSTR name = IDC_ARROW;
    switch (shape) {
    case StandardCursor::Arrow: name = IDC_ARROW; break;
    case StandardCursor::IBeam: name = IDC_IBEAM; break;
    case StandardCursor::Crosshair: name = IDC_CROSS; break;
    case StandardCursor::Hand: name = IDC_HAND; break;
    case StandardCursor::HResize: name = IDC_SIZEWE; break;
    case StandardCursor::VResize: name = IDC_SIZENS; break;
    }
    HCURSOR cursor = LoadCursorW(nullptr, name);
    if (!cursor) {
        error = MakeError(ErrorCode::PlatformError, "Standard-Cursor konnte nicht geladen werden.");
        return nullptr;
    }
    return std::make_unique<Win32CursorBackend>(cursor, false);
}

Win32WindowBackend::Win32WindowBackend(const WindowHints& hints, Error& error)
    : m_size{hints.width, hints.height}, m_title(hints.title), m_hints(hints) {
    m_resizable = hints.resizable;
    m_decorated = hints.decorated;
    m_floating = hints.floating;
    m_mousePassthrough = hints.mousePassthrough;

    if (hints.contextApi != ContextApi::Native || hints.clientApi != ClientApi::OpenGL) {
        error = MakeError(ErrorCode::FeatureUnavailable,
                          "Das Win32-Backend unterstuetzt nur native WGL-Kontexte.");
        return;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!m_resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (!m_decorated) {
        style &= ~(WS_CAPTION | WS_THICKFRAME);
    }
    if (m_floating) {
        style |= WS_EX_TOPMOST;
    }

    RECT rectangle{0, 0, hints.width, hints.height};
    AdjustWindowRectEx(&rectangle, style, FALSE, 0);

    m_hwnd = CreateWindowExW(
        m_floating ? WS_EX_TOPMOST : 0, L"sglWindowClass", WideFromUtf8(m_title).c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top, nullptr, nullptr, g_state.instance, this);
    if (!m_hwnd) {
        ReportWin32Error(ErrorCode::PlatformError, "Win32-Fenster konnte nicht erstellt werden");
        error = GetError();
        return;
    }

    g_state.windows[m_hwnd] = this;

    m_hdc = GetDC(m_hwnd);
    createContext(error);
    if (error.isError()) {
        return;
    }

    if (!hints.autoIconify) {
        // Nothing to configure; Windows always iconifies on minimise.
    }

    DragAcceptFiles(m_hwnd, TRUE);

    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = m_hwnd;
    m_rawMouseRegistered = RegisterRawInputDevices(&device, 1, sizeof(device)) == TRUE;

    updateStyle();
    if (hints.visible) {
        show();
    }
    if (hints.maximized) {
        maximize();
    }
    if (hints.focused) {
        focus();
    }
}

Win32WindowBackend::~Win32WindowBackend() {
    destroy();
}

void Win32WindowBackend::createContext(Error& error) {
    PIXELFORMATDESCRIPTOR descriptor{};
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = static_cast<BYTE>(m_hints.redBits + m_hints.greenBits + m_hints.blueBits);
    descriptor.cAlphaBits = static_cast<BYTE>(m_hints.alphaBits);
    descriptor.cDepthBits = static_cast<BYTE>(m_hints.depthBits);
    descriptor.cStencilBits = static_cast<BYTE>(m_hints.stencilBits);
    descriptor.iLayerType = PFD_MAIN_PLANE;

    const int format = ChoosePixelFormat(m_hdc, &descriptor);
    if (format == 0 || !SetPixelFormat(m_hdc, format, &descriptor)) {
        ReportWin32Error(ErrorCode::FormatUnavailable,
                         "Kein passendes Win32-Pixelformat gefunden");
        error = GetError();
        return;
    }

    HGLRC legacy = wglCreateContext(m_hdc);
    if (!legacy) {
        ReportWin32Error(ErrorCode::ApiUnavailable, "WGL-Kontext konnte nicht erstellt werden");
        error = GetError();
        return;
    }
    wglMakeCurrent(m_hdc, legacy);

    auto createContextAttribs = reinterpret_cast<CreateContextAttribsProc>(
        wglGetProcAddress("wglCreateContextAttribsARB"));

    if (createContextAttribs) {
        std::vector<int> attribs;
        attribs.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
        attribs.push_back(m_hints.contextVersionMajor);
        attribs.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
        attribs.push_back(m_hints.contextVersionMinor);
        if (m_hints.contextProfile == ContextProfile::Core) {
            attribs.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
            attribs.push_back(WGL_CONTEXT_CORE_PROFILE_BIT_ARB);
        } else if (m_hints.contextProfile == ContextProfile::Compatibility) {
            attribs.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
            attribs.push_back(WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB);
        }
        int flags = 0;
        if (m_hints.contextDebug) flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
        if (m_hints.contextForwardCompat) flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
        if (m_hints.contextRobustness != ContextRobustness::None) {
            flags |= WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB;
        }
        if (flags != 0) {
            attribs.push_back(WGL_CONTEXT_FLAGS_ARB);
            attribs.push_back(flags);
        }
        if (m_hints.contextRobustness != ContextRobustness::None) {
            attribs.push_back(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
            attribs.push_back(m_hints.contextRobustness == ContextRobustness::LoseContextOnReset
                                  ? WGL_LOSE_CONTEXT_ON_RESET_ARB
                                  : WGL_NO_RESET_NOTIFICATION_ARB);
        }
        attribs.push_back(0);
        m_hglrc = createContextAttribs(m_hdc, nullptr, attribs.data());
    }

    if (!m_hglrc) {
        m_hglrc = legacy;
        legacy = nullptr;
    } else {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacy);
    }

    if (!m_hglrc) {
        error = MakeError(ErrorCode::VersionUnavailable,
                          "Der angeforderte OpenGL-Kontext konnte nicht erstellt werden.");
        return;
    }
    wglMakeCurrent(nullptr, nullptr);
}

void Win32WindowBackend::destroy() {
    if (!m_hwnd) {
        return;
    }
    if (m_hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;
    }
    if (m_hdc) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }
    g_state.windows.erase(m_hwnd);
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
    m_visible = false;
    m_iconified = false;
}

void Win32WindowBackend::makeContextCurrent() {
    if (m_hdc && m_hglrc) {
        wglMakeCurrent(m_hdc, m_hglrc);
    }
}

void Win32WindowBackend::swapBuffers() {
    if (m_hdc) {
        SwapBuffers(m_hdc);
    }
}

void* Win32WindowBackend::getProcAddress(const char* name) {
    void* address = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (address == nullptr || address == reinterpret_cast<void*>(1) ||
        address == reinterpret_cast<void*>(2) || address == reinterpret_cast<void*>(3) ||
        address == reinterpret_cast<void*>(-1)) {
        static HMODULE opengl = LoadLibraryW(L"opengl32.dll");
        address = opengl ? reinterpret_cast<void*>(GetProcAddress(opengl, name)) : nullptr;
    }
    return address;
}

NativeHandle Win32WindowBackend::getNativeHandle() const {
    NativeHandle handle;
    handle.platform = NativePlatform::Win32;
    handle.window = m_hwnd;
    handle.context = m_hglrc;
    handle.surface = m_hdc;
    return handle;
}

void Win32WindowBackend::setSwapInterval(i32 interval) {
    m_swapInterval = interval;
    auto swapInterval =
        reinterpret_cast<SwapIntervalProc>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (!swapInterval) {
        swapInterval = reinterpret_cast<SwapIntervalProc>(wglGetProcAddress("wglSwapIntervalMESA"));
    }
    if (swapInterval) {
        swapInterval(interval);
    }
}

bool Win32WindowBackend::isExtensionSupported(const char* name) {
    using GetStringiProc = const unsigned char*(APIENTRY*)(unsigned int, unsigned int);
    using GetIntegervProc = void(APIENTRY*)(unsigned int, int*);
    auto getStringi = reinterpret_cast<GetStringiProc>(getProcAddress("glGetStringi"));
    auto getIntegerv = reinterpret_cast<GetIntegervProc>(getProcAddress("glGetIntegerv"));
    if (!getStringi || !getIntegerv) {
        return false;
    }
    constexpr unsigned int GL_EXTENSIONS_ENUM = 0x1F03;
    constexpr unsigned int GL_NUM_EXTENSIONS_ENUM = 0x821D;
    int count = 0;
    getIntegerv(GL_NUM_EXTENSIONS_ENUM, &count);
    for (int i = 0; i < count; ++i) {
        const auto* extension = reinterpret_cast<const char*>(
            getStringi(GL_EXTENSIONS_ENUM, static_cast<unsigned int>(i)));
        if (extension && std::strcmp(extension, name) == 0) {
            return true;
        }
    }
    return false;
}

void Win32WindowBackend::setTitle(std::string_view title) {
    m_title = std::string(title);
    SetWindowTextW(m_hwnd, WideFromUtf8(m_title).c_str());
}

void Win32WindowBackend::setSize(ivec2 size) {
    RECT rectangle{0, 0, size.x, size.y};
    AdjustWindowRectEx(&rectangle, GetWindowLongW(m_hwnd, GWL_STYLE), FALSE,
                       GetWindowLongW(m_hwnd, GWL_EXSTYLE));
    SetWindowPos(m_hwnd, nullptr, 0, 0, rectangle.right - rectangle.left,
                 rectangle.bottom - rectangle.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32WindowBackend::setPosition(ivec2 position) {
    RECT rectangle{position.x, position.y, position.x, position.y};
    AdjustWindowRectEx(&rectangle, static_cast<DWORD>(GetWindowLongW(m_hwnd, GWL_STYLE)), FALSE,
                       static_cast<DWORD>(GetWindowLongW(m_hwnd, GWL_EXSTYLE)));
    SetWindowPos(m_hwnd, nullptr, rectangle.left, rectangle.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

ivec2 Win32WindowBackend::getPosition() const {
    POINT point{0, 0};
    ClientToScreen(m_hwnd, &point);
    return ivec2(point.x, point.y);
}

ivec4 Win32WindowBackend::getFrameSize() const {
    RECT windowRect{};
    RECT clientRect{};
    POINT clientOrigin{0, 0};
    GetWindowRect(m_hwnd, &windowRect);
    GetClientRect(m_hwnd, &clientRect);
    ClientToScreen(m_hwnd, &clientOrigin);
    const i32 clientWidth = clientRect.right - clientRect.left;
    const i32 clientHeight = clientRect.bottom - clientRect.top;
    const i32 left = clientOrigin.x - windowRect.left;
    const i32 top = clientOrigin.y - windowRect.top;
    const i32 right = windowRect.right - clientOrigin.x - clientWidth;
    const i32 bottom = windowRect.bottom - clientOrigin.y - clientHeight;
    return ivec4(left, top, right, bottom);
}

vec2 Win32WindowBackend::getContentScale() const {
    using GetDpiForWindowProc = UINT(WINAPI*)(HWND);
    static auto getDpiForWindow = [] {
        if (HMODULE module = GetModuleHandleW(L"user32.dll")) {
            return reinterpret_cast<GetDpiForWindowProc>(
                GetProcAddress(module, "GetDpiForWindow"));
        }
        return static_cast<GetDpiForWindowProc>(nullptr);
    }();
    const UINT dpi = getDpiForWindow ? getDpiForWindow(m_hwnd) : 96;
    const f32 scale = dpi > 0 ? static_cast<f32>(dpi) / 96.0f : 1.0f;
    return {scale, scale};
}

void Win32WindowBackend::show() {
    ShowWindow(m_hwnd, SW_SHOW);
    m_visible = true;
}

void Win32WindowBackend::hide() {
    ShowWindow(m_hwnd, SW_HIDE);
    m_visible = false;
}

void Win32WindowBackend::focus() {
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
}

void Win32WindowBackend::requestAttention() {
    FLASHWINFO info{};
    info.cbSize = sizeof(info);
    info.hwnd = m_hwnd;
    info.dwFlags = FLASHW_ALL;
    info.uCount = 3;
    FlashWindowEx(&info);
}

void Win32WindowBackend::iconify() {
    ShowWindow(m_hwnd, SW_MINIMIZE);
}

void Win32WindowBackend::restore() {
    ShowWindow(m_hwnd, SW_RESTORE);
    m_iconified = false;
}

void Win32WindowBackend::maximize() {
    ShowWindow(m_hwnd, SW_MAXIMIZE);
}

i32 Win32WindowBackend::getAttrib(WindowAttrib attrib) const {
    switch (attrib) {
    case WindowAttrib::Focused: return GetForegroundWindow() == m_hwnd ? 1 : 0;
    case WindowAttrib::Iconified: return IsIconic(m_hwnd) ? 1 : 0;
    case WindowAttrib::Resizable: return m_resizable ? 1 : 0;
    case WindowAttrib::Visible: return IsWindowVisible(m_hwnd) ? 1 : 0;
    case WindowAttrib::Decorated: return m_decorated ? 1 : 0;
    case WindowAttrib::AutoIconify: return m_hints.autoIconify ? 1 : 0;
    case WindowAttrib::Floating: return m_floating ? 1 : 0;
    case WindowAttrib::Maximized: return IsZoomed(m_hwnd) ? 1 : 0;
    case WindowAttrib::Hovered: return m_hovered ? 1 : 0;
    case WindowAttrib::FocusOnShow: return m_hints.focusOnShow ? 1 : 0;
    case WindowAttrib::MousePassthrough: return m_mousePassthrough ? 1 : 0;
    case WindowAttrib::ScaleToMonitor: return m_hints.scaleToMonitor ? 1 : 0;
    case WindowAttrib::Doublebuffer: return m_hints.doublebuffer ? 1 : 0;
    case WindowAttrib::ClientApi: return static_cast<i32>(m_hints.clientApi);
    case WindowAttrib::ContextCreationApi: return static_cast<i32>(m_hints.contextApi);
    case WindowAttrib::ContextVersionMajor: return m_hints.contextVersionMajor;
    case WindowAttrib::ContextVersionMinor: return m_hints.contextVersionMinor;
    case WindowAttrib::ContextRevision: return 0;
    case WindowAttrib::ContextRobustness: return static_cast<i32>(m_hints.contextRobustness);
    case WindowAttrib::ContextReleaseBehavior: return static_cast<i32>(m_hints.contextReleaseBehavior);
    case WindowAttrib::ContextDebug: return m_hints.contextDebug ? 1 : 0;
    case WindowAttrib::ContextNoError: return m_hints.contextNoError ? 1 : 0;
    case WindowAttrib::ContextForwardCompat: return m_hints.contextForwardCompat ? 1 : 0;
    case WindowAttrib::OpenGLProfile: return static_cast<i32>(m_hints.contextProfile);
    case WindowAttrib::Samples: return m_hints.samples;
    case WindowAttrib::RedBits: return m_hints.redBits;
    case WindowAttrib::GreenBits: return m_hints.greenBits;
    case WindowAttrib::BlueBits: return m_hints.blueBits;
    case WindowAttrib::AlphaBits: return m_hints.alphaBits;
    case WindowAttrib::DepthBits: return m_hints.depthBits;
    case WindowAttrib::StencilBits: return m_hints.stencilBits;
    case WindowAttrib::AccumRedBits: return m_hints.accumRedBits;
    case WindowAttrib::AccumGreenBits: return m_hints.accumGreenBits;
    case WindowAttrib::AccumBlueBits: return m_hints.accumBlueBits;
    case WindowAttrib::AccumAlphaBits: return m_hints.accumAlphaBits;
    case WindowAttrib::AuxBuffers: return m_hints.auxBuffers;
    case WindowAttrib::Stereo: return m_hints.stereo ? 1 : 0;
    case WindowAttrib::SRGBCapable: return m_hints.srgbCapable ? 1 : 0;
    case WindowAttrib::RefreshRate: return m_hints.refreshRate;
    case WindowAttrib::TransparentFramebuffer: return m_hints.transparentFramebuffer ? 1 : 0;
    case WindowAttrib::CenterCursor: return 0;
    }
    return 0;
}

void Win32WindowBackend::updateStyle() {
    LONG_PTR style = GetWindowLongPtrW(m_hwnd, GWL_STYLE);
    if (m_resizable) {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    } else {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (m_decorated) {
        style |= WS_CAPTION;
    } else {
        style &= ~WS_CAPTION;
    }
    SetWindowLongPtrW(m_hwnd, GWL_STYLE, style);
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void Win32WindowBackend::setAttrib(WindowAttrib attrib, bool value) {
    switch (attrib) {
    case WindowAttrib::Visible:
        value ? show() : hide();
        break;
    case WindowAttrib::Resizable:
        m_resizable = value;
        updateStyle();
        break;
    case WindowAttrib::Decorated:
        m_decorated = value;
        updateStyle();
        break;
    case WindowAttrib::Floating:
        m_floating = value;
        SetWindowPos(m_hwnd, value ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
        break;
    case WindowAttrib::Maximized:
        value ? maximize() : restore();
        break;
    case WindowAttrib::MousePassthrough:
        m_mousePassthrough = value;
        break;
    default:
        break;
    }
}

void Win32WindowBackend::setOpacity(f32 opacity) {
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if (m_opacity < 1.0f) {
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(m_hwnd, 0, static_cast<BYTE>(m_opacity * 255.0f), LWA_ALPHA);
    } else {
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    }
}


void Win32WindowBackend::setSizeLimits(ivec2 minimum, ivec2 maximum) {
    m_sizeLimitsMin = minimum;
    m_sizeLimitsMax = maximum;
}

void Win32WindowBackend::setAspectRatio(i32 numerator, i32 denominator) {
    m_aspectNumerator = numerator;
    m_aspectDenominator = denominator;
}

void Win32WindowBackend::setIcon(std::span<const WindowIcon> images) {
    m_icon.assign(images.begin(), images.end());
    if (images.empty() || images[0].size.x <= 0) {
        return;
    }
    const WindowIcon& image = images[0];
    const auto pixels = static_cast<std::size_t>(image.size.x) *
                        static_cast<std::size_t>(image.size.y);
    if (image.rgba8.size() < pixels * 4) {
        return;
    }

    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = image.size.x;
    header.bV5Height = -image.size.y;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;

    HDC screen = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header),
                                     DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color || !bits) {
        return;
    }
    auto* destination = static_cast<u8*>(bits);
    for (std::size_t i = 0; i < pixels; ++i) {
        destination[i * 4 + 0] = image.rgba8[i * 4 + 2];
        destination[i * 4 + 1] = image.rgba8[i * 4 + 1];
        destination[i * 4 + 2] = image.rgba8[i * 4 + 0];
        destination[i * 4 + 3] = image.rgba8[i * 4 + 3];
    }
    HBITMAP mask = CreateBitmap(image.size.x, image.size.y, 1, 1, nullptr);
    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmMask = mask;
    info.hbmColor = color;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(color);
    DeleteObject(mask);
    if (icon) {
        SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        if (m_hints.maximized) {
            DestroyIcon(icon);
        }
    }
}

void Win32WindowBackend::setMonitor(const Monitor& monitor, ivec2 position, ivec2 size,
                                    i32 refreshRate) {
    if (monitor.isValid() && size.x > 0 && size.y > 0) {
        if (!m_fullscreen) {
            GetWindowRect(m_hwnd, &m_windowedRect);
            m_windowedStyle = static_cast<DWORD>(GetWindowLongW(m_hwnd, GWL_STYLE));
            m_windowedExStyle = static_cast<DWORD>(GetWindowLongW(m_hwnd, GWL_EXSTYLE));
        }
        m_fullscreen = true;
        m_fullscreenMonitor = monitor;

        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(static_cast<HMONITOR>(MonitorAccess::Native(monitor)), &info)) {
            SetWindowLongW(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(m_hwnd, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
                         info.rcMonitor.right - info.rcMonitor.left,
                         info.rcMonitor.bottom - info.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
        (void)position;
        (void)refreshRate;
    } else if (m_fullscreen) {
        m_fullscreen = false;
        m_fullscreenMonitor = Monitor{};
        SetWindowLongW(m_hwnd, GWL_STYLE, static_cast<LONG>(m_windowedStyle));
        SetWindowLongW(m_hwnd, GWL_EXSTYLE, static_cast<LONG>(m_windowedExStyle));
        SetWindowPos(m_hwnd, nullptr, m_windowedRect.left, m_windowedRect.top,
                     m_windowedRect.right - m_windowedRect.left,
                     m_windowedRect.bottom - m_windowedRect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

Monitor Win32WindowBackend::getMonitor() const {
    if (m_fullscreen && m_fullscreenMonitor.isValid()) {
        return m_fullscreenMonitor;
    }
    const HMONITOR monitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    return MonitorAccess::Make(monitor, Platform::Win32);
}

void Win32WindowBackend::updateCursorMode() {
    switch (m_cursorMode) {
    case CursorMode::Normal:
        ReleaseCapture();
        SetCursor(m_cursor ? m_cursor->getCursor() : LoadCursorW(nullptr, IDC_ARROW));
        break;
    case CursorMode::Hidden:
        ReleaseCapture();
        SetCursor(nullptr);
        break;
    case CursorMode::Disabled:
    case CursorMode::Captured: {
        SetCapture(m_hwnd);
        SetCursor(nullptr);
        RECT client{};
        GetClientRect(m_hwnd, &client);
        POINT center{(client.right - client.left) / 2, (client.bottom - client.top) / 2};
        ClientToScreen(m_hwnd, &center);
        SetCursorPos(center.x, center.y);
        break;
    }
    }
}

void Win32WindowBackend::setInputMode(InputMode mode, i32 value) {
    switch (mode) {
    case InputMode::Cursor:
        m_cursorMode = static_cast<CursorMode>(value);
        updateCursorMode();
        break;
    case InputMode::StickyKeys: m_stickyKeys = value != 0; break;
    case InputMode::StickyMouseButtons: m_stickyMouseButtons = value != 0; break;
    case InputMode::LockKeyMods: m_lockKeyMods = value != 0; break;
    case InputMode::RawMouseMotion: m_rawMouseMotion = value != 0; break;
    }
}

i32 Win32WindowBackend::getInputMode(InputMode mode) const {
    switch (mode) {
    case InputMode::Cursor: return static_cast<i32>(m_cursorMode);
    case InputMode::StickyKeys: return m_stickyKeys ? 1 : 0;
    case InputMode::StickyMouseButtons: return m_stickyMouseButtons ? 1 : 0;
    case InputMode::LockKeyMods: return m_lockKeyMods ? 1 : 0;
    case InputMode::RawMouseMotion: return m_rawMouseMotion ? 1 : 0;
    }
    return 0;
}

std::string Win32WindowBackend::getKeyName(Key key, i32 scancode) const {
    (void)scancode;
    if (key >= Key::A && key <= Key::Z) {
        return std::string(1, static_cast<char>('a' + (static_cast<i32>(key) - static_cast<i32>(Key::A))));
    }
    if (key >= Key::Num0 && key <= Key::Num9) {
        return std::string(1, static_cast<char>('0' + (static_cast<i32>(key) - static_cast<i32>(Key::Num0))));
    }
    if (key == Key::Space) return " ";
    if (key == Key::Escape) return "escape";
    if (key == Key::Enter) return "enter";
    if (key == Key::Tab) return "tab";
    if (key == Key::Backspace) return "backspace";
    if (key == Key::Delete) return "delete";
    if (key == Key::Insert) return "insert";
    if (key == Key::Left) return "left";
    if (key == Key::Right) return "right";
    if (key == Key::Up) return "up";
    if (key == Key::Down) return "down";
    if (key >= Key::F1 && key <= Key::F25) {
        return "f" + std::to_string(static_cast<i32>(key) - static_cast<i32>(Key::F1) + 1);
    }
    return {};
}

i32 Win32WindowBackend::getKeyScancode(Key key) const {
    const UINT vk = static_cast<UINT>(MapVirtualKeyW(VirtualKeyForKey(key), MAPVK_VK_TO_VSC));
    return vk == 0 ? -1 : static_cast<i32>(vk);
}

namespace {
UINT VirtualKeyForKey(Key key) {
    if (key >= Key::A && key <= Key::Z) {
        return 'A' + static_cast<UINT>(key) - static_cast<UINT>(Key::A);
    }
    if (key >= Key::Num0 && key <= Key::Num9) {
        return '0' + static_cast<UINT>(key) - static_cast<UINT>(Key::Num0);
    }
    if (key >= Key::F1 && key <= Key::F24) {
        return VK_F1 + static_cast<UINT>(key) - static_cast<UINT>(Key::F1);
    }
    if (key >= Key::KP0 && key <= Key::KP9) {
        return VK_NUMPAD0 + static_cast<UINT>(key) - static_cast<UINT>(Key::KP0);
    }
    switch (key) {
    case Key::Space: return VK_SPACE;
    case Key::Escape: return VK_ESCAPE;
    case Key::Enter: return VK_RETURN;
    case Key::Tab: return VK_TAB;
    case Key::Backspace: return VK_BACK;
    case Key::Delete: return VK_DELETE;
    case Key::Insert: return VK_INSERT;
    case Key::Left: return VK_LEFT;
    case Key::Right: return VK_RIGHT;
    case Key::Up: return VK_UP;
    case Key::Down: return VK_DOWN;
    case Key::Home: return VK_HOME;
    case Key::End: return VK_END;
    case Key::PageUp: return VK_PRIOR;
    case Key::PageDown: return VK_NEXT;
    case Key::LeftShift: return VK_LSHIFT;
    case Key::RightShift: return VK_RSHIFT;
    case Key::LeftControl: return VK_LCONTROL;
    case Key::RightControl: return VK_RCONTROL;
    case Key::LeftAlt: return VK_LMENU;
    case Key::RightAlt: return VK_RMENU;
    case Key::LeftSuper: return VK_LWIN;
    case Key::RightSuper: return VK_RWIN;
    default: return 0;
    }
}
} // namespace

bool Win32WindowBackend::isKeyPressed(Key key) const {
    const UINT vk = VirtualKeyForKey(key);
    if (vk == 0) {
        return false;
    }
    return (GetKeyState(static_cast<int>(vk)) & 0x8000) != 0;
}

bool Win32WindowBackend::isMouseButtonPressed(MouseButton button) const {
    const auto index = static_cast<std::size_t>(button);
    return index < m_pressedButtons.size() && m_pressedButtons[index];
}

vec2 Win32WindowBackend::getCursorPos() const {
    POINT point{};
    if (GetCursorPos(&point)) {
        ScreenToClient(m_hwnd, &point);
        return vec2(static_cast<f32>(point.x), static_cast<f32>(point.y));
    }
    return m_cursorPos;
}

void Win32WindowBackend::setCursorPos(vec2 position) {
    POINT point{static_cast<LONG>(position.x), static_cast<LONG>(position.y)};
    ClientToScreen(m_hwnd, &point);
    SetCursorPos(point.x, point.y);
    m_cursorPos = position;
}

void Win32WindowBackend::setCursor(CursorBackend* cursor) {
    m_cursor = dynamic_cast<Win32CursorBackend*>(cursor);
    updateCursorMode();
}

std::string Win32WindowBackend::getClipboardString() const {
    if (!OpenClipboard(m_hwnd)) {
        return {};
    }
    std::string result;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (text) {
            result = Utf8FromWide(text);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return result;
}

void Win32WindowBackend::setClipboardString(std::string_view text) {
    if (!OpenClipboard(m_hwnd)) {
        return;
    }
    EmptyClipboard();
    const std::wstring wide = WideFromUtf8(text);
    const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* destination = GlobalLock(memory)) {
            std::memcpy(destination, wide.c_str(), bytes);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
        }
    }
    CloseClipboard();
}

LRESULT Win32WindowBackend::handleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE:
        m_closeRequested = true;
        if (m_events) m_events->onClose();
        return 0;

    case WM_DESTROY:
        return 0;

    case WM_SIZE: {
        if (wparam == SIZE_MINIMIZED) {
            if (!m_iconified) {
                m_iconified = true;
                m_visible = false;
                if (m_events) m_events->onIconify(true);
            }
        } else {
            if (m_iconified) {
                m_iconified = false;
                if (m_events) m_events->onIconify(false);
            }
            m_visible = true;
            const ivec2 size(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            if (size != m_size && size.x > 0 && size.y > 0) {
                m_size = size;
                if (m_events) {
                    m_events->onSize(size);
                    m_events->onFramebufferSize(size);
                }
            }
        }
        return 0;
    }

    case WM_MOVE: {
        const ivec2 position = getPosition();
        if (position != m_position) {
            m_position = position;
            if (m_events) m_events->onPosition(position);
        }
        return 0;
    }

    case WM_SETFOCUS:
        if (!m_focused) {
            m_focused = true;
            if (m_events) m_events->onFocus(true);
        }
        return 0;

    case WM_KILLFOCUS:
        if (m_focused) {
            m_focused = false;
            if (m_events) m_events->onFocus(false);
        }
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        if (message == WM_SYSKEYDOWN && wparam == VK_F4) {
            return DefWindowProcW(m_hwnd, message, wparam, lparam);
        }
        if (message == WM_SYSKEYDOWN && wparam == VK_MENU && !m_hints.win32KeyboardMenu) {
            return 0;
        }
        const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        const bool repeat = pressed && (lparam & (1 << 30)) != 0;
        const Key key = TranslateVirtualKey(wparam, lparam);
        Modifiers mods = CurrentModifiers();
        if (m_stickyKeys) {
            if (pressed && !repeat) {
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
            m_events->onKey(key, static_cast<i32>((lparam >> 16) & 0xFF),
                            pressed ? (repeat ? KeyAction::Repeat : KeyAction::Press)
                                    : KeyAction::Release,
                            mods);
        }
        if (!pressed && key == Key::Menu) {
            return 0;
        }
        return 0;
    }

    case WM_CHAR: {
        u32 codepoint = static_cast<u32>(wparam);
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            m_pendingHighSurrogate = codepoint;
            return 0;
        }
        if (codepoint >= 0xDC00 && codepoint <= 0xDFFF && m_pendingHighSurrogate != 0) {
            codepoint = 0x10000 + ((m_pendingHighSurrogate - 0xD800) << 10) + (codepoint - 0xDC00);
            m_pendingHighSurrogate = 0;
        } else {
            m_pendingHighSurrogate = 0;
        }
        if (m_events && (codepoint >= 32 || codepoint == 9 || codepoint == 10 || codepoint == 13)) {
            m_events->onChar(codepoint);
            m_events->onCharMods(codepoint, CurrentModifiers());
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        m_cursorPos = vec2(static_cast<f32>(point.x), static_cast<f32>(point.y));
        m_hovered = true;
        if (m_cursorMode == CursorMode::Disabled || m_cursorMode == CursorMode::Captured) {
            if (m_rawMouseMotion) {
                return 0;
            }
            const vec2 center(static_cast<f32>(m_size.x) * 0.5f,
                              static_cast<f32>(m_size.y) * 0.5f);
            m_cursorPos += vec2(static_cast<f32>(point.x) - center.x,
                                static_cast<f32>(point.y) - center.y);
        }
        if (m_events) m_events->onCursorPos(m_cursorPos);
        return 0;
    }

    case WM_MOUSELEAVE:
        m_hovered = false;
        if (m_events) m_events->onCursorEnter(false);
        return 0;

    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: {
        const bool pressed = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                             message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
        MouseButton button = MouseButton::Left;
        std::size_t index = 0;
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) {
            button = MouseButton::Left; index = 0;
        } else if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) {
            button = MouseButton::Right; index = 1;
        } else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) {
            button = MouseButton::Middle; index = 2;
        } else {
            const WORD xbutton = GET_XBUTTON_WPARAM(wparam);
            index = xbutton == XBUTTON1 ? 3 : 4;
            button = static_cast<MouseButton>(index);
        }
        m_pressedButtons[index] = pressed;
        if (pressed) {
            SetCapture(m_hwnd);
        } else {
            ReleaseCapture();
        }
        if (m_events) {
            m_events->onMouseButton(button, pressed ? KeyAction::Press : KeyAction::Release,
                                    CurrentModifiers());
        }
        if (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP) {
            return TRUE;
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
        if (m_events) {
            if (message == WM_MOUSEWHEEL) {
                m_events->onScroll(vec2(0.0f, static_cast<f32>(delta) / WHEEL_DELTA));
            } else {
                m_events->onScroll(vec2(static_cast<f32>(delta) / WHEEL_DELTA, 0.0f));
            }
        }
        return 0;
    }

    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT) {
            if (m_cursorMode == CursorMode::Normal) {
                SetCursor(m_cursor ? m_cursor->getCursor() : LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            }
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        const UINT dpi = HIWORD(wparam);
        const f32 scale = static_cast<f32>(dpi) / 96.0f;
        if (m_events) m_events->onContentScale(vec2(scale, scale));
        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        if (m_sizeLimitsMin.x > 0) info->ptMinTrackSize.x = m_sizeLimitsMin.x;
        if (m_sizeLimitsMin.y > 0) info->ptMinTrackSize.y = m_sizeLimitsMin.y;
        if (m_sizeLimitsMax.x > 0) info->ptMaxTrackSize.x = m_sizeLimitsMax.x;
        if (m_sizeLimitsMax.y > 0) info->ptMaxTrackSize.y = m_sizeLimitsMax.y;
        if (m_aspectNumerator > 0 && m_aspectDenominator > 0) {
            const f32 aspect = static_cast<f32>(m_aspectNumerator) /
                               static_cast<f32>(m_aspectDenominator);
            info->ptMaxTrackSize.x = static_cast<LONG>(info->ptMaxTrackSize.y * aspect);
            info->ptMinTrackSize.x = static_cast<LONG>(info->ptMinTrackSize.y * aspect);
        }
        return 0;
    }

    case WM_DROPFILES: {
        auto drop = reinterpret_cast<HDROP>(wparam);
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::string> paths;
        for (UINT i = 0; i < count; ++i) {
            const UINT length = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring path(length, L'\0');
            DragQueryFileW(drop, i, path.data(), length + 1);
            paths.push_back(Utf8FromWide(path));
        }
        DragFinish(drop);
        if (m_events && !paths.empty()) {
            m_events->onDrop(std::move(paths));
        }
        return 0;
    }

    case WM_INPUT: {
        if (m_cursorMode == CursorMode::Disabled || m_cursorMode == CursorMode::Captured) {
            UINT size = 0;
            GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &size,
                            sizeof(RAWINPUTHEADER));
            std::vector<u8> buffer(size);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, buffer.data(),
                                &size, sizeof(RAWINPUTHEADER)) == size) {
                const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
                if (raw->header.dwType == RIM_TYPEMOUSE &&
                    (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
                    m_cursorPos += vec2(static_cast<f32>(raw->data.mouse.lLastX),
                                        static_cast<f32>(raw->data.mouse.lLastY));
                    if (m_events) m_events->onCursorPos(m_cursorPos);
                }
            }
        }
        return DefWindowProcW(m_hwnd, message, wparam, lparam);
    }

    case WM_NCHITTEST:
        if (m_mousePassthrough) {
            return HTTRANSPARENT;
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            m_focused = false;
        } else {
            m_focused = true;
        }
        break;

    default:
        break;
    }
    return DefWindowProcW(m_hwnd, message, wparam, lparam);
}

} // namespace sgl::detail::win32

namespace sgl::detail {

std::unique_ptr<Backend> CreateWin32Backend(Error& error) {
    auto backend = std::make_unique<win32::Win32BackendImpl>();
    (void)error;
    return backend;
}

} // namespace sgl::detail

#endif // _WIN32
