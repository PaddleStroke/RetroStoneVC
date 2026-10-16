/*
 * The player's progress (stats.h): XP, levels, games finished, the days played, the games won. Two records in the save
 * store: "stats/main" (the numbers, little-endian) and "stats/won" (the ids of the games won at least once, between
 * spaces).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "stats.h"
#include "shell.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAIN_KEY "stats/main"
#define WON_KEY "stats/won"
#define WON_MAX 4000
#define VERSION 1
/* "stats/main": version (1), xp (4), finished (4), last day (4), days (8: bit k, the day last - k played), today's
 * games (2: on the last day), best streak (2), flags (1), then by section xp (4) and games (4) */
#define MAIN_LEN (1 + 4 + 4 + 4 + 8 + 2 + 2 + 1 + STATS_SECTIONS * 8)

static struct {
    uint32_t xp, finished;
    int32_t last;                               /* the last day played (0: never) */
    uint64_t days;
    uint32_t last_n;                            /* the games finished on the last day */
    uint32_t best;
    uint8_t flags;
    uint32_t sec_xp[STATS_SECTIONS], sec_n[STATS_SECTIONS];
} S;
static char won[WON_MAX + 2] = " ";             /* " id id ... " */
static int fixed_today;                         /* the picture mode's today (0: the device's date) */

/* ---- the calendar ------------------------------------------------------------------------------------------------ */
int stats_day(int y, int m, int d)              /* days from 1970-01-01 (the civil calendar's usual formula) */
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int stats_today(void)
{
    if (fixed_today) return fixed_today;
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);              /* (the main thread only) */
    return lt ? stats_day(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday) : 0;
}

void stats_set_today(int day) { fixed_today = day; }

static int weekday(int day) { return ((day + 3) % 7 + 7) % 7; }   /* 1970-01-01 was a Thursday: 0 Monday */

/* the days played, seen from `day`: bit k, day - k */
static uint64_t days_from(int day)
{
    if (!S.last) return 0;
    int shift = day - S.last;
    if (shift < 0) return S.days;               /* the clock went back: as it was */
    return shift >= 64 ? 0 : S.days << shift;
}

static int run_from(uint64_t d, int k)          /* the days in a row from bit k back */
{
    int n = 0;
    while (k < 64 && (d >> k & 1)) { n++; k++; }
    return n;
}

/* ---- kept -------------------------------------------------------------------------------------------------------- */
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> 8 * i); }
static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static void save(void)
{
    uint8_t b[MAIN_LEN], *p = b;
    *p++ = VERSION;
    put32(p, S.xp); p += 4;
    put32(p, S.finished); p += 4;
    put32(p, (uint32_t)S.last); p += 4;
    put32(p, (uint32_t)S.days); put32(p + 4, (uint32_t)(S.days >> 32)); p += 8;
    p[0] = (uint8_t)S.last_n; p[1] = (uint8_t)(S.last_n >> 8); p += 2;
    p[0] = (uint8_t)S.best; p[1] = (uint8_t)(S.best >> 8); p += 2;
    *p++ = S.flags;
    for (int k = 0; k < STATS_SECTIONS; k++, p += 8) {
        put32(p, S.sec_xp[k]);
        put32(p + 4, S.sec_n[k]);
    }
    ui_save_set(MAIN_KEY, b, MAIN_LEN);
}

void stats_load(void)
{
    memset(&S, 0, sizeof S);
    uint8_t b[MAIN_LEN];
    if (ui_save_get(MAIN_KEY, b, MAIN_LEN) == MAIN_LEN && b[0] == VERSION) {
        const uint8_t *p = b + 1;
        S.xp = get32(p); p += 4;
        S.finished = get32(p); p += 4;
        S.last = (int32_t)get32(p); p += 4;
        S.days = get32(p) | (uint64_t)get32(p + 4) << 32; p += 8;
        S.last_n = p[0] | p[1] << 8; p += 2;
        S.best = p[0] | p[1] << 8; p += 2;
        S.flags = *p++;
        for (int k = 0; k < STATS_SECTIONS; k++, p += 8) {
            S.sec_xp[k] = get32(p);
            S.sec_n[k] = get32(p + 4);
        }
    }
    int n = ui_save_get(WON_KEY, won + 1, WON_MAX);
    won[0] = ' ';
    won[1 + (n > 0 ? n : 0)] = 0;
    if (n <= 0) won[1] = 0;
}

int stats_won(const char *id)
{
    char k[80];
    snprintf(k, sizeof k, " %s ", id);
    return strstr(won, k) != NULL;
}

static void won_add(const char *id)
{
    if (!id || stats_won(id) || strlen(won) + strlen(id) + 2 > WON_MAX) return;
    if (won[strlen(won) - 1] != ' ') strcat(won, " ");
    strcat(won, id);
    strcat(won, " ");
    ui_save_set(WON_KEY, won + 1, (int)strlen(won + 1));
}

/* ---- the events -------------------------------------------------------------------------------------------------- */
void stats_award(int section, uint32_t xp)
{
    if (!xp) return;
    S.xp += xp;
    if (section >= 0 && section < STATS_SECTIONS) S.sec_xp[section] += xp;
    save();
}

void stats_finish(int section, const char *id, uint32_t xp, int is_won, unsigned flags)
{
    int day = stats_today();
    if (day > 0) {
        S.days = days_from(day) | 1;
        S.last_n = S.last == day ? S.last_n + 1 : 1;
        S.last = day;
        uint32_t run = (uint32_t)run_from(S.days, 0);
        if (run > S.best) S.best = run;
    }
    S.finished++;
    S.xp += xp;
    S.flags |= (uint8_t)flags;
    if (section >= 0 && section < STATS_SECTIONS) {
        S.sec_xp[section] += xp;
        S.sec_n[section]++;
    }
    save();
    if (is_won) won_add(id);
}

int stats_level_need(int level) { return 100 + 50 * (level - 1); }

void stats_get(stats_t *s)
{
    memset(s, 0, sizeof *s);
    s->xp = S.xp;
    s->finished = S.finished;
    s->level = 1;
    uint32_t rest = S.xp;
    while (rest >= (uint32_t)stats_level_need(s->level)) rest -= (uint32_t)stats_level_need(s->level++);
    s->level_xp = rest;
    s->level_need = (uint32_t)stats_level_need(s->level);
    int day = stats_today();
    uint64_t d = days_from(day);
    s->today = S.last == day ? (int)S.last_n : 0;
    s->streak = d & 1 ? run_from(d, 0) : run_from(d, 1);   /* not played yet today: yesterday's still counts */
    s->best = (int)S.best > s->streak ? (int)S.best : s->streak;
    s->weekday = weekday(day);
    for (int k = 0; k <= s->weekday; k++)       /* Monday .. today */
        if (d >> k & 1) s->week |= 1u << (s->weekday - k);
    s->flags = S.flags;
    memcpy(s->sec_xp, S.sec_xp, sizeof S.sec_xp);
    memcpy(s->sec_n, S.sec_n, sizeof S.sec_n);
}

void stats_demo(const char *const *ids, int n)
{
    memset(&S, 0, sizeof S);
    int day = stats_today();
    S.last = day;
    S.days = 0x1f | 1ull << 6 | 1ull << 7 | 1ull << 9;   /* five days in a row, a few before */
    S.last_n = 3;
    S.best = 9;
    S.finished = 57;
    S.xp = 1340;
    S.flags = STATS_BEAT_HARD;
    static const uint32_t sx[STATS_SECTIONS] = {520, 300, 180, 120, 160, 60}, sn[STATS_SECTIONS] = {24, 9, 6, 7, 8, 3};
    memcpy(S.sec_xp, sx, sizeof sx);
    memcpy(S.sec_n, sn, sizeof sn);
    save();
    won[0] = ' ';
    won[1] = 0;
    for (int i = 0; i < n; i++) won_add(ids[i]);
}
