#include "sgl/Time.hpp"

#include <chrono>

namespace sgl {
namespace {

using Clock = std::chrono::steady_clock;

Clock::time_point g_epoch = Clock::now();
f64 g_offset = 0.0;

f64 ElapsedSeconds() {
    return std::chrono::duration<f64>(Clock::now() - g_epoch).count();
}

} // namespace

f64 GetTime() {
    return ElapsedSeconds() + g_offset;
}

void SetTime(f64 time) {
    g_offset = time - ElapsedSeconds();
}

u64 GetTimerValue() {
    const auto ticks = Clock::now().time_since_epoch().count();
    return static_cast<u64>(ticks);
}

u64 GetTimerFrequency() {
    constexpr f64 frequency =
        static_cast<f64>(Clock::period::den) / static_cast<f64>(Clock::period::num);
    return static_cast<u64>(frequency);
}

namespace detail {
void ResetTimeBase() {
    g_epoch = Clock::now();
    g_offset = 0.0;
}
} // namespace detail

} // namespace sgl
