// ─────────────────────────────────────────────────────────────────────────────
// model_viewer – glTF-/FBX-/OBJ-Modelle mit sgl laden und rendern
//
// Zeigt character.glb, turrican.glb, zombie.fbx und FuturisticGirl.obj
// nebeneinander. Alle Modelle rotieren kontinuierlich um die Y-Achse.
// Die OBJ-Figur hat keine Animation und wird nur gedreht. Die Meshes werden mit ihren Materialien
// (Albedo, Metallic, Roughness, AO, Normal, Emissive) über einen einfachen
// PBR-Metallic-Roughness-Shader gezeichnet.
//
//   ./sgl_example_model_viewer
//   ./sgl_example_model_viewer --frame bild.png [--time 0.5]
//
// Mit --frame wird ein einzelnes Bild gerendert und gespeichert; das Fenster
// bleibt dabei unsichtbar.
// ─────────────────────────────────────────────────────────────────────────────

#include "ExampleCommon.hpp"

#include <sgl/Image.hpp>
#include <sgl/Model.hpp>
#include <sgl/Shader.hpp>
#include <sgl/Texture.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace sgl;
using namespace gl;

namespace {

constexpr f32 kTargetHeight = 1.7f;
constexpr f32 kModelSpacing = 1.9f;

const char* const kModelShader = R"(
#pragma stage vertex
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in uvec4 aBoneIds;
layout(location = 5) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uBones[128];
uniform int uSkinned;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;
out vec3 vTangent;
out float vHandedness;

void main() {
    vec4 localPosition = vec4(aPosition, 1.0);
    vec3 localNormal = aNormal;
    vec3 localTangent = aTangent.xyz;

    if (uSkinned != 0) {
        mat4 skin =
            uBones[aBoneIds.x] * aBoneWeights.x +
            uBones[aBoneIds.y] * aBoneWeights.y +
            uBones[aBoneIds.z] * aBoneWeights.z +
            uBones[aBoneIds.w] * aBoneWeights.w;

        localPosition = skin * localPosition;
        localNormal = mat3(skin) * localNormal;
        localTangent = mat3(skin) * localTangent;
    }

    vec4 world = uModel * localPosition;
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));

    vWorldPos = world.xyz;
    vNormal = normalize(normalMatrix * localNormal);
    vTangent = mat3(uModel) * localTangent;
    vHandedness = aTangent.w;
    vUv = aUv;

    gl_Position = uProjection * uView * world;
}

#pragma stage fragment
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUv;
in vec3 vTangent;
in float vHandedness;

uniform sampler2D uAlbedo;
uniform sampler2D uMetallic;
uniform sampler2D uRoughness;
uniform sampler2D uAO;
uniform sampler2D uNormal;
uniform sampler2D uEmissive;

uniform vec3 uViewPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;

uniform float uNormalScale;
uniform float uOcclusionStrength;
uniform float uEmissiveStrength;
uniform int uAlphaMode;
uniform float uAlphaCutoff;

out vec4 fragColor;

const float PI = 3.14159265359;

float distributionGGX(const vec3 N, const vec3 H, const float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(N, H), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = (nDotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denominator * denominator, 0.0001);
}

float geometrySchlickGGX(const float nDotV, const float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.0001);
}

float geometrySmith(const vec3 N, const vec3 V, const vec3 L, const float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(const float cosTheta, const vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 albedoSample = texture(uAlbedo, vUv);
    if (uAlphaMode == 1 && albedoSample.a < uAlphaCutoff) {
        discard;
    }

    vec3 albedo = albedoSample.rgb;
    float metallic = texture(uMetallic, vUv).r;
    float roughness = clamp(texture(uRoughness, vUv).r, 0.04, 1.0);
    float ao = texture(uAO, vUv).r;

    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangent - N * dot(N, vTangent));
    if (length(T) < 0.0001) {
        T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
    }
    vec3 B = cross(N, T) * vHandedness;

    vec3 normalSample = texture(uNormal, vUv).rgb * 2.0 - 1.0;
    normalSample.xy *= uNormalScale;
    N = normalize(mat3(T, B, N) * normalSample);

    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    float nDotL = max(dot(N, L), 0.0);
    vec3 H = normalize(V + L);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * nDotL + 0.0001;
    vec3 specular = numerator / denominator;
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    vec3 radiance = uLightColor;
    vec3 color = (kD * albedo / PI + specular) * radiance * nDotL;

    float occlusion = mix(1.0, ao, clamp(uOcclusionStrength, 0.0, 1.0));
    color += albedo * occlusion * 0.14;
    color += texture(uEmissive, vUv).rgb * uEmissiveStrength;

    // Reinhard + Gamma, weil das Standard-Framebuffer nicht sRGB ist.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    float alpha = (uAlphaMode == 2) ? albedoSample.a : 1.0;
    fragColor = vec4(color, alpha);
}
)";

Texture2d MakeTexture(const Image& image, const bool srgb) {
    TextureOptions options;
    options.mipmaps = MipmapMode::Linear;
    options.minFilter = TextureFilter::Linear;
    options.magFilter = TextureFilter::Linear;
    options.wrapS = TextureWrap::Repeat;
    options.wrapT = TextureWrap::Repeat;
    options.isSrgb = srgb;

    return Texture2d::FromImage(image, options);
}

struct GpuMaterial {
    Texture2d albedo;
    Texture2d metallic;
    Texture2d roughness;
    Texture2d ao;
    Texture2d normal;
    Texture2d emissive;

    f32 normalScale{1.0f};
    f32 occlusionStrength{1.0f};
    f32 emissiveStrength{1.0f};
    f32 alphaCutoff{0.5f};
    i32 alphaMode{0};
};

struct GpuMesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei indexCount{0};
    mat4 nodeTransform{1.0f};
    i32 skinIndex{-1};
    bool isSkinned{false};
    GpuMaterial material;
};

struct ModelInstance {
    Model model;
    std::vector<GpuMesh> meshes;
    ModelAnimation animation;
    vec3 center{0.0f};
    f32 scale{1.0f};
    f32 rotationOffset{0.0f};
};

bool UploadMaterial(const Material& source, GpuMaterial& target) {
    try {
        target.albedo = MakeTexture(source.getAlbedoMap(), true);
        target.metallic = MakeTexture(source.getMetallicMap(), false);
        target.roughness = MakeTexture(source.getRoughnessMap(), false);
        target.ao = MakeTexture(source.getAmbientOcclusionMap(), false);
        target.normal = MakeTexture(source.getNormalMap(), false);
        target.emissive = MakeTexture(source.getEmissiveMap(), true);

        target.normalScale = source.getNormalScale();
        target.occlusionStrength = source.getOcclusionStrength();
        target.emissiveStrength = source.getEmissiveStrength();
        target.alphaCutoff = source.getAlphaCutoff();
        target.alphaMode = static_cast<i32>(source.getAlphaMode());

        return true;
    } catch (const std::exception& error) {
        SLOGE("Materialtextur konnte nicht hochgeladen werden: %s\n", error.what());
        return false;
    }
}

bool UploadMesh(const Mesh& source, GpuMesh& target) {
    if (source.getMaterial() == nullptr || source.getVertices().empty() || source.getIndices().empty()) {
        return false;
    }

    if (!UploadMaterial(*source.getMaterial(), target.material)) return false;

    glCreateVertexArrays(1, &target.vao);
    glCreateBuffers(1, &target.vbo);
    glCreateBuffers(1, &target.ebo);

    glNamedBufferData(target.vbo,
                      static_cast<GLsizeiptr>(source.getVertices().size() * sizeof(MeshVertex)),
                      source.getVertices().data(), GL_STATIC_DRAW);
    glNamedBufferData(target.ebo,
                      static_cast<GLsizeiptr>(source.getIndices().size() * sizeof(u32)),
                      source.getIndices().data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(MeshVertex));

    glVertexArrayVertexBuffer(target.vao, 0, target.vbo, 0, stride);
    glVertexArrayAttribFormat(target.vao, 0, 3, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(MeshVertex::PositionOffset()));
    glVertexArrayAttribBinding(target.vao, 0, 0);
    glEnableVertexArrayAttrib(target.vao, 0);

    glVertexArrayAttribFormat(target.vao, 1, 3, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(MeshVertex::NormalOffset()));
    glVertexArrayAttribBinding(target.vao, 1, 0);
    glEnableVertexArrayAttrib(target.vao, 1);

    glVertexArrayAttribFormat(target.vao, 2, 2, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(MeshVertex::UvOffset()));
    glVertexArrayAttribBinding(target.vao, 2, 0);
    glEnableVertexArrayAttrib(target.vao, 2);

    glVertexArrayAttribFormat(target.vao, 3, 4, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(MeshVertex::TangentOffset()));
    glVertexArrayAttribBinding(target.vao, 3, 0);
    glEnableVertexArrayAttrib(target.vao, 3);

    glVertexArrayAttribIFormat(target.vao, 4, 4, GL_UNSIGNED_INT,
                               static_cast<GLuint>(MeshVertex::BoneIdsOffset()));
    glVertexArrayAttribBinding(target.vao, 4, 0);
    glEnableVertexArrayAttrib(target.vao, 4);

    glVertexArrayAttribFormat(target.vao, 5, 4, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(MeshVertex::BoneWeightsOffset()));
    glVertexArrayAttribBinding(target.vao, 5, 0);
    glEnableVertexArrayAttrib(target.vao, 5);

    glVertexArrayElementBuffer(target.vao, target.ebo);

    target.indexCount = static_cast<GLsizei>(source.getIndices().size());
    return true;
}

mat4 NodeWorldTransform(const Model& model, const i32 nodeIndex) {
    mat4 result(1.0f);

    i32 current = nodeIndex;
    while (current >= 0 && current < static_cast<i32>(model.getNodes().size())) {
        result = model.getNodes()[static_cast<std::size_t>(current)].localTransform * result;
        current = model.getNodes()[static_cast<std::size_t>(current)].parent;
    }

    return result;
}

i32 FindMeshNode(const Model& model, const i32 meshIndex) {
    for (std::size_t i = 0; i < model.getNodes().size(); ++i) {
        if (model.getNodes()[i].mesh == meshIndex) return static_cast<i32>(i);
    }
    return -1;
}

const ModelAnimation* PickAnimation(const std::vector<ModelAnimation>& animations,
                                    const std::string_view name = {}) {
    if (animations.empty()) return nullptr;

    if (name.empty()) return &animations.front();

    for (const ModelAnimation& animation : animations) {
        if (animation.getName() == name) return &animation;
    }

    return nullptr;
}

void RetargetAnimation(ModelAnimation& animation, const Model& model) {
    std::unordered_map<std::string, i32> nodes;
    nodes.reserve(model.getNodes().size());

    for (std::size_t i = 0; i < model.getNodes().size(); ++i) {
        nodes.emplace(model.getNodes()[i].name, static_cast<i32>(i));
    }

    std::vector<AnimationChannel> channels;
    channels.reserve(animation.getChannels().size());

    for (AnimationChannel channel : animation.getChannels()) {
        const auto found = nodes.find(channel.nodeName);

        if (found != nodes.end()) {
            channel.node = found->second;
        } else if (!channel.nodeName.empty() || channel.node < 0 ||
                   channel.node >= static_cast<i32>(model.getNodes().size())) {
            continue;
        }

        channels.push_back(std::move(channel));
    }

    animation.setChannels(std::move(channels));
}

f32 SampleScalar(const AnimationSampler& sampler, const f32 time, const u32 component) {
    if (sampler.getKeyCount() == 0) return 0.0f;

    if (time <= sampler.input.front()) return sampler.getScalar(0, 0, component);
    if (time >= sampler.input.back()) return sampler.getScalar(sampler.getKeyCount() - 1, 0, component);

    const auto upper = std::upper_bound(sampler.input.begin(), sampler.input.end(), time);
    const auto next = static_cast<u32>(std::distance(sampler.input.begin(), upper));
    const u32 previous = next - 1;

    const f32 t0 = sampler.input[previous];
    const f32 t1 = sampler.input[next];
    const f32 value0 = sampler.getScalar(previous, 0, component);
    const f32 value1 = sampler.getScalar(next, 0, component);

    if (sampler.interpolation == AnimationInterpolation::Step || t1 <= t0) {
        return value0;
    }

    const f32 factor = std::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f);
    return value0 + (value1 - value0) * factor;
}

vec3 SampleVec3(const AnimationSampler& sampler, const f32 time) {
    return vec3(SampleScalar(sampler, time, 0), SampleScalar(sampler, time, 1),
                SampleScalar(sampler, time, 2));
}

quat SampleQuat(const AnimationSampler& sampler, const f32 time) {
    const quat result(SampleScalar(sampler, time, 3), SampleScalar(sampler, time, 0),
                      SampleScalar(sampler, time, 1), SampleScalar(sampler, time, 2));

    const f32 length = glm::length(result);
    return length > 0.0001f ? result / length : quat(1.0f, 0.0f, 0.0f, 0.0f);
}

std::vector<mat4> BuildLocalTransforms(const Model& model, const ModelAnimation& animation,
                                       const f32 time) {
    std::vector<mat4> local(model.getNodes().size(), mat4(1.0f));

    for (std::size_t i = 0; i < model.getNodes().size(); ++i) {
        vec3 nodeScale(1.0f);
        quat nodeRotation(1.0f, 0.0f, 0.0f, 0.0f);
        vec3 nodeTranslation(0.0f);
        vec3 skew(0.0f);
        vec4 perspective(0.0f);

        glm::decompose(model.getNodes()[i].localTransform, nodeScale, nodeRotation, nodeTranslation,
                       skew, perspective);

        for (const AnimationChannel& channel : animation.getChannels()) {
            if (channel.node != static_cast<i32>(i) || channel.sampler < 0 ||
                channel.sampler >= static_cast<i32>(animation.getSamplers().size())) {
                continue;
            }

            const AnimationSampler& sampler =
                animation.getSamplers()[static_cast<std::size_t>(channel.sampler)];

            switch (channel.path) {
            case AnimationPath::Translation:
                nodeTranslation = SampleVec3(sampler, time);
                break;
            case AnimationPath::Rotation:
                nodeRotation = SampleQuat(sampler, time);
                break;
            case AnimationPath::Scale:
                nodeScale = SampleVec3(sampler, time);
                break;
            case AnimationPath::Weights:
                break;
            }
        }

        local[i] = glm::translate(mat4(1.0f), nodeTranslation) * glm::mat4_cast(nodeRotation) *
                   glm::scale(mat4(1.0f), nodeScale);
    }

    return local;
}

std::vector<mat4> BuildWorldTransforms(const Model& model, const std::vector<mat4>& local) {
    std::vector<mat4> world(model.getNodes().size(), mat4(1.0f));
    std::vector<bool> done(model.getNodes().size(), false);

    std::function<mat4(i32)> resolve = [&](const i32 index) -> mat4 {
        if (index < 0 || index >= static_cast<i32>(model.getNodes().size())) return mat4(1.0f);
        if (done[static_cast<std::size_t>(index)]) return world[static_cast<std::size_t>(index)];

        const ModelNode& node = model.getNodes()[static_cast<std::size_t>(index)];
        const mat4 parent = resolve(node.parent);
        world[static_cast<std::size_t>(index)] = parent * local[static_cast<std::size_t>(index)];
        done[static_cast<std::size_t>(index)] = true;

        return world[static_cast<std::size_t>(index)];
    };

    for (std::size_t i = 0; i < model.getNodes().size(); ++i) {
        resolve(static_cast<i32>(i));
    }

    return world;
}

std::vector<mat4> BuildBoneMatrices(const Model& model, const i32 skinIndex,
                                    const std::vector<mat4>& world) {
    std::vector<mat4> bones(128, mat4(1.0f));

    if (skinIndex < 0 || skinIndex >= static_cast<i32>(model.getSkins().size())) return bones;

    const ModelSkin& skin = model.getSkins()[static_cast<std::size_t>(skinIndex)];
    const std::size_t count = std::min({skin.joints.size(), skin.inverseBindMatrices.size(),
                                        bones.size()});

    for (std::size_t i = 0; i < count; ++i) {
        const i32 joint = skin.joints[i];
        if (joint < 0 || joint >= static_cast<i32>(world.size())) continue;

        bones[i] = world[static_cast<std::size_t>(joint)] * skin.inverseBindMatrices[i];
    }

    return bones;
}

std::optional<ModelInstance> LoadInstance(const std::filesystem::path& path,
                                          const f32 rotationOffset) {
    auto result = ModelIO::Load(path);
    if (!result) {
        SLOGE("%s: %s\n", path.string().c_str(), result.error().what());
        return std::nullopt;
    }

    ModelInstance instance;
    instance.model = std::move(*result);
    instance.rotationOffset = rotationOffset;

    instance.meshes.resize(instance.model.getMeshes().size());

    for (std::size_t i = 0; i < instance.model.getMeshes().size(); ++i) {
        GpuMesh& gpu = instance.meshes[i];
        const Mesh& mesh = instance.model.getMeshes()[i];
        const i32 node = mesh.getNodeIndex() >= 0
                             ? mesh.getNodeIndex()
                             : FindMeshNode(instance.model, static_cast<i32>(i));
        gpu.nodeTransform = NodeWorldTransform(instance.model, node);
        gpu.skinIndex =
            node >= 0 ? instance.model.getNodes()[static_cast<std::size_t>(node)].skin : -1;
        gpu.isSkinned =
            gpu.skinIndex >= 0 &&
            gpu.skinIndex < static_cast<i32>(instance.model.getSkins().size()) &&
            !instance.model.getSkins()[static_cast<std::size_t>(gpu.skinIndex)].joints.empty();

        if (!UploadMesh(instance.model.getMeshes()[i], gpu)) {
            SLOGE("%s: Mesh konnte nicht hochgeladen werden\n", path.string().c_str());
            return std::nullopt;
        }
    }

    vec3 minimum(std::numeric_limits<f32>::max());
    vec3 maximum(-std::numeric_limits<f32>::max());
    bool hasBounds = false;

    for (std::size_t i = 0; i < instance.model.getMeshes().size(); ++i) {
        const Mesh& mesh = instance.model.getMeshes()[i];
        const mat4 transform = instance.meshes[i].nodeTransform;

        for (const MeshVertex& vertex : mesh.getVertices()) {
            const vec3 point = vec3(transform * vec4(vertex.getPosition(), 1.0f));
            minimum = glm::min(minimum, point);
            maximum = glm::max(maximum, point);
            hasBounds = true;
        }
    }

    if (!hasBounds) return std::nullopt;

    instance.center = (minimum + maximum) * 0.5f;
    const f32 height = maximum.y - minimum.y;
    instance.scale = height > 0.0001f ? kTargetHeight / height : 1.0f;

    SLOGI("%s: %zu Mesh(es), %zu Nodes, %zu Skins\n", path.filename().string().c_str(),
          instance.model.getMeshes().size(), instance.model.getNodes().size(), instance.model.getSkins().size());

    return instance;
}

void DrawInstance(const ModelInstance& instance, const Shader& shader, const f32 time,
                  const f32 xPosition) {
    const f32 angle = time * 0.6f + instance.rotationOffset;

    const mat4 user =
        glm::translate(mat4(1.0f), vec3(xPosition, kTargetHeight * 0.5f, 0.0f)) *
        glm::rotate(mat4(1.0f), angle, vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(mat4(1.0f), vec3(instance.scale)) *
        glm::translate(mat4(1.0f), -instance.center);

    const f32 animationTime =
        instance.animation.getDuration() > 0.0f
            ? std::fmod(time, instance.animation.getDuration())
            : 0.0f;

    const std::vector<mat4> local =
        BuildLocalTransforms(instance.model, instance.animation, animationTime);
    const std::vector<mat4> world = BuildWorldTransforms(instance.model, local);

    for (std::size_t i = 0; i < instance.meshes.size(); ++i) {
        const GpuMesh& mesh = instance.meshes[i];

        shader.setUniform("uNormalScale", mesh.material.normalScale);
        shader.setUniform("uOcclusionStrength", mesh.material.occlusionStrength);
        shader.setUniform("uEmissiveStrength", mesh.material.emissiveStrength);
        shader.setUniform("uAlphaMode", mesh.material.alphaMode);
        shader.setUniform("uAlphaCutoff", mesh.material.alphaCutoff);

        if (mesh.isSkinned) {
            const std::vector<mat4> bones =
                BuildBoneMatrices(instance.model, mesh.skinIndex, world);

            shader.setUniform("uBones", bones.data(), static_cast<GLsizei>(bones.size()));
            shader.setUniform("uSkinned", 1);
            shader.setUniform("uModel", user);
        } else {
            shader.setUniform("uSkinned", 0);
            shader.setUniform("uModel", user * mesh.nodeTransform);
        }

        mesh.material.albedo.use(0);
        mesh.material.metallic.use(1);
        mesh.material.roughness.use(2);
        mesh.material.ao.use(3);
        mesh.material.normal.use(4);
        mesh.material.emissive.use(5);

        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
}

void RenderScene(const Shader& shader, const ModelInstance& left, const ModelInstance& middle,
                 const ModelInstance& right, const ModelInstance& obj, const f32 time,
                 const ivec2 framebuffer) {
    glViewport(0, 0, framebuffer.x, framebuffer.y);
    glClearColor(0.045f, 0.055f, 0.075f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const f32 aspect =
        static_cast<f32>(framebuffer.x) / static_cast<f32>(std::max(1, framebuffer.y));

    const vec3 cameraPosition(0.0f, 1.25f, 6.6f);
    const mat4 view = glm::lookAt(cameraPosition, vec3(0.0f, 0.85f, 0.0f),
                                  vec3(0.0f, 1.0f, 0.0f));
    const mat4 projection =
        glm::perspective(glm::radians(45.0f), aspect, 0.05f, 100.0f);

    shader.use();
    shader.setUniform("uView", view);
    shader.setUniform("uProjection", projection);
    shader.setUniform("uViewPos", cameraPosition);
    shader.setUniform("uLightDir", glm::normalize(vec3(-0.5f, -1.0f, -0.4f)));
    shader.setUniform("uLightColor", vec3(3.2f));

    shader.setUniform("uAlbedo", 0);
    shader.setUniform("uMetallic", 1);
    shader.setUniform("uRoughness", 2);
    shader.setUniform("uAO", 3);
    shader.setUniform("uNormal", 4);
    shader.setUniform("uEmissive", 5);

    DrawInstance(left, shader, time, -1.5f * kModelSpacing);
    DrawInstance(middle, shader, time, -0.5f * kModelSpacing);
    DrawInstance(right, shader, time, 0.5f * kModelSpacing);
    DrawInstance(obj, shader, time, 1.5f * kModelSpacing);
}

void SaveFrame(const Window& window, const std::filesystem::path& path) {
    const ivec2 size = window.getFramebufferSize();
    if (size.x <= 0 || size.y <= 0) return;

    std::vector<u8> pixels(static_cast<std::size_t>(size.x) * size.y * 4);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    Image image(size.x, size.y, ImageFormat::Rgba,
                std::span<const u8>(pixels.data(), pixels.size()));
    image.flipVertically();
    ImageIO::Save(image, path);

    SLOGI("Bild gespeichert: %s\n", path.string().c_str());
}

struct Options {
    bool headless{false};
    std::filesystem::path frame;
    f32 time{0.0f};
};

Options ParseOptions(const int argc, char** argv) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--frame" && i + 1 < argc) {
            options.headless = true;
            options.frame = argv[++i];
        } else if (argument == "--time" && i + 1 < argc) {
            options.time = static_cast<f32>(std::atof(argv[++i]));
        }
    }

    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = ParseOptions(argc, argv);

    WindowHints hints;
    hints.width = 1280;
    hints.height = 720;
    // Im Headless-Modus wird in ein eigenes FBO gerendert; MSAA im
    // Standard-Framebuffer ist dort weder nötig noch überall verfügbar.
    hints.samples = options.headless ? 0 : 4;
    hints.visible = !options.headless;

    std::unique_ptr<Window> window =
        example::CreateWindow("sgl - Model Viewer", hints.width, hints.height, hints);
    if (!window) return 1;

    example::CloseOnEscape(*window);

    if (!glCreateVertexArrays || !glCreateTextures || !glNamedBufferData ||
        !glVertexArrayElementBuffer) {
        SLOGE("Dieses Beispiel braucht OpenGL 4.5+ (Direct State Access).\n");
        window.reset();
        sgl::Terminate();
        return 1;
    }

    ShaderSourceOptions shaderOptions;
    shaderOptions.version = 330;
    Shader shader = Shader::FromSource(kModelShader, shaderOptions);
    if (!shader.isValid()) {
        SLOGE("Modelshader: %s\n", shader.getLastError().c_str());
        window.reset();
        sgl::Terminate();
        return 1;
    }

    const std::filesystem::path resources = SGL_MODEL_RESOURCES;

    auto left = LoadInstance(resources / "character.glb", 0.0f);
    auto middle = LoadInstance(resources / "turrican.glb", 3.14159265f);
    auto right = LoadInstance(resources / "zombie.fbx", 1.57079633f);
    // OBJ hat keine Animation; die Figur wird nur ueber die Zeit gedreht.
    auto obj = LoadInstance(resources / "FuturisticGirl.obj", 0.78539816f);

    if (!left || !middle || !right || !obj) {
        window.reset();
        sgl::Terminate();
        return 1;
    }

    const auto characterAnimations = ModelAnimationIO::Load(resources / "character.glb");
    if (characterAnimations) {
        if (const ModelAnimation* animation = PickAnimation(*characterAnimations)) {
            left->animation = *animation;
            SLOGI("character.glb: Animation \"%s\" (%.2f s)\n", animation->getName().c_str(),
                  animation->getDuration());
        }
    } else {
        SLOGW("character.glb: Animationen nicht ladbar: %s\n",
              characterAnimations.error().what());
    }

    const auto turricanAnimations = ModelAnimationIO::Load(resources / "turrican.glb");
    if (turricanAnimations) {
        if (const ModelAnimation* animation = PickAnimation(*turricanAnimations, "Walking")) {
            middle->animation = *animation;
            SLOGI("turrican.glb: Animation \"%s\" (%.2f s)\n", animation->getName().c_str(),
                  animation->getDuration());
        }
    } else {
        SLOGW("turrican.glb: Animationen nicht ladbar: %s\n", turricanAnimations.error().what());
    }

    const auto walkAnimations = ModelAnimationIO::Load(resources / "walk.fbx");
    if (walkAnimations) {
        if (const ModelAnimation* animation = PickAnimation(*walkAnimations)) {
            right->animation = *animation;
            RetargetAnimation(right->animation, right->model);
            SLOGI("zombie.fbx: Animation \"%s\" aus walk.fbx (%.2f s)\n",
                  right->animation.getName().c_str(), right->animation.getDuration());
        }
    } else {
        SLOGW("walk.fbx: Animationen nicht ladbar: %s\n", walkAnimations.error().what());
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const f64 startTime = sgl::GetTime();

    while (!window->isCloseRequested()) {
        const f32 time = options.headless
                             ? options.time
                             : static_cast<f32>(sgl::GetTime() - startTime);

        RenderScene(shader, *left, *middle, *right, *obj, time, window->getFramebufferSize());

        if (options.headless) {
            SaveFrame(*window, options.frame);
            break;
        }

        window->swapBuffers();
        sgl::PollEvents();
    }

    window.reset();
    sgl::Terminate();
    return 0;
}
