#pragma once

#include "Types.hpp"

namespace sgl {

void PollEvents();
void WaitEvents();
void WaitEventsTimeout(f64 seconds);
void PostEmptyEvent();

} // namespace sgl
