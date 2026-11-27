#include "Playtest.hpp"

#include <algorithm>
#include <cmath>

namespace editor {
namespace {

constexpr f32 kMoveSpeed = 260.0f;
constexpr f32 kJumpSpeed = 560.0f;
constexpr f32 kGravity = 1600.0f;
constexpr f32 kMaxFall = 1400.0f;

} // namespace

void Playtest::reset(const Level &level) {
    m_width = std::max(level.tileW * 0.72f, 16.0f);
    m_height = std::max(level.tileH * 1.5f, 28.0f);
    m_x = static_cast<f32>(level.tileW) * 2.0f;
    m_y = static_cast<f32>(level.tileH) * 2.0f;
    m_vx = 0.0f;
    m_vy = 0.0f;
    m_grounded = false;

    for (const ObjectLayer &layer : level.objectLayers) {
        for (const Entity &entity : layer.entities) {
            if (entity.def != "Startpunkt" && entity.attrs.count("Start") == 0) continue;
            m_x = entity.x + (entity.w - m_width) * 0.5f;
            m_y = entity.y + entity.h - m_height;
            return;
        }
    }
}

bool Playtest::overlapsSolid(const Level &level, f32 x, f32 y) const {
    const int x0 = static_cast<int>(std::floor(x / static_cast<f32>(level.tileW)));
    const int x1 = static_cast<int>(std::floor((x + m_width - 0.001f) /
                                               static_cast<f32>(level.tileW)));
    const int y0 = static_cast<int>(std::floor(y / static_cast<f32>(level.tileH)));
    const int y1 = static_cast<int>(std::floor((y + m_height - 0.001f) /
                                               static_cast<f32>(level.tileH)));
    for (const TileLayer &layer : level.tileLayers) {
        if (!layer.visible) continue;
        for (int ty = y0; ty <= y1; ++ty) {
            for (int tx = x0; tx <= x1; ++tx) {
                if (tx < 0 || ty < 0 || tx >= level.width || ty >= level.height) continue;
                if (level.tileAt(layer, tx, ty) != 0) return true;
            }
        }
    }
    return false;
}

void Playtest::step(const Level &level, f32 dt, f32 move, bool jump) {
    dt = std::clamp(dt, 0.0f, 0.05f);
    m_vx = std::clamp(move, -1.0f, 1.0f) * kMoveSpeed;
    m_vy = std::min(m_vy + kGravity * dt, kMaxFall);

    if (jump && m_grounded) {
        m_vy = -kJumpSpeed;
        m_grounded = false;
    }

    // Horizontale Bewegung achsenweise auflösen.
    const f32 nextX = m_x + m_vx * dt;
    if (!overlapsSolid(level, nextX, m_y)) {
        m_x = nextX;
    } else {
        m_vx = 0.0f;
    }

    // Vertikale Bewegung; bei Bodenkontakt auf die Kachelkante schnappen.
    const f32 nextY = m_y + m_vy * dt;
    if (!overlapsSolid(level, m_x, nextY)) {
        m_y = nextY;
        m_grounded = false;
    } else {
        if (m_vy > 0.0f) {
            const f32 bottom = std::floor((nextY + m_height) / static_cast<f32>(level.tileH)) *
                               static_cast<f32>(level.tileH);
            m_y = bottom - m_height;
            m_grounded = true;
        } else if (m_vy < 0.0f) {
            const f32 top = std::floor(nextY / static_cast<f32>(level.tileH) + 1.0f) *
                            static_cast<f32>(level.tileH);
            m_y = top;
            m_grounded = false;
        }
        m_vy = 0.0f;
    }

    m_x = std::clamp(m_x, 0.0f, std::max(static_cast<f32>(level.width * level.tileW) - m_width, 0.0f));
    if (m_y > static_cast<f32>(level.height * level.tileH) + 400.0f) {
        reset(level);
    }
}

} // namespace editor
