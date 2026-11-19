/*
 * Wi-Fi & Update on the knob itself.
 *
 *   - A "Wi-Fi" row on the Settings page opens the Wi-Fi & Update screen:
 *     network, address, firmware version, update state, and buttons to
 *     check/install and to open or close the setup access point.
 *   - When a newer release turns up while the title screen is showing, a
 *     prompt offers it: Install or Later. While an update is written (from
 *     here or from the settings page) a panel shows its progress.
 *
 * Built in C, not in the EEZ project: it is a thin layer over the game's
 * screens, styled with the project's own styles (ui/styles.h).
 *
 * All functions run in the LVGL task with the display lock held.
 */
#pragma once

/** After ui_init(): the Settings row, the screen and the prompt. */
void net_ui_init(void);

/** The Wi-Fi & Update screen object, for app.c's screen stack. */
void net_ui_show(void);
