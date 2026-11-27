#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Input.hpp"
#include "Types.hpp"

namespace sgl {

bool IsJoystickPresent(i32 jid);
std::string GetJoystickName(i32 jid);
std::string GetJoystickGuid(i32 jid);
std::span<const f32> GetJoystickAxes(i32 jid);
std::span<const u8> GetJoystickButtons(i32 jid);
std::span<const JoystickHat> GetJoystickHats(i32 jid);

void SetJoystickUserPointer(i32 jid, void* pointer);
void* GetJoystickUserPointer(i32 jid);

bool IsGamepad(i32 jid);
std::string GetGamepadName(i32 jid);
std::optional<GamepadState> GetGamepadState(i32 jid);
/// Fügt ein Mapping im SDL-GameControllerDB-Format hinzu.
bool UpdateGamepadMappings(std::string_view mappings);

void SetJoystickCallback(std::function<void(i32, bool)> callback);

} // namespace sgl
