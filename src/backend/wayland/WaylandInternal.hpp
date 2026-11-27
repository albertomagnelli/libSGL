#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "backend/Backend.hpp"
#include "sgl/Monitor.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"

namespace sgl::detail::wayland {

struct WaylandOutput {
    wl_output* output = nullptr;
    zxdg_output_v1* xdgOutput = nullptr;
    ivec2 position{0, 0};
    ivec2 logicalSize{0, 0};
    ivec2 physicalSize{0, 0};
    std::string name;
    std::string description;
    int scale = 1;
    int transform = 0;
    i32 refreshRate = 0;
    bool hasMode = false;
};

struct WaylandState {
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    wl_shm* shm = nullptr;
    wl_seat* seat = nullptr;
    wl_pointer* pointer = nullptr;
    wl_keyboard* keyboard = nullptr;
    wl_touch* touch = nullptr;
    xdg_wm_base* wmBase = nullptr;
    zxdg_output_manager_v1* outputManager = nullptr;
    zxdg_decoration_manager_v1* decorationManager = nullptr;
    wp_viewporter* viewporter = nullptr;
    wl_data_device_manager* dataDeviceManager = nullptr;
    wl_data_device* dataDevice = nullptr;
    wl_cursor_theme* cursorTheme = nullptr;

    std::vector<std::unique_ptr<WaylandOutput>> outputs;
    std::unordered_map<wl_surface*, class WaylandWindowBackend*> windows;

    class WaylandWindowBackend* keyboardFocus = nullptr;
    class WaylandWindowBackend* pointerFocus = nullptr;

    uint32_t lastSerial = 0;
    uint32_t pointerEnterSerial = 0;

    MonitorCallback monitorCallback;

    int pipeFds[2] = {-1, -1};

    std::string clipboardText;
    wl_data_source* clipboardSource = nullptr;
    wl_data_offer* clipboardOffer = nullptr;
    wl_data_offer* dropOffer = nullptr;

    bool initialized = false;
};

WaylandState& state();
WaylandOutput* findOutput(wl_output* output);
class WaylandWindowBackend* windowForSurface(wl_surface* surface);
const wl_data_source_listener* dataSourceListenerPtr();
void rebuildMonitors(bool notify);
void dispatchEvents(bool block, f64 timeout);

class WaylandCursorBackend final : public CursorBackend {
public:
    WaylandCursorBackend(wl_surface* surface, wl_buffer* buffer, ivec2 size, ivec2 hotspot,
                         bool ownsBuffer = true)
        : m_surface(surface), m_buffer(buffer), m_size(size), m_hotspot(hotspot),
          m_ownsBuffer(ownsBuffer) {}
    ~WaylandCursorBackend() override;

    [[nodiscard]] void* getNativeHandle() const override {
        return reinterpret_cast<void*>(m_surface);
    }
    [[nodiscard]] wl_surface* getSurface() const { return m_surface; }
    [[nodiscard]] wl_buffer* getBuffer() const { return m_buffer; }
    [[nodiscard]] ivec2 getHotspot() const { return m_hotspot; }
    [[nodiscard]] ivec2 getSize() const { return m_size; }

private:
    wl_surface* m_surface = nullptr;
    wl_buffer* m_buffer = nullptr;
    ivec2 m_size{0, 0};
    ivec2 m_hotspot{0, 0};
    bool m_ownsBuffer = true;
};

class WaylandWindowBackend final : public WindowBackend {
public:
    WaylandWindowBackend(const WindowHints& hints, Error& error);
    ~WaylandWindowBackend() override;

    void setEvents(WindowEvents* events) override { m_events = events; }
    void makeContextCurrent() override;
    void swapBuffers() override;
    void* getProcAddress(const char* name) override;
    [[nodiscard]] NativeHandle getNativeHandle() const override;
    [[nodiscard]] bool isCloseRequested() const override { return m_closeRequested; }
    void setCloseRequested(bool requested) override { m_closeRequested = requested; }
    [[nodiscard]] ivec2 getSize() const override { return m_size; }
    [[nodiscard]] ivec2 getFramebufferSize() const override;
    void setSwapInterval(i32 interval) override;
    [[nodiscard]] i32 getSwapInterval() const override { return m_swapInterval; }
    [[nodiscard]] bool isExtensionSupported(const char* name) override;
    [[nodiscard]] std::string getTitle() const override { return m_title; }
    void setTitle(std::string_view title) override;
    void setSize(ivec2 size) override;
    void setPosition(ivec2 position) override;
    [[nodiscard]] ivec2 getPosition() const override { return {0, 0}; }
    [[nodiscard]] ivec4 getFrameSize() const override { return {0, 0, 0, 0}; }
    [[nodiscard]] vec2 getContentScale() const override;
    void show() override;
    void hide() override;
    void focus() override {}
    void requestAttention() override;
    void iconify() override;
    void restore() override;
    void maximize() override;
    [[nodiscard]] i32 getAttrib(WindowAttrib attrib) const override;
    void setAttrib(WindowAttrib attrib, bool value) override;
    [[nodiscard]] f32 getOpacity() const override { return 1.0f; }
    void setOpacity(f32) override {}
    void setSizeLimits(ivec2 minimum, ivec2 maximum) override;
    void setAspectRatio(i32, i32) override {}
    void setIcon(std::span<const WindowIcon>) override {}
    [[nodiscard]] std::vector<WindowIcon> getIcon() const override { return {}; }
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

    void destroy();

    // Event handling
    void handleConfigure(ivec2 size);
    void handleToplevelConfigure(ivec2 size, bool maximized, bool fullscreen, bool activated);
    void handleClose();
    void handleSurfaceEnter(wl_output* output);
    void handleSurfaceLeave(wl_output* output);
    void handleKeymap(uint32_t format, int fd, uint32_t size);
    void handleKeyboardEnter(uint32_t serial, wl_array* keys);
    void handleKeyboardLeave();
    void handleKey(uint32_t serial, uint32_t time, uint32_t key, uint32_t keyState);
    void handleModifiers(uint32_t serial, uint32_t modsDepressed, uint32_t modsLatched,
                         uint32_t modsLocked, uint32_t group);
    void handlePointerEnter(uint32_t serial, wl_surface* surface, vec2 position);
    void handlePointerLeave();
    void handlePointerMotion(vec2 position);
    void handlePointerButton(uint32_t serial, uint32_t button, bool pressed);
    void handlePointerAxis(vec2 offset);
    void handleDrop(std::vector<std::string> paths);

    [[nodiscard]] wl_surface* getSurface() const { return m_surface; }
    [[nodiscard]] EGLSurface getEGLSurface() const { return m_eglSurface; }
    [[nodiscard]] EGLContext getEGLContext() const { return m_eglContext; }
    [[nodiscard]] EGLDisplay getEGLDisplay() const { return m_eglDisplay; }
    [[nodiscard]] bool isMapped() const { return m_mapped; }
    [[nodiscard]] WaylandOutput* getCurrentOutput() const { return m_currentOutput; }

    void updateCursor();
    void setLastSerial(uint32_t serial) { m_lastSerial = serial; }

private:
    void createEGL(Error& error);
    void destroyEGL();
    void updateInputRegion();
    void applyDecorations();

    wl_surface* m_surface = nullptr;
    xdg_surface* m_xdgSurface = nullptr;
    xdg_toplevel* m_toplevel = nullptr;
    zxdg_toplevel_decoration_v1* m_decoration = nullptr;
    wp_viewport* m_viewport = nullptr;
    wl_egl_window* m_eglWindow = nullptr;
    wl_region* m_inputRegion = nullptr;

    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLConfig m_eglConfig = nullptr;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    EGLContext m_eglContext = EGL_NO_CONTEXT;

    ivec2 m_size{0, 0};
    ivec2 m_pendingSize{0, 0};
    ivec2 m_minSize{0, 0};
    ivec2 m_maxSize{0, 0};
    std::string m_title;
    std::string m_appId = "sgl";
    WindowEvents* m_events = nullptr;
    WindowHints m_hints;

    bool m_closeRequested = false;
    bool m_configured = false;
    bool m_mapped = false;
    bool m_visible = false;
    bool m_maximized = false;
    bool m_fullscreen = false;
    bool m_focused = false;
    bool m_hovered = false;
    bool m_resizable = true;
    bool m_decorated = true;
    bool m_activated = false;
    bool m_scaleToMonitor = false;
    bool m_mousePassthrough = false;
    bool m_minimized = false;

    i32 m_swapInterval = 0;
    CursorMode m_cursorMode = CursorMode::Normal;
    bool m_stickyKeys = false;
    bool m_stickyMouseButtons = false;
    bool m_lockKeyMods = true;
    bool m_rawMouseMotion = false;

    xkb_context* m_xkbContext = nullptr;
    xkb_keymap* m_xkbKeymap = nullptr;
    xkb_state* m_xkbState = nullptr;
    std::unordered_set<uint32_t> m_pressedKeys;
    std::array<bool, 8> m_pressedButtons{};
    Modifiers m_stickyMods = Modifiers::None;
    Modifiers m_stickyButtons = Modifiers::None;

    vec2 m_cursorPos{0.0f, 0.0f};
    uint32_t m_lastSerial = 0;

    WaylandCursorBackend* m_cursor = nullptr;
    WaylandOutput* m_currentOutput = nullptr;
    float m_scale = 1.0f;

    Monitor m_fullscreenMonitor;
    std::string m_clipboardText;
};

} // namespace sgl::detail::wayland
