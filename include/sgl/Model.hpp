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
//   auto model = ModelIO::Load("character.glb");
//   if (!model) { /* model.error().getMessage() */ }
//
//   auto animations = ModelAnimationIO::Load("character.glb");
//   if (animations) {
//       for (const ModelAnimation& animation : *animations) {
//           /* Animation auf model.getNodes() anwenden */
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
#include <utility>
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
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] const std::vector<MeshVertex>& getVertices() const noexcept { return m_vertices; }
    [[nodiscard]] std::vector<MeshVertex>&       getVertices() noexcept { return m_vertices; }
    void setVertices(std::vector<MeshVertex> vertices) { m_vertices = std::move(vertices); }

    [[nodiscard]] const std::vector<u32>& getIndices() const noexcept { return m_indices; }
    [[nodiscard]] std::vector<u32>&       getIndices() noexcept { return m_indices; }
    void setIndices(std::vector<u32> indices) { m_indices = std::move(indices); }

    [[nodiscard]] const std::shared_ptr<Material>& getMaterial() const noexcept { return m_material; }
    [[nodiscard]] std::shared_ptr<Material>&       getMaterial() noexcept { return m_material; }
    void setMaterial(std::shared_ptr<Material> material) { m_material = std::move(material); }

    [[nodiscard]] MeshPrimitiveMode getPrimitiveMode() const noexcept { return m_primitiveMode; }
    void setPrimitiveMode(MeshPrimitiveMode mode) noexcept { m_primitiveMode = mode; }

    // Node, der dieses Mesh instanziiert (-1 = unbekannt).
    [[nodiscard]] i32 getNodeIndex() const noexcept { return m_nodeIndex; }
    void setNodeIndex(i32 index) noexcept { m_nodeIndex = index; }

    [[nodiscard]] bool isValid() const noexcept;

private:
    std::string               m_name;
    std::vector<MeshVertex>   m_vertices;
    std::vector<u32>          m_indices;
    std::shared_ptr<Material> m_material;
    MeshPrimitiveMode         m_primitiveMode{MeshPrimitiveMode::Triangles};
    i32                       m_nodeIndex{-1};
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
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] const std::vector<Mesh>& getMeshes() const noexcept { return m_meshes; }
    [[nodiscard]] std::vector<Mesh>&       getMeshes() noexcept { return m_meshes; }
    void setMeshes(std::vector<Mesh> meshes) { m_meshes = std::move(meshes); }

    [[nodiscard]] const std::vector<ModelNode>& getNodes() const noexcept { return m_nodes; }
    [[nodiscard]] std::vector<ModelNode>&       getNodes() noexcept { return m_nodes; }
    void setNodes(std::vector<ModelNode> nodes) { m_nodes = std::move(nodes); }

    [[nodiscard]] const std::vector<ModelSkin>& getSkins() const noexcept { return m_skins; }
    [[nodiscard]] std::vector<ModelSkin>&       getSkins() noexcept { return m_skins; }
    void setSkins(std::vector<ModelSkin> skins) { m_skins = std::move(skins); }

    [[nodiscard]] bool isValid() const noexcept;

private:
    std::string            m_name;
    std::vector<Mesh>      m_meshes;
    std::vector<ModelNode> m_nodes;
    std::vector<ModelSkin> m_skins;
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
    static std::expected<Model, ModelError> Load(
        const std::filesystem::path& file,
        const ModelLoadOptions& options = {});

    // basePath ist das Verzeichnis für relative Buffer-/Bild-URIs.
    static std::expected<Model, ModelError> LoadFromMemory(
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

    [[nodiscard]] u32 getKeyCount() const noexcept;
    [[nodiscard]] u32 getValueCount() const noexcept;
    [[nodiscard]] u32 getKeyStride() const noexcept;

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
    // Node-Name zum Retargeting zwischen Dateien (z. B. walk.fbx -> zombie.fbx).
    std::string nodeName;
    AnimationPath path{AnimationPath::Translation};
    i32 sampler{-1};
};

class ModelAnimation {
public:
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] f32 getDuration() const noexcept { return m_duration; }
    void setDuration(f32 duration) noexcept { m_duration = duration; }

    [[nodiscard]] const std::vector<AnimationSampler>& getSamplers() const noexcept { return m_samplers; }
    [[nodiscard]] std::vector<AnimationSampler>&       getSamplers() noexcept { return m_samplers; }
    void setSamplers(std::vector<AnimationSampler> samplers) { m_samplers = std::move(samplers); }

    [[nodiscard]] const std::vector<AnimationChannel>& getChannels() const noexcept { return m_channels; }
    [[nodiscard]] std::vector<AnimationChannel>&       getChannels() noexcept { return m_channels; }
    void setChannels(std::vector<AnimationChannel> channels) { m_channels = std::move(channels); }

    [[nodiscard]] bool isValid() const noexcept;

private:
    std::string                   m_name;
    f32                           m_duration{0.0f};
    std::vector<AnimationSampler> m_samplers;
    std::vector<AnimationChannel> m_channels;
};

class ModelAnimationIO {
public:
    static std::expected<std::vector<ModelAnimation>, ModelError> Load(
        const std::filesystem::path& file);

    static std::expected<std::vector<ModelAnimation>, ModelError> LoadFromMemory(
        std::span<const u8> data,
        const std::filesystem::path& basePath = {});
};

}  // namespace sgl
