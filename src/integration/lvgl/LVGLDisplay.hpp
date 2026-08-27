#pragma once

#include "platform/Window.hpp"

#include <lvgl.h>

#include <cstdint>
#include <vector>

namespace lvgl_integration {

class LVGLDisplay final {
public:
    explicit LVGLDisplay(platform::Window& window) noexcept;
    ~LVGLDisplay() noexcept;

    LVGLDisplay(const LVGLDisplay&) = delete;
    LVGLDisplay& operator=(const LVGLDisplay&) = delete;
    LVGLDisplay(LVGLDisplay&&) = delete;
    LVGLDisplay& operator=(LVGLDisplay&&) = delete;

    bool initialize(int width, int height) noexcept;
    bool resize(int width, int height) noexcept;
    void present() const noexcept;

    [[nodiscard]] bool is_initialized() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] lv_display_t* handle() const noexcept;
    [[nodiscard]] platform::TextureHandle texture() const noexcept;

private:
    static void flush_callback(lv_display_t* display,
                               const lv_area_t* area,
                               std::uint8_t* px_map);
    void flush(const lv_area_t* area, std::uint8_t* px_map) noexcept;

    bool allocate_buffers(int width,
                          int height,
                          std::vector<std::uint8_t>& buffer_a,
                          std::vector<std::uint8_t>& buffer_b) const;
    void destroy_display() noexcept;

    platform::Window* window_ = nullptr;
    lv_display_t* display_ = nullptr;
    platform::TextureHandle texture_ = 0U;
    std::vector<std::uint8_t> framebuffer_a_;
    std::vector<std::uint8_t> framebuffer_b_;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
};

}
