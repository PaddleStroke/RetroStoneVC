/*
 * RetroStone Arcade: when to ask for a Google Play review (the in-app review prompt). Pure logic, no SDK: the
 * runtime (rsb_runtime.c) feeds it the games that return to the menu, the tests (tests/review_test.c) a fake clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The owner's rules (apps/android/README.md, "The review prompt"):
 *   - eligible after 6 games played (a launch from the menu that returned to the menu) on 2 different days at least
 *     (so never on the first game exit);
 *   - asked when the player comes back to the menu right after a run that improved the game's best score, or else
 *     at the 10th return to the menu since becoming eligible;
 *   - only there: never in a game, never at the app's start;
 *   - 30 days at least between two requests, 3 requests at most, ever (Play never says whether the player rated).
 */
#ifndef RSB_REVIEW_H
#define RSB_REVIEW_H

#define RSB_REVIEW_GAMES    6           /* games played ... */
#define RSB_REVIEW_DAYS     2           /* ... on that many different days: eligible */
#define RSB_REVIEW_RETURNS  10          /* without a new best: ask at that return to the menu once eligible */
#define RSB_REVIEW_SPACING  30          /* days between two requests */
#define RSB_REVIEW_MAX      3           /* requests, ever */

typedef struct rsb_review {
    int games;                          /* games played */
    int days;                           /* the different days they were played on */
    int last_play_day;                  /* the last of them (a day number: rsb_review_today), -1: none */
    int returns;                        /* returns to the menu while eligible (since the last request's wait) */
    int last_ask_day;                   /* the last request, -1: none */
    int asks;                           /* requests made */
} rsb_review;

#define RSB_REVIEW_ASK_BEST    1        /* rsb_review_game_ended: ask now, a new best */
#define RSB_REVIEW_ASK_RETURNS 2        /* ask now, the 10th return */
#define RSB_REVIEW_ASK_FORCED  3        /* ask now, the debug flag */

void rsb_review_reset(rsb_review *r);
/* A game returned to the menu on <day>; new_best: its run improved the game's best score; force: the debug flag
 * (ask at every return, the counters of requests untouched). Returns 0, or why to ask now (RSB_REVIEW_ASK_*). */
int  rsb_review_game_ended(rsb_review *r, int day, int new_best, int force);
/* Today, the local calendar date, as a day number (days since 1970-01-01). */
int  rsb_review_today(void);
/* The counters' file (key=value lines): a missing or unreadable file is a fresh start. 0: ok. */
int  rsb_review_load(rsb_review *r, const char *path);
int  rsb_review_save(const rsb_review *r, const char *path);

#endif
