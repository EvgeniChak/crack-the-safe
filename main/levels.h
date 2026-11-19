/*
 * Crack the Safe -- the level table.
 *
 * Every level is one row in g_levels[] (levels.c). To add, remove or retune a
 * level, edit that table only: the rules (game.c), the haptics (app.c), the
 * prize on screen (ui_game.c) and the prize's sound (sfx.c) all read it.
 *
 * The number of levels is however many rows the table has. After the last
 * one the game wraps back to level 1.
 */
#pragma once

#include <stdint.h>

/* Prize art and sounds that exist: images prize_1..prize_8 in the EEZ
 * project, and one generated sound for each in sfx.c. A level picks one by
 * number; several levels may share a prize. Adding a 9th prize means new art
 * in EEZ Studio, a new sound in sfx.c, and bumping this. */
#define PRIZE_COUNT 8

typedef enum {
    PRIZE_COINS = 1,
    PRIZE_SILVER,
    PRIZE_GOLD,
    PRIZE_CASH,
    PRIZE_GEMS,
    PRIZE_PEARLS,
    PRIZE_CROWN,
    PRIZE_DIAMOND,
} prize_t;

typedef struct {
    uint32_t time_ms;     /* time limit for the whole combination          */
    uint8_t  feel_range;  /* the buzz starts this many detents out (>= 2)  */
    uint8_t  min_gap;     /* each number is at least this far from the one
                           * before it (and number 1 from the dial at rest);
                           * 1..GAME_POSITIONS/2                            */
    prize_t  prize;       /* what is behind the door                       */
} level_def_t;

/** How many levels the table has. */
int levels_count(void);

/** The row for `level`, 1-based. Out of range is clamped to the first or
 *  last level, so a caller never reads past the table. */
const level_def_t *level_get(int level);
