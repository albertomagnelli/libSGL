#pragma once

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include <sgl/Audio.hpp>
#include <sgl/Log.hpp>

namespace audiodemo {

/// Pfad zu einer Datei aus examples/audio/resources.
[[nodiscard]] inline std::filesystem::path Resource(std::string_view name) {
#ifdef SGL_AUDIO_RESOURCES
    return std::filesystem::path(SGL_AUDIO_RESOURCES) / name;
#else
    return std::filesystem::path("resources") / name;
#endif
}

inline void SleepSeconds(const f32 seconds) {
    if (seconds <= 0.0f) {
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<i32>(seconds * 1000.0f)));
}

[[nodiscard]] inline std::string FormatTime(const f32 seconds) {
    const auto total = static_cast<i32>(seconds);
    const auto millis = static_cast<i32>((seconds - static_cast<f32>(total)) * 1000.0f);
    std::ostringstream text;
    text << (total / 60) << ':' << std::setfill('0') << std::setw(2) << (total % 60)
         << '.' << std::setw(3) << millis;
    return text.str();
}

inline void InitAudio() {
    sgl::InitAudioDevice();
    if (sgl::IsAudioDeviceReady()) {
        SLOGI << "Audiogeraet bereit.";
    } else {
        SLOGW << "Kein Audiogeraet verfuegbar; Beispiele laufen stumm weiter.";
    }
}

inline void CloseAudio() {
    sgl::CloseAudioDevice();
}

}  // namespace audiodemo
