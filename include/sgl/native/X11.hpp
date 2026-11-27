#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::x11 {

void* getDisplay(const Window& window);
unsigned long getWindow(const Window& window);
void* getGLXContext(const Window& window);
unsigned long getGLXWindow(const Window& window);

void* getDisplay(const Monitor& monitor);
unsigned long getMonitor(const Monitor& monitor);

} // namespace sgl::native::x11
