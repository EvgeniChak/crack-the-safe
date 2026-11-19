/*
 * Crack the Safe -- the level table. See levels.h.
 *
 * One row per level, in order. The defaults reproduce the original eight
 * levels: the time halves down to a 20 s floor, then the buzz starts closer
 * to the number instead.
 */

#include "levels.h"

#include "game.h"

static const level_def_t g_levels[] = {
    /*  time_ms  feel_range  min_gap  prize          */
    { 300000u,   10,         5,       PRIZE_COINS   },  /* 1  5:00 */
    { 150000u,   10,         5,       PRIZE_SILVER  },  /* 2  2:30 */
    {  60000u,   10,         5,       PRIZE_GOLD    },  /* 3  1:00 */
    {  30000u,   10,         5,       PRIZE_CASH    },  /* 4  0:30 */
    {  20000u,    8,         5,       PRIZE_GEMS    },  /* 5  0:20 */
    {  20000u,    6,         5,       PRIZE_PEARLS  },  /* 6  0:20 */
    {  20000u,    5,         5,       PRIZE_CROWN   },  /* 7  0:20 */
    {  20000u,    4,         5,       PRIZE_DIAMOND },  /* 8  0:20 */
};

#define N_LEVELS ((int)(sizeof g_levels / sizeof g_levels[0]))

/* game_t.level is a uint8_t. */
_Static_assert(N_LEVELS >= 1 && N_LEVELS <= 255, "1..255 levels");

int levels_count(void)
{
    return N_LEVELS;
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
