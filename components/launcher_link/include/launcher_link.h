/*
 * launcher_link -- a game's side of the Knob Launcher.
 *
 * The launcher sits in the factory partition and boots a game by pointing
 * otadata at the game's slot. Everything a game has to do to fit in:
 *
 *   1. Call launcher_link_init() early in app_main(), after nvs_flash_init().
 *      Under a launcher it confirms the image (else the bootloader rolls a
 *      fresh install back on the next reset) and points the NEXT boot at the
 *      launcher -- so a reset, a crash or a power cycle comes back to the
 *      menu. Standalone it does nothing: the game confirms itself as before.
 *   2. Offer a way out: launcher_exit() from a menu item, and/or
 *      launcher_link_hold_to_exit() for "hold the screen to leave".
 *   3. Do not update itself over the air while launcher_link_present():
 *      esp_ota_get_next_update_partition() is then the OTHER GAME's slot.
 *      Updates go through the launcher.
 *   4. Before esp_deep_sleep_start(), call launcher_link_before_sleep():
 *      waking is a reset, and it should wake into the game, not the menu.
 *
 * Shared settings (brightness, volume) are set in the launcher and read here.
 *
 * Flashed on its own, without a launcher, a game still runs: every call is
 * then a no-op and launcher_link_present() says false, so the game can hide
 * its Exit item.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Where the shared settings live (the launcher writes them). */
#define LAUNCHER_NVS_NS        "sys"
#define LAUNCHER_KEY_BRIGHT    "bright"     /* u8, 5..100 percent */
#define LAUNCHER_KEY_VOLUME    "vol"        /* u8, 0..10          */
#define LAUNCHER_DEF_BRIGHT    80
#define LAUNCHER_DEF_VOLUME    3

/* The launcher's project name, as in its app description. */
#define LAUNCHER_PROJECT_NAME  "knob_launcher"

/** See the header comment. Once, after nvs_flash_init(). */
void launcher_link_init(void);

/** Point the next boot at this game again, for a deep sleep it should wake
 *  up in. No-op without a launcher. */
void launcher_link_before_sleep(void);

/** True when a launcher is installed (factory partition holds it) and this
 *  app is not it. */
bool launcher_link_present(void);

/** Restart into the launcher. Does not return -- unless there is no launcher,
 *  in which case it returns at once and nothing happens. */
void launcher_exit(void);

/**
 * "Hold to leave": once the touch indev has been held down for hold_ms
 * without letting go, restart into the launcher. A ring on lv_layer_top()
 * fills up over the last part of the hold so the player can see it coming
 * and let go. hold_ms of 4000 sits well clear of a game's own long press.
 *
 * Call with the LVGL lock held. No-op without a launcher or without indev.
 */
void launcher_link_hold_to_exit(lv_indev_t *touch, uint32_t hold_ms);

/** Shared settings, read from NVS on every call (cheap; not for a hot loop). */
uint8_t launcher_link_brightness(void);     /* 5..100 */
uint8_t launcher_link_volume(void);         /* 0..10  */

#ifdef __cplusplus
}
#endif
