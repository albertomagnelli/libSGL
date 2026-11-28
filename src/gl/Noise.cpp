#include <sgl/Noise.hpp>

#include <sgl/GL.hpp>
#include <sgl/Log.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sgl {

using namespace gl;

namespace {

constexpr const char* kNoiseComputeShader = R"NOISE(#version 460 core
// Perlin- oder Simplex-Rauschen, 1D/2D/3D -- Variante per Define gesetzt
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

#if DIMENSION == 1
layout(binding = 0, rgba16f) writeonly uniform image1D imgOut;
#elif DIMENSION == 2
layout(binding = 0, rgba16f) writeonly uniform image2D imgOut;
#else
layout(binding = 0, rgba16f) writeonly uniform image3D imgOut;
#endif

uniform ivec3 u_size;
uniform float u_scale;
uniform float u_lacunarity;
uniform float u_gain;
uniform int   u_octaves;
uniform uint  u_seed;
uniform int   u_tileable;

const float TWO_PI = 6.283185307179586;

uint HashUint(uint value) {
  value ^= value >> 17u;
  value *= 0xed5ad4bbu;
  value ^= value >> 11u;
  value *= 0xac4c1b51u;
  value ^= value >> 15u;
  return value;
}

uint HashCell(ivec3 cell, const int period, uint seed) {
  if (period > 0) cell = ivec3(mod(vec3(cell), vec3(float(period))));

  const uint hash = uint(cell.x + 4096)*1597334677u ^ uint(cell.y + 4096)*3812015801u ^
                    uint(cell.z + 4096)*2798796415u;

  return HashUint(hash ^ (seed*2654435761u));
}

float Hash01(const ivec3 cell, const int period, uint seed) {
  return float(HashCell(cell, period, seed) & 0xFFFFFFu)/16777216.0;
}

vec2 Gradient2(const ivec3 cell, const int period, uint seed) {
  const float angle = Hash01(cell, period, seed)*TWO_PI;
  return vec2(cos(angle), sin(angle));
}

vec3 Gradient3(const ivec3 cell, const int period, uint seed) {
  switch (HashCell(cell, period, seed) % 12u) {
    case 0u: return vec3(1.0, 1.0, 0.0);
    case 1u: return vec3(-1.0, 1.0, 0.0);
    case 2u: return vec3(1.0, -1.0, 0.0);
    case 3u: return vec3(-1.0, -1.0, 0.0);
    case 4u: return vec3(1.0, 0.0, 1.0);
    case 5u: return vec3(-1.0, 0.0, 1.0);
    case 6u: return vec3(1.0, 0.0, -1.0);
    case 7u: return vec3(-1.0, 0.0, -1.0);
    case 8u: return vec3(0.0, 1.0, 1.0);
    case 9u: return vec3(0.0, -1.0, 1.0);
    case 10u: return vec3(0.0, 1.0, -1.0);
    default: return vec3(0.0, -1.0, -1.0);
  }
}

float Fade(const float t) { return t*t*t*(t*(t*6.0 - 15.0) + 10.0); }

float Perlin1(const float x, const int period, uint seed) {
  const int cell = int(floor(x));
  const float fraction = x - float(cell);
  const float fade = Fade(fraction);
  const float g0 = Hash01(ivec3(cell, 0, 0), period, seed)*2.0 - 1.0;
  const float g1 = Hash01(ivec3(cell + 1, 0, 0), period, seed)*2.0 - 1.0;
  return mix(g0*fraction, g1*(fraction - 1.0), fade)*2.0;
}

float Perlin2(const vec2 p, const int period, uint seed) {
  const ivec2 cell = ivec2(floor(p));
  const vec2 fraction = fract(p);
  const vec2 fade = vec2(Fade(fraction.x), Fade(fraction.y));
  const ivec3 base = ivec3(cell, 0);

  const float n00 = dot(Gradient2(base, period, seed), fraction - vec2(0.0, 0.0));
  const float n10 = dot(Gradient2(base + ivec3(1, 0, 0), period, seed), fraction - vec2(1.0, 0.0));
  const float n01 = dot(Gradient2(base + ivec3(0, 1, 0), period, seed), fraction - vec2(0.0, 1.0));
  const float n11 = dot(Gradient2(base + ivec3(1, 1, 0), period, seed), fraction - vec2(1.0, 1.0));

  return mix(mix(n00, n10, fade.x), mix(n01, n11, fade.x), fade.y)*1.4142;
}

float Perlin3(const vec3 p, const int period, uint seed) {
  const ivec3 cell = ivec3(floor(p));
  const vec3 fraction = fract(p);
  const vec3 fade = vec3(Fade(fraction.x), Fade(fraction.y), Fade(fraction.z));

  float values[8];
  for (int index = 0; index < 8; ++index) {
    const ivec3 offset = ivec3(index & 1, (index >> 1) & 1, (index >> 2) & 1);
    values[index] = dot(Gradient3(cell + offset, period, seed), fraction - vec3(offset));
  }

  const float lower = mix(mix(values[0], values[1], fade.x), mix(values[2], values[3], fade.x), fade.y);
  const float upper = mix(mix(values[4], values[5], fade.x), mix(values[6], values[7], fade.x), fade.y);
  return mix(lower, upper, fade.z)*1.1547;
}

float Simplex1(const float x, const int period, uint seed) { return Perlin1(x, period, seed); }

float Simplex2(const vec2 p, const int period, uint seed) {
  const float skewFactor = 0.36602540378;
  const float unskewFactor = 0.2113248654;

  const vec2 skewed = p + ((p.x + p.y)*skewFactor);
  const vec2 cell = floor(skewed);
  const vec2 origin = cell - ((cell.x + cell.y)*unskewFactor);
  const vec2 d0 = p - origin;

  const vec2 offset1 = (d0.x > d0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  const vec2 d1 = d0 - offset1 + unskewFactor;
  const vec2 d2 = d0 - 1.0 + (2.0*unskewFactor);
  const ivec3 base = ivec3(ivec2(cell), 0);

  float sum = 0.0;
  const float t0 = 0.5 - dot(d0, d0);
  if (t0 > 0.0) sum += t0*t0*t0*t0*dot(Gradient2(base, period, seed), d0);
  const float t1 = 0.5 - dot(d1, d1);
  if (t1 > 0.0) sum += t1*t1*t1*t1*dot(Gradient2(base + ivec3(ivec2(offset1), 0), period, seed), d1);
  const float t2 = 0.5 - dot(d2, d2);
  if (t2 > 0.0) sum += t2*t2*t2*t2*dot(Gradient2(base + ivec3(1, 1, 0), period, seed), d2);

  return 70.0*sum;
}

float Simplex3(const vec3 p, const int period, uint seed) {
  const float skewFactor = 1.0/3.0;
  const float unskewFactor = 1.0/6.0;

  const float skew = (p.x + p.y + p.z)*skewFactor;
  const vec3 cell = floor(p + skew);
  const float unskew = (cell.x + cell.y + cell.z)*unskewFactor;
  const vec3 d0 = p - (cell - unskew);

  vec3 offset1;
  vec3 offset2;

  if (d0.x >= d0.y) {
    if (d0.y >= d0.z) {
      offset1 = vec3(1.0, 0.0, 0.0);
      offset2 = vec3(1.0, 1.0, 0.0);
    } else if (d0.x >= d0.z) {
      offset1 = vec3(1.0, 0.0, 0.0);
      offset2 = vec3(1.0, 0.0, 1.0);
    } else {
      offset1 = vec3(0.0, 0.0, 1.0);
      offset2 = vec3(1.0, 0.0, 1.0);
    }
  } else {
    if (d0.y < d0.z) {
      offset1 = vec3(0.0, 0.0, 1.0);
      offset2 = vec3(0.0, 1.0, 1.0);
    } else if (d0.x < d0.z) {
      offset1 = vec3(0.0, 1.0, 0.0);
      offset2 = vec3(0.0, 1.0, 1.0);
    } else {
      offset1 = vec3(0.0, 1.0, 0.0);
      offset2 = vec3(1.0, 1.0, 0.0);
    }
  }

  const vec3 d1 = d0 - offset1 + unskewFactor;
  const vec3 d2 = d0 - offset2 + (2.0*unskewFactor);
  const vec3 d3 = d0 - 1.0 + (3.0*unskewFactor);
  const ivec3 base = ivec3(cell);

  float sum = 0.0;
  const float t0 = 0.6 - dot(d0, d0);
  if (t0 > 0.0) sum += t0*t0*t0*t0*dot(Gradient3(base, period, seed), d0);
  const float t1 = 0.6 - dot(d1, d1);
  if (t1 > 0.0) sum += t1*t1*t1*t1*dot(Gradient3(base + ivec3(offset1), period, seed), d1);
  const float t2 = 0.6 - dot(d2, d2);
  if (t2 > 0.0) sum += t2*t2*t2*t2*dot(Gradient3(base + ivec3(offset2), period, seed), d2);
  const float t3 = 0.6 - dot(d3, d3);
  if (t3 > 0.0) sum += t3*t3*t3*t3*dot(Gradient3(base + ivec3(1.0), period, seed), d3);

  return 32.0*sum;
}

float NoiseAt(const vec3 uv, const float scale, const int period, uint seed) {
  const vec3 p = uv*scale;

#if DIMENSION == 1
  #if SIMPLEX
    return Simplex1(p.x, period, seed);
  #else
    return Perlin1(p.x, period, seed);
  #endif
#elif DIMENSION == 2
  #if SIMPLEX
    return Simplex2(p.xy, period, seed);
  #else
    return Perlin2(p.xy, period, seed);
  #endif
#else
  #if SIMPLEX
    return Simplex3(p, period, seed);
  #else
    return Perlin3(p, period, seed);
  #endif
#endif
}

float Fbm(const vec3 uv, const float baseScale, uint seed) {
  float sum = 0.0;
  float amplitude = 1.0;
  float total = 0.0;
  float scale = baseScale;

  for (int octave = 0; octave < u_octaves; ++octave) {
    const int period = (u_tileable != 0) ? max(1, int(round(scale))) : 0;
    sum += amplitude*NoiseAt(uv, scale, period, seed + uint(octave)*131u);
    total += amplitude;
    amplitude *= u_gain;
    scale *= u_lacunarity;
  }

  return (sum/max(total, 0.00001))*0.5 + 0.5;
}

void main() {
#if DIMENSION == 1
  const ivec3 gid = ivec3(int(gl_GlobalInvocationID.x), 0, 0);
  if (gid.x >= u_size.x) return;
  const vec3 uv = vec3(float(gid.x)/float(u_size.x), 0.0, 0.0);
#elif DIMENSION == 2
  const ivec3 gid = ivec3(ivec2(gl_GlobalInvocationID.xy), 0);
  if ((gid.x >= u_size.x) || (gid.y >= u_size.y)) return;
  const vec3 uv = vec3(float(gid.x)/float(u_size.x), float(gid.y)/float(u_size.y), 0.0);
#else
  const ivec3 gid = ivec3(gl_GlobalInvocationID);
  if (any(greaterThanEqual(gid, u_size))) return;
  const vec3 uv = vec3(gid)/vec3(u_size);
#endif

  const float red = Fbm(uv, u_scale, u_seed);
  const float green = Fbm(uv, u_scale + 1.0, u_seed + 7331u);
  const float blue = 1.0 - abs((2.0*red) - 1.0);

  const vec4 result = vec4(red, green, blue, 1.0);

#if DIMENSION == 1
  imageStore(imgOut, gid.x, result);
#elif DIMENSION == 2
  imageStore(imgOut, gid.xy, result);
#else
  imageStore(imgOut, gid, result);
#endif
}
)NOISE";

i32 ClampOctaves(const i32 value) { return std::clamp(value, 1, 12); }

}  // namespace

bool Noise::create() {
    return create(NoiseConfig{});
}

bool Noise::create(const NoiseConfig& config) {
    unload();

    m_config = config;
    m_config.dimension = std::clamp(config.dimension, 1, 3);
    m_config.width = std::clamp(config.width, 1, 2048);
    m_config.height = std::clamp(config.height, 1, 2048);
    m_config.depth = std::clamp(config.depth, 1, 512);
    m_config.octaves = ClampOctaves(config.octaves);
    m_config.lacunarity = std::max(config.lacunarity, 1.0f);
    m_config.gain = std::clamp(config.gain, 0.0f, 1.0f);
    m_config.frequency = std::max(config.frequency, 0.001f);
    m_config.period = std::clamp(config.period, 1, 512);

    if (m_config.dimension < 3) m_config.depth = 1;
    if (m_config.dimension < 2) m_config.height = 1;

    if (m_config.isTileable) {
        m_config.frequency = static_cast<f32>(std::max(1, static_cast<i32>(std::lround(m_config.frequency))));
        m_config.period = std::max(1, static_cast<i32>(std::lround(m_config.period)));
    }

    const i32 levels = static_cast<i32>(std::floor(std::log2(static_cast<f32>(std::max(m_config.width, 1)))));

    if (m_config.dimension == 1) {
        glCreateTextures(GL_TEXTURE_1D, 1, &m_texture);
        glTextureStorage1D(m_texture, levels + 1, GL_RGBA16F, m_config.width);
    } else if (m_config.dimension == 2) {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_texture);
        glTextureStorage2D(m_texture, levels + 1, GL_RGBA16F, m_config.width, m_config.height);
    } else {
        const i32 depthLevels =
            static_cast<i32>(std::floor(std::log2(static_cast<f32>(std::max(m_config.depth, 1)))));
        glCreateTextures(GL_TEXTURE_3D, 1, &m_texture);
        glTextureStorage3D(m_texture, std::min(levels, depthLevels) + 1, GL_RGBA16F, m_config.width,
                           m_config.height, m_config.depth);
    }

    glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(m_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const GLenum wrap = m_config.isTileable ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_S, wrap);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_T, wrap);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_R, wrap);

    if (!generate()) {
        unload();
        return false;
    }

    SLOGD << "Noise: " << ((m_config.type == NoiseType::Simplex) ? "Simplex" : "Perlin") << " "
         << m_config.dimension << "D, " << m_config.width << "x" << m_config.height << "x"
         << m_config.depth << ", " << m_config.octaves << " Oktaven"
         << (m_config.isTileable ? ", kachelbar" : "");

    return true;
}

bool Noise::buildProgram() {
    if (m_program.isValid() && m_programDimension == m_config.dimension &&
        m_programType == static_cast<i32>(m_config.type)) {
        return true;
    }

    m_program = Shader{};
    m_programDimension = 0;
    m_programType = -1;

    ShaderSourceOptions options;
    options.defines = {{"DIMENSION", std::to_string(m_config.dimension)},
                       {"SIMPLEX", (m_config.type == NoiseType::Simplex) ? "1" : "0"}};

    try {
        m_program = Shader::FromSource(ShaderStage::Compute, kNoiseComputeShader, options);
    } catch (const ShaderError& error) {
        SLOGE << "Noise: Compute-Shader laesst sich nicht bauen: " << error.what();
        return false;
    }

    m_programDimension = m_config.dimension;
    m_programType = static_cast<i32>(m_config.type);
    return true;
}

bool Noise::runCompute() {
    if (m_texture == 0 || !buildProgram()) {
        return false;
    }

    m_program.use();
    glBindImageTexture(0, m_texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    m_program.setUniform("u_size", ivec3(m_config.width, m_config.height, m_config.depth));
    m_program.setUniform("u_scale",
                         m_config.isTileable ? static_cast<f32>(m_config.period) : m_config.frequency);
    m_program.setUniform("u_lacunarity", m_config.lacunarity);
    m_program.setUniform("u_gain", m_config.gain);
    m_program.setUniform("u_octaves", m_config.octaves);
    m_program.setUniform("u_seed", m_config.seed);
    m_program.setUniform("u_tileable", m_config.isTileable ? 1 : 0);

    const i32 groupsX = (m_config.width + 7) / 8;
    const i32 groupsY = (m_config.dimension >= 2) ? ((m_config.height + 7) / 8) : 1;
    const i32 groupsZ = (m_config.dimension >= 3) ? ((m_config.depth + 7) / 8) : 1;

    glDispatchCompute(static_cast<GLuint>(groupsX), static_cast<GLuint>(groupsY),
                      static_cast<GLuint>(groupsZ));
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    Shader::Unbind();

    glGenerateTextureMipmap(m_texture);
    return true;
}

bool Noise::generate() {
    if (m_texture == 0) {
        return false;
    }

    if (!runCompute()) {
        SLOGE << "Noise: Berechnung fehlgeschlagen";
        return false;
    }

    return true;
}

bool Noise::setSize(const i32 width, const i32 height, const i32 depth) {
    NoiseConfig config = m_config;
    config.width = width;
    config.height = (m_config.dimension >= 2) ? height : 1;
    config.depth = (m_config.dimension >= 3) ? depth : 1;
    return create(config);
}

Noise::~Noise() {
    unload();
}

void Noise::unload() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    m_program = Shader{};
    m_programDimension = 0;
    m_programType = -1;
}

void Noise::setSeed(const u32 seed) {
    m_config.seed = seed;
    generate();
}

void Noise::setOctaves(const i32 octaves) {
    m_config.octaves = ClampOctaves(octaves);
    generate();
}

void Noise::setFrequency(const f32 frequency) {
    m_config.frequency = std::max(frequency, 0.001f);
    generate();
}

void Noise::setPeriod(const i32 period) {
    m_config.period = std::clamp(period, 1, 512);
    generate();
}

void Noise::setTileable(const bool isTileable) {
    m_config.isTileable = isTileable;
    generate();
}

void Noise::setLacunarity(const f32 lacunarity) {
    m_config.lacunarity = std::max(lacunarity, 1.0f);
    generate();
}

void Noise::setGain(const f32 gain) {
    m_config.gain = std::clamp(gain, 0.0f, 1.0f);
    generate();
}

void Noise::setType(const NoiseType type) {
    if (m_config.type == type) return;

    m_config.type = type;
    generate();
}

void Noise::setDimension(const i32 dimension) {
    const i32 clamped = std::clamp(dimension, 1, 3);
    if (m_config.dimension == clamped) return;

    NoiseConfig config = m_config;
    config.dimension = clamped;
    create(config);
}

Image Noise::toImage() const {
    if (m_texture == 0) {
        return {};
    }

    const i32 layers = (m_config.dimension == 3) ? m_config.depth : 1;
    const i32 rows = (m_config.dimension == 1) ? 1 : m_config.height * layers;
    const i32 width = (m_config.dimension == 1) ? m_config.width : m_config.width;

    std::vector<f32> texels(static_cast<std::size_t>(width) * static_cast<std::size_t>(rows) * 4u, 0.0f);

    if (m_config.dimension == 1) {
        glGetTextureImage(m_texture, 0, GL_RGBA, GL_FLOAT,
                          static_cast<GLsizei>(texels.size() * sizeof(f32)), texels.data());
    } else {
        glGetTextureImage(m_texture, 0, GL_RGBA, GL_FLOAT,
                          static_cast<GLsizei>(texels.size() * sizeof(f32)), texels.data());
    }

    Image image(width, rows, ImageFormat::Rgba);
    if (image.isEmpty()) {
        return {};
    }

    u8* pixels = image.getData();
    for (std::size_t index = 0; index < texels.size(); ++index) {
        const f32 clamped = std::clamp(texels[index], 0.0f, 1.0f);
        pixels[index] = static_cast<u8>(std::lround(clamped * 255.0f));
    }

    return image;
}

}  // namespace sgl
