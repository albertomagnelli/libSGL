#include "sgl/Context.hpp"

#include <string>

#include "backend/Backend.hpp"

namespace sgl {

void* getProcAddress(std::string_view name) {
    Window* window = detail::getCurrentWindow();
    if (!window) {
        reportError(ErrorCode::NoCurrentContext, "Kein aktueller OpenGL-Kontext.");
        return nullptr;
    }
    return window->getProcAddress(name);
}

bool isExtensionSupported(std::string_view name) {
    Window* window = detail::getCurrentWindow();
    if (!window) {
        reportError(ErrorCode::NoCurrentContext, "Kein aktueller OpenGL-Kontext.");
        return false;
    }
    return window->isExtensionSupported(name);
}

Window* getCurrentWindow() {
    return detail::getCurrentWindow();
}

} // namespace sgl
