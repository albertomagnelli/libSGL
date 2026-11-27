#pragma once

#include <expected>
#include <memory_resource>
#include <string>

#include "Error.hpp"
#include "Types.hpp"

namespace sgl {

enum class Platform : i32 {
    Any = 0x00060000,
    Win32 = 0x00060001,
    Cocoa = 0x00060002,
    Wayland = 0x00060003,
    X11 = 0x00060004,
    Null = 0x00060005,
};

enum class InitHint : i32 {
    JoystickHatButtons = 0x00050001,
    AnglePlatformType = 0x00050002,
    Platform = 0x00050003,
    CocoaChdirResources = 0x00051001,
    CocoaMenubar = 0x00051002,
    WaylandLibdecor = 0x00053001,
};

enum class AnglePlatformType : i32 {
    None = 0x00037001,
    OpenGL = 0x00037002,
    OpenGLES = 0x00037003,
    D3D9 = 0x00037004,
    D3D11 = 0x00037005,
    Vulkan = 0x00037007,
    Metal = 0x00037008,
};

struct InitOptions {
    std::pmr::memory_resource* allocator = nullptr;
};

struct Version {
    i32 major = 0;
    i32 minor = 0;
    i32 revision = 0;
};

std::expected<void, Error> Init(const InitOptions& options = {});
void Terminate();

void SetInitHint(InitHint hint, i64 value);
void SetInitHint(InitHint hint, bool value);
inline void SetInitHint(InitHint hint, int value) { SetInitHint(hint, static_cast<i64>(value)); }

Version GetVersion();
std::string GetVersionString();
Platform GetPlatform();
bool IsPlatformSupported(Platform platform);

} // namespace sgl
