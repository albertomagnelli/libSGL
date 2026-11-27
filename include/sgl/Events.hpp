#pragma once

#include "Types.hpp"

namespace sgl {

void pollEvents();
void waitEvents();
void waitEventsTimeout(f64 seconds);
void postEmptyEvent();

} // namespace sgl
