#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "../DisplayConfig.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace platform {

using TextureHandle = std::uint32_t;

struct MousePosition {
    float x = 0.0F;
    float y = 0.0F;
    bool inside = false;
};

class Window final {
public:
    Window(int width,
           int height,
           std::string title,
           lvgl_integration::PresentationMode presentation_mode =
               lvgl_integration::PresentationMode::Stretch,
           lvgl_integration::ScreenShape screen_shape =
               lvgl_integration::ScreenShape::Rectangle,
           int corner_radius = 0);
    ~Window() noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    bool open() noexcept;
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    void set_size(int width, int height) noexcept;

    void poll_input() noexcept;

    [[nodiscard]] bool was_resized() const noexcept;
    bool take_resize_event() noexcept;
    [[nodiscard]] int screen_width() const noexcept;
    [[nodiscard]] int screen_height() const noexcept;
    [[nodiscard]] int render_width() const noexcept;
    [[nodiscard]] int render_height() const noexcept;
    [[nodiscard]] MousePosition mouse_position(int content_width,
                                                 int content_height) const noexcept;
    [[nodiscard]] bool left_mouse_button_down() const noexcept;
    [[nodiscard]] bool is_key_down(int key) const noexcept;

    float take_mouse_wheel_y() noexcept;
    std::optional<int> take_key_pressed() noexcept;
    std::optional<unsigned int> take_char_pressed() noexcept;

    [[nodiscard]] TextureHandle create_texture(int width,
                                               int height,
                                               const void* pixels) const noexcept;
    void update_texture(TextureHandle texture,
                        int width,
                        int height,
                        const void* pixels) const noexcept;
    void destroy_texture(TextureHandle& texture) const noexcept;

    void begin_frame() const noexcept;
    void end_frame() noexcept;
    void present(TextureHandle texture, int content_width, int content_height) const noexcept;

    [[nodiscard]] GLFWwindow* native_handle() const noexcept;

private:
    static void glfw_error_callback(int code, const char* description) noexcept;
    static void framebuffer_size_callback(GLFWwindow* window,
                                          int width,
                                          int height) noexcept;
    static void key_callback(GLFWwindow* window,
                             int key,
                             int scancode,
                             int action,
                             int mods) noexcept;
    static void char_callback(GLFWwindow* window,
                              unsigned int codepoint) noexcept;
    static void scroll_callback(GLFWwindow* window,
                                double x_offset,
                                double y_offset) noexcept;

    void refresh_dimensions() noexcept;
    void calculate_presentation_viewport(int content_width,
                                         int content_height,
                                         int& viewport_x,
                                         int& viewport_y,
                                         int& viewport_width,
                                         int& viewport_height) const noexcept;
    bool point_inside_screen_shape(float x, float y, int content_width, int content_height) const noexcept;
    void load_window_icon() noexcept;
    void load_window_geometry() noexcept;
    void save_window_geometry() noexcept;
    bool initialize_present_pipeline() noexcept;
    void destroy_present_pipeline() noexcept;
    void pace_frame() noexcept;

    int requested_width_;
    int requested_height_;
    std::string title_;
    GLFWwindow* window_ = nullptr;
    bool glfw_initialized_ = false;
    bool open_ = false;
    lvgl_integration::PresentationMode presentation_mode_;
    lvgl_integration::ScreenShape screen_shape_;
    int corner_radius_;

    int screen_width_ = 1;
    int screen_height_ = 1;
    int render_width_ = 1;
    int render_height_ = 1;
    bool resized_ = false;
    int window_position_x_ = 0;
    int window_position_y_ = 0;
    int saved_window_width_ = 0;
    int saved_window_height_ = 0;
    bool saved_window_position_loaded_ = false;
    bool window_geometry_loaded_ = false;
    double next_frame_deadline_ = 0.0;

    MousePosition mouse_position_{};
    bool left_mouse_button_down_ = false;
    float mouse_wheel_y_ = 0.0F;
    std::deque<int> pressed_keys_;
    std::deque<unsigned int> pressed_chars_;

    std::uint32_t present_program_ = 0U;
    std::uint32_t present_vao_ = 0U;
    std::uint32_t present_vbo_ = 0U;
    int present_texture_location_ = -1;
    int present_screen_shape_location_ = -1;
    int present_mask_size_location_ = -1;
    int present_corner_radius_location_ = -1;
};

}
