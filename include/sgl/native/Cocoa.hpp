#pragma once

#include "../Monitor.hpp"
#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::cocoa {

void* getNSWindow(const Window& window);
void* getNSGLContext(const Window& window);
void* getNSScreen(const Monitor& monitor);

} // namespace sgl::native::cocoa
