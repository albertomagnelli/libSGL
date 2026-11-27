#pragma once

#include "../Native.hpp"
#include "../Window.hpp"

namespace sgl::native::egl {

void* getDisplay(const Window& window);
void* getSurface(const Window& window);
void* getContext(const Window& window);

} // namespace sgl::native::egl
