/*
 * Crack the Safe -- rules. See game.h for the rules in prose.
 */

#include "game.h"

#include <stdlib.h>

/* Consecutive numbers (and number 1 vs the dial's resting position) are at
 * least min_gap apart (from the level table), so every number takes a real
 * turn to reach. Clamped: no two positions on the dial are further apart
 * than half of it, and a larger gap would never be dealt. */
static int min_gap(const game_t *g)
{
    int gap = game_params(g)->min_gap;
    if (gap < 1) {
        gap = 1;
    }
    if (gap > GAME_POSITIONS / 2) {
        gap = GAME_POSITIONS / 2;
    }
    return gap;
}

static uint32_t s_rng;

static uint32_t rnd(void)
{
    /* xorshift32 -- the seed comes from the hardware RNG in main.c. */
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int wrap(int v)
{
    v %= GAME_POSITIONS;
    return v < 0 ? v + GAME_POSITIONS : v;
}

static int dist(int a, int b)
{
    const int d = abs(a - b) % GAME_POSITIONS;
    return d < GAME_POSITIONS - d ? d : GAME_POSITIONS - d;
}

int game_dial_value(const game_t *g)
{
    /* Turning right moves the ring clockwise, so the number under the index
     * counts DOWN -- the way a real combination dial reads. */
    return wrap(-g->ticks);
}

int game_stage_dir(const game_t *g)
{
    return (g->stage % 2 == 0) ? +1 : -1;
}

int game_distance(const game_t *g)
{
    if (g->mode != GAME_PLAY || g->stage >= GAME_CODE_LEN) {
        return -1;
    }
    const int v = game_dial_value(g), target = g->code[g->stage];
    /* Turning right counts the number under the index DOWN. */
    return game_stage_dir(g) > 0 ? wrap(v - target) : wrap(target - v);
}

static void set_mode(game_t *g, game_mode_t m, uint32_t now_ms)
{
    g->mode = m;
    g->mode_since_ms = now_ms;
}

const level_def_t *game_params(const game_t *g)
{
    return level_for_play(g->play, g->level ? g->level : 1);
}

static bool fixed_code(const game_t *g)
{
    return g->play == PLAY_DAILY || g->play == PLAY_CHALLENGE;
}

/* Deal a new code and go back to number 1. Keeps the attempt count. Daily
 * and Challenge deal the same code every time: that is the point of them. */
static void deal(game_t *g, uint32_t now_ms)
{
    int prev = game_dial_value(g);
    const int gap = min_gap(g);
    for (int k = 0; k < GAME_CODE_LEN; k++) {
        int v;
        if (fixed_code(g)) {
            v = g->fixed_code[k] % GAME_POSITIONS;
        } else {
            do {
                v = (int)(rnd() % GAME_POSITIONS);
            } while (dist(v, prev) < gap);
        }
        g->code[k] = (uint8_t)v;
        prev = v;
        /* A false click 3..5 detents before the number, inside the buzz but
         * short of the number itself. */
        const int range = game_params(g)->feel_range;
        g->decoy[k] = 0;
        if (game_params(g)->decoy && range > 4) {
            const int d = 3 + (int)(rnd() % 3);
            g->decoy[k] = (uint8_t)(d < range ? d : range - 1);
        }
    }
    g->stage = 0;
    g->locked = 0;
    g->travel = 0;
    g->on_target = false;
    g->hit_felt = false;
    (void)now_ms;
}

uint32_t game_level_ms(int level)
{
    return level_get(level)->time_ms;
}

/* No clock in Practice: shown as this much left, and never runs out. */
#define NO_LIMIT_MS (99u * 60u * 1000u)

int game_next_level(const game_t *g)
{
    if (g->play != PLAY_CLASSIC) {
        return 1;                       /* the other modes have no levels */
    }
    if (g->mode == GAME_UNLOCKED) {
        return g->level >= levels_count() ? 1 : g->level + 1;
    }
    if (g->level) {
        return g->level;
    }
    return g->start_level >= 1 && g->start_level <= levels_count() ? g->start_level : 1;
}

uint32_t game_time_left_ms(const game_t *g, uint32_t now_ms)
{
    if (g->mode == GAME_TITLE || g->mode == GAME_INSTRUCTIONS) {
        return game_level_ms(1);
    }
    const uint32_t end = g->finished_ms ? g->finished_ms : now_ms;
    const uint32_t used = end - g->started_ms;
    const uint32_t limit = game_params(g)->time_ms;
    if (limit == 0) {
        return NO_LIMIT_MS;
    }
    return used >= limit ? 0 : limit - used;
}

static unsigned start_round(game_t *g, uint32_t now_ms)
{
    g->level = (uint8_t)game_next_level(g);
    g->timed_out = false;
    g->loss = LOSS_WRONG;
    g->misses = 0;
    for (int k = 0; k < GAME_CODE_LEN; k++) {
        g->split_ms[k] = 0;
    }
    g->attempts = 1;
    g->started_ms = now_ms;
    g->finished_ms = 0;
    deal(g, now_ms);
    set_mode(g, GAME_PLAY, now_ms);
    return GAME_EV_STARTED;
}

void game_init(game_t *g, uint32_t seed)
{
    *g = (game_t){ 0 };
    s_rng = seed ? seed : 0x9E3779B9u;
    g->mode = GAME_TITLE;
}

/* Game over. The red hold shows what went wrong; when it ends, game_tick()
 * starts a fresh game with the dial back at 0. */
static unsigned fail(game_t *g, uint32_t now_ms)
{
    g->on_target = false;
    g->hit_felt = false;
    g->finished_ms = now_ms;       /* freezes the clock display in the red */
    g->loss = LOSS_WRONG;          /* callers change it for time / the guard */
    set_mode(g, GAME_WRONG, now_ms);
    return GAME_EV_WRONG;
}

/* Practice: the mistake is forgiven. The number has to be found again from
 * here -- back up a little and come at it once more. */
static unsigned miss(game_t *g)
{
    g->on_target = false;
    g->hit_felt = false;
    g->travel = 0;
    if (g->misses < UINT16_MAX) {
        g->misses++;
    }
    return GAME_EV_MISS;
}

static unsigned open_safe(game_t *g, uint32_t now_ms)
{
    g->split_ms[GAME_CODE_LEN - 1] = now_ms - g->started_ms;
    g->on_target = false;
    g->hit_felt = false;
    g->locked = GAME_CODE_LEN;
    g->stage = GAME_CODE_LEN;
    g->finished_ms = now_ms;
    set_mode(g, GAME_UNLOCKED, now_ms);
    return GAME_EV_UNLOCKED;
}

/* Is the click on the number felt at the current turning speed? */
static bool felt(const game_t *g)
{
    if (g->speed_dps <= GAME_FEEL_SLOW_DPS) {
        return true;
    }
    if (g->speed_dps >= GAME_FEEL_FAST_DPS) {
        return false;
    }
    const uint32_t span = GAME_FEEL_FAST_DPS - GAME_FEEL_SLOW_DPS;
    const uint32_t keep = GAME_FEEL_FAST_DPS - g->speed_dps;   /* span..0 */
    return rnd() % span < keep;
}

/* Smoothed detents per second. Detents arrive in batches (the board delivers
 * every 10 ms), so speed is measured batch to batch; after a pause it starts
 * over rather than averaging in the idle time. */
static void update_speed(game_t *g, int detents, uint32_t now_ms)
{
    const uint32_t n = (uint32_t)abs(detents);
    const uint32_t dt = now_ms - g->last_turn_ms;
    g->last_turn_ms = now_ms;
    if (dt >= 500) {
        g->speed_dps = 0;          /* first detent after a pause: slow */
        return;
    }
    const uint32_t inst = n * 1000u / (dt ? dt : 1u);
    g->speed_dps = (g->speed_dps + inst) / 2;
}

/* One detent of play. */
static unsigned step(game_t *g, int delta, uint32_t now_ms)
{
    g->ticks += delta;
    const int v = game_dial_value(g);
    const int dir = game_stage_dir(g);

    if (delta == dir) {
        if (g->on_target) {
            /* Kept going past the number instead of reversing on it. */
            g->timed_out = false;
            return g->play == PLAY_PRACTICE ? miss(g) : fail(g, now_ms);
        }
        if (g->travel < 255) {
            g->travel++;
        }
        if (v == g->code[g->stage]) {
            /* On the number, whether or not it is felt: going past it now
             * loses, and stopping on it counts (reverse to lock; the last
             * number also opens after a dwell, in game_tick). Speed only
             * decides whether the player gets the HIT that tells them. */
            g->on_target = true;
            g->hit_felt = felt(g);
            return g->hit_felt ? GAME_EV_HIT : GAME_EV_NONE;
        }
        return GAME_EV_NONE;
    }

    /* Reversing. */
    if (g->on_target && g->stage == GAME_CODE_LEN - 1) {
        return open_safe(g, now_ms);
    }
    if (g->on_target) {
        g->split_ms[g->stage] = now_ms - g->started_ms;
        g->locked++;
        g->stage++;
        g->travel = 1;            /* this detent already counts toward the next */
        g->on_target = false;
        g->hit_felt = false;
        return GAME_EV_LOCKED;
    }
    if (g->travel >= 2) {
        /* Committed to a direction, then turned back somewhere other than
         * the number. */
        return g->play == PLAY_PRACTICE ? miss(g) : fail(g, now_ms);
    }
    /* A detent or two of wobble before committing is forgiven. */
    if (g->travel > 0) {
        g->travel--;
    }
    return GAME_EV_NONE;
}

unsigned game_rotate(game_t *g, int detents, uint32_t now_ms)
{
    unsigned ev = GAME_EV_NONE;
    const int sign = detents > 0 ? +1 : -1;

    switch (g->mode) {
    case GAME_TITLE:
        /* The dial does not move: it is still at 0 when Start is touched. */
        set_mode(g, GAME_INSTRUCTIONS, now_ms);
        return GAME_EV_INSTRUCTIONS;

    case GAME_INSTRUCTIONS:
        return GAME_EV_NONE;      /* only the Start button moves on */

    case GAME_UNLOCKED:
        if (now_ms - g->mode_since_ms >= GAME_REPLAY_LOCKOUT_MS) {
            return start_round(g, now_ms);
        }
        return GAME_EV_NONE;

    case GAME_WRONG:
        return GAME_EV_NONE;      /* frozen in red until the reset */

    case GAME_PLAY:
        update_speed(g, detents, now_ms);
        /* Detent by detent: a fast spin is judged at every position it
         * crosses, so passing the number is never skipped over. */
        for (int i = 0; i != detents; i += sign) {
            ev |= step(g, sign, now_ms);
            if (g->mode != GAME_PLAY) {
                break;            /* rest of the spin is discarded */
            }
        }
        return ev;
    }
    return ev;
}

unsigned game_tap(game_t *g, uint32_t now_ms)
{
    if (g->mode == GAME_TITLE) {
        set_mode(g, GAME_INSTRUCTIONS, now_ms);
        return GAME_EV_INSTRUCTIONS;
    }
    if (g->mode == GAME_UNLOCKED &&
        now_ms - g->mode_since_ms >= GAME_REPLAY_LOCKOUT_MS) {
        return start_round(g, now_ms);
    }
    return GAME_EV_NONE;
}

unsigned game_start(game_t *g, uint32_t now_ms)
{
    if (g->mode == GAME_INSTRUCTIONS) {
        return start_round(g, now_ms);
    }
    return GAME_EV_NONE;
}

unsigned game_new(game_t *g, uint32_t now_ms)
{
    return game_reset(g, now_ms) | start_round(g, now_ms);
}

void game_pause(game_t *g, uint32_t now_ms)
{
    if (!g->paused) {
        g->paused = true;
        g->paused_ms = now_ms;
    }
}

void game_resume(game_t *g, uint32_t now_ms)
{
    if (!g->paused) {
        return;
    }
    const uint32_t d = now_ms - g->paused_ms;
    g->paused = false;
    /* Shift every timestamp the rules measure from by the time away. */
    g->mode_since_ms += d;
    g->last_turn_ms += d;
    if (g->started_ms) {
        g->started_ms += d;
    }
    if (g->finished_ms) {
        g->finished_ms += d;
    }
}

unsigned game_reset(game_t *g, uint32_t now_ms)
{
    const uint32_t keep_rng = s_rng;
    const play_t play = g->play;
    uint8_t code[GAME_CODE_LEN];
    for (int k = 0; k < GAME_CODE_LEN; k++) {
        code[k] = g->fixed_code[k];
    }
    const uint8_t start = g->start_level;
    game_init(g, keep_rng);           /* title, level 0, ticks 0 */
    s_rng = keep_rng;
    g->play = play;                   /* ... but how rounds are played stays */
    for (int k = 0; k < GAME_CODE_LEN; k++) {
        g->fixed_code[k] = code[k];
    }
    g->start_level = start;
    set_mode(g, GAME_TITLE, now_ms);
    return GAME_EV_RESET;
}

unsigned game_tick(game_t *g, uint32_t now_ms)
{
    if (g->mode == GAME_PLAY && g->on_target && g->stage == GAME_CODE_LEN - 1 &&
        now_ms - g->last_turn_ms >= GAME_OPEN_DWELL_MS) {
        return open_safe(g, now_ms);
    }
    if (g->mode == GAME_PLAY && game_params(g)->time_ms != 0 &&
        game_time_left_ms(g, now_ms) == 0) {
        const unsigned ev = fail(g, now_ms);
        g->timed_out = true;
        g->loss = LOSS_TIME;
        return ev | GAME_EV_TIMEOUT;
    }
    if (g->mode == GAME_WRONG && now_ms - g->mode_since_ms >= GAME_WRONG_HOLD_MS) {
        /* A new game: dial back to 0, fresh numbers dealt from there, the
         * attempt count and the clock restarted. */
        g->ticks = 0;
        g->speed_dps = 0;
        return start_round(g, now_ms) | GAME_EV_RESUMED;
    }
    return GAME_EV_NONE;
}

uint32_t game_elapsed_s(const game_t *g, uint32_t now_ms)
{
    if (g->started_ms == 0 && (g->mode == GAME_TITLE || g->mode == GAME_INSTRUCTIONS)) {
        return 0;
    }
    const uint32_t end = g->finished_ms ? g->finished_ms : now_ms;
    return (end - g->started_ms) / 1000u;
}

void game_set_play(game_t *g, play_t play, const uint8_t code[GAME_CODE_LEN])
{
    g->play = play < PLAY_COUNT ? play : PLAY_CLASSIC;
    for (int k = 0; k < GAME_CODE_LEN; k++) {
        g->fixed_code[k] = code ? (uint8_t)(code[k] % GAME_POSITIONS) : 0;
    }
}

void game_set_start_level(game_t *g, int level)
{
    g->start_level = (uint8_t)(level >= 1 && level <= levels_count() ? level : 1);
}

unsigned game_caught(game_t *g, uint32_t now_ms)
{
    if (g->mode != GAME_PLAY) {
        return GAME_EV_NONE;
    }
    g->timed_out = false;
    const unsigned ev = fail(g, now_ms);
    g->loss = LOSS_CAUGHT;
    return ev;
}

void game_daily_code(uint32_t yyyymmdd, uint8_t out[GAME_CODE_LEN])
{
    /* Its own generator, seeded by the date alone, so every knob deals the
     * same three numbers today -- with the same spacing rule as a normal
     * deal, from the dial at rest on 0. */
    uint32_t r = yyyymmdd * 2654435761u ^ 0x5AFEC0DEu;
    int prev = 0;
    for (int k = 0; k < GAME_CODE_LEN; k++) {
        int v;
        do {
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            v = (int)(r % GAME_POSITIONS);
        } while (dist(v, prev) < 5);
        out[k] = (uint8_t)v;
        prev = v;
    }
}
