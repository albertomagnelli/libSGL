#include <sgl/Model.hpp>

#include <sgl/Color.hpp>
#include <sgl/Image.hpp>
#include <sgl/Log.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "cgltf.h"

namespace sgl {

namespace {

// ─── cgltf-Hilfen ────────────────────────────────────────────────────────────

using CgltfData = std::unique_ptr<cgltf_data, void (*)(cgltf_data*)>;

std::string CgltfResultText(const cgltf_result result) {
  switch (result) {
  case cgltf_result_success:
    return "Erfolg";
  case cgltf_result_data_too_short:
    return "Datei ist zu kurz";
  case cgltf_result_unknown_format:
    return "unbekanntes Format";
  case cgltf_result_invalid_json:
    return "ungueltiges JSON";
  case cgltf_result_invalid_gltf:
    return "ungueltiges glTF";
  case cgltf_result_invalid_options:
    return "ungueltige Optionen";
  case cgltf_result_file_not_found:
    return "Datei nicht gefunden";
  case cgltf_result_io_error:
    return "E/A-Fehler";
  case cgltf_result_out_of_memory:
    return "Speicher erschoepft";
  case cgltf_result_legacy_gltf:
    return "veraltetes glTF";
  case cgltf_result_max_enum:
    break;
  }
  return "unbekannter cgltf-Fehler";
}

std::expected<CgltfData, ModelError> ParseFile(const std::filesystem::path& file) {
  cgltf_options options{};
  cgltf_data* raw = nullptr;

  const std::string path = file.string();
  cgltf_result result = cgltf_parse_file(&options, path.c_str(), &raw);
  if (result != cgltf_result_success) {
    return std::unexpected(ModelError("cgltf: \"" + path + "\": " + CgltfResultText(result)));
  }

  CgltfData data(raw, cgltf_free);

  result = cgltf_load_buffers(&options, data.get(), path.c_str());
  if (result != cgltf_result_success) {
    return std::unexpected(ModelError("cgltf: Buffer von \"" + path + "\": " +
                                      CgltfResultText(result)));
  }

  return data;
}

std::expected<CgltfData, ModelError> ParseMemory(std::span<const u8> bytes,
                                                 const std::filesystem::path& basePath) {
  if (bytes.empty()) {
    return std::unexpected(ModelError("cgltf: Speicherpuffer ist leer"));
  }

  cgltf_options options{};
  cgltf_data* raw = nullptr;

  cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
  if (result != cgltf_result_success) {
    return std::unexpected(ModelError("cgltf: " + CgltfResultText(result)));
  }

  CgltfData data(raw, cgltf_free);

  // cgltf erwartet fuer relative Buffer-URIs einen Dateipfad, aus dem es das
  // Verzeichnis ableitet. basePath ist hier ein Verzeichnis.
  const std::string base =
      basePath.empty() ? std::string() : (basePath / "__model__.gltf").string();
  result = cgltf_load_buffers(&options, data.get(), base.empty() ? nullptr : base.c_str());
  if (result != cgltf_result_success) {
    return std::unexpected(ModelError("cgltf: Buffer: " + CgltfResultText(result)));
  }

  return data;
}

// ─── Basistypen ──────────────────────────────────────────────────────────────

u8 ToByte(const f32 value) {
  return static_cast<u8>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

f32 FromByte(const u8 value) {
  return static_cast<f32>(value) / 255.0f;
}

mat4 ToMat4(const cgltf_float* values) {
  mat4 result(1.0f);

  for (i32 column = 0; column < 4; ++column) {
    for (i32 row = 0; row < 4; ++row) {
      result[column][row] = values[column * 4 + row];
    }
  }

  return result;
}

std::string NameOr(const char* name, const std::string& fallback) {
  return (name != nullptr && name[0] != '\0') ? std::string(name) : fallback;
}

MeshPrimitiveMode ToPrimitiveMode(const cgltf_primitive_type type) {
  switch (type) {
  case cgltf_primitive_type_points:
    return MeshPrimitiveMode::Points;
  case cgltf_primitive_type_lines:
    return MeshPrimitiveMode::Lines;
  case cgltf_primitive_type_line_loop:
    return MeshPrimitiveMode::LineLoop;
  case cgltf_primitive_type_line_strip:
    return MeshPrimitiveMode::LineStrip;
  case cgltf_primitive_type_triangle_strip:
    return MeshPrimitiveMode::TriangleStrip;
  case cgltf_primitive_type_triangle_fan:
    return MeshPrimitiveMode::TriangleFan;
  case cgltf_primitive_type_triangles:
  case cgltf_primitive_type_invalid:
  case cgltf_primitive_type_max_enum:
    break;
  }
  return MeshPrimitiveMode::Triangles;
}

// ─── Bild- und Materialhelfer ────────────────────────────────────────────────

int Base64Value(const char character) {
  if (character >= 'A' && character <= 'Z') return character - 'A';
  if (character >= 'a' && character <= 'z') return character - 'a' + 26;
  if (character >= '0' && character <= '9') return character - '0' + 52;
  if (character == '+') return 62;
  if (character == '/') return 63;
  return -1;
}

std::optional<std::vector<u8>> DecodeBase64(std::string_view text) {
  std::vector<u8> result;
  result.reserve(text.size() * 3 / 4);

  u32 accumulator = 0;
  i32 bits = -8;

  for (const char character : text) {
    if (std::isspace(static_cast<unsigned char>(character))) continue;
    if (character == '=') break;

    const int value = Base64Value(character);
    if (value < 0) return std::nullopt;

    accumulator = (accumulator << 6) | static_cast<u32>(value);
    bits += 6;

    if (bits >= 0) {
      result.push_back(static_cast<u8>((accumulator >> bits) & 0xFFu));
      bits -= 8;
    }
  }

  return result;
}

std::optional<std::vector<u8>> DecodeDataUri(std::string_view uri) {
  if (!uri.starts_with("data:")) return std::nullopt;

  const std::size_t comma = uri.find(',');
  if (comma == std::string_view::npos) return std::nullopt;

  const std::string_view header = uri.substr(5, comma - 5);
  if (header.find("base64") == std::string_view::npos) return std::nullopt;

  return DecodeBase64(uri.substr(comma + 1));
}

Image ExtractChannel(const Image& image, const i32 channel) {
  const i32 selected = std::clamp(channel, 0, std::max(0, image.channelCount() - 1));

  Image result(image.width(), image.height(), ImageFormat::Gray);

  for (i32 y = 0; y < image.height(); ++y) {
    for (i32 x = 0; x < image.width(); ++x) {
      result.setAt(x, y, 0, image.at(x, y, selected));
    }
  }

  return result;
}

Image MultiplyRgb(const Image& image, const vec3& factor) {
  Image result = image.converted(ImageFormat::Rgb);

  for (i32 y = 0; y < result.height(); ++y) {
    for (i32 x = 0; x < result.width(); ++x) {
      result.setAt(x, y, 0, ToByte(FromByte(result.at(x, y, 0)) * factor.x));
      result.setAt(x, y, 1, ToByte(FromByte(result.at(x, y, 1)) * factor.y));
      result.setAt(x, y, 2, ToByte(FromByte(result.at(x, y, 2)) * factor.z));
    }
  }

  return result;
}

void MultiplyGray(Image& image, const f32 factor) {
  for (i32 y = 0; y < image.height(); ++y) {
    for (i32 x = 0; x < image.width(); ++x) {
      image.setAt(x, y, 0, ToByte(FromByte(image.at(x, y, 0)) * factor));
    }
  }
}

bool IsWhite(const vec3& value) {
  return value.x == 1.0f && value.y == 1.0f && value.z == 1.0f;
}

struct LoadContext {
  const cgltf_data& data;
  std::filesystem::path baseDir;
  bool loadMaterials{true};

  std::vector<Image> images;
  std::vector<bool> imageLoaded;
  std::vector<std::shared_ptr<Material>> materials;
  std::shared_ptr<Material> defaultMaterial;

  LoadContext(const cgltf_data& source, std::filesystem::path base, const bool load)
      : data(source),
        baseDir(std::move(base)),
        loadMaterials(load),
        images(static_cast<std::size_t>(source.images_count)),
        imageLoaded(static_cast<std::size_t>(source.images_count), false),
        materials(static_cast<std::size_t>(source.materials_count)),
        defaultMaterial(std::make_shared<Material>()) {}
};

Image LoadImageData(const cgltf_image& source, const std::filesystem::path& baseDir) {
  if (source.buffer_view != nullptr) {
    const u8* bytes = cgltf_buffer_view_data(source.buffer_view);
    if (bytes != nullptr) {
      return ImageIO::loadFromMemory(
          std::span<const u8>(bytes, static_cast<std::size_t>(source.buffer_view->size)));
    }
  }

  if (source.uri == nullptr) return {};

  const std::string_view uri(source.uri);

  if (uri.starts_with("data:")) {
    const auto bytes = DecodeDataUri(uri);
    if (!bytes) return {};

    return ImageIO::loadFromMemory(*bytes);
  }

  std::string decoded(source.uri);
  cgltf_decode_uri(decoded.data());

  std::filesystem::path path(decoded);
  if (path.is_relative() && !baseDir.empty()) path = baseDir / path;

  return ImageIO::load(path);
}

const Image* GetImage(LoadContext& context, const cgltf_image* source) {
  if (source == nullptr) return nullptr;

  const cgltf_size index = cgltf_image_index(&context.data, source);
  if (index < context.images.size() && context.imageLoaded[index]) {
    return context.images[index].isEmpty() ? nullptr : &context.images[index];
  }

  Image image;

  try {
    image = LoadImageData(*source, context.baseDir);
  } catch (const std::exception& error) {
    SLOGW << "Model: Bild \"" << (source->uri ? source->uri : source->name ? source->name : "?")
          << "\" nicht ladbar: " << error.what();
  }

  if (index < context.images.size()) {
    context.imageLoaded[index] = true;
    context.images[index] = std::move(image);
    return context.images[index].isEmpty() ? nullptr : &context.images[index];
  }

  return nullptr;
}

MaterialAlphaMode ToAlphaMode(const cgltf_alpha_mode mode) {
  switch (mode) {
  case cgltf_alpha_mode_mask:
    return MaterialAlphaMode::Mask;
  case cgltf_alpha_mode_blend:
    return MaterialAlphaMode::Blend;
  case cgltf_alpha_mode_opaque:
  case cgltf_alpha_mode_max_enum:
    break;
  }
  return MaterialAlphaMode::Opaque;
}

std::shared_ptr<Material> GetMaterial(LoadContext& context, const cgltf_material* source) {
  if ((source == nullptr) || !context.loadMaterials) return context.defaultMaterial;

  const cgltf_size index = cgltf_material_index(&context.data, source);
  if (index < context.materials.size() && context.materials[index] != nullptr) {
    return context.materials[index];
  }

  auto material = std::make_shared<Material>();

  material->setAlphaMode(ToAlphaMode(source->alpha_mode));
  material->setAlphaCutoff(source->alpha_cutoff);
  material->setDoubleSided(source->double_sided != 0);
  material->setNormalScale(source->normal_texture.texture != nullptr
                               ? source->normal_texture.scale
                               : 1.0f);
  material->setOcclusionStrength(source->occlusion_texture.texture != nullptr
                                     ? source->occlusion_texture.scale
                                     : 1.0f);
  material->setEmissiveStrength(source->has_emissive_strength
                                    ? source->emissive_strength.emissive_strength
                                    : 1.0f);

  if (source->has_pbr_metallic_roughness) {
    const cgltf_pbr_metallic_roughness& pbr = source->pbr_metallic_roughness;

    const Image* baseImage = GetImage(context, pbr.base_color_texture.texture != nullptr
                                                   ? pbr.base_color_texture.texture->image
                                                   : nullptr);

    if (baseImage != nullptr) {
      const vec3 factor(pbr.base_color_factor[0], pbr.base_color_factor[1],
                        pbr.base_color_factor[2]);
      material->setAlbedo(IsWhite(factor) ? *baseImage : MultiplyRgb(*baseImage, factor));
    } else {
      material->setAlbedo(FromLinear(vec3(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                          pbr.base_color_factor[2]),
                                     ToByte(pbr.base_color_factor[3])));
    }

    const Image* metallicRoughness =
        GetImage(context, pbr.metallic_roughness_texture.texture != nullptr
                              ? pbr.metallic_roughness_texture.texture->image
                              : nullptr);

    if (metallicRoughness != nullptr) {
      Image roughness = ExtractChannel(*metallicRoughness, 1);
      Image metallic = ExtractChannel(*metallicRoughness, 2);

      MultiplyGray(roughness, pbr.roughness_factor);
      MultiplyGray(metallic, pbr.metallic_factor);

      material->setRoughness(roughness);
      material->setMetallic(metallic);
    } else {
      material->setRoughness(pbr.roughness_factor);
      material->setMetallic(pbr.metallic_factor);
    }
  }

  const Image* normalImage = GetImage(context, source->normal_texture.texture != nullptr
                                                   ? source->normal_texture.texture->image
                                                   : nullptr);
  if (normalImage != nullptr) material->setNormal(*normalImage);

  const Image* occlusionImage = GetImage(context, source->occlusion_texture.texture != nullptr
                                                      ? source->occlusion_texture.texture->image
                                                      : nullptr);
  if (occlusionImage != nullptr) {
    material->setAmbientOcclusion(ExtractChannel(*occlusionImage, 0));
  }

  const Image* emissiveImage = GetImage(context, source->emissive_texture.texture != nullptr
                                                     ? source->emissive_texture.texture->image
                                                     : nullptr);
  if (emissiveImage != nullptr) {
    const vec3 factor(source->emissive_factor[0], source->emissive_factor[1],
                      source->emissive_factor[2]);
    material->setEmissive(IsWhite(factor) ? *emissiveImage : MultiplyRgb(*emissiveImage, factor));
  } else {
    material->setEmissive(FromLinear(vec3(source->emissive_factor[0], source->emissive_factor[1],
                                          source->emissive_factor[2])));
  }

  if (index < context.materials.size()) context.materials[index] = material;

  return material;
}

// ─── Modellaufbau ────────────────────────────────────────────────────────────

void BuildNodes(const cgltf_data& data, Model& model) {
  model.nodes.resize(static_cast<std::size_t>(data.nodes_count));

  for (cgltf_size i = 0; i < data.nodes_count; ++i) {
    const cgltf_node& source = data.nodes[i];
    ModelNode& node = model.nodes[static_cast<std::size_t>(i)];

    node.name = NameOr(source.name, "node" + std::to_string(i));
    node.parent = source.parent != nullptr
                      ? static_cast<i32>(cgltf_node_index(&data, source.parent))
                      : -1;
    node.mesh = source.mesh != nullptr ? static_cast<i32>(cgltf_mesh_index(&data, source.mesh)) : -1;
    node.skin = source.skin != nullptr ? static_cast<i32>(cgltf_skin_index(&data, source.skin)) : -1;

    node.children.reserve(static_cast<std::size_t>(source.children_count));
    for (cgltf_size child = 0; child < source.children_count; ++child) {
      node.children.push_back(static_cast<i32>(cgltf_node_index(&data, source.children[child])));
    }

    cgltf_float matrix[16];
    cgltf_node_transform_local(&source, matrix);
    node.localTransform = ToMat4(matrix);
  }
}

void BuildSkins(const cgltf_data& data, Model& model) {
  model.skins.resize(static_cast<std::size_t>(data.skins_count));

  for (cgltf_size i = 0; i < data.skins_count; ++i) {
    const cgltf_skin& source = data.skins[i];
    ModelSkin& skin = model.skins[static_cast<std::size_t>(i)];

    skin.name = NameOr(source.name, "skin" + std::to_string(i));
    skin.skeleton = source.skeleton != nullptr
                        ? static_cast<i32>(cgltf_node_index(&data, source.skeleton))
                        : -1;

    skin.joints.reserve(static_cast<std::size_t>(source.joints_count));
    for (cgltf_size joint = 0; joint < source.joints_count; ++joint) {
      skin.joints.push_back(static_cast<i32>(cgltf_node_index(&data, source.joints[joint])));
    }

    skin.inverseBindMatrices.assign(static_cast<std::size_t>(source.joints_count), mat4(1.0f));

    if (source.inverse_bind_matrices != nullptr) {
      const std::size_t floatCount =
          static_cast<std::size_t>(source.inverse_bind_matrices->count) * 16;
      std::vector<f32> values(floatCount);

      const cgltf_size written =
          cgltf_accessor_unpack_floats(source.inverse_bind_matrices, values.data(), floatCount);
      if (written >= floatCount) {
        for (std::size_t joint = 0; joint < skin.inverseBindMatrices.size(); ++joint) {
          skin.inverseBindMatrices[joint] = ToMat4(&values[joint * 16]);
        }
      }
    }
  }
}

std::expected<Mesh, ModelError> BuildMesh(LoadContext& context, const cgltf_mesh& sourceMesh,
                                          const cgltf_primitive& primitive,
                                          const cgltf_size meshIndex, const cgltf_size primitiveIndex) {
  const cgltf_accessor* positions =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
  if (positions == nullptr || positions->count == 0) {
    return std::unexpected(ModelError("Model: Mesh ohne POSITION-Accessor"));
  }

  Mesh mesh;
  mesh.name = NameOr(sourceMesh.name, "mesh" + std::to_string(meshIndex));
  if (sourceMesh.primitives_count > 1) {
    mesh.name += "/primitive" + std::to_string(primitiveIndex);
  }
  mesh.primitiveMode = ToPrimitiveMode(primitive.type);
  mesh.material = GetMaterial(context, primitive.material != nullptr ? primitive.material : nullptr);

  const cgltf_accessor* normals =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
  const cgltf_accessor* uvs =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
  const cgltf_accessor* tangents =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0);
  const cgltf_accessor* joints =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
  const cgltf_accessor* weights =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);

  const cgltf_size vertexCount = positions->count;
  mesh.vertices.resize(static_cast<std::size_t>(vertexCount));

  for (cgltf_size i = 0; i < vertexCount; ++i) {
    MeshVertex& vertex = mesh.vertices[static_cast<std::size_t>(i)];

    cgltf_float values[4] = {};
    if (cgltf_accessor_read_float(positions, i, values, 3)) {
      vertex.position = vec3(values[0], values[1], values[2]);
    }

    if (normals != nullptr && cgltf_accessor_read_float(normals, i, values, 3)) {
      vertex.normal = vec3(values[0], values[1], values[2]);
    }

    if (uvs != nullptr && cgltf_accessor_read_float(uvs, i, values, 2)) {
      vertex.uv = vec2(values[0], values[1]);
    }

    if (tangents != nullptr && cgltf_accessor_read_float(tangents, i, values, 4)) {
      vertex.tangent = vec4(values[0], values[1], values[2], values[3]);
    }

    if (joints != nullptr) {
      cgltf_uint ids[4] = {};
      if (cgltf_accessor_read_uint(joints, i, ids, 4)) {
        vertex.boneIds = uvec4(ids[0], ids[1], ids[2], ids[3]);
      }
    }

    if (weights != nullptr && cgltf_accessor_read_float(weights, i, values, 4)) {
      vertex.boneWeights = vec4(values[0], values[1], values[2], values[3]);
      vertex.normalizeBoneWeights();
    }
  }

  if (primitive.indices != nullptr) {
    mesh.indices.resize(static_cast<std::size_t>(primitive.indices->count));
    for (cgltf_size i = 0; i < primitive.indices->count; ++i) {
      const cgltf_size index = cgltf_accessor_read_index(primitive.indices, i);
      if (index >= vertexCount) {
        return std::unexpected(ModelError("Model: Index " + std::to_string(index) +
                                          " liegt ausserhalb der Vertexliste"));
      }
      mesh.indices[static_cast<std::size_t>(i)] = static_cast<u32>(index);
    }
  } else {
    mesh.indices.resize(static_cast<std::size_t>(vertexCount));
    for (cgltf_size i = 0; i < vertexCount; ++i) {
      mesh.indices[static_cast<std::size_t>(i)] = static_cast<u32>(i);
    }
  }

  return mesh;
}

std::expected<Model, ModelError> BuildModel(const cgltf_data& data,
                                            std::filesystem::path baseDir,
                                            std::string name,
                                            const ModelLoadOptions& options) {
  Model model;
  model.name = std::move(name);

  LoadContext context(data, std::move(baseDir), options.loadMaterials);

  for (cgltf_size meshIndex = 0; meshIndex < data.meshes_count; ++meshIndex) {
    const cgltf_mesh& sourceMesh = data.meshes[meshIndex];

    for (cgltf_size primitiveIndex = 0; primitiveIndex < sourceMesh.primitives_count;
         ++primitiveIndex) {
      auto mesh = BuildMesh(context, sourceMesh, sourceMesh.primitives[primitiveIndex], meshIndex,
                            primitiveIndex);
      if (!mesh) return std::unexpected(std::move(mesh.error()));

      model.meshes.push_back(std::move(*mesh));
    }
  }

  if (model.meshes.empty()) {
    return std::unexpected(ModelError("Model: keine Meshes gefunden"));
  }

  BuildNodes(data, model);
  BuildSkins(data, model);

  if (data.scene != nullptr && data.scene->name != nullptr && data.scene->name[0] != '\0') {
    model.name = data.scene->name;
  }

  return model;
}

// ─── Animationen ─────────────────────────────────────────────────────────────

AnimationPath ToAnimationPath(const cgltf_animation_path_type path) {
  switch (path) {
  case cgltf_animation_path_type_rotation:
    return AnimationPath::Rotation;
  case cgltf_animation_path_type_scale:
    return AnimationPath::Scale;
  case cgltf_animation_path_type_weights:
    return AnimationPath::Weights;
  case cgltf_animation_path_type_translation:
  case cgltf_animation_path_type_invalid:
  case cgltf_animation_path_type_max_enum:
    break;
  }
  return AnimationPath::Translation;
}

AnimationInterpolation ToAnimationInterpolation(const cgltf_interpolation_type type) {
  switch (type) {
  case cgltf_interpolation_type_step:
    return AnimationInterpolation::Step;
  case cgltf_interpolation_type_cubic_spline:
    return AnimationInterpolation::CubicSpline;
  case cgltf_interpolation_type_linear:
  case cgltf_interpolation_type_max_enum:
    break;
  }
  return AnimationInterpolation::Linear;
}

std::vector<ModelAnimation> BuildAnimations(const cgltf_data& data) {
  std::vector<ModelAnimation> animations;
  animations.reserve(static_cast<std::size_t>(data.animations_count));

  for (cgltf_size index = 0; index < data.animations_count; ++index) {
    const cgltf_animation& source = data.animations[index];

    ModelAnimation animation;
    animation.name = NameOr(source.name, "animation" + std::to_string(index));

    animation.samplers.resize(static_cast<std::size_t>(source.samplers_count));

    for (cgltf_size samplerIndex = 0; samplerIndex < source.samplers_count; ++samplerIndex) {
      const cgltf_animation_sampler& sourceSampler = source.samplers[samplerIndex];
      AnimationSampler& sampler = animation.samplers[static_cast<std::size_t>(samplerIndex)];

      sampler.interpolation = ToAnimationInterpolation(sourceSampler.interpolation);

      if (sourceSampler.input != nullptr) {
        sampler.input.resize(static_cast<std::size_t>(sourceSampler.input->count));
        const cgltf_size written =
            cgltf_accessor_unpack_floats(sourceSampler.input, sampler.input.data(),
                                         sampler.input.size());
        sampler.input.resize(static_cast<std::size_t>(written));

        if (!sampler.input.empty()) {
          animation.duration = std::max(animation.duration, sampler.input.back());
        }
      }

      if (sourceSampler.output != nullptr) {
        const u32 components =
            static_cast<u32>(cgltf_num_components(sourceSampler.output->type));
        const std::size_t floatCount =
            static_cast<std::size_t>(sourceSampler.output->count) * components;

        sampler.output.resize(floatCount);
        const cgltf_size written =
            cgltf_accessor_unpack_floats(sourceSampler.output, sampler.output.data(), floatCount);
        sampler.output.resize(static_cast<std::size_t>(written));

        sampler.components = components;
        sampler.valuesPerKey =
            sampler.input.empty()
                ? 0
                : static_cast<u32>(sourceSampler.output->count / sourceSampler.input->count);
      }
    }

    animation.channels.reserve(static_cast<std::size_t>(source.channels_count));

    for (cgltf_size channelIndex = 0; channelIndex < source.channels_count; ++channelIndex) {
      const cgltf_animation_channel& sourceChannel = source.channels[channelIndex];
      if (sourceChannel.target_node == nullptr || sourceChannel.sampler == nullptr) continue;

      AnimationChannel channel;
      channel.node = static_cast<i32>(cgltf_node_index(&data, sourceChannel.target_node));
      channel.path = ToAnimationPath(sourceChannel.target_path);
      channel.sampler = static_cast<i32>(sourceChannel.sampler - source.samplers);

      animation.channels.push_back(channel);
    }

    animations.push_back(std::move(animation));
  }

  return animations;
}

}  // namespace

// ─── Mesh / Model ────────────────────────────────────────────────────────────

bool Mesh::isValid() const noexcept {
  if (vertices.empty() || indices.empty() || material == nullptr) return false;

  for (const u32 index : indices) {
    if (index >= vertices.size()) return false;
  }

  return true;
}

bool Model::isValid() const noexcept {
  if (meshes.empty()) return false;

  for (const Mesh& mesh : meshes) {
    if (!mesh.isValid()) return false;
  }

  return true;
}

// ─── AnimationSampler / ModelAnimation ───────────────────────────────────────

u32 AnimationSampler::keyCount() const noexcept {
  return static_cast<u32>(input.size());
}

u32 AnimationSampler::valueCount() const noexcept {
  if (valuesPerKey == 0) return 0;
  return (interpolation == AnimationInterpolation::CubicSpline) ? valuesPerKey / 3
                                                                : valuesPerKey;
}

u32 AnimationSampler::keyStride() const noexcept {
  return components * valuesPerKey;
}

f32 AnimationSampler::getScalar(const u32 key, const u32 value, const u32 component) const noexcept {
  if (key >= keyCount() || value >= valueCount() || component >= components) return 0.0f;

  const u32 mainOffset = (interpolation == AnimationInterpolation::CubicSpline)
                             ? components * valueCount()
                             : 0;

  const std::size_t index = static_cast<std::size_t>(key) * keyStride() + mainOffset +
                            static_cast<std::size_t>(value) * components + component;

  return index < output.size() ? output[index] : 0.0f;
}

vec3 AnimationSampler::getVec3(const u32 key) const noexcept {
  return vec3(getScalar(key, 0, 0), getScalar(key, 0, 1), getScalar(key, 0, 2));
}

quat AnimationSampler::getQuat(const u32 key) const noexcept {
  return quat(getScalar(key, 0, 3), getScalar(key, 0, 0), getScalar(key, 0, 1),
              getScalar(key, 0, 2));
}

bool ModelAnimation::isValid() const noexcept {
  if (samplers.empty() || channels.empty()) return false;

  for (const AnimationChannel& channel : channels) {
    if (channel.sampler < 0 ||
        channel.sampler >= static_cast<i32>(samplers.size())) {
      return false;
    }
  }

  return true;
}

// ─── ModelIO / ModelAnimationIO ──────────────────────────────────────────────

std::expected<Model, ModelError> ModelIO::load(const std::filesystem::path& file,
                                               const ModelLoadOptions& options) {
  auto parsed = ParseFile(file);
  if (!parsed) return std::unexpected(std::move(parsed.error()));

  CgltfData data = std::move(*parsed);

  return BuildModel(*data, file.parent_path(), file.stem().string(), options);
}

std::expected<Model, ModelError> ModelIO::loadFromMemory(std::span<const u8> data,
                                                         const std::filesystem::path& basePath,
                                                         const ModelLoadOptions& options) {
  auto parsed = ParseMemory(data, basePath);
  if (!parsed) return std::unexpected(std::move(parsed.error()));

  CgltfData model = std::move(*parsed);

  return BuildModel(*model, basePath, "model", options);
}

std::expected<std::vector<ModelAnimation>, ModelError> ModelAnimationIO::load(
    const std::filesystem::path& file) {
  auto parsed = ParseFile(file);
  if (!parsed) return std::unexpected(std::move(parsed.error()));

  CgltfData data = std::move(*parsed);

  return BuildAnimations(*data);
}

std::expected<std::vector<ModelAnimation>, ModelError> ModelAnimationIO::loadFromMemory(
    std::span<const u8> data, const std::filesystem::path& basePath) {
  auto parsed = ParseMemory(data, basePath);
  if (!parsed) return std::unexpected(std::move(parsed.error()));

  CgltfData model = std::move(*parsed);

  return BuildAnimations(*model);
}

}  // namespace sgl
