#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Noise
//
// Rauschtexturen per Compute-Shader (Perlin/Simplex, 1D/2D/3D).
//
//   NoiseType: Perlin, Simplex
//   NoiseConfig { type, dimension, width, height, depth, octaves, lacunarity,
//                 gain, frequency, period, isTileable, seed }
//   Noise: create, setConfig, unload, isValid, generate, setSize,
//          setSeed/seed, setOctaves/octaves, setFrequency/frequency,
//          setPeriod/period, setTileable/isTileable, setLacunarity/lacunarity,
//          setGain/gain, setType/type, setDimension/dimension,
//          textureId, width, height, depth, is3D, toImage
//
// Kanäle der RGBA16F-Textur: R = fBm, G = zweite Variante, B = ridged,
// A = 1. Kachelbar entsteht durch ein periodisches Hash-Gitter.
//
// Beispiel:
//
//   sgl::NoiseConfig config;
//   config.dimension = 2;
//   config.isTileable = true;
//   sgl::Noise noise;
//   noise.create(config);
//   sgl::Image image = noise.toImage();
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

#include "Image.hpp"
#include "Shader.hpp"

namespace sgl {

enum class NoiseType { Perlin = 0, Simplex = 1 };

struct NoiseConfig {
    NoiseType type{NoiseType::Perlin};
    i32 dimension{2};  // 1, 2 oder 3
    i32 width{256};
    i32 height{256};
    i32 depth{16};

    i32 octaves{4};
    f32 lacunarity{2.0f};
    f32 gain{0.5f};

    f32 frequency{4.0f};
    i32 period{8};
    bool isTileable{false};

    u32 seed{1337};
};

class Noise {
public:
    Noise() = default;
    ~Noise();

    Noise(const Noise&) = delete;
    Noise& operator=(const Noise&) = delete;
    Noise(Noise&&) noexcept = default;
    Noise& operator=(Noise&&) noexcept = default;

    bool create();
    bool create(const NoiseConfig& config);
    // Wie create(); paart mit config().
    bool setConfig(const NoiseConfig& config) { return create(config); }

    void unload();
    [[nodiscard]] bool isValid() const noexcept { return m_texture != 0 && m_program.isValid(); }

    bool generate();
    // Neue Texturgröße; paart mit width(), height() und depth().
    bool setSize(const i32 width, const i32 height, const i32 depth);

    // ── Einstellungen ───────────────────────────────────────────────────────
    void setSeed(const u32 seed);
    [[nodiscard]] u32 seed() const noexcept { return m_config.seed; }
    void setOctaves(const i32 octaves);
    [[nodiscard]] i32 octaves() const noexcept { return m_config.octaves; }
    void setFrequency(const f32 frequency);
    [[nodiscard]] f32 frequency() const noexcept { return m_config.frequency; }
    void setPeriod(const i32 period);
    [[nodiscard]] i32 period() const noexcept { return m_config.period; }
    void setTileable(const bool isTileable);
    [[nodiscard]] bool isTileable() const noexcept { return m_config.isTileable; }
    void setLacunarity(const f32 lacunarity);
    [[nodiscard]] f32 lacunarity() const noexcept { return m_config.lacunarity; }
    void setGain(const f32 gain);
    [[nodiscard]] f32 gain() const noexcept { return m_config.gain; }
    void setType(const NoiseType type);
    [[nodiscard]] NoiseType type() const noexcept { return m_config.type; }
    void setDimension(const i32 dimension);
    [[nodiscard]] i32 dimension() const noexcept { return m_config.dimension; }

    [[nodiscard]] const NoiseConfig& config() const noexcept { return m_config; }

    // ── Ergebnis ────────────────────────────────────────────────────────────
    [[nodiscard]] u32 textureId() const noexcept { return m_texture; }
    [[nodiscard]] i32 width() const noexcept { return m_config.width; }
    [[nodiscard]] i32 height() const noexcept { return m_config.height; }
    [[nodiscard]] i32 depth() const noexcept { return m_config.depth; }
    [[nodiscard]] bool is3D() const noexcept { return m_config.dimension == 3; }

    // Werte aus der Textur lesen (RGBA16F -> 8 Bit, R/G/B wie im Shader).
    [[nodiscard]] Image toImage() const;

private:
    bool buildProgram();
    bool runCompute();

    NoiseConfig m_config{};
    u32 m_texture{0};
    Shader m_program{};
    i32 m_programDimension{0};
    i32 m_programType{-1};
};

}  // namespace sgl
