#include <sgl/MeshVertex.hpp>

#include <algorithm>

namespace sgl {

bool MeshVertex::hasSkinning() const noexcept {
  return influenceCount() > 0;
}

u32 MeshVertex::influenceCount() const noexcept {
  u32 count = 0;

  for (u32 i = 0; i < kMaxBoneInfluences; ++i) {
    if (boneWeights[i] > 0.0f) ++count;
  }

  return count;
}

bool MeshVertex::setBoneInfluence(const u32 slot, const u32 boneId, const f32 weight) {
  if (slot >= kMaxBoneInfluences) return false;

  boneIds[slot]     = boneId;
  boneWeights[slot] = std::max(0.0f, weight);

  return true;
}

void MeshVertex::normalizeBoneWeights() {
  const f32 sum = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;

  if (sum <= 0.0f) {
    clearBoneInfluences();

    return;
  }

  const f32 inverse = 1.0f / sum;
  boneWeights *= inverse;
}

void MeshVertex::clearBoneInfluences() {
  boneWeights = vec4(0.0f);
  boneIds     = uvec4(0u);
}

}  // namespace sgl
