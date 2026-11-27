#include "sgl/Joystick.hpp"

#include "backend/Backend.hpp"

namespace sgl {
namespace {

detail::Backend* requireBackend() {
    auto* backend = detail::getBackend();
    if (!backend) {
        reportError(ErrorCode::NotInitialized, "sgl ist nicht initialisiert.");
    }
    return backend;
}

} // namespace

bool isJoystickPresent(i32 jid) {
    auto* backend = requireBackend();
    return backend && backend->isJoystickPresent(jid);
}

std::string getJoystickName(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getJoystickName(jid) : std::string{};
}

std::string getJoystickGUID(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getJoystickGUID(jid) : std::string{};
}

std::span<const f32> getJoystickAxes(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getJoystickAxes(jid) : std::span<const f32>{};
}

std::span<const u8> getJoystickButtons(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getJoystickButtons(jid) : std::span<const u8>{};
}

std::span<const JoystickHat> getJoystickHats(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getJoystickHats(jid) : std::span<const JoystickHat>{};
}

void setJoystickUserPointer(i32 jid, void* pointer) {
    if (auto* backend = requireBackend()) {
        backend->setJoystickUserPointer(jid, pointer);
    }
}

void* getJoystickUserPointer(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getJoystickUserPointer(jid) : nullptr;
}

bool isGamepad(i32 jid) {
    auto* backend = requireBackend();
    return backend && backend->isGamepad(jid);
}

std::string getGamepadName(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getGamepadName(jid) : std::string{};
}

std::optional<GamepadState> getGamepadState(i32 jid) {
    auto* backend = requireBackend();
    return backend ? backend->getGamepadState(jid) : std::nullopt;
}

bool updateGamepadMappings(std::string_view mappings) {
    auto* backend = requireBackend();
    return backend && backend->updateGamepadMappings(mappings);
}

void setJoystickCallback(std::function<void(i32, bool)> callback) {
    if (auto* backend = requireBackend()) {
        backend->setJoystickCallback(std::move(callback));
    }
}

} // namespace sgl
