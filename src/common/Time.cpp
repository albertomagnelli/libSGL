#include "sgl/Time.hpp"

#include <chrono>

namespace sgl {
namespace {

using Clock = std::chrono::steady_clock;

Clock::time_point g_epoch = Clock::now();
f64 g_offset = 0.0;

f64 elapsedSeconds() {
    return std::chrono::duration<f64>(Clock::now() - g_epoch).count();
}

} // namespace

f64 getTime() {
    return elapsedSeconds() + g_offset;
}

void setTime(f64 time) {
    g_offset = time - elapsedSeconds();
}

u64 getTimerValue() {
    const auto ticks = Clock::now().time_since_epoch().count();
    return static_cast<u64>(ticks);
}

u64 getTimerFrequency() {
    constexpr f64 frequency =
        static_cast<f64>(Clock::period::den) / static_cast<f64>(Clock::period::num);
    return static_cast<u64>(frequency);
}

namespace detail {
void resetTimeBase() {
    g_epoch = Clock::now();
    g_offset = 0.0;
}
} // namespace detail

} // namespace sgl
