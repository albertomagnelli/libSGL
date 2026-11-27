#pragma once

#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::egl {

void* GetDisplay(const Window& window);
void* GetSurface(const Window& window);
void* GetContext(const Window& window);

} // namespace sgl::native::egl
