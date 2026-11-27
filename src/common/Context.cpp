#include "sgl/Context.hpp"

#include <string>

#include "backend/Backend.hpp"

namespace sgl {

void* GetProcAddress(std::string_view name) {
    Window* window = detail::GetCurrentWindow();
    if (!window) {
        ReportError(ErrorCode::NoCurrentContext, "Kein aktueller OpenGL-Kontext.");
        return nullptr;
    }
    return window->getProcAddress(name);
}

bool IsExtensionSupported(std::string_view name) {
    Window* window = detail::GetCurrentWindow();
    if (!window) {
        ReportError(ErrorCode::NoCurrentContext, "Kein aktueller OpenGL-Kontext.");
        return false;
    }
    return window->isExtensionSupported(name);
}

Window* GetCurrentWindow() {
    return detail::GetCurrentWindow();
}

} // namespace sgl
