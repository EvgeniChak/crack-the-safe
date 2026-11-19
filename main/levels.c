/*
 * Crack the Safe -- the level table. See levels.h.
 *
 * One row per level, in order: the time halves down to a 20 s floor, then
 * the buzz starts closer to the number instead, and from level 6 a false
 * click waits a few detents before each number.
 */

#include "levels.h"

#include "game.h"

static const level_def_t g_levels[] = {
    /*  time_ms  feel_range  min_gap  prize          decoy */
    { 300000u,   10,         5,       PRIZE_COINS,   0 },  /* 1  5:00 */
    { 150000u,   10,         5,       PRIZE_SILVER,  0 },  /* 2  2:30 */
    {  60000u,   10,         5,       PRIZE_GOLD,    0 },  /* 3  1:00 */
    {  30000u,   10,         5,       PRIZE_CASH,    0 },  /* 4  0:30 */
    {  20000u,    8,         5,       PRIZE_GEMS,    0 },  /* 5  0:20 */
    {  20000u,    8,         5,       PRIZE_PEARLS,  1 },  /* 6  0:20, false clicks */
    {  20000u,    7,         5,       PRIZE_CROWN,   1 },  /* 7  0:20, false clicks */
    {  20000u,    6,         5,       PRIZE_DIAMOND, 1 },  /* 8  0:20, false clicks */
};

/* The modes other than Classic: one row each, whatever the level. */
static const level_def_t s_practice  = {      0u, 10, 5, PRIZE_COINS,   0 };
static const level_def_t s_daily     = {  60000u,  8, 5, PRIZE_DIAMOND, 1 };
static const level_def_t s_challenge = { 120000u, 10, 5, PRIZE_CROWN,   0 };

#define N_LEVELS ((int)(sizeof g_levels / sizeof g_levels[0]))

/* game_t.level is a uint8_t. */
_Static_assert(N_LEVELS >= 1 && N_LEVELS <= 255, "1..255 levels");

int levels_count(void)
{
    return N_LEVELS;
}

const level_def_t *level_for_play(play_t play, int level)
{
    switch (play) {
    case PLAY_PRACTICE:  return &s_practice;
    case PLAY_DAILY:     return &s_daily;
    case PLAY_CHALLENGE: return &s_challenge;
    default:             return level_get(level);
    }
}

const level_def_t *level_get(int level)
{
    if (level < 1) {
        level = 1;
    }
    if (level > N_LEVELS) {
        level = N_LEVELS;
    }
    return &g_levels[level - 1];
}
