#include "sgl/Input.hpp"

#include "sgl/Context.hpp"
#include "sgl/Window.hpp"

namespace sgl {

std::string getKeyName(Key key, i32 scancode) {
    if (Window* window = getCurrentWindow()) {
        return window->getKeyName(key, scancode);
    }
    return {};
}

i32 getKeyScancode(Key key) {
    if (Window* window = getCurrentWindow()) {
        return window->getKeyScancode(key);
    }
    return -1;
}

bool isRawMouseMotionSupported() {
    if (Window* window = getCurrentWindow()) {
        return window->isRawMouseMotionSupported();
    }
    return false;
}

} // namespace sgl
