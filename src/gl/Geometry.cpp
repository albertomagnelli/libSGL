#include <sgl/Geometry.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace sgl {

// ─── Grunddaten ─────────────────────────────────────────────────────────────

Rectangle::Rectangle(f32 positionX, f32 positionY, f32 rectangleWidth, f32 rectangleHeight)
    : x(positionX), y(positionY), width(rectangleWidth), height(rectangleHeight) {}

vec2 Rectangle::getPosition() const {
    return {x, y};
}

vec2 Rectangle::getSize() const {
    return {width, height};
}

vec2 Rectangle::getCenter() const {
    return {x + (width * 0.5f), y + (height * 0.5f)};
}

f32 Rectangle::getLeft() const {
    return x;
}

f32 Rectangle::getTop() const {
    return y;
}

f32 Rectangle::getRight() const {
    return x + width;
}

f32 Rectangle::getBottom() const {
    return y + height;
}

bool Rectangle::isEmpty() const {
    return (width <= 0.0f) || (height <= 0.0f);
}

// ─── Kollision ──────────────────────────────────────────────────────────────

bool Rectangle::contains(const vec2& point) const {
    return (point.x >= x) && (point.x <= (x + width)) && (point.y >= y) &&
           (point.y <= (y + height));
}

bool Rectangle::containsStrict(const vec2& point) const {
    return (point.x > x) && (point.x < (x + width)) && (point.y > y) && (point.y < (y + height));
}

bool Rectangle::contains(const Rectangle& other) const {
    return (other.x >= x) && (other.y >= y) && (other.getRight() <= getRight()) &&
           (other.getBottom() <= getBottom());
}

bool Rectangle::overlaps(const Rectangle& other) const {
    return (x < other.getRight()) && (other.x < getRight()) && (y < other.getBottom()) &&
           (other.y < getBottom());
}

Rectangle Rectangle::intersection(const Rectangle& other) const {
    const f32 left = std::max(x, other.x);
    const f32 top = std::max(y, other.y);
    const f32 right = std::min(this->getRight(), other.getRight());
    const f32 bottom = std::min(this->getBottom(), other.getBottom());

    if ((right <= left) || (bottom <= top)) {
        return {left, top, 0.0f, 0.0f};
    }
    return {left, top, right - left, bottom - top};
}

Rectangle Rectangle::united(const Rectangle& other) const {
    const f32 left = std::min(x, other.x);
    const f32 top = std::min(y, other.y);
    const f32 right = std::max(this->getRight(), other.getRight());
    const f32 bottom = std::max(this->getBottom(), other.getBottom());

    return {left, top, right - left, bottom - top};
}

f32 Rectangle::overlapArea(const Rectangle& other) const {
    const Rectangle overlap = intersection(other);
    return overlap.width * overlap.height;
}

bool Rectangle::equals(const Rectangle& other, const f32 tolerance) const {
    return (std::abs(x - other.x) <= tolerance) && (std::abs(y - other.y) <= tolerance) &&
           (std::abs(width - other.width) <= tolerance) &&
           (std::abs(height - other.height) <= tolerance);
}

// ─── Gedrehte Rechtecke ──────────────────────────────────────────────────────

bool Rectangle::containsRotated(const vec2& point, const vec2& origin,
                                const f32 rotationDegrees) const {
    const vec2 pivot(x + origin.x, y + origin.y);
    const vec2 local = Rotate(point - pivot, -rotationDegrees) + pivot;
    return contains(local);
}

bool Rectangle::OverlapsRotated(const Rectangle& a, const vec2& aOrigin, const f32 aRotation,
                                const Rectangle& b, const vec2& bOrigin, const f32 bRotation) {
    const vec2 cornersA[4] = {a.cornerRotated(0, aOrigin, aRotation),
                              a.cornerRotated(1, aOrigin, aRotation),
                              a.cornerRotated(2, aOrigin, aRotation),
                              a.cornerRotated(3, aOrigin, aRotation)};
    const vec2 cornersB[4] = {b.cornerRotated(0, bOrigin, bRotation),
                              b.cornerRotated(1, bOrigin, bRotation),
                              b.cornerRotated(2, bOrigin, bRotation),
                              b.cornerRotated(3, bOrigin, bRotation)};

    // vier Trennachsen: je zwei Kantenrichtungen (entartete Rechtecke haben
    // keine Richtung – dann bleibt es bei der Achse der Gegenseite)
    const auto safeNormalize = [](const vec2& vector) -> vec2 {
        const f32 length = glm::length(vector);
        return (length < 1e-6f) ? vec2(1.0f, 0.0f) : (vector / length);
    };

    const vec2 axes[4] = {safeNormalize(cornersA[1] - cornersA[0]),
                          safeNormalize(cornersA[3] - cornersA[0]),
                          safeNormalize(cornersB[1] - cornersB[0]),
                          safeNormalize(cornersB[3] - cornersB[0])};

    for (const vec2& axis : axes) {
        f32 minA = glm::dot(cornersA[0], axis);
        f32 maxA = minA;
        f32 minB = glm::dot(cornersB[0], axis);
        f32 maxB = minB;

        for (i32 i = 1; i < 4; ++i) {
            const f32 projectionA = glm::dot(cornersA[i], axis);
            const f32 projectionB = glm::dot(cornersB[i], axis);

            minA = std::min(minA, projectionA);
            maxA = std::max(maxA, projectionA);
            minB = std::min(minB, projectionB);
            maxB = std::max(maxB, projectionB);
        }

        // eine Lücke auf dieser Achse -> keine Überschneidung
        if ((maxA <= minB) || (maxB <= minA)) {
            return false;
        }
    }
    return true;
}

vec2 Rectangle::cornerRotated(const i32 index, const vec2& origin,
                              const f32 rotationDegrees) const {
    const vec2 pivot(x + origin.x, y + origin.y);
    const vec2 corners[4] = {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
    const vec2 local = corners[(index < 0 || index > 3) ? 0 : index] - pivot;
    return Rotate(local, rotationDegrees) + pivot;
}

vec2 Rectangle::Rotate(const vec2& point, const f32 degrees) {
    const f32 radians = degrees * (3.14159265358979323846f / 180.0f);
    const f32 cosine = std::cos(radians);
    const f32 sine = std::sin(radians);
    return {(point.x * cosine) - (point.y * sine), (point.x * sine) + (point.y * cosine)};
}

// ─── Freie Helfer ───────────────────────────────────────────────────────────

Rectangle RectangleFromCorners(const vec2& first, const vec2& second) {
    const vec2 topLeft = glm::min(first, second);
    const vec2 bottomRight = glm::max(first, second);
    return Rectangle{topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};
}

Rectangle RectangleFromSize(const vec2& position, const vec2& size) {
    return Rectangle{position.x, position.y, size.x, size.y};
}

}  // namespace sgl
