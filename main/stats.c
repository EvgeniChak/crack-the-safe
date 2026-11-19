/*
 * Progress, records and settings, kept in NVS. See stats.h.
 */

#include "stats.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static const char *TAG = "stats";

#define NVS_NS      "stats"
#define NVS_KEY     "s1"
#define MAGIC       0x53544131u       /* "STA1"; bump to start over */
#define MAX_LEVELS  32

typedef struct {
    uint32_t ms;                      /* whole combination                  */
    uint32_t split[GAME_CODE_LEN];    /* ms from the start to each number   */
    uint8_t  level;
    uint8_t  play;
    uint16_t attempts;
} solve_t;

typedef struct {
    uint32_t magic;
    uint8_t  level;                   /* Classic: carry on from here        */
    uint8_t  guard, mic, sleep_min;   /* settings                           */
    uint32_t wins, losses, timeouts, caught;
    uint32_t lost_on[GAME_CODE_LEN];  /* losses by the number being searched */
    uint32_t best_ms[MAX_LEVELS];     /* Classic, per level                 */
    solve_t  recent[STATS_RECENT];    /* Classic, oldest first              */
    uint8_t  recent_n;
    uint8_t  chal_set;
    uint8_t  chal_code[GAME_CODE_LEN];
    uint32_t daily_date;              /* yyyymmdd the two below are for     */
    uint32_t daily_best_ms;
    uint32_t daily_tries;
} store_t;

static store_t           s;
static SemaphoreHandle_t s_lock;

#define LOCK()   xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)

static void defaults(void)
{
    memset(&s, 0, sizeof s);
    s.magic = MAGIC;
    s.level = 1;
    s.mic = 1;
    s.sleep_min = 5;
}

static void save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(h, NVS_KEY, &s, sizeof s) == ESP_OK) {
        nvs_commit(h);
    }
    nvs_close(h);
}

void stats_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    defaults();
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        store_t tmp;
        size_t n = sizeof tmp;
        if (nvs_get_blob(h, NVS_KEY, &tmp, &n) == ESP_OK && n == sizeof tmp &&
            tmp.magic == MAGIC) {
            s = tmp;
        }
        nvs_close(h);
    }
    if (s.level < 1 || s.level > levels_count()) {
        s.level = 1;
    }
    ESP_LOGI(TAG, "level %u, %lu wins, %lu losses", s.level,
             (unsigned long)s.wins, (unsigned long)s.losses);
}

/* ---- Dates ------------------------------------------------------------------ */

uint32_t stats_today(void)
{
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 < 2025) {
        return 0;                     /* the clock was never set */
    }
    return (uint32_t)((tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday);
}

/* A new day forgets yesterday's Daily. Lock held. */
static void roll_daily(void)
{
    const uint32_t today = stats_today();
    if (today && s.daily_date != today) {
        s.daily_date = today;
        s.daily_best_ms = 0;
        s.daily_tries = 0;
    }
}

/* ---- Averages ----------------------------------------------------------------- */

/* Lock held. */
static uint32_t ao_locked(int n)
{
    if (n < 3 || s.recent_n < n) {
        return 0;
    }
    uint64_t sum = 0;
    uint32_t lo = UINT32_MAX, hi = 0;
    for (int i = s.recent_n - n; i < s.recent_n; i++) {
        const uint32_t t = s.recent[i].ms;
        sum += t;
        lo = t < lo ? t : lo;
        hi = t > hi ? t : hi;
    }
    return (uint32_t)((sum - lo - hi) / (uint64_t)(n - 2));
}

uint32_t stats_ao(int n)
{
    LOCK();
    const uint32_t v = ao_locked(n);
    UNLOCK();
    return v;
}

/* ---- Rounds ------------------------------------------------------------------- */

stats_win_t stats_win(const game_t *g)
{
    stats_win_t w = { 0 };
    const uint32_t ms = g->finished_ms - g->started_ms;
    LOCK();
    s.wins++;
    if (g->play == PLAY_CLASSIC) {
        const int lv = g->level;
        if (lv >= 1 && lv <= MAX_LEVELS) {
            w.prev_ms = s.best_ms[lv - 1];
            if (w.prev_ms == 0 || ms < w.prev_ms) {
                s.best_ms[lv - 1] = ms;
                w.pb = true;
            }
        }
        if (s.recent_n == STATS_RECENT) {
            memmove(&s.recent[0], &s.recent[1], sizeof s.recent[0] * (STATS_RECENT - 1));
            s.recent_n--;
        }
        solve_t *r = &s.recent[s.recent_n++];
        r->ms = ms;
        r->level = (uint8_t)lv;
        r->play = (uint8_t)g->play;
        r->attempts = g->attempts;
        for (int k = 0; k < GAME_CODE_LEN; k++) {
            r->split[k] = g->split_ms[k];
        }
        w.ao5_ms = ao_locked(5);
        const int next = game_next_level(g);
        s.level = (uint8_t)next;      /* carry on from the next level */
    } else if (g->play == PLAY_DAILY) {
        roll_daily();
        s.daily_tries++;
        w.prev_ms = s.daily_best_ms;
        if (s.daily_best_ms == 0 || ms < s.daily_best_ms) {
            s.daily_best_ms = ms;
            w.daily_pb = true;
        }
    } else if (g->play == PLAY_CHALLENGE) {
        s.chal_set = 0;               /* cracked: the friend's code is used up */
    }
    save();
    UNLOCK();
    return w;
}

void stats_loss(const game_t *g)
{
    LOCK();
    s.losses++;
    if (g->loss == LOSS_TIME) {
        s.timeouts++;
    } else if (g->loss == LOSS_CAUGHT) {
        s.caught++;
    }
    if (g->stage < GAME_CODE_LEN) {
        s.lost_on[g->stage]++;
    }
    if (g->play == PLAY_DAILY) {
        roll_daily();
        s.daily_tries++;
    }
    save();
    UNLOCK();
}

int stats_level(void)
{
    LOCK();
    const int v = s.level;
    UNLOCK();
    return v;
}

void stats_set_level(int level)
{
    LOCK();
    if (level >= 1 && level <= levels_count() && s.level != level) {
        s.level = (uint8_t)level;
        save();
    }
    UNLOCK();
}

uint32_t stats_best(int level)
{
    LOCK();
    const uint32_t v = level >= 1 && level <= MAX_LEVELS ? s.best_ms[level - 1] : 0;
    UNLOCK();
    return v;
}

uint32_t stats_daily_best(void)
{
    LOCK();
    roll_daily();
    const uint32_t v = s.daily_best_ms;
    UNLOCK();
    return v;
}

/* ---- Challenge -------------------------------------------------------------------- */

bool stats_challenge(uint8_t code[GAME_CODE_LEN])
{
    LOCK();
    const bool set = s.chal_set;
    if (set && code) {
        memcpy(code, s.chal_code, GAME_CODE_LEN);
    }
    UNLOCK();
    return set;
}

void stats_set_challenge(const uint8_t code[GAME_CODE_LEN])
{
    LOCK();
    memcpy(s.chal_code, code, GAME_CODE_LEN);
    s.chal_set = 1;
    save();
    UNLOCK();
}

void stats_clear_challenge(void)
{
    LOCK();
    s.chal_set = 0;
    save();
    UNLOCK();
}

/* ---- Settings ---------------------------------------------------------------------- */

#define SETTER(name, field, type)            \
    void name(type v)                        \
    {                                        \
        LOCK();                              \
        if (s.field != (uint8_t)v) {         \
            s.field = (uint8_t)v;            \
            save();                          \
        }                                    \
        UNLOCK();                            \
    }

bool    stats_guard(void)     { LOCK(); const bool v = s.guard; UNLOCK(); return v; }
bool    stats_mic(void)       { LOCK(); const bool v = s.mic; UNLOCK(); return v; }
uint8_t stats_sleep_min(void) { LOCK(); const uint8_t v = s.sleep_min; UNLOCK(); return v; }
SETTER(stats_set_guard, guard, bool)
SETTER(stats_set_mic, mic, bool)
SETTER(stats_set_sleep_min, sleep_min, uint8_t)

void stats_reset(void)
{
    LOCK();
    const uint8_t guard = s.guard, mic = s.mic, sleep_min = s.sleep_min;
    defaults();
    s.guard = guard;
    s.mic = mic;
    s.sleep_min = sleep_min;
    save();
    UNLOCK();
}

/* ---- JSON ---------------------------------------------------------------------------- */

char *stats_json(void)
{
    cJSON *j = cJSON_CreateObject();
    LOCK();
    roll_daily();
    cJSON_AddNumberToObject(j, "level", s.level);
    cJSON_AddNumberToObject(j, "levels", levels_count());
    cJSON_AddNumberToObject(j, "wins", s.wins);
    cJSON_AddNumberToObject(j, "losses", s.losses);
    cJSON_AddNumberToObject(j, "timeouts", s.timeouts);
    cJSON_AddNumberToObject(j, "caught", s.caught);
    cJSON *lost = cJSON_AddArrayToObject(j, "lost_on");
    for (int k = 0; k < GAME_CODE_LEN; k++) {
        cJSON_AddItemToArray(lost, cJSON_CreateNumber(s.lost_on[k]));
    }
    cJSON *best = cJSON_AddArrayToObject(j, "best_ms");
    for (int i = 0; i < levels_count() && i < MAX_LEVELS; i++) {
        cJSON_AddItemToArray(best, cJSON_CreateNumber(s.best_ms[i]));
    }
    cJSON_AddNumberToObject(j, "ao5", ao_locked(5));
    cJSON_AddNumberToObject(j, "ao12", ao_locked(12));
    cJSON *rec = cJSON_AddArrayToObject(j, "recent");
    for (int i = 0; i < s.recent_n; i++) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "ms", s.recent[i].ms);
        cJSON_AddNumberToObject(r, "level", s.recent[i].level);
        cJSON_AddNumberToObject(r, "attempts", s.recent[i].attempts);
        cJSON *sp = cJSON_AddArrayToObject(r, "split");
        for (int k = 0; k < GAME_CODE_LEN; k++) {
            cJSON_AddItemToArray(sp, cJSON_CreateNumber(s.recent[i].split[k]));
        }
        cJSON_AddItemToArray(rec, r);
    }
    cJSON_AddNumberToObject(j, "today", stats_today());
    cJSON_AddNumberToObject(j, "daily_best", s.daily_best_ms);
    cJSON_AddNumberToObject(j, "daily_tries", s.daily_tries);
    cJSON_AddBoolToObject(j, "challenge", s.chal_set);
    cJSON_AddBoolToObject(j, "guard", s.guard);
    cJSON_AddBoolToObject(j, "mic", s.mic);
    cJSON_AddNumberToObject(j, "sleep_min", s.sleep_min);
    UNLOCK();
    char *out = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return out;
}
