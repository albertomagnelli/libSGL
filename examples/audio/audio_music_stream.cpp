// ─────────────────────────────────────────────────────────────────────────────
// audio_music_stream – gestreamte Musik aus country.mp3
//
// Zeigt sgl::Music: Laden, Schleife, Lautstaerke, Seek und Zeitabfragen.
// Die Wiedergabe laeuft standardmaessig 8 Sekunden; die Dauer kann als erstes
// Argument uebergeben werden.
//
//   ./sgl_example_audio_music_stream [sekunden]
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
    if (!music.LoadFromFile(audiodemo::Resource("country.mp3").string())) {
        SLOGE << "country.mp3 konnte nicht geladen werden.";
        audiodemo::CloseAudio();
        return 1;
    }

    SLOGI << "country.mp3: " << audiodemo::FormatTime(music.TimeLength())
          << ", streaming=" << (music.IsStreaming() ? "ja" : "nein");

    music.SetVolume(0.6f);
    music.SetLooping(true);
    music.Play();

    f32 elapsed = 0.0f;
    while (elapsed < duration) {
        audiodemo::SleepSeconds(0.25f);
        elapsed += 0.25f;

        // miniaudio streamt selbst; Update() bleibt fuer API-Paritaet aufrufbar.
        music.Update();

        if ((static_cast<i32>(elapsed * 4.0f) % 4) == 0) {
            SLOGI << "Wiedergabe: " << audiodemo::FormatTime(music.TimePlayed()) << " / "
                  << audiodemo::FormatTime(music.TimeLength());
        }
    }

    if (duration > 2.0f) {
        SLOGI << "Seek auf 1,0 s ...";
        music.Seek(1.0f);
        audiodemo::SleepSeconds(0.5f);
    }

    music.Stop();
    music.Unload();

    audiodemo::CloseAudio();
    return 0;
}
