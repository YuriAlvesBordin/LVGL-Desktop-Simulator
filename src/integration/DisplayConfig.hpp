#ifndef LVGL_GLFW_DISPLAY_CONFIG_BRIDGE_HPP
#define LVGL_GLFW_DISPLAY_CONFIG_BRIDGE_HPP

#include <display_config.h>

#if LVGL_GLFW_PRESENTATION_MODE != LVGL_GLFW_PRESENTATION_MODE_STRETCH && \
    LVGL_GLFW_PRESENTATION_MODE != LVGL_GLFW_PRESENTATION_MODE_PRESERVE_ASPECT_RATIO
#error "LVGL_GLFW_PRESENTATION_MODE must select a supported presentation mode"
#endif

#if LVGL_GLFW_SCREEN_SHAPE != LVGL_GLFW_SCREEN_SHAPE_RECTANGLE && \
    LVGL_GLFW_SCREEN_SHAPE != LVGL_GLFW_SCREEN_SHAPE_ROUNDED && \
    LVGL_GLFW_SCREEN_SHAPE != LVGL_GLFW_SCREEN_SHAPE_CIRCLE
#error "LVGL_GLFW_SCREEN_SHAPE must select a supported screen shape"
#endif

#if LVGL_GLFW_OPENGL_CORE_PROFILE != 1
#error "LVGL_GLFW_OPENGL_CORE_PROFILE must remain enabled"
#endif

#if LVGL_GLFW_MAX_FPS < 0
#error "LVGL_GLFW_MAX_FPS must be zero or greater"
#endif

namespace lvgl_integration {

enum class PresentationMode {
    Stretch,
    PreserveAspectRatio
};

enum class ScreenShape {
    Rectangle,
    Rounded,
    Circle
};

inline constexpr int lvgl_width = LVGL_GLFW_LVGL_WIDTH;
inline constexpr int lvgl_height = LVGL_GLFW_LVGL_HEIGHT;
inline constexpr int window_width = LVGL_GLFW_WINDOW_WIDTH;
inline constexpr int window_height = LVGL_GLFW_WINDOW_HEIGHT;
inline constexpr int corner_radius = LVGL_GLFW_CORNER_RADIUS;
inline constexpr const char* window_title = LVGL_GLFW_WINDOW_TITLE;
inline constexpr const char* icon_path = LVGL_GLFW_ICON_PATH;
inline constexpr int opengl_major = LVGL_GLFW_OPENGL_MAJOR;
inline constexpr int opengl_minor = LVGL_GLFW_OPENGL_MINOR;
inline constexpr bool opengl_core_profile = LVGL_GLFW_OPENGL_CORE_PROFILE != 0;
inline constexpr int window_resizable = LVGL_GLFW_WINDOW_RESIZABLE;
inline constexpr int window_visible = LVGL_GLFW_WINDOW_VISIBLE;
inline constexpr int window_scale_to_monitor = LVGL_GLFW_WINDOW_SCALE_TO_MONITOR;
inline constexpr int swap_interval = LVGL_GLFW_SWAP_INTERVAL;
inline constexpr int max_fps = LVGL_GLFW_MAX_FPS;
inline constexpr ScreenShape screen_shape =
#if LVGL_GLFW_SCREEN_SHAPE == LVGL_GLFW_SCREEN_SHAPE_ROUNDED
    ScreenShape::Rounded;
#elif LVGL_GLFW_SCREEN_SHAPE == LVGL_GLFW_SCREEN_SHAPE_CIRCLE
    ScreenShape::Circle;
#else
    ScreenShape::Rectangle;
#endif
inline constexpr PresentationMode presentation_mode =
#if LVGL_GLFW_PRESENTATION_MODE == LVGL_GLFW_PRESENTATION_MODE_PRESERVE_ASPECT_RATIO
    PresentationMode::PreserveAspectRatio;
#else
    PresentationMode::Stretch;
#endif

}

#endif
