#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/OpenGL.h>

#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <dlfcn.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "backend/Backend.hpp"
#include "backend/BackendFactory.hpp"
#include "common/GamepadMapping.hpp"
#include "common/MonitorAccess.hpp"

namespace sgl::detail::cocoa {
class CocoaWindowBackend;
struct State;
State& state();
} // namespace sgl::detail::cocoa

@interface SglView : NSOpenGLView <NSDraggingDestination>
@property(nonatomic, assign) sgl::detail::cocoa::CocoaWindowBackend* backend;
@end

@interface SglWindow : NSWindow <NSWindowDelegate>
@property(nonatomic, assign) sgl::detail::cocoa::CocoaWindowBackend* backend;
@end

namespace sgl::detail::cocoa {

std::string utf8FromNSString(NSString* string) {
    if (!string) {
        return {};
    }
    const char* text = [string UTF8String];
    return text ? std::string(text) : std::string{};
}

NSString* nsStringFromUtf8(std::string_view text) {
    return [[NSString alloc] initWithBytes:text.data() length:text.size()
                                   encoding:NSUTF8StringEncoding];
}

CGFloat flipY(CGFloat y, CGFloat height) {
    return height - y;
}

struct JoystickAxisElement {
    IOHIDElementRef element = nullptr;
    u32 usage = 0;
    i64 minimum = 0;
    i64 maximum = 0;
    f32 value = 0.0f;
};

struct JoystickButtonElement {
    IOHIDElementRef element = nullptr;
    u32 usage = 0;
    u8 value = 0;
};

struct JoystickHatElement {
    IOHIDElementRef element = nullptr;
    u32 usage = 0;
    JoystickHat value = JoystickHat::Centered;
};

struct JoystickDevice {
    IOHIDDeviceRef device = nullptr;
    std::string name;
    std::string guid;
    u64 vendor = 0;
    u64 product = 0;
    u64 version = 0;

    std::vector<JoystickAxisElement> axisElements;
    std::vector<JoystickButtonElement> buttonElements;
    std::vector<JoystickHatElement> hatElements;

    std::vector<f32> axes;
    std::vector<u8> buttons;
    std::vector<JoystickHat> hats;

    std::optional<GamepadMapping> mapping;
    bool gamepad = false;
};

struct State {
    bool initialized = false;
    bool applicationConfigured = false;
    std::unordered_map<NSWindow*, CocoaWindowBackend*> windows;
    MonitorCallback monitorCallback;
    std::vector<Monitor> monitors;
    std::vector<void*> monitorHandles;

    std::function<void(i32, bool)> joystickCallback;
    IOHIDManagerRef hidManager = nullptr;
    std::vector<JoystickDevice> joysticks;

    id screenObserver = nil;
    u64 clipboardChangeCount = 0;
};

State g_state;

void refreshJoysticks();

std::string cfStringToUtf8(CFStringRef string) {
    if (!string) {
        return {};
    }
    const CFIndex length = CFStringGetLength(string);
    const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::vector<char> buffer(static_cast<std::size_t>(maxSize));
    if (CFStringGetCString(string, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
        return std::string(buffer.data());
    }
    return {};
}

template <typename T>
T cfProperty(IOHIDDeviceRef device, CFStringRef key) {
    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (!value) {
        return T{};
    }
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        if constexpr (std::is_arithmetic_v<T>) {
            SInt64 number = 0;
            CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberSInt64Type, &number);
            return static_cast<T>(number);
        }
        return T{};
    }
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        if constexpr (std::is_same_v<T, std::string>) {
            return cfStringToUtf8(static_cast<CFStringRef>(value));
        }
        return T{};
    }
    return T{};
}

Key translateKeyCode(unsigned short keyCode) {
    switch (keyCode) {
    case 0: return Key::A;
    case 1: return Key::S;
    case 2: return Key::D;
    case 3: return Key::F;
    case 4: return Key::H;
    case 5: return Key::G;
    case 6: return Key::Z;
    case 7: return Key::X;
    case 8: return Key::C;
    case 9: return Key::V;
    case 11: return Key::B;
    case 12: return Key::Q;
    case 13: return Key::W;
    case 14: return Key::E;
    case 15: return Key::R;
    case 16: return Key::Y;
    case 17: return Key::T;
    case 18: return Key::Num1;
    case 19: return Key::Num2;
    case 20: return Key::Num3;
    case 21: return Key::Num4;
    case 22: return Key::Num6;
    case 23: return Key::Num5;
    case 24: return Key::Equal;
    case 25: return Key::Num9;
    case 26: return Key::Num7;
    case 27: return Key::Minus;
    case 28: return Key::Num8;
    case 29: return Key::Num0;
    case 30: return Key::RightBracket;
    case 31: return Key::O;
    case 32: return Key::U;
    case 33: return Key::LeftBracket;
    case 34: return Key::I;
    case 35: return Key::P;
    case 36: return Key::Enter;
    case 37: return Key::L;
    case 38: return Key::J;
    case 39: return Key::Apostrophe;
    case 40: return Key::K;
    case 41: return Key::Semicolon;
    case 42: return Key::Backslash;
    case 43: return Key::Comma;
    case 44: return Key::Slash;
    case 45: return Key::N;
    case 46: return Key::M;
    case 47: return Key::Period;
    case 48: return Key::Tab;
    case 49: return Key::Space;
    case 50: return Key::GraveAccent;
    case 51: return Key::Backspace;
    case 53: return Key::Escape;
    case 54: return Key::RightSuper;
    case 55: return Key::LeftSuper;
    case 56: return Key::LeftShift;
    case 57: return Key::CapsLock;
    case 58: return Key::LeftAlt;
    case 59: return Key::LeftControl;
    case 60: return Key::RightShift;
    case 61: return Key::RightAlt;
    case 62: return Key::RightControl;
    case 64: return Key::F17;
    case 65: return Key::KPDecimal;
    case 67: return Key::KPMultiply;
    case 69: return Key::KPAdd;
    case 71: return Key::NumLock;
    case 75: return Key::KPDivide;
    case 76: return Key::KPEnter;
    case 78: return Key::KPSubtract;
    case 79: return Key::F18;
    case 80: return Key::F19;
    case 81: return Key::KPEqual;
    case 82: return Key::KP0;
    case 83: return Key::KP1;
    case 84: return Key::KP2;
    case 85: return Key::KP3;
    case 86: return Key::KP4;
    case 87: return Key::KP5;
    case 88: return Key::KP6;
    case 89: return Key::KP7;
    case 91: return Key::KP8;
    case 92: return Key::KP9;
    case 96: return Key::F5;
    case 97: return Key::F6;
    case 98: return Key::F7;
    case 99: return Key::F3;
    case 100: return Key::F8;
    case 101: return Key::F9;
    case 103: return Key::F11;
    case 105: return Key::F13;
    case 106: return Key::F16;
    case 107: return Key::F14;
    case 109: return Key::F10;
    case 111: return Key::F12;
    case 114: return Key::Insert;
    case 115: return Key::Home;
    case 116: return Key::PageUp;
    case 117: return Key::Delete;
    case 118: return Key::F4;
    case 119: return Key::End;
    case 120: return Key::F2;
    case 121: return Key::PageDown;
    case 122: return Key::F1;
    case 123: return Key::Left;
    case 124: return Key::Right;
    case 125: return Key::Down;
    case 126: return Key::Up;
    default: return Key::Unknown;
    }
}

unsigned short keyCodeForKey(Key key) {
    if (key >= Key::A && key <= Key::Z) {
        static const unsigned short codes[] = {0, 11, 8, 2, 14, 3, 5, 4, 34, 38, 40, 37, 46,
                                               45, 31, 35, 12, 15, 1, 17, 32, 9, 13, 7, 16, 6};
        return codes[static_cast<int>(key) - static_cast<int>(Key::A)];
    }
    static const std::unordered_map<int, unsigned short> map = {
        {static_cast<int>(Key::Space), 49},   {static_cast<int>(Key::Escape), 53},
        {static_cast<int>(Key::Enter), 36},   {static_cast<int>(Key::Tab), 48},
        {static_cast<int>(Key::Backspace), 51}, {static_cast<int>(Key::Delete), 117},
        {static_cast<int>(Key::Insert), 114}, {static_cast<int>(Key::Left), 123},
        {static_cast<int>(Key::Right), 124},  {static_cast<int>(Key::Down), 125},
        {static_cast<int>(Key::Up), 126},     {static_cast<int>(Key::Home), 115},
        {static_cast<int>(Key::End), 119},    {static_cast<int>(Key::PageUp), 116},
        {static_cast<int>(Key::PageDown), 121}, {static_cast<int>(Key::LeftShift), 56},
        {static_cast<int>(Key::RightShift), 60}, {static_cast<int>(Key::LeftControl), 59},
        {static_cast<int>(Key::RightControl), 62}, {static_cast<int>(Key::LeftAlt), 58},
        {static_cast<int>(Key::RightAlt), 61}, {static_cast<int>(Key::LeftSuper), 55},
        {static_cast<int>(Key::RightSuper), 54}, {static_cast<int>(Key::CapsLock), 57},
        {static_cast<int>(Key::NumLock), 71}, {static_cast<int>(Key::KP0), 82},
        {static_cast<int>(Key::KP1), 83},     {static_cast<int>(Key::KP2), 84},
        {static_cast<int>(Key::KP3), 85},     {static_cast<int>(Key::KP4), 86},
        {static_cast<int>(Key::KP5), 87},     {static_cast<int>(Key::KP6), 88},
        {static_cast<int>(Key::KP7), 89},     {static_cast<int>(Key::KP8), 91},
        {static_cast<int>(Key::KP9), 92},     {static_cast<int>(Key::KPDecimal), 65},
        {static_cast<int>(Key::KPDivide), 75}, {static_cast<int>(Key::KPMultiply), 67},
        {static_cast<int>(Key::KPSubtract), 78}, {static_cast<int>(Key::KPAdd), 69},
        {static_cast<int>(Key::KPEnter), 76}, {static_cast<int>(Key::KPEqual), 81},
        {static_cast<int>(Key::Minus), 27},   {static_cast<int>(Key::Equal), 24},
        {static_cast<int>(Key::LeftBracket), 33}, {static_cast<int>(Key::RightBracket), 30},
        {static_cast<int>(Key::Backslash), 42}, {static_cast<int>(Key::Semicolon), 41},
        {static_cast<int>(Key::Apostrophe), 39}, {static_cast<int>(Key::Comma), 43},
        {static_cast<int>(Key::Period), 47},  {static_cast<int>(Key::Slash), 44},
        {static_cast<int>(Key::GraveAccent), 50},
    };
    const auto it = map.find(static_cast<int>(key));
    if (it != map.end()) {
        return it->second;
    }
    if (key >= Key::F1 && key <= Key::F12) {
        static const unsigned short codes[] = {122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 103, 111};
        return codes[static_cast<int>(key) - static_cast<int>(Key::F1)];
    }
    return 0xFFFF;
}

Modifiers modifiersFromEvent(NSEvent* event) {
    const NSEventModifierFlags flags = [event modifierFlags];
    Modifiers mods = Modifiers::None;
    if ((flags & NSEventModifierFlagShift) != 0) mods |= Modifiers::Shift;
    if ((flags & NSEventModifierFlagControl) != 0) mods |= Modifiers::Control;
    if ((flags & NSEventModifierFlagOption) != 0) mods |= Modifiers::Alt;
    if ((flags & NSEventModifierFlagCommand) != 0) mods |= Modifiers::Super;
    if ((flags & NSEventModifierFlagCapsLock) != 0) mods |= Modifiers::CapsLock;
    return mods;
}

class CocoaCursorBackend final : public CursorBackend {
public:
    explicit CocoaCursorBackend(NSCursor* cursor, bool owned) : m_cursor(cursor), m_owned(owned) {}
    ~CocoaCursorBackend() override {
        if (m_owned && m_cursor) {
            [m_cursor release];
        }
    }
    [[nodiscard]] void* getNativeHandle() const override { return m_cursor; }
    [[nodiscard]] NSCursor* getCursor() const { return m_cursor; }

private:
    NSCursor* m_cursor = nil;
    bool m_owned = false;
};

class CocoaWindowBackend final : public WindowBackend {
public:
    CocoaWindowBackend(const WindowHints& hints, Error& error);
    ~CocoaWindowBackend() override;

    void setEvents(WindowEvents* events) override { m_events = events; }
    void makeContextCurrent() override;
    void swapBuffers() override;
    void* getProcAddress(const char* name) override;
    [[nodiscard]] NativeHandle getNativeHandle() const override;
    [[nodiscard]] bool isCloseRequested() const override { return m_closeRequested; }
    void setCloseRequested(bool requested) override { m_closeRequested = requested; }
    [[nodiscard]] ivec2 getSize() const override;
    [[nodiscard]] ivec2 getFramebufferSize() const override;
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
    void setIcon(std::span<const Image> images) override;
    [[nodiscard]] std::vector<Image> getIcon() const override { return m_icon; }
    void setMonitor(const Monitor& monitor, ivec2 position, ivec2 size, i32 refreshRate) override;
    [[nodiscard]] Monitor getMonitor() const override;
    void setInputMode(InputMode mode, i32 value) override;
    [[nodiscard]] i32 getInputMode(InputMode mode) const override;
    [[nodiscard]] bool isRawMouseMotionSupported() const override { return false; }
    [[nodiscard]] std::string getKeyName(Key key, i32 scancode) const override;
    [[nodiscard]] i32 getKeyScancode(Key key) const override;
    [[nodiscard]] bool isKeyPressed(Key key) const override;
    [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const override;
    [[nodiscard]] vec2 getCursorPos() const override { return m_cursorPos; }
    void setCursorPos(vec2 position) override;
    void setCursor(CursorBackend* cursor) override;
    [[nodiscard]] std::string getClipboardString() const override;
    void setClipboardString(std::string_view text) override;

    void handleKeyDown(NSEvent* event, bool down);
    void handleFlagsChanged(NSEvent* event);
    void handleMouse(NSEvent* event, MouseButton button, bool down);
    void handleCursorMove(NSEvent* event);
    void handleScroll(NSEvent* event);
    void handleResize();
    void handleMove();
    void handleFocus(bool focused);
    void handleClose();
    void handleMiniaturize(bool minimized);
    void handleZoom(bool zoomed);
    void handleDrop(NSArray<NSURL*>* urls);
    void handleScreenChange();
    [[nodiscard]] NSWindow* getNSWindow() const { return m_window; }
    [[nodiscard]] NSOpenGLContext* getGLContext() const { return m_context; }
    void updateCursor();

    void destroy();

private:
    NSWindow* m_window = nil;
    SglView* m_view = nil;
    NSOpenGLContext* m_context = nil;
    NSOpenGLPixelFormat* m_pixelFormat = nil;

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
    bool m_mousePassthrough = false;
    bool m_fullscreen = false;

    i32 m_swapInterval = 0;
    f32 m_opacity = 1.0f;
    CursorMode m_cursorMode = CursorMode::Normal;
    bool m_lockKeyMods = true;
    bool m_rawMouseMotion = false;
    bool m_stickyKeys = false;
    bool m_stickyMouseButtons = false;

    std::unordered_set<unsigned short> m_pressedKeys;
    std::array<bool, 8> m_pressedButtons{};
    vec2 m_cursorPos{0.0f, 0.0f};
    CocoaCursorBackend* m_cursor = nullptr;
    std::vector<Image> m_icon;
    NSScreen* m_fullscreenScreen = nil;
    NSRect m_windowedFrame{};
    bool m_cursorHidden = false;
    bool m_cursorAssociated = false;
};

// ---------------------------------------------------------------------------
// Monitor helpers
// ---------------------------------------------------------------------------

Monitor makeMonitor(NSScreen* screen) {
    return MonitorAccess::make(screen, Platform::Cocoa);
}

NSScreen* screenFromMonitor(const Monitor& monitor) {
    return static_cast<NSScreen*>(MonitorAccess::native(monitor));
}

void rebuildMonitors(bool notify) {
    State& s = g_state;
    std::vector<Monitor> monitors;
    std::vector<void*> handles;
    for (NSScreen* screen in [NSScreen screens]) {
        monitors.push_back(makeMonitor(screen));
        handles.push_back(screen);
    }
    if (notify && s.monitorCallback) {
        for (void* handle : s.monitorHandles) {
            if (std::find(handles.begin(), handles.end(), handle) == handles.end()) {
                s.monitorCallback(makeMonitor(static_cast<NSScreen*>(handle)), false);
            }
        }
        for (void* handle : handles) {
            if (std::find(s.monitorHandles.begin(), s.monitorHandles.end(), handle) ==
                s.monitorHandles.end()) {
                s.monitorCallback(makeMonitor(static_cast<NSScreen*>(handle)), true);
            }
        }
    }
    s.monitors = std::move(monitors);
    s.monitorHandles = std::move(handles);
}

double screenTopLeftY(NSScreen* screen) {
    NSScreen* primary = [NSScreen screens].firstObject;
    const CGFloat primaryHeight = primary ? primary.frame.size.height : 0.0;
    const NSRect frame = screen.frame;
    return primaryHeight - (frame.origin.y + frame.size.height);
}

CGDirectDisplayID displayIdFromScreen(NSScreen* screen) {
    NSDictionary* description = screen.deviceDescription;
    NSNumber* number = description[@"NSScreenNumber"];
    return number ? static_cast<CGDirectDisplayID>([number unsignedIntValue]) : 0;
}

class CocoaBackendImpl final : public Backend {
public:
    bool init(const InitOptions&, Error& error) override;
    void terminate() override;
    [[nodiscard]] Platform getPlatform() const override { return Platform::Cocoa; }

    std::unique_ptr<WindowBackend> createWindow(const WindowHints& hints, Error& error) override {
        auto window = std::make_unique<CocoaWindowBackend>(hints, error);
        if (error.isError()) {
            return nullptr;
        }
        return window;
    }

    std::unique_ptr<CursorBackend> createCursor(const Image& image, ivec2 hotspot,
                                                Error& error) override;
    std::unique_ptr<CursorBackend> createStandardCursor(StandardCursor shape, Error& error) override;

    [[nodiscard]] std::vector<Monitor> getMonitors() override { return g_state.monitors; }
    [[nodiscard]] Monitor getPrimaryMonitor() override {
        return g_state.monitors.empty() ? Monitor{} : g_state.monitors.front();
    }

    void pollEvents() override;
    void waitEvents(f64 timeout) override;
    void postEmptyEvent() override;

    void setMonitorCallback(MonitorCallback callback) override {
        g_state.monitorCallback = std::move(callback);
    }

    [[nodiscard]] bool isMonitorConnected(const Monitor& monitor) override {
        void* handle = MonitorAccess::native(monitor);
        return std::find(g_state.monitorHandles.begin(), g_state.monitorHandles.end(), handle) !=
               g_state.monitorHandles.end();
    }

    [[nodiscard]] ivec2 getMonitorPosition(const Monitor& monitor) override {
        NSScreen* screen = screenFromMonitor(monitor);
        if (!screen) return {0, 0};
        return ivec2(static_cast<i32>(screen.frame.origin.x),
                     static_cast<i32>(screenTopLeftY(screen)));
    }

    [[nodiscard]] Monitor::Workarea getMonitorWorkarea(const Monitor& monitor) override {
        Monitor::Workarea area;
        NSScreen* screen = screenFromMonitor(monitor);
        if (screen) {
            const NSRect frame = screen.visibleFrame;
            area.position = ivec2(static_cast<i32>(frame.origin.x),
                                  static_cast<i32>(screenTopLeftY(screen) + (screen.frame.size.height -
                                                                            frame.origin.y -
                                                                            frame.size.height)));
            area.size = ivec2(static_cast<i32>(frame.size.width),
                              static_cast<i32>(frame.size.height));
        }
        return area;
    }

    [[nodiscard]] ivec2 getMonitorPhysicalSize(const Monitor& monitor) override {
        NSScreen* screen = screenFromMonitor(monitor);
        if (!screen) return {0, 0};
        NSDictionary* description = screen.deviceDescription;
        NSSize size = [description[@"NSDeviceSize"] sizeValue];
        return ivec2(static_cast<i32>(size.width), static_cast<i32>(size.height));
    }

    [[nodiscard]] vec2 getMonitorContentScale(const Monitor& monitor) override {
        NSScreen* screen = screenFromMonitor(monitor);
        const CGFloat scale = screen ? screen.backingScaleFactor : 1.0;
        return {static_cast<f32>(scale), static_cast<f32>(scale)};
    }

    [[nodiscard]] std::string getMonitorName(const Monitor& monitor) override {
        NSScreen* screen = screenFromMonitor(monitor);
        if (!screen) return {};
        if ([screen respondsToSelector:@selector(localizedName)]) {
            return utf8FromNSString([screen localizedName]);
        }
        return {};
    }

    [[nodiscard]] std::vector<VideoMode> getVideoModes(const Monitor& monitor) override {
        std::vector<VideoMode> modes;
        NSScreen* screen = screenFromMonitor(monitor);
        const CGDirectDisplayID display = screen ? displayIdFromScreen(screen) : 0;
        if (display == 0) {
            return modes;
        }
        CFArrayRef allModes = CGDisplayCopyAllDisplayModes(display, nullptr);
        if (!allModes) {
            return modes;
        }
        const CFIndex count = CFArrayGetCount(allModes);
        for (CFIndex i = 0; i < count; ++i) {
            const auto* mode = static_cast<const CGDisplayMode*>(
                CFArrayGetValueAtIndex(allModes, i));
            VideoMode videoMode;
            videoMode.size = ivec2(static_cast<i32>(CGDisplayModeGetWidth(mode)),
                                   static_cast<i32>(CGDisplayModeGetHeight(mode)));
            videoMode.refreshRate = static_cast<i32>(std::lround(CGDisplayModeGetRefreshRate(mode)));
            modes.push_back(videoMode);
        }
        CFRelease(allModes);
        return modes;
    }

    [[nodiscard]] VideoMode getVideoMode(const Monitor& monitor) override {
        VideoMode result;
        NSScreen* screen = screenFromMonitor(monitor);
        if (!screen) return result;
        const CGDirectDisplayID display = displayIdFromScreen(screen);
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display);
        if (mode) {
            result.size = ivec2(static_cast<i32>(CGDisplayModeGetWidth(mode)),
                                static_cast<i32>(CGDisplayModeGetHeight(mode)));
            result.refreshRate = static_cast<i32>(std::lround(CGDisplayModeGetRefreshRate(mode)));
            CGDisplayModeRelease(mode);
        }
        return result;
    }

    void setGamma(const Monitor& monitor, f32 gamma) override {
        if (gamma <= 0.0f) {
            reportError(ErrorCode::InvalidValue, "Gamma muss groesser als 0 sein.");
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
        NSScreen* screen = screenFromMonitor(monitor);
        const CGDirectDisplayID display = screen ? displayIdFromScreen(screen) : 0;
        if (display == 0) return result;
        std::array<CGGammaValue, 256> red{};
        std::array<CGGammaValue, 256> green{};
        std::array<CGGammaValue, 256> blue{};
        if (CGGetDisplayTransferByTable(display, 256, red.data(), green.data(), blue.data()) ==
            kCGErrorSuccess) {
            for (std::size_t i = 0; i < 256; ++i) {
                result.red[i] = static_cast<u16>(std::lround(red[i] * 65535.0));
                result.green[i] = static_cast<u16>(std::lround(green[i] * 65535.0));
                result.blue[i] = static_cast<u16>(std::lround(blue[i] * 65535.0));
            }
        }
        return result;
    }

    void setGammaRamp(const Monitor& monitor, const GammaRamp& ramp) override {
        NSScreen* screen = screenFromMonitor(monitor);
        const CGDirectDisplayID display = screen ? displayIdFromScreen(screen) : 0;
        if (display == 0) return;
        std::array<CGGammaValue, 256> red{};
        std::array<CGGammaValue, 256> green{};
        std::array<CGGammaValue, 256> blue{};
        for (std::size_t i = 0; i < 256; ++i) {
            red[i] = static_cast<CGGammaValue>(ramp.red[i]) / 65535.0;
            green[i] = static_cast<CGGammaValue>(ramp.green[i]) / 65535.0;
            blue[i] = static_cast<CGGammaValue>(ramp.blue[i]) / 65535.0;
        }
        CGSetDisplayTransferByTable(display, 256, red.data(), green.data(), blue.data());
    }

    [[nodiscard]] NativeHandle getMonitorNativeHandle(const Monitor& monitor) override {
        NativeHandle handle;
        handle.platform = NativePlatform::Cocoa;
        handle.window = MonitorAccess::native(monitor);
        return handle;
    }

    [[nodiscard]] bool isJoystickPresent(i32 jid) override {
        return jid >= 0 && static_cast<std::size_t>(jid) < g_state.joysticks.size();
    }

    [[nodiscard]] std::string getJoystickName(i32 jid) override {
        if (!isJoystickPresent(jid)) return {};
        return g_state.joysticks[static_cast<std::size_t>(jid)].name;
    }

    [[nodiscard]] std::string getJoystickGUID(i32 jid) override {
        if (!isJoystickPresent(jid)) return {};
        return g_state.joysticks[static_cast<std::size_t>(jid)].guid;
    }

    [[nodiscard]] std::span<const f32> getJoystickAxes(i32 jid) override {
        if (!isJoystickPresent(jid)) return {};
        return g_state.joysticks[static_cast<std::size_t>(jid)].axes;
    }

    [[nodiscard]] std::span<const u8> getJoystickButtons(i32 jid) override {
        if (!isJoystickPresent(jid)) return {};
        return g_state.joysticks[static_cast<std::size_t>(jid)].buttons;
    }

    [[nodiscard]] std::span<const JoystickHat> getJoystickHats(i32 jid) override {
        if (!isJoystickPresent(jid)) return {};
        return g_state.joysticks[static_cast<std::size_t>(jid)].hats;
    }

    [[nodiscard]] bool isGamepad(i32 jid) override {
        if (!isJoystickPresent(jid)) return false;
        return g_state.joysticks[static_cast<std::size_t>(jid)].gamepad;
    }

    [[nodiscard]] std::string getGamepadName(i32 jid) override {
        if (!isJoystickPresent(jid)) return {};
        const auto& device = g_state.joysticks[static_cast<std::size_t>(jid)];
        return device.mapping ? device.mapping->name : std::string{};
    }

    [[nodiscard]] std::optional<GamepadState> getGamepadState(i32 jid) override {
        if (!isJoystickPresent(jid)) return std::nullopt;
        const auto& device = g_state.joysticks[static_cast<std::size_t>(jid)];
        if (!device.mapping) return std::nullopt;
        return applyGamepadMapping(*device.mapping, device.axes, device.buttons, device.hats);
    }

    bool updateGamepadMappings(std::string_view mappings) override {
        const bool added = GamepadMappingDatabase::instance().addMappings(mappings);
        for (auto& device : g_state.joysticks) {
            device.mapping =
                GamepadMappingDatabase::instance().find(device.guid, "Mac OS X");
            device.gamepad = device.mapping.has_value();
        }
        return added;
    }

    void setJoystickCallback(std::function<void(i32, bool)> callback) override {
        g_state.joystickCallback = std::move(callback);
    }
};


// ---------------------------------------------------------------------------
// HID joystick enumeration
// ---------------------------------------------------------------------------

i64 elementValue(IOHIDDeviceRef device, IOHIDElementRef element) {
    IOHIDValueRef value = nullptr;
    if (IOHIDDeviceGetValue(device, element, &value) == kIOReturnSuccess && value) {
        return IOHIDValueGetIntegerValue(value);
    }
    return 0;
}

JoystickHat hatFromValue(i64 value) {
    switch (value) {
    case 0: return JoystickHat::Up;
    case 1: return JoystickHat::RightUp;
    case 2: return JoystickHat::Right;
    case 3: return JoystickHat::RightDown;
    case 4: return JoystickHat::Down;
    case 5: return JoystickHat::LeftDown;
    case 6: return JoystickHat::Left;
    case 7: return JoystickHat::LeftUp;
    default: return JoystickHat::Centered;
    }
}


struct RawHidElement {
    IOHIDElementRef element = nullptr;
    u32 usage = 0;
};

void readJoystick(JoystickDevice& joystick) {
    joystick.axes.resize(joystick.axisElements.size());
    for (std::size_t i = 0; i < joystick.axisElements.size(); ++i) {
        JoystickAxisElement& axis = joystick.axisElements[i];
        const i64 raw = elementValue(joystick.device, axis.element);
        // Auto-Kalibrierung wie bei GLFW: beobachtete Min/Max erweitern.
        axis.minimum = std::min(axis.minimum, raw);
        axis.maximum = std::max(axis.maximum, raw);
        axis.value = axis.maximum > axis.minimum
                         ? 2.0f * static_cast<f32>(raw - axis.minimum) /
                                   static_cast<f32>(axis.maximum - axis.minimum) -
                               1.0f
                         : 0.0f;
        joystick.axes[i] = axis.value;
    }

    joystick.buttons.resize(joystick.buttonElements.size());
    for (std::size_t i = 0; i < joystick.buttonElements.size(); ++i) {
        JoystickButtonElement& button = joystick.buttonElements[i];
        button.value = elementValue(joystick.device, button.element) != 0 ? 1 : 0;
        joystick.buttons[i] = button.value;
    }

    joystick.hats.resize(joystick.hatElements.size());
    for (std::size_t i = 0; i < joystick.hatElements.size(); ++i) {
        JoystickHatElement& hat = joystick.hatElements[i];
        hat.value = hatFromValue(elementValue(joystick.device, hat.element));
        joystick.hats[i] = hat.value;
    }
}

bool openJoystick(IOHIDDeviceRef device, JoystickDevice& joystick) {
    joystick.device = device;
    joystick.name = cfProperty<std::string>(device, CFSTR(kIOHIDProductKey));
    if (joystick.name.empty()) {
        joystick.name = "Unknown Joystick";
    }
    joystick.vendor = cfProperty<u64>(device, CFSTR(kIOHIDVendorIDKey));
    joystick.product = cfProperty<u64>(device, CFSTR(kIOHIDProductIDKey));
    joystick.version = cfProperty<u64>(device, CFSTR(kIOHIDVersionNumberKey));
    joystick.guid = makeCocoaGamepadGuid(joystick.vendor, joystick.product,
                                         joystick.version, joystick.name);

    if (IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        return false;
    }

    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, nullptr,
                                                          kIOHIDOptionsTypeNone);
    if (!elements) {
        IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
        return false;
    }

    std::vector<RawHidElement> rawAxes;
    std::vector<RawHidElement> rawButtons;
    std::vector<RawHidElement> rawHats;

    const CFIndex count = CFArrayGetCount(elements);
    for (CFIndex i = 0; i < count; ++i) {
        auto* element = static_cast<IOHIDElementRef>(
            const_cast<void*>(CFArrayGetValueAtIndex(elements, i)));
        if (!element || CFGetTypeID(element) != IOHIDElementGetTypeID()) {
            continue;
        }
        const IOHIDElementType type = IOHIDElementGetType(element);
        if (type != kIOHIDElementTypeInput_Axis && type != kIOHIDElementTypeInput_Button &&
            type != kIOHIDElementTypeInput_Misc) {
            continue;
        }

        const u32 page = IOHIDElementGetUsagePage(element);
        const u32 usage = IOHIDElementGetUsage(element);
        std::vector<RawHidElement>* target = nullptr;

        if (page == kHIDPage_GenericDesktop) {
            switch (usage) {
            case kHIDUsage_GD_X:
            case kHIDUsage_GD_Y:
            case kHIDUsage_GD_Z:
            case kHIDUsage_GD_Rx:
            case kHIDUsage_GD_Ry:
            case kHIDUsage_GD_Rz:
            case kHIDUsage_GD_Slider:
            case kHIDUsage_GD_Dial:
            case kHIDUsage_GD_Wheel:
                target = &rawAxes;
                break;
            case kHIDUsage_GD_Hatswitch:
                target = &rawHats;
                break;
            case kHIDUsage_GD_DPadUp:
            case kHIDUsage_GD_DPadRight:
            case kHIDUsage_GD_DPadDown:
            case kHIDUsage_GD_DPadLeft:
            case kHIDUsage_GD_SystemMainMenu:
            case kHIDUsage_GD_Select:
            case kHIDUsage_GD_Start:
                target = &rawButtons;
                break;
            default:
                break;
            }
        } else if (page == kHIDPage_Simulation) {
            switch (usage) {
            case kHIDUsage_Sim_Accelerator:
            case kHIDUsage_Sim_Brake:
            case kHIDUsage_Sim_Throttle:
            case kHIDUsage_Sim_Rudder:
            case kHIDUsage_Sim_Steering:
                target = &rawAxes;
                break;
            default:
                break;
            }
        } else if (page == kHIDPage_Button || page == kHIDPage_Consumer) {
            target = &rawButtons;
        }

        if (target) {
            target->push_back({element, usage});
        }
    }
    CFRelease(elements);

    auto byUsage = [](const RawHidElement& a, const RawHidElement& b) {
        return a.usage < b.usage;
    };
    std::stable_sort(rawAxes.begin(), rawAxes.end(), byUsage);
    std::stable_sort(rawButtons.begin(), rawButtons.end(), byUsage);
    std::stable_sort(rawHats.begin(), rawHats.end(), byUsage);

    joystick.axisElements.clear();
    for (const RawHidElement& element : rawAxes) {
        JoystickAxisElement axis;
        axis.element = element.element;
        axis.usage = element.usage;
        axis.minimum = IOHIDElementGetLogicalMin(element.element);
        axis.maximum = IOHIDElementGetLogicalMax(element.element);
        joystick.axisElements.push_back(axis);
    }

    joystick.buttonElements.clear();
    for (const RawHidElement& element : rawButtons) {
        JoystickButtonElement button;
        button.element = element.element;
        button.usage = element.usage;
        joystick.buttonElements.push_back(button);
    }

    joystick.hatElements.clear();
    for (const RawHidElement& element : rawHats) {
        JoystickHatElement hat;
        hat.element = element.element;
        hat.usage = element.usage;
        joystick.hatElements.push_back(hat);
    }

    joystick.mapping = GamepadMappingDatabase::instance().find(joystick.guid, "Mac OS X");
    joystick.gamepad = joystick.mapping.has_value();
    readJoystick(joystick);
    return true;
}

void refreshJoysticks() {
    State& s = g_state;
    if (!s.hidManager) {
        return;
    }

    CFSetRef set = IOHIDManagerCopyDevices(s.hidManager);
    if (!set) {
        return;
    }

    const CFIndex count = CFSetGetCount(set);
    std::vector<const void*> values(static_cast<std::size_t>(count));
    CFSetGetValues(set, values.data());

    auto contains = [&values](IOHIDDeviceRef device) {
        return std::find(values.begin(), values.end(), reinterpret_cast<const void*>(device)) !=
               values.end();
    };

    // Entfernte Geräte zuerst austragen.
    for (std::size_t i = s.joysticks.size(); i-- > 0;) {
        if (!contains(s.joysticks[i].device)) {
            if (s.joystickCallback) {
                s.joystickCallback(static_cast<i32>(i), false);
            }
            IOHIDDeviceClose(s.joysticks[i].device, kIOHIDOptionsTypeNone);
            s.joysticks.erase(s.joysticks.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }

    // Bestehende Geräte aktualisieren.
    for (auto& joystick : s.joysticks) {
        readJoystick(joystick);
    }

    // Neue Geräte anhängen.
    for (const void* value : values) {
        auto* device = static_cast<IOHIDDeviceRef>(const_cast<void*>(value));
        const bool known = std::any_of(s.joysticks.begin(), s.joysticks.end(),
                                       [device](const JoystickDevice& joystick) {
                                           return joystick.device == device;
                                       });
        if (known) {
            continue;
        }

        JoystickDevice joystick;
        if (!openJoystick(device, joystick)) {
            continue;
        }
        s.joysticks.push_back(std::move(joystick));
        if (s.joystickCallback) {
            s.joystickCallback(static_cast<i32>(s.joysticks.size() - 1), true);
        }
    }

    CFRelease(set);
}

} // namespace sgl::detail::cocoa

using namespace sgl::detail;
using namespace sgl::detail::cocoa;

@implementation SglView

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)isOpaque { return YES; }

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    NSArray* existing = [[self trackingAreas] copy];
    for (NSTrackingArea* area in existing) {
        [self removeTrackingArea:area];
    }
    [existing release];
    NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                                    NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect;
    NSTrackingArea* area = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                         options:options
                                                           owner:self
                                                        userInfo:nil];
    [self addTrackingArea:area];
    [area release];
}

- (void)keyDown:(NSEvent*)event {
    if (_backend) _backend->handleKeyDown(event, true);
}

- (void)keyUp:(NSEvent*)event {
    if (_backend) _backend->handleKeyDown(event, false);
}

- (void)flagsChanged:(NSEvent*)event {
    if (_backend) _backend->handleFlagsChanged(event);
}

- (void)mouseDown:(NSEvent*)event {
    if (_backend) _backend->handleMouse(event, MouseButton::Left, true);
}
- (void)mouseUp:(NSEvent*)event {
    if (_backend) _backend->handleMouse(event, MouseButton::Left, false);
}
- (void)rightMouseDown:(NSEvent*)event {
    if (_backend) _backend->handleMouse(event, MouseButton::Right, true);
}
- (void)rightMouseUp:(NSEvent*)event {
    if (_backend) _backend->handleMouse(event, MouseButton::Right, false);
}
- (void)otherMouseDown:(NSEvent*)event {
    if (_backend) _backend->handleMouse(event, MouseButton::Middle, true);
}
- (void)otherMouseUp:(NSEvent*)event {
    if (_backend) _backend->handleMouse(event, MouseButton::Middle, false);
}
- (void)mouseMoved:(NSEvent*)event {
    if (_backend) _backend->handleCursorMove(event);
}
- (void)mouseDragged:(NSEvent*)event {
    if (_backend) _backend->handleCursorMove(event);
}
- (void)rightMouseDragged:(NSEvent*)event {
    if (_backend) _backend->handleCursorMove(event);
}
- (void)otherMouseDragged:(NSEvent*)event {
    if (_backend) _backend->handleCursorMove(event);
}
- (void)scrollWheel:(NSEvent*)event {
    if (_backend) _backend->handleScroll(event);
}
- (void)mouseEntered:(NSEvent*)event {
    if (_backend) _backend->handleCursorMove(event);
}
- (void)mouseExited:(NSEvent*)event {
    (void)event;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    (void)sender;
    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard* pasteboard = [sender draggingPasteboard];
    NSArray<NSURL*>* urls =
        [pasteboard readObjectsForClasses:@[ [NSURL class] ]
                                  options:@{NSPasteboardURLReadingFileURLsOnlyKey : @YES}];
    if (_backend && urls) {
        _backend->handleDrop(urls);
    }
    return YES;
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    if (_backend) _backend->handleScreenChange();
}

@end

@implementation SglWindow

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if (_backend) _backend->handleClose();
    return NO;
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (_backend) _backend->handleResize();
}

- (void)windowDidMove:(NSNotification*)notification {
    (void)notification;
    if (_backend) _backend->handleMove();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    (void)notification;
    if (_backend) _backend->handleFocus(true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
    (void)notification;
    if (_backend) _backend->handleFocus(false);
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    (void)notification;
    if (_backend) _backend->handleMiniaturize(true);
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    (void)notification;
    if (_backend) _backend->handleMiniaturize(false);
}

- (void)windowDidZoom:(NSNotification*)notification {
    (void)notification;
    if (_backend) _backend->handleZoom([self isZoomed]);
}


@end

namespace sgl::detail::cocoa {

CocoaWindowBackend::CocoaWindowBackend(const WindowHints& hints, Error& error)
    : m_size{hints.width, hints.height}, m_title(hints.title), m_hints(hints) {
    if (!g_state.initialized) {
        error = makeError(ErrorCode::PlatformUnavailable, "Cocoa ist nicht initialisiert.");
        return;
    }
    if (hints.contextApi != ContextApi::Native || hints.clientApi != ClientApi::OpenGL) {
        error = makeError(ErrorCode::FeatureUnavailable,
                          "Das Cocoa-Backend unterstuetzt nur native OpenGL-Kontexte.");
        return;
    }
    if (hints.contextVersionMajor > 4 ||
        (hints.contextVersionMajor == 4 && hints.contextVersionMinor > 1)) {
        error = makeError(ErrorCode::VersionUnavailable,
                          "macOS unterstuetzt hoechstens OpenGL 4.1.");
        return;
    }

    m_resizable = hints.resizable;
    m_decorated = hints.decorated;

    NSOpenGLPixelFormatAttribute attributes[32] = {};
    int index = 0;
    attributes[index++] = NSOpenGLPFAOpenGLProfile;
    attributes[index++] = hints.contextProfile == ContextProfile::Core
                              ? NSOpenGLProfileVersion4_1Core
                              : NSOpenGLProfileVersionLegacy;
    attributes[index++] = NSOpenGLPFAColorSize;
    attributes[index++] = static_cast<NSOpenGLPixelFormatAttribute>(
        hints.redBits + hints.greenBits + hints.blueBits);
    attributes[index++] = NSOpenGLPFAAlphaSize;
    attributes[index++] = static_cast<NSOpenGLPixelFormatAttribute>(hints.alphaBits);
    attributes[index++] = NSOpenGLPFADepthSize;
    attributes[index++] = static_cast<NSOpenGLPixelFormatAttribute>(hints.depthBits);
    attributes[index++] = NSOpenGLPFAStencilSize;
    attributes[index++] = static_cast<NSOpenGLPixelFormatAttribute>(hints.stencilBits);
    if (hints.doublebuffer) {
        attributes[index++] = NSOpenGLPFADoubleBuffer;
    }
    if (hints.samples > 0) {
        attributes[index++] = NSOpenGLPFASampleBuffers;
        attributes[index++] = 1;
        attributes[index++] = NSOpenGLPFASamples;
        attributes[index++] = static_cast<NSOpenGLPixelFormatAttribute>(hints.samples);
    }
    attributes[index++] = 0;

    m_pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    if (!m_pixelFormat) {
        error = makeError(ErrorCode::FormatUnavailable,
                          "Kein passendes NSOpenGL-Pixelformat gefunden.");
        return;
    }

    NSRect frame = NSMakeRect(0, 0, hints.width, hints.height);
    m_view = [[SglView alloc] initWithFrame:frame pixelFormat:m_pixelFormat];
    m_view.backend = this;
    [m_view registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];

    NSUInteger style = NSWindowStyleMaskTexturedBackground | NSWindowStyleMaskClosable;
    if (m_resizable) style |= NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    if (m_decorated) style |= NSWindowStyleMaskTitled;
    if (!m_decorated) style = NSWindowStyleMaskBorderless;

    m_window = [[SglWindow alloc] initWithContentRect:frame
                                            styleMask:style
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
    m_window.backend = this;
    m_window.delegate = m_window;
    [m_window setContentView:m_view];
    [m_window setReleasedWhenClosed:NO];
    setTitle(m_title);
    if (hints.floating) {
        [m_window setLevel:NSFloatingWindowLevel];
    }
    if (hints.mousePassthrough) {
        [m_window setIgnoresMouseEvents:YES];
    }
    if (hints.transparentFramebuffer) {
        [m_window setOpaque:NO];
        [m_window setBackgroundColor:[NSColor clearColor]];
    }

    m_context = [[NSOpenGLContext alloc] initWithFormat:m_pixelFormat shareContext:nil];
    if (!m_context) {
        error = makeError(ErrorCode::PlatformError,
                          "NSOpenGL-Kontext konnte nicht erstellt werden.");
        return;
    }
    [m_context setView:m_view];
    [m_view setOpenGLContext:m_context];

    if (hints.resizable) {
        [m_window setContentMinSize:NSMakeSize(0, 0)];
        [m_window setContentMaxSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
    } else {
        [m_window setContentMinSize:NSMakeSize(hints.width, hints.height)];
        [m_window setContentMaxSize:NSMakeSize(hints.width, hints.height)];
    }

    g_state.windows[m_window] = this;
    m_visible = false;

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

CocoaWindowBackend::~CocoaWindowBackend() {
    destroy();
}

void CocoaWindowBackend::destroy() {
    if (!m_window) {
        return;
    }
    g_state.windows.erase(m_window);
    [m_window setDelegate:nil];
    m_window.backend = nullptr;
    [m_window orderOut:nil];
    if (m_view) {
        m_view.backend = nullptr;
        [m_view release];
        m_view = nil;
    }
    if (m_context) {
        [m_context clearDrawable];
        [m_context release];
        m_context = nil;
    }
    if (m_pixelFormat) {
        [m_pixelFormat release];
        m_pixelFormat = nil;
    }
    [m_window release];
    m_window = nil;
    m_visible = false;
}

void CocoaWindowBackend::makeContextCurrent() {
    [m_context makeCurrentContext];
}

void CocoaWindowBackend::swapBuffers() {
    [m_context flushBuffer];
}

void* CocoaWindowBackend::getProcAddress(const char* name) {
    void* address = dlsym(RTLD_DEFAULT, name);
    if (!address) {
        static void* opengl =
            dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
        if (opengl) {
            address = dlsym(opengl, name);
        }
    }
    return address;
}

NativeHandle CocoaWindowBackend::getNativeHandle() const {
    NativeHandle handle;
    handle.platform = NativePlatform::Cocoa;
    handle.window = m_window;
    handle.context = m_context;
    return handle;
}

ivec2 CocoaWindowBackend::getSize() const {
    const NSRect bounds = m_view ? m_view.bounds : NSMakeRect(0, 0, m_size.x, m_size.y);
    return ivec2(static_cast<i32>(bounds.size.width), static_cast<i32>(bounds.size.height));
}

ivec2 CocoaWindowBackend::getFramebufferSize() const {
    ivec2 size = getSize();
    if (m_view) {
        const NSRect backing = [m_view convertRectToBacking:m_view.bounds];
        size = ivec2(static_cast<i32>(backing.size.width), static_cast<i32>(backing.size.height));
    }
    return size;
}

void CocoaWindowBackend::setSwapInterval(i32 interval) {
    m_swapInterval = interval;
    GLint value = interval;
    [m_context setValues:&value forParameter:NSOpenGLContextParameterSwapInterval];
}

bool CocoaWindowBackend::isExtensionSupported(const char* name) {
    using GetStringiProc = const unsigned char* (*)(unsigned int, unsigned int);
    using GetIntegervProc = void (*)(unsigned int, int*);
    auto getStringi = reinterpret_cast<GetStringiProc>(getProcAddress("glGetStringi"));
    auto getIntegerv = reinterpret_cast<GetIntegervProc>(getProcAddress("glGetIntegerv"));
    if (!getStringi || !getIntegerv) {
        return false;
    }
    int count = 0;
    getIntegerv(0x821D, &count);
    for (int i = 0; i < count; ++i) {
        const auto* extension = reinterpret_cast<const char*>(
            getStringi(0x1F03, static_cast<unsigned int>(i)));
        if (extension && std::strcmp(extension, name) == 0) {
            return true;
        }
    }
    return false;
}

void CocoaWindowBackend::setTitle(std::string_view title) {
    m_title = std::string(title);
    NSString* string = nsStringFromUtf8(m_title);
    [m_window setTitle:string];
    [string release];
}

void CocoaWindowBackend::setSize(ivec2 size) {
    [m_window setContentSize:NSMakeSize(size.x, size.y)];
}

void CocoaWindowBackend::setPosition(ivec2 position) {
    NSScreen* primary = [NSScreen screens].firstObject;
    const CGFloat primaryHeight = primary ? primary.frame.size.height : 0.0;
    const NSRect frame = m_window.frame;
    const CGFloat y = primaryHeight - position.y - frame.size.height;
    [m_window setFrameOrigin:NSMakePoint(position.x, y)];
}

ivec2 CocoaWindowBackend::getPosition() const {
    const NSRect frame = m_window.frame;
    NSScreen* primary = [NSScreen screens].firstObject;
    const CGFloat primaryHeight = primary ? primary.frame.size.height : 0.0;
    return ivec2(static_cast<i32>(frame.origin.x),
                 static_cast<i32>(primaryHeight - frame.origin.y - frame.size.height));
}

ivec4 CocoaWindowBackend::getFrameSize() const {
    const NSRect frame = m_window.frame;
    const NSRect content = [m_window contentRectForFrameRect:frame];
    const i32 left = static_cast<i32>(content.origin.x - frame.origin.x);
    const i32 bottom = static_cast<i32>(content.origin.y - frame.origin.y);
    const i32 top = static_cast<i32>(frame.size.height - content.size.height - bottom);
    const i32 right = static_cast<i32>(frame.size.width - content.size.width - left);
    return ivec4(left, top, right, bottom);
}

vec2 CocoaWindowBackend::getContentScale() const {
    const CGFloat scale = m_window ? m_window.backingScaleFactor : 1.0;
    return {static_cast<f32>(scale), static_cast<f32>(scale)};
}

void CocoaWindowBackend::show() {
    [m_window makeKeyAndOrderFront:nil];
    m_visible = true;
}

void CocoaWindowBackend::hide() {
    [m_window orderOut:nil];
    m_visible = false;
}

void CocoaWindowBackend::focus() {
    [NSApp activateIgnoringOtherApps:YES];
    [m_window makeKeyAndOrderFront:nil];
}

void CocoaWindowBackend::requestAttention() {
    [NSApp requestUserAttention:NSCriticalRequest];
}

void CocoaWindowBackend::iconify() {
    [m_window miniaturize:nil];
}

void CocoaWindowBackend::restore() {
    [m_window deminiaturize:nil];
    m_iconified = false;
}

void CocoaWindowBackend::maximize() {
    if (![m_window isZoomed]) {
        [m_window zoom:nil];
    }
}

i32 CocoaWindowBackend::getAttrib(WindowAttrib attrib) const {
    switch (attrib) {
    case WindowAttrib::Focused: return m_window.isKeyWindow ? 1 : 0;
    case WindowAttrib::Iconified: return m_window.isMiniaturized ? 1 : 0;
    case WindowAttrib::Resizable: return m_resizable ? 1 : 0;
    case WindowAttrib::Visible: return m_window.isVisible ? 1 : 0;
    case WindowAttrib::Decorated: return m_decorated ? 1 : 0;
    case WindowAttrib::AutoIconify: return m_hints.autoIconify ? 1 : 0;
    case WindowAttrib::Floating: return m_window.level == NSFloatingWindowLevel ? 1 : 0;
    case WindowAttrib::Maximized: return m_window.isZoomed ? 1 : 0;
    case WindowAttrib::Hovered: return m_hovered ? 1 : 0;
    case WindowAttrib::FocusOnShow: return m_hints.focusOnShow ? 1 : 0;
    case WindowAttrib::MousePassthrough: return m_window.ignoresMouseEvents ? 1 : 0;
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

void CocoaWindowBackend::setAttrib(WindowAttrib attrib, bool value) {
    switch (attrib) {
    case WindowAttrib::Visible: value ? show() : hide(); break;
    case WindowAttrib::Resizable:
        m_resizable = value;
        {
            NSUInteger style = m_window.styleMask;
            if (value) {
                style |= NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
            } else {
                style &= ~(NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable);
            }
            [m_window setStyleMask:style];
        }
        break;
    case WindowAttrib::Decorated:
        m_decorated = value;
        [m_window setStyleMask:value ? (m_window.styleMask | NSWindowStyleMaskTitled)
                                     : NSWindowStyleMaskBorderless];
        break;
    case WindowAttrib::Floating:
        [m_window setLevel:value ? NSFloatingWindowLevel : NSNormalWindowLevel];
        break;
    case WindowAttrib::Maximized: value ? maximize() : restore(); break;
    case WindowAttrib::MousePassthrough:
        [m_window setIgnoresMouseEvents:value];
        break;
    case WindowAttrib::ScaleToMonitor:
        m_hints.scaleToMonitor = value;
        break;
    default: break;
    }
}

void CocoaWindowBackend::setOpacity(f32 opacity) {
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
    [m_window setAlphaValue:m_opacity];
}

void CocoaWindowBackend::setSizeLimits(ivec2 minimum, ivec2 maximum) {
    [m_window setContentMinSize:NSMakeSize(std::max(minimum.x, 0), std::max(minimum.y, 0))];
    [m_window setContentMaxSize:NSMakeSize(maximum.x > 0 ? maximum.x : CGFLOAT_MAX,
                                           maximum.y > 0 ? maximum.y : CGFLOAT_MAX)];
}

void CocoaWindowBackend::setAspectRatio(i32 numerator, i32 denominator) {
    if (numerator > 0 && denominator > 0) {
        [m_window setContentAspectRatio:NSMakeSize(numerator, denominator)];
    }
}

void CocoaWindowBackend::setIcon(std::span<const Image> images) {
    m_icon.assign(images.begin(), images.end());
    if (images.empty() || images[0].size.x <= 0 || images[0].size.y <= 0) {
        return;
    }
    const Image& image = images[0];
    const auto pixels = static_cast<std::size_t>(image.size.x) *
                        static_cast<std::size_t>(image.size.y);
    if (image.rgba8.size() < pixels * 4) {
        return;
    }
    NSBitmapImageRep* rep =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                pixelsWide:image.size.x
                                                pixelsHigh:image.size.y
                                             bitsPerSample:8
                                           samplesPerPixel:4
                                                  hasAlpha:YES
                                                  isPlanar:NO
                                            colorSpaceName:NSDeviceRGBColorSpace
                                               bytesPerRow:image.size.x * 4
                                              bitsPerPixel:32];
    std::memcpy([rep bitmapData], image.rgba8.data(), pixels * 4);
    NSImage* nsImage = [[NSImage alloc] initWithSize:NSMakeSize(image.size.x, image.size.y)];
    [nsImage addRepresentation:rep];
    [NSApp setApplicationIconImage:nsImage];
    [nsImage release];
    [rep release];
}

void CocoaWindowBackend::setMonitor(const Monitor& monitor, ivec2, ivec2 size, i32) {
    if (monitor.isValid() && size.x > 0 && size.y > 0) {
        NSScreen* screen = screenFromMonitor(monitor);
        if (screen) {
            if (!m_fullscreen) {
                m_windowedFrame = m_window.frame;
            }
            m_fullscreen = true;
            m_fullscreenScreen = screen;
            [m_window setStyleMask:NSWindowStyleMaskBorderless];
            [m_window setFrame:screen.frame display:YES];
            [m_window setLevel:NSMainMenuWindowLevel];
        }
    } else if (m_fullscreen) {
        m_fullscreen = false;
        m_fullscreenScreen = nil;
        [m_window setStyleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                               NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable];
        [m_window setFrame:m_windowedFrame display:YES];
        [m_window setLevel:NSNormalWindowLevel];
    }
}

Monitor CocoaWindowBackend::getMonitor() const {
    NSScreen* screen = m_window ? m_window.screen : nil;
    if (!screen) {
        screen = [NSScreen screens].firstObject;
    }
    return screen ? makeMonitor(screen) : Monitor{};
}

void CocoaWindowBackend::setInputMode(InputMode mode, i32 value) {
    switch (mode) {
    case InputMode::Cursor:
        m_cursorMode = static_cast<CursorMode>(value);
        updateCursor();
        break;
    case InputMode::StickyKeys: m_stickyKeys = value != 0; break;
    case InputMode::StickyMouseButtons: break;
    case InputMode::LockKeyMods: m_lockKeyMods = value != 0; break;
    case InputMode::RawMouseMotion: m_rawMouseMotion = value != 0; break;
    }
}

i32 CocoaWindowBackend::getInputMode(InputMode mode) const {
    switch (mode) {
    case InputMode::Cursor: return static_cast<i32>(m_cursorMode);
    case InputMode::StickyKeys: return m_stickyKeys ? 1 : 0;
    case InputMode::StickyMouseButtons: return 0;
    case InputMode::LockKeyMods: return m_lockKeyMods ? 1 : 0;
    case InputMode::RawMouseMotion: return m_rawMouseMotion ? 1 : 0;
    }
    return 0;
}

std::string CocoaWindowBackend::getKeyName(Key key, i32) const {
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
    return {};
}

i32 CocoaWindowBackend::getKeyScancode(Key key) const {
    const unsigned short code = keyCodeForKey(key);
    return code == 0xFFFF ? -1 : static_cast<i32>(code);
}

bool CocoaWindowBackend::isKeyPressed(Key key) const {
    const unsigned short code = keyCodeForKey(key);
    return code != 0xFFFF && m_pressedKeys.contains(code);
}

bool CocoaWindowBackend::isMouseButtonPressed(MouseButton button) const {
    const auto index = static_cast<std::size_t>(button);
    return index < m_pressedButtons.size() && m_pressedButtons[index];
}

void CocoaWindowBackend::setCursorPos(vec2 position) {
    const NSPoint point = NSMakePoint(position.x, flipY(position.y, getSize().y));
    const NSPoint screenPoint = [m_window convertPointToScreen:[m_view convertPoint:point
                                                                             toView:nil]];
    CGWarpMouseCursorPosition(CGPointMake(screenPoint.x,
                                          [[NSScreen screens].firstObject frame].size.height -
                                              screenPoint.y));
    m_cursorPos = position;
}

void CocoaWindowBackend::setCursor(CursorBackend* cursor) {
    m_cursor = dynamic_cast<CocoaCursorBackend*>(cursor);
    updateCursor();
}

void CocoaWindowBackend::updateCursor() {
    if (m_cursorMode == CursorMode::Normal || m_cursorMode == CursorMode::Hidden) {
        if (m_cursorHidden) {
            [NSCursor unhide];
            m_cursorHidden = false;
        }
        if (m_cursorAssociated) {
            CGAssociateMouseAndMouseCursorPosition(true);
            m_cursorAssociated = false;
        }
        if (m_cursorMode == CursorMode::Normal) {
            if (m_cursor) {
                [m_cursor->getCursor() set];
            } else {
                [[NSCursor arrowCursor] set];
            }
        } else if (!m_cursorHidden) {
            [NSCursor hide];
            m_cursorHidden = true;
        }
    } else {
        if (!m_cursorHidden) {
            [NSCursor hide];
            m_cursorHidden = true;
        }
        if (!m_cursorAssociated) {
            CGAssociateMouseAndMouseCursorPosition(false);
            m_cursorAssociated = true;
        }
    }
}

std::string CocoaWindowBackend::getClipboardString() const {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
    return utf8FromNSString(string);
}

void CocoaWindowBackend::setClipboardString(std::string_view text) {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    NSString* string = nsStringFromUtf8(text);
    [pasteboard setString:string forType:NSPasteboardTypeString];
    [string release];
}

void CocoaWindowBackend::handleKeyDown(NSEvent* event, bool down) {
    const unsigned short keyCode = [event keyCode];
    const Key key = translateKeyCode(keyCode);
    KeyAction action = down ? KeyAction::Press : KeyAction::Release;
    if (down) {
        if ([event isARepeat] || m_pressedKeys.contains(keyCode)) {
            action = KeyAction::Repeat;
        } else {
            m_pressedKeys.insert(keyCode);
        }
    } else {
        m_pressedKeys.erase(keyCode);
    }

    Modifiers mods = modifiersFromEvent(event);
    if (!m_lockKeyMods) {
        mods = static_cast<Modifiers>(static_cast<int>(mods) &
                                      ~static_cast<int>(Modifiers::CapsLock));
    }
    if (m_events) {
        m_events->onKey(key, static_cast<i32>(keyCode), action, mods);
    }

    if (down) {
        NSString* characters = [event characters];
        if (characters) {
            const NSUInteger length = [characters length];
            for (NSUInteger i = 0; i < length; ++i) {
                const unichar character = [characters characterAtIndex:i];
                if (m_events && character >= 32) {
                    m_events->onChar(static_cast<u32>(character));
                    m_events->onCharMods(static_cast<u32>(character), mods);
                }
            }
        }
    }
}

void CocoaWindowBackend::handleFlagsChanged(NSEvent* event) {
    const unsigned short keyCode = [event keyCode];
    const Key key = translateKeyCode(keyCode);
    const bool pressed = !m_pressedKeys.contains(keyCode);
    if (pressed) {
        m_pressedKeys.insert(keyCode);
    } else {
        m_pressedKeys.erase(keyCode);
    }
    if (m_events) {
        m_events->onKey(key, static_cast<i32>(keyCode),
                        pressed ? KeyAction::Press : KeyAction::Release,
                        modifiersFromEvent(event));
    }
}

void CocoaWindowBackend::handleMouse(NSEvent* event, MouseButton button, bool down) {
    const auto index = static_cast<std::size_t>(button);
    if (index < m_pressedButtons.size()) {
        m_pressedButtons[index] = down;
    }
    if (m_events) {
        m_events->onMouseButton(button, down ? KeyAction::Press : KeyAction::Release,
                                modifiersFromEvent(event));
    }
}

void CocoaWindowBackend::handleCursorMove(NSEvent* event) {
    if (!m_view) return;
    const NSPoint point = [m_view convertPoint:[event locationInWindow] fromView:nil];
    m_cursorPos = vec2(static_cast<f32>(point.x),
                       static_cast<f32>(flipY(point.y, getSize().y)));
    m_hovered = true;
    if (m_cursorMode == CursorMode::Disabled || m_cursorMode == CursorMode::Captured) {
        const vec2 center(static_cast<f32>(getSize().x) * 0.5f,
                          static_cast<f32>(getSize().y) * 0.5f);
        m_cursorPos = center + (m_cursorPos - center);
    }
    if (m_events) {
        m_events->onCursorPos(m_cursorPos);
    }
}

void CocoaWindowBackend::handleScroll(NSEvent* event) {
    if (m_events) {
        const f32 x = static_cast<f32>([event scrollingDeltaX]);
        const f32 y = static_cast<f32>([event scrollingDeltaY]);
        m_events->onScroll(vec2(x, y));
    }
}

void CocoaWindowBackend::handleResize() {
    const ivec2 size = getSize();
    if (size != m_size) {
        m_size = size;
        if (m_events) {
            m_events->onSize(size);
            m_events->onFramebufferSize(getFramebufferSize());
        }
    }
    const vec2 scale = getContentScale();
    if (m_events) {
        m_events->onContentScale(scale);
    }
}

void CocoaWindowBackend::handleMove() {
    const ivec2 position = getPosition();
    if (position != m_position) {
        m_position = position;
        if (m_events) m_events->onPosition(position);
    }
}

void CocoaWindowBackend::handleFocus(bool focused) {
    m_focused = focused;
    if (m_events) m_events->onFocus(focused);
}

void CocoaWindowBackend::handleClose() {
    m_closeRequested = true;
    if (m_events) m_events->onClose();
}

void CocoaWindowBackend::handleMiniaturize(bool minimized) {
    m_iconified = minimized;
    if (m_events) m_events->onIconify(minimized);
}

void CocoaWindowBackend::handleZoom(bool zoomed) {
    m_maximized = zoomed;
    if (m_events) m_events->onMaximize(zoomed);
}

void CocoaWindowBackend::handleDrop(NSArray<NSURL*>* urls) {
    std::vector<std::string> paths;
    for (NSURL* url in urls) {
        paths.push_back(utf8FromNSString([url path]));
    }
    if (m_events && !paths.empty()) {
        m_events->onDrop(std::move(paths));
    }
}

void CocoaWindowBackend::handleScreenChange() {
    if (m_events) {
        m_events->onContentScale(getContentScale());
    }
}

bool CocoaBackendImpl::init(const InitOptions&, Error& error) {
    if (g_state.initialized) {
        return true;
    }

    @autoreleasepool {
        if (!NSApp) {
            [NSApplication sharedApplication];
        }
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        g_state.applicationConfigured = true;

        g_state.screenObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:NSApplicationDidChangeScreenParametersNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification*) {
                        rebuildMonitors(true);
                    }];

        IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        if (manager) {
            NSArray* matching = @[
                @{[NSString stringWithUTF8String:kIOHIDDeviceUsagePageKey] : @(kHIDPage_GenericDesktop),
                  [NSString stringWithUTF8String:kIOHIDDeviceUsageKey] : @(kHIDUsage_GD_Joystick)},
                @{[NSString stringWithUTF8String:kIOHIDDeviceUsagePageKey] : @(kHIDPage_GenericDesktop),
                  [NSString stringWithUTF8String:kIOHIDDeviceUsageKey] : @(kHIDUsage_GD_GamePad)},
                @{[NSString stringWithUTF8String:kIOHIDDeviceUsagePageKey] : @(kHIDPage_GenericDesktop),
                  [NSString stringWithUTF8String:kIOHIDDeviceUsageKey] : @(kHIDUsage_GD_MultiAxisController)}
            ];
            IOHIDManagerSetDeviceMatchingMultiple(manager, (CFArrayRef)matching);
            IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
            g_state.hidManager = manager;
        }
    }

    rebuildMonitors(false);
    refreshJoysticks();
    g_state.initialized = true;
    return true;
}

void CocoaBackendImpl::terminate() {
    if (!g_state.initialized) {
        return;
    }
    std::vector<CocoaWindowBackend*> windows;
    for (auto& [window, backend] : g_state.windows) {
        (void)window;
        windows.push_back(backend);
    }
    for (auto* window : windows) {
        window->destroy();
    }
    g_state.windows.clear();
    for (auto& joystick : g_state.joysticks) {
        if (joystick.device) {
            IOHIDDeviceClose(joystick.device, kIOHIDOptionsTypeNone);
        }
    }
    g_state.joysticks.clear();
    if (g_state.hidManager) {
        IOHIDManagerClose(g_state.hidManager, kIOHIDOptionsTypeNone);
        CFRelease(g_state.hidManager);
        g_state.hidManager = nullptr;
    }
    if (g_state.screenObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:g_state.screenObserver];
        g_state.screenObserver = nil;
    }
    g_state.monitors.clear();
    g_state.monitorHandles.clear();
    g_state.monitorCallback = nullptr;
    g_state.joystickCallback = nullptr;
    g_state.initialized = false;
}

std::unique_ptr<CursorBackend> CocoaBackendImpl::createCursor(const Image& image, ivec2 hotspot,
                                                              Error& error) {
    if (image.size.x <= 0 || image.size.y <= 0) {
        error = makeError(ErrorCode::InvalidValue, "Cursor-Bild hat eine ungueltige Groesse.");
        return nullptr;
    }
    const auto pixels = static_cast<std::size_t>(image.size.x) *
                        static_cast<std::size_t>(image.size.y);
    if (image.rgba8.size() < pixels * 4) {
        error = makeError(ErrorCode::InvalidValue, "Cursor-Bild enthaelt zu wenige Pixel.");
        return nullptr;
    }

    NSBitmapImageRep* rep =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                pixelsWide:image.size.x
                                                pixelsHigh:image.size.y
                                             bitsPerSample:8
                                           samplesPerPixel:4
                                                  hasAlpha:YES
                                                  isPlanar:NO
                                            colorSpaceName:NSDeviceRGBColorSpace
                                               bytesPerRow:image.size.x * 4
                                              bitsPerPixel:32];
    std::memcpy([rep bitmapData], image.rgba8.data(), pixels * 4);
    NSImage* nsImage = [[NSImage alloc] initWithSize:NSMakeSize(image.size.x, image.size.y)];
    [nsImage addRepresentation:rep];
    NSCursor* cursor = [[NSCursor alloc] initWithImage:nsImage
                                               hotSpot:NSMakePoint(hotspot.x, hotspot.y)];
    [nsImage release];
    [rep release];
    if (!cursor) {
        error = makeError(ErrorCode::PlatformError, "Cursor konnte nicht erstellt werden.");
        return nullptr;
    }
    return std::make_unique<CocoaCursorBackend>(cursor, true);
}

std::unique_ptr<CursorBackend> CocoaBackendImpl::createStandardCursor(StandardCursor shape,
                                                                       Error& error) {
    NSCursor* cursor = [NSCursor arrowCursor];
    switch (shape) {
    case StandardCursor::Arrow: cursor = [NSCursor arrowCursor]; break;
    case StandardCursor::IBeam: cursor = [NSCursor IBeamCursor]; break;
    case StandardCursor::Crosshair: cursor = [NSCursor crosshairCursor]; break;
    case StandardCursor::Hand: cursor = [NSCursor pointingHandCursor]; break;
    case StandardCursor::HResize: cursor = [NSCursor resizeLeftRightCursor]; break;
    case StandardCursor::VResize: cursor = [NSCursor resizeUpDownCursor]; break;
    }
    if (!cursor) {
        error = makeError(ErrorCode::PlatformError, "Standard-Cursor konnte nicht geladen werden.");
        return nullptr;
    }
    return std::make_unique<CocoaCursorBackend>(cursor, false);
}

void CocoaBackendImpl::pollEvents() {
    @autoreleasepool {
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]) != nil) {
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
    refreshJoysticks();
}

void CocoaBackendImpl::waitEvents(f64 timeout) {
    @autoreleasepool {
        NSDate* until = timeout < 0.0 ? [NSDate distantFuture]
                                      : [NSDate dateWithTimeIntervalSinceNow:timeout];
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:until
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }
        pollEvents();
    }
}

void CocoaBackendImpl::postEmptyEvent() {
    NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:NO];
}

} // namespace sgl::detail::cocoa

namespace sgl::detail {

std::unique_ptr<Backend> createCocoaBackend(Error& error) {
    auto backend = std::make_unique<cocoa::CocoaBackendImpl>();
    (void)error;
    return backend;
}

} // namespace sgl::detail

#endif // __APPLE__
