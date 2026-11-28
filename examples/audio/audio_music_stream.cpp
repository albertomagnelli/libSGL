// ─────────────────────────────────────────────────────────────────────────────
// audio_music_stream – gestreamte Musik aus country.mp3
//
// Zeigt sgl::Music: Laden, Schleife, Lautstaerke, Seek und Zeitabfragen.
// Ohne Argument wird country.mp3 gespielt. Optional kann eine andere Datei
// (z. B. target.qoa) und danach die Dauer angegeben werden.
//
//   ./sgl_example_audio_music_stream [datei] [sekunden]
//   ./sgl_example_audio_music_stream 8
// ─────────────────────────────────────────────────────────────────────────────

#include "AudioExampleCommon.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace sgl;

int main(int argc, char** argv) {
    f32 duration = 8.0f;
    std::string fileName = "country.mp3";

    if (argc > 1) {
        const std::string first = argv[1];
        const bool isNumber = first.find_first_not_of("0123456789.") == std::string::npos;

        if (isNumber) {
            duration = static_cast<f32>(std::atof(first.c_str()));
        } else {
            fileName = first;
            if (argc > 2) {
                duration = static_cast<f32>(std::atof(argv[2]));
            }
        }
    }

    if (duration <= 0.0f) {
        duration = 8.0f;
    }

    audiodemo::InitAudio();

    const std::string path = std::filesystem::exists(fileName)
                                 ? fileName
                                 : audiodemo::Resource(fileName).string();

    Music music;
    if (!music.loadFromFile(path)) {
        SLOGE << path << " konnte nicht geladen werden.";
        audiodemo::CloseAudio();
        return 1;
    }

    SLOGI << path << ": " << audiodemo::FormatTime(music.getTimeLength())
          << ", streaming=" << (music.isStreaming() ? "ja" : "nein");

    music.setVolume(0.6f);
    music.setLooping(true);
    music.play();

    f32 elapsed = 0.0f;
    while (elapsed < duration) {
        audiodemo::SleepSeconds(0.25f);
        elapsed += 0.25f;

        // miniaudio streamt selbst; Update() bleibt fuer API-Paritaet aufrufbar.
        music.update();

        if ((static_cast<i32>(elapsed * 4.0f) % 4) == 0) {
            SLOGI << "Wiedergabe: " << audiodemo::FormatTime(music.getTimePlayed()) << " / "
                  << audiodemo::FormatTime(music.getTimeLength());
        }
    }

    if (duration > 2.0f) {
        SLOGI << "Seek auf 1,0 s ...";
        music.seek(1.0f);
        audiodemo::SleepSeconds(0.5f);
    }

    music.stop();
    music.unload();

    audiodemo::CloseAudio();
    return 0;
}
