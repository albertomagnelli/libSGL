//
// Created by ingsto on 14.11.26.
//
// Audio: Wave, Sound, Music und AudioStream auf miniaudio 0.11.
//
// miniaudio wird nur hier eingebunden (pimpl), damit die oeffentlichen Header
// schlank bleiben. Ohne Ausgabegeraet funktioniert das Dekodieren weiter,
// Wiedergabe bleibt dann stumm (Warnung einmal im Log).
//
#include <sgl/Audio.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string_view>
#include <vector>

#include <sgl/Log.hpp>

#include "gl/AudioCodecs.hpp"
#include "miniaudio.h"

namespace sgl {

namespace {
  // ---------------------------------------------------------------------------
  // Engine: ein Geraet je Prozess
  // ---------------------------------------------------------------------------
  struct AudioEngine {
    ma_engine  engine{};
    bool       ready{false};
    f32        volume{1.0f};
    std::mutex mutex;
  };

  AudioEngine &Engine() {
    static AudioEngine      audio;
    static std::once_flag   once;

    std::call_once(once, []() {
      ma_engine_config config = ma_engine_config_init();

      if (ma_engine_init(&config, &audio.engine) != MA_SUCCESS) {
        SLOGW << "Audio: kein Ausgabegeraet -- Wiedergabe bleibt stumm (Dekodieren geht weiter)";
        audio.ready = false;

        return;
      }

      audio.ready = true;

      const ma_device *device = ma_engine_get_device(&audio.engine);

      SLOGD << "Audio: Geraet bereit (" << static_cast<int>(device->sampleRate) << " Hz, "
            << static_cast<int>(device->playback.channels) << " Kanaele, "
            << ma_get_backend_name(device->pContext->backend) << ")";
    });

    return audio;
  }

  bool EngineReady() { return Engine().ready; }

  // ---------------------------------------------------------------------------
  // Dekodieren nach 32-Bit-Float, Kanaele/Samplerate der Quelle
  // ---------------------------------------------------------------------------
  bool DecodeFile(const std::string &path, std::vector<f32> &samples, i32 &sampleRate, i32 &channels) {
    samples.clear();
    sampleRate = 0;
    channels   = 0;

    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder              decoder{};

    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS) {
      SLOGW << "Audio: \"" << path << "\" nicht dekodierbar (unterstuetzt: WAV, FLAC, MP3, OGG, QOA)";

      return false;
    }

    const auto sourceChannels = static_cast<i32>(decoder.outputChannels);
    const auto sourceRate     = static_cast<i32>(decoder.outputSampleRate);

    ma_uint64 length = 0;

    if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) != MA_SUCCESS) {
      // unbekannte Laenge: alles lesen, bis Schluss
      length = 0;
    }

    std::vector<f32> buffer;
    buffer.resize((length > 0) ? (static_cast<std::size_t>(length) * static_cast<std::size_t>(sourceChannels))
                               : (65536 * static_cast<std::size_t>(sourceChannels)));

    std::size_t written = 0;

    while (true) {
      const auto capacityFrames =
          (buffer.size() - written) / static_cast<std::size_t>(std::max(1, sourceChannels));

      if (capacityFrames == 0) {
        buffer.resize(buffer.size() * 2);
        continue;
      }

      ma_uint64 read = 0;

      if (ma_decoder_read_pcm_frames(&decoder, buffer.data() + written, capacityFrames, &read) != MA_SUCCESS) break;

      if (read == 0) break;

      written += static_cast<std::size_t>(read) * static_cast<std::size_t>(sourceChannels);

      if ((length > 0) && (written >= buffer.size())) break;
    }

    ma_decoder_uninit(&decoder);

    if (written == 0) {
      SLOGW << "Audio: \"" << path << "\" enthaelt keine Samples";

      return false;
    }

    buffer.resize(written);
    samples    = std::move(buffer);
    sampleRate = sourceRate;
    channels   = sourceChannels;

    return true;
  }

  bool DecodeMemory(const u8 *data, const std::size_t size, std::vector<f32> &samples, i32 &sampleRate,
                    i32 &channels) {
    samples.clear();
    sampleRate = 0;
    channels   = 0;

    if ((data == nullptr) || (size == 0)) return false;

    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder              decoder{};

    if (ma_decoder_init_memory(data, size, &config, &decoder) != MA_SUCCESS) {
      SLOGW << "Audio: Speicherinhalt nicht dekodierbar";

      return false;
    }

    ma_uint64 length = 0;

    if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) != MA_SUCCESS) length = 0;

    std::vector<f32> buffer;
    buffer.resize((length > 0) ? (static_cast<std::size_t>(length) * static_cast<std::size_t>(decoder.outputChannels))
                               : 65536);

    std::size_t written = 0;

    while (true) {
      const auto capacityFrames = (buffer.size() - written) / std::max(1u, decoder.outputChannels);

      if (capacityFrames == 0) {
        buffer.resize(buffer.size() * 2);
        continue;
      }

      ma_uint64 read = 0;

      if (ma_decoder_read_pcm_frames(&decoder, buffer.data() + written, capacityFrames, &read) != MA_SUCCESS) break;
      if (read == 0) break;

      written += static_cast<std::size_t>(read) * decoder.outputChannels;

      if ((length > 0) && (written >= buffer.size())) break;
    }

    const auto sourceRate     = static_cast<i32>(decoder.outputSampleRate);
    const auto sourceChannels = static_cast<i32>(decoder.outputChannels);

    ma_decoder_uninit(&decoder);

    if (written == 0) return false;

    buffer.resize(written);
    samples    = std::move(buffer);
    sampleRate = sourceRate;
    channels   = sourceChannels;

    return true;
  }

  void WarnOnce(const char *message) {
    static std::mutex          mutex;
    static std::vector<std::string> reported;

    std::lock_guard<std::mutex> lock(mutex);

    for (const std::string &entry : reported) {
      if (entry == message) return;
    }

    reported.emplace_back(message);
    SLOGW << message;
  }

  std::string NormalizeExtension(std::string_view extension) {
    if (extension.empty()) return {};

    std::string result(extension);

    if (!result.empty() && result.front() != '.') result.insert(result.begin(), '.');

    for (char &character : result) {
      character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    return result;
  }

  std::string FileExtension(const std::string &path) {
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');

    if ((dot == std::string::npos) || ((slash != std::string::npos) && (dot < slash))) return {};

    return NormalizeExtension(std::string_view(path).substr(dot));
  }

  bool HasExtension(const std::string &path, std::string_view extension) {
    return FileExtension(path) == NormalizeExtension(extension);
  }

  bool DecodeQoaFileIntoWave(const std::string &path, Wave &wave) {
    std::vector<f32> samples;
    i32 sampleRate = 0;
    i32 channels   = 0;

    if (!detail::DecodeQoaFile(path, samples, sampleRate, channels)) return false;

    return wave.setSamples(std::move(samples), sampleRate, channels);
  }

  bool DecodeQoaMemoryIntoWave(const u8 *data, const std::size_t size, Wave &wave) {
    std::vector<f32> samples;
    i32 sampleRate = 0;
    i32 channels   = 0;

    if (!detail::DecodeQoaMemory(data, size, samples, sampleRate, channels)) return false;

    return wave.setSamples(std::move(samples), sampleRate, channels);
  }

  bool DecodeXmFileIntoWave(const std::string &path, Wave &wave) {
    std::vector<f32> samples;
    i32 sampleRate = 0;
    i32 channels   = 0;

    if (!detail::DecodeXmFile(path, samples, sampleRate, channels)) return false;

    return wave.setSamples(std::move(samples), sampleRate, channels);
  }

  bool DecodeXmMemoryIntoWave(const u8 *data, const std::size_t size, Wave &wave) {
    std::vector<f32> samples;
    i32 sampleRate = 0;
    i32 channels   = 0;

    if (!detail::DecodeXmMemory(data, size, samples, sampleRate, channels)) return false;

    return wave.setSamples(std::move(samples), sampleRate, channels);
  }
} // namespace

// =============================================================================
// Wave
// =============================================================================
bool Wave::loadFromFile(const std::string &path) {
  unload();

  if (HasExtension(path, ".qoa")) {
    if (DecodeQoaFileIntoWave(path, *this)) return true;

    SLOGW << "Audio: \"" << path << "\" ist kein gueltiges QOA";

    return false;
  }

  return DecodeFile(path, m_samples, m_sampleRate, m_channels);
}

bool Wave::loadFromMemory(const u8 *data, const std::size_t size, const std::string &extension) {
  unload();

  const std::string format = NormalizeExtension(extension);

  if ((format == ".qoa") || detail::IsQoaMemory(data, size)) {
    if (DecodeQoaMemoryIntoWave(data, size, *this)) return true;

    SLOGW << "Audio: QOA-Speicherinhalt ist ungueltig";

    return false;
  }

  // miniaudio erkennt WAV/FLAC/MP3/OGG am Inhalt.
  return DecodeMemory(data, size, m_samples, m_sampleRate, m_channels);
}

bool Wave::setSamples(std::vector<f32> samples, const i32 sampleRate, const i32 channels) {
  if ((sampleRate <= 0) || (channels <= 0) || samples.empty()) return false;

  m_samples    = std::move(samples);
  m_sampleRate = sampleRate;
  m_channels   = channels;

  return true;
}

void Wave::unload() {
  m_samples.clear();
  m_sampleRate = 0;
  m_channels   = 0;
}

bool Wave::isValid() const { return !m_samples.empty() && (m_sampleRate > 0) && (m_channels > 0); }

i32 Wave::frameCount() const {
  return (m_channels > 0) ? static_cast<i32>(m_samples.size() / static_cast<std::size_t>(m_channels)) : 0;
}

i32 Wave::sampleRate() const { return m_sampleRate; }

i32 Wave::channels() const { return m_channels; }

i32 Wave::sampleSize() const { return static_cast<i32>(sizeof(f32)); }

i32 Wave::sampleCount() const { return static_cast<i32>(m_samples.size()); }

f32 Wave::duration() const {
  if ((m_sampleRate <= 0) || (m_channels <= 0)) return 0.0f;

  return static_cast<f32>(frameCount()) / static_cast<f32>(m_sampleRate);
}

const std::vector<f32> &Wave::samples() const { return m_samples; }

std::vector<f32> &Wave::samples() { return m_samples; }

Wave Wave::copy() const { return *this; }

bool Wave::crop(const i32 firstFrame, const i32 lastFrame) {
  if (!isValid()) return false;

  const i32 frames = frameCount();
  const i32 first  = std::clamp(firstFrame, 0, frames - 1);
  const i32 last   = std::clamp(lastFrame, first, frames - 1);

  const auto start = static_cast<std::size_t>(first) * static_cast<std::size_t>(m_channels);
  const auto end   = (static_cast<std::size_t>(last) + 1) * static_cast<std::size_t>(m_channels);

  std::vector<f32> cropped(m_samples.begin() + static_cast<std::ptrdiff_t>(start),
                           m_samples.begin() + static_cast<std::ptrdiff_t>(end));

  m_samples = std::move(cropped);

  return true;
}

bool Wave::convert(const i32 sampleRate, const i32 channels) {
  if (!isValid() || (sampleRate <= 0) || (channels <= 0)) return false;

  if ((sampleRate == m_sampleRate) && (channels == m_channels)) return true;

  const auto frames = static_cast<ma_uint64>(frameCount());

  // Zielgroesse bestimmen (miniaudio rechnet linear um und mischt Kanaele)
  const auto needed = ma_convert_frames(nullptr, 0, ma_format_f32, static_cast<ma_uint32>(channels),
                                        static_cast<ma_uint32>(sampleRate), m_samples.data(), frames, ma_format_f32,
                                        static_cast<ma_uint32>(m_channels), static_cast<ma_uint32>(m_sampleRate));

  if (needed == 0) return false;

  std::vector<f32> converted(static_cast<std::size_t>(needed) * static_cast<std::size_t>(channels));

  const auto written = ma_convert_frames(converted.data(), needed, ma_format_f32, static_cast<ma_uint32>(channels),
                                         static_cast<ma_uint32>(sampleRate), m_samples.data(), frames, ma_format_f32,
                                         static_cast<ma_uint32>(m_channels), static_cast<ma_uint32>(m_sampleRate));

  if (written == 0) return false;

  converted.resize(static_cast<std::size_t>(written) * static_cast<std::size_t>(channels));

  m_samples    = std::move(converted);
  m_sampleRate = sampleRate;
  m_channels   = channels;

  return true;
}

bool Wave::saveToFile(const std::string &path) const {
  if (!isValid()) return false;

  if (HasExtension(path, ".qoa")) {
    return detail::EncodeQoaFile(path, m_samples, m_sampleRate, m_channels);
  }

  std::ofstream stream(path, std::ios::binary);

  if (!stream) {
    SLOGE << "Wave: \"" << path << "\" nicht schreibbar";

    return false;
  }

  // 16-Bit-PCM-WAV schreiben
  const auto frames      = static_cast<u32>(frameCount());
  const auto channels    = static_cast<u16>(m_channels);
  const auto sampleRate  = static_cast<u32>(m_sampleRate);
  const auto bits        = static_cast<u16>(16);
  const auto blockAlign  = static_cast<u16>(channels * (bits / 8));
  const auto byteRate    = sampleRate * blockAlign;
  const auto dataBytes   = static_cast<u32>(m_samples.size() * sizeof(i16));

  auto writeU32 = [&stream](const u32 value) { stream.write(reinterpret_cast<const char *>(&value), 4); };
  auto writeU16 = [&stream](const u16 value) { stream.write(reinterpret_cast<const char *>(&value), 2); };

  stream.write("RIFF", 4);
  writeU32(36 + dataBytes);
  stream.write("WAVE", 4);

  stream.write("fmt ", 4);
  writeU32(16);      // Blockgroesse
  writeU16(1);       // PCM
  writeU16(channels);
  writeU32(sampleRate);
  writeU32(byteRate);
  writeU16(blockAlign);
  writeU16(bits);

  stream.write("data", 4);
  writeU32(dataBytes);

  for (const f32 sample : m_samples) {
    const f32 clamped = std::clamp(sample, -1.0f, 1.0f);
    const auto value  = static_cast<i16>(std::lround(clamped * 32767.0f));

    stream.write(reinterpret_cast<const char *>(&value), 2);
  }

  (void)frames;

  return stream.good();
}

bool Wave::saveAsCode(const std::string &path, const std::string &variableName) const {
  if (!isValid()) return false;

  std::ofstream stream(path);

  if (!stream) return false;

  stream << "// erzeugt mit sgl -- " << frameCount() << " Rahmen, " << m_sampleRate << " Hz, " << m_channels
         << " Kanaele\n";
  stream << "static const float " << variableName << "[] = {\n";

  for (std::size_t i = 0; i < m_samples.size(); ++i) {
    if ((i % 8) == 0) stream << "    ";

    stream << m_samples[i] << "f";

    if ((i + 1) < m_samples.size()) stream << ", ";
    if (((i + 1) % 8) == 0) stream << "\n";
  }

  stream << "\n};\n";
  stream << "static const int " << variableName << "FrameCount = " << frameCount() << ";\n";
  stream << "static const int " << variableName << "SampleRate = " << m_sampleRate << ";\n";
  stream << "static const int " << variableName << "Channels = " << m_channels << ";\n";

  return stream.good();
}

// =============================================================================
// Sound
// =============================================================================
struct Sound::Impl {
  ma_audio_buffer       buffer{};
  bool                  bufferReady{false};
  std::vector<ma_sound> voices;
  f32                   volume{1.0f};
  f32                   pitch{1.0f};
  f32                   pan{0.0f};
  bool                  looping{false};
  bool                  paused{false};

  void applyParameters() {
    for (ma_sound &voice : voices) {
      ma_sound_set_volume(&voice, volume);
      ma_sound_set_pitch(&voice, pitch);
      ma_sound_set_pan(&voice, pan);
      ma_sound_set_looping(&voice, looping ? MA_TRUE : MA_FALSE);
    }
  }
};

Sound::Sound() : m_impl(std::make_unique<Impl>()) {}

Sound::~Sound() { unload(); }

Sound::Sound(Sound &&other) noexcept : m_impl(std::move(other.m_impl)), m_wave(std::move(other.m_wave)) {}

Sound &Sound::operator=(Sound &&other) noexcept {
  if (this == &other) return *this;

  unload();

  m_impl = std::move(other.m_impl);
  m_wave = std::move(other.m_wave);

  return *this;
}

bool Sound::loadFromFile(const std::string &path, const i32 voices) {
  Wave wave;

  if (!wave.loadFromFile(path)) return false;

  return loadFromWave(wave, voices);
}

bool Sound::loadFromWave(const Wave &wave, const i32 voices) {
  unload();

  if (!wave.isValid()) {
    SLOGW << "Sound: Wavedaten sind leer";

    return false;
  }

  m_wave = wave; // eigene Kopie; die Stimmen zeigen auf deren Puffer
  m_impl = std::make_unique<Impl>();

  AudioEngine &audio = Engine();

  if (!audio.ready) {
    static bool warned = false;

    if (!warned) {
      warned = true;
      SLOGW << "Sound: ohne Audiogeraet nur geladen, nicht abspielbar";
    }

    return true; // Daten sind geladen; IsValid() meldet true, Wiedergabe bleibt stumm
  }

  const auto frames = static_cast<ma_uint64>(m_wave.frameCount());

  ma_audio_buffer_config config = ma_audio_buffer_config_init(
      ma_format_f32, static_cast<ma_uint32>(m_wave.channels()), frames, m_wave.samples().data(), nullptr);
  config.sampleRate = static_cast<ma_uint32>(m_wave.sampleRate());

  if (ma_audio_buffer_init(&config, &m_impl->buffer) != MA_SUCCESS) {
    SLOGE << "Sound: Puffer laesst sich nicht anlegen";

    return false;
  }

  m_impl->bufferReady = true;

  const i32 count = std::max(1, voices);

  m_impl->voices.resize(static_cast<std::size_t>(count));

  for (ma_sound &voice : m_impl->voices) {
    if (ma_sound_init_from_data_source(&audio.engine, &m_impl->buffer, 0, nullptr, &voice) != MA_SUCCESS) {
      SLOGE << "Sound: Stimme laesst sich nicht anlegen";
      m_impl->voices.clear();

      return false;
    }
  }

  m_impl->applyParameters();

  return true;
}

void Sound::unload() {
  AudioEngine &audio = Engine();

  if (m_impl != nullptr) {
    for (ma_sound &voice : m_impl->voices) {
      if (audio.ready) ma_sound_uninit(&voice);
    }

    m_impl->voices.clear();

    if (m_impl->bufferReady && audio.ready) ma_audio_buffer_uninit(&m_impl->buffer);

    m_impl->bufferReady = false;
    m_impl              = std::make_unique<Impl>();
  }

  m_wave.unload();
}

bool Sound::isValid() const { return m_wave.isValid() && (m_impl != nullptr); }

const Wave &Sound::data() const { return m_wave; }

f32 Sound::duration() const { return m_wave.duration(); }

i32 Sound::voiceCount() const { return (m_impl != nullptr) ? static_cast<i32>(m_impl->voices.size()) : 0; }

void Sound::play() {
  if ((m_impl == nullptr) || m_impl->voices.empty()) {
    WarnOnce("Sound: Play ohne Stimme (kein Audiogeraet?)");

    return;
  }

  // freie Stimme suchen, sonst die erste wiederverwenden
  ma_sound *target = &m_impl->voices.front();

  for (ma_sound &voice : m_impl->voices) {
    if (!ma_sound_is_playing(&voice)) {
      target = &voice;

      break;
    }
  }

  m_impl->paused = false;

  ma_sound_seek_to_pcm_frame(target, 0);
  ma_sound_start(target);
}

void Sound::stop() {
  if (m_impl == nullptr) return;

  for (ma_sound &voice : m_impl->voices) {
    ma_sound_stop(&voice);
    ma_sound_seek_to_pcm_frame(&voice, 0);
  }

  m_impl->paused = false;
}

void Sound::pause() {
  if (m_impl == nullptr) return;

  for (ma_sound &voice : m_impl->voices) ma_sound_stop(&voice); // Cursor bleibt stehen

  m_impl->paused = true;
}

void Sound::resume() {
  if (m_impl == nullptr) return;

  for (ma_sound &voice : m_impl->voices) ma_sound_start(&voice);

  m_impl->paused = false;
}

bool Sound::isPlaying() const {
  if (m_impl == nullptr) return false;

  for (const ma_sound &voice : m_impl->voices) {
    if (ma_sound_is_playing(&voice)) return true;
  }

  return false;
}

void Sound::setVolume(const f32 volume) {
  if (m_impl == nullptr) return;

  m_impl->volume = std::clamp(volume, 0.0f, 4.0f);
  m_impl->applyParameters();
}

f32 Sound::volume() const { return (m_impl != nullptr) ? m_impl->volume : 0.0f; }

void Sound::setPitch(const f32 pitch) {
  if (m_impl == nullptr) return;

  m_impl->pitch = std::clamp(pitch, 0.01f, 8.0f);
  m_impl->applyParameters();
}

f32 Sound::pitch() const { return (m_impl != nullptr) ? m_impl->pitch : 1.0f; }

void Sound::setPan(const f32 pan) {
  if (m_impl == nullptr) return;

  m_impl->pan = std::clamp(pan, -1.0f, 1.0f);
  m_impl->applyParameters();
}

f32 Sound::pan() const { return (m_impl != nullptr) ? m_impl->pan : 0.0f; }

void Sound::setLooping(const bool looping) {
  if (m_impl == nullptr) return;

  m_impl->looping = looping;

  m_impl->applyParameters();
}

bool Sound::isLooping() const { return (m_impl != nullptr) && m_impl->looping; }

// =============================================================================
// Music
// =============================================================================
struct Music::Impl {
  ma_sound        sound{};
  ma_decoder      decoder{};       // nur fuer LoadFromMemory
  ma_audio_buffer buffer{};        // QOA/XM: vollstaendig dekodiert
  Wave            decoded;         // Daten fuer buffer
  bool            soundReady{false};
  bool            decoderReady{false};
  bool            bufferReady{false};
  bool            streaming{false};
  bool            paused{false};
  f32             volume{1.0f};
  f32             pitch{1.0f};
  f32             pan{0.0f};
  bool            looping{false};
  std::vector<u8> memory; // muss leben, solange der Decoder laeuft
};

bool Music::InitBuffer(Impl &impl, Wave wave) {
  if (!wave.isValid()) return false;

  AudioEngine &audio = Engine();

  impl.decoded = std::move(wave);

  const auto frames = static_cast<ma_uint64>(impl.decoded.frameCount());

  ma_audio_buffer_config config = ma_audio_buffer_config_init(
      ma_format_f32, static_cast<ma_uint32>(impl.decoded.channels()), frames,
      impl.decoded.samples().data(), nullptr);
  config.sampleRate = static_cast<ma_uint32>(impl.decoded.sampleRate());

  if (ma_audio_buffer_init(&config, &impl.buffer) != MA_SUCCESS) {
    SLOGE << "Music: Puffer laesst sich nicht anlegen";

    return false;
  }

  impl.bufferReady = true;

  if (ma_sound_init_from_data_source(&audio.engine, &impl.buffer, 0, nullptr,
                                     &impl.sound) != MA_SUCCESS) {
    SLOGE << "Music: Stimme laesst sich nicht anlegen";

    return false;
  }

  impl.soundReady = true;
  impl.streaming  = false;

  ma_sound_set_volume(&impl.sound, impl.volume);
  ma_sound_set_pitch(&impl.sound, impl.pitch);
  ma_sound_set_pan(&impl.sound, impl.pan);

  return true;
}

Music::Music() : m_impl(std::make_unique<Impl>()) {}

Music::~Music() { unload(); }

Music::Music(Music &&other) noexcept : m_impl(std::move(other.m_impl)) {}

Music &Music::operator=(Music &&other) noexcept {
  if (this == &other) return *this;

  unload();

  m_impl = std::move(other.m_impl);

  return *this;
}

bool Music::loadFromFile(const std::string &path) {
  unload();

  m_impl = std::make_unique<Impl>();

  AudioEngine &audio = Engine();

  if (!audio.ready) {
    WarnOnce("Music: ohne Audiogeraet nicht abspielbar");

    return false;
  }

  const std::string extension = FileExtension(path);

  // QOA und XM werden vollständig nach PCM dekodiert. Das hält Wiedergabe,
  // Seek und Looping identisch zu den übrigen Music-Formaten.
  if ((extension == ".qoa") || (extension == ".xm")) {
    Wave decoded;

    const bool isQoa = extension == ".qoa";
    const bool ok = isQoa ? DecodeQoaFileIntoWave(path, decoded)
                          : DecodeXmFileIntoWave(path, decoded);

    if (!ok) {
      SLOGW << "Music: \"" << path << "\" nicht dekodierbar ("
            << (isQoa ? "QOA" : "XM") << ")";

      return false;
    }

    return InitBuffer(*m_impl, std::move(decoded));
  }

  constexpr auto flags = MA_SOUND_FLAG_STREAM;

  if (ma_sound_init_from_file(&audio.engine, path.c_str(), flags, nullptr, nullptr, &m_impl->sound) != MA_SUCCESS) {
    SLOGW << "Music: \"" << path << "\" nicht ladbar (unterstuetzt: WAV, FLAC, MP3, OGG, QOA, XM)";

    return false;
  }

  m_impl->soundReady = true;
  m_impl->streaming  = true;

  ma_sound_set_volume(&m_impl->sound, m_impl->volume);
  ma_sound_set_pitch(&m_impl->sound, m_impl->pitch);
  ma_sound_set_pan(&m_impl->sound, m_impl->pan);

  return true;
}

bool Music::loadFromMemory(const u8 *data, const std::size_t size, const std::string &extension) {
  unload();

  m_impl = std::make_unique<Impl>();

  if ((data == nullptr) || (size == 0)) return false;

  const std::string format = NormalizeExtension(extension);

  AudioEngine &audio = Engine();

  if (!audio.ready) {
    WarnOnce("Music: ohne Audiogeraet nicht abspielbar");

    return false;
  }

  if ((format == ".qoa") || detail::IsQoaMemory(data, size)) {
    Wave decoded;

    if (!DecodeQoaMemoryIntoWave(data, size, decoded)) {
      SLOGW << "Music: QOA-Speicherinhalt ist ungueltig";

      return false;
    }

    return InitBuffer(*m_impl, std::move(decoded));
  }

  if ((format == ".xm") || detail::IsXmMemory(data, size)) {
    Wave decoded;

    if (!DecodeXmMemoryIntoWave(data, size, decoded)) {
      SLOGW << "Music: XM-Speicherinhalt ist ungueltig";

      return false;
    }

    return InitBuffer(*m_impl, std::move(decoded));
  }

  m_impl->memory.assign(data, data + size);

  const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);

  if (ma_decoder_init_memory(m_impl->memory.data(), m_impl->memory.size(), &config, &m_impl->decoder) != MA_SUCCESS) {
    SLOGW << "Music: Speicherinhalt nicht dekodierbar";

    return false;
  }

  m_impl->decoderReady = true;

  if (ma_sound_init_from_data_source(&audio.engine, &m_impl->decoder, 0, nullptr, &m_impl->sound) != MA_SUCCESS) {
    SLOGE << "Music: Stimme laesst sich nicht anlegen";

    return false;
  }

  m_impl->soundReady = true;
  m_impl->streaming  = false;

  ma_sound_set_volume(&m_impl->sound, m_impl->volume);
  ma_sound_set_pitch(&m_impl->sound, m_impl->pitch);
  ma_sound_set_pan(&m_impl->sound, m_impl->pan);

  return true;
}

void Music::unload() {
  AudioEngine &audio = Engine();

  if (m_impl != nullptr) {
    if (m_impl->soundReady && audio.ready) ma_sound_uninit(&m_impl->sound);
    if (m_impl->decoderReady && audio.ready) ma_decoder_uninit(&m_impl->decoder);
    if (m_impl->bufferReady && audio.ready) ma_audio_buffer_uninit(&m_impl->buffer);

    m_impl->soundReady   = false;
    m_impl->decoderReady = false;
    m_impl->bufferReady  = false;
    m_impl->decoded.unload();
    m_impl->memory.clear();

    m_impl = std::make_unique<Impl>();
  }
}

bool Music::isValid() const { return (m_impl != nullptr) && m_impl->soundReady; }

bool Music::isStreaming() const { return (m_impl != nullptr) && m_impl->streaming; }

void Music::play() {
  if (!isValid()) return;

  m_impl->paused = false;

  ma_sound_seek_to_pcm_frame(&m_impl->sound, 0);
  ma_sound_start(&m_impl->sound);
}

void Music::stop() {
  if (!isValid()) return;

  ma_sound_stop(&m_impl->sound);
  ma_sound_seek_to_pcm_frame(&m_impl->sound, 0);

  m_impl->paused = false;
}

void Music::pause() {
  if (!isValid()) return;

  ma_sound_stop(&m_impl->sound); // Cursor bleibt

  m_impl->paused = true;
}

void Music::resume() {
  if (!isValid()) return;

  ma_sound_start(&m_impl->sound);

  m_impl->paused = false;
}

bool Music::isPlaying() const { return isValid() && (ma_sound_is_playing(&m_impl->sound) != MA_FALSE); }

bool Music::isPaused() const { return (m_impl != nullptr) && m_impl->paused; }

void Music::update() {
  // miniaudio streamt im Audiothread -- hier gibt es nichts nachzufuellen
}

void Music::seek(const f32 seconds) {
  if (!isValid()) return;

  ma_sound_seek_to_second(&m_impl->sound, std::max(0.0f, seconds));
}

f32 Music::timeLength() const {
  if (!isValid()) return 0.0f;

  float length = 0.0f;

  ma_sound_get_length_in_seconds(&m_impl->sound, &length);

  return length;
}

f32 Music::timePlayed() const {
  if (!isValid()) return 0.0f;

  float cursor = 0.0f;

  ma_sound_get_cursor_in_seconds(&m_impl->sound, &cursor);

  return cursor;
}

void Music::setVolume(const f32 volume) {
  if (!isValid()) return;

  m_impl->volume = std::max(0.0f, volume);
  ma_sound_set_volume(&m_impl->sound, m_impl->volume);
}

f32 Music::volume() const { return (m_impl != nullptr) ? m_impl->volume : 0.0f; }

void Music::setPitch(const f32 pitch) {
  if (!isValid()) return;

  m_impl->pitch = std::clamp(pitch, 0.01f, 8.0f);
  ma_sound_set_pitch(&m_impl->sound, m_impl->pitch);
}

void Music::setPan(const f32 pan) {
  if (!isValid()) return;

  m_impl->pan = std::clamp(pan, -1.0f, 1.0f);
  ma_sound_set_pan(&m_impl->sound, m_impl->pan);
}

void Music::setLooping(const bool looping) {
  if (!isValid()) return;

  m_impl->looping = looping;

  ma_sound_set_looping(&m_impl->sound, looping ? MA_TRUE : MA_FALSE);
}

bool Music::isLooping() const { return (m_impl != nullptr) && m_impl->looping; }

// =============================================================================
// AudioStream: eigener PCM-Strom (Ringpuffer als miniaudio-Datenquelle)
// =============================================================================
struct AudioStream::Impl {
  // Die Datenquelle muss mit ma_data_source_base beginnen
  struct Source {
    ma_data_source_base base{};
    std::mutex          mutex;
    std::vector<f32>    ring;
    std::uint64_t       readFrame{0};
    std::uint64_t       writeFrame{0};
    std::uint64_t       capacityFrames{0};
    std::uint64_t       consumedFrames{0};
    u32                 channels{2};
    u32                 sampleRate{48000};
  };

  Source   source;
  ma_sound sound{};
  bool     soundReady{false};
  f32      volume{1.0f};
  f32      pitch{1.0f};
  f32      pan{0.0f};
};

namespace {
  AudioStream::Impl::Source *SourceOf(ma_data_source *dataSource) {
    return reinterpret_cast<AudioStream::Impl::Source *>(dataSource);
  }

  ma_result SourceRead(ma_data_source *dataSource, void *framesOut, const ma_uint64 frameCount,
                       ma_uint64 *framesRead) {
    auto *source = SourceOf(dataSource);

    if ((source == nullptr) || (framesOut == nullptr)) return MA_INVALID_ARGS;

    std::lock_guard<std::mutex> lock(source->mutex);

    const auto     channels  = source->channels;
    const ma_uint64 available = static_cast<ma_uint64>(source->writeFrame - source->readFrame);
    const ma_uint64 toCopy    = std::min(frameCount, available);

    auto *output = static_cast<f32 *>(framesOut);

    for (ma_uint64 frame = 0; frame < toCopy; ++frame) {
      const auto index = static_cast<std::size_t>((source->readFrame + frame) % source->capacityFrames) * channels;

      for (u32 channel = 0; channel < channels; ++channel) output[(frame * channels) + channel] = source->ring[index + channel];
    }

    // Rest mit Stille fuellen: der Strom laeuft weiter, auch wenn nichts nachkommt
    if (toCopy < frameCount) {
      std::memset(output + (static_cast<std::size_t>(toCopy) * channels), 0,
                  static_cast<std::size_t>((frameCount - toCopy) * channels) * sizeof(f32));
    }

    source->readFrame += toCopy;
    source->consumedFrames += toCopy;

    if (framesRead != nullptr) *framesRead = frameCount;

    return MA_SUCCESS;
  }

  ma_result SourceSeek(ma_data_source *, ma_uint64) { return MA_NOT_IMPLEMENTED; }

  ma_result SourceGetDataFormat(ma_data_source *dataSource, ma_format *format, ma_uint32 *channels,
                                ma_uint32 *sampleRate, ma_channel *, size_t) {
    auto *source = SourceOf(dataSource);

    if (source == nullptr) return MA_INVALID_ARGS;

    if (format != nullptr) *format = ma_format_f32;
    if (channels != nullptr) *channels = source->channels;
    if (sampleRate != nullptr) *sampleRate = source->sampleRate;

    return MA_SUCCESS;
  }

  ma_result SourceGetCursor(ma_data_source *dataSource, ma_uint64 *cursor) {
    auto *source = SourceOf(dataSource);

    if ((source == nullptr) || (cursor == nullptr)) return MA_INVALID_ARGS;

    std::lock_guard<std::mutex> lock(source->mutex);

    *cursor = source->consumedFrames;

    return MA_SUCCESS;
  }

  ma_result SourceGetLength(ma_data_source *, ma_uint64 *length) {
    if (length != nullptr) *length = 0; // unbekannt: endloser Strom

    return MA_NOT_IMPLEMENTED;
  }

  ma_data_source_vtable g_streamSourceVtable = {SourceRead, SourceSeek, SourceGetDataFormat, SourceGetCursor,
                                                SourceGetLength, nullptr, 0};
} // namespace

AudioStream::AudioStream() : m_impl(std::make_unique<Impl>()) {}

AudioStream::~AudioStream() { unload(); }

AudioStream::AudioStream(AudioStream &&other) noexcept : m_impl(std::move(other.m_impl)) {}

AudioStream &AudioStream::operator=(AudioStream &&other) noexcept {
  if (this == &other) return *this;

  unload();

  m_impl = std::move(other.m_impl);

  return *this;
}

bool AudioStream::load(const u32 sampleRate, const u32 channels, const u32 bufferFrames) {
  unload();

  m_impl = std::make_unique<Impl>();

  if ((sampleRate == 0) || (channels == 0) || (bufferFrames == 0)) return false;

  m_impl->source.channels       = channels;
  m_impl->source.sampleRate     = sampleRate;
  m_impl->source.capacityFrames = bufferFrames;
  m_impl->source.ring.assign(static_cast<std::size_t>(bufferFrames) * channels, 0.0f);

  ma_data_source_config baseConfig = ma_data_source_config_init();

  baseConfig.vtable = &g_streamSourceVtable;

  if (ma_data_source_init(&baseConfig, &m_impl->source.base) != MA_SUCCESS) {
    SLOGE << "AudioStream: Datenquelle laesst sich nicht anlegen";

    return false;
  }

  AudioEngine &audio = Engine();

  if (!audio.ready) {
    WarnOnce("AudioStream: ohne Audiogeraet nicht abspielbar");

    return true; // Puffer nutzbar, Wiedergabe bleibt stumm
  }

  if (ma_sound_init_from_data_source(&audio.engine, &m_impl->source, 0, nullptr, &m_impl->sound) != MA_SUCCESS) {
    SLOGE << "AudioStream: Stimme laesst sich nicht anlegen";

    return false;
  }

  m_impl->soundReady = true;

  ma_sound_set_volume(&m_impl->sound, m_impl->volume);
  ma_sound_set_pitch(&m_impl->sound, m_impl->pitch);
  ma_sound_set_pan(&m_impl->sound, m_impl->pan);

  return true;
}

void AudioStream::unload() {
  AudioEngine &audio = Engine();

  if (m_impl != nullptr) {
    if (m_impl->soundReady && audio.ready) ma_sound_uninit(&m_impl->sound);
    if (m_impl->source.capacityFrames > 0) ma_data_source_uninit(&m_impl->source.base);

    m_impl->soundReady = false;
    m_impl = std::make_unique<Impl>();
  }
}

bool AudioStream::isValid() const { return (m_impl != nullptr) && (m_impl->source.capacityFrames > 0); }

bool AudioStream::push(const f32 *samples, const i32 frameCount) {
  if (!isValid() || (samples == nullptr) || (frameCount <= 0)) return false;

  auto &source = m_impl->source;

  std::lock_guard<std::mutex> lock(source.mutex);

  const auto free = source.capacityFrames - (source.writeFrame - source.readFrame);

  if (static_cast<std::uint64_t>(frameCount) > free) return false; // Puffer voll

  for (i32 frame = 0; frame < frameCount; ++frame) {
    const auto target = static_cast<std::size_t>((source.writeFrame + frame) % source.capacityFrames) * source.channels;

    for (u32 channel = 0; channel < source.channels; ++channel) {
      source.ring[target + channel] = samples[(static_cast<std::size_t>(frame) * source.channels) + channel];
    }
  }

  source.writeFrame += static_cast<std::uint64_t>(frameCount);

  return true;
}

void AudioStream::clear() {
  if (!isValid()) return;

  std::lock_guard<std::mutex> lock(m_impl->source.mutex);

  m_impl->source.readFrame  = 0;
  m_impl->source.writeFrame = 0;
}

i32 AudioStream::queuedFrames() const {
  if (!isValid()) return 0;

  std::lock_guard<std::mutex> lock(m_impl->source.mutex);

  return static_cast<i32>(m_impl->source.writeFrame - m_impl->source.readFrame);
}

i32 AudioStream::capacityFrames() const { return isValid() ? static_cast<i32>(m_impl->source.capacityFrames) : 0; }

i32 AudioStream::sampleRate() const { return isValid() ? static_cast<i32>(m_impl->source.sampleRate) : 0; }

i32 AudioStream::channels() const { return isValid() ? static_cast<i32>(m_impl->source.channels) : 0; }

void AudioStream::play() {
  if (!isValid() || !m_impl->soundReady) return;

  ma_sound_start(&m_impl->sound);
}

void AudioStream::pause() {
  if (!isValid() || !m_impl->soundReady) return;

  ma_sound_stop(&m_impl->sound);
}

void AudioStream::resume() {
  if (!isValid() || !m_impl->soundReady) return;

  ma_sound_start(&m_impl->sound);
}

void AudioStream::stop() {
  if (!isValid() || !m_impl->soundReady) return;

  ma_sound_stop(&m_impl->sound);
  clear();
}

bool AudioStream::isPlaying() const { return isValid() && m_impl->soundReady && (ma_sound_is_playing(&m_impl->sound) != MA_FALSE); }

void AudioStream::setVolume(const f32 volume) {
  m_impl->volume = std::max(0.0f, volume);
  if (isValid() && m_impl->soundReady) ma_sound_set_volume(&m_impl->sound, m_impl->volume);
}

void AudioStream::setPitch(const f32 pitch) {
  m_impl->pitch = std::clamp(pitch, 0.01f, 8.0f);
  if (isValid() && m_impl->soundReady) ma_sound_set_pitch(&m_impl->sound, m_impl->pitch);
}

void AudioStream::setPan(const f32 pan) {
  m_impl->pan = std::clamp(pan, -1.0f, 1.0f);
  if (isValid() && m_impl->soundReady) ma_sound_set_pan(&m_impl->sound, m_impl->pan);
}

void AudioStream::setLooping(const bool looping) {
  if (isValid() && m_impl->soundReady) ma_sound_set_looping(&m_impl->sound, looping ? MA_TRUE : MA_FALSE);
}

// =============================================================================
// Freie Funktionen
// =============================================================================
void InitAudioDevice() { (void)Engine(); }

void CloseAudioDevice() {
  AudioEngine &audio = Engine();

  if (!audio.ready) return;

  ma_engine_uninit(&audio.engine);
  audio.ready = false;
}

bool IsAudioDeviceReady() { return EngineReady(); }

void SetMasterVolume(const f32 volume) {
  AudioEngine &audio = Engine();

  audio.volume = std::max(0.0f, volume);

  if (audio.ready) ma_engine_set_volume(&audio.engine, audio.volume);
}

f32 GetMasterVolume() { return Engine().volume; }

Wave LoadWave(const std::string_view fileName) {
  Wave wave;

  if (!wave.loadFromFile(std::string(fileName))) return Wave{};

  return wave;
}

Wave LoadWaveFromMemory(const std::string_view fileType, const u8 *fileData, const i32 dataSize) {
  Wave wave;

  if ((fileData == nullptr) || (dataSize <= 0)) return Wave{};
  if (!wave.loadFromMemory(fileData, static_cast<std::size_t>(dataSize), std::string(fileType))) return Wave{};

  return wave;
}

bool IsWaveValid(const Wave &wave) { return wave.isValid(); }

bool ExportWave(const Wave &wave, const std::string_view fileName) {
  return wave.saveToFile(std::string(fileName));
}

bool ExportWaveAsCode(const Wave &wave, const std::string_view fileName) {
  return wave.saveAsCode(std::string(fileName));
}

Wave WaveCopy(const Wave &wave) { return wave.copy(); }

bool WaveCrop(Wave &wave, const i32 initFrame, const i32 finalFrame) { return wave.crop(initFrame, finalFrame); }

bool WaveFormat(Wave &wave, const i32 sampleRate, const i32 sampleSize, const i32 channels) {
  if (sampleSize != 32) {
    SLOGW << "WaveFormat: nur 32 Bit float wird unterstuetzt (angefragt: " << sampleSize << ")";

    return false;
  }

  return wave.convert(sampleRate, channels);
}

f32 *LoadWaveSamples(const Wave &wave) {
  if (!wave.isValid()) return nullptr;

  auto *copy = new f32[wave.samples().size()];

  std::memcpy(copy, wave.samples().data(), wave.samples().size() * sizeof(f32));

  return copy;
}

void UnloadWaveSamples(f32 *samples) { delete[] samples; }

Sound LoadSound(const std::string_view fileName) {
  Sound sound;

  if (!sound.loadFromFile(std::string(fileName))) return Sound{};

  return sound;
}

Sound LoadSound(const std::string_view fileName, const i32 voices) {
  Sound sound;

  if (!sound.loadFromFile(std::string(fileName), voices)) return Sound{};

  return sound;
}

Sound LoadSoundFromWave(const Wave &wave) {
  Sound sound;

  if (!sound.loadFromWave(wave)) return Sound{};

  return sound;
}

Sound LoadSoundFromWave(const Wave &wave, const i32 voices) {
  Sound sound;

  if (!sound.loadFromWave(wave, voices)) return Sound{};

  return sound;
}

Sound LoadSoundAlias(const Sound &source) {
  // eigene Kopie der Daten (kein geteilter Puffer -- dokumentiert)
  return LoadSoundFromWave(source.data());
}

bool IsSoundValid(const Sound &sound) { return sound.isValid(); }

void UnloadSound(Sound &sound) { sound.unload(); }

void PlaySound(Sound &sound) { sound.play(); }

void StopSound(Sound &sound) { sound.stop(); }

void PauseSound(Sound &sound) { sound.pause(); }

void ResumeSound(Sound &sound) { sound.resume(); }

bool IsSoundPlaying(const Sound &sound) { return sound.isPlaying(); }

void SetSoundVolume(Sound &sound, const f32 volume) { sound.setVolume(volume); }

void SetSoundPitch(Sound &sound, const f32 pitch) { sound.setPitch(pitch); }

void SetSoundPan(Sound &sound, const f32 pan) { sound.setPan(pan); }

Music LoadMusicStream(const std::string_view fileName) {
  Music music;

  if (!music.loadFromFile(std::string(fileName))) return Music{};

  return music;
}

Music LoadMusicStreamFromMemory(const std::string_view fileType, const u8 *data, const i32 dataSize) {
  Music music;

  if ((data == nullptr) || (dataSize <= 0)) return Music{};
  if (!music.loadFromMemory(data, static_cast<std::size_t>(dataSize), std::string(fileType))) return Music{};

  return music;
}

bool IsMusicValid(const Music &music) { return music.isValid(); }

void UnloadMusicStream(Music &music) { music.unload(); }

void PlayMusicStream(Music &music) { music.play(); }

bool IsMusicStreamPlaying(const Music &music) { return music.isPlaying(); }

void UpdateMusicStream(Music &music) { music.update(); }

void StopMusicStream(Music &music) { music.stop(); }

void PauseMusicStream(Music &music) { music.pause(); }

void ResumeMusicStream(Music &music) { music.resume(); }

void SeekMusicStream(Music &music, const f32 position) { music.seek(position); }

void SetMusicVolume(Music &music, const f32 volume) { music.setVolume(volume); }

void SetMusicPitch(Music &music, const f32 pitch) { music.setPitch(pitch); }

void SetMusicPan(Music &music, const f32 pan) { music.setPan(pan); }

f32 GetMusicTimeLength(const Music &music) { return music.timeLength(); }

f32 GetMusicTimePlayed(const Music &music) { return music.timePlayed(); }

AudioStream LoadAudioStream(const u32 sampleRate, const u32 sampleSize, const u32 channels) {
  AudioStream stream;

  if (sampleSize != 32) {
    SLOGW << "LoadAudioStream: nur 32 Bit float (angefragt: " << sampleSize << ")";

    return AudioStream{};
  }

  if (!stream.load(sampleRate, channels)) return AudioStream{};

  return stream;
}

bool IsAudioStreamValid(const AudioStream &stream) { return stream.isValid(); }

void UnloadAudioStream(AudioStream &stream) { stream.unload(); }

void UpdateAudioStream(AudioStream &stream, const void *data, const i32 frameCount) {
  if ((data == nullptr) || (frameCount <= 0)) return;

  stream.push(static_cast<const f32 *>(data), frameCount);
}

bool IsAudioStreamProcessed(const AudioStream &stream) {
  return stream.isValid() && (stream.queuedFrames() == 0);
}

void PlayAudioStream(AudioStream &stream) { stream.play(); }

void PauseAudioStream(AudioStream &stream) { stream.pause(); }

void ResumeAudioStream(AudioStream &stream) { stream.resume(); }

bool IsAudioStreamPlaying(const AudioStream &stream) { return stream.isPlaying(); }

void StopAudioStream(AudioStream &stream) { stream.stop(); }

void SetAudioStreamVolume(AudioStream &stream, const f32 volume) { stream.setVolume(volume); }

void SetAudioStreamPitch(AudioStream &stream, const f32 pitch) { stream.setPitch(pitch); }

void SetAudioStreamPan(AudioStream &stream, const f32 pan) { stream.setPan(pan); }

void SetAudioStreamBufferSizeDefault(const i32 size) { (void)size; }

void SetAudioStreamCallback(AudioStream &stream, AudioCallback callback) {
  (void)stream;
  (void)callback;
  WarnOnce("Audio: SetAudioStreamCallback wird von diesem Backend nicht unterstuetzt (Puffer selbst fuellen)");
}

void AttachAudioStreamProcessor(AudioStream &stream, AudioCallback processor) {
  (void)stream;
  (void)processor;
  WarnOnce("Audio: AttachAudioStreamProcessor wird von diesem Backend nicht unterstuetzt");
}

void DetachAudioStreamProcessor(AudioStream &stream, AudioCallback processor) {
  (void)stream;
  (void)processor;
}

void AttachAudioMixedProcessor(AudioCallback processor) {
  (void)processor;
  WarnOnce("Audio: AttachAudioMixedProcessor wird von diesem Backend nicht unterstuetzt");
}

void DetachAudioMixedProcessor(AudioCallback processor) { (void)processor; }

}  // namespace sgl
