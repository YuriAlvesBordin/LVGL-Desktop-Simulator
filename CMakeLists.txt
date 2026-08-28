cmake_minimum_required(VERSION 3.25)

set(LVGL_GLFW_PROJECT_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/config/project_config.h")
set(LVGL_GLFW_DISPLAY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/config/display_config.h")
set(LVGL_GLFW_LVGL_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf.h")
set(LVGL_GLFW_INTEGRATION_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/config/integration_config.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${LVGL_GLFW_PROJECT_CONFIG}"
    "${LVGL_GLFW_DISPLAY_CONFIG}"
    "${LVGL_GLFW_LVGL_CONFIG}"
    "${LVGL_GLFW_INTEGRATION_CONFIG}"
)

function(lvgl_glfw_read_define header macro output)
    file(STRINGS "${header}" matched_line REGEX "^#[ \\t]*define[ \\t]+${macro}[ \\t]+")
    if(NOT matched_line)
        message(FATAL_ERROR "Missing ${macro} in ${header}")
    endif()
    string(REGEX REPLACE "^#[ \\t]*define[ \\t]+${macro}[ \\t]+(.+)$" "\\1" value "${matched_line}")
    string(STRIP "${value}" value)
    if(value MATCHES "^\".*\"$")
        string(REGEX REPLACE "^\"(.*)\"$" "\\1" value "${value}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_PROJECT_VERSION LVGL_GLFW_PROJECT_VERSION)

project(lvgl_glfw_desktop
    VERSION ${LVGL_GLFW_PROJECT_VERSION}
    DESCRIPTION "LVGL 9.5 desktop simulator using GLFW and OpenGL 3.3"
    LANGUAGES C CXX
)

lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_C_STANDARD LVGL_GLFW_C_STANDARD)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_CXX_STANDARD LVGL_GLFW_CXX_STANDARD)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_BUILD_SHARED_LIBS LVGL_GLFW_BUILD_SHARED_LIBS)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_GLFW_LIBRARY_TYPE LVGL_GLFW_GLFW_LIBRARY_TYPE)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_BUILD_GLFW_DOCS LVGL_GLFW_BUILD_GLFW_DOCS)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_BUILD_GLFW_TESTS LVGL_GLFW_BUILD_GLFW_TESTS)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_BUILD_GLFW_EXAMPLES LVGL_GLFW_BUILD_GLFW_EXAMPLES)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_INSTALL_GLFW LVGL_GLFW_INSTALL_GLFW)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_BUILD_GLFW_X11 LVGL_GLFW_BUILD_GLFW_X11)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_BUILD_GLFW_WAYLAND LVGL_GLFW_BUILD_GLFW_WAYLAND)
lvgl_glfw_read_define("${LVGL_GLFW_PROJECT_CONFIG}" LVGL_GLFW_USE_THORVG_INTERNAL LVGL_GLFW_USE_THORVG_INTERNAL)
lvgl_glfw_read_define("${LVGL_GLFW_LVGL_CONFIG}" LV_BUILD_EXAMPLES LV_BUILD_EXAMPLES)
lvgl_glfw_read_define("${LVGL_GLFW_LVGL_CONFIG}" LV_BUILD_DEMOS LV_BUILD_DEMOS)

lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_LVGL_WIDTH LVGL_GLFW_LVGL_WIDTH)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_LVGL_HEIGHT LVGL_GLFW_LVGL_HEIGHT)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_WINDOW_WIDTH LVGL_GLFW_WINDOW_WIDTH)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_WINDOW_HEIGHT LVGL_GLFW_WINDOW_HEIGHT)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_PRESENTATION_MODE LVGL_GLFW_PRESENTATION_MODE)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_SCREEN_SHAPE LVGL_GLFW_SCREEN_SHAPE)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_CORNER_RADIUS LVGL_GLFW_CORNER_RADIUS)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_WINDOW_TITLE LVGL_GLFW_WINDOW_TITLE)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_ICON_PATH LVGL_GLFW_ICON_PATH)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_OPENGL_MAJOR LVGL_GLFW_OPENGL_MAJOR)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_OPENGL_MINOR LVGL_GLFW_OPENGL_MINOR)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_OPENGL_CORE_PROFILE LVGL_GLFW_OPENGL_CORE_PROFILE)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_WINDOW_RESIZABLE LVGL_GLFW_WINDOW_RESIZABLE)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_WINDOW_VISIBLE LVGL_GLFW_WINDOW_VISIBLE)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_WINDOW_SCALE_TO_MONITOR LVGL_GLFW_WINDOW_SCALE_TO_MONITOR)
lvgl_glfw_read_define("${LVGL_GLFW_DISPLAY_CONFIG}" LVGL_GLFW_SWAP_INTERVAL LVGL_GLFW_SWAP_INTERVAL)

set(CMAKE_C_STANDARD "${LVGL_GLFW_C_STANDARD}")
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD "${LVGL_GLFW_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LV_BUILD_CONF_PATH "${LVGL_GLFW_LVGL_CONFIG}" CACHE FILEPATH "Derived LVGL configuration path" FORCE)
set(CONFIG_LV_BUILD_DEMOS "${LV_BUILD_DEMOS}" CACHE BOOL "Derived LVGL demo build flag" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES "${LV_BUILD_EXAMPLES}" CACHE BOOL "Derived LVGL example build flag" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL "${LVGL_GLFW_USE_THORVG_INTERNAL}" CACHE BOOL "Derived LVGL ThorVG build flag" FORCE)
set(BUILD_SHARED_LIBS "${LVGL_GLFW_BUILD_SHARED_LIBS}" CACHE BOOL "Derived project linkage policy" FORCE)
set(GLFW_LIBRARY_TYPE "${LVGL_GLFW_GLFW_LIBRARY_TYPE}" CACHE STRING "Derived GLFW library type" FORCE)
set(GLFW_BUILD_DOCS "${LVGL_GLFW_BUILD_GLFW_DOCS}" CACHE BOOL "Derived GLFW documentation flag" FORCE)
set(GLFW_BUILD_TESTS "${LVGL_GLFW_BUILD_GLFW_TESTS}" CACHE BOOL "Derived GLFW test flag" FORCE)
set(GLFW_BUILD_EXAMPLES "${LVGL_GLFW_BUILD_GLFW_EXAMPLES}" CACHE BOOL "Derived GLFW example flag" FORCE)
set(GLFW_INSTALL "${LVGL_GLFW_INSTALL_GLFW}" CACHE BOOL "Derived GLFW install flag" FORCE)
set(GLFW_BUILD_X11 "${LVGL_GLFW_BUILD_GLFW_X11}" CACHE BOOL "Derived GLFW X11 flag" FORCE)
set(GLFW_BUILD_WAYLAND "${LVGL_GLFW_BUILD_GLFW_WAYLAND}" CACHE BOOL "Derived GLFW Wayland flag" FORCE)

if(NOT "${LVGL_GLFW_OPENGL_MAJOR}" STREQUAL "3" OR NOT "${LVGL_GLFW_OPENGL_MINOR}" STREQUAL "3" OR NOT "${LVGL_GLFW_OPENGL_CORE_PROFILE}" STREQUAL "1")
    message(FATAL_ERROR "The project requires OpenGL 3.3 core")
endif()

if("${LVGL_GLFW_WINDOW_TITLE}" STREQUAL "")
    message(FATAL_ERROR "LVGL_GLFW_WINDOW_TITLE must not be empty")
endif()

if(NOT "${LVGL_GLFW_CORNER_RADIUS}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR "LVGL_GLFW_CORNER_RADIUS must be a non-negative integer")
endif()

if(NOT "${LVGL_GLFW_PRESENTATION_MODE}" MATCHES "^[012]$")
    message(FATAL_ERROR "LVGL_GLFW_PRESENTATION_MODE must be 0, 1, or 2")
endif()

if(NOT "${LVGL_GLFW_SCREEN_SHAPE}" MATCHES "^[012]$")
    message(FATAL_ERROR "LVGL_GLFW_SCREEN_SHAPE must be 0, 1, or 2")
endif()

if(NOT "${LVGL_GLFW_WINDOW_TITLE}" MATCHES "^[^\"\\\\;]*$")
    message(FATAL_ERROR "LVGL_GLFW_WINDOW_TITLE cannot contain quotes, backslashes, or semicolons")
endif()

set(LVGL_GLFW_ICON_RC "")
if(LVGL_GLFW_ICON_PATH)
    if(NOT EXISTS "${LVGL_GLFW_ICON_PATH}")
        message(FATAL_ERROR "LVGL_GLFW_ICON_PATH does not exist: ${LVGL_GLFW_ICON_PATH}")
    endif()

    get_filename_component(LVGL_GLFW_ICON_EXTENSION "${LVGL_GLFW_ICON_PATH}" EXT)
    if(WIN32)
        if(NOT LVGL_GLFW_ICON_EXTENSION MATCHES "^[.][Ii][Cc][Oo]$")
            message(FATAL_ERROR "LVGL_GLFW_ICON_PATH must point to an ICO file on Windows")
        endif()
        file(TO_CMAKE_PATH "${LVGL_GLFW_ICON_PATH}" LVGL_GLFW_ICON_RESOURCE_PATH)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WindowIcon.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/WindowIcon.rc"
            @ONLY
        )
        set(LVGL_GLFW_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/generated/WindowIcon.rc")
    else()
        if(NOT LVGL_GLFW_ICON_EXTENSION MATCHES "^[.][Bb][Mm][Pp]$")
            message(FATAL_ERROR "LVGL_GLFW_ICON_PATH must point to an uncompressed BMP file")
        endif()

        file(READ "${LVGL_GLFW_ICON_PATH}" LVGL_GLFW_ICON_HEX HEX)
        string(LENGTH "${LVGL_GLFW_ICON_HEX}" LVGL_GLFW_ICON_HEX_LENGTH)
        math(EXPR LVGL_GLFW_ICON_SIZE "${LVGL_GLFW_ICON_HEX_LENGTH} / 2")
        string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1, " LVGL_GLFW_ICON_BYTES "${LVGL_GLFW_ICON_HEX}")
    endif()
else()
    set(LVGL_GLFW_ICON_SIZE 0)
    set(LVGL_GLFW_ICON_BYTES "")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WindowIcon.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/WindowIcon.hpp"
    @ONLY
)

foreach(DISPLAY_DIMENSION
        LVGL_GLFW_LVGL_WIDTH
        LVGL_GLFW_LVGL_HEIGHT
        LVGL_GLFW_WINDOW_WIDTH
        LVGL_GLFW_WINDOW_HEIGHT)
    if(NOT "${${DISPLAY_DIMENSION}}" MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "${DISPLAY_DIMENSION} must be a positive integer")
    endif()
endforeach()

if("${LVGL_GLFW_PRESENTATION_MODE}" STREQUAL "0")
    set(LVGL_GLFW_PRESENTATION_MODE_NAME "stretch")
elseif("${LVGL_GLFW_PRESENTATION_MODE}" STREQUAL "1")
    set(LVGL_GLFW_PRESENTATION_MODE_NAME "preserve-aspect-ratio")
else()
    set(LVGL_GLFW_PRESENTATION_MODE_NAME "fixed-size")
endif()

if("${LVGL_GLFW_SCREEN_SHAPE}" STREQUAL "0")
    set(LVGL_GLFW_SCREEN_SHAPE_NAME "rectangle")
elseif("${LVGL_GLFW_SCREEN_SHAPE}" STREQUAL "1")
    set(LVGL_GLFW_SCREEN_SHAPE_NAME "rounded")
else()
    set(LVGL_GLFW_SCREEN_SHAPE_NAME "circle")
endif()

set(LVGL_GLFW_SHADER_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/lvgl/src/libs/gltf/gltf_view/assets/lv_gltf_view_shader.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/lvgl/src/drivers/opengles/assets/lv_opengles_shader.c"
)
add_subdirectory(external/lvgl EXCLUDE_FROM_ALL)
if(MSVC)
    set_source_files_properties(
        ${LVGL_GLFW_SHADER_ASSETS}
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external/lvgl"
        PROPERTIES COMPILE_FLAGS "/TP"
    )
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
        ${LVGL_GLFW_SHADER_ASSETS}
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external/lvgl"
        PROPERTIES COMPILE_FLAGS "-std=gnu11"
    )
endif()
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_definitions(lvgl PRIVATE _POSIX_C_SOURCE=200809L)
endif()

add_subdirectory(external/glfw EXCLUDE_FROM_ALL)

add_subdirectory(src/integration)
add_subdirectory(src/app)

message(STATUS "LVGL logical display: ${LVGL_GLFW_LVGL_WIDTH}x${LVGL_GLFW_LVGL_HEIGHT}")
message(STATUS "Initial window size: ${LVGL_GLFW_WINDOW_WIDTH}x${LVGL_GLFW_WINDOW_HEIGHT}")
message(STATUS "Presentation mode: ${LVGL_GLFW_PRESENTATION_MODE_NAME}")
message(STATUS "Screen shape: ${LVGL_GLFW_SCREEN_SHAPE_NAME}")
message(STATUS "Window title: ${LVGL_GLFW_WINDOW_TITLE}")
if(LVGL_GLFW_ICON_PATH)
    message(STATUS "Window icon: ${LVGL_GLFW_ICON_PATH}")
else()
    message(STATUS "Window icon: none")
endif()

add_executable(lvgl_glfw_app
    src/main.cpp
    ${LVGL_GLFW_ICON_RC}
)

target_link_libraries(lvgl_glfw_app
    PRIVATE
        lvgl_glfw_integration
        lvgl_glfw_app_ui
)

target_compile_features(lvgl_glfw_app PRIVATE cxx_std_${LVGL_GLFW_CXX_STANDARD})

if(MSVC)
    target_compile_options(lvgl_glfw_app PRIVATE /W4 /permissive-)
else()
    target_compile_options(lvgl_glfw_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(UNIX)
    find_program(BASH_EXECUTABLE bash)
    if(BASH_EXECUTABLE)
        add_custom_target(live-preview
            COMMAND ${CMAKE_COMMAND} -E env
                    BUILD_DIR=${CMAKE_BINARY_DIR}
                    CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    ${BASH_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/live_preview.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            USES_TERMINAL
            COMMENT "Watching LVGL application sources and restarting the desktop preview"
        )
        add_custom_target(package-release
            COMMAND ${BASH_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_release.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            USES_TERMINAL
            COMMENT "Building and packaging the standalone LVGL desktop simulator"
        )
    endif()
elseif(WIN32)
    add_custom_target(live-preview
        COMMAND ${CMAKE_COMMAND} -E env
                "BUILD_DIR=${CMAKE_BINARY_DIR}"
                "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
                cmd /c call
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/live_preview.bat"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Watching LVGL application sources and restarting the desktop preview"
    )
    add_custom_target(package-release
        COMMAND ${CMAKE_COMMAND} -E env
                "BUILD_DIR=${CMAKE_BINARY_DIR}"
                cmd /c call
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_release.bat"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Building and packaging the standalone LVGL desktop simulator"
    )
endif()

set_target_properties(lvgl_glfw_app PROPERTIES
    OUTPUT_NAME "lvgl-glfw-app"
)
