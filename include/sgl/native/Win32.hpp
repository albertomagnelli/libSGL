#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::win32 {

void* GetHwnd(const Window& window);
void* GetHdc(const Window& window);
void* GetHglrc(const Window& window);
void* GetMonitor(const Monitor& monitor);

} // namespace sgl::native::win32
