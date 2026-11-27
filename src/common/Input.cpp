#include "sgl/Input.hpp"

#include "sgl/Context.hpp"
#include "sgl/Window.hpp"

namespace sgl {

std::string GetKeyName(Key key, i32 scancode) {
    if (Window* window = GetCurrentWindow()) {
        return window->getKeyName(key, scancode);
    }
    return {};
}

i32 GetKeyScancode(Key key) {
    if (Window* window = GetCurrentWindow()) {
        return window->getKeyScancode(key);
    }
    return -1;
}

bool IsRawMouseMotionSupported() {
    if (Window* window = GetCurrentWindow()) {
        return window->isRawMouseMotionSupported();
    }
    return false;
}

} // namespace sgl
