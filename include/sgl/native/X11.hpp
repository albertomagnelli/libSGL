#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::x11 {

void* GetDisplay(const Window& window);
unsigned long GetWindow(const Window& window);
void* GetGlxContext(const Window& window);
unsigned long GetGlxWindow(const Window& window);

void* GetDisplay(const Monitor& monitor);
unsigned long GetMonitor(const Monitor& monitor);

} // namespace sgl::native::x11
