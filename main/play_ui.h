/*
 * The Mode screen: a "Mode" button on the menu (touch and hold) opens it.
 *
 *   Classic    the levels (carries on from the saved one)
 *   Practice   no clock, the distance to the number shown, misses forgiven
 *   Daily      today's code, the same on every knob (needs the network clock)
 *   Challenge  a code a friend set on the settings page (when one is waiting)
 *   Guard      on / off: the watchman patrols the rounds (guard.h)
 *
 * Built in C over the EEZ screens, with the project's own styles, like
 * net_ui.c. LVGL task, lock held.
 */
#pragma once

void play_ui_init(void);

/** Load the Mode screen (app.c's screen stack). */
void play_ui_show(void);
