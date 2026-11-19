/*
 * The guard and the microphone. See guard.h.
 */

#include "guard.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "guard";

#define STEPS_MS        1500
#define FREEZE_MS       2500
#define AWAY_MIN_MS     12000
#define AWAY_SPAN_MS    13000

#define MIC_CLK         GPIO_NUM_45
#define MIC_DIN         GPIO_NUM_46
#define MIC_RATE        16000
#define MIC_BLOCK       320            /* 20 ms */
#define MIC_FLOOR       350            /* mean |sample| that is never "loud" */
#define MIC_FACTOR      4              /* this many times the room is loud */

static guard_phase_t s_phase;
static bool          s_on, s_listen;
static uint32_t      s_left_ms;        /* until the next phase */

static i2s_chan_handle_t s_rx;
static atomic_int        s_level;      /* mean |sample| of the last block  */
static atomic_int        s_room;       /* the room's level, slow average   */
static atomic_bool       s_learning;   /* learn the room (guard away)      */
static atomic_bool       s_peak;       /* a loud block since FREEZE began  */

/* ---- Microphone --------------------------------------------------------------- */

static void mic_task(void *arg)
{
    (void)arg;
    int16_t *buf = malloc(MIC_BLOCK * sizeof(int16_t));
    int32_t dc = 0;
    for (;;) {
        size_t got = 0;
        if (!buf || i2s_channel_read(s_rx, buf, MIC_BLOCK * sizeof(int16_t), &got,
                                     pdMS_TO_TICKS(200)) != ESP_OK || got == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        const int n = (int)(got / sizeof(int16_t));
        int64_t sum = 0;
        for (int i = 0; i < n; i++) {
            dc += (buf[i] - dc) / 64;          /* follow the DC offset */
            const int v = buf[i] - dc;
            sum += v < 0 ? -v : v;
        }
        const int level = (int)(sum / (n ? n : 1));
        atomic_store(&s_level, level);
        int room = atomic_load(&s_room);
        if (atomic_load(&s_learning)) {
            room = room ? room + (level - room) / 32 : level;
            atomic_store(&s_room, room);
        } else {
            const int loud = room * MIC_FACTOR > MIC_FLOOR ? room * MIC_FACTOR : MIC_FLOOR;
            if (level > loud) {
                atomic_store(&s_peak, true);
            }
        }
    }
}

static void mic_start(void)
{
    if (s_rx) {
        i2s_channel_enable(s_rx);
        return;
    }
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&cc, NULL, &s_rx) != ESP_OK) {
        ESP_LOGW(TAG, "no I2S channel for the mic");
        s_rx = NULL;
        return;
    }
    i2s_pdm_rx_config_t pc = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(MIC_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = { .clk = MIC_CLK, .din = MIC_DIN },
    };
    if (i2s_channel_init_pdm_rx_mode(s_rx, &pc) != ESP_OK || i2s_channel_enable(s_rx) != ESP_OK) {
        ESP_LOGW(TAG, "mic did not start");
        i2s_del_channel(s_rx);
        s_rx = NULL;
        return;
    }
    atomic_store(&s_learning, true);
    xTaskCreatePinnedToCore(mic_task, "mic", 3072, NULL, 2, NULL, 0);
    ESP_LOGI(TAG, "mic on");
}

static void mic_stop(void)
{
    if (s_rx) {
        i2s_channel_disable(s_rx);   /* the task's reads just time out */
    }
}

/* ---- Patrol -------------------------------------------------------------------- */

static uint32_t away_ms(void)
{
    return AWAY_MIN_MS + esp_random() % AWAY_SPAN_MS;
}

void guard_init(void)
{
    s_phase = GUARD_AWAY;
}

void guard_round(bool on, bool listen)
{
    s_on = on;
    s_phase = GUARD_AWAY;
    s_left_ms = away_ms();
    if (on && listen && !s_listen) {
        mic_start();
    } else if ((!on || !listen) && s_listen) {
        mic_stop();
    }
    s_listen = on && listen && s_rx;
    atomic_store(&s_learning, true);
    atomic_store(&s_peak, false);
}

void guard_stop(void)
{
    s_on = false;
    s_phase = GUARD_AWAY;
    atomic_store(&s_learning, true);
}

guard_phase_t guard_tick(uint32_t dt_ms)
{
    if (!s_on) {
        return GUARD_AWAY;
    }
    if (dt_ms < s_left_ms) {
        s_left_ms -= dt_ms;
        return s_phase;
    }
    switch (s_phase) {
    case GUARD_AWAY:
        s_phase = GUARD_STEPS;
        s_left_ms = STEPS_MS;
        break;
    case GUARD_STEPS:
        s_phase = GUARD_FREEZE;
        s_left_ms = FREEZE_MS;
        atomic_store(&s_peak, false);
        atomic_store(&s_learning, false);   /* listen against the room */
        break;
    default:
        s_phase = GUARD_AWAY;
        s_left_ms = away_ms();
        atomic_store(&s_learning, true);
        break;
    }
    return s_phase;
}

guard_phase_t guard_phase(void)
{
    return s_on ? s_phase : GUARD_AWAY;
}

bool guard_heard(void)
{
    return s_on && s_listen && s_phase == GUARD_FREEZE && atomic_load(&s_peak);
}
