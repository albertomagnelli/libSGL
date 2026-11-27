#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sgl/Context.hpp"
#include "sgl/Cursor.hpp"
#include "sgl/Error.hpp"
#include "sgl/Init.hpp"
#include "sgl/Input.hpp"
#include "sgl/Joystick.hpp"
#include "sgl/Monitor.hpp"
#include "sgl/Native.hpp"
#include "sgl/Types.hpp"
#include "sgl/Window.hpp"

namespace sgl::detail {

class CursorBackend {
public:
    virtual ~CursorBackend() = default;
    [[nodiscard]] virtual void* getNativeHandle() const = 0;
};

class WindowBackend {
public:
    virtual ~WindowBackend() = default;

    virtual void setEvents(WindowEvents* events) = 0;
    virtual void makeContextCurrent() = 0;
    virtual void swapBuffers() = 0;
    virtual void* getProcAddress(const char* name) = 0;
    [[nodiscard]] virtual NativeHandle getNativeHandle() const = 0;
    [[nodiscard]] virtual bool isCloseRequested() const = 0;
    virtual void setCloseRequested(bool requested) = 0;
    [[nodiscard]] virtual ivec2 getSize() const = 0;
    [[nodiscard]] virtual ivec2 getFramebufferSize() const = 0;

    virtual void setSwapInterval(i32) {}
    [[nodiscard]] virtual i32 getSwapInterval() const { return 0; }
    [[nodiscard]] virtual bool isExtensionSupported(const char*) { return false; }
    [[nodiscard]] virtual std::string getTitle() const { return {}; }
    virtual void setTitle(std::string_view) {}
    virtual void setSize(ivec2) {}
    virtual void setPosition(ivec2) {}
    [[nodiscard]] virtual ivec2 getPosition() const { return {0, 0}; }
    [[nodiscard]] virtual ivec4 getFrameSize() const { return {0, 0, 0, 0}; }
    [[nodiscard]] virtual vec2 getContentScale() const { return {1.0f, 1.0f}; }
    virtual void show() {}
    virtual void hide() {}
    virtual void focus() {}
    virtual void requestAttention() {}
    virtual void iconify() {}
    virtual void restore() {}
    virtual void maximize() {}
    [[nodiscard]] virtual i32 getAttrib(WindowAttrib) const { return 0; }
    virtual void setAttrib(WindowAttrib, bool) {}
    [[nodiscard]] virtual f32 getOpacity() const { return 1.0f; }
    virtual void setOpacity(f32) {}
    virtual void setSizeLimits(ivec2, ivec2) {}
    virtual void setAspectRatio(i32, i32) {}
    virtual void setIcon(std::span<const WindowIcon>) {}
    [[nodiscard]] virtual std::vector<WindowIcon> getIcon() const { return {}; }
    virtual void setMonitor(const Monitor&, ivec2, ivec2, i32) {}
    [[nodiscard]] virtual Monitor getMonitor() const { return {}; }
    virtual void setInputMode(InputMode, i32) {}
    [[nodiscard]] virtual i32 getInputMode(InputMode) const {
        return static_cast<i32>(CursorMode::Normal);
    }
    [[nodiscard]] virtual bool isRawMouseMotionSupported() const { return false; }
    [[nodiscard]] virtual std::string getKeyName(Key, i32) const { return {}; }
    [[nodiscard]] virtual i32 getKeyScancode(Key) const { return -1; }
    [[nodiscard]] virtual bool isKeyPressed(Key) const { return false; }
    [[nodiscard]] virtual bool isMouseButtonPressed(MouseButton) const { return false; }
    [[nodiscard]] virtual vec2 getCursorPos() const { return {0.0f, 0.0f}; }
    virtual void setCursorPos(vec2) {}
    virtual void setCursor(CursorBackend*) {}
    [[nodiscard]] virtual std::string getClipboardString() const { return {}; }
    virtual void setClipboardString(std::string_view) {}
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual bool init(const InitOptions& options, Error& error) = 0;
    virtual void terminate() = 0;
    [[nodiscard]] virtual Platform getPlatform() const = 0;
    virtual std::unique_ptr<WindowBackend> createWindow(const WindowHints& hints, Error& error) = 0;

    virtual std::unique_ptr<CursorBackend> createCursor(const WindowIcon&, ivec2, Error& error) {
        error = MakeError(ErrorCode::FeatureUnavailable, "Cursor wird von diesem Backend nicht unterstuetzt.");
        return nullptr;
    }
    virtual std::unique_ptr<CursorBackend> createStandardCursor(StandardCursor, Error& error) {
        error = MakeError(ErrorCode::FeatureUnavailable, "Standard-Cursor wird von diesem Backend nicht unterstuetzt.");
        return nullptr;
    }

    [[nodiscard]] virtual std::vector<Monitor> getMonitors() { return {}; }
    [[nodiscard]] virtual Monitor getPrimaryMonitor() { return {}; }
    virtual void pollEvents() = 0;
    virtual void waitEvents(f64) = 0;
    virtual void postEmptyEvent() = 0;

    virtual void setMonitorCallback(std::function<void(Monitor, bool)>) {}
    [[nodiscard]] virtual bool isMonitorConnected(const Monitor&) { return false; }
    [[nodiscard]] virtual ivec2 getMonitorPosition(const Monitor&) { return {0, 0}; }
    [[nodiscard]] virtual Monitor::Workarea getMonitorWorkarea(const Monitor&) { return {}; }
    [[nodiscard]] virtual ivec2 getMonitorPhysicalSize(const Monitor&) { return {0, 0}; }
    [[nodiscard]] virtual vec2 getMonitorContentScale(const Monitor&) { return {1.0f, 1.0f}; }
    [[nodiscard]] virtual std::string getMonitorName(const Monitor&) { return {}; }
    [[nodiscard]] virtual std::vector<VideoMode> getVideoModes(const Monitor&) { return {}; }
    [[nodiscard]] virtual VideoMode getVideoMode(const Monitor&) { return {}; }
    virtual void setGamma(const Monitor&, f32) {}
    [[nodiscard]] virtual GammaRamp getGammaRamp(const Monitor&) { return {}; }
    virtual void setGammaRamp(const Monitor&, const GammaRamp&) {}
    [[nodiscard]] virtual NativeHandle getMonitorNativeHandle(const Monitor&) { return {}; }

    [[nodiscard]] virtual bool isJoystickPresent(i32) { return false; }
    [[nodiscard]] virtual std::string getJoystickName(i32) { return {}; }
    [[nodiscard]] virtual std::string getJoystickGUID(i32) { return {}; }
    [[nodiscard]] virtual std::span<const f32> getJoystickAxes(i32) { return {}; }
    [[nodiscard]] virtual std::span<const u8> getJoystickButtons(i32) { return {}; }
    [[nodiscard]] virtual std::span<const JoystickHat> getJoystickHats(i32) { return {}; }
    virtual void setJoystickUserPointer(i32, void*) {}
    [[nodiscard]] virtual void* getJoystickUserPointer(i32) { return nullptr; }
    [[nodiscard]] virtual bool isGamepad(i32) { return false; }
    [[nodiscard]] virtual std::string getGamepadName(i32) { return {}; }
    [[nodiscard]] virtual std::optional<GamepadState> getGamepadState(i32) { return std::nullopt; }
    virtual bool updateGamepadMappings(std::string_view) { return false; }
    virtual void setJoystickCallback(std::function<void(i32, bool)>) {}
};

Backend* GetBackend();
void SetBackend(std::unique_ptr<Backend> backend);
Window* GetCurrentWindow();
void SetCurrentWindow(Window* window);
std::unique_ptr<Backend> CreatePlatformBackend(Platform platform, Error& error);
bool InitPlatform(const InitOptions& options, Error& error);
void TerminatePlatform();

} // namespace sgl::detail
