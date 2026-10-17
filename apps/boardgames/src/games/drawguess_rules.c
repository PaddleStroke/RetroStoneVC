/*
 * Draw and Guess: the rules (drawguess.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/drawguess.h"

static uint32_t rnd(dg_state *s, uint32_t n)
{
    s->rng ^= s->rng << 13;
    s->rng ^= s->rng >> 17;
    s->rng ^= s->rng << 5;
    return s->rng % n;
}

int dg_rounds(int n) { return n <= 3 ? 3 : 2; }

static void deal(dg_state *s)
{
    s->phase = DG_LOOK;
    s->drawer = s->turn % s->n;
    s->word = s->deck[s->turn % DG_WORDS];
    s->guesser = -1;
    memset(s->gain, 0, sizeof s->gain);
}

void dg_new(dg_state *s, int n, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->n = n < DG_MINP ? DG_MINP : n > DG_MAXP ? DG_MAXP : n;
    s->turns = s->n * dg_rounds(s->n);
    s->rng = seed * 2654435761u + 0x3c6ef372u;
    if (!s->rng) s->rng = 1;
    for (int i = 0; i < DG_WORDS; i++) s->deck[i] = (uint16_t)i;
    for (int i = DG_WORDS - 1; i > 0; i--) {
        int j = (int)rnd(s, (uint32_t)i + 1);
        uint16_t t = s->deck[i];
        s->deck[i] = s->deck[j];
        s->deck[j] = t;
    }
    deal(s);
}

int dg_begin(dg_state *s)
{
    if (s->phase != DG_LOOK) return 0;
    s->phase = DG_DRAW;
    return 1;
}

int dg_guessed(dg_state *s, int p)
{
    if (s->phase != DG_DRAW || p < 0 || p >= s->n || p == s->drawer) return 0;
    s->guesser = p;
    s->gain[p] = 2;
    s->gain[s->drawer] = 1;
    s->score[p] += 2;
    s->score[s->drawer] += 1;
    s->phase = DG_SHOW;
    return 1;
}

int dg_timeout(dg_state *s)
{
    if (s->phase != DG_DRAW) return 0;
    s->phase = DG_SHOW;
    return 1;
}

int dg_next(dg_state *s)
{
    if (s->phase != DG_SHOW) return 0;
    if (++s->turn >= s->turns) {
        s->turn = s->turns - 1;
        s->phase = DG_OVER;
    } else deal(s);
    return 1;
}

unsigned dg_winners(const dg_state *s)
{
    int best = -1;
    unsigned m = 0;
    for (int i = 0; i < s->n; i++) {
        if (s->score[i] > best) { best = s->score[i]; m = 0; }
        if (s->score[i] == best) m |= 1u << i;
    }
    return m;
}

int dg_script(dg_state *s, const char *p)
{
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char c = *p++;
        int k = *p >= '0' && *p <= '9' ? (int)strtol(p, (char **)&p, 10) : -1;
        int ok = c == 'l' ? dg_begin(s) : c == 'g' ? dg_guessed(s, k) : c == 't' ? dg_timeout(s)
               : c == 'n' ? dg_next(s) : 0;
        if (!ok) return 0;
        while (*p && *p != ' ') p++;
    }
    return 1;
}
