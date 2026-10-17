/*
 * The Spy: the rules (spy.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/spy.h"

static uint32_t rnd(spy_state *s, uint32_t n)
{
    s->rng ^= s->rng << 13;
    s->rng ^= s->rng >> 17;
    s->rng ^= s->rng << 5;
    return s->rng % n;
}

static void deal(spy_state *s)
{
    s->phase = SPY_LOOK;
    s->looker = 0;
    s->place = s->deck[s->round];
    s->spy = (int)rnd(s, (uint32_t)s->n);
    s->first = s->round % s->n;
    s->accused = s->guess = -1;
    s->how = 0;
}

void spy_new(spy_state *s, int n, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->n = n < SPY_MINP ? SPY_MINP : n > SPY_MAXP ? SPY_MAXP : n;
    s->rng = seed * 2654435761u + 0x9e3779b9u;
    if (!s->rng) s->rng = 1;
    for (int i = 0; i < SPY_PLACES; i++) s->deck[i] = (uint8_t)i;
    for (int i = SPY_PLACES - 1; i > 0; i--) {
        int j = (int)rnd(s, (uint32_t)i + 1);
        uint8_t t = s->deck[i];
        s->deck[i] = s->deck[j];
        s->deck[j] = t;
    }
    deal(s);
}

int spy_minutes(int n) { return n <= 3 ? 5 : n == 4 ? 6 : n == 5 ? 7 : 8; }

int spy_looked(spy_state *s)
{
    if (s->phase != SPY_LOOK) return 0;
    if (++s->looker >= s->n) s->phase = SPY_TALK;
    return 1;
}

int spy_call(spy_state *s)
{
    if (s->phase != SPY_TALK) return 0;
    s->phase = SPY_VOTE;
    return 1;
}

int spy_stop(spy_state *s)
{
    if (s->phase != SPY_TALK) return 0;
    s->phase = SPY_GUESS;
    s->how = SPY_STOPPED;
    return 1;
}

static void score(spy_state *s)
{
    memset(s->gain, 0, sizeof s->gain);
    if (s->how && s->guess == s->place) s->gain[s->spy] = 3;
    else if (s->how)
        for (int i = 0; i < s->n; i++) s->gain[i] = i != s->spy;
    else s->gain[s->spy] = 2;
    for (int i = 0; i < s->n; i++) s->score[i] += s->gain[i];
    s->phase = SPY_SCORE;
}

int spy_accuse(spy_state *s, int person)
{
    if (s->phase != SPY_VOTE || person < 0 || person >= s->n) return 0;
    s->accused = person;
    if (person == s->spy) {
        s->how = SPY_CAUGHT;
        s->phase = SPY_GUESS;
    } else score(s);
    return 1;
}

int spy_name(spy_state *s, int place)
{
    if (s->phase != SPY_GUESS || place < 0 || place >= SPY_PLACES) return 0;
    s->guess = place;
    score(s);
    return 1;
}

int spy_next(spy_state *s)
{
    if (s->phase != SPY_SCORE) return 0;
    if (++s->round >= SPY_ROUNDS) {
        s->round = SPY_ROUNDS - 1;
        s->phase = SPY_OVER;
    } else deal(s);
    return 1;
}

unsigned spy_winners(const spy_state *s)
{
    int best = -1;
    unsigned m = 0;
    for (int i = 0; i < s->n; i++) {
        if (s->score[i] > best) { best = s->score[i]; m = 0; }
        if (s->score[i] == best) m |= 1u << i;
    }
    return m;
}

int spy_script(spy_state *s, const char *p)
{
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char c = *p++;
        int k = -1;
        if (*p >= '0' && *p <= '9') k = (int)strtol(p, (char **)&p, 10);
        int ok = c == 'l' ? spy_looked(s) : c == 'c' ? spy_call(s) : c == 's' ? spy_stop(s)
               : c == 'v' ? spy_accuse(s, k) : c == 'g' ? spy_name(s, k) : c == 'n' ? spy_next(s) : 0;
        if (!ok) return 0;
        while (*p && *p != ' ') p++;
    }
    return 1;
}
