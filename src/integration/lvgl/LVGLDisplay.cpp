#include "LVGLDisplay.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>

namespace lvgl_integration {

LVGLDisplay::LVGLDisplay(platform::Window& window) noexcept
    : window_(&window)
{
}

LVGLDisplay::~LVGLDisplay() noexcept
{
    destroy_display();
}

bool LVGLDisplay::initialize(int width, int height) noexcept
{
    if (initialized_ || window_ == nullptr || !window_->is_open() || width <= 0 || height <= 0) {
        return false;
    }

    try {
        std::vector<std::uint8_t> buffer_a;
        std::vector<std::uint8_t> buffer_b;
        if (!allocate_buffers(width, height, buffer_a, buffer_b)) {
            return false;
        }

        lv_display_t* display = lv_display_create(width, height);
        if (display == nullptr) {
            return false;
        }

        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_user_data(display, this);
        lv_display_set_flush_cb(display, &LVGLDisplay::flush_callback);
        lv_display_set_buffers(display,
                               buffer_a.data(),
                               buffer_b.data(),
                               static_cast<std::uint32_t>(buffer_a.size()),
                               LV_DISPLAY_RENDER_MODE_FULL);

        const platform::TextureHandle texture =
            window_->create_texture(width, height, buffer_a.data());
        if (texture == 0U) {
            lv_display_delete(display);
            return false;
        }

        display_ = display;
        texture_ = texture;
        framebuffer_a_ = std::move(buffer_a);
        framebuffer_b_ = std::move(buffer_b);
        width_ = width;
        height_ = height;
        initialized_ = true;
        return true;
    }
    catch (const std::bad_alloc&) {
        return false;
    }
}

bool LVGLDisplay::resize(int width, int height) noexcept
{
    if (!initialized_ || window_ == nullptr || width <= 0 || height <= 0 ||
        (width == width_ && height == height_)) {
        return width == width_ && height == height_;
    }

    try {
        std::vector<std::uint8_t> new_buffer_a;
        std::vector<std::uint8_t> new_buffer_b;
        if (!allocate_buffers(width, height, new_buffer_a, new_buffer_b)) {
            return false;
        }

        const platform::TextureHandle new_texture =
            window_->create_texture(width, height, new_buffer_a.data());
        if (new_texture == 0U) {
            return false;
        }

        platform::TextureHandle old_texture = texture_;
        texture_ = new_texture;
        window_->destroy_texture(old_texture);
        framebuffer_a_.swap(new_buffer_a);
        framebuffer_b_.swap(new_buffer_b);
        width_ = width;
        height_ = height;

        lv_display_set_resolution(display_, width, height);
        lv_display_set_buffers(display_,
                               framebuffer_a_.data(),
                               framebuffer_b_.data(),
                               static_cast<std::uint32_t>(framebuffer_a_.size()),
                               LV_DISPLAY_RENDER_MODE_FULL);
        return true;
    }
    catch (const std::bad_alloc&) {
        return false;
    }
}

void LVGLDisplay::present() const noexcept
{
    if (!initialized_ || window_ == nullptr) {
        return;
    }

    window_->present(texture_, width_, height_);
}

bool LVGLDisplay::is_initialized() const noexcept
{
    return initialized_;
}

int LVGLDisplay::width() const noexcept
{
    return width_;
}

int LVGLDisplay::height() const noexcept
{
    return height_;
}

lv_display_t* LVGLDisplay::handle() const noexcept
{
    return display_;
}

platform::TextureHandle LVGLDisplay::texture() const noexcept
{
    return texture_;
}

void LVGLDisplay::flush_callback(lv_display_t* display,
                                 const lv_area_t* area,
                                 std::uint8_t* px_map)
{
    if (display == nullptr) {
        return;
    }

    auto* backend = static_cast<LVGLDisplay*>(lv_display_get_user_data(display));
    if (backend != nullptr) {
        backend->flush(area, px_map);
    }
    else {
        lv_display_flush_ready(display);
    }
}

void LVGLDisplay::flush(const lv_area_t* area, std::uint8_t* px_map) noexcept
{
    if (display_ == nullptr || window_ == nullptr || px_map == nullptr || area == nullptr) {
        if (display_ != nullptr) {
            lv_display_flush_ready(display_);
        }
        return;
    }

    if (area->x1 > area->x2 || area->y1 > area->y2) {
        lv_display_flush_ready(display_);
        return;
    }

    window_->update_texture(texture_, area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, px_map);
    lv_display_flush_ready(display_);
}

bool LVGLDisplay::allocate_buffers(int width,
                                   int height,
                                   std::vector<std::uint8_t>& buffer_a,
                                   std::vector<std::uint8_t>& buffer_b) const
{
    constexpr std::size_t bytes_per_pixel = 2U;
    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);

    if (width_size == 0U || height_size == 0U ||
        width_size > std::numeric_limits<std::size_t>::max() / height_size) {
        return false;
    }

    const std::size_t pixel_count = width_size * height_size;
    if (pixel_count > std::numeric_limits<std::size_t>::max() / bytes_per_pixel) {
        return false;
    }

    const std::size_t buffer_size = pixel_count * bytes_per_pixel;
    if (buffer_size > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    buffer_a.assign(buffer_size, 0U);
    buffer_b.assign(buffer_size, 0U);
    return true;
}

void LVGLDisplay::destroy_display() noexcept
{
    if (display_ != nullptr) {
        lv_display_delete(display_);
        display_ = nullptr;
    }

    if (window_ != nullptr) {
        window_->destroy_texture(texture_);
    }

    framebuffer_a_.clear();
    framebuffer_b_.clear();
    width_ = 0;
    height_ = 0;
    initialized_ = false;
}

}
