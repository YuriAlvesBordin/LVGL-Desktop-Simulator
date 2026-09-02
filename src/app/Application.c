#include "Application.h"

#include <lvgl.h>
#include <screenHandler.h>

void app_application_initialize(void)
{
    screen_handler_init();
}
