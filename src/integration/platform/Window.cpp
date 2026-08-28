#include "Window.hpp"

#include <glad/gl.h>
#include "WindowIcon.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include <vector>
#include <utility>

namespace platform {
namespace {

constexpr char vertex_shader_source[] =
    "#version " LVGL_GLFW_OPENGL_GLSL_VERSION " core\n"
    R"GLSL(
layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_texcoord;
out vec2 v_texcoord;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
}
)GLSL";

constexpr char fragment_shader_source[] =
    "#version " LVGL_GLFW_OPENGL_GLSL_VERSION " core\n"
    R"GLSL(
in vec2 v_texcoord;
out vec4 out_color;
uniform sampler2D u_texture;
uniform int u_screen_shape;
uniform vec2 u_mask_size;
uniform float u_corner_radius;

float rounded_box_distance(vec2 point, vec2 half_size, float radius)
{
    vec2 distance_to_corner = abs(point) - half_size + vec2(radius);
    return length(max(distance_to_corner, vec2(0.0))) +
           min(max(distance_to_corner.x, distance_to_corner.y), 0.0) - radius;
}

void main()
{
    vec4 color = texture(u_texture, v_texcoord);
    vec2 point = v_texcoord * u_mask_size - u_mask_size * 0.5;
    float distance_to_edge = 0.0;

    if (u_screen_shape == 1) {
        vec2 half_size = u_mask_size * 0.5;
        float radius = min(u_corner_radius, min(half_size.x, half_size.y));
        distance_to_edge = rounded_box_distance(point, half_size, radius);
    }
    else if (u_screen_shape == 2) {
        distance_to_edge = length(point) - min(u_mask_size.x, u_mask_size.y) * 0.5;
    }

    float coverage = u_screen_shape == 0
        ? 1.0
        : 1.0 - smoothstep(-1.0, 1.0, distance_to_edge);
    color.a *= coverage;
    if (color.a <= 0.001) {
        discard;
    }
    out_color = color;
}
)GLSL";

std::string window_geometry_path() noexcept
{
    const char* state_home = std::getenv("XDG_STATE_HOME");
    if (state_home != nullptr && *state_home != '\0') {
        return std::string{state_home} + "/lvgl-glfw-window-geometry";
    }

    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return std::string{home} + "/.lvgl-glfw-window-geometry";
    }

    return {};
}

std::uint32_t compile_shader(GLenum type, const char* source) noexcept
{
    const std::uint32_t shader = glCreateShader(type);
    if (shader == 0U) {
        return 0U;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        char log[1024]{};
        GLsizei length = 0;
        glGetShaderInfoLog(shader, sizeof(log), &length, log);
        std::cerr << "GLSL shader compilation failed: " << log << '\n';
        glDeleteShader(shader);
        return 0U;
    }

    return shader;
}

std::uint32_t link_program(std::uint32_t vertex_shader,
                           std::uint32_t fragment_shader) noexcept
{
    const std::uint32_t program = glCreateProgram();
    if (program == 0U) {
        return 0U;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        char log[1024]{};
        GLsizei length = 0;
        glGetProgramInfoLog(program, sizeof(log), &length, log);
        std::cerr << "GLSL program link failed: " << log << '\n';
        glDeleteProgram(program);
        return 0U;
    }

    return program;
}

}

Window::Window(int width,
               int height,
               std::string title,
               lvgl_integration::PresentationMode presentation_mode,
               lvgl_integration::ScreenShape screen_shape,
               int corner_radius)
    : requested_width_(width),
      requested_height_(height),
      title_(std::move(title)),
      presentation_mode_(presentation_mode),
      screen_shape_(screen_shape),
      corner_radius_(std::max(corner_radius, 0))
{
}

Window::~Window() noexcept
{
    close();
}

bool Window::open() noexcept
{
    if (open_) {
        return true;
    }

    if (requested_width_ <= 0 || requested_height_ <= 0 || title_.empty()) {
        return false;
    }

    glfwSetErrorCallback(&Window::glfw_error_callback);

    if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    }
    else if (std::getenv("DISPLAY") != nullptr) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }

    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Error: glfwInit() failed.\n";
        return false;
    }
    glfw_initialized_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, lvgl_integration::opengl_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, lvgl_integration::opengl_minor);
    glfwWindowHint(GLFW_OPENGL_PROFILE,
                    lvgl_integration::opengl_core_profile ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_COMPAT_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, lvgl_integration::window_resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, lvgl_integration::window_visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR,
                   lvgl_integration::window_scale_to_monitor ? GLFW_TRUE : GLFW_FALSE);

    load_window_geometry();
    const int initial_width = saved_window_width_ > 0 ? saved_window_width_ : requested_width_;
    const int initial_height = saved_window_height_ > 0 ? saved_window_height_ : requested_height_;
    window_ = glfwCreateWindow(initial_width, initial_height, title_.c_str(), nullptr, nullptr);
    if (window_ == nullptr) {
        const char* description = nullptr;
        const int code = glfwGetError(&description);
        std::cerr << "Error: glfwCreateWindow() failed (" << code << ")"
                  << (description != nullptr ? std::string{": "} + description : std::string{})
                  << "\n";
        glfwTerminate();
        glfw_initialized_ = false;
        return false;
    }

    load_window_icon();
    glfwMakeContextCurrent(window_);
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        std::cerr << "Error: failed to load the requested OpenGL functions.\n";
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        glfw_initialized_ = false;
        return false;
    }

    if (!initialize_present_pipeline()) {
        std::cerr << "Error: failed to initialize the OpenGL presentation pipeline.\n";
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        glfw_initialized_ = false;
        return false;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, &Window::framebuffer_size_callback);
    glfwSetKeyCallback(window_, &Window::key_callback);
    glfwSetCharCallback(window_, &Window::char_callback);
    glfwSetScrollCallback(window_, &Window::scroll_callback);
    glfwSwapInterval(lvgl_integration::swap_interval);

    if (saved_window_position_loaded_ && glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) {
        glfwSetWindowPos(window_, window_position_x_, window_position_y_);
    }

    open_ = true;
    refresh_dimensions();
    resized_ = false;
    next_frame_deadline_ = glfwGetTime();
    return true;
}

void Window::close() noexcept
{
    if (window_ != nullptr) {
        save_window_geometry();
        destroy_present_pipeline();
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    if (glfw_initialized_) {
        glfwTerminate();
        glfw_initialized_ = false;
    }

    open_ = false;
    next_frame_deadline_ = 0.0;
    pressed_keys_.clear();
    pressed_chars_.clear();
}

bool Window::is_open() const noexcept
{
    return open_ && window_ != nullptr && glfwWindowShouldClose(window_) == GLFW_FALSE;
}

bool Window::should_close() const noexcept
{
    return !is_open();
}

void Window::set_size(int width, int height) noexcept
{
    if (!is_open() || width <= 0 || height <= 0) {
        return;
    }

    glfwSetWindowSize(window_, width, height);
    glfwPollEvents();
    refresh_dimensions();
}

void Window::poll_input() noexcept
{
    if (!is_open()) {
        return;
    }

    glfwPollEvents();
    refresh_dimensions();

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window_, &x, &y);

    const float scale_x = static_cast<float>(render_width_) /
                          static_cast<float>(std::max(screen_width_, 1));
    const float scale_y = static_cast<float>(render_height_) /
                          static_cast<float>(std::max(screen_height_, 1));
    mouse_position_.x = static_cast<float>(x) * scale_x;
    mouse_position_.y = static_cast<float>(y) * scale_y;
    left_mouse_button_down_ = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

bool Window::was_resized() const noexcept
{
    return resized_;
}

bool Window::take_resize_event() noexcept
{
    const bool value = resized_;
    resized_ = false;
    return value;
}

int Window::screen_width() const noexcept
{
    return screen_width_;
}

int Window::screen_height() const noexcept
{
    return screen_height_;
}

int Window::render_width() const noexcept
{
    return render_width_;
}

int Window::render_height() const noexcept
{
    return render_height_;
}

MousePosition Window::mouse_position(int content_width, int content_height) const noexcept
{
    MousePosition result{};
    if (window_ == nullptr || content_width <= 0 || content_height <= 0) {
        return result;
    }

    int viewport_x = 0;
    int viewport_y = 0;
    int viewport_width = render_width_;
    int viewport_height = render_height_;
    calculate_presentation_viewport(content_width,
                                     content_height,
                                     viewport_x,
                                     viewport_y,
                                     viewport_width,
                                     viewport_height);

    const float framebuffer_x = mouse_position_.x;
    const float framebuffer_y = mouse_position_.y;
    const float content_scale_x = static_cast<float>(content_width) /
                                  static_cast<float>(std::max(viewport_width, 1));
    const float content_scale_y = static_cast<float>(content_height) /
                                  static_cast<float>(std::max(viewport_height, 1));
    result.x = std::clamp((framebuffer_x - static_cast<float>(viewport_x)) * content_scale_x,
                          0.0F,
                          static_cast<float>(content_width - 1));
    result.y = std::clamp((framebuffer_y - static_cast<float>(viewport_y)) * content_scale_y,
                          0.0F,
                          static_cast<float>(content_height - 1));

    const bool inside_viewport = framebuffer_x >= static_cast<float>(viewport_x) &&
                                 framebuffer_x < static_cast<float>(viewport_x + viewport_width) &&
                                 framebuffer_y >= static_cast<float>(viewport_y) &&
                                 framebuffer_y < static_cast<float>(viewport_y + viewport_height);
    result.inside = inside_viewport &&
                    point_inside_screen_shape(result.x, result.y, content_width, content_height);
    return result;
}

bool Window::left_mouse_button_down() const noexcept
{
    return left_mouse_button_down_;
}

bool Window::is_key_down(int key) const noexcept
{
    return is_open() && glfwGetKey(window_, key) == GLFW_PRESS;
}

float Window::take_mouse_wheel_y() noexcept
{
    const float value = mouse_wheel_y_;
    mouse_wheel_y_ = 0.0F;
    return value;
}

std::optional<int> Window::take_key_pressed() noexcept
{
    if (pressed_keys_.empty()) {
        return std::nullopt;
    }

    const int value = pressed_keys_.front();
    pressed_keys_.pop_front();
    return value;
}

std::optional<unsigned int> Window::take_char_pressed() noexcept
{
    if (pressed_chars_.empty()) {
        return std::nullopt;
    }

    const unsigned int value = pressed_chars_.front();
    pressed_chars_.pop_front();
    return value;
}

TextureHandle Window::create_texture(int width,
                                     int height,
                                     const void* pixels) const noexcept
{
    if (!is_open() || width <= 0 || height <= 0 || pixels == nullptr) {
        return 0U;
    }

    TextureHandle texture = 0U;
    glGenTextures(1, &texture);
    if (texture == 0U) {
        return 0U;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 width,
                 height,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_SHORT_5_6_5,
                 pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void Window::update_texture(TextureHandle texture,
                            int x,
                            int y,
                            int width,
                            int height,
                            const void* pixels) const noexcept
{
    if (!is_open() || texture == 0U || width <= 0 || height <= 0 || pixels == nullptr) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    x,
                    y,
                    width,
                    height,
                    GL_RGB,
                    GL_UNSIGNED_SHORT_5_6_5,
                    pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Window::destroy_texture(TextureHandle& texture) const noexcept
{
    if (texture != 0U) {
        glDeleteTextures(1, &texture);
        texture = 0U;
    }
}

void Window::begin_frame() const noexcept
{
    if (!is_open()) {
        return;
    }

    glViewport(0, 0, render_width_, render_height_);
    glClearColor(LVGL_GLFW_WINDOW_BACKGROUND_RED,
                 LVGL_GLFW_WINDOW_BACKGROUND_GREEN,
                 LVGL_GLFW_WINDOW_BACKGROUND_BLUE,
                 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Window::end_frame() noexcept
{
    if (!is_open()) {
        return;
    }

    glfwSwapBuffers(window_);
    pace_frame();
}

void Window::pace_frame() noexcept
{
    if (!is_open() || lvgl_integration::max_fps <= 0) {
        return;
    }

    const double frame_duration = 1.0 / static_cast<double>(lvgl_integration::max_fps);
    const double now = glfwGetTime();
    if (next_frame_deadline_ < now - frame_duration) {
        next_frame_deadline_ = now;
    }

    next_frame_deadline_ += frame_duration;
    const double remaining = next_frame_deadline_ - now;
    if (remaining > 0.0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(remaining));
    }
}

void Window::present(TextureHandle texture, int content_width, int content_height) const noexcept
{
    if (!is_open() || texture == 0U || present_program_ == 0U || present_vao_ == 0U ||
        content_width <= 0 || content_height <= 0) {
        return;
    }

    int viewport_x = 0;
    int viewport_y = 0;
    int viewport_width = render_width_;
    int viewport_height = render_height_;
    calculate_presentation_viewport(content_width,
                                     content_height,
                                     viewport_x,
                                     viewport_y,
                                     viewport_width,
                                     viewport_height);

    glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    glUseProgram(present_program_);
    glBindVertexArray(present_vao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(present_texture_location_, 0);
    glUniform1i(present_screen_shape_location_, static_cast<int>(screen_shape_));
    glUniform2f(present_mask_size_location_,
                static_cast<float>(content_width),
                static_cast<float>(content_height));
    glUniform1f(present_corner_radius_location_, static_cast<float>(corner_radius_));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

GLFWwindow* Window::native_handle() const noexcept
{
    return window_;
}

void Window::glfw_error_callback(int code, const char* description) noexcept
{
    std::cerr << "GLFW error " << code << ": "
              << (description != nullptr ? description : "unknown error") << '\n';
}

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) noexcept
{
    auto* instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (instance == nullptr) {
        return;
    }

    instance->render_width_ = std::max(width, 1);
    instance->render_height_ = std::max(height, 1);
    instance->resized_ = true;
}

void Window::key_callback(GLFWwindow* window,
                          int key,
                          int scancode,
                          int action,
                          int mods) noexcept
{
    (void)scancode;
    (void)mods;
    auto* instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (instance == nullptr) {
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        instance->pressed_keys_.push_back(key);
    }

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void Window::char_callback(GLFWwindow* window, unsigned int codepoint) noexcept
{
    auto* instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (instance != nullptr) {
        instance->pressed_chars_.push_back(codepoint);
    }
}

void Window::scroll_callback(GLFWwindow* window,
                             double x_offset,
                             double y_offset) noexcept
{
    (void)x_offset;
    auto* instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (instance != nullptr) {
        instance->mouse_wheel_y_ += static_cast<float>(y_offset);
    }
}

void Window::load_window_geometry() noexcept
{
    if (window_geometry_loaded_) {
        return;
    }

    window_geometry_loaded_ = true;
    const std::string path = window_geometry_path();
    if (path.empty()) {
        return;
    }

    FILE* file = std::fopen(path.c_str(), "r");
    if (file == nullptr) {
        return;
    }

    int position_x = 0;
    int position_y = 0;
    int width = 0;
    int height = 0;
    const int fields = std::fscanf(file, "%d %d %d %d", &position_x, &position_y, &width, &height);
    std::fclose(file);

    if (fields == 4 && width > 0 && height > 0 &&
        saved_geometry_visible(position_x, position_y, width, height)) {
        window_position_x_ = position_x;
        window_position_y_ = position_y;
        saved_window_width_ = width;
        saved_window_height_ = height;
        saved_window_position_loaded_ = true;
    }
}

bool Window::saved_geometry_visible(int position_x,
                                    int position_y,
                                    int width,
                                    int height) const noexcept
{
    int monitor_count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    if (monitors == nullptr || monitor_count <= 0) {
        return true;
    }

    for (int index = 0; index < monitor_count; ++index) {
        int work_x = 0;
        int work_y = 0;
        int work_width = 0;
        int work_height = 0;
        glfwGetMonitorWorkarea(monitors[index], &work_x, &work_y, &work_width, &work_height);
        if (work_width <= 0 || work_height <= 0) {
            continue;
        }

        const bool intersects = position_x < work_x + work_width &&
                                position_x + width > work_x &&
                                position_y < work_y + work_height &&
                                position_y + height > work_y;
        if (intersects) {
            return true;
        }
    }

    return false;
}

void Window::save_window_geometry() noexcept
{
    if (window_ == nullptr) {
        return;
    }

    int position_x = window_position_x_;
    int position_y = window_position_y_;
    int width = 0;
    int height = 0;

    glfwGetWindowSize(window_, &width, &height);
    if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) {
        glfwGetWindowPos(window_, &position_x, &position_y);
        window_position_x_ = position_x;
        window_position_y_ = position_y;
        saved_window_position_loaded_ = true;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    saved_window_width_ = width;
    saved_window_height_ = height;
    const std::string path = window_geometry_path();
    if (path.empty()) {
        return;
    }

    FILE* file = std::fopen(path.c_str(), "w");
    if (file == nullptr) {
        return;
    }

    std::fprintf(file, "%d %d %d %d\n",
                 window_position_x_,
                 window_position_y_,
                 saved_window_width_,
                 saved_window_height_);
    std::fclose(file);
}

void Window::load_window_icon() noexcept
{
    const auto& icon_data = lvgl_integration::window_icon_data;
    if (window_ == nullptr || icon_data.empty()) {
        return;
    }

    const auto read_u16 = [&icon_data](std::size_t offset) -> std::uint16_t {
        return static_cast<std::uint16_t>(icon_data[offset]) |
               (static_cast<std::uint16_t>(icon_data[offset + 1U]) << 8U);
    };
    const auto read_u32 = [&icon_data](std::size_t offset) -> std::uint32_t {
        return static_cast<std::uint32_t>(icon_data[offset]) |
               (static_cast<std::uint32_t>(icon_data[offset + 1U]) << 8U) |
               (static_cast<std::uint32_t>(icon_data[offset + 2U]) << 16U) |
               (static_cast<std::uint32_t>(icon_data[offset + 3U]) << 24U);
    };

    if (icon_data.size() < 54U || icon_data[0] != 'B' || icon_data[1] != 'M') {
        return;
    }

    const std::uint32_t pixel_offset = read_u32(10U);
    const std::uint32_t bitmap_width_value = read_u32(18U);
    const std::uint32_t bitmap_height_value = read_u32(22U);
    const std::int32_t bitmap_width = static_cast<std::int32_t>(bitmap_width_value);
    const std::int32_t bitmap_height = static_cast<std::int32_t>(bitmap_height_value);
    const std::uint16_t bits_per_pixel = read_u16(28U);
    const std::uint32_t compression = read_u32(30U);
    if (bitmap_width_value > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        (bitmap_height_value & 0x7FFFFFFFU) == 0U ||
        bitmap_width <= 0 || bitmap_height == 0 ||
        (bits_per_pixel != 24U && bits_per_pixel != 32U) || compression != 0U) {
        return;
    }

    const std::int64_t absolute_height = bitmap_height < 0
        ? -static_cast<std::int64_t>(bitmap_height)
        : static_cast<std::int64_t>(bitmap_height);
    const std::size_t width = static_cast<std::size_t>(bitmap_width);
    const std::size_t height = static_cast<std::size_t>(absolute_height);
    const std::size_t bytes_per_pixel = bits_per_pixel / 8U;
    if (width > std::numeric_limits<std::size_t>::max() / bytes_per_pixel) {
        return;
    }

    const std::size_t unpadded_row_size = width * bytes_per_pixel;
    const std::size_t row_size = (unpadded_row_size + 3U) & ~static_cast<std::size_t>(3U);
    if (height > std::numeric_limits<std::size_t>::max() / row_size ||
        row_size * height > icon_data.size() ||
        pixel_offset > icon_data.size() - row_size * height ||
        width > std::numeric_limits<std::size_t>::max() / height ||
        width * height > std::numeric_limits<std::size_t>::max() / 4U) {
        return;
    }

    try {
        std::vector<std::uint8_t> pixels(width * height * 4U);
        const bool top_down = bitmap_height < 0;
        for (std::size_t y = 0; y < height; ++y) {
            const std::size_t source_y = top_down ? y : height - 1U - y;
            const std::size_t source_row = static_cast<std::size_t>(pixel_offset) + source_y * row_size;
            for (std::size_t x = 0; x < width; ++x) {
                const std::size_t source = source_row + x * bytes_per_pixel;
                const std::size_t destination = (y * width + x) * 4U;
                pixels[destination] = icon_data[source + 2U];
                pixels[destination + 1U] = icon_data[source + 1U];
                pixels[destination + 2U] = icon_data[source];
                pixels[destination + 3U] = bits_per_pixel == 32U ? icon_data[source + 3U] : 255U;
            }
        }

        GLFWimage image{
            static_cast<int>(width),
            static_cast<int>(height),
            pixels.data()
        };
        glfwSetWindowIcon(window_, 1, &image);
    }
    catch (const std::bad_alloc&) {
    }
}

void Window::calculate_presentation_viewport(int content_width,
                                               int content_height,
                                               int& viewport_x,
                                               int& viewport_y,
                                               int& viewport_width,
                                               int& viewport_height) const noexcept
{
    viewport_x = 0;
    viewport_y = 0;
    viewport_width = std::max(render_width_, 1);
    viewport_height = std::max(render_height_, 1);

    if (content_width <= 0 || content_height <= 0 ||
        presentation_mode_ != lvgl_integration::PresentationMode::PreserveAspectRatio) {
        return;
    }

    const double content_aspect = static_cast<double>(content_width) /
                                  static_cast<double>(content_height);
    const double window_aspect = static_cast<double>(render_width_) /
                                 static_cast<double>(render_height_);
    if (window_aspect > content_aspect) {
        viewport_width = std::max(
            1,
            static_cast<int>(std::lround(static_cast<double>(render_height_) * content_aspect)));
        viewport_x = (render_width_ - viewport_width) / 2;
    }
    else {
        viewport_height = std::max(
            1,
            static_cast<int>(std::lround(static_cast<double>(render_width_) / content_aspect)));
        viewport_y = (render_height_ - viewport_height) / 2;
    }
}

bool Window::point_inside_screen_shape(float x,
                                       float y,
                                       int content_width,
                                       int content_height) const noexcept
{
    if (screen_shape_ == lvgl_integration::ScreenShape::Rectangle) {
        return true;
    }

    const float half_width = static_cast<float>(content_width) * 0.5F;
    const float half_height = static_cast<float>(content_height) * 0.5F;
    const float point_x = x - half_width;
    const float point_y = y - half_height;

    if (screen_shape_ == lvgl_integration::ScreenShape::Circle) {
        const float radius = std::min(half_width, half_height);
        return point_x * point_x + point_y * point_y <= radius * radius;
    }

    const float radius = std::min({
        static_cast<float>(corner_radius_),
        half_width,
        half_height
    });
    const float distance_x = std::abs(point_x) - half_width + radius;
    const float distance_y = std::abs(point_y) - half_height + radius;
    const float outside_x = std::max(distance_x, 0.0F);
    const float outside_y = std::max(distance_y, 0.0F);
    const float distance = std::sqrt(outside_x * outside_x + outside_y * outside_y) +
                           std::min(std::max(distance_x, distance_y), 0.0F) - radius;
    return distance <= 0.0F;
}

void Window::refresh_dimensions() noexcept
{
    if (window_ == nullptr) {
        return;
    }

    int new_screen_width = 1;
    int new_screen_height = 1;
    int new_render_width = 1;
    int new_render_height = 1;
    glfwGetWindowSize(window_, &new_screen_width, &new_screen_height);
    glfwGetFramebufferSize(window_, &new_render_width, &new_render_height);

    new_screen_width = std::max(new_screen_width, 1);
    new_screen_height = std::max(new_screen_height, 1);
    new_render_width = std::max(new_render_width, 1);
    new_render_height = std::max(new_render_height, 1);

    resized_ = resized_ || new_screen_width != screen_width_ ||
               new_screen_height != screen_height_ ||
               new_render_width != render_width_ ||
               new_render_height != render_height_;

    screen_width_ = new_screen_width;
    screen_height_ = new_screen_height;
    render_width_ = new_render_width;
    render_height_ = new_render_height;
}

bool Window::initialize_present_pipeline() noexcept
{
    const std::uint32_t vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    const std::uint32_t fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (vertex_shader == 0U || fragment_shader == 0U) {
        if (vertex_shader != 0U) {
            glDeleteShader(vertex_shader);
        }
        if (fragment_shader != 0U) {
            glDeleteShader(fragment_shader);
        }
        return false;
    }

    present_program_ = link_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (present_program_ == 0U) {
        return false;
    }

    constexpr float vertices[] = {
        -1.0F, -1.0F, 0.0F, 1.0F,
         1.0F, -1.0F, 1.0F, 1.0F,
         1.0F,  1.0F, 1.0F, 0.0F,
        -1.0F, -1.0F, 0.0F, 1.0F,
         1.0F,  1.0F, 1.0F, 0.0F,
        -1.0F,  1.0F, 0.0F, 0.0F,
    };

    glGenVertexArrays(1, &present_vao_);
    glGenBuffers(1, &present_vbo_);
    if (present_vao_ == 0U || present_vbo_ == 0U) {
        destroy_present_pipeline();
        return false;
    }

    glBindVertexArray(present_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, present_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          reinterpret_cast<const void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    present_texture_location_ = glGetUniformLocation(present_program_, "u_texture");
    present_screen_shape_location_ = glGetUniformLocation(present_program_, "u_screen_shape");
    present_mask_size_location_ = glGetUniformLocation(present_program_, "u_mask_size");
    present_corner_radius_location_ = glGetUniformLocation(present_program_, "u_corner_radius");
    return present_texture_location_ >= 0 &&
           present_screen_shape_location_ >= 0 &&
           present_mask_size_location_ >= 0 &&
           present_corner_radius_location_ >= 0;
}

void Window::destroy_present_pipeline() noexcept
{
    if (present_vbo_ != 0U) {
        glDeleteBuffers(1, &present_vbo_);
        present_vbo_ = 0U;
    }
    if (present_vao_ != 0U) {
        glDeleteVertexArrays(1, &present_vao_);
        present_vao_ = 0U;
    }
    if (present_program_ != 0U) {
        glDeleteProgram(present_program_);
        present_program_ = 0U;
    }
    present_texture_location_ = -1;
    present_screen_shape_location_ = -1;
    present_mask_size_location_ = -1;
    present_corner_radius_location_ = -1;
}

}
