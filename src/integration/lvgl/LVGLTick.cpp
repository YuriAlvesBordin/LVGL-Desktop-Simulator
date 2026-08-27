#include "LVGLTick.hpp"

#include <lvgl.h>

#include <chrono>
#include <limits>

namespace lvgl_integration {
namespace {

using Clock = std::chrono::steady_clock;
Clock::time_point g_start_time;

}

void LVGLTick::install()
{
    g_start_time = Clock::now();
    lv_tick_set_cb(&LVGLTick::get);
}

std::uint32_t LVGLTick::get()
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - g_start_time
    ).count();

    if (elapsed <= 0) {
        return 0U;
    }

    constexpr auto max_tick = static_cast<long long>(std::numeric_limits<std::uint32_t>::max());
    if (elapsed >= max_tick) {
        return std::numeric_limits<std::uint32_t>::max();
    }

    return static_cast<std::uint32_t>(elapsed);
}

}
