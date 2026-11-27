#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sgl/Input.hpp"
#include "sgl/Types.hpp"

namespace sgl::detail {

extern const char* const kGameControllerDbLines[];

enum class MappedInputType : u8 {
    None,
    Button,
    Axis,
    HatBit,
};

struct MappedInput {
    MappedInputType type = MappedInputType::None;
    i32 index = -1;
    f32 axisScale = 1.0f;
    f32 axisOffset = 0.0f;

    [[nodiscard]] bool isNone() const { return type == MappedInputType::None; }
};

struct GamepadMapping {
    std::string guid;
    std::string name;
    std::array<MappedInput, static_cast<std::size_t>(GamepadButton::Count)> buttons{};
    std::array<MappedInput, static_cast<std::size_t>(GamepadAxis::Count)> axes{};
};

struct GamepadMappingEntry {
    GamepadMapping mapping;
    std::string platform;
};

/// Parst eine einzelne Zeile im SDL-GameControllerDB-Format.
bool ParseGamepadMapping(std::string_view line, GamepadMapping& mapping, std::string& platform);

/// Datenbank der eingebetteten SDL-GameControllerDB plus Laufzeit-Updates.
class GamepadMappingDatabase {
public:
    static GamepadMappingDatabase& Instance();

    /// Fügt eine oder mehrere Mapping-Zeilen hinzu. Neuere Einträge haben Vorrang.
    bool addMappings(std::string_view text);

    /// Sucht ein Mapping anhand GUID und Plattformname ("Linux", "Windows",
    /// "Mac OS X"). Ein leerer Plattformname im Eintrag passt auf alle.
    [[nodiscard]] std::optional<GamepadMapping> find(std::string_view guid,
                                                     std::string_view platform) const;

private:
    GamepadMappingDatabase() = default;
    void ensureLoaded() const;

    mutable bool m_loaded = false;
    mutable std::vector<GamepadMappingEntry> m_entries;
};

/// Wendet ein Mapping auf Rohdaten an. Die Y-Konvention ist negativ = oben.
[[nodiscard]] GamepadState ApplyGamepadMapping(const GamepadMapping& mapping,
                                               std::span<const f32> axes,
                                               std::span<const u8> buttons,
                                               std::span<const JoystickHat> hats);

/// Normalisiert eine GUID auf 32 Kleinbuchstaben.
[[nodiscard]] std::string NormalizeGamepadGuid(std::string_view guid);

/// Erzeugt eine SDL-kompatible Linux-GUID aus evdev-IDs.
[[nodiscard]] std::string MakeLinuxGamepadGuid(u16 bus, u16 vendor, u16 product, u16 version,
                                               std::string_view name);

/// Erzeugt eine SDL-kompatible macOS-GUID aus IOKit-IDs.
[[nodiscard]] std::string MakeCocoaGamepadGuid(u64 vendor, u64 product, u64 version,
                                               std::string_view name);

} // namespace sgl::detail
