# Pick up every source under SmarthWatch/ automatically: adding a new
# screen (e.g. SmarthWatch/Screens/Foo/UI/fooUI.c) requires no build file
# change. CONFIGURE_DEPENDS re-runs the glob on each build.
file(GLOB_RECURSE LVGL_GLFW_SMARTWATCH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/SmarthWatch/*.c"
)

add_library(lvgl_glfw_app_ui STATIC
    Application.c
    ${LVGL_GLFW_SMARTWATCH_SOURCES}
)

target_include_directories(lvgl_glfw_app_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SmarthWatch"
)

target_link_libraries(lvgl_glfw_app_ui
    PUBLIC
        lvgl::lvgl
        lvgl::examples
        lvgl::demos
)

target_compile_features(lvgl_glfw_app_ui PUBLIC c_std_11)

# Convert fonts dropped into assets/fonts/ and compile them into this target.
# See docs/guide/custom-fonts.md.
include("${CMAKE_SOURCE_DIR}/cmake/Fonts.cmake")
lvgl_glfw_setup_fonts(lvgl_glfw_app_ui)

if(MSVC)
    target_compile_options(lvgl_glfw_app_ui PRIVATE /W4)
else()
    target_compile_options(lvgl_glfw_app_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
