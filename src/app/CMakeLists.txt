add_library(lvgl_glfw_app_ui STATIC
    Application.c
)

target_include_directories(lvgl_glfw_app_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(lvgl_glfw_app_ui
    PUBLIC
        lvgl::lvgl
        lvgl::examples
        lvgl::demos
)

target_compile_features(lvgl_glfw_app_ui PUBLIC c_std_11)

if(MSVC)
    target_compile_options(lvgl_glfw_app_ui PRIVATE /W4)
else()
    target_compile_options(lvgl_glfw_app_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
