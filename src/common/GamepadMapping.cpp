#include "GamepadMapping.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>

namespace sgl::detail {
namespace {

struct Target {
    bool valid = false;
    bool isAxis = false;
    i32 index = -1;
};

Target targetForField(std::string_view field) {
    if (field == "a") return {true, false, static_cast<i32>(GamepadButton::A)};
    if (field == "b") return {true, false, static_cast<i32>(GamepadButton::B)};
    if (field == "x") return {true, false, static_cast<i32>(GamepadButton::X)};
    if (field == "y") return {true, false, static_cast<i32>(GamepadButton::Y)};
    if (field == "back") return {true, false, static_cast<i32>(GamepadButton::Back)};
    if (field == "start") return {true, false, static_cast<i32>(GamepadButton::Start)};
    if (field == "guide") return {true, false, static_cast<i32>(GamepadButton::Guide)};
    if (field == "leftshoulder") return {true, false, static_cast<i32>(GamepadButton::LeftBumper)};
    if (field == "rightshoulder") return {true, false, static_cast<i32>(GamepadButton::RightBumper)};
    if (field == "leftstick") return {true, false, static_cast<i32>(GamepadButton::LeftThumb)};
    if (field == "rightstick") return {true, false, static_cast<i32>(GamepadButton::RightThumb)};
    if (field == "dpup") return {true, false, static_cast<i32>(GamepadButton::DPadUp)};
    if (field == "dpdown") return {true, false, static_cast<i32>(GamepadButton::DPadDown)};
    if (field == "dpleft") return {true, false, static_cast<i32>(GamepadButton::DPadLeft)};
    if (field == "dpright") return {true, false, static_cast<i32>(GamepadButton::DPadRight)};

    if (field == "leftx") return {true, true, static_cast<i32>(GamepadAxis::LeftX)};
    if (field == "lefty") return {true, true, static_cast<i32>(GamepadAxis::LeftY)};
    if (field == "rightx") return {true, true, static_cast<i32>(GamepadAxis::RightX)};
    if (field == "righty") return {true, true, static_cast<i32>(GamepadAxis::RightY)};
    if (field == "lefttrigger") return {true, true, static_cast<i32>(GamepadAxis::LeftTrigger)};
    if (field == "righttrigger") return {true, true, static_cast<i32>(GamepadAxis::RightTrigger)};

    return {};
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

bool parseNumber(std::string_view& text, i32& value) {
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr == begin) {
        return false;
    }
    text.remove_prefix(static_cast<std::size_t>(result.ptr - begin));
    return true;
}

bool parseInput(std::string_view text, MappedInput& input) {
    text = trim(text);
    if (text.empty()) {
        return false;
    }

    i32 minimum = -1;
    i32 maximum = 1;
    if (text.front() == '+') {
        minimum = 0;
        text.remove_prefix(1);
    } else if (text.front() == '-') {
        maximum = 0;
        text.remove_prefix(1);
    }

    if (text.empty()) {
        return false;
    }

    const char kind = text.front();
    text.remove_prefix(1);

    if (kind == 'a') {
        i32 index = 0;
        if (!parseNumber(text, index)) {
            return false;
        }
        input.type = MappedInputType::Axis;
        input.index = index;
        input.axisScale = 2.0f / static_cast<f32>(maximum - minimum);
        input.axisOffset = -static_cast<f32>(maximum + minimum);

        if (!text.empty() && text.front() == '~') {
            input.axisScale = -input.axisScale;
            input.axisOffset = -input.axisOffset;
            text.remove_prefix(1);
        }
        return text.empty();
    }

    if (kind == 'b') {
        i32 index = 0;
        if (!parseNumber(text, index)) {
            return false;
        }
        input.type = MappedInputType::Button;
        input.index = index;
        return text.empty();
    }

    if (kind == 'h') {
        i32 hat = 0;
        if (!parseNumber(text, hat) || text.empty() || text.front() != '.') {
            return false;
        }
        text.remove_prefix(1);
        i32 bit = 0;
        if (!parseNumber(text, bit)) {
            return false;
        }
        input.type = MappedInputType::HatBit;
        input.index = (hat << 4) | bit;
        return text.empty();
    }

    return false;
}

std::string_view nextField(std::string_view& line) {
    const std::size_t comma = line.find(',');
    if (comma == std::string_view::npos) {
        const std::string_view field = line;
        line = {};
        return field;
    }
    const std::string_view field = line.substr(0, comma);
    line.remove_prefix(comma + 1);
    return field;
}

} // namespace

std::string normalizeGamepadGuid(std::string_view guid) {
    std::string result;
    result.reserve(guid.size());
    for (char character : guid) {
        if (std::isspace(static_cast<unsigned char>(character)) != 0) {
            continue;
        }
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

std::string makeLinuxGamepadGuid(u16 bus, u16 vendor, u16 product, u16 version,
                                 std::string_view name) {
    char guid[33] = {};
    if (vendor != 0 && product != 0 && version != 0) {
        std::snprintf(guid, sizeof(guid),
                      "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000",
                      static_cast<unsigned>(bus & 0xff), static_cast<unsigned>(bus >> 8),
                      static_cast<unsigned>(vendor & 0xff), static_cast<unsigned>(vendor >> 8),
                      static_cast<unsigned>(product & 0xff), static_cast<unsigned>(product >> 8),
                      static_cast<unsigned>(version & 0xff), static_cast<unsigned>(version >> 8));
    } else {
        std::array<unsigned char, 11> bytes{};
        std::memcpy(bytes.data(), name.data(), std::min<std::size_t>(bytes.size(), name.size()));
        std::snprintf(guid, sizeof(guid),
                      "%02x%02x0000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                      static_cast<unsigned>(bus & 0xff), static_cast<unsigned>(bus >> 8),
                      bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
                      bytes[7], bytes[8], bytes[9], bytes[10]);
    }
    return guid;
}

std::string makeCocoaGamepadGuid(u64 vendor, u64 product, u64 version, std::string_view name) {
    char guid[33] = {};
    if (vendor != 0 && product != 0) {
        std::snprintf(guid, sizeof(guid),
                      "03000000%02x%02x0000%02x%02x0000%02x%02x0000",
                      static_cast<unsigned>(vendor & 0xff),
                      static_cast<unsigned>((vendor >> 8) & 0xff),
                      static_cast<unsigned>(product & 0xff),
                      static_cast<unsigned>((product >> 8) & 0xff),
                      static_cast<unsigned>(version & 0xff),
                      static_cast<unsigned>((version >> 8) & 0xff));
    } else {
        std::array<unsigned char, 11> bytes{};
        std::memcpy(bytes.data(), name.data(), std::min<std::size_t>(bytes.size(), name.size()));
        std::snprintf(guid, sizeof(guid),
                      "05000000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00", bytes[0],
                      bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                      bytes[8], bytes[9], bytes[10]);
    }
    return guid;
}

bool parseGamepadMapping(std::string_view line, GamepadMapping& mapping, std::string& platform) {
    line = trim(line);
    if (line.empty() || line.front() == '#') {
        return false;
    }

    mapping = GamepadMapping{};
    platform.clear();

    std::string_view guidField = nextField(line);
    std::string_view nameField = nextField(line);
    mapping.guid = normalizeGamepadGuid(guidField);
    mapping.name = std::string(trim(nameField));

    if (mapping.guid.size() != 32 ||
        !std::all_of(mapping.guid.begin(), mapping.guid.end(), [](char character) {
            return std::isxdigit(static_cast<unsigned char>(character)) != 0;
        })) {
        return false;
    }

    while (!line.empty()) {
        std::string_view field = trim(nextField(line));
        if (field.empty()) {
            continue;
        }

        const std::size_t colon = field.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim(field.substr(0, colon));
        const std::string_view value = trim(field.substr(colon + 1));

        if (key == "platform") {
            platform = std::string(value);
            continue;
        }

        const Target target = targetForField(key);
        if (!target.valid) {
            continue;
        }

        MappedInput input;
        if (!parseInput(value, input)) {
            return false;
        }

        if (target.isAxis) {
            if (target.index >= 0 && target.index < static_cast<i32>(GamepadAxis::Count)) {
                mapping.axes[static_cast<std::size_t>(target.index)] = input;
            }
        } else {
            if (target.index >= 0 && target.index < static_cast<i32>(GamepadButton::Count)) {
                mapping.buttons[static_cast<std::size_t>(target.index)] = input;
            }
        }
    }

    return true;
}

GamepadMappingDatabase& GamepadMappingDatabase::instance() {
    static GamepadMappingDatabase database;
    return database;
}

void GamepadMappingDatabase::ensureLoaded() const {
    if (m_loaded) {
        return;
    }
    m_loaded = true;

    for (const char* const* line = kGameControllerDbLines; *line != nullptr; ++line) {
        std::string_view text = *line;
        if (!text.empty() && text.back() == '\r') {
            text.remove_suffix(1);
        }
        if (text.empty() || text.front() == '#') {
            continue;
        }

        GamepadMappingEntry entry;
        if (parseGamepadMapping(text, entry.mapping, entry.platform)) {
            m_entries.push_back(std::move(entry));
        }
    }
}

bool GamepadMappingDatabase::addMappings(std::string_view text) {
    ensureLoaded();

    bool added = false;
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        std::string_view line = newline == std::string_view::npos ? text : text.substr(0, newline);
        if (newline == std::string_view::npos) {
            text = {};
        } else {
            text.remove_prefix(newline + 1);
        }

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        GamepadMappingEntry entry;
        if (parseGamepadMapping(line, entry.mapping, entry.platform)) {
            m_entries.push_back(std::move(entry));
            added = true;
        }
    }
    return added;
}

std::optional<GamepadMapping> GamepadMappingDatabase::find(std::string_view guid,
                                                           std::string_view platform) const {
    ensureLoaded();

    const std::string normalized = normalizeGamepadGuid(guid);
    if (normalized.size() != 32) {
        return std::nullopt;
    }

    // Neueste Einträge zuerst; plattformspezifische und plattformlose
    // Einträge sind beide gültig.
    for (auto iterator = m_entries.rbegin(); iterator != m_entries.rend(); ++iterator) {
        if (iterator->mapping.guid == normalized &&
            (iterator->platform == platform || iterator->platform.empty())) {
            return iterator->mapping;
        }
    }
    return std::nullopt;
}

GamepadState applyGamepadMapping(const GamepadMapping& mapping, std::span<const f32> axes,
                                 std::span<const u8> buttons,
                                 std::span<const JoystickHat> hats) {
    GamepadState state;

    auto mappedAxis = [&axes](const MappedInput& input) {
        if (input.type != MappedInputType::Axis || input.index < 0 ||
            static_cast<std::size_t>(input.index) >= axes.size()) {
            return 0.0f;
        }
        const f32 value = axes[static_cast<std::size_t>(input.index)] * input.axisScale +
                          input.axisOffset;
        return std::clamp(value, -1.0f, 1.0f);
    };

    auto mappedButton = [&buttons](const MappedInput& input) {
        return input.type == MappedInputType::Button && input.index >= 0 &&
               static_cast<std::size_t>(input.index) < buttons.size() &&
               buttons[static_cast<std::size_t>(input.index)] != 0;
    };

    auto mappedHat = [&hats](const MappedInput& input) {
        if (input.type != MappedInputType::HatBit) {
            return false;
        }
        const i32 hat = input.index >> 4;
        const i32 bit = input.index & 0xf;
        return hat >= 0 && static_cast<std::size_t>(hat) < hats.size() &&
               (static_cast<i32>(hats[static_cast<std::size_t>(hat)]) & bit) != 0;
    };

    for (std::size_t i = 0; i < mapping.buttons.size(); ++i) {
        const MappedInput& input = mapping.buttons[i];
        switch (input.type) {
        case MappedInputType::Button:
            state.buttons[i] = mappedButton(input) ? 1 : 0;
            break;
        case MappedInputType::Axis: {
            const f32 value = mappedAxis(input);
            const bool pressed =
                (input.axisOffset < 0.0f || (input.axisOffset == 0.0f && input.axisScale > 0.0f))
                    ? value >= 0.0f
                    : value <= 0.0f;
            state.buttons[i] = pressed ? 1 : 0;
            break;
        }
        case MappedInputType::HatBit:
            state.buttons[i] = mappedHat(input) ? 1 : 0;
            break;
        case MappedInputType::None:
            break;
        }
    }

    for (std::size_t i = 0; i < mapping.axes.size(); ++i) {
        const MappedInput& input = mapping.axes[i];
        switch (input.type) {
        case MappedInputType::Axis:
            state.axes[i] = mappedAxis(input);
            break;
        case MappedInputType::Button:
            state.axes[i] = mappedButton(input) ? 1.0f : -1.0f;
            break;
        case MappedInputType::HatBit:
            state.axes[i] = mappedHat(input) ? 1.0f : -1.0f;
            break;
        case MappedInputType::None:
            break;
        }
    }

    return state;
}

} // namespace sgl::detail
