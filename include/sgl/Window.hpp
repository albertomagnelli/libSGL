#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Context.hpp"
#include "Cursor.hpp"
#include "Error.hpp"
#include "Init.hpp"
#include "Input.hpp"
#include "Monitor.hpp"
#include "Native.hpp"
#include "Types.hpp"

namespace sgl {

enum class WindowAttrib : i32 {
    Focused = 0x00020001,
    Iconified = 0x00020002,
    Resizable = 0x00020003,
    Visible = 0x00020004,
    Decorated = 0x00020005,
    AutoIconify = 0x00020006,
    Floating = 0x00020007,
    Maximized = 0x00020008,
    CenterCursor = 0x00020009,
    TransparentFramebuffer = 0x0002000A,
    Hovered = 0x0002000B,
    FocusOnShow = 0x0002000C,
    MousePassthrough = 0x0002000D,
    ScaleToMonitor = 0x0002200C,
    Doublebuffer = 0x00021010,
    ClientApi = 0x00022001,
    ContextCreationApi = 0x0002200B,
    ContextVersionMajor = 0x00022002,
    ContextVersionMinor = 0x00022003,
    ContextRevision = 0x00022004,
    ContextRobustness = 0x00022005,
    ContextReleaseBehavior = 0x00022009,
    ContextDebug = 0x00022007,
    ContextNoError = 0x0002200A,
    ContextForwardCompat = 0x00022006,
    OpenGLProfile = 0x00022008,
    Samples = 0x0002100D,
    RedBits = 0x00021001,
    GreenBits = 0x00021002,
    BlueBits = 0x00021003,
    AlphaBits = 0x00021004,
    DepthBits = 0x00021005,
    StencilBits = 0x00021006,
    AccumRedBits = 0x00021007,
    AccumGreenBits = 0x00021008,
    AccumBlueBits = 0x00021009,
    AccumAlphaBits = 0x0002100A,
    AuxBuffers = 0x0002100B,
    Stereo = 0x0002100C,
    SRGBCapable = 0x0002100E,
    RefreshRate = 0x0002100F,
};

struct WindowHints {
    i32 width = 800;
    i32 height = 600;
    std::string title;

    bool resizable = true;
    bool visible = true;
    bool decorated = true;
    bool focused = true;
    bool autoIconify = true;
    bool floating = false;
    bool maximized = false;
    bool centerCursor = false;
    bool transparentFramebuffer = false;
    bool focusOnShow = true;
    bool scaleToMonitor = false;
    bool mousePassthrough = false;
    bool doublebuffer = true;
    bool srgbCapable = false;
    bool stereo = false;
    bool cocoaRetinaFramebuffer = true;
    bool cocoaGraphicsSwitching = false;
    bool win32KeyboardMenu = false;

    ClientApi clientApi = ClientApi::OpenGL;
    ContextApi contextApi = ContextApi::Native;
    i32 contextVersionMajor = 1;
    i32 contextVersionMinor = 0;
    ContextProfile contextProfile = ContextProfile::Any;
    ContextRobustness contextRobustness = ContextRobustness::None;
    ContextReleaseBehavior contextReleaseBehavior = ContextReleaseBehavior::Any;
    bool contextDebug = false;
    bool contextNoError = false;
    bool contextForwardCompat = false;

    i32 redBits = 8;
    i32 greenBits = 8;
    i32 blueBits = 8;
    i32 alphaBits = 8;
    i32 depthBits = 24;
    i32 stencilBits = 8;
    i32 accumRedBits = 0;
    i32 accumGreenBits = 0;
    i32 accumBlueBits = 0;
    i32 accumAlphaBits = 0;
    i32 auxBuffers = 0;
    i32 samples = 0;
    i32 refreshRate = 0;

    std::string cocoaFrameName;
    std::string x11ClassName;
    std::string x11InstanceName;
};

namespace detail {

class WindowBackend;
struct WindowEvents {
    virtual ~WindowEvents() = default;
    virtual void onClose() = 0;
    virtual void onSize(ivec2 size) = 0;
    virtual void onFramebufferSize(ivec2 size) = 0;
    virtual void onContentScale(vec2 scale) = 0;
    virtual void onPosition(ivec2 position) = 0;
    virtual void onFocus(bool focused) = 0;
    virtual void onIconify(bool iconified) = 0;
    virtual void onMaximize(bool maximized) = 0;
    virtual void onRefresh() = 0;
    virtual void onKey(Key key, i32 scancode, KeyAction action, Modifiers mods) = 0;
    virtual void onChar(u32 codepoint) = 0;
    virtual void onCharMods(u32 codepoint, Modifiers mods) = 0;
    virtual void onMouseButton(MouseButton button, KeyAction action, Modifiers mods) = 0;
    virtual void onCursorPos(vec2 position) = 0;
    virtual void onCursorEnter(bool entered) = 0;
    virtual void onScroll(vec2 offset) = 0;
    virtual void onDrop(std::vector<std::string> paths) = 0;
};

} // namespace detail

WindowHints getDefaultWindowHints();

class Window final : public detail::WindowEvents {
public:
    static std::expected<std::unique_ptr<Window>, Error> create(const WindowHints& hints = {});
    ~Window() override;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Kontext
    void makeContextCurrent();
    void swapBuffers();
    void setSwapInterval(i32 interval);
    [[nodiscard]] i32 getSwapInterval() const;
    [[nodiscard]] void* getProcAddress(std::string_view name) const;
    [[nodiscard]] bool isExtensionSupported(std::string_view name) const;

    // Zustand
    [[nodiscard]] bool isCloseRequested() const;
    void setCloseRequested(bool requested);

    [[nodiscard]] std::string getTitle() const;
    void setTitle(std::string_view title);

    [[nodiscard]] ivec2 getSize() const;
    void setSize(ivec2 size);
    [[nodiscard]] ivec2 getPosition() const;
    void setPosition(ivec2 position);
    [[nodiscard]] ivec2 getFramebufferSize() const;
    [[nodiscard]] ivec4 getFrameSize() const;
    [[nodiscard]] vec2 getContentScale() const;

    void show();
    void hide();
    void focus();
    void requestAttention();
    void iconify();
    void restore();
    void maximize();

    [[nodiscard]] bool isAttrib(WindowAttrib attrib) const;
    [[nodiscard]] i32 getAttrib(WindowAttrib attrib) const;
    void setAttrib(WindowAttrib attrib, bool value);

    [[nodiscard]] bool isVisible() const { return isAttrib(WindowAttrib::Visible); }
    void setVisible(bool value) { setAttrib(WindowAttrib::Visible, value); }
    [[nodiscard]] bool isResizable() const { return isAttrib(WindowAttrib::Resizable); }
    void setResizable(bool value) { setAttrib(WindowAttrib::Resizable, value); }
    [[nodiscard]] bool isDecorated() const { return isAttrib(WindowAttrib::Decorated); }
    void setDecorated(bool value) { setAttrib(WindowAttrib::Decorated, value); }
    [[nodiscard]] bool isFocused() const { return isAttrib(WindowAttrib::Focused); }
    [[nodiscard]] bool isIconified() const { return isAttrib(WindowAttrib::Iconified); }
    [[nodiscard]] bool isMaximized() const { return isAttrib(WindowAttrib::Maximized); }
    [[nodiscard]] bool isHovered() const { return isAttrib(WindowAttrib::Hovered); }
    [[nodiscard]] bool isFloating() const { return isAttrib(WindowAttrib::Floating); }
    void setFloating(bool value) { setAttrib(WindowAttrib::Floating, value); }
    [[nodiscard]] bool isAutoIconify() const { return isAttrib(WindowAttrib::AutoIconify); }
    void setAutoIconify(bool value) { setAttrib(WindowAttrib::AutoIconify, value); }
    [[nodiscard]] bool isFocusOnShow() const { return isAttrib(WindowAttrib::FocusOnShow); }
    void setFocusOnShow(bool value) { setAttrib(WindowAttrib::FocusOnShow, value); }
    [[nodiscard]] bool isMousePassthrough() const { return isAttrib(WindowAttrib::MousePassthrough); }
    void setMousePassthrough(bool value) { setAttrib(WindowAttrib::MousePassthrough, value); }
    [[nodiscard]] bool isTransparentFramebuffer() const { return isAttrib(WindowAttrib::TransparentFramebuffer); }
    [[nodiscard]] bool isScaleToMonitor() const { return isAttrib(WindowAttrib::ScaleToMonitor); }
    void setScaleToMonitor(bool value) { setAttrib(WindowAttrib::ScaleToMonitor, value); }
    [[nodiscard]] bool isDoubleBuffered() const { return isAttrib(WindowAttrib::Doublebuffer); }

    [[nodiscard]] f32 getOpacity() const;
    void setOpacity(f32 opacity);

    void setSizeLimits(ivec2 minimum, ivec2 maximum);
    void setAspectRatio(i32 numerator, i32 denominator);
    void setIcon(std::span<const Image> images);
    [[nodiscard]] std::vector<Image> getIcon() const;

    void setMonitor(const Monitor& monitor, ivec2 position, ivec2 size, i32 refreshRate);
    void setMonitor(const Monitor& monitor);
    [[nodiscard]] std::optional<Monitor> getMonitor() const;

    void setUserPointer(void* pointer);
    [[nodiscard]] void* getUserPointer() const;

    // Eingabe
    void setInputMode(InputMode mode, i32 value);
    void setCursorMode(CursorMode mode) { setInputMode(InputMode::Cursor, static_cast<i32>(mode)); }
    [[nodiscard]] i32 getInputMode(InputMode mode) const;
    [[nodiscard]] CursorMode getCursorMode() const {
        return static_cast<CursorMode>(getInputMode(InputMode::Cursor));
    }
    [[nodiscard]] bool isRawMouseMotionSupported() const;
    [[nodiscard]] std::string getKeyName(Key key, i32 scancode) const;
    [[nodiscard]] i32 getKeyScancode(Key key) const;
    [[nodiscard]] bool isKeyPressed(Key key) const;
    [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const;
    [[nodiscard]] vec2 getCursorPos() const;
    void setCursorPos(vec2 position);
    void setCursor(Cursor* cursor);
    void setCursor(Cursor& cursor) { setCursor(&cursor); }

    [[nodiscard]] std::string getClipboardString() const;
    void setClipboardString(std::string_view text);

    // Callbacks
    void setCloseCallback(std::function<void()> callback);
    void setSizeCallback(std::function<void(ivec2)> callback);
    void setFramebufferSizeCallback(std::function<void(ivec2)> callback);
    void setContentScaleCallback(std::function<void(vec2)> callback);
    void setPositionCallback(std::function<void(ivec2)> callback);
    void setFocusCallback(std::function<void(bool)> callback);
    void setIconifyCallback(std::function<void(bool)> callback);
    void setMaximizeCallback(std::function<void(bool)> callback);
    void setRefreshCallback(std::function<void()> callback);
    void setKeyCallback(std::function<void(Key, i32, KeyAction, Modifiers)> callback);
    void setCharCallback(std::function<void(u32)> callback);
    void setCharModsCallback(std::function<void(u32, Modifiers)> callback);
    void setMouseButtonCallback(std::function<void(MouseButton, KeyAction, Modifiers)> callback);
    void setCursorPosCallback(std::function<void(vec2)> callback);
    void setCursorEnterCallback(std::function<void(bool)> callback);
    void setScrollCallback(std::function<void(vec2)> callback);
    void setDropCallback(std::function<void(std::span<const std::string>)> callback);

    [[nodiscard]] NativeHandle getNativeHandle() const;

    // detail::WindowEvents
    void onClose() override;
    void onSize(ivec2 size) override;
    void onFramebufferSize(ivec2 size) override;
    void onContentScale(vec2 scale) override;
    void onPosition(ivec2 position) override;
    void onFocus(bool focused) override;
    void onIconify(bool iconified) override;
    void onMaximize(bool maximized) override;
    void onRefresh() override;
    void onKey(Key key, i32 scancode, KeyAction action, Modifiers mods) override;
    void onChar(u32 codepoint) override;
    void onCharMods(u32 codepoint, Modifiers mods) override;
    void onMouseButton(MouseButton button, KeyAction action, Modifiers mods) override;
    void onCursorPos(vec2 position) override;
    void onCursorEnter(bool entered) override;
    void onScroll(vec2 offset) override;
    void onDrop(std::vector<std::string> paths) override;

private:
    friend class detail::WindowBackend;
    explicit Window(std::unique_ptr<detail::WindowBackend> backend);

    std::unique_ptr<detail::WindowBackend> m_backend;
    std::string m_title;
    void* m_userPointer = nullptr;

    std::function<void()> m_closeCallback;
    std::function<void(ivec2)> m_sizeCallback;
    std::function<void(ivec2)> m_framebufferSizeCallback;
    std::function<void(vec2)> m_contentScaleCallback;
    std::function<void(ivec2)> m_positionCallback;
    std::function<void(bool)> m_focusCallback;
    std::function<void(bool)> m_iconifyCallback;
    std::function<void(bool)> m_maximizeCallback;
    std::function<void()> m_refreshCallback;
    std::function<void(Key, i32, KeyAction, Modifiers)> m_keyCallback;
    std::function<void(u32)> m_charCallback;
    std::function<void(u32, Modifiers)> m_charModsCallback;
    std::function<void(MouseButton, KeyAction, Modifiers)> m_mouseButtonCallback;
    std::function<void(vec2)> m_cursorPosCallback;
    std::function<void(bool)> m_cursorEnterCallback;
    std::function<void(vec2)> m_scrollCallback;
    std::function<void(std::span<const std::string>)> m_dropCallback;
};

} // namespace sgl
