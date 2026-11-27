// ─────────────────────────────────────────────────────────────────────────────
// audio_raw_stream – eigener PCM-Strom mit einem Sinuston
//
// Erzeugt einen 440-Hz-Sinuston und schiebt ihn blockweise in einen
// sgl::AudioStream. Der Beispielaufruf zeigt, wie ein eigener Datenstrom
// (z. B. Synthesizer, Tracker, Netzwerk-Audio) an die Engine angebunden wird.
//
//   ./sgl_example_audio_raw_stream [sekunden]
// ─────────────────────────────────────────────────────────────────────────────

#include "AudioExampleCommon.hpp"

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <vector>

using namespace sgl;

int main(int argc, char** argv) {
    f32 duration = 3.0f;

    if (argc > 1) {
        const f32 requested = static_cast<f32>(std::atof(argv[1]));
        if (requested > 0.0f) {
            duration = requested;
        }
    }

    audiodemo::InitAudio();

    constexpr i32 sampleRate = 48000;
    constexpr i32 channels = 2;
    constexpr i32 blockFrames = 1024;
    constexpr f32 frequency = 440.0f;

    AudioStream stream;
    if (!stream.Load(sampleRate, channels, 4096)) {
        SLOGE << "AudioStream konnte nicht angelegt werden.";
        audiodemo::CloseAudio();
        return 1;
    }

    stream.SetVolume(0.25f);
    stream.Play();

    std::vector<f32> block(static_cast<std::size_t>(blockFrames) * channels);
    f32 phase = 0.0f;
    const f32 phaseStep = 2.0f * std::numbers::pi_v<f32> * frequency / static_cast<f32>(sampleRate);
    const f32 blockSeconds = static_cast<f32>(blockFrames) / static_cast<f32>(sampleRate);

    f32 elapsed = 0.0f;

    while (elapsed < duration) {
        for (i32 frame = 0; frame < blockFrames; ++frame) {
            const f32 value = 0.5f * std::sin(phase);
            phase += phaseStep;

            for (i32 channel = 0; channel < channels; ++channel) {
                block[static_cast<std::size_t>(frame) * channels + channel] = value;
            }
        }

        if (!stream.Push(block.data(), blockFrames)) {
            SLOGW << "AudioStream-Puffer voll; halte das Beispiel an.";
            break;
        }

        audiodemo::SleepSeconds(blockSeconds);
        elapsed += blockSeconds;
    }

    stream.Stop();
    stream.Unload();

    SLOGI << "Sinuston beendet (" << duration << " s angefordert).";

    audiodemo::CloseAudio();
    return 0;
}
