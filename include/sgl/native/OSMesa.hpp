#pragma once

#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::osmesa {

void* GetContext(const Window& window);
void* GetColorBuffer(const Window& window);
void* GetDepthBuffer(const Window& window);

} // namespace sgl::native::osmesa
