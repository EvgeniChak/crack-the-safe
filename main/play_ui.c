/*
 * The Mode screen and the menu's Mode button. See play_ui.h.
 */

#include "play_ui.h"

#include <stdio.h>

#include "app.h"
#include "screens.h"
#include "stats.h"
#include "styles.h"

#define REFRESH_MS 300

static lv_obj_t *s_scr;
static lv_obj_t *s_btn[PLAY_COUNT], *s_btn_text[PLAY_COUNT];
static lv_obj_t *s_guard_value, *s_note;
static lv_obj_t *s_menu_text;

static const char *const NAMES[PLAY_COUNT] = { "Classic", "Practice", "Daily", "Challenge" };

static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, const char *text,
                        lv_event_cb_t cb, void *user, lv_obj_t **text_out)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 44);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    add_style_button_outline(b);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *t = lv_label_create(b);
    lv_obj_set_pos(t, 0, 13);
    lv_obj_set_size(t, w, LV_SIZE_CONTENT);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    add_style_button_outline_text(t);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(t, text);
    if (text_out) {
        *text_out = t;
    }
    return b;
}

/* ---- Events ------------------------------------------------------------------ */

static void on_mode(lv_event_t *e)
{
    app_play((play_t)(intptr_t)lv_event_get_user_data(e));
}

static void on_guard(lv_event_t *e)
{
    (void)e;
    stats_set_guard(!stats_guard());
}

static void on_back(lv_event_t *e)
{
    (void)e;
    app_back();
}

static void on_open(lv_event_t *e)
{
    (void)e;
    app_modes_open();
}

/* ---- Refresh ----------------------------------------------------------------- */

static void refresh(void)
{
    const play_t cur = g_game.play;
    char buf[32];
    snprintf(buf, sizeof buf, "Mode: %s", NAMES[cur]);
    lv_label_set_text(s_menu_text, buf);

    if (lv_screen_active() != s_scr) {
        return;
    }
    const bool have_day = stats_today() != 0;
    const bool have_chal = stats_challenge(NULL);
    for (int p = 0; p < PLAY_COUNT; p++) {
        /* The current mode is the filled button. */
        const bool can = p == PLAY_DAILY ? have_day : p == PLAY_CHALLENGE ? have_chal : true;
        if (can) {
            lv_obj_remove_state(s_btn[p], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_btn[p], LV_STATE_DISABLED);
        }
        if (p == (int)cur) {
            lv_obj_add_state(s_btn[p], LV_STATE_CHECKED);
            lv_obj_add_state(s_btn_text[p], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_btn[p], LV_STATE_CHECKED);
            lv_obj_remove_state(s_btn_text[p], LV_STATE_CHECKED);
        }
    }
    lv_label_set_text(s_guard_value, stats_guard() ? (stats_mic() ? "On + mic" : "On") : "Off");

    const char *note = "";
    if (!have_day) {
        note = "Daily needs Wi-Fi for the date";
    } else if (!have_chal) {
        note = "Set a Challenge at knob.local";
    }
    lv_label_set_text(s_note, note);
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    refresh();
}

/* ---- Build ------------------------------------------------------------------- */

static void build_menu_button(void)
{
    /* Room for a fourth button on the menu: move the others up. */
    lv_obj_set_y(objects.menu_title, 52);
    lv_obj_set_y(objects.menu_new, 148);
    lv_obj_set_y(objects.menu_settings, 200);
    lv_obj_set_y(objects.menu_back, 252);

    lv_obj_t *b = button(objects.page_menu, 90, 96, 180, "Mode: Classic", on_open, NULL,
                         &s_menu_text);
    (void)b;
}

static void build_screen(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, 360, 360);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    add_style_screen_root(s_scr);

    lv_obj_t *title = lv_label_create(s_scr);
    lv_obj_set_pos(title, 80, 46);
    lv_obj_set_size(title, 200, LV_SIZE_CONTENT);
    add_style_title_text(title);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(title, "Mode");

    static const int X[2] = { 58, 184 }, Y[2] = { 88, 140 };
    for (int p = 0; p < PLAY_COUNT; p++) {
        s_btn[p] = button(s_scr, X[p % 2], Y[p / 2], 118, NAMES[p], on_mode,
                          (void *)(intptr_t)p, &s_btn_text[p]);
    }

    lv_obj_t *row = lv_button_create(s_scr);
    lv_obj_set_pos(row, 50, 194);
    lv_obj_set_size(row, 260, 44);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    add_style_device_row(row);
    lv_obj_add_event_cb(row, on_guard, LV_EVENT_CLICKED, NULL);
    lv_obj_t *t = lv_label_create(row);
    lv_obj_set_pos(t, 40, 13);
    lv_obj_set_size(t, 100, LV_SIZE_CONTENT);
    add_style_button_text(t);
    lv_label_set_text(t, "Guard");
    s_guard_value = lv_label_create(row);
    lv_obj_set_pos(s_guard_value, 140, 13);
    lv_obj_set_size(s_guard_value, 106, LV_SIZE_CONTENT);
    add_style_setting_value(s_guard_value);
    lv_obj_set_style_text_align(s_guard_value, LV_TEXT_ALIGN_RIGHT, 0);

    s_note = lv_label_create(s_scr);
    lv_obj_set_pos(s_note, 50, 244);
    lv_obj_set_size(s_note, 260, LV_SIZE_CONTENT);
    add_style_setting_value(s_note);
    lv_obj_set_style_text_align(s_note, LV_TEXT_ALIGN_CENTER, 0);

    button(s_scr, 130, 268, 100, "Back", on_back, NULL, NULL);
}

/* The reward's title ("New Record! Level 3") in the EEZ title font is too
 * wide for its pill and wrapped onto the code below it: one line, drawn at
 * 70 % and centred. */
static void shrink_reward_title(void)
{
    lv_obj_t *t = objects.reward_title;
    lv_label_set_long_mode(t, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(t, LV_SIZE_CONTENT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_transform_pivot_x(t, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(t, LV_PCT(50), 0);
    lv_obj_set_style_transform_scale(t, 180, 0);    /* 256 = 100 % */
}

void play_ui_init(void)
{
    shrink_reward_title();
    build_menu_button();
    build_screen();
    lv_timer_create(timer_cb, REFRESH_MS, NULL);
    refresh();
}

void play_ui_show(void)
{
    lv_scr_load_anim(s_scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    refresh();
}
