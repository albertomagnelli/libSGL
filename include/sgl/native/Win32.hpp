#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::win32 {

void* getHWND(const Window& window);
void* getHDC(const Window& window);
void* getHGLRC(const Window& window);
void* getMonitor(const Monitor& monitor);

} // namespace sgl::native::win32
