#include <catch_amalgamated.hpp>

#include <sgl/Color.hpp>
#include <sgl/Geometry.hpp>

using namespace sgl;

TEST_CASE("Color wandelt zwischen Farbmodellen", "[color]") {
    const Color red = FromHsv(Hsv{0.0f, 1.0f, 1.0f});
    REQUIRE(red.r == 255);
    REQUIRE(red.g == 0);
    REQUIRE(red.b == 0);

    const Hsv hsv = ToHsv(red);
    REQUIRE(hsv.h == Catch::Approx(0.0f).margin(0.5f));
    REQUIRE(hsv.s == Catch::Approx(1.0f).margin(0.01f));
    REQUIRE(hsv.v == Catch::Approx(1.0f).margin(0.01f));

    const Color mixed = Mix(RED, BLUE, 0.5f);
    REQUIRE(mixed.r > 0);
    REQUIRE(mixed.b > 0);

    REQUIRE(ToHex(Color{255, 0, 128, 255}) == "#ff0080");
    REQUIRE(ToHex(Color{255, 0, 128, 128}, true) == "#ff008080");
    REQUIRE(FromHex("#ff0080") == Color(255, 0, 128, 255));
}

TEST_CASE("Color-Hilfsfunktionen", "[color]") {
    REQUIRE(IsOpaque(WHITE));
    REQUIRE(IsTransparent(BLANK));
    REQUIRE(IsGray(Color{128, 128, 128, 255}));

    const Color premultiplied = Premultiply(Color{200, 100, 50, 128});
    const Color restored = Unpremultiply(premultiplied);
    REQUIRE(IsNearlyEqual(restored, Color{200, 100, 50, 128}, 2));

    REQUIRE(Luminance(WHITE) == Catch::Approx(1.0f).margin(0.01f));
    REQUIRE(Luminance(BLACK) == Catch::Approx(0.0f).margin(0.01f));
}

TEST_CASE("Rectangle-Kollisionen und Rotation", "[geometry]") {
    const Rectangle box(10.0f, 20.0f, 100.0f, 50.0f);
    REQUIRE(box.left() == 10.0f);
    REQUIRE(box.top() == 20.0f);
    REQUIRE(box.right() == 110.0f);
    REQUIRE(box.bottom() == 70.0f);
    REQUIRE(box.center() == vec2(60.0f, 45.0f));
    REQUIRE(box.contains(vec2(50.0f, 40.0f)));
    REQUIRE_FALSE(box.contains(vec2(0.0f, 0.0f)));
    REQUIRE(box.contains(Rectangle(20.0f, 30.0f, 20.0f, 20.0f)));
    REQUIRE_FALSE(box.contains(Rectangle(0.0f, 0.0f, 20.0f, 20.0f)));

    const Rectangle other(100.0f, 60.0f, 30.0f, 30.0f);
    REQUIRE(box.overlaps(other));
    const Rectangle overlap = box.intersection(other);
    REQUIRE(overlap.width == 10.0f);
    REQUIRE(overlap.height == 10.0f);
    REQUIRE(box.overlapArea(other) == 100.0f);

    const Rectangle united = box.united(other);
    REQUIRE(united.x == 10.0f);
    REQUIRE(united.y == 20.0f);
    REQUIRE(united.width == 120.0f);
    REQUIRE(united.height == 70.0f);

    const Rectangle corners = RectangleFromCorners(vec2(80.0f, 50.0f), vec2(20.0f, 10.0f));
    REQUIRE(corners.x == 20.0f);
    REQUIRE(corners.y == 10.0f);
    REQUIRE(corners.width == 60.0f);
    REQUIRE(corners.height == 40.0f);

    const vec2 rotated = Rectangle::Rotate(vec2(1.0f, 0.0f), 90.0f);
    REQUIRE(rotated.x == Catch::Approx(0.0f).margin(0.001f));
    REQUIRE(rotated.y == Catch::Approx(1.0f).margin(0.001f));
}
