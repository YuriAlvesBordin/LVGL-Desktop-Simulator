#include "Application.h"

#include <lvgl.h>
#include <widgets/lv_demo_widgets.h>

void app_application_initialize(void)
{
    // lv_demo_widgets();

    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_center(btn);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Live preview");
    lv_obj_center(label);

}
