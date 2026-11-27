#pragma once

#include "Types.hpp"

namespace sgl {

enum class NativePlatform : i32 {
    Unknown = 0,
    Win32,
    Cocoa,
    Wayland,
    X11,
    EGL,
    OSMesa,
    Null,
};

struct NativeHandle {
    NativePlatform platform = NativePlatform::Unknown;
    void* display = nullptr;
    void* window = nullptr;
    void* context = nullptr;
    void* surface = nullptr;
    void* eglDisplay = nullptr;
};

} // namespace sgl
