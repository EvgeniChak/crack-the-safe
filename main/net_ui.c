/*
 * Wi-Fi & Update on the knob. See net_ui.h.
 */

#include "net_ui.h"

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "knob_board.h"
#include "net.h"
#include "screens.h"
#include "styles.h"

#define REFRESH_MS 250

/* DRV2605 ROM effects, as in app.c. */
#define FX_DOUBLE_CLICK 10
#define FX_SHARP_TICK   24

static lv_obj_t *s_scr;
static lv_obj_t *s_net, *s_addr, *s_ver, *s_upd;
static lv_obj_t *s_btn_upd, *s_btn_upd_text;
static lv_obj_t *s_btn_ap, *s_btn_ap_text;
static lv_obj_t *s_row_value;                 /* on the Settings page */

static lv_obj_t *s_modal, *s_modal_text, *s_modal_bar, *s_modal_btns;
static char      s_prompted_for[32];          /* release already offered */
static uint32_t  s_seen_serial = UINT32_MAX;

/* ---- Small builders ------------------------------------------------------- */

static lv_obj_t *label(lv_obj_t *parent, int y, void (*style)(lv_obj_t *))
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, 50, y);
    lv_obj_set_size(l, 260, LV_SIZE_CONTENT);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    style(l);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "");
    return l;
}

static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, bool primary,
                        const char *text, lv_event_cb_t cb, lv_obj_t **text_out)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 44);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    if (primary) {
        add_style_start_button(b);
    } else {
        add_style_button_outline(b);
    }
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *t = lv_label_create(b);
    lv_obj_set_pos(t, 0, 13);
    lv_obj_set_size(t, w, LV_SIZE_CONTENT);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    if (primary) {
        add_style_start_button_text(t);
    } else {
        add_style_button_outline_text(t);
    }
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(t, text);
    if (text_out) {
        *text_out = t;
    }
    return b;
}

/* ---- Events --------------------------------------------------------------- */

static void on_back(lv_event_t *e)
{
    (void)e;
    app_back();
}

static void on_upd(lv_event_t *e)
{
    (void)e;
    net_status_t st;
    net_get(&st);
    knob_board_haptic(FX_SHARP_TICK);
    if (st.upd == UPD_AVAILABLE) {
        net_install_update();
    } else {
        net_check_update();
    }
}

static void on_ap(lv_event_t *e)
{
    (void)e;
    net_status_t st;
    net_get(&st);
    knob_board_haptic(FX_SHARP_TICK);
    net_setup_ap(!st.ap_on);
}

static void on_open(lv_event_t *e)
{
    (void)e;
    app_network_open();
}

static void on_install(lv_event_t *e)
{
    (void)e;
    knob_board_haptic(FX_DOUBLE_CLICK);
    net_install_update();
}

static void on_later(lv_event_t *e)
{
    (void)e;
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

/* ---- Text for the state ---------------------------------------------------- */

static const char *upd_text(const net_status_t *st, char *buf, size_t n)
{
    switch (st->upd) {
    case UPD_CHECKING:   return "Checking for updates...";
    case UPD_UP_TO_DATE: return "Up to date";
    case UPD_AVAILABLE:  snprintf(buf, n, "%s available", st->latest); return buf;
    case UPD_INSTALLING: snprintf(buf, n, "Installing... %u%%", st->progress); return buf;
    case UPD_DONE:       return "Done, restarting";
    case UPD_ERROR:      snprintf(buf, n, "Update: %s", st->msg); return buf;
    default:             return st->sta == NET_CONNECTED ? "Not checked yet" : "Updates need Wi-Fi";
    }
}

static void refresh(const net_status_t *st)
{
    char buf[160], u[96];

    /* Settings row: the network in a word. */
    const char *v = !st->ssid[0] ? (st->ap_on ? "Setup" : "Off")
                  : st->sta == NET_CONNECTED ? "On"
                  : st->sta == NET_FAILED ? "No signal" : "...";
    if (st->upd == UPD_AVAILABLE) {
        v = "Update!";
    }
    lv_label_set_text(s_row_value, v);

    /* The screen. */
    if (st->ssid[0]) {
        snprintf(buf, sizeof buf, "Wi-Fi: %s", st->ssid);
    } else {
        snprintf(buf, sizeof buf, "Wi-Fi: not set");
    }
    lv_label_set_text(s_net, buf);

    if (st->ap_on) {
        snprintf(buf, sizeof buf, "Setup: join %s,\nopen 192.168.4.1", st->ap_ssid);
    } else if (st->sta == NET_CONNECTED) {
        snprintf(buf, sizeof buf, NET_HOSTNAME ".local  %s", st->ip);
    } else if (st->sta == NET_FAILED) {
        snprintf(buf, sizeof buf, "Can't reach the network");
    } else if (st->sta == NET_CONNECTING) {
        snprintf(buf, sizeof buf, "Connecting...");
    } else {
        buf[0] = 0;
    }
    lv_label_set_text(s_addr, buf);

    snprintf(buf, sizeof buf, "Firmware %s", st->version);
    lv_label_set_text(s_ver, buf);
    lv_label_set_text(s_upd, upd_text(st, u, sizeof u));

    lv_label_set_text(s_btn_upd_text, st->upd == UPD_AVAILABLE ? "Install" : "Check");
    const bool busy = st->upd == UPD_CHECKING || st->upd == UPD_INSTALLING ||
                      st->upd == UPD_DONE || st->sta != NET_CONNECTED;
    if (busy && st->upd != UPD_AVAILABLE) {
        lv_obj_add_state(s_btn_upd, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(s_btn_upd, LV_STATE_DISABLED);
    }
    lv_label_set_text(s_btn_ap_text, st->ap_on ? "Close setup" : "Setup Wi-Fi");
    if (st->ap_on && !st->ssid[0]) {
        lv_obj_add_state(s_btn_ap, LV_STATE_DISABLED);   /* nothing to fall back on */
    } else {
        lv_obj_remove_state(s_btn_ap, LV_STATE_DISABLED);
    }

    /* The prompt / progress panel. */
    const bool working = st->upd == UPD_INSTALLING || st->upd == UPD_DONE;
    if (working) {
        snprintf(buf, sizeof buf, st->upd == UPD_DONE ? "Update written.\nRestarting..."
                                                      : "Updating to %s\nKeep the knob on.",
                 st->latest[0] ? st->latest : "new firmware");
        lv_label_set_text(s_modal_text, buf);
        lv_bar_set_value(s_modal_bar, st->progress, LV_ANIM_OFF);
        lv_obj_remove_flag(s_modal_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_modal_btns, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (st->upd == UPD_AVAILABLE && app_is_idle() &&
        strcmp(s_prompted_for, st->latest) != 0) {
        strlcpy(s_prompted_for, st->latest, sizeof s_prompted_for);
        snprintf(buf, sizeof buf, "Update available\n%s  ->  %s", st->version, st->latest);
        lv_label_set_text(s_modal_text, buf);
        lv_obj_add_flag(s_modal_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_modal_btns, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
        knob_board_haptic(FX_DOUBLE_CLICK);
        return;
    }
    if (st->upd != UPD_AVAILABLE && !lv_obj_has_flag(s_modal, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(s_modal_btns, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);    /* an install that failed */
    }
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    net_status_t st;
    net_get(&st);
    /* The prompt also waits for the title screen, so re-check while one is
     * pending even when nothing changed. */
    const bool pending = st.upd == UPD_AVAILABLE && strcmp(s_prompted_for, st.latest) != 0;
    if (st.serial == s_seen_serial && !pending) {
        return;
    }
    s_seen_serial = st.serial;
    refresh(&st);
}

/* ---- Build ------------------------------------------------------------------ */

static void build_settings_row(void)
{
    /* Room for a third row: move the page's widgets up a little. */
    lv_obj_set_y(objects.settings_title, 58);
    lv_obj_set_y(objects.settings_hp, 100);
    lv_obj_set_y(objects.settings_vol, 150);
    lv_obj_set_y(objects.settings_back, 258);

    lv_obj_t *row = lv_button_create(objects.page_settings);
    lv_obj_set_pos(row, 50, 200);
    lv_obj_set_size(row, 260, 44);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    add_style_device_row(row);
    lv_obj_add_event_cb(row, on_open, LV_EVENT_CLICKED, NULL);

    lv_obj_t *t = lv_label_create(row);
    lv_obj_set_pos(t, 40, 13);
    lv_obj_set_size(t, 120, LV_SIZE_CONTENT);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    add_style_button_text(t);
    lv_label_set_text(t, "Wi-Fi & Update");

    s_row_value = lv_label_create(row);
    lv_obj_set_pos(s_row_value, 160, 13);
    lv_obj_set_size(s_row_value, 86, LV_SIZE_CONTENT);
    lv_label_set_long_mode(s_row_value, LV_LABEL_LONG_CLIP);
    add_style_setting_value(s_row_value);
    lv_obj_set_style_text_align(s_row_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_row_value, "");
}

static void build_screen(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, 360, 360);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    add_style_screen_root(s_scr);

    lv_obj_t *title = label(s_scr, 58, add_style_title_text);
    lv_label_set_text(title, "Wi-Fi & Update");

    s_net = label(s_scr, 104, add_style_button_text);
    s_addr = label(s_scr, 130, add_style_setting_value);
    s_ver = label(s_scr, 176, add_style_setting_value);
    s_upd = label(s_scr, 198, add_style_button_text);

    s_btn_upd = button(s_scr, 58, 232, 118, true, "Check", on_upd, &s_btn_upd_text);
    s_btn_ap = button(s_scr, 184, 232, 118, false, "Setup Wi-Fi", on_ap, &s_btn_ap_text);
    button(s_scr, 130, 286, 100, false, "Back", on_back, NULL);
}

static void build_modal(void)
{
    s_modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_modal, 360, 360);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);
    add_style_screen_root(s_modal);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_CLICKABLE);   /* swallow taps */

    s_modal_text = label(s_modal, 110, add_style_button_text);

    s_modal_bar = lv_bar_create(s_modal);
    lv_obj_set_pos(s_modal_bar, 70, 190);
    lv_obj_set_size(s_modal_bar, 220, 10);
    lv_bar_set_range(s_modal_bar, 0, 100);

    s_modal_btns = lv_obj_create(s_modal);
    lv_obj_remove_style_all(s_modal_btns);
    lv_obj_set_pos(s_modal_btns, 0, 0);
    lv_obj_set_size(s_modal_btns, 360, 360);
    lv_obj_remove_flag(s_modal_btns, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_modal_btns, LV_OBJ_FLAG_CLICKABLE);
    button(s_modal_btns, 58, 200, 118, true, "Install", on_install, NULL);
    button(s_modal_btns, 184, 200, 118, false, "Later", on_later, NULL);

    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void net_ui_init(void)
{
    build_settings_row();
    build_screen();
    build_modal();
    lv_timer_create(timer_cb, REFRESH_MS, NULL);
}

void net_ui_show(void)
{
    s_seen_serial = UINT32_MAX;                       /* repaint now */
    lv_scr_load_anim(s_scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
