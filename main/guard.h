/*
 * The guard: a night watchman doing his rounds while you work the dial.
 *
 * Every 12-25 s of play (Classic, Daily, Challenge -- never Practice) he
 * comes by:
 *   GUARD_STEPS   1.5 s  footsteps: soft bumps in the knob, "Footsteps..."
 *   GUARD_FREEZE  2.5 s  he is right outside: "FREEZE!" -- move the dial, or
 *                        (with the microphone on) make a sound, and you are
 *                        caught
 * then he walks on.
 *
 * The microphone is the board's PDM mic (GPIO45 clock, GPIO46 data) on
 * I2S0, read on its own task: a loud moment is one well above the room's
 * own level, which it keeps learning while the guard is away.
 *
 * guard_tick() and friends run in the LVGL task (app.c); the mic task only
 * publishes levels.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GUARD_AWAY = 0,
    GUARD_STEPS,
    GUARD_FREEZE,
} guard_phase_t;

/** Once at boot. */
void guard_init(void);

/** A round began: start the patrol (or not, if `on` is false). */
void guard_round(bool on, bool listen);

/** The round ended. */
void guard_stop(void);

/** `dt_ms` of play went by (not called while paused). Returns the phase,
 *  which the caller compares with the last one to react to changes. */
guard_phase_t guard_tick(uint32_t dt_ms);

guard_phase_t guard_phase(void);

/** In FREEZE with the microphone on, something loud was heard. */
bool guard_heard(void);
