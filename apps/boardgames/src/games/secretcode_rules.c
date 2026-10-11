/*
 * Secret Code: the rules (secretcode.h). No SDL: tested by tests/secretcode_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/secretcode.h"

const sc_variant sc_variants[SC_VARIANTS] = {
    {4, 6, 10, 0},                              /* gentle: no colour twice in the code */
    {4, 6, 10, 1},                              /* classic */
    {5, 8, 12, 1},                              /* harder */
};
const char sc_letters[SC_MAXCOLOURS + 1] = "RYGBWKPS";

int sc_colour(char c)
{
    const char *p = c ? strchr(sc_letters, c >= 'a' && c <= 'z' ? c - 32 : c) : NULL;
    return p ? (int)(p - sc_letters) : -1;
}

/* the blacks: the holes that agree; the whites: for each colour, the pegs both have, less the blacks */
void sc_score(const int8_t *code, const int8_t *guess, int pegs, int *black, int *white)
{
    int b = 0, both = 0, nc[SC_MAXCOLOURS] = {0}, ng[SC_MAXCOLOURS] = {0};
    for (int i = 0; i < pegs; i++) {
        if (code[i] == guess[i]) b++;
        if (code[i] >= 0 && code[i] < SC_MAXCOLOURS) nc[code[i]]++;
        if (guess[i] >= 0 && guess[i] < SC_MAXCOLOURS) ng[guess[i]]++;
    }
    for (int c = 0; c < SC_MAXCOLOURS; c++) both += nc[c] < ng[c] ? nc[c] : ng[c];
    *black = b;
    *white = both - b;
}

void sc_make_code(int variant, int (*rnd)(int), int8_t *code)
{
    const sc_variant *v = &sc_variants[variant < 0 || variant >= SC_VARIANTS ? 1 : variant];
    int8_t bag[SC_MAXCOLOURS];
    for (int c = 0; c < v->colours; c++) bag[c] = (int8_t)c;
    for (int i = 0; i < v->pegs; i++) {
        if (v->repeats) { code[i] = (int8_t)rnd(v->colours); continue; }
        int k = i + rnd(v->colours - i);        /* drawn from the colours not used yet */
        int8_t t = bag[i];
        bag[i] = bag[k];
        bag[k] = t;
        code[i] = bag[i];
    }
}

void sc_new(sc_game *g, int variant, const int8_t *code)
{
    memset(g, 0, sizeof *g);
    g->v = sc_variants[variant < 0 || variant >= SC_VARIANTS ? 1 : variant];
    memset(g->guess, -1, sizeof g->guess);
    memset(g->code, -1, sizeof g->code);
    memcpy(g->code, code, (size_t)g->v.pegs);
}

int sc_status(const sc_game *g)
{
    if (g->n && !memcmp(g->guess[g->n - 1], g->code, (size_t)g->v.pegs)) return SC_SOLVED;
    return g->n >= g->v.rows ? SC_FAILED : SC_PLAY;
}

int sc_left(const sc_game *g) { return g->v.rows - g->n; }

int sc_put(sc_game *g, int hole, int colour)
{
    if (sc_status(g) != SC_PLAY || hole < 0 || hole >= g->v.pegs || colour < 0 || colour >= g->v.colours) return 0;
    int8_t *row = g->guess[g->n];
    if (row[hole] == colour) return 1;          /* already there: nothing to take back */
    if (g->nh == SC_HIST) memmove(g->hist, g->hist + 1, sizeof g->hist[0] * --g->nh);   /* the oldest forgotten */
    g->hist[g->nh].hole = (int8_t)hole;
    g->hist[g->nh++].was = row[hole];
    row[hole] = (int8_t)colour;
    return 1;
}

int sc_undo(sc_game *g)
{
    if (!g->nh || sc_status(g) != SC_PLAY) return -1;
    g->nh--;
    g->guess[g->n][g->hist[g->nh].hole] = g->hist[g->nh].was;
    return g->hist[g->nh].hole;
}

int sc_full(const sc_game *g)
{
    if (sc_status(g) != SC_PLAY) return 0;
    for (int i = 0; i < g->v.pegs; i++)
        if (g->guess[g->n][i] < 0) return 0;
    return 1;
}

int sc_check(sc_game *g)
{
    if (!sc_full(g)) return 0;
    int b, w;
    sc_score(g->code, g->guess[g->n], g->v.pegs, &b, &w);
    g->black[g->n] = (uint8_t)b;
    g->white[g->n] = (uint8_t)w;
    g->n++;
    g->nh = 0;                                  /* a guess checked stays */
    return 1;
}
