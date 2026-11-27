#include "sgl/Joystick.hpp"

#include "backend/Backend.hpp"

namespace sgl {
namespace {

detail::Backend* RequireBackend() {
    auto* backend = detail::GetBackend();
    if (!backend) {
        ReportError(ErrorCode::NotInitialized, "sgl ist nicht initialisiert.");
    }
    return backend;
}

} // namespace

bool IsJoystickPresent(i32 jid) {
    auto* backend = RequireBackend();
    return backend && backend->isJoystickPresent(jid);
}

std::string GetJoystickName(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getJoystickName(jid) : std::string{};
}

std::string GetJoystickGuid(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getJoystickGUID(jid) : std::string{};
}

std::span<const f32> GetJoystickAxes(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getJoystickAxes(jid) : std::span<const f32>{};
}

std::span<const u8> GetJoystickButtons(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getJoystickButtons(jid) : std::span<const u8>{};
}

std::span<const JoystickHat> GetJoystickHats(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getJoystickHats(jid) : std::span<const JoystickHat>{};
}

void SetJoystickUserPointer(i32 jid, void* pointer) {
    if (auto* backend = RequireBackend()) {
        backend->setJoystickUserPointer(jid, pointer);
    }
}

void* GetJoystickUserPointer(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getJoystickUserPointer(jid) : nullptr;
}

bool IsGamepad(i32 jid) {
    auto* backend = RequireBackend();
    return backend && backend->isGamepad(jid);
}

std::string GetGamepadName(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getGamepadName(jid) : std::string{};
}

std::optional<GamepadState> GetGamepadState(i32 jid) {
    auto* backend = RequireBackend();
    return backend ? backend->getGamepadState(jid) : std::nullopt;
}

bool UpdateGamepadMappings(std::string_view mappings) {
    auto* backend = RequireBackend();
    return backend && backend->updateGamepadMappings(mappings);
}

void SetJoystickCallback(std::function<void(i32, bool)> callback) {
    if (auto* backend = RequireBackend()) {
        backend->setJoystickCallback(std::move(callback));
    }
}

} // namespace sgl
