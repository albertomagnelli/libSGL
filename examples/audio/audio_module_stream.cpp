// ─────────────────────────────────────────────────────────────────────────────
// audio_module_stream – XM-Trackermusik mit sgl::Music
//
// XM ist kein Sample-Format wie WAV, sondern ein Modul-Song. sgl rendert das
// Modul beim Laden einmal nach PCM und spielt es dann wie jede andere Music.
//
//   ./sgl_example_audio_module_stream [sekunden]
// ─────────────────────────────────────────────────────────────────────────────

#include "AudioExampleCommon.hpp"

#include <cstdlib>

using namespace sgl;

int main(int argc, char** argv) {
    f32 duration = 8.0f;

    if (argc > 1) {
        const f32 requested = static_cast<f32>(std::atof(argv[1]));
        if (requested > 0.0f) {
            duration = requested;
        }
    }

    audiodemo::InitAudio();

    Music music;
    if (!music.LoadFromFile(audiodemo::Resource("mini1111.xm").string())) {
        SLOGE << "mini1111.xm konnte nicht geladen werden.";
        audiodemo::CloseAudio();
        return 1;
    }

    SLOGI << "mini1111.xm: " << audiodemo::FormatTime(music.TimeLength())
          << " (XM-Modul, als PCM gerendert)";

    music.SetVolume(0.5f);
    music.SetLooping(true);
    music.Play();

    const f32 step = 0.5f;
    f32 elapsed = 0.0f;

    while (elapsed < duration) {
        audiodemo::SleepSeconds(step);
        elapsed += step;

        SLOGI << "Wiedergabe: " << audiodemo::FormatTime(music.TimePlayed()) << " / "
              << audiodemo::FormatTime(music.TimeLength());
    }

    music.Stop();
    music.Unload();

    audiodemo::CloseAudio();
    return 0;
}
