#include <catch_amalgamated.hpp>

#include <array>
#include <string>

#include "common/GamepadMapping.hpp"

using namespace sgl;
using namespace sgl::detail;

TEST_CASE("SDL-Mapping wird geparst", "[gamepad][mapping]") {
    const std::string_view line =
        "030000006d0400001dc2000014400000,Logitech F310 Gamepad (XInput),"
        "a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,"
        "guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,"
        "rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,"
        "start:b7,x:b2,y:b3,platform:Linux,";

    GamepadMapping mapping;
    std::string platform;
    REQUIRE(ParseGamepadMapping(line, mapping, platform));
    REQUIRE(platform == "Linux");
    REQUIRE(mapping.guid == "030000006d0400001dc2000014400000");
    REQUIRE(mapping.name == "Logitech F310 Gamepad (XInput)");

    REQUIRE(mapping.buttons[static_cast<std::size_t>(GamepadButton::A)].type ==
            MappedInputType::Button);
    REQUIRE(mapping.buttons[static_cast<std::size_t>(GamepadButton::A)].index == 0);
    REQUIRE(mapping.buttons[static_cast<std::size_t>(GamepadButton::DPadUp)].type ==
            MappedInputType::HatBit);
    REQUIRE(mapping.buttons[static_cast<std::size_t>(GamepadButton::DPadUp)].index == 0x01);
    REQUIRE(mapping.buttons[static_cast<std::size_t>(GamepadButton::DPadLeft)].index == 0x08);

    REQUIRE(mapping.axes[static_cast<std::size_t>(GamepadAxis::LeftX)].type ==
            MappedInputType::Axis);
    REQUIRE(mapping.axes[static_cast<std::size_t>(GamepadAxis::LeftX)].index == 0);
    REQUIRE(mapping.axes[static_cast<std::size_t>(GamepadAxis::LeftX)].axisScale ==
            Catch::Approx(1.0f));
    REQUIRE(mapping.axes[static_cast<std::size_t>(GamepadAxis::LeftY)].index == 1);
    REQUIRE(mapping.axes[static_cast<std::size_t>(GamepadAxis::LeftTrigger)].index == 2);
    REQUIRE(mapping.axes[static_cast<std::size_t>(GamepadAxis::RightTrigger)].index == 5);
}

TEST_CASE("Mapping-Invertierung und Halbachsen", "[gamepad][mapping]") {
    GamepadMapping mapping;
    std::string platform;

    REQUIRE(ParseGamepadMapping(
        "03000000000000000000000000000000,Test,lefty:a1~,righty:-a2,lefttrigger:+a3,"
        "platform:Linux,",
        mapping, platform));

    const auto& leftY = mapping.axes[static_cast<std::size_t>(GamepadAxis::LeftY)];
    REQUIRE(leftY.axisScale == Catch::Approx(-1.0f));
    REQUIRE(leftY.axisOffset == Catch::Approx(0.0f));

    const auto& rightY = mapping.axes[static_cast<std::size_t>(GamepadAxis::RightY)];
    REQUIRE(rightY.axisScale == Catch::Approx(2.0f));
    REQUIRE(rightY.axisOffset == Catch::Approx(1.0f));

    const auto& leftTrigger = mapping.axes[static_cast<std::size_t>(GamepadAxis::LeftTrigger)];
    REQUIRE(leftTrigger.axisScale == Catch::Approx(2.0f));
    REQUIRE(leftTrigger.axisOffset == Catch::Approx(-1.0f));
}

TEST_CASE("Mapping wird auf Rohdaten angewendet", "[gamepad][mapping]") {
    GamepadMapping mapping;
    std::string platform;
    REQUIRE(ParseGamepadMapping(
        "03000000000000000000000000000000,Test,a:b0,b:b1,x:a0,leftx:a0,lefty:a1~,"
        "lefttrigger:b2,dpup:h0.1,platform:Linux,",
        mapping, platform));

    const std::array<f32, 2> axes = {0.5f, 0.25f};
    const std::array<u8, 3> buttons = {1, 0, 1};
    const std::array<JoystickHat, 1> hats = {JoystickHat::Up};

    const GamepadState state = ApplyGamepadMapping(mapping, axes, buttons, hats);

    REQUIRE(state.buttons[static_cast<std::size_t>(GamepadButton::A)] == 1);
    REQUIRE(state.buttons[static_cast<std::size_t>(GamepadButton::B)] == 0);
    REQUIRE(state.buttons[static_cast<std::size_t>(GamepadButton::X)] == 1);
    REQUIRE(state.buttons[static_cast<std::size_t>(GamepadButton::DPadUp)] == 1);
    REQUIRE(state.axes[static_cast<std::size_t>(GamepadAxis::LeftX)] == Catch::Approx(0.5f));
    REQUIRE(state.axes[static_cast<std::size_t>(GamepadAxis::LeftY)] == Catch::Approx(-0.25f));
    REQUIRE(state.axes[static_cast<std::size_t>(GamepadAxis::LeftTrigger)] ==
            Catch::Approx(1.0f));
}

TEST_CASE("SDL-kompatible GUIDs werden erzeugt", "[gamepad][guid]") {
    REQUIRE(MakeLinuxGamepadGuid(0x0003, 0x046d, 0xc21d, 0x4014, "Logitech Gamepad F310") ==
            "030000006d0400001dc2000014400000");
    REQUIRE(MakeCocoaGamepadGuid(0x045e, 0x028e, 0x0114, "Xbox 360 Controller") ==
            "030000005e0400008e02000014010000");
}

TEST_CASE("Eingebettete Datenbank findet bekannte Geräte", "[gamepad][database]") {
    auto& database = GamepadMappingDatabase::Instance();

    const auto f310 = database.find("030000006d0400001dc2000014400000", "Linux");
    REQUIRE(f310.has_value());
    REQUIRE(f310->name.find("F310") != std::string::npos);

    REQUIRE_FALSE(database.find("00000000000000000000000000000000", "Linux").has_value());
}

TEST_CASE("updateGamepadMappings ergänzt eigene Einträge", "[gamepad][database]") {
    auto& database = GamepadMappingDatabase::Instance();

    const std::string custom =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa,Mein Controller,a:b0,b:b1,leftx:a0,lefty:a1,"
        "platform:Linux,";
    REQUIRE(database.addMappings(custom));

    const auto mapping = database.find("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "Linux");
    REQUIRE(mapping.has_value());
    REQUIRE(mapping->name == "Mein Controller");
    REQUIRE(mapping->buttons[static_cast<std::size_t>(GamepadButton::A)].index == 0);
}
