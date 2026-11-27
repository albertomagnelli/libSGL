#pragma once

#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::osmesa {

void* getContext(const Window& window);
void* getColorBuffer(const Window& window);
void* getDepthBuffer(const Window& window);

} // namespace sgl::native::osmesa
