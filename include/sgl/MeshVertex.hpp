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
// Die Klasse ist ein Aggregate und kann direkt mit designated initializers
// gefüllt werden. Für Skin-Daten stehen kleine Hilfsmethoden bereit.
//
// Beispiel:
//
//   MeshVertex vertex{
//       .position    = vec3(1.0f, 2.0f, 3.0f),
//       .normal      = vec3(0.0f, 0.0f, 1.0f),
//       .uv          = vec2(0.25f, 0.75f),
//       .tangent     = vec4(1.0f, 0.0f, 0.0f, 1.0f),
//       .boneWeights = vec4(1.0f, 0.0f, 0.0f, 0.0f),
//       .boneIds     = uvec4(0u, 0u, 0u, 0u),
//   };
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

namespace sgl {

class MeshVertex {
public:
    static constexpr u32 kMaxBoneInfluences = 4;

    vec3 position{0.0f};
    vec3 normal{0.0f, 0.0f, 1.0f};
    vec2 uv{0.0f};

    // xyz = Tangentenrichtung, w = Handedness der Bitangente (+1 oder -1).
    vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};

    // Parallele Arrays: boneWeights[i] gehört zu boneIds[i].
    vec4 boneWeights{0.0f};
    uvec4 boneIds{0u};

    // True, wenn mindestens ein Bone-Gewicht größer als null ist.
    [[nodiscard]] bool hasSkinning() const noexcept;

    // Anzahl der Gewichte größer als null (0..4).
    [[nodiscard]] u32 influenceCount() const noexcept;

    // Setzt einen Bone-Einfluss im Slot 0..3. Negative Gewichte werden auf 0
    // geklemmt. Liefert false, wenn der Slot außerhalb von 0..3 liegt.
    bool setBoneInfluence(u32 slot, u32 boneId, f32 weight);

    // Normiert die vier Gewichte auf Summe 1. Bei Summe 0 werden alle
    // Bone-Einflüsse geleert.
    void normalizeBoneWeights();

    void clearBoneInfluences();
};

}  // namespace sgl
