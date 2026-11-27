// ─────────────────────────────────────────────────────────────────────────────
// audio_info – Metadaten der Audio-Ressourcen anzeigen
//
// Dekodiert alle Dateien aus examples/audio/resources ohne Wiedergabe und
// zeigt Samplerate, Kanaele, Rahmen und Dauer. Unterstuetzt werden WAV, FLAC,
// MP3 und OGG; XM/QOA werden von miniaudio nicht dekodiert.
//
//   ./sgl_example_audio_info
// ─────────────────────────────────────────────────────────────────────────────

#include "AudioExampleCommon.hpp"

#include <array>
#include <string_view>

using namespace sgl;

int main() {
    constexpr std::array<std::string_view, 9> files{
        "coin.wav", "spring.wav", "sound.wav", "weird.wav",
        "target.flac", "target.ogg", "country.mp3",
        "mini1111.xm", "target.qoa",
    };

    i32 loaded = 0;

    for (const std::string_view name : files) {
        const auto path = audiodemo::Resource(name);

        Wave wave;
        if (!wave.LoadFromFile(path.string())) {
            SLOGW << name << ": nicht dekodierbar (Format wird nicht unterstuetzt)";
            continue;
        }

        ++loaded;
        SLOGI << name << ": " << wave.SampleRate() << " Hz, " << wave.Channels()
              << " Kanal/Kanaele, " << wave.FrameCount() << " Rahmen, "
              << audiodemo::FormatTime(wave.Duration());
    }

    SLOGI << loaded << " von " << files.size() << " Dateien erfolgreich dekodiert.";

    return loaded > 0 ? 0 : 1;
}
