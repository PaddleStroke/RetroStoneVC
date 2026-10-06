/*
 * RetroStone Arcade: when to ask for a Google Play review (rsb_review.h has the rules).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rsb_review.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void rsb_review_reset(rsb_review *r)
{
    memset(r, 0, sizeof *r);
    r->last_play_day = r->last_ask_day = -1;
}

int rsb_review_game_ended(rsb_review *r, int day, int new_best, int force)
{
    r->games++;
    if (day != r->last_play_day) {
        r->days++;
        r->last_play_day = day;
    }
    if (force) return RSB_REVIEW_ASK_FORCED;
    if (r->asks >= RSB_REVIEW_MAX) return 0;
    if (r->last_ask_day >= 0 && day - r->last_ask_day < RSB_REVIEW_SPACING) return 0;   /* (a clock set back too) */
    if (r->games < RSB_REVIEW_GAMES || r->days < RSB_REVIEW_DAYS) return 0;
    r->returns++;
    int why = new_best ? RSB_REVIEW_ASK_BEST : r->returns >= RSB_REVIEW_RETURNS ? RSB_REVIEW_ASK_RETURNS : 0;
    if (why) {
        r->asks++;
        r->last_ask_day = day;
        r->returns = 0;
    }
    return why;
}

int rsb_review_today(void)
{
    time_t t = time(NULL);
    const struct tm *tm = localtime(&t);
    if (!tm) return (int)(t / 86400);
    /* days from civil (H. Hinnant's algorithm): the local date, whatever the time zone's offset */
    int y = tm->tm_year + 1900, m = tm->tm_mon + 1, d = tm->tm_mday;
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

#define FIELDS(X) X(games) X(days) X(last_play_day) X(returns) X(last_ask_day) X(asks)

int rsb_review_load(rsb_review *r, const char *path)
{
    rsb_review_reset(r);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char key[32];
    int v;
    while (fscanf(f, " %31[a-z_]=%d", key, &v) == 2) {
#define READ(k) if (!strcmp(key, #k)) r->k = v;
        FIELDS(READ)
#undef READ
    }
    fclose(f);
    return 0;
}

int rsb_review_save(const rsb_review *r, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
#define WRITE(k) fprintf(f, #k "=%d\n", r->k);
    FIELDS(WRITE)
#undef WRITE
    return fclose(f) ? -1 : 0;
}
