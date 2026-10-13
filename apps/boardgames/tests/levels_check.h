/*
 * The patience games' designed levels checked (their tests): each one a whole pack in letters (sollevels.h), dealt by
 * the rules, won by the search within the budget the level maker used, and that win played again from the deal (or
 * the win the maker's search found, kept with the level, played: Spider).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef LEVELS_CHECK_H
#define LEVELS_CHECK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/sollevels.h"

/* how many of the `count` levels are right (each rank of each suit there `ncards / 52` times, or as many times in all
 * as the pack has; dealt; won by the search; the win played again); the wrong ones are printed */
static inline int lc_levels(const char *name, const sollevel *lv, int count, int ncards,
                            void (*deal)(solrec *r, int variant, const uint8_t *deck), const solsearch *S,
                            long budget)
{
    static solrec r, q;
    static solmove path[4000];
    int good = 0;
    long most = 0, sum = 0;
    clock_t t0 = clock();
    for (int l = 0; l < count; l++) {
        uint8_t deck[104];
        int ranks[14] = {0};
        int ok = (int)strlen(lv[l].cards) == ncards && sol_cards_read(lv[l].cards, deck, 104) == ncards;
        if (ok) {
            for (int i = 0; i < ncards; i++) ranks[CARD_RANK(deck[i])]++;
            for (int k = 1; k <= 13; k++) ok &= ranks[k] == ncards / 13;
            if (ncards == 52) {                 /* one pack: every card once */
                int seen[256] = {0};
                for (int i = 0; i < 52; i++) ok &= !seen[deck[i]]++;
            }
        }
        long nodes = 0;
        int n = -1;
        if (ok && lv[l].win) {                  /* the win the search found when it made the level: played */
            deal(&r, lv[l].variant, deck);
            ok = sol_win_play(&r, S->R, lv[l].win);
            n = (int)strlen(lv[l].win);
        } else if (ok) {
            deal(&r, lv[l].variant, deck);
            n = sol_solve(S, &r, budget, path, 1500, &nodes);
            ok = n > 0;
        }
        if (ok && !lv[l].win) {                 /* the win again, from the deal */
            deal(&q, lv[l].variant, deck);
            for (int i = 0; i < n && ok; i++) ok = S->R->play(&q, path[i]);
            ok = ok && S->R->won(&q.pos);
        }
        if (!ok) printf("FAIL %s level %d (search: %d moves, %ld positions)\n", name, l + 1, n, nodes);
        good += ok;
        sum += nodes;
        if (nodes > most) most = nodes;
    }
    printf("ok   %s: %d levels won (%s; %ld positions at most, %ld on average; %.1fs)\n", name, good,
           count && lv[0].win ? "the search's wins played" : "by the search", most, sum / (count ? count : 1),
           (double)(clock() - t0) / CLOCKS_PER_SEC);
    return good;
}

#endif
