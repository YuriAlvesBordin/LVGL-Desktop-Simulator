#include "app/Application.h"
#include "integration/DisplayConfig.hpp"
#include "integration/lvgl/LVGLDisplay.hpp"
#include "integration/lvgl/LVGLInput.hpp"
#include "integration/lvgl/LVGLTick.hpp"
#include "integration/platform/Window.hpp"

#include <lvgl.h>

#include <cstdlib>
#include <csignal>
#include <iostream>
#include <string>
#include <utility>

namespace {

volatile std::sig_atomic_t shutdown_requested = 0;

void request_shutdown(int) noexcept
{
    shutdown_requested = 1;
}

}

int main()
{
    std::signal(SIGINT, request_shutdown);
    std::signal(SIGTERM, request_shutdown);

    std::string window_title = lvgl_integration::window_title;
    if (std::getenv("LVGL_GLFW_PREVIEW") != nullptr) {
        window_title += " - Preview";
    }

    platform::Window window(lvgl_integration::window_width,
                            lvgl_integration::window_height,
                            std::move(window_title),
                            lvgl_integration::presentation_mode,
                            lvgl_integration::screen_shape,
                            lvgl_integration::corner_radius);
    if (!window.open()) {
        std::cerr << "Error: failed to create the GLFW window.\n";
        return EXIT_FAILURE;
    }

    lv_init();
    lvgl_integration::LVGLTick::install();

    int exit_code = EXIT_SUCCESS;
    {
        lvgl_integration::LVGLDisplay display(window);
        if (!display.initialize(lvgl_integration::lvgl_width, lvgl_integration::lvgl_height)) {
            std::cerr << "Error: failed to initialize the LVGL display.\n";
            exit_code = EXIT_FAILURE;
        }
        else {
            lvgl_integration::LVGLInput input(window, display.handle());
            if (!input.initialize()) {
                std::cerr << "Error: failed to initialize LVGL input.\n";
                exit_code = EXIT_FAILURE;
            }
            else {
                app_application_initialize();
                window.take_resize_event();

                while (!window.should_close() && shutdown_requested == 0) {
                    window.poll_input();

                    (void)window.take_resize_event();
                    (void)lv_timer_handler();

                    window.begin_frame();
                    display.present();
                    window.end_frame();
                }
            }
        }
    }

    lv_deinit();
    window.close();
    return exit_code;
}
