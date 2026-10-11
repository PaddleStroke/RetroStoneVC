/*
 * Shut the Box: the rules (shutthebox.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "games/shutthebox.h"

int stb_tiles(int variant) { return variant == STB_TWELVE ? 12 : variant == STB_TEN ? 10 : 9; }
int stb_seat(int player) { static const int seat[STB_PLAYERS] = {0, 2, 1, 3}; return seat[player & 3]; }
unsigned stb_full(int tiles) { return ((1u << (tiles + 1)) - 1) & ~1u; }

int stb_sum(unsigned tiles)
{
    int s = 0;
    for (int t = 1; t <= STB_MAX_TILES; t++)
        if (tiles >> t & 1) s += t;
    return s;
}

int stb_combos(unsigned open, int sum)
{
    int n = 0;
    if (sum <= 0) return 0;
    for (unsigned s = open; s; s = (s - 1) & open) n += stb_sum(s) == sum;
    return n;
}

static int makes(unsigned open, int sum) { return sum == 0 || (sum > 0 && stb_combos(open, sum) > 0); }

void stb_new(sgame *g, int variant, int players)
{
    memset(g, 0, sizeof *g);
    spos *p = &g->pos;
    p->variant = (uint8_t)(variant >= 0 && variant < STB_VARIANTS ? variant : STB_NINE);
    p->tiles = (uint8_t)stb_tiles(p->variant);
    p->players = (uint8_t)(players < 1 ? 1 : players > STB_PLAYERS ? STB_PLAYERS : players);
    for (int i = 0; i < p->players; i++) p->open[i] = (uint16_t)stb_full(p->tiles);
    p->winner = -1;
    p->phase = STB_THROW;
}

int stb_total(const spos *p) { return p->phase != STB_SHUT ? 0 : p->die[0] + (p->ndice > 1 ? p->die[1] : 0); }

int stb_one_die(const spos *p)
{
    return p->phase == STB_THROW && !(p->open[p->turn] & stb_full(p->tiles) & ~stb_full(6));
}

unsigned stb_usable(const spos *p, unsigned sel)
{
    unsigned open = p->open[p->turn], u = 0;
    int left = stb_total(p) - stb_sum(sel);
    if (p->phase != STB_SHUT || (sel & ~open)) return 0;
    for (int t = 1; t <= p->tiles; t++) {
        unsigned b = 1u << t;
        if ((open & b) && !(sel & b) && makes(open & ~sel & ~b, left - t)) u |= b;
    }
    return u;
}

int stb_score(const spos *p, int player) { return stb_sum(p->open[player]); }

unsigned stb_winners(const spos *p)
{
    if (p->phase != STB_OVER || p->players < 2) return 0;
    if (p->boxed) return 1u << p->winner;
    int best = 1000;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) {
        int s = stb_score(p, q);
        if (s < best) best = s, m = 0;
        if (s == best) m |= 1u << q;
    }
    return m;
}

static void push(sgame *g, uint16_t ev)
{
    g->past[g->n] = g->pos;
    g->ev[g->n++] = ev;
}

/* the player's turn is over: the next one clockwise who has not played, or the end of the round */
static void end_turn(spos *p)
{
    p->played |= (uint8_t)(1u << p->turn);
    int seat = stb_seat(p->turn);
    for (int k = 1; k <= 4; k++)
        for (int q = 0; q < p->players; q++)
            if (stb_seat(q) == ((seat + k) & 3) && !(p->played >> q & 1)) {
                p->turn = (uint8_t)q;
                p->phase = STB_THROW;
                return;
            }
    p->phase = STB_OVER;                        /* the round is over: the lowest score wins, a tie is a draw */
    if (p->players == 1) return;
    int best = 1000, n = 0;
    for (int q = 0; q < p->players; q++) {
        int s = stb_score(p, q);
        if (s < best) best = s, n = 0, p->winner = (int8_t)q;
        n += s == best;
    }
    if (n > 1) p->winner = -1;
}

int stb_throw(sgame *g, int ndice, int first, int second)
{
    spos *p = &g->pos;
    if (p->phase != STB_THROW || g->n >= STB_MAX_EVENTS || (ndice != 1 && ndice != 2)) return 0;
    if (first < 1 || first > 6 || (ndice == 2 && (second < 1 || second > 6))) return 0;
    if (ndice == 1 && !stb_one_die(p)) return 0;
    if (ndice == 1) second = 0;
    push(g, (uint16_t)(STB_THROW_EV | ndice << 8 | first << 4 | second));
    p->ndice = (uint8_t)ndice;
    p->die[0] = (uint8_t)first;
    p->die[1] = (uint8_t)second;
    p->throws++;
    p->phase = STB_SHUT;
    if (stb_combos(p->open[p->turn], stb_total(p))) return ST_SHUT;
    end_turn(p);
    return ST_DEAD;
}

int stb_shut(sgame *g, unsigned tiles)
{
    spos *p = &g->pos;
    if (p->phase != STB_SHUT || g->n >= STB_MAX_EVENTS || !tiles || (tiles & ~(unsigned)p->open[p->turn]) ||
        stb_sum(tiles) != stb_total(p))
        return 0;
    push(g, (uint16_t)tiles);
    p->open[p->turn] = (uint16_t)(p->open[p->turn] & ~tiles);
    p->phase = STB_THROW;
    if (p->open[p->turn]) return 1;
    p->played |= (uint8_t)(1u << p->turn);      /* the box shut: a win at once */
    p->boxed = 1;
    p->winner = (int8_t)p->turn;
    p->phase = STB_OVER;
    return 2;
}

int stb_undo(sgame *g)
{
    int k = g->n - 1;
    while (k >= 0 && (g->ev[k] & STB_THROW_EV)) k--;
    if (k < 0) return 0;
    g->pos = g->past[k];
    g->n = k;
    return 1;
}

int stb_script(sgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 'd') {                        /* a throw: one or two digits */
            int d[2] = {0, 0}, n = 0;
            for (s++; isdigit((unsigned char)*s); s++)
                if (n < 2) d[n++] = *s - '0';
                else return 0;
            if (!n || !stb_throw(g, n, d[0], d[1])) return 0;
        } else if (isdigit((unsigned char)*s)) {  /* tiles: numbers joined by + */
            unsigned tiles = 0;
            for (;;) {
                char *end;
                long t = strtol(s, &end, 10);
                if (end == s || t < 1 || t > STB_MAX_TILES || (tiles >> t & 1)) return 0;
                tiles |= 1u << t;
                s = end;
                if (*s != '+') break;
                s++;
            }
            if (!stb_shut(g, tiles)) return 0;
        } else return 0;
        if (*s && *s != ' ') return 0;
    }
    return 1;
}
