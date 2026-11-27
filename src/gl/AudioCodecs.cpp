#include "gl/AudioCodecs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>

// QOA stellt seine Deklarationen C++-tauglich bereit. Die Implementierung liegt
// in third_party/audio/qoa_impl.c.
#include "qoa.h"

// jar_xm.h ist ein C-Header ohne extern "C"-Block. Die Implementierung liegt in
// third_party/audio/jar_xm_impl.c; hier werden nur die benoetigten Funktionen
// mit C-Bindung deklariert.
extern "C" {
typedef struct jar_xm_context_s jar_xm_context_t;

int jar_xm_create_context_from_file(jar_xm_context_t** context, uint32_t rate,
                                    const char* filename);
int jar_xm_create_context_safe(jar_xm_context_t** context, const char* moduleData,
                               size_t moduleDataLength, uint32_t rate);
void jar_xm_free_context(jar_xm_context_t* context);
void jar_xm_generate_samples(jar_xm_context_t* context, float* output,
                             size_t sampleCount);
uint64_t jar_xm_get_remaining_samples(jar_xm_context_t* context);
void jar_xm_reset(jar_xm_context_t* context);
void jar_xm_set_max_loop_count(jar_xm_context_t* context, uint8_t loopCount);
}

namespace sgl::detail {

namespace {

constexpr f32 kPcmScale = 32768.0f;

bool ReadFileBytes(const std::string& path, std::vector<u8>& bytes) {
  bytes.clear();

  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) return false;

  const std::streamoff size = stream.tellg();
  if (size <= 0) return false;

  bytes.resize(static_cast<std::size_t>(size));
  stream.seekg(0, std::ios::beg);

  if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) {
    bytes.clear();
    return false;
  }

  return true;
}

bool DecodeXmContext(jar_xm_context_t* context, std::vector<f32>& samples,
                     i32& sampleRate, i32& channels, const u32 renderRate) {
  if (context == nullptr) return false;

  // Einmal die Länge eines Durchlaufs bestimmen. jar_xm_get_remaining_samples()
  // läuft dafür bis zum Loop-Punkt und wird danach zurückgesetzt.
  jar_xm_set_max_loop_count(context, 0);
  const u64 frames = jar_xm_get_remaining_samples(context);
  jar_xm_reset(context);
  jar_xm_set_max_loop_count(context, 0);

  if (frames == 0 || frames > static_cast<u64>(std::numeric_limits<i32>::max())) {
    jar_xm_free_context(context);
    return false;
  }

  const auto frameCount = static_cast<std::size_t>(frames);
  if (frameCount > (std::numeric_limits<std::size_t>::max() / 2)) {
    jar_xm_free_context(context);
    return false;
  }

  samples.resize(frameCount * 2);
  jar_xm_generate_samples(context, samples.data(), frameCount);
  jar_xm_free_context(context);

  sampleRate = static_cast<i32>(renderRate);
  channels   = 2;

  return true;
}

}  // namespace

// ─── QOA ─────────────────────────────────────────────────────────────────────

bool IsQoaMemory(const u8* data, const std::size_t size) noexcept {
  return data != nullptr && size >= 4 && std::memcmp(data, "qoaf", 4) == 0;
}

bool DecodeQoaMemory(const u8* data, const std::size_t size,
                     std::vector<f32>& samples, i32& sampleRate, i32& channels) {
  samples.clear();
  sampleRate = 0;
  channels   = 0;

  if (!IsQoaMemory(data, size) || size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  qoa_desc description{};
  short* pcm = qoa_decode(data, static_cast<int>(size), &description);
  if (pcm == nullptr) return false;

  const std::size_t count =
      static_cast<std::size_t>(description.samples) * static_cast<std::size_t>(description.channels);

  samples.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    samples[i] = static_cast<f32>(pcm[i]) / kPcmScale;
  }

  std::free(pcm);

  sampleRate = static_cast<i32>(description.samplerate);
  channels   = static_cast<i32>(description.channels);

  return !samples.empty() && sampleRate > 0 && channels > 0;
}

bool DecodeQoaFile(const std::string& path, std::vector<f32>& samples,
                   i32& sampleRate, i32& channels) {
  std::vector<u8> bytes;
  if (!ReadFileBytes(path, bytes)) return false;

  return DecodeQoaMemory(bytes.data(), bytes.size(), samples, sampleRate, channels);
}

bool EncodeQoaFile(const std::string& path, const std::vector<f32>& samples,
                   const i32 sampleRate, const i32 channels) {
  if (samples.empty() || sampleRate <= 0 || channels <= 0 ||
      samples.size() % static_cast<std::size_t>(channels) != 0) {
    return false;
  }

  std::vector<short> pcm(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const f32 clamped = std::clamp(samples[i], -1.0f, 1.0f);
    pcm[i] = static_cast<short>(std::lround(clamped * 32767.0f));
  }

  qoa_desc description{};
  description.channels   = static_cast<unsigned int>(channels);
  description.samplerate = static_cast<unsigned int>(sampleRate);
  description.samples    = static_cast<unsigned int>(samples.size() / static_cast<std::size_t>(channels));

  return qoa_write(path.c_str(), pcm.data(), &description) > 0;
}

// ─── XM ──────────────────────────────────────────────────────────────────────

bool IsXmMemory(const u8* data, const std::size_t size) noexcept {
  constexpr char kMagic[] = "Extended Module: ";
  return data != nullptr && size >= sizeof(kMagic) - 1 &&
         std::memcmp(data, kMagic, sizeof(kMagic) - 1) == 0;
}

bool DecodeXmMemory(const u8* data, const std::size_t size, std::vector<f32>& samples,
                    i32& sampleRate, i32& channels, const u32 renderRate) {
  samples.clear();
  sampleRate = 0;
  channels   = 0;

  if (!IsXmMemory(data, size)) return false;

  jar_xm_context_t* context = nullptr;
  if (jar_xm_create_context_safe(&context, reinterpret_cast<const char*>(data), size,
                                 renderRate) != 0) {
    jar_xm_free_context(context);
    return false;
  }

  return DecodeXmContext(context, samples, sampleRate, channels, renderRate);
}

bool DecodeXmFile(const std::string& path, std::vector<f32>& samples,
                  i32& sampleRate, i32& channels, const u32 renderRate) {
  samples.clear();
  sampleRate = 0;
  channels   = 0;

  jar_xm_context_t* context = nullptr;
  if (jar_xm_create_context_from_file(&context, renderRate, path.c_str()) != 0) {
    jar_xm_free_context(context);
    return false;
  }

  return DecodeXmContext(context, samples, sampleRate, channels, renderRate);
}

}  // namespace sgl::detail
