#pragma once

#include <string_view>

#include "Types.hpp"

namespace sgl {

enum class ClientApi : i32 {
    NoApi = 0,
    OpenGL = 0x00030001,
    OpenGLES = 0x00030002,
};

enum class ContextApi : i32 {
    Native = 0x00036001,
    EGL = 0x00036002,
    OSMesa = 0x00036003,
};

enum class ContextProfile : i32 {
    Any = 0,
    Core = 0x00032001,
    Compatibility = 0x00032002,
};

enum class ContextRobustness : i32 {
    None = 0,
    NoResetNotification = 0x00031001,
    LoseContextOnReset = 0x00031002,
};

enum class ContextReleaseBehavior : i32 {
    Any = 0,
    Flush = 0x00035001,
    None = 0x00035002,
};

void* GetProcAddress(std::string_view name);
bool IsExtensionSupported(std::string_view name);

class Window;
Window* GetCurrentWindow();

} // namespace sgl
