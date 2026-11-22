/*
 * The way back to the Knob Launcher (components/launcher_link in
 * EvgeniChak/knob-launcher):
 *
 *   - a "Launcher" button in the touch-and-hold menu, between Settings and
 *     Back, and
 *   - touch and hold anywhere for 4 s (a ring fills up from 1.5 s on, so
 *     letting go in time is easy).
 *
 * Flashed on its own, without a launcher, neither appears and the menu keeps
 * its authored layout.
 *
 * Built in C, not in the EEZ project, styled with the project's own styles.
 * Runs in the LVGL task with the display lock held.
 */

#include "launcher_ui.h"

#include "knob_board.h"
#include "launcher_link.h"
#include "screens.h"
#include "styles.h"

#define HOLD_TO_EXIT_MS 4000

static void on_launcher(lv_event_t *e)
{
    (void)e;
    knob_board_haptic(1);
    launcher_exit();
}

void launcher_ui_init(void)
{
    if (!launcher_link_present()) {
        return;
    }
    launcher_link_hold_to_exit(knob_board_touch_indev(), HOLD_TO_EXIT_MS);

    /* Room for a fourth button: move the menu up a little. */
    lv_obj_set_y(objects.menu_title, 48);
    lv_obj_set_y(objects.menu_new, 92);
    lv_obj_set_y(objects.menu_settings, 150);
    lv_obj_set_y(objects.menu_back, 258);

    lv_obj_t *b = lv_button_create(objects.page_menu);
    lv_obj_set_pos(b, 90, 204);
    lv_obj_set_size(b, 180, 44);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    add_style_button_outline(b);
    lv_obj_add_event_cb(b, on_launcher, LV_EVENT_CLICKED, NULL);

    lv_obj_t *t = lv_label_create(b);
    lv_obj_set_pos(t, 0, 13);
    lv_obj_set_size(t, 180, LV_SIZE_CONTENT);
    add_style_button_outline_text(t);
    lv_label_set_text(t, LV_SYMBOL_HOME " Launcher");
}
