/*
 * The touch-and-hold menu as a wheel.
 *
 * Five items -- Back, New Game, Mode, Settings, Launcher -- do not fit the
 * round panel as a column of buttons. So the page shows three at a time: the
 * selected one in the middle, its neighbours above and below, dimmed. Turning
 * the knob moves the wheel a step per detent; a tap on the middle item opens
 * it (a tap on a neighbour opens that one, too).
 *
 * The items are the page's existing buttons (the EEZ ones, play_ui's Mode
 * button, launcher_ui's Launcher button), only moved and dimmed here, so
 * whatever each one does is unchanged. The Launcher item exists only under
 * the launcher.
 */

#include "menu_wheel.h"

#include <stddef.h>

#include "knob_board.h"
#include "launcher_ui.h"
#include "play_ui.h"
#include "screens.h"

#define MAX_ITEMS   6
#define CENTER_Y    154       /* top of the middle item                  */
#define STEP_Y      60        /* to the next item                        */
#define ANIM_MS     140

static lv_obj_t *s_items[MAX_ITEMS];
static int       s_n;
static int       s_sel;
static lv_obj_t *s_count;     /* "2 / 5" under the wheel */

static void place(lv_obj_t *b, int y, lv_opa_t opa, bool anim)
{
    if (anim) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, b);
        lv_anim_set_values(&a, lv_obj_get_y(b), y);
        lv_anim_set_duration(&a, ANIM_MS);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    } else {
        lv_anim_delete(b, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_obj_set_y(b, y);
    }
    lv_obj_set_style_opa(b, opa, 0);
}

static void layout(bool anim)
{
    for (int i = 0; i < s_n; i++) {
        lv_obj_t *b = s_items[i];
        const int k = i - s_sel;
        if (k < -1 || k > 1) {
            lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(b, CENTER_Y + k * STEP_Y);   /* parked, for the next anim */
            continue;
        }
        lv_obj_remove_flag(b, LV_OBJ_FLAG_HIDDEN);
        place(b, CENTER_Y + k * STEP_Y, k == 0 ? LV_OPA_COVER : LV_OPA_40, anim);
    }
    lv_label_set_text_fmt(s_count, "%d / %d", s_sel + 1, s_n);
}

static void add(lv_obj_t *b)
{
    if (b && s_n < MAX_ITEMS) {
        lv_obj_set_x(b, 90);
        s_items[s_n++] = b;
    }
}

void menu_wheel_init(void)
{
    add(objects.menu_back);
    add(objects.menu_new);
    add(play_ui_mode_button());
    add(objects.menu_settings);
    add(launcher_ui_button());          /* NULL without a launcher */

    lv_obj_set_y(objects.menu_title, 52);

    s_count = lv_label_create(objects.page_menu);
    lv_obj_set_width(s_count, 120);
    lv_obj_set_pos(s_count, 120, 282);
    lv_obj_set_style_text_align(s_count, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_count, lv_color_hex(0x8a93a0), 0);
    lv_obj_set_style_text_font(s_count, &lv_font_montserrat_14, 0);

    s_sel = 0;
    layout(false);
}

void menu_wheel_reset(void)
{
    if (s_n) {
        s_sel = 0;
        layout(false);
    }
}

void menu_wheel_rotate(int detents)
{
    if (!s_n) {
        return;
    }
    int sel = s_sel + detents;
    if (sel < 0 || sel >= s_n) {
        sel = sel < 0 ? 0 : s_n - 1;
        knob_board_haptic(7);           /* soft bump: the end of the wheel */
    }
    if (sel != s_sel) {
        s_sel = sel;
        layout(true);
    }
}
