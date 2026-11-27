#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "Init.hpp"
#include "Native.hpp"
#include "Types.hpp"

namespace sgl {

namespace detail {
class Backend;
struct MonitorAccess;
}

struct VideoMode {
    ivec2 size{0, 0};
    i32 redBits = 8;
    i32 greenBits = 8;
    i32 blueBits = 8;
    i32 refreshRate = 0;
};

struct GammaRamp {
    std::array<u16, 256> red{};
    std::array<u16, 256> green{};
    std::array<u16, 256> blue{};
};

class Monitor {
public:
    struct Workarea {
        ivec2 position{0, 0};
        ivec2 size{0, 0};
    };

    Monitor() = default;

    [[nodiscard]] bool isValid() const { return m_native != nullptr; }
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] ivec2 getPosition() const;
    [[nodiscard]] Workarea getWorkarea() const;
    [[nodiscard]] ivec2 getPhysicalSize() const;
    [[nodiscard]] vec2 getContentScale() const;
    [[nodiscard]] std::string getName() const;
    [[nodiscard]] std::vector<VideoMode> getVideoModes() const;
    [[nodiscard]] VideoMode getVideoMode() const;
    void setGamma(f32 gamma) const;
    [[nodiscard]] GammaRamp getGammaRamp() const;
    void setGammaRamp(const GammaRamp& ramp) const;

    void setUserPointer(void* pointer) { m_userPointer = pointer; }
    [[nodiscard]] void* getUserPointer() const { return m_userPointer; }

    [[nodiscard]] NativeHandle getNativeHandle() const;

    friend bool operator==(const Monitor& a, const Monitor& b) {
        return a.m_native == b.m_native && a.m_platform == b.m_platform;
    }
    friend bool operator!=(const Monitor& a, const Monitor& b) { return !(a == b); }

private:
    friend class detail::Backend;
    friend class Window;
    friend struct detail::MonitorAccess;

    void* m_native = nullptr;
    Platform m_platform = Platform::Any;
    void* m_userPointer = nullptr;
};

using MonitorCallback = std::function<void(Monitor monitor, bool connected)>;

std::vector<Monitor> getMonitors();
Monitor getPrimaryMonitor();
void setMonitorCallback(MonitorCallback callback);

} // namespace sgl
