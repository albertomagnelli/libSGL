#include "sgl/Window.hpp"

#include <mutex>
#include <utility>

#include "backend/Backend.hpp"
#include "sgl/Font.hpp"

namespace sgl {
namespace {

std::once_flag g_defaultFontOnce;

} // namespace

WindowHints GetDefaultWindowHints() {
    return WindowHints{};
}

std::expected<std::unique_ptr<Window>, Error> Window::Create(const WindowHints& hints) {
    auto* backend = detail::GetBackend();
    if (!backend) {
        return std::unexpected(detail::MakeError(ErrorCode::NotInitialized,
                                                 "sgl ist nicht initialisiert."));
    }

    Error error;
    auto windowBackend = backend->createWindow(hints, error);
    if (!windowBackend) {
        if (!error.isError()) {
            error = detail::MakeError(ErrorCode::PlatformError, "Fenster konnte nicht erstellt werden.");
        }
        return std::unexpected(std::move(error));
    }

    auto window = std::unique_ptr<Window>(new Window(std::move(windowBackend)));
    window->m_title = hints.title;

    std::call_once(g_defaultFontOnce, [] {
        g_defaultFont = Font::LoadDefault();
    });

    return window;
}

Window::Window(std::unique_ptr<detail::WindowBackend> backend)
    : m_backend(std::move(backend)) {
    m_backend->setEvents(this);
}

Window::~Window() {
    if (detail::GetCurrentWindow() == this) {
        detail::SetCurrentWindow(nullptr);
    }
}

void Window::makeContextCurrent() {
    m_backend->makeContextCurrent();
    detail::SetCurrentWindow(this);
}

void Window::swapBuffers() {
    m_backend->swapBuffers();
}

void Window::setSwapInterval(i32 interval) {
    m_backend->setSwapInterval(interval);
}

i32 Window::getSwapInterval() const {
    return m_backend->getSwapInterval();
}

void* Window::getProcAddress(std::string_view name) const {
    const std::string owned(name);
    return m_backend->getProcAddress(owned.c_str());
}

bool Window::isExtensionSupported(std::string_view name) const {
    const std::string owned(name);
    return m_backend->isExtensionSupported(owned.c_str());
}

bool Window::isCloseRequested() const {
    return m_backend->isCloseRequested();
}

void Window::setCloseRequested(bool requested) {
    m_backend->setCloseRequested(requested);
}

std::string Window::getTitle() const {
    return m_backend->getTitle();
}

void Window::setTitle(std::string_view title) {
    m_title = std::string(title);
    m_backend->setTitle(title);
}

ivec2 Window::getSize() const {
    return m_backend->getSize();
}

void Window::setSize(ivec2 size) {
    m_backend->setSize(size);
}

ivec2 Window::getPosition() const {
    return m_backend->getPosition();
}

void Window::setPosition(ivec2 position) {
    m_backend->setPosition(position);
}

ivec2 Window::getFramebufferSize() const {
    return m_backend->getFramebufferSize();
}

ivec4 Window::getFrameSize() const {
    return m_backend->getFrameSize();
}

vec2 Window::getContentScale() const {
    return m_backend->getContentScale();
}

void Window::show() {
    m_backend->show();
}

void Window::hide() {
    m_backend->hide();
}

void Window::focus() {
    m_backend->focus();
}

void Window::requestAttention() {
    m_backend->requestAttention();
}

void Window::iconify() {
    m_backend->iconify();
}

void Window::restore() {
    m_backend->restore();
}

void Window::maximize() {
    m_backend->maximize();
}

bool Window::isAttrib(WindowAttrib attrib) const {
    return m_backend->getAttrib(attrib) != 0;
}

i32 Window::getAttrib(WindowAttrib attrib) const {
    return m_backend->getAttrib(attrib);
}

void Window::setAttrib(WindowAttrib attrib, bool value) {
    m_backend->setAttrib(attrib, value);
}

f32 Window::getOpacity() const {
    return m_backend->getOpacity();
}

void Window::setOpacity(f32 opacity) {
    m_backend->setOpacity(opacity);
}

void Window::setSizeLimits(ivec2 minimum, ivec2 maximum) {
    m_backend->setSizeLimits(minimum, maximum);
}

void Window::setAspectRatio(i32 numerator, i32 denominator) {
    m_backend->setAspectRatio(numerator, denominator);
}

void Window::setIcon(std::span<const WindowIcon> images) {
    m_backend->setIcon(images);
}

std::vector<WindowIcon> Window::getIcon() const {
    return m_backend->getIcon();
}

void Window::setMonitor(const Monitor& monitor, ivec2 position, ivec2 size, i32 refreshRate) {
    m_backend->setMonitor(monitor, position, size, refreshRate);
}

void Window::setMonitor(const Monitor& monitor) {
    m_backend->setMonitor(monitor, getPosition(), getSize(), 0);
}

std::optional<Monitor> Window::getMonitor() const {
    Monitor monitor = m_backend->getMonitor();
    if (!monitor.isValid()) {
        return std::nullopt;
    }
    return monitor;
}

void Window::setUserPointer(void* pointer) {
    m_userPointer = pointer;
}

void* Window::getUserPointer() const {
    return m_userPointer;
}

void Window::setInputMode(InputMode mode, i32 value) {
    m_backend->setInputMode(mode, value);
}

i32 Window::getInputMode(InputMode mode) const {
    return m_backend->getInputMode(mode);
}

bool Window::isRawMouseMotionSupported() const {
    return m_backend->isRawMouseMotionSupported();
}

std::string Window::getKeyName(Key key, i32 scancode) const {
    return m_backend->getKeyName(key, scancode);
}

i32 Window::getKeyScancode(Key key) const {
    return m_backend->getKeyScancode(key);
}

bool Window::isKeyPressed(Key key) const {
    return m_backend->isKeyPressed(key);
}

bool Window::isMouseButtonPressed(MouseButton button) const {
    return m_backend->isMouseButtonPressed(button);
}

vec2 Window::getCursorPos() const {
    return m_backend->getCursorPos();
}

void Window::setCursorPos(vec2 position) {
    m_backend->setCursorPos(position);
}

void Window::setCursor(Cursor* cursor) {
    m_backend->setCursor(cursor ? cursor->m_backend.get() : nullptr);
}

std::string Window::getClipboardString() const {
    return m_backend->getClipboardString();
}

void Window::setClipboardString(std::string_view text) {
    m_backend->setClipboardString(text);
}

void Window::setCloseCallback(std::function<void()> callback) {
    m_closeCallback = std::move(callback);
}

void Window::setSizeCallback(std::function<void(ivec2)> callback) {
    m_sizeCallback = std::move(callback);
}

void Window::setFramebufferSizeCallback(std::function<void(ivec2)> callback) {
    m_framebufferSizeCallback = std::move(callback);
}

void Window::setContentScaleCallback(std::function<void(vec2)> callback) {
    m_contentScaleCallback = std::move(callback);
}

void Window::setPositionCallback(std::function<void(ivec2)> callback) {
    m_positionCallback = std::move(callback);
}

void Window::setFocusCallback(std::function<void(bool)> callback) {
    m_focusCallback = std::move(callback);
}

void Window::setIconifyCallback(std::function<void(bool)> callback) {
    m_iconifyCallback = std::move(callback);
}

void Window::setMaximizeCallback(std::function<void(bool)> callback) {
    m_maximizeCallback = std::move(callback);
}

void Window::setRefreshCallback(std::function<void()> callback) {
    m_refreshCallback = std::move(callback);
}

void Window::setKeyCallback(std::function<void(Key, i32, KeyAction, Modifiers)> callback) {
    m_keyCallback = std::move(callback);
}

void Window::setCharCallback(std::function<void(u32)> callback) {
    m_charCallback = std::move(callback);
}

void Window::setCharModsCallback(std::function<void(u32, Modifiers)> callback) {
    m_charModsCallback = std::move(callback);
}

void Window::setMouseButtonCallback(std::function<void(MouseButton, KeyAction, Modifiers)> callback) {
    m_mouseButtonCallback = std::move(callback);
}

void Window::setCursorPosCallback(std::function<void(vec2)> callback) {
    m_cursorPosCallback = std::move(callback);
}

void Window::setCursorEnterCallback(std::function<void(bool)> callback) {
    m_cursorEnterCallback = std::move(callback);
}

void Window::setScrollCallback(std::function<void(vec2)> callback) {
    m_scrollCallback = std::move(callback);
}

void Window::setDropCallback(std::function<void(std::span<const std::string>)> callback) {
    m_dropCallback = std::move(callback);
}

NativeHandle Window::getNativeHandle() const {
    return m_backend->getNativeHandle();
}

void Window::onClose() {
    if (m_closeCallback) m_closeCallback();
}

void Window::onSize(ivec2 size) {
    if (m_sizeCallback) m_sizeCallback(size);
}

void Window::onFramebufferSize(ivec2 size) {
    if (m_framebufferSizeCallback) m_framebufferSizeCallback(size);
}

void Window::onContentScale(vec2 scale) {
    if (m_contentScaleCallback) m_contentScaleCallback(scale);
}

void Window::onPosition(ivec2 position) {
    if (m_positionCallback) m_positionCallback(position);
}

void Window::onFocus(bool focused) {
    if (m_focusCallback) m_focusCallback(focused);
}

void Window::onIconify(bool iconified) {
    if (m_iconifyCallback) m_iconifyCallback(iconified);
}

void Window::onMaximize(bool maximized) {
    if (m_maximizeCallback) m_maximizeCallback(maximized);
}

void Window::onRefresh() {
    if (m_refreshCallback) m_refreshCallback();
}

void Window::onKey(Key key, i32 scancode, KeyAction action, Modifiers mods) {
    if (m_keyCallback) m_keyCallback(key, scancode, action, mods);
}

void Window::onChar(u32 codepoint) {
    if (m_charCallback) m_charCallback(codepoint);
}

void Window::onCharMods(u32 codepoint, Modifiers mods) {
    if (m_charModsCallback) m_charModsCallback(codepoint, mods);
}

void Window::onMouseButton(MouseButton button, KeyAction action, Modifiers mods) {
    if (m_mouseButtonCallback) m_mouseButtonCallback(button, action, mods);
}

void Window::onCursorPos(vec2 position) {
    if (m_cursorPosCallback) m_cursorPosCallback(position);
}

void Window::onCursorEnter(bool entered) {
    if (m_cursorEnterCallback) m_cursorEnterCallback(entered);
}

void Window::onScroll(vec2 offset) {
    if (m_scrollCallback) m_scrollCallback(offset);
}

void Window::onDrop(std::vector<std::string> paths) {
    if (m_dropCallback) {
        m_dropCallback(std::span<const std::string>(paths));
    }
}

} // namespace sgl
