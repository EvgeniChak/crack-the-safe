/*
 * What the knob remembers between power-ups (NVS namespace "stats"):
 *
 *   - progress: the Classic level to carry on from;
 *   - records: best time per level, the last 12 solves with their splits
 *     (time to each number), ao5 / ao12, wins and losses, and on which
 *     number rounds are lost;
 *   - the Daily safe: today's date, best time and tries;
 *   - a Challenge code set from the settings page;
 *   - settings: guard, microphone, sleep timeout.
 *
 * Written only when something happens (a round won or lost, a setting
 * changed) -- a few writes a minute at most.
 *
 * Any task: everything takes the module's own lock. The game_t passed in is
 * only read during the call.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "game.h"

#define STATS_RECENT 12

typedef struct {
    bool     pb;          /* best time ever on this level (Classic)         */
    bool     daily_pb;    /* best time today on the Daily safe              */
    uint32_t prev_ms;     /* the record it beat; 0 if there was none        */
    uint32_t ao5_ms;      /* average of 5 after this solve; 0 if too few    */
} stats_win_t;

/** Load. Once, after nvs_flash_init(). */
void stats_init(void);

/* ---- Rounds ------------------------------------------------------------- */

/** A round was won (g in GAME_UNLOCKED). Saves; reports any record. */
stats_win_t stats_win(const game_t *g);

/** A round was lost (g in GAME_WRONG). Saves. */
void stats_loss(const game_t *g);

/** Classic: the level Start picks up at. */
int  stats_level(void);
void stats_set_level(int level);

/** Average of the last n Classic solves, best and worst dropped (n = 5 or
 *  12, as speedcubers do it); 0 if there are fewer than n. */
uint32_t stats_ao(int n);

/** Best Classic time on `level`, ms; 0 if never solved. */
uint32_t stats_best(int level);

/* ---- Daily and Challenge ------------------------------------------------- */

/** Today as yyyymmdd from the network clock (net.c sets it); 0 if the
 *  clock has not been set since power-up. */
uint32_t stats_today(void);

/** Best Daily time today, ms; 0 if not solved today. */
uint32_t stats_daily_best(void);

/** The Challenge code, if one is waiting. */
bool stats_challenge(uint8_t code[GAME_CODE_LEN]);
void stats_set_challenge(const uint8_t code[GAME_CODE_LEN]);
void stats_clear_challenge(void);

/* ---- Settings ------------------------------------------------------------- */

bool    stats_guard(void);            /* the guard patrols Classic rounds */
void    stats_set_guard(bool on);
bool    stats_mic(void);              /* ... and listens */
void    stats_set_mic(bool on);
uint8_t stats_sleep_min(void);        /* minutes idle before sleep; 0 never */
void    stats_set_sleep_min(uint8_t min);

/* ---- For the settings page -------------------------------------------------- */

/** Everything above as JSON, malloc'ed; the caller frees it. */
char *stats_json(void);

/** Forget records and progress (not the settings). */
void stats_reset(void);
