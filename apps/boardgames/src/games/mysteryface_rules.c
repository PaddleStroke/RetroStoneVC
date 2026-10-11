/*
 * Mystery Face: the rules (mysteryface.h). No SDL. A game is its list of actions: undo replays them without the one
 * taken back.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/mysteryface.h"

#define K HAIR_BLACK
#define B HAIR_BROWN
#define Y HAIR_BLOND
#define R HAIR_RED
#define W HAIR_WHITE
#define L STYLE_LONG
#define S STYLE_SHORT
#define C STYLE_CURLY
#define X STYLE_BALD
#define G MF_GLASSES
#define H MF_HAT
#define BE MF_BEARD
#define M MF_MOUSTACHE
#define E MF_EARRINGS

/* the 24, so that every question splits them: 5 with glasses, 5 hats, 5 beards, 5 moustaches, 5 earrings, 4 bald,
 * 7 long hair, 5 curly; hair 6 black, 5 brown, 5 blond, 3 red, 5 white; eyes 12 brown, 7 blue, 5 green; skin tones
 * 5, 5, 5, 5, 4. No two alike in the answers. */
const mf_face MF_FACE[MF_FACES] = {
    {K, L, EYE_BROWN, 3, E},        {Y, S, EYE_BLUE, 0, G},         {R, C, EYE_GREEN, 1, 0},
    {W, X, EYE_BLUE, 1, G | BE | M}, {B, S, EYE_BROWN, 2, H | M},   {K, C, EYE_BROWN, 4, E},
    {Y, L, EYE_GREEN, 1, H},        {W, S, EYE_BROWN, 2, M},        {B, L, EYE_BLUE, 0, E | G},
    {K, S, EYE_BROWN, 4, BE},       {R, S, EYE_BLUE, 0, BE | M},    {W, L, EYE_GREEN, 3, H | E},
    {K, X, EYE_BROWN, 2, 0},        {B, C, EYE_GREEN, 3, G},        {Y, S, EYE_BROWN, 2, H | BE | M},
    {W, C, EYE_BLUE, 2, 0},         {B, X, EYE_BROWN, 1, BE},       {K, L, EYE_BLUE, 1, H},
    {R, L, EYE_BROWN, 3, E},        {Y, C, EYE_BROWN, 4, 0},        {W, X, EYE_BROWN, 4, 0},
    {B, S, EYE_GREEN, 0, 0},        {K, S, EYE_BLUE, 3, G},         {Y, L, EYE_BROWN, 0, 0},
};

int mf_has(int f, int q)
{
    if (f < 0 || f >= MF_FACES) return 0;
    const mf_face *c = &MF_FACE[f];
    switch (q) {
    case MQ_HAT: return !!(c->has & MF_HAT);
    case MQ_GLASSES: return !!(c->has & MF_GLASSES);
    case MQ_BEARD: return !!(c->has & MF_BEARD);
    case MQ_MOUSTACHE: return !!(c->has & MF_MOUSTACHE);
    case MQ_EARRINGS: return !!(c->has & MF_EARRINGS);
    case MQ_BALD: return c->style == STYLE_BALD;
    case MQ_LONG: return c->style == STYLE_LONG;
    case MQ_CURLY: return c->style == STYLE_CURLY;
    case MQ_EYE_BROWN: case MQ_EYE_BLUE: case MQ_EYE_GREEN: return c->eyes == q - MQ_EYE_BROWN;
    default: return q >= MQ_BLACK && q <= MQ_WHITE && c->hair == q - MQ_BLACK;
    }
}

static void reset(mfpos *p)
{
    memset(p, 0, sizeof *p);
    p->phase = MF_PICK;
    p->secret[0] = p->secret[1] = -1;
    p->asked = p->answer = p->winner = p->guess = -1;
}

void mf_new(mfgame *g)
{
    reset(&g->pos);
    g->n = 0;
}

int mf_legal(const mfpos *p, int a)
{
    int k = MA_KIND(a), x = MA_ARG(a);
    switch (k) {
    case MA_PICK: return p->phase == MF_PICK && x < MF_FACES;
    case MA_END: return p->phase == MF_PICK ? p->secret[p->turn] >= 0 : p->phase == MF_PLAY && p->asked >= 0;
    case MA_ASK: return p->phase == MF_PLAY && p->asked < 0 && x < MF_QUESTIONS;
    case MA_FLIP: return p->phase == MF_PLAY && x < MF_FACES;
    case MA_GUESS: return p->phase == MF_PLAY && p->asked < 0 && x < MF_FACES && !(p->down[p->turn] >> x & 1);
    }
    return 0;
}

static void apply(mfpos *p, int a)
{
    int x = MA_ARG(a), me = p->turn, other = !me;
    switch (MA_KIND(a)) {
    case MA_PICK: p->secret[me] = (int8_t)x; break;
    case MA_FLIP: p->down[me] ^= 1u << x; break;
    case MA_ASK:
        p->asked = (int8_t)x;
        p->answer = (int8_t)mf_has(p->secret[other], x);
        p->questions[me]++;
        break;
    case MA_GUESS:
        p->guess = (int8_t)x;
        p->winner = (int8_t)(x == p->secret[other] ? me : other);
        p->phase = MF_OVER;
        break;
    case MA_END:
        if (p->phase == MF_PICK) {
            if (me == 1) p->phase = MF_PLAY;    /* both picked: player 0 asks first */
            p->turn = (uint8_t)other;
        } else {
            p->turn = (uint8_t)other;
            p->asked = p->answer = -1;
        }
        break;
    }
}

int mf_play(mfgame *g, int a)
{
    if (g->n >= MF_MAX_ACTS || !mf_legal(&g->pos, a)) return 0;
    apply(&g->pos, a);
    g->act[g->n++] = (uint8_t)a;
    return 1;
}

int mf_undo(mfgame *g)
{
    if (g->pos.phase == MF_OVER) return 0;
    int i = g->n - 1;
    while (i >= 0 && MA_KIND(g->act[i]) != MA_END && MA_KIND(g->act[i]) != MA_FLIP && MA_KIND(g->act[i]) != MA_PICK)
        i--;                                    /* past this turn's question */
    if (i < 0 || MA_KIND(g->act[i]) == MA_END) return 0;
    memmove(g->act + i, g->act + i + 1, (size_t)(g->n - i - 1));
    int n = g->n - 1;
    reset(&g->pos);
    for (int k = 0; k < n; k++) apply(&g->pos, g->act[k]);
    g->n = n;
    return 1;
}

int mf_up(const mfpos *p, int pl)
{
    int n = 0;
    for (int f = 0; f < MF_FACES; f++) n += !(p->down[pl] >> f & 1);
    return n;
}

int mf_script(mfgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int a;
        if (c == 'e') a = MA_END;
        else {
            char *end;
            long v = strtol(s, &end, 10);
            if (end == s || v < 0 || v > 31) return 0;
            s = end;
            a = c == 'p' ? MA_PICK : c == 'q' ? MA_ASK : c == 'f' ? MA_FLIP : c == 'g' ? MA_GUESS : -1;
            if (a < 0) return 0;
            a |= (int)v;
        }
        if (!mf_play(g, a)) return 0;
    }
    return 1;
}
