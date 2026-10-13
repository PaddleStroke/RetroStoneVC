/*
 * The trick bots' tests (tests/ai_<id>_test.c for the games on games/trickbot.h): what they share. A check line, a
 * view revealed (a test may look at every hand: the deal as the core plays it), a determinised deal checked against
 * its view (the counts, the observer's hand, the known cards, the voids and caps, every card once).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_TRICKS_H
#define AI_TRICKS_H

#include <stdio.h>
#include <string.h>

#include "games/trickbot.h"

static int tt_fails;
static inline void tt_check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    tt_fails += !ok;
}

/* every hand known: hands[s] (n[s] cards) */
static inline void tt_reveal(tb_state *v, const uint8_t *hands, int stride, const uint8_t *n)
{
    for (int s = 0; s < v->players; s++) {
        memcpy(v->hand[s], hands + (size_t)s * stride, n[s]);
        v->n[s] = v->nknown[s] = n[s];
    }
    v->full = 1;
}

/* a determinised deal w of the view v: the counts kept, the known cards where they were, the caps kept, and the
 * cards dealt are the unseen ones (each once) */
static inline int tt_dealt_ok(const tb_state *v, const tb_state *w)
{
    int ok = w->full, cnt[256] = {0};
    for (int s = 0; s < v->players; s++) {
        ok &= w->n[s] == v->n[s];
        for (int i = 0; i < v->nknown[s]; i++) ok &= w->hand[s][i] == v->hand[s][i];
        for (int i = v->nknown[s]; i < w->n[s]; i++) {
            int c = w->hand[s][i];
            cnt[c]++;
            ok &= tb_fits(v, s, c);
        }
    }
    for (int i = v->nstock_known; i < w->nstock; i++) cnt[w->stock[i]]++;
    for (int i = 0; i < w->nrest; i++) cnt[w->rest[i]]++;
    for (int i = 0; i < v->nunseen; i++) cnt[v->unseen[i]]--;
    for (int c = 0; c < 256; c++) ok &= cnt[c] == 0;
    return ok;
}

#ifdef AI_SELFPLAY_H
/* the short run (make test: a few games a pairing) checks the moves, the ends and the time; the scores need the full
 * run's games (card games are luck as well), so their targets apply only with --full */
static inline void tt_targets(sp_pair *P, int n, int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    if (!full)
        for (int i = 0; i < n; i++) P[i].min_pct = 0;
}
#endif

#endif
