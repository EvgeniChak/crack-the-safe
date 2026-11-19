/*
 * Saving the battery.
 *
 *   - 60 s without a touch or a turn: the backlight dims to POWER_DIM_PCT.
 *   - stats_sleep_min() minutes (5 by default; 0 = never): deep sleep. The
 *     backlight goes off and the S3 sleeps until the knob is turned -- each
 *     detent pulls one of its contacts (GPIO7 / GPIO8) low, which wakes it
 *     (EXT1). Waking is a fresh boot; progress is in NVS (stats.c).
 *   - On USB power it only dims, and it never sleeps while an update is
 *     being written.
 *
 * LVGL task (app.c's timer); power_activity() from the knob callback.
 */
#pragma once

#include <stdint.h>

#define POWER_DIM_AFTER_MS 60000
#define POWER_DIM_PCT      15

void power_init(void);

/** The knob turned (touches are seen through LVGL). */
void power_activity(void);

/** Call regularly. */
void power_tick(void);
