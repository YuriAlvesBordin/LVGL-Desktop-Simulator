#include "LVGLInput.hpp"

#include "platform/Window.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace lvgl_integration {

LVGLInput::LVGLInput(platform::Window& window, lv_display_t* display) noexcept
    : window_(&window),
      display_(display)
{
}

LVGLInput::~LVGLInput() noexcept
{
    if (keyboard_indev_ != nullptr) {
        lv_indev_delete(keyboard_indev_);
        keyboard_indev_ = nullptr;
    }

    if (mouse_indev_ != nullptr) {
        lv_indev_delete(mouse_indev_);
        mouse_indev_ = nullptr;
    }

    if (keyboard_group_ != nullptr) {
        if (lv_group_get_default() == keyboard_group_) {
            lv_group_set_default(nullptr);
        }
        lv_group_delete(keyboard_group_);
        keyboard_group_ = nullptr;
    }

    initialized_ = false;
}

bool LVGLInput::initialize() noexcept
{
    if (initialized_ || window_ == nullptr || display_ == nullptr) {
        return false;
    }

    mouse_indev_ = lv_indev_create();
    keyboard_indev_ = lv_indev_create();
    if (mouse_indev_ == nullptr || keyboard_indev_ == nullptr) {
        if (keyboard_indev_ != nullptr) {
            lv_indev_delete(keyboard_indev_);
            keyboard_indev_ = nullptr;
        }
        if (mouse_indev_ != nullptr) {
            lv_indev_delete(mouse_indev_);
            mouse_indev_ = nullptr;
        }
        return false;
    }

    lv_indev_set_type(mouse_indev_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse_indev_, &LVGLInput::mouse_read_callback);
    lv_indev_set_user_data(mouse_indev_, this);
    lv_indev_set_display(mouse_indev_, display_);
    lv_indev_set_scroll_limit(mouse_indev_, 4);
    lv_indev_set_scroll_throw(mouse_indev_, 10);

    keyboard_group_ = lv_group_create();
    if (keyboard_group_ == nullptr) {
        lv_indev_delete(keyboard_indev_);
        lv_indev_delete(mouse_indev_);
        keyboard_indev_ = nullptr;
        mouse_indev_ = nullptr;
        return false;
    }

    lv_group_set_default(keyboard_group_);

    lv_indev_set_type(keyboard_indev_, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keyboard_indev_, &LVGLInput::keyboard_read_callback);
    lv_indev_set_user_data(keyboard_indev_, this);
    lv_indev_set_display(keyboard_indev_, display_);
    lv_indev_set_group(keyboard_indev_, keyboard_group_);

    initialized_ = true;
    return true;
}

bool LVGLInput::is_initialized() const noexcept
{
    return initialized_;
}

lv_indev_t* LVGLInput::mouse_indev() const noexcept
{
    return mouse_indev_;
}

lv_indev_t* LVGLInput::keyboard_indev() const noexcept
{
    return keyboard_indev_;
}

void LVGLInput::mouse_read_callback(lv_indev_t* indev, lv_indev_data_t* data)
{
    if (indev == nullptr || data == nullptr) {
        return;
    }

    auto* input = static_cast<LVGLInput*>(lv_indev_get_user_data(indev));
    if (input != nullptr) {
        input->read_mouse(data);
    }
}

void LVGLInput::keyboard_read_callback(lv_indev_t* indev, lv_indev_data_t* data)
{
    if (indev == nullptr || data == nullptr) {
        return;
    }

    auto* input = static_cast<LVGLInput*>(lv_indev_get_user_data(indev));
    if (input != nullptr) {
        input->read_keyboard(data);
    }
}

void LVGLInput::read_mouse(lv_indev_data_t* data) noexcept
{
    if (window_ == nullptr) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->enc_diff = 0;
        return;
    }

    const platform::MousePosition position = window_->mouse_position(
        lv_display_get_horizontal_resolution(display_),
        lv_display_get_vertical_resolution(display_));
    data->point.x = static_cast<int32_t>(position.x);
    data->point.y = static_cast<int32_t>(position.y);
    data->state = position.inside && window_->left_mouse_button_down()
                      ? LV_INDEV_STATE_PRESSED
                      : LV_INDEV_STATE_RELEASED;

    const float wheel_y = window_->take_mouse_wheel_y();
    data->enc_diff = static_cast<int16_t>(std::clamp(
        static_cast<int>(wheel_y),
        static_cast<int>(std::numeric_limits<int16_t>::min()),
        static_cast<int>(std::numeric_limits<int16_t>::max())
    ));
}

void LVGLInput::read_keyboard(lv_indev_data_t* data) noexcept
{
    if (window_ == nullptr) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key_;
        return;
    }

    if (keyboard_needs_release_) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key_;
        keyboard_needs_release_ = false;
        return;
    }

    const auto key = window_->take_key_pressed();
    const auto character = window_->take_char_pressed();
    std::uint32_t next_key = 0U;

    if (character.has_value() && *character >= 0x20U) {
        next_key = *character;
    }
    else if (key.has_value()) {
        next_key = map_key(*key);
    }

    if (next_key == 0U) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key_;
        return;
    }

    last_key_ = next_key;
    data->key = last_key_;
    data->state = LV_INDEV_STATE_PRESSED;
    keyboard_needs_release_ = true;
}

std::uint32_t LVGLInput::map_key(int glfw_key) noexcept
{
    switch (glfw_key) {
        case GLFW_KEY_UP:        return LV_KEY_UP;
        case GLFW_KEY_DOWN:      return LV_KEY_DOWN;
        case GLFW_KEY_LEFT:      return LV_KEY_LEFT;
        case GLFW_KEY_RIGHT:     return LV_KEY_RIGHT;
        case GLFW_KEY_ESCAPE:    return LV_KEY_ESC;
        case GLFW_KEY_DELETE:    return LV_KEY_DEL;
        case GLFW_KEY_BACKSPACE: return LV_KEY_BACKSPACE;
        case GLFW_KEY_ENTER:     return LV_KEY_ENTER;
        case GLFW_KEY_TAB:       return LV_KEY_NEXT;
        case GLFW_KEY_HOME:      return LV_KEY_HOME;
        case GLFW_KEY_END:       return LV_KEY_END;
        case GLFW_KEY_PAGE_UP:   return LV_KEY_PREV;
        case GLFW_KEY_PAGE_DOWN: return LV_KEY_NEXT;
        default:                 return 0U;
    }
}

}
