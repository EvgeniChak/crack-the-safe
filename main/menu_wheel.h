/*
 * The touch-and-hold menu as a wheel: the knob moves through the items, a
 * tap on the one in the middle opens it. See menu_wheel.c.
 */
#pragma once

/** After every module has put its button on the menu page (play_ui_init,
 *  launcher_ui_init). With the display lock held. */
void menu_wheel_init(void);

/** The menu has just opened: back to the first item (Back). */
void menu_wheel_reset(void);

/** Knob detents while the menu is open. */
void menu_wheel_rotate(int detents);
