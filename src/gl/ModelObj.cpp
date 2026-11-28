#include "ModelObj.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sgl/Color.hpp>
#include <sgl/Image.hpp>
#include <sgl/Log.hpp>

#include "tiny_obj_loader.h"

namespace sgl {
namespace {

// ─── Kleine Helfer ───────────────────────────────────────────────────────────

std::string Trim(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();

  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
  text.erase(0, begin);

  return text;
}

std::string Lowercase(std::string text) {
  for (char& character : text) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return text;
}

bool IsZero(const vec3& value) {
  return std::abs(value.x) < 1e-6f && std::abs(value.y) < 1e-6f && std::abs(value.z) < 1e-6f;
}

bool IsWhite(const vec3& value) {
  return std::abs(value.x - 1.0f) < 1e-6f && std::abs(value.y - 1.0f) < 1e-6f &&
         std::abs(value.z - 1.0f) < 1e-6f;
}

u8 ToByte(const f32 value) {
  return static_cast<u8>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

f32 FromByte(const u8 value) { return static_cast<f32>(value) / 255.0f; }

Image MultiplyRgb(const Image& image, const vec3& factor) {
  Image result = image.converted(ImageFormat::Rgb);

  for (i32 y = 0; y < result.getHeight(); ++y) {
    for (i32 x = 0; x < result.getWidth(); ++x) {
      result.setChannel(x, y, 0, ToByte(FromByte(result.getChannel(x, y, 0)) * factor.x));
      result.setChannel(x, y, 1, ToByte(FromByte(result.getChannel(x, y, 1)) * factor.y));
      result.setChannel(x, y, 2, ToByte(FromByte(result.getChannel(x, y, 2)) * factor.z));
    }
  }

  return result;
}

bool LooksLikeNormalMap(const std::string& name) {
  const std::string lower = Lowercase(name);
  return lower.find("normal") != std::string::npos || lower.find("_n.") != std::string::npos ||
         lower.find("_nrm") != std::string::npos || lower.find("nrm.") != std::string::npos;
}

std::string NormalizePath(std::string name) {
  std::replace(name.begin(), name.end(), '\\', '/');
  return name;
}

std::optional<std::string> ReadTextFile(const std::filesystem::path& path, std::string& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "Datei \"" + path.string() + "\" laesst sich nicht oeffnen";
    return std::nullopt;
  }

  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0) {
    error = "Dateigroesse von \"" + path.string() + "\" ist nicht lesbar";
    return std::nullopt;
  }

  file.seekg(0, std::ios::beg);

  std::string text(static_cast<std::size_t>(size), '\0');
  if (size > 0 && !file.read(text.data(), static_cast<std::streamsize>(size))) {
    error = "Datei \"" + path.string() + "\" laesst sich nicht vollstaendig lesen";
    return std::nullopt;
  }

  return text;
}

// ─── Texturen ────────────────────────────────────────────────────────────────

class ObjTextureCache {
public:
  ObjTextureCache(std::filesystem::path baseDir, std::vector<std::filesystem::path> mtlDirs)
      : m_baseDir(std::move(baseDir)), m_mtlDirs(std::move(mtlDirs)) {}

  [[nodiscard]] const Image* load(const std::string& name, const std::filesystem::path& preferredDir) {
    if (name.empty()) return nullptr;

    const std::filesystem::path raw(NormalizePath(name));

    std::vector<std::filesystem::path> candidates;
    if (raw.is_absolute()) {
      candidates.push_back(raw);
    } else {
      if (!preferredDir.empty()) candidates.push_back(preferredDir / raw);
      if (!m_baseDir.empty()) candidates.push_back(m_baseDir / raw);
      for (const std::filesystem::path& dir : m_mtlDirs) candidates.push_back(dir / raw);
      candidates.push_back(raw);
    }

    for (const std::filesystem::path& candidate : candidates) {
      std::error_code error;
      if (!std::filesystem::is_regular_file(candidate, error)) continue;

      const std::string key = candidate.lexically_normal().string();

      if (const auto found = m_images.find(key); found != m_images.end()) {
        return found->second.isEmpty() ? nullptr : &found->second;
      }

      Image image;
      try {
        image = ImageIO::Load(candidate);
      } catch (const std::exception& exception) {
        SLOGW << "Model(OBJ): Textur \"" << candidate.string()
              << "\" nicht ladbar: " << exception.what();
        m_images.emplace(key, Image{});
        return nullptr;
      }

      if (image.isEmpty()) {
        SLOGW << "Model(OBJ): Textur \"" << candidate.string() << "\" ist leer";
        m_images.emplace(key, Image{});
        return nullptr;
      }

      const auto inserted = m_images.emplace(key, std::move(image));
      return &inserted.first->second;
    }

    if (m_warned.insert(NormalizePath(name)).second) {
      SLOGW << "Model(OBJ): Textur \"" << name << "\" nicht gefunden";
    }

    return nullptr;
  }

  [[nodiscard]] const std::vector<std::filesystem::path>& mtlDirs() const noexcept {
    return m_mtlDirs;
  }

private:
  std::filesystem::path                           m_baseDir;
  std::vector<std::filesystem::path>              m_mtlDirs;
  std::unordered_map<std::string, Image>          m_images;
  std::unordered_set<std::string>                 m_warned;
};

// ─── MTL-Loader ──────────────────────────────────────────────────────────────

class ObjMaterialReader final : public tinyobj::MaterialReader {
public:
  explicit ObjMaterialReader(std::filesystem::path baseDir) : m_baseDir(std::move(baseDir)) {}

  bool operator()(const std::string& matId, std::vector<tinyobj::material_t>* materials,
                  std::map<std::string, int>* materialMap, std::string* warn,
                  std::string* err) override {
    const std::filesystem::path path = resolve(matId);

    if (path.empty()) {
      if (warn != nullptr) *warn += "MTL \"" + matId + "\" nicht gefunden\n";
      return true;
    }

    std::ifstream stream(path);
    if (!stream) {
      if (warn != nullptr) *warn += "MTL \"" + path.string() + "\" nicht lesbar\n";
      return true;
    }

    const std::size_t before = (materials != nullptr) ? materials->size() : 0;

    tinyobj::LoadMtl(materialMap, materials, &stream, warn, err);

    const std::filesystem::path directory = path.parent_path();
    if (!directory.empty() &&
        std::find(m_mtlDirs.begin(), m_mtlDirs.end(), directory) == m_mtlDirs.end()) {
      m_mtlDirs.push_back(directory);
    }

    if (materials != nullptr) {
      for (std::size_t i = before; i < materials->size(); ++i) {
        m_materialDirs[(*materials)[i].name] = directory;
      }
    }

    return true;
  }

  [[nodiscard]] const std::filesystem::path& dirFor(const std::string& materialName) const {
    const auto found = m_materialDirs.find(materialName);
    if (found != m_materialDirs.end()) return found->second;

    static const std::filesystem::path empty;
    return empty;
  }

  [[nodiscard]] const std::vector<std::filesystem::path>& mtlDirs() const noexcept {
    return m_mtlDirs;
  }

private:
  [[nodiscard]] std::filesystem::path resolve(const std::string& matId) const {
    const std::filesystem::path raw(NormalizePath(matId));

    if (raw.is_absolute()) {
      return std::filesystem::is_regular_file(raw) ? raw : std::filesystem::path{};
    }

    if (!m_baseDir.empty()) {
      const std::filesystem::path candidate = m_baseDir / raw;
      if (std::filesystem::is_regular_file(candidate)) return candidate;
    }

    if (std::filesystem::is_regular_file(raw)) return raw;

    return {};
  }

  std::filesystem::path                                      m_baseDir;
  std::vector<std::filesystem::path>                         m_mtlDirs;
  std::unordered_map<std::string, std::filesystem::path>     m_materialDirs;
};

// ─── Material ────────────────────────────────────────────────────────────────

std::shared_ptr<Material> BuildMaterial(const tinyobj::material_t& source,
                                        const std::filesystem::path& mtlDir,
                                        ObjTextureCache& textures) {
  auto material = std::make_shared<Material>();

  const vec3 diffuse(static_cast<f32>(source.diffuse[0]), static_cast<f32>(source.diffuse[1]),
                     static_cast<f32>(source.diffuse[2]));

  if (const Image* albedo = textures.load(source.diffuse_texname, mtlDir); albedo != nullptr) {
    material->setAlbedo((IsWhite(diffuse) || IsZero(diffuse)) ? *albedo
                                                              : MultiplyRgb(*albedo, diffuse));
  } else if (!IsZero(diffuse)) {
    material->setAlbedo(FromVec4(vec4(diffuse, static_cast<f32>(source.dissolve))));
  }

  const vec3 emission(static_cast<f32>(source.emission[0]), static_cast<f32>(source.emission[1]),
                      static_cast<f32>(source.emission[2]));

  if (const Image* emissive = textures.load(source.emissive_texname, mtlDir); emissive != nullptr) {
    material->setEmissive((IsWhite(emission) || IsZero(emission)) ? *emissive
                                                                 : MultiplyRgb(*emissive, emission));
  } else if (!IsZero(emission)) {
    material->setEmissive(FromVec4(vec4(emission, 1.0f)));
  }

  const Image* normal = textures.load(source.normal_texname, mtlDir);
  if (normal != nullptr) material->setNormal(*normal);

  const Image* bump = textures.load(source.bump_texname, mtlDir);
  if (bump != nullptr) {
    if (normal == nullptr && LooksLikeNormalMap(source.bump_texname)) {
      material->setNormal(*bump);
    } else {
      material->setHeight(*bump);
    }
  }

  if (const Image* ambient = textures.load(source.ambient_texname, mtlDir); ambient != nullptr) {
    material->setAmbientOcclusion(*ambient);
  }

  if (const Image* roughness = textures.load(source.roughness_texname, mtlDir);
      roughness != nullptr) {
    material->setRoughness(*roughness);
  }

  if (const Image* metallic = textures.load(source.metallic_texname, mtlDir); metallic != nullptr) {
    material->setMetallic(*metallic);
  }

  if (static_cast<f32>(source.dissolve) < 0.999f) {
    material->setAlphaMode(MaterialAlphaMode::Blend);
  }

  return material;
}

// ─── Vertex-Aufbau ────────────────────────────────────────────────────────────

struct ObjVertexKey {
  i32 position{-1};
  i32 texcoord{-1};
  i32 normal{-1};
  i32 smoothing{-1};
  u32 flatFace{0};

  bool operator==(const ObjVertexKey&) const = default;
};

struct ObjVertexKeyHash {
  std::size_t operator()(const ObjVertexKey& key) const noexcept {
    std::size_t hash = std::hash<i32>{}(key.position);
    const auto combine = [&hash](const std::size_t value) {
      hash ^= value + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    };
    combine(std::hash<i32>{}(key.texcoord));
    combine(std::hash<i32>{}(key.normal));
    combine(std::hash<i32>{}(key.smoothing));
    combine(std::hash<u32>{}(key.flatFace));
    return hash;
  }
};

struct SmoothKey {
  i32 position{-1};
  i32 group{0};

  bool operator==(const SmoothKey&) const = default;
};

struct SmoothKeyHash {
  std::size_t operator()(const SmoothKey& key) const noexcept {
    std::size_t hash = std::hash<i32>{}(key.position);
    hash ^= std::hash<i32>{}(key.group) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    return hash;
  }
};

bool ReadVec3(const std::vector<tinyobj::real_t>& values, const i32 index, vec3& out) {
  if (index < 0) return false;

  const std::size_t base = static_cast<std::size_t>(index) * 3;
  if (base + 2 >= values.size()) return false;

  out = vec3(static_cast<f32>(values[base]), static_cast<f32>(values[base + 1]),
             static_cast<f32>(values[base + 2]));
  return true;
}

bool ReadVec2(const std::vector<tinyobj::real_t>& values, const i32 index, vec2& out) {
  if (index < 0) return false;

  const std::size_t base = static_cast<std::size_t>(index) * 2;
  if (base + 1 >= values.size()) return false;

  out = vec2(static_cast<f32>(values[base]), static_cast<f32>(values[base + 1]));
  return true;
}

vec3 NormalizeOr(const vec3& value, const vec3& fallback) {
  const f32 length = glm::length(value);
  return (length > 1e-8f) ? value / length : fallback;
}

std::expected<Mesh, ModelError> BuildObjMesh(const tinyobj::attrib_t& attrib,
                                             const tinyobj::mesh_t& source,
                                             const std::vector<std::size_t>& offsets,
                                             const std::vector<std::size_t>& faces,
                                             const std::shared_ptr<Material>& material,
                                             const std::string& name, const bool flipV) {
  Mesh mesh;
  mesh.setName(name);
  mesh.setMaterial(material);
  mesh.setPrimitiveMode(MeshPrimitiveMode::Triangles);

  std::vector<MeshVertex>& vertices = mesh.getVertices();
  std::vector<u32>         indices;
  indices.reserve(source.indices.size());
  vertices.reserve(attrib.vertices.size() / 3 + 16);

  std::unordered_map<ObjVertexKey, u32, ObjVertexKeyHash> lookup;
  lookup.reserve(attrib.vertices.size() / 3 + 16);

  std::vector<ObjVertexKey> vertexKeys;
  vertexKeys.reserve(attrib.vertices.size() / 3 + 16);

  std::unordered_map<SmoothKey, vec3, SmoothKeyHash> smoothNormals;
  std::vector<vec3>                                  flatNormals;

  for (const std::size_t face : faces) {
    if (face + 1 >= offsets.size()) {
      return std::unexpected(ModelError("OBJ: Flaechenindex ausserhalb der Daten"));
    }

    const std::size_t base = offsets[face];
    const std::size_t count = offsets[face + 1] - base;
    if (count < 3) continue;
    if (base + count > source.indices.size()) {
      return std::unexpected(ModelError("OBJ: Flaechenindex ausserhalb der Indexliste"));
    }

    const u32 smoothing =
        (face < source.smoothing_group_ids.size()) ? source.smoothing_group_ids[face] : 0U;

    for (std::size_t triangle = 1; triangle + 1 < count; ++triangle) {
      const tinyobj::index_t corners[3] = {source.indices[base], source.indices[base + triangle],
                                           source.indices[base + triangle + 1]};

      vec3 position[3];
      for (i32 corner = 0; corner < 3; ++corner) {
        if (!ReadVec3(attrib.vertices, corners[corner].vertex_index, position[corner])) {
          return std::unexpected(ModelError("OBJ: Vertexindex ausserhalb der Vertexliste"));
        }
      }

      const vec3 faceNormal = glm::cross(position[1] - position[0], position[2] - position[0]);
      const vec3 flatNormal = NormalizeOr(faceNormal, vec3(0.0f, 0.0f, 1.0f));
      const u32  flatFace = static_cast<u32>(flatNormals.size());
      flatNormals.push_back(flatNormal);

      for (i32 corner = 0; corner < 3; ++corner) {
        ObjVertexKey key;
        key.position = corners[corner].vertex_index;
        key.texcoord = corners[corner].texcoord_index;
        key.normal = corners[corner].normal_index;

        if (key.normal >= 0) {
          key.smoothing = -1;
          key.flatFace = 0;
        } else if (smoothing > 0) {
          key.smoothing = static_cast<i32>(smoothing);
          key.flatFace = 0;
          const auto inserted = smoothNormals.try_emplace(
              SmoothKey{key.position, key.smoothing}, vec3(0.0f));
          inserted.first->second += faceNormal;
        } else {
          key.smoothing = 0;
          key.flatFace = flatFace;
        }

        if (const auto found = lookup.find(key); found != lookup.end()) {
          indices.push_back(found->second);
          continue;
        }

        if (vertices.size() >= std::numeric_limits<u32>::max()) {
          return std::unexpected(ModelError("OBJ: zu viele Vertices"));
        }

        MeshVertex vertex;
        vertex.setPosition(position[corner]);

        if (key.texcoord >= 0) {
          vec2 uv;
          if (!ReadVec2(attrib.texcoords, key.texcoord, uv)) {
            return std::unexpected(ModelError("OBJ: Texturkoordinate ausserhalb der Daten"));
          }
          if (flipV) uv.y = 1.0f - uv.y;
          vertex.setUv(uv);
        }

        if (key.normal >= 0) {
          vec3 normal;
          if (!ReadVec3(attrib.normals, key.normal, normal)) {
            return std::unexpected(ModelError("OBJ: Normale ausserhalb der Daten"));
          }
          vertex.setNormal(normal);
        }

        const u32 index = static_cast<u32>(vertices.size());
        vertices.push_back(vertex);
        vertexKeys.push_back(key);
        lookup.emplace(key, index);
        indices.push_back(index);
      }
    }
  }

  if (vertices.empty() || indices.empty()) {
    return std::unexpected(ModelError("OBJ: Mesh ohne Dreiecke"));
  }

  for (std::size_t i = 0; i < vertexKeys.size(); ++i) {
    const ObjVertexKey& key = vertexKeys[i];
    if (key.normal >= 0) continue;

    vec3 normal(0.0f, 0.0f, 1.0f);

    if (key.smoothing > 0) {
      if (const auto found = smoothNormals.find(SmoothKey{key.position, key.smoothing});
          found != smoothNormals.end()) {
        normal = NormalizeOr(found->second, normal);
      }
    } else if (key.flatFace < flatNormals.size()) {
      normal = flatNormals[key.flatFace];
    }

    vertices[i].setNormal(normal);
  }

  mesh.setIndices(std::move(indices));
  return mesh;
}

// ─── Modellaufbau ─────────────────────────────────────────────────────────────

std::expected<Model, ModelError> BuildObjModel(std::string objText,
                                               const std::filesystem::path& baseDir,
                                               const std::string& modelName,
                                               const ModelLoadOptions& options) {
  if (objText.size() >= 3 && static_cast<u8>(objText[0]) == 0xEF &&
      static_cast<u8>(objText[1]) == 0xBB && static_cast<u8>(objText[2]) == 0xBF) {
    objText.erase(0, 3);
  }

  tinyobj::attrib_t                attrib;
  std::vector<tinyobj::shape_t>    shapes;
  std::vector<tinyobj::material_t> materials;
  std::string                      warn;
  std::string                      err;

  ObjMaterialReader materialReader(baseDir);
  std::istringstream objStream(std::move(objText));

  const bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &objStream,
                                   options.loadMaterials ? &materialReader : nullptr, true, false);

  if (!warn.empty()) SLOGW << "Model(OBJ): " << Trim(warn);

  if (!ok) {
    return std::unexpected(ModelError("OBJ: " + (err.empty() ? std::string("Laden fehlgeschlagen")
                                                              : Trim(err))));
  }

  if (!err.empty()) SLOGW << "Model(OBJ): " << Trim(err);

  Model model;
  model.setName(modelName);

  ObjTextureCache textures(baseDir, materialReader.mtlDirs());

  auto defaultMaterial = std::make_shared<Material>();
  std::vector<std::shared_ptr<Material>> materialCache(materials.size());

  if (options.loadMaterials) {
    for (std::size_t i = 0; i < materials.size(); ++i) {
      materialCache[i] = BuildMaterial(materials[i], materialReader.dirFor(materials[i].name), textures);
    }
  }

  for (const tinyobj::shape_t& shape : shapes) {
    const tinyobj::mesh_t& source = shape.mesh;
    if (source.indices.empty() || source.num_face_vertices.empty()) continue;

    const std::size_t faceCount = source.num_face_vertices.size();

    std::vector<std::size_t> offsets(faceCount + 1, 0);
    for (std::size_t face = 0; face < faceCount; ++face) {
      offsets[face + 1] = offsets[face] + source.num_face_vertices[face];
    }

    std::map<i32, std::vector<std::size_t>> facesByMaterial;
    for (std::size_t face = 0; face < faceCount; ++face) {
      const i32 materialId =
          (face < source.material_ids.size()) ? source.material_ids[face] : -1;
      facesByMaterial[materialId].push_back(face);
    }

    for (const auto& [materialId, faces] : facesByMaterial) {
      std::shared_ptr<Material> material = defaultMaterial;
      if (options.loadMaterials && materialId >= 0 &&
          static_cast<std::size_t>(materialId) < materialCache.size() &&
          materialCache[static_cast<std::size_t>(materialId)] != nullptr) {
        material = materialCache[static_cast<std::size_t>(materialId)];
      }

      std::string meshName = shape.name.empty() ? modelName : shape.name;
      if (facesByMaterial.size() > 1) {
        if (materialId >= 0 && static_cast<std::size_t>(materialId) < materials.size() &&
            !materials[static_cast<std::size_t>(materialId)].name.empty()) {
          meshName += "/" + materials[static_cast<std::size_t>(materialId)].name;
        } else {
          meshName += "/" + std::to_string(materialId);
        }
      }

      auto built = BuildObjMesh(attrib, source, offsets, faces, material, meshName,
                                options.flipObjV);
      if (!built) return std::unexpected(std::move(built.error()));

      const i32 nodeIndex = static_cast<i32>(model.getNodes().size());

      ModelNode node;
      node.name = meshName;
      node.mesh = static_cast<i32>(model.getMeshes().size());
      model.getNodes().push_back(std::move(node));

      built->setNodeIndex(nodeIndex);
      model.getMeshes().push_back(std::move(*built));
    }
  }

  if (model.getMeshes().empty()) {
    return std::unexpected(ModelError("OBJ: keine Flaechen gefunden"));
  }

  return model;
}

}  // namespace

namespace detail {

bool IsObjFile(const std::filesystem::path& file) {
  return Lowercase(file.extension().string()) == ".obj";
}

bool LooksLikeObj(const std::span<const u8> data) {
  std::size_t index = 0;

  if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
    index = 3;
  }

  while (index < data.size()) {
    while (index < data.size() && std::isspace(static_cast<unsigned char>(data[index]))) ++index;

    if (index >= data.size()) break;

    if (data[index] == '#') {
      while (index < data.size() && data[index] != '\n') ++index;
      continue;
    }

    const std::size_t begin = index;
    while (index < data.size() && !std::isspace(static_cast<unsigned char>(data[index]))) ++index;

    const std::string_view token(reinterpret_cast<const char*>(data.data() + begin), index - begin);

    return token == "v" || token == "vt" || token == "vn" || token == "f" || token == "o" ||
           token == "g" || token == "s" || token == "mtllib" || token == "usemtl" ||
           token == "l" || token == "p";
  }

  return false;
}

std::expected<Model, ModelError> LoadObjFile(const std::filesystem::path& file,
                                             const ModelLoadOptions& options) {
  std::string error;
  auto        text = ReadTextFile(file, error);
  if (!text) return std::unexpected(ModelError("OBJ: " + error));

  std::string modelName = file.stem().string();
  if (modelName.empty()) modelName = "model";

  return BuildObjModel(std::move(*text), file.parent_path(), modelName, options);
}

std::expected<Model, ModelError> LoadObjMemory(const std::span<const u8> data,
                                               const std::filesystem::path& basePath,
                                               const ModelLoadOptions& options) {
  if (data.empty()) {
    return std::unexpected(ModelError("OBJ: Speicherpuffer ist leer"));
  }

  std::string text(reinterpret_cast<const char*>(data.data()), data.size());

  return BuildObjModel(std::move(text), basePath, "model", options);
}

}  // namespace detail
}  // namespace sgl
