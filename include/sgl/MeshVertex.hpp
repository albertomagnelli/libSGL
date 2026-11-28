#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// sgl · MeshVertex
//
// CPU-seitiger Vertex für statische und geskinnte Netze. Die Attribute
// entsprechen den üblichen glTF-/GPU-Layouts:
//
//   position     vec3
//   normal       vec3
//   uv           vec2
//   tangent      vec4  (xyz = Tangente, w = Bitangenten-Handedness ±1)
//   boneWeights  vec4  (maximal vier Gewichte)
//   boneIds      uvec4 (maximal vier Bone-Indizes)
//
// Es gibt keine OpenGL-/Buffer-Bindung: MeshVertex beschreibt nur die Daten.
// Die Attribute werden über Getter/Setter gelesen und geschrieben. Für das
// GPU-Layout stehen statische Offset-Helfer bereit. Für Skin-Daten gibt es
// zusätzlich kleine Hilfsmethoden.
//
// Beispiel:
//
//   MeshVertex vertex;
//   vertex.setPosition(vec3(1.0f, 2.0f, 3.0f));
//   vertex.setNormal(vec3(0.0f, 0.0f, 1.0f));
//   vertex.setUv(vec2(0.25f, 0.75f));
//   vertex.setTangent(vec4(1.0f, 0.0f, 0.0f, 1.0f));
//   vertex.setBoneWeights(vec4(1.0f, 0.0f, 0.0f, 0.0f));
//   vertex.setBoneIds(uvec4(0u, 0u, 0u, 0u));
// ─────────────────────────────────────────────────────────────────────────────

#include <cstddef>

#include "Types.hpp"

namespace sgl {

class MeshVertex {
public:
    static constexpr u32 kMaxBoneInfluences = 4;

    // ── Attribute ────────────────────────────────────────────────────────────
    [[nodiscard]] const vec3& getPosition() const noexcept { return m_position; }
    void setPosition(const vec3& value) noexcept { m_position = value; }

    [[nodiscard]] const vec3& getNormal() const noexcept { return m_normal; }
    void setNormal(const vec3& value) noexcept { m_normal = value; }

    [[nodiscard]] const vec2& getUv() const noexcept { return m_uv; }
    void setUv(const vec2& value) noexcept { m_uv = value; }

    // xyz = Tangentenrichtung, w = Handedness der Bitangente (+1 oder -1).
    [[nodiscard]] const vec4& getTangent() const noexcept { return m_tangent; }
    void setTangent(const vec4& value) noexcept { m_tangent = value; }

    // Parallele Arrays: boneWeights[i] gehört zu boneIds[i].
    [[nodiscard]] const vec4& getBoneWeights() const noexcept { return m_boneWeights; }
    void setBoneWeights(const vec4& value) noexcept { m_boneWeights = value; }

    [[nodiscard]] const uvec4& getBoneIds() const noexcept { return m_boneIds; }
    void setBoneIds(const uvec4& value) noexcept { m_boneIds = value; }

    // ── GPU-Layout ───────────────────────────────────────────────────────────
    // Byte-Offsets für glVertexArrayAttribFormat; ersetzt offsetof auf private
    // Member von außerhalb der Klasse.
    [[nodiscard]] static constexpr std::size_t PositionOffset() noexcept {
        return offsetof(MeshVertex, m_position);
    }
    [[nodiscard]] static constexpr std::size_t NormalOffset() noexcept {
        return offsetof(MeshVertex, m_normal);
    }
    [[nodiscard]] static constexpr std::size_t UvOffset() noexcept { return offsetof(MeshVertex, m_uv); }
    [[nodiscard]] static constexpr std::size_t TangentOffset() noexcept {
        return offsetof(MeshVertex, m_tangent);
    }
    [[nodiscard]] static constexpr std::size_t BoneWeightsOffset() noexcept {
        return offsetof(MeshVertex, m_boneWeights);
    }
    [[nodiscard]] static constexpr std::size_t BoneIdsOffset() noexcept {
        return offsetof(MeshVertex, m_boneIds);
    }

    // True, wenn mindestens ein Bone-Gewicht größer als null ist.
    [[nodiscard]] bool hasSkinning() const noexcept;

    // Anzahl der Gewichte größer als null (0..4).
    [[nodiscard]] u32 getInfluenceCount() const noexcept;

    // Setzt einen Bone-Einfluss im Slot 0..3. Negative Gewichte werden auf 0
    // geklemmt. Liefert false, wenn der Slot außerhalb von 0..3 liegt.
    bool setBoneInfluence(u32 slot, u32 boneId, f32 weight);

    // Normiert die vier Gewichte auf Summe 1. Bei Summe 0 werden alle
    // Bone-Einflüsse geleert.
    void normalizeBoneWeights();

    void clearBoneInfluences();

private:
    vec3  m_position{0.0f};
    vec3  m_normal{0.0f, 0.0f, 1.0f};
    vec2  m_uv{0.0f};
    vec4  m_tangent{1.0f, 0.0f, 0.0f, 1.0f};
    vec4  m_boneWeights{0.0f};
    uvec4 m_boneIds{0u};
};

}  // namespace sgl
