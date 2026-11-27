#pragma once

// Interne Decoder für Formate, die miniaudio nicht mitbringt:
//   QOA  -> qoa.h (MIT)
//   XM   -> jar_xm.h (WTFPL v2)
//
// Die Implementierung liegt in AudioCodecs.cpp; die Third-Party-Header werden
// nur dort eingebunden. Alle Ausgaben sind 32-Bit-Float, verschachtelt.

#include <cstddef>
#include <string>
#include <vector>

#include <sgl/Types.hpp>

namespace sgl::detail {

// ─── QOA ─────────────────────────────────────────────────────────────────────
[[nodiscard]] bool IsQoaMemory(const u8* data, std::size_t size) noexcept;

[[nodiscard]] bool DecodeQoaMemory(const u8* data, std::size_t size,
                                   std::vector<f32>& samples, i32& sampleRate,
                                   i32& channels);

[[nodiscard]] bool DecodeQoaFile(const std::string& path,
                                 std::vector<f32>& samples, i32& sampleRate,
                                 i32& channels);

[[nodiscard]] bool EncodeQoaFile(const std::string& path,
                                 const std::vector<f32>& samples, i32 sampleRate,
                                 i32 channels);

// ─── XM ──────────────────────────────────────────────────────────────────────
[[nodiscard]] bool IsXmMemory(const u8* data, std::size_t size) noexcept;

// Dekodiert ein Modul einmal vollständig nach Stereo-PCM. XM ist kein
// Sample-Format wie WAV, sondern ein Tracker-Song; deshalb ist das Ergebnis
// immer stereo und wird mit der angegebenen Rate gerendert.
[[nodiscard]] bool DecodeXmMemory(const u8* data, std::size_t size,
                                  std::vector<f32>& samples, i32& sampleRate,
                                  i32& channels, u32 renderRate = 48000);

[[nodiscard]] bool DecodeXmFile(const std::string& path,
                                std::vector<f32>& samples, i32& sampleRate,
                                i32& channels, u32 renderRate = 48000);

}  // namespace sgl::detail
