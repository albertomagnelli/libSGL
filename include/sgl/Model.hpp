#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Model
//
// CPU-seitiges glTF-/FBX-Modell (cgltf/ufbx) ohne OpenGL-, Texture- oder
// Shader-Bindung:
//
//   Model             Meshes, Nodes und Skins
//   Mesh              MeshVertex-Puffer, Indizes und Material
//   ModelIO           Laden von .gltf/.glb/.fbx inklusive Materialien
//   ModelAnimation    fertig geladene Animationsdaten
//   ModelAnimationIO  Laden von Animationen aus .gltf/.glb/.fbx
//
// Bilder werden über sgl::ImageIO geladen (externe Dateien, GLB-Buffer-Views,
// Data-URIs und eingebettete FBX-Texturen). Materialien werden als sgl::Material gespeichert und von
// mehreren Meshes gemeinsam genutzt.
//
// Beispiel:
//
//   auto model = ModelIO::load("character.glb");
//   if (!model) { /* model.error().getMessage() */ }
//
//   auto animations = ModelAnimationIO::load("character.glb");
//   if (animations) {
//       for (const ModelAnimation& animation : *animations) {
//           /* Animation auf model.nodes anwenden */
//       }
//   }
// ─────────────────────────────────────────────────────────────────────────────

#include "Material.hpp"
#include "MeshVertex.hpp"
#include "Types.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

// ─── Fehler ──────────────────────────────────────────────────────────────────

class ModelError : public std::runtime_error {
public:
    explicit ModelError(const std::string& message) : std::runtime_error(message) {}
};

// ─── Mesh ────────────────────────────────────────────────────────────────────

enum class MeshPrimitiveMode : u8 {
    Points,
    Lines,
    LineLoop,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan
};

class Mesh {
public:
    std::string name;
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    std::shared_ptr<Material> material;
    MeshPrimitiveMode primitiveMode{MeshPrimitiveMode::Triangles};
    // Node, der dieses Mesh instanziiert (-1 = unbekannt).
    i32 nodeIndex{-1};

    [[nodiscard]] bool isValid() const noexcept;
};

// ─── Nodes und Skins ─────────────────────────────────────────────────────────

struct ModelNode {
    std::string name;
    i32 parent{-1};
    std::vector<i32> children;
    mat4 localTransform{1.0f};
    i32 mesh{-1};
    i32 skin{-1};
};

struct ModelSkin {
    std::string name;
    std::vector<i32> joints;
    std::vector<mat4> inverseBindMatrices;
    i32 skeleton{-1};
};

// ─── Modell ──────────────────────────────────────────────────────────────────

class Model {
public:
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<ModelNode> nodes;
    std::vector<ModelSkin> skins;

    [[nodiscard]] bool isValid() const noexcept;
};

// ─── Modell laden ────────────────────────────────────────────────────────────

struct ModelLoadOptions {
    bool loadMaterials{true};
    // FBX speichert UVs mit Ursprung unten links; sgl/glTF verwenden oben
    // links. Standardmäßig wird V beim FBX-Import gedreht.
    bool flipFbxV{true};
};

class ModelIO {
public:
    static std::expected<Model, ModelError> load(
        const std::filesystem::path& file,
        const ModelLoadOptions& options = {});

    // basePath ist das Verzeichnis für relative Buffer-/Bild-URIs.
    static std::expected<Model, ModelError> loadFromMemory(
        std::span<const u8> data,
        const std::filesystem::path& basePath = {},
        const ModelLoadOptions& options = {});
};

// ─── Animationen ─────────────────────────────────────────────────────────────

enum class AnimationPath : u8 {
    Translation,
    Rotation,
    Scale,
    Weights
};

enum class AnimationInterpolation : u8 {
    Linear,
    Step,
    CubicSpline
};

struct AnimationSampler {
    std::vector<f32> input;   // Key-Zeiten
    std::vector<f32> output;  // flache, interleaved Werte
    u32 components{0};        // 1, 3 oder 4 je Wert
    u32 valuesPerKey{1};      // Werte pro Key; bei Cubic-Spline inkl. Tangenten
    AnimationInterpolation interpolation{AnimationInterpolation::Linear};

    [[nodiscard]] u32 keyCount() const noexcept;
    [[nodiscard]] u32 valueCount() const noexcept;
    [[nodiscard]] u32 keyStride() const noexcept;

    // value = Index innerhalb der Hauptwerte eines Keys, component = Kanal.
    // Bei Cubic-Spline werden die In-/Out-Tangenten übersprungen.
    [[nodiscard]] f32 getScalar(u32 key, u32 value = 0, u32 component = 0) const noexcept;
    [[nodiscard]] vec3 getVec3(u32 key) const noexcept;
    [[nodiscard]] quat getQuat(u32 key) const noexcept;
};

struct AnimationChannel {
    // Index in Model::nodes, wenn Modell und Animationen aus derselben Datei
    // geladen wurden.
    i32 node{-1};
    AnimationPath path{AnimationPath::Translation};
    i32 sampler{-1};
};

class ModelAnimation {
public:
    std::string name;
    f32 duration{0.0f};
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;

    [[nodiscard]] bool isValid() const noexcept;
};

class ModelAnimationIO {
public:
    static std::expected<std::vector<ModelAnimation>, ModelError> load(
        const std::filesystem::path& file);

    static std::expected<std::vector<ModelAnimation>, ModelError> loadFromMemory(
        std::span<const u8> data,
        const std::filesystem::path& basePath = {});
};

}  // namespace sgl
