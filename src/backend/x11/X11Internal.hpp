#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "backend/Backend.hpp"
#include "sgl/Monitor.hpp"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>
#include <X11/XKBlib.h>

#include <GL/glx.h>
#include <GL/glxext.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

// X11 defines None as a macro, which clashes with our scoped enumerators.
#undef None

namespace sgl::detail::x11 {

inline constexpr int X11None = 0;

struct State {
    Display* display = nullptr;
    int screen = 0;
    ::Window root = 0;

    bool hasXrandr = false;
    bool hasXinerama = false;
    bool hasXcursor = false;
    bool hasXi2 = false;
    bool detectableRepeat = false;

    int xiOpcode = 0;
    int rrEventBase = 0;
    int rrErrorBase = 0;

    Atom wmProtocols = 0;
    Atom wmDeleteWindow = 0;
    Atom wmTakeFocus = 0;
    Atom netWmName = 0;
    Atom netWmIcon = 0;
    Atom netWmPid = 0;
    Atom netWmState = 0;
    Atom netWmStateFullscreen = 0;
    Atom netWmStateMaximizedVert = 0;
    Atom netWmStateMaximizedHorz = 0;
    Atom netWmStateHidden = 0;
    Atom netWmStateFocused = 0;
    Atom netWmStateAbove = 0;
    Atom netWmStateDemandsAttention = 0;
    Atom netWmWindowOpacity = 0;
    Atom netFrameExtents = 0;
    Atom netWorkarea = 0;
    Atom motifWmHints = 0;
    Atom utf8String = 0;
    Atom clipboard = 0;
    Atom targets = 0;
    Atom incr = 0;
    Atom atomPair = 0;
    Atom deleteAtom = 0;
    Atom sglSelection = 0;
    Atom textUriList = 0;
    Atom xdndAware = 0;
    Atom xdndEnter = 0;
    Atom xdndPosition = 0;
    Atom xdndStatus = 0;
    Atom xdndLeave = 0;
    Atom xdndDrop = 0;
    Atom xdndFinished = 0;
    Atom xdndSelection = 0;
    Atom xdndTypeList = 0;

    XIM im = nullptr;
    ::Cursor invisibleCursor = 0;
    int pipeFds[2] = {-1, -1};

    std::string clipboardString;
    bool clipboardOwned = false;
    std::string selectionResult;
    bool selectionReady = false;

    std::vector<Monitor> monitors;
    std::vector<void*> monitorHandles;
    MonitorCallback monitorCallback;

    bool initialized = false;
};

State& state();
void rebuildMonitors(bool notify);
void processPendingEvents();

Key translateKeysym(KeySym keysym);
KeySym keysymForKey(Key key);
int scancodeForKey(Key key);
std::string keyName(Key key, i32 scancode);

class X11WindowBackend;

void registerWindow(X11WindowBackend* window, ::Window handle);
void unregisterWindow(::Window handle);
X11WindowBackend* findWindow(::Window handle);
X11WindowBackend* focusedWindow();

class X11CursorBackend final : public CursorBackend {
public:
    X11CursorBackend(::Cursor cursor, bool owned) : m_cursor(cursor), m_owned(owned) {}
    ~X11CursorBackend() override {
        if (m_owned && m_cursor != 0 && state().display) {
            XFreeCursor(state().display, m_cursor);
        }
    }
    [[nodiscard]] void* getNativeHandle() const override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_cursor));
    }
    [[nodiscard]] ::Cursor getCursor() const { return m_cursor; }

private:
    ::Cursor m_cursor = 0;
    bool m_owned = false;
};

class X11WindowBackend final : public WindowBackend {
public:
    X11WindowBackend(const WindowHints& hints, Error& error);
    ~X11WindowBackend() override;

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
    [[nodiscard]] bool isRawMouseMotionSupported() const override;
    [[nodiscard]] std::string getKeyName(Key key, i32 scancode) const override;
    [[nodiscard]] i32 getKeyScancode(Key key) const override;
    [[nodiscard]] bool isKeyPressed(Key key) const override;
    [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const override;
    [[nodiscard]] vec2 getCursorPos() const override;
    void setCursorPos(vec2 position) override;
    void setCursor(CursorBackend* cursor) override;
    [[nodiscard]] std::string getClipboardString() const override;
    void setClipboardString(std::string_view text) override;

    void handleEvent(XEvent& event);
    void destroy();
    void handleClientMessage(XClientMessageEvent& event);
    void handleSelectionRequest(XSelectionRequestEvent& event);
    void handleSelectionNotify(XSelectionEvent& event);
    void handleSelectionClear(XSelectionClearEvent& event);
    void handleXdnd(XClientMessageEvent& event);
    void handleRawMotion(XIRawEvent& event);

    void createEglContext(const WindowHints& hints, Error& error);

    [[nodiscard]] ::Window getX11Window() const { return m_window; }
    [[nodiscard]] GLXContext getGLXContext() const { return m_glxContext; }
    [[nodiscard]] GLXWindow getGLXWindow() const { return m_glxWindow; }
    [[nodiscard]] Display* getDisplay() const { return state().display; }
    [[nodiscard]] bool isFullscreen() const { return m_fullscreen; }

    void setClipboardText(std::string text) { m_clipboardText = std::move(text); }
    [[nodiscard]] const std::string& getClipboardText() const { return m_clipboardText; }

private:
    void updateCursorMode();
    void warpCursorToCenter();
    void updateCursorPositionFromEvent(int x, int y);
    void updateModifiers(unsigned int state, Modifiers& mods) const;
    void updateNetWmState();
    void sendXdndStatus(XClientMessageEvent& event, bool accept);

    ::Window m_window = 0;
    Colormap m_colormap = 0;
    GLXContext m_glxContext = nullptr;
    GLXWindow m_glxWindow = 0;
    XIC m_ic = nullptr;

    bool m_useEgl = false;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLConfig m_eglConfig = nullptr;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    EGLContext m_eglContext = EGL_NO_CONTEXT;

    ivec2 m_size{0, 0};
    ivec2 m_position{0, 0};
    std::string m_title;
    WindowEvents* m_events = nullptr;

    bool m_closeRequested = false;
    bool m_visible = false;
    bool m_iconified = false;
    bool m_maximized = false;
    bool m_focused = false;
    bool m_hovered = false;
    bool m_resizable = true;
    bool m_decorated = true;
    bool m_floating = false;
    bool m_autoIconify = true;
    bool m_focusOnShow = true;
    bool m_mousePassthrough = false;
    bool m_doublebuffer = true;
    bool m_fullscreen = false;
    bool m_restoreWindowedGeometry = false;
    bool m_scaleToMonitor = false;
    bool m_pointerConfined = false;
    bool m_focusPending = false;
    bool m_mapped = false;

    i32 m_swapInterval = 0;
    f32 m_opacity = 1.0f;

    CursorMode m_cursorMode = CursorMode::Normal;
    bool m_stickyKeys = false;
    bool m_stickyMouseButtons = false;
    bool m_lockKeyMods = true;
    bool m_rawMouseMotion = false;

    std::unordered_set<int> m_pressedKeys;
    std::array<bool, 8> m_pressedMouseButtons{};
    Modifiers m_stickyMods = Modifiers::None;
    Modifiers m_stickyButtons = Modifiers::None;

    mutable vec2 m_cursorPos{0.0f, 0.0f};
    vec2 m_lastCursorPos{0.0f, 0.0f};
    mutable bool m_cursorTracked = false;
    vec2 m_rawDelta{0.0f, 0.0f};
    X11CursorBackend* m_cursor = nullptr;

    ivec2 m_sizeLimitsMin{0, 0};
    ivec2 m_sizeLimitsMax{0, 0};
    i32 m_aspectNumerator = 0;
    i32 m_aspectDenominator = 0;

    std::vector<WindowIcon> m_icon;
    std::vector<std::vector<u8>> m_iconStorage;

    void* m_fullscreenMonitor = nullptr;
    ivec2 m_windowedPosition{0, 0};
    ivec2 m_windowedSize{0, 0};

    std::string m_clipboardText;
    std::string m_dropText;
    ::Window m_xdndSource = 0;

    bool m_glx = false;
    WindowHints m_hints;
};

} // namespace sgl::detail::x11
