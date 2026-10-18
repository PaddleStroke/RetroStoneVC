/*
 * The player's progress across the app (the Stats page): experience points (XP) and levels, the games finished, the
 * days played (the streak), the games won, a few flags for the achievements. Kept in the save store ("stats/..."),
 * on the device only. No SDL drawing here: the shell reports the ends of games, the pages read the totals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The XP rules (the shell applies them, stats_* only add up):
 *   - a puzzle's level solved: STATS_XP_LEVEL + STATS_XP_STAR a star the first time, then STATS_XP_STAR for each star
 *     it gains later; a puzzle made at random (no level), solved: the same as a first time, each time;
 *   - a board or card game finished: STATS_XP_GAME; won by a person against the device: STATS_XP_BOT times the
 *     strongest bot's level (1 easy, 2 medium, 3 hard) on top;
 *   - the Arcade (later): stats_award(STATS_ARCADE, xp) per game played and per new best score.
 * A day counts in the streak once a game or a level is finished on it.
 */
#ifndef STATS_H
#define STATS_H

#include <stdint.h>

#define STATS_SECTIONS 8                        /* the menu's sections (shell.c SEC_*: the Arcade 6, Logic 7) */
#define STATS_ARCADE 6
#define STATS_XP_LEVEL 10
#define STATS_XP_STAR 5
#define STATS_XP_GAME 5
#define STATS_XP_BOT 20

enum { STATS_BEAT_HARD = 1 };                   /* flags: a person beat a hard bot */

typedef struct {
    uint32_t xp, finished;                      /* every XP, every game or level finished */
    int level;                                  /* 1.. */
    uint32_t level_xp, level_need;              /* the XP into this level, and how many it takes to the next */
    int today;                                  /* the games finished today */
    int streak, best;                           /* the days in a row up to today (or yesterday), the longest */
    unsigned week;                              /* this week's days played: bit 0 Monday .. bit 6 Sunday */
    int weekday;                                /* today: 0 Monday .. 6 Sunday */
    unsigned last7;                             /* the last seven days played: bit k, k days ago (bit 0 today) */
    unsigned flags;                             /* STATS_BEAT_HARD... */
    uint32_t sec_xp[STATS_SECTIONS], sec_n[STATS_SECTIONS];   /* by section: XP, games finished */
} stats_t;

void stats_load(void);                          /* from the save store (after save_open) */
/* a game or a level finished (the day counts in the streak): its section, its id, the XP it gives, won (a person won
 * it, a puzzle solved: the per-section profile counts the games won at least once), flags gained */
void stats_finish(int section, const char *id, uint32_t xp, int won, unsigned flags);
/* XP alone (a level's stars improved; the Arcade: a new best score) */
void stats_award(int section, uint32_t xp);
void stats_get(stats_t *s);
int  stats_won(const char *id);                 /* 1: won (solved) at least once */
int  stats_level_need(int level);               /* the XP from level to level + 1 */

/* today, as a day number (days since 1970-01-01, the device's local date); the picture mode fixes it
 * (stats_set_today) so its pictures repeat */
int  stats_today(void);
void stats_set_today(int day);
int  stats_day(int y, int m, int d);            /* a date's day number */
/* the picture mode's made-up progress (--stats-demo): a level, a streak, a week, some games won */
void stats_demo(const char *const *won_ids, int n);

#endif
