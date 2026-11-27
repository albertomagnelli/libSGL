#include "sgl/native/Cocoa.hpp"
#include "sgl/native/EGL.hpp"
#include "sgl/native/OSMesa.hpp"
#include "sgl/native/Wayland.hpp"
#include "sgl/native/Win32.hpp"
#include "sgl/native/X11.hpp"

#include <cstdint>

namespace sgl::native {

namespace {

void* PointerToWindowHandle(void* value) {
    return value;
}

} // namespace

namespace x11 {

void* GetDisplay(const Window& window) {
    return window.getNativeHandle().display;
}

unsigned long GetWindow(const Window& window) {
    return static_cast<unsigned long>(
        reinterpret_cast<uintptr_t>(window.getNativeHandle().window));
}

void* GetGlxContext(const Window& window) {
    return window.getNativeHandle().context;
}

unsigned long GetGlxWindow(const Window& window) {
    return static_cast<unsigned long>(
        reinterpret_cast<uintptr_t>(window.getNativeHandle().surface));
}

void* GetDisplay(const Monitor& monitor) {
    return monitor.getNativeHandle().display;
}

unsigned long GetMonitor(const Monitor& monitor) {
    return static_cast<unsigned long>(
        reinterpret_cast<uintptr_t>(monitor.getNativeHandle().window));
}

} // namespace x11

namespace wayland {

void* GetDisplay(const Window& window) {
    return window.getNativeHandle().display;
}

void* GetSurface(const Window& window) {
    return window.getNativeHandle().window;
}

void* GetEglDisplay(const Window& window) {
    return window.getNativeHandle().eglDisplay;
}

void* GetEglSurface(const Window& window) {
    return window.getNativeHandle().surface;
}

void* GetEglContext(const Window& window) {
    return window.getNativeHandle().context;
}

void* GetDisplay(const Monitor& monitor) {
    return monitor.getNativeHandle().display;
}

void* GetMonitor(const Monitor& monitor) {
    return monitor.getNativeHandle().window;
}

} // namespace wayland

namespace win32 {

void* GetHwnd(const Window& window) {
    return PointerToWindowHandle(window.getNativeHandle().window);
}

void* GetHdc(const Window& window) {
    return window.getNativeHandle().surface;
}

void* GetHglrc(const Window& window) {
    return window.getNativeHandle().context;
}

void* GetMonitor(const Monitor& monitor) {
    return monitor.getNativeHandle().window;
}

} // namespace win32

namespace cocoa {

void* GetNsWindow(const Window& window) {
    return window.getNativeHandle().window;
}

void* GetNsglContext(const Window& window) {
    return window.getNativeHandle().context;
}

void* GetNsScreen(const Monitor& monitor) {
    return monitor.getNativeHandle().window;
}

} // namespace cocoa

namespace egl {

void* GetDisplay(const Window& window) {
    return window.getNativeHandle().eglDisplay;
}

void* GetSurface(const Window& window) {
    return window.getNativeHandle().surface;
}

void* GetContext(const Window& window) {
    return window.getNativeHandle().context;
}

} // namespace egl

namespace osmesa {

void* GetContext(const Window& window) {
    return window.getNativeHandle().context;
}

void* GetColorBuffer(const Window& window) {
    return window.getNativeHandle().window;
}

void* GetDepthBuffer(const Window&) {
    return nullptr;
}

} // namespace osmesa

} // namespace sgl::native
