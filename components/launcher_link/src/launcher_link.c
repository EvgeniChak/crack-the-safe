/*
 * launcher_link -- see launcher_link.h.
 */

#include "launcher_link.h"

#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "nvs.h"

static const char *TAG = "launcher_link";

/* The hold-to-exit ring shows up once this much of the hold has passed. */
#define RING_SHOW_MS   1500
#define POLL_MS        40

static const esp_partition_t *factory(void)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
}

bool launcher_link_present(void)
{
    const esp_partition_t *f = factory();
    if (!f || esp_ota_get_running_partition() == f) {
        return false;
    }
    esp_app_desc_t d;
    return esp_ota_get_partition_description(f, &d) == ESP_OK &&
           strcmp(d.project_name, LAUNCHER_PROJECT_NAME) == 0;
}

void launcher_link_init(void)
{
    if (!launcher_link_present()) {
        ESP_LOGI(TAG, "no launcher: standalone");
        return;
    }
    /* Keep this image: a no-op unless it is fresh and still on probation. */
    esp_ota_mark_app_valid_cancel_rollback();
    /* Next boot -> launcher. Setting the factory partition erases otadata;
     * the launcher points it back here the next time this game is played. */
    const esp_err_t err = esp_ota_set_boot_partition(factory());
    ESP_LOGI(TAG, "under the launcher; next boot goes back to it (%s)",
             esp_err_to_name(err));
}

void launcher_link_before_sleep(void)
{
    if (launcher_link_present()) {
        esp_ota_set_boot_partition(esp_ota_get_running_partition());
    }
}

void launcher_exit(void)
{
    if (!launcher_link_present()) {
        ESP_LOGW(TAG, "exit: no launcher installed");
        return;
    }
    ESP_LOGI(TAG, "exit to launcher");
    esp_ota_set_boot_partition(factory());
    esp_restart();
}

/* ---- Shared settings ---------------------------------------------------- */

static uint8_t read_u8(const char *key, uint8_t def, uint8_t lo, uint8_t hi)
{
    nvs_handle_t h;
    uint8_t v = def;
    if (nvs_open(LAUNCHER_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_u8(h, key, &v) != ESP_OK) {
            v = def;
        }
        nvs_close(h);
    }
    return v < lo ? lo : v > hi ? hi : v;
}

uint8_t launcher_link_brightness(void)
{
    return read_u8(LAUNCHER_KEY_BRIGHT, LAUNCHER_DEF_BRIGHT, 5, 100);
}

uint8_t launcher_link_volume(void)
{
    return read_u8(LAUNCHER_KEY_VOLUME, LAUNCHER_DEF_VOLUME, 0, 10);
}

/* ---- Hold to exit ------------------------------------------------------- */

typedef struct {
    lv_indev_t *indev;
    uint32_t    hold_ms;
    uint32_t    held_ms;
    lv_obj_t   *veil;
    lv_obj_t   *ring;
} hold_t;

static hold_t s_hold;

static void hold_build(void)
{
    lv_obj_t *veil = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(veil);
    lv_obj_set_size(veil, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(veil, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(veil, LV_OPA_60, 0);
    lv_obj_remove_flag(veil, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(veil, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *ring = lv_arc_create(veil);
    lv_obj_set_size(ring, LV_PCT(94), LV_PCT(94));
    lv_obj_center(ring);
    lv_arc_set_rotation(ring, 270);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_range(ring, 0, 100);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ring, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0x2a313c), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0xe3b341), LV_PART_INDICATOR);

    lv_obj_t *t = lv_label_create(veil);
    lv_label_set_text(t, "Keep holding\nto exit");
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(t);

    s_hold.veil = veil;
    s_hold.ring = ring;
}

static void hold_poll(lv_timer_t *t)
{
    (void)t;
    if (lv_indev_get_state(s_hold.indev) != LV_INDEV_STATE_PRESSED) {
        if (s_hold.held_ms) {
            s_hold.held_ms = 0;
            lv_obj_add_flag(s_hold.veil, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    s_hold.held_ms += POLL_MS;
    if (s_hold.held_ms >= s_hold.hold_ms) {
        launcher_exit();
        return;
    }
    if (s_hold.held_ms >= RING_SHOW_MS) {
        const uint32_t span = s_hold.hold_ms - RING_SHOW_MS;
        const int pct = (int)((s_hold.held_ms - RING_SHOW_MS) * 100 / (span ? span : 1));
        lv_arc_set_value(s_hold.ring, pct);
        if (lv_obj_has_flag(s_hold.veil, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_remove_flag(s_hold.veil, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_hold.veil);
        }
    }
}

void launcher_link_hold_to_exit(lv_indev_t *touch, uint32_t hold_ms)
{
    if (!touch || s_hold.indev || !launcher_link_present()) {
        return;
    }
    s_hold.indev = touch;
    s_hold.hold_ms = hold_ms > RING_SHOW_MS + 500 ? hold_ms : RING_SHOW_MS + 500;
    hold_build();
    lv_timer_create(hold_poll, POLL_MS, NULL);
}
