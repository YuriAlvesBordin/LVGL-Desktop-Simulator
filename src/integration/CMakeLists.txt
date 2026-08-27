add_library(lvgl_glfw_integration STATIC
    platform/Window.cpp
    lvgl/LVGLDisplay.cpp
    lvgl/LVGLInput.cpp
    lvgl/LVGLTick.cpp
    ${PROJECT_SOURCE_DIR}/external/lvgl/src/drivers/opengles/glad/src/gl.c
)

target_include_directories(lvgl_glfw_integration
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/config"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/external/lvgl/src/drivers/opengles/glad/include"
        "${PROJECT_BINARY_DIR}/generated"
)

target_link_libraries(lvgl_glfw_integration
    PUBLIC
        lvgl::lvgl
        glfw
)

if(MSVC)
    target_compile_options(lvgl_glfw_integration PRIVATE
        "/FI${LVGL_GLFW_INTEGRATION_CONFIG}"
    )
else()
    target_compile_options(lvgl_glfw_integration PRIVATE
        "-include"
        "${LVGL_GLFW_INTEGRATION_CONFIG}"
    )
endif()

find_package(OpenGL REQUIRED)
target_link_libraries(lvgl_glfw_integration PUBLIC OpenGL::GL)

target_compile_features(lvgl_glfw_integration PUBLIC cxx_std_${LVGL_GLFW_CXX_STANDARD})

if(MSVC)
    target_compile_options(lvgl_glfw_integration PRIVATE /W4 /permissive-)
else()
    target_compile_options(lvgl_glfw_integration PRIVATE -Wall -Wextra -Wpedantic)
endif()
