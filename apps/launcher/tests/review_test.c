/*
 * RetroStone Arcade: the review prompt's rules (apps/launcher/src/rsb_review.h), scenario by scenario, with a fake
 * clock (the day numbers given to rsb_review_game_ended). Run by make -f apps/launcher/bundle.mk check.
 *   review_test <work dir>
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rsb_review.h"
#include <stdio.h>
#include <string.h>

enum { NO = 0, BEST = RSB_REVIEW_ASK_BEST, TENTH = RSB_REVIEW_ASK_RETURNS, FORCED = RSB_REVIEW_ASK_FORCED };
typedef struct { int n, day, new_best, force, expect; } step;   /* n games back to the menu, each giving expect */
typedef struct { const char *name; step s[12]; } scenario;

static const scenario table[] = {
    {"never on the first exit, even with a new best", {{1, 100, 1, 0, NO}}},
    {"6 games on one day: not eligible", {{12, 100, 1, 0, NO}}},
    {"5 games, then the 6th on another day with a new best: asked",
     {{5, 100, 0, 0, NO}, {1, 101, 1, 0, BEST}}},
    {"6 games on 2 days, then a new best", {{5, 100, 1, 0, NO}, {1, 103, 0, 0, NO}, {1, 103, 1, 0, BEST}}},
    {"7 games on day 1, the 8th on day 2 with a new best", {{7, 100, 1, 0, NO}, {1, 101, 1, 0, BEST}}},
    {"no new best: the 10th return once eligible", {{5, 100, 0, 0, NO}, {9, 101, 0, 0, NO}, {1, 102, 0, 0, TENTH}}},
    {"the 10th return counts again after a request",
     {{5, 100, 0, 0, NO}, {9, 101, 0, 0, NO}, {1, 101, 0, 0, TENTH}, {9, 140, 0, 0, NO}, {1, 140, 0, 0, TENTH}}},
    {"30 days between requests", {{5, 100, 0, 0, NO}, {1, 101, 1, 0, BEST}, {1, 101, 1, 0, NO},
                                  {1, 130, 1, 0, NO}, {1, 131, 1, 0, BEST}}},
    {"the returns during the 30 days do not count",
     {{5, 100, 0, 0, NO}, {1, 101, 1, 0, BEST}, {20, 110, 0, 0, NO}, {9, 131, 0, 0, NO}, {1, 131, 0, 0, TENTH}}},
    {"3 requests at most, ever", {{5, 100, 0, 0, NO}, {1, 101, 1, 0, BEST}, {1, 131, 1, 0, BEST},
                                  {1, 161, 1, 0, BEST}, {1, 191, 1, 0, NO}, {20, 900, 1, 0, NO}}},
    {"a clock set back does not ask", {{5, 100, 0, 0, NO}, {1, 101, 1, 0, BEST}, {1, 50, 1, 0, NO}}},
    {"the debug flag asks at once, and is not counted", {{1, 100, 0, 1, FORCED}, {1, 100, 0, 1, FORCED},
                                                         {4, 100, 0, 0, NO}, {1, 101, 1, 0, BEST}}},
};

int main(int argc, char **argv)
{
    int fails = 0;
    for (unsigned t = 0; t < sizeof table / sizeof table[0]; t++) {
        rsb_review r;
        rsb_review_reset(&r);
        int game = 0, ok = 1;
        for (const step *s = table[t].s; s->n && ok; s++)
            for (int i = 0; i < s->n && ok; i++) {
                int got = rsb_review_game_ended(&r, s->day, s->new_best, s->force);
                game++;
                if (got != s->expect) {
                    printf("FAIL: %s: game %d (day %d): %d, expected %d\n", table[t].name, game, s->day, got, s->expect);
                    ok = 0;
                }
            }
        if (!ok) fails++;
        else printf("ok   review: %s\n", table[t].name);
    }
    /* the counters' file: a round trip, and a missing file is a fresh start */
    char path[1024];
    snprintf(path, sizeof path, "%s/review.cfg", argc > 1 ? argv[1] : ".");
    rsb_review a, b;
    rsb_review_reset(&a);
    a.games = 17; a.days = 4; a.last_play_day = 20000; a.returns = 3; a.last_ask_day = 19990; a.asks = 2;
    if (rsb_review_save(&a, path) || rsb_review_load(&b, path) || memcmp(&a, &b, sizeof a)) {
        printf("FAIL: review: the counters' file round trip (%s)\n", path);
        fails++;
    }
    remove(path);
    if (rsb_review_load(&b, path) == 0 || b.games || b.last_ask_day != -1) {
        printf("FAIL: review: a missing file is not a fresh start\n");
        fails++;
    }
    if (rsb_review_today() < 20000) {                        /* 2024-10-04 */
        printf("FAIL: review: today is day %d\n", rsb_review_today());
        fails++;
    }
    if (!fails) printf("ok   review: the counters' file, today = day %d\n", rsb_review_today());
    return fails != 0;
}
