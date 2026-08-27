#pragma once

#include <cstdint>

namespace lvgl_integration {

class LVGLTick final {
public:
    static void install();

private:
    static std::uint32_t get();
};

}
