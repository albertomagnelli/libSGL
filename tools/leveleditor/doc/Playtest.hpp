#pragma once

#include "Document.hpp"

namespace editor {

// Minimaler Playtest: eine Spielfigur mit Schwerkraft, Laufen und Springen
// gegen alle nicht-leeren Kacheln der sichtbaren Kachel-Layer.
class Playtest {
public:
    void reset(const Level &level);
    void step(const Level &level, f32 dt, f32 move, bool jump);

    [[nodiscard]] vec2 position() const { return {m_x, m_y}; }
    [[nodiscard]] vec2 velocity() const { return {m_vx, m_vy}; }
    [[nodiscard]] vec2 size() const { return {m_width, m_height}; }
    [[nodiscard]] bool isGrounded() const { return m_grounded; }

private:
    [[nodiscard]] bool overlapsSolid(const Level &level, f32 x, f32 y) const;

    f32 m_x = 0.0f;
    f32 m_y = 0.0f;
    f32 m_vx = 0.0f;
    f32 m_vy = 0.0f;
    f32 m_width = 24.0f;
    f32 m_height = 46.0f;
    bool m_grounded = false;
};

} // namespace editor
