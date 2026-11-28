#include <sgl/MeshVertex.hpp>

#include <algorithm>

namespace sgl {

bool MeshVertex::hasSkinning() const noexcept { return getInfluenceCount() > 0; }

u32 MeshVertex::getInfluenceCount() const noexcept {
  u32 count = 0;

  for (u32 i = 0; i < kMaxBoneInfluences; ++i) {
    if (m_boneWeights[i] > 0.0f) ++count;
  }

  return count;
}

bool MeshVertex::setBoneInfluence(const u32 slot, const u32 boneId, const f32 weight) {
  if (slot >= kMaxBoneInfluences) return false;

  m_boneIds[slot]     = boneId;
  m_boneWeights[slot] = std::max(0.0f, weight);

  return true;
}

void MeshVertex::normalizeBoneWeights() {
  const f32 sum = m_boneWeights.x + m_boneWeights.y + m_boneWeights.z + m_boneWeights.w;

  if (sum <= 0.0f) {
    clearBoneInfluences();

    return;
  }

  const f32 inverse = 1.0f / sum;
  m_boneWeights *= inverse;
}

void MeshVertex::clearBoneInfluences() {
  m_boneWeights = vec4(0.0f);
  m_boneIds     = uvec4(0u);
}

}  // namespace sgl
