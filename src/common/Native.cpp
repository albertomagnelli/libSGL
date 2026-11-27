#include "sgl/native/Cocoa.hpp"
#include "sgl/native/EGL.hpp"
#include "sgl/native/OSMesa.hpp"
#include "sgl/native/Wayland.hpp"
#include "sgl/native/Win32.hpp"
#include "sgl/native/X11.hpp"

#include <cstdint>

namespace sgl::native {

namespace {

void* pointerToWindowHandle(void* value) {
    return value;
}

} // namespace

namespace x11 {

void* getDisplay(const Window& window) {
    return window.getNativeHandle().display;
}

unsigned long getWindow(const Window& window) {
    return static_cast<unsigned long>(
        reinterpret_cast<uintptr_t>(window.getNativeHandle().window));
}

void* getGLXContext(const Window& window) {
    return window.getNativeHandle().context;
}

unsigned long getGLXWindow(const Window& window) {
    return static_cast<unsigned long>(
        reinterpret_cast<uintptr_t>(window.getNativeHandle().surface));
}

void* getDisplay(const Monitor& monitor) {
    return monitor.getNativeHandle().display;
}

unsigned long getMonitor(const Monitor& monitor) {
    return static_cast<unsigned long>(
        reinterpret_cast<uintptr_t>(monitor.getNativeHandle().window));
}

} // namespace x11

namespace wayland {

void* getDisplay(const Window& window) {
    return window.getNativeHandle().display;
}

void* getSurface(const Window& window) {
    return window.getNativeHandle().window;
}

void* getEGLDisplay(const Window& window) {
    return window.getNativeHandle().eglDisplay;
}

void* getEGLSurface(const Window& window) {
    return window.getNativeHandle().surface;
}

void* getEGLContext(const Window& window) {
    return window.getNativeHandle().context;
}

void* getDisplay(const Monitor& monitor) {
    return monitor.getNativeHandle().display;
}

void* getMonitor(const Monitor& monitor) {
    return monitor.getNativeHandle().window;
}

} // namespace wayland

namespace win32 {

void* getHWND(const Window& window) {
    return pointerToWindowHandle(window.getNativeHandle().window);
}

void* getHDC(const Window& window) {
    return window.getNativeHandle().surface;
}

void* getHGLRC(const Window& window) {
    return window.getNativeHandle().context;
}

void* getMonitor(const Monitor& monitor) {
    return monitor.getNativeHandle().window;
}

} // namespace win32

namespace cocoa {

void* getNSWindow(const Window& window) {
    return window.getNativeHandle().window;
}

void* getNSGLContext(const Window& window) {
    return window.getNativeHandle().context;
}

void* getNSScreen(const Monitor& monitor) {
    return monitor.getNativeHandle().window;
}

} // namespace cocoa

namespace egl {

void* getDisplay(const Window& window) {
    return window.getNativeHandle().eglDisplay;
}

void* getSurface(const Window& window) {
    return window.getNativeHandle().surface;
}

void* getContext(const Window& window) {
    return window.getNativeHandle().context;
}

} // namespace egl

namespace osmesa {

void* getContext(const Window& window) {
    return window.getNativeHandle().context;
}

void* getColorBuffer(const Window& window) {
    return window.getNativeHandle().window;
}

void* getDepthBuffer(const Window&) {
    return nullptr;
}

} // namespace osmesa

} // namespace sgl::native
