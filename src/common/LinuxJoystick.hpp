#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "sgl/Input.hpp"
#include "sgl/Types.hpp"

namespace sgl::detail {

class LinuxJoysticks {
public:
    bool isPresent(i32 jid) const;
    std::string getName(i32 jid) const;
    std::string getGuid(i32 jid) const;
    std::span<const f32> getAxes(i32 jid) const;
    std::span<const u8> getButtons(i32 jid) const;
    std::span<const JoystickHat> getHats(i32 jid) const;

    void setUserPointer(i32 jid, void* pointer);
    void* getUserPointer(i32 jid) const;

    bool isGamepad(i32 jid) const;
    std::string getGamepadName(i32 jid) const;
    std::optional<GamepadState> getGamepadState(i32 jid) const;
    bool updateGamepadMappings(std::string_view mappings);

    void setCallback(std::function<void(i32, bool)> callback);
    void poll();

private:
    struct Impl;
    Impl& impl();
    const Impl& impl() const;
};

LinuxJoysticks& GetLinuxJoysticks();

} // namespace sgl::detail
