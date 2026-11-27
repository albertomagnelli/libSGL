#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::wayland {

void* getDisplay(const Window& window);
void* getSurface(const Window& window);
void* getEGLDisplay(const Window& window);
void* getEGLSurface(const Window& window);
void* getEGLContext(const Window& window);

void* getDisplay(const Monitor& monitor);
void* getMonitor(const Monitor& monitor);

} // namespace sgl::native::wayland
