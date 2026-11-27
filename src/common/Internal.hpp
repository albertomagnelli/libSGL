#pragma once

#include "sgl/Init.hpp"

namespace sgl::detail {

struct InitHintsState {
    Platform platform = Platform::Any;
    bool joystickHatButtons = true;
    bool cocoaChdirResources = true;
    bool cocoaMenubar = true;
    bool waylandLibdecor = true;
    AnglePlatformType anglePlatformType = AnglePlatformType::None;
};

InitHintsState& getInitHints();
bool isInitialized();
void resetTimeBase();

} // namespace sgl::detail
