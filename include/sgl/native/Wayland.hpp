#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::wayland {

void* GetDisplay(const Window& window);
void* GetSurface(const Window& window);
void* GetEglDisplay(const Window& window);
void* GetEglSurface(const Window& window);
void* GetEglContext(const Window& window);

void* GetDisplay(const Monitor& monitor);
void* GetMonitor(const Monitor& monitor);

} // namespace sgl::native::wayland
