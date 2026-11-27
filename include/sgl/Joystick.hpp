#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Input.hpp"
#include "Types.hpp"

namespace sgl {

bool isJoystickPresent(i32 jid);
std::string getJoystickName(i32 jid);
std::string getJoystickGUID(i32 jid);
std::span<const f32> getJoystickAxes(i32 jid);
std::span<const u8> getJoystickButtons(i32 jid);
std::span<const JoystickHat> getJoystickHats(i32 jid);

void setJoystickUserPointer(i32 jid, void* pointer);
void* getJoystickUserPointer(i32 jid);

bool isGamepad(i32 jid);
std::string getGamepadName(i32 jid);
std::optional<GamepadState> getGamepadState(i32 jid);
bool updateGamepadMappings(std::string_view mappings);

void setJoystickCallback(std::function<void(i32, bool)> callback);

} // namespace sgl
