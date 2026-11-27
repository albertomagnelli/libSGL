#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::cocoa {

void* GetNsWindow(const Window& window);
void* GetNsglContext(const Window& window);
void* GetNsScreen(const Monitor& monitor);

} // namespace sgl::native::cocoa
