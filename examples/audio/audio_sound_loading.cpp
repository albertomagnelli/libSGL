// ─────────────────────────────────────────────────────────────────────────────
// audio_sound_loading – kurze Effekte laden und abspielen
//
// Laedt coin.wav, spring.wav, sound.wav und weird.wav als sgl::Sound. Jeder
// Effekt wird einmal abgespielt; sound.wav laeuft zusaetzlich als Akkord mit
// mehreren Stimmen, unterschiedlicher Tonhoehe und Panorama-Position.
//
//   ./sgl_example_audio_sound_loading
// ─────────────────────────────────────────────────────────────────────────────

#include "AudioExampleCommon.hpp"

#include <array>
#include <cstddef>
#include <string_view>

using namespace sgl;

int main() {
    audiodemo::InitAudio();

    constexpr std::array<std::string_view, 5> effects{
        "coin.wav", "spring.wav", "sound.wav", "weird.wav", "target.qoa",
    };

    for (const std::string_view name : effects) {
        const auto path = audiodemo::Resource(name);
        Sound sound;

        if (!sound.loadFromFile(path.string(), 8)) {
            SLOGW << name << ": konnte nicht geladen werden";
            continue;
        }

        SLOGI << name << " (" << audiodemo::FormatTime(sound.getDuration()) << ")";
        sound.play();
        audiodemo::SleepSeconds(sound.getDuration() + 0.1f);
        sound.stop();
    }

    // Mehrere Sounds gleichzeitig: ein kurzer Akkord aus drei sound.wav-Stimmen.
    SLOGI << "Drei sound.wav-Stimmen gleichzeitig (Pitch/Panorama variiert).";
    std::array<Sound, 3> chord;
    bool chordReady = true;

    for (std::size_t voice = 0; voice < chord.size(); ++voice) {
        if (!chord[voice].loadFromFile(audiodemo::Resource("sound.wav").string(), 1)) {
            chordReady = false;
            continue;
        }

        chord[voice].setVolume(0.3f);
        chord[voice].setPitch(0.75f + static_cast<f32>(voice) * 0.25f);
        chord[voice].setPan(static_cast<f32>(voice) - 1.0f);
    }

    if (chordReady) {
        for (std::size_t voice = 0; voice < chord.size(); ++voice) {
            chord[voice].play();
            audiodemo::SleepSeconds(0.08f);
        }

        audiodemo::SleepSeconds(chord[0].getDuration() + 0.2f);

        for (Sound& sound : chord) {
            sound.stop();
        }
    }

    audiodemo::CloseAudio();
    return 0;
}
