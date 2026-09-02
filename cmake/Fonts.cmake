# Custom font pipeline: fonts dropped into assets/fonts are converted into
# LVGL C sources under src/app/fonts/ by scripts/convert_fonts.py and compiled
# into the UI library. See docs/guide/custom-fonts.md.

function(lvgl_glfw_setup_fonts target)
    set(LVGL_GLFW_FONTS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/fonts")
    set(LVGL_GLFW_FONTS_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fonts")
    set(LVGL_GLFW_FONTS_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/convert_fonts.py")
    set(LVGL_GLFW_FONTS_CONFIG "${LVGL_GLFW_FONTS_SOURCE_DIR}/fonts.toml")

    file(GLOB LVGL_GLFW_FONT_SOURCES CONFIGURE_DEPENDS
        "${LVGL_GLFW_FONTS_SOURCE_DIR}/*.ttf"
        "${LVGL_GLFW_FONTS_SOURCE_DIR}/*.otf"
        "${LVGL_GLFW_FONTS_SOURCE_DIR}/*.woff"
    )
    file(GLOB LVGL_GLFW_FONT_GENERATED
        "${LVGL_GLFW_FONTS_OUT_DIR}/lv_font_*.c"
        "${LVGL_GLFW_FONTS_OUT_DIR}/uiFontUser.c"
    )

    if(NOT LVGL_GLFW_FONT_SOURCES)
        if(LVGL_GLFW_FONT_GENERATED)
            message(WARNING "Custom fonts: no fonts in ${LVGL_GLFW_FONTS_SOURCE_DIR} but generated "
                "sources remain in ${LVGL_GLFW_FONTS_OUT_DIR}; compiling them as-is")
            target_sources(${target} PRIVATE ${LVGL_GLFW_FONT_GENERATED})
        else()
            message(STATUS "Custom fonts: none (drop .ttf/.otf/.woff files into ${LVGL_GLFW_FONTS_SOURCE_DIR})")
        endif()
        return()
    endif()

    # Re-run the configure step when the converter or its configuration changes.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${LVGL_GLFW_FONTS_SCRIPT}"
    )
    if(EXISTS "${LVGL_GLFW_FONTS_CONFIG}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${LVGL_GLFW_FONTS_CONFIG}"
        )
    endif()

    # Pick the first candidate interpreter that can actually import freetype:
    # machines often have several Pythons and not all of them have freetype-py.
    set(LVGL_GLFW_FONTS_CANDIDATES "")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        list(APPEND LVGL_GLFW_FONTS_CANDIDATES "${Python3_EXECUTABLE}")
    endif()
    find_program(LVGL_GLFW_FONTS_PYTHON3_PATH python3)
    if(LVGL_GLFW_FONTS_PYTHON3_PATH)
        list(APPEND LVGL_GLFW_FONTS_CANDIDATES "${LVGL_GLFW_FONTS_PYTHON3_PATH}")
    endif()
    find_program(LVGL_GLFW_FONTS_PYTHON_PATH python)
    if(LVGL_GLFW_FONTS_PYTHON_PATH)
        list(APPEND LVGL_GLFW_FONTS_CANDIDATES "${LVGL_GLFW_FONTS_PYTHON_PATH}")
    endif()
    list(REMOVE_DUPLICATES LVGL_GLFW_FONTS_CANDIDATES)

    set(LVGL_GLFW_FONTS_INTERPRETER "")
    set(LVGL_GLFW_FONTS_CONVERTER_READY OFF)
    foreach(LVGL_GLFW_FONTS_CANDIDATE ${LVGL_GLFW_FONTS_CANDIDATES})
        execute_process(
            COMMAND "${LVGL_GLFW_FONTS_CANDIDATE}" -c "import freetype"
            RESULT_VARIABLE LVGL_GLFW_FONTS_IMPORT_RESULT
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(LVGL_GLFW_FONTS_IMPORT_RESULT EQUAL 0)
            set(LVGL_GLFW_FONTS_INTERPRETER "${LVGL_GLFW_FONTS_CANDIDATE}")
            set(LVGL_GLFW_FONTS_CONVERTER_READY ON)
            break()
        endif()
    endforeach()

    if(NOT LVGL_GLFW_FONTS_CONVERTER_READY)
        if(LVGL_GLFW_FONTS_CANDIDATES)
            message(WARNING "Custom fonts: none of the Python interpreters "
                "(${LVGL_GLFW_FONTS_CANDIDATES}) can import the 'freetype' module; "
                "install it with: pip3 install freetype-py")
        else()
            message(WARNING "Custom fonts: Python 3 interpreter not found; cannot convert fonts")
        endif()
    endif()

    if(NOT LVGL_GLFW_FONTS_CONVERTER_READY)
        if(LVGL_GLFW_FONT_GENERATED)
            message(WARNING "Custom fonts: cannot convert right now - compiling the existing "
                "generated sources in ${LVGL_GLFW_FONTS_OUT_DIR} as-is (they may be stale)")
            target_sources(${target} PRIVATE ${LVGL_GLFW_FONT_GENERATED})
            return()
        endif()
        message(FATAL_ERROR
            "Custom fonts found in ${LVGL_GLFW_FONTS_SOURCE_DIR} but they cannot be converted.\n"
            "Install Python 3 plus the freetype-py module (pip3 install freetype-py), "
            "or remove the fonts from ${LVGL_GLFW_FONTS_SOURCE_DIR}.")
    endif()

    # Ask the converter what it will generate so the outputs can be wired into
    # the build as proper generated files.
    execute_process(
        COMMAND "${LVGL_GLFW_FONTS_INTERPRETER}" "${LVGL_GLFW_FONTS_SCRIPT}" --plan
            --source-dir "${LVGL_GLFW_FONTS_SOURCE_DIR}"
            --out-dir "${LVGL_GLFW_FONTS_OUT_DIR}"
        RESULT_VARIABLE LVGL_GLFW_FONTS_PLAN_RESULT
        OUTPUT_VARIABLE LVGL_GLFW_FONTS_PLAN
        ERROR_VARIABLE LVGL_GLFW_FONTS_PLAN_ERROR
    )
    if(NOT LVGL_GLFW_FONTS_PLAN_RESULT EQUAL 0)
        message(FATAL_ERROR "Custom fonts: converter planning failed:\n${LVGL_GLFW_FONTS_PLAN_ERROR}")
    endif()

    set(LVGL_GLFW_FONT_OUTPUTS "")
    string(STRIP "${LVGL_GLFW_FONTS_PLAN}" LVGL_GLFW_FONTS_PLAN)
    if(LVGL_GLFW_FONTS_PLAN)
        string(REPLACE "\n" ";" LVGL_GLFW_FONT_OUTPUT_NAMES "${LVGL_GLFW_FONTS_PLAN}")
        foreach(LVGL_GLFW_FONT_OUTPUT_NAME ${LVGL_GLFW_FONT_OUTPUT_NAMES})
            string(STRIP "${LVGL_GLFW_FONT_OUTPUT_NAME}" LVGL_GLFW_FONT_OUTPUT_NAME)
            if(NOT LVGL_GLFW_FONT_OUTPUT_NAME MATCHES "^[A-Za-z0-9_.-]+$")
                message(FATAL_ERROR "Custom fonts: converter reported an unexpected output name: "
                    "'${LVGL_GLFW_FONT_OUTPUT_NAME}'")
            endif()
            list(APPEND LVGL_GLFW_FONT_OUTPUTS "${LVGL_GLFW_FONTS_OUT_DIR}/${LVGL_GLFW_FONT_OUTPUT_NAME}")
        endforeach()
    endif()

    set(LVGL_GLFW_FONT_DEPENDS "${LVGL_GLFW_FONTS_SCRIPT}" ${LVGL_GLFW_FONT_SOURCES})
    if(EXISTS "${LVGL_GLFW_FONTS_CONFIG}")
        list(APPEND LVGL_GLFW_FONT_DEPENDS "${LVGL_GLFW_FONTS_CONFIG}")
    endif()

    add_custom_command(
        OUTPUT ${LVGL_GLFW_FONT_OUTPUTS}
        COMMAND "${LVGL_GLFW_FONTS_INTERPRETER}" "${LVGL_GLFW_FONTS_SCRIPT}" --convert
            --source-dir "${LVGL_GLFW_FONTS_SOURCE_DIR}"
            --out-dir "${LVGL_GLFW_FONTS_OUT_DIR}"
        DEPENDS ${LVGL_GLFW_FONT_DEPENDS}
        COMMENT "Converting user fonts into LVGL C sources"
        VERBATIM
    )
    target_sources(${target} PRIVATE ${LVGL_GLFW_FONT_OUTPUTS})

    add_custom_target(fonts
        COMMAND "${LVGL_GLFW_FONTS_INTERPRETER}" "${LVGL_GLFW_FONTS_SCRIPT}" --convert
            --source-dir "${LVGL_GLFW_FONTS_SOURCE_DIR}"
            --out-dir "${LVGL_GLFW_FONTS_OUT_DIR}"
        DEPENDS ${LVGL_GLFW_FONT_DEPENDS}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Converting user fonts into LVGL C sources"
        VERBATIM
    )
    message(STATUS "Custom fonts: converting ${LVGL_GLFW_FONT_SOURCES} into ${LVGL_GLFW_FONTS_OUT_DIR}")
endfunction()
