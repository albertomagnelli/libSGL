#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <vector>

#include <sgl/Audio.hpp>

using namespace sgl;

TEST_CASE("Wave haelt PCM-Daten und rechnet sie um", "[audio][wave]") {
    std::vector<f32> samples(200, 0.25f);

    Wave wave;
    REQUIRE(wave.SetSamples(std::move(samples), 48000, 2));
    REQUIRE(wave.IsValid());
    REQUIRE(wave.FrameCount() == 100);
    REQUIRE(wave.SampleCount() == 200);
    REQUIRE(wave.SampleRate() == 48000);
    REQUIRE(wave.Channels() == 2);
    REQUIRE(wave.SampleSize() == static_cast<i32>(sizeof(f32)));
    REQUIRE(wave.Duration() == Catch::Approx(100.0f / 48000.0f));

    REQUIRE(wave.Crop(10, 19));
    REQUIRE(wave.FrameCount() == 10);
    REQUIRE(wave.SampleCount() == 20);

    REQUIRE(wave.Convert(24000, 1));
    REQUIRE(wave.SampleRate() == 24000);
    REQUIRE(wave.Channels() == 1);
    REQUIRE(wave.FrameCount() == 5);

    const Wave copy = wave.Copy();
    REQUIRE(copy.IsValid());
    REQUIRE(copy.FrameCount() == wave.FrameCount());
}

TEST_CASE("Wave schreibt und liest eine WAV-Datei", "[audio][wave][io]") {
    Wave wave;
    REQUIRE(wave.SetSamples(std::vector<f32>(100, 0.5f), 22050, 1));

    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "sgl-audio-test.wav";
    std::filesystem::remove(file);

    REQUIRE(wave.SaveToFile(file.string()));
    REQUIRE(std::filesystem::exists(file));

    Wave loaded;
    REQUIRE(loaded.LoadFromFile(file.string()));
    REQUIRE(loaded.IsValid());
    REQUIRE(loaded.SampleRate() == 22050);
    REQUIRE(loaded.Channels() == 1);
    REQUIRE(loaded.FrameCount() == 100);

    const std::filesystem::path codeFile =
        std::filesystem::temp_directory_path() / "sgl-audio-test.h";
    std::filesystem::remove(codeFile);
    REQUIRE(wave.SaveAsCode(codeFile.string(), "sglTestSamples"));
    REQUIRE(std::filesystem::exists(codeFile));

    std::filesystem::remove(file);
    std::filesystem::remove(codeFile);
}

TEST_CASE("Sound und AudioStream lassen sich aus Daten anlegen", "[audio]") {
    Wave wave;
    REQUIRE(wave.SetSamples(std::vector<f32>(400, 0.0f), 44100, 2));

    Sound sound;
    REQUIRE(sound.LoadFromWave(wave, 2));
    REQUIRE(sound.IsValid());
    REQUIRE(sound.VoiceCount() == 2);
    REQUIRE(sound.Data().FrameCount() == 200);

    AudioStream stream;
    REQUIRE(stream.Load(48000, 2, 256));
    REQUIRE(stream.IsValid());
    REQUIRE(stream.CapacityFrames() == 256);

    const std::array<f32, 32> frames{};
    REQUIRE(stream.Push(frames.data(), 16));
    REQUIRE(stream.QueuedFrames() == 16);

    stream.Clear();
    REQUIRE(stream.QueuedFrames() == 0);
    stream.Unload();
    REQUIRE_FALSE(stream.IsValid());
}
