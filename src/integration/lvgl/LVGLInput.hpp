#pragma once

#include <lvgl.h>

#include <cstdint>

namespace platform {
class Window;
}

namespace lvgl_integration {

class LVGLInput final {
public:
    LVGLInput(platform::Window& window, lv_display_t* display) noexcept;
    ~LVGLInput() noexcept;

    LVGLInput(const LVGLInput&) = delete;
    LVGLInput& operator=(const LVGLInput&) = delete;
    LVGLInput(LVGLInput&&) = delete;
    LVGLInput& operator=(LVGLInput&&) = delete;

    bool initialize() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept;
    [[nodiscard]] lv_indev_t* mouse_indev() const noexcept;
    [[nodiscard]] lv_indev_t* keyboard_indev() const noexcept;

private:
    static void mouse_read_callback(lv_indev_t* indev, lv_indev_data_t* data);
    static void keyboard_read_callback(lv_indev_t* indev, lv_indev_data_t* data);

    void read_mouse(lv_indev_data_t* data) noexcept;
    void read_keyboard(lv_indev_data_t* data) noexcept;
    [[nodiscard]] static std::uint32_t map_key(int glfw_key) noexcept;

    platform::Window* window_ = nullptr;
    lv_display_t* display_ = nullptr;
    lv_indev_t* mouse_indev_ = nullptr;
    lv_indev_t* keyboard_indev_ = nullptr;
    lv_group_t* keyboard_group_ = nullptr;
    std::uint32_t last_key_ = 0U;
    bool keyboard_needs_release_ = false;
    bool initialized_ = false;
};

}
