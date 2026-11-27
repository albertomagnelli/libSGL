#pragma once
// sgl · Audio (Wave, Sound, Music, AudioStream)
//
// Backend ist miniaudio (eingebettet in third_party/audio, kein externes
// Paket): Wave/FLAC/MP3 direkt, Ogg-Vorbis ueber stb_vorbis.
//
//   Wave    : dekodierte PCM-Daten im Speicher (32 Bit float, verschachtelt)
//   Sound   : kurzer Effekt, mehrere Stimmen gleichzeitig (Standard 8),
//             Daten liegen vollstaendig im Speicher
//   Music   : gestreamt aus der Datei, eine Wiedergabe, mit Position/Seek
//   AudioStream : eigener Datenstrom, den der Aufrufer selbst fuellt
//
// miniaudio bleibt in der Implementierung (pimpl) -- die oeffentlichen Header
// ziehen die 96k-Zeilen-Bibliothek nicht mit.

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Types.hpp"

namespace sgl {

//------------------------------------------------------------------------------------
// Wave: dekodierte Audiodaten
//------------------------------------------------------------------------------------
class Wave {
public:
  Wave() = default;
  Wave(const Wave &)            = default;
  Wave &operator=(const Wave &) = default;
  Wave(Wave &&) noexcept        = default;
  Wave &operator=(Wave &&) noexcept = default;
  ~Wave() = default;

  bool LoadFromFile(const std::string &path);
  bool LoadFromMemory(const u8 *data, std::size_t size, const std::string &extension = std::string());

  // Rohdaten setzen (muss zu Channele/ Samplerate passen)
  bool SetSamples(std::vector<f32> samples, i32 sampleRate, i32 channels);

  void Unload();
  bool IsValid() const;

  i32 FrameCount() const;  // Anzahl Rahmen
  i32 SampleRate() const;  // Rahmen pro Sekunde
  i32 Channels() const;    // 1 = mono, 2 = stereo
  i32 SampleSize() const;  // Bytes je Sample (immer 4 bei 32 Bit float)
  i32 SampleCount() const; // Rahmen * Kanaele

  f32 Duration() const; // Sekunden

  const std::vector<f32> &Samples() const;
  std::vector<f32>       &Samples();

  Wave Copy() const;

  // Rahmenbereich behalten (initFrame..finalFrame, beide inklusive)
  bool Crop(i32 firstFrame, i32 lastFrame);

  // Samplerate/Kanaele umrechnen (sampleSize muss 32 sein)
  bool Convert(i32 sampleRate, i32 channels);

  // Als 16-Bit-PCM-WAV schreiben
  bool SaveToFile(const std::string &path) const;

  // Als C-Header mit float-Array schreiben
  bool SaveAsCode(const std::string &path, const std::string &variableName = "waveSamples") const;

private:
  std::vector<f32> m_samples;
  i32              m_sampleRate{};
  i32              m_channels{};
};

//------------------------------------------------------------------------------------
// Sound: kurzer Effekt mit mehreren Stimmen
//------------------------------------------------------------------------------------
class Sound {
public:
  Sound();
  ~Sound();
  Sound(Sound &&) noexcept;
  Sound &operator=(Sound &&) noexcept;
  Sound(const Sound &)            = delete;
  Sound &operator=(const Sound &) = delete;

  bool LoadFromFile(const std::string &path, i32 voices = 8);
  bool LoadFromWave(const Wave &wave, i32 voices = 8);

  void Unload();
  bool IsValid() const; // Daten dekodiert (Wiedergabe braucht zusaetzlich ein Geraet)

  const Wave &Data() const;
  f32         Duration() const;
  i32         VoiceCount() const;

  void Play();
  void Stop();
  void Pause();
  void Resume();
  bool IsPlaying() const;

  void SetVolume(f32 volume); // 1.0 = normal
  f32  Volume() const;
  void SetPitch(f32 pitch); // 1.0 = Originaltonhoehe
  f32  Pitch() const;
  void SetPan(f32 pan); // -1.0 links .. 1.0 rechts
  f32  Pan() const;
  void SetLooping(bool looping);
  bool IsLooping() const;

  struct Impl; // intern (Definition in Audio.cpp)

private:
  std::unique_ptr<Impl> m_impl;
  Wave                  m_wave;
};

//------------------------------------------------------------------------------------
// Music: gestreamte Wiedergabe
//------------------------------------------------------------------------------------
class Music {
public:
  Music();
  ~Music();
  Music(Music &&) noexcept;
  Music &operator=(Music &&) noexcept;
  Music(const Music &)            = delete;
  Music &operator=(const Music &) = delete;

  bool LoadFromFile(const std::string &path);
  bool LoadFromMemory(const u8 *data, std::size_t size, const std::string &extension = std::string());

  void Unload();
  bool IsValid() const;
  bool IsStreaming() const; // true = aus der Datei gestreamt

  void Play();
  void Stop();
  void Pause();
  void Resume();
  bool IsPlaying() const;
  bool IsPaused() const;
  void Update(); // nicht noetig (miniaudio streamt selbst), bleibt fuer raylib-Aufrufe

  void Seek(f32 seconds);
  f32  TimeLength() const; // Sekunden
  f32  TimePlayed() const;

  void SetVolume(f32 volume);
  f32  Volume() const;
  void SetPitch(f32 pitch);
  void SetPan(f32 pan);
  void SetLooping(bool looping);
  bool IsLooping() const;

  struct Impl; // intern (Definition in Audio.cpp)

private:
  std::unique_ptr<Impl> m_impl;
};

//------------------------------------------------------------------------------------
// AudioStream: eigener PCM-Strom, den der Aufrufer fuellt
//------------------------------------------------------------------------------------
class AudioStream {
public:
  AudioStream();
  ~AudioStream();
  AudioStream(AudioStream &&) noexcept;
  AudioStream &operator=(AudioStream &&) noexcept;
  AudioStream(const AudioStream &)            = delete;
  AudioStream &operator=(const AudioStream &) = delete;

  bool Load(u32 sampleRate, u32 channels = 2, u32 bufferFrames = 8192);
  void Unload();
  bool IsValid() const;

  // Rahmen (interleaved) anhaengen; gibt false zurueck, wenn der Puffer voll ist
  bool Push(const f32 *samples, i32 frameCount);
  void Clear();
  i32  QueuedFrames() const;
  i32  CapacityFrames() const;
  i32  SampleRate() const;
  i32  Channels() const;

  void Play();
  void Pause();
  void Resume();
  void Stop();
  bool IsPlaying() const;

  void SetVolume(f32 volume);
  void SetPitch(f32 pitch);
  void SetPan(f32 pan);
  void SetLooping(bool looping);

  struct Impl; // intern (Definition in Audio.cpp)

private:
  std::unique_ptr<Impl> m_impl;
};

//------------------------------------------------------------------------------------
// Freie Funktionen
//------------------------------------------------------------------------------------
void InitAudioDevice();
void CloseAudioDevice();
bool IsAudioDeviceReady();
void SetMasterVolume(f32 volume);
f32  GetMasterVolume();

Wave LoadWave(std::string_view fileName);
Wave LoadWaveFromMemory(std::string_view fileType, const u8 *fileData, i32 dataSize);
bool IsWaveValid(const Wave &wave);
bool ExportWave(const Wave &wave, std::string_view fileName);
bool ExportWaveAsCode(const Wave &wave, std::string_view fileName);
Wave WaveCopy(const Wave &wave);
bool WaveCrop(Wave &wave, i32 initFrame, i32 finalFrame);
bool WaveFormat(Wave &wave, i32 sampleRate, i32 sampleSize, i32 channels);
f32 *LoadWaveSamples(const Wave &wave);
void UnloadWaveSamples(f32 *samples);

Sound LoadSound(std::string_view fileName);
Sound LoadSound(std::string_view fileName, i32 voices);
Sound LoadSoundFromWave(const Wave &wave);
Sound LoadSoundFromWave(const Wave &wave, i32 voices);
Sound LoadSoundAlias(const Sound &source);
bool  IsSoundValid(const Sound &sound);
void  UnloadSound(Sound &sound);
void  PlaySound(Sound &sound);
void  StopSound(Sound &sound);
void  PauseSound(Sound &sound);
void  ResumeSound(Sound &sound);
bool  IsSoundPlaying(const Sound &sound);
void  SetSoundVolume(Sound &sound, f32 volume);
void  SetSoundPitch(Sound &sound, f32 pitch);
void  SetSoundPan(Sound &sound, f32 pan);

Music LoadMusicStream(std::string_view fileName);
Music LoadMusicStreamFromMemory(std::string_view fileType, const u8 *data, i32 dataSize);
bool  IsMusicValid(const Music &music);
void  UnloadMusicStream(Music &music);
void  PlayMusicStream(Music &music);
bool  IsMusicStreamPlaying(const Music &music);
void  UpdateMusicStream(Music &music);
void  StopMusicStream(Music &music);
void  PauseMusicStream(Music &music);
void  ResumeMusicStream(Music &music);
void  SeekMusicStream(Music &music, f32 position);
void  SetMusicVolume(Music &music, f32 volume);
void  SetMusicPitch(Music &music, f32 pitch);
void  SetMusicPan(Music &music, f32 pan);
f32   GetMusicTimeLength(const Music &music); // Sekunden
f32   GetMusicTimePlayed(const Music &music); // Sekunden

AudioStream LoadAudioStream(u32 sampleRate, u32 sampleSize, u32 channels);
bool        IsAudioStreamValid(const AudioStream &stream);
void        UnloadAudioStream(AudioStream &stream);
void        UpdateAudioStream(AudioStream &stream, const void *data, i32 frameCount);
bool        IsAudioStreamProcessed(const AudioStream &stream);
void        PlayAudioStream(AudioStream &stream);
void        PauseAudioStream(AudioStream &stream);
void        ResumeAudioStream(AudioStream &stream);
bool        IsAudioStreamPlaying(const AudioStream &stream);
void        StopAudioStream(AudioStream &stream);
void        SetAudioStreamVolume(AudioStream &stream, f32 volume);
void        SetAudioStreamPitch(AudioStream &stream, f32 pitch);
void        SetAudioStreamPan(AudioStream &stream, f32 pan);
void        SetAudioStreamBufferSizeDefault(i32 size);

// Klangverarbeitung (raylib-API): von diesem Backend nicht unterstuetzt, die
// Funktionen melden das einmal im Log.
using AudioCallback = void (*)(void *bufferData, u32 frames);
void SetAudioStreamCallback(AudioStream &stream, AudioCallback callback);
void AttachAudioStreamProcessor(AudioStream &stream, AudioCallback processor);
void DetachAudioStreamProcessor(AudioStream &stream, AudioCallback processor);
void AttachAudioMixedProcessor(AudioCallback processor);
void DetachAudioMixedProcessor(AudioCallback processor);

}  // namespace sgl
