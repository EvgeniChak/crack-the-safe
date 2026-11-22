/*
 * The way back to the Knob Launcher. See launcher_ui.c.
 */
#pragma once

#include "lvgl.h"

/** After ui_init(), with the display lock held. */
void launcher_ui_init(void);

/** The menu's Launcher button; NULL without a launcher. */
lv_obj_t *launcher_ui_button(void);
