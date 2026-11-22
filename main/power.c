/*
 * Dimming and deep sleep. See power.h.
 */

#include "power.h"

#include <stdbool.h>

#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "knob_board.h"
#include "launcher_link.h"
#include "net.h"
#include "stats.h"

static const char *TAG = "power";

#define WAKE_A GPIO_NUM_8    /* the knob's contacts (board_pins.h) */
#define WAKE_B GPIO_NUM_7

static int64_t s_last_knob_us;
static bool    s_dim;

static uint32_t ms_since_knob(void)
{
    return (uint32_t)((esp_timer_get_time() - s_last_knob_us) / 1000);
}

void power_init(void)
{
    s_last_knob_us = esp_timer_get_time();
    const esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
    if (why == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "woken by the knob");
    }
}

void power_activity(void)
{
    s_last_knob_us = esp_timer_get_time();
}

static void sleep_now(void)
{
    ESP_LOGI(TAG, "idle: going to sleep, turn the knob to wake");
    knob_board_haptic_pulse(0, 0);
    knob_board_backlight_set(0);
    vTaskDelay(pdMS_TO_TICKS(50));

    const uint64_t mask = (1ull << WAKE_A) | (1ull << WAKE_B);
    rtc_gpio_pullup_en(WAKE_A);
    rtc_gpio_pullup_en(WAKE_B);
    rtc_gpio_pulldown_dis(WAKE_A);
    rtc_gpio_pulldown_dis(WAKE_B);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    launcher_link_before_sleep();      /* wake into the game, not the menu */
    esp_deep_sleep_start();
}

void power_tick(void)
{
    const uint32_t touch = lv_display_get_inactive_time(NULL);
    const uint32_t knob = ms_since_knob();
    const uint32_t idle = touch < knob ? touch : knob;

    const bool dim = idle >= POWER_DIM_AFTER_MS;
    if (dim != s_dim) {
        knob_board_backlight_set(dim ? POWER_DIM_PCT : 100);
        s_dim = dim;
    }

    const uint8_t min = stats_sleep_min();
    if (min == 0 || idle < (uint32_t)min * 60000u) {
        return;
    }
    knob_board_battery_t b;
    if (knob_board_battery_get(&b) && b.charging) {
        return;                       /* on USB: no need to sleep */
    }
    net_status_t st;
    net_get(&st);
    if (st.upd == UPD_INSTALLING || st.upd == UPD_DONE) {
        return;                       /* never in the middle of an update */
    }
    sleep_now();
}
