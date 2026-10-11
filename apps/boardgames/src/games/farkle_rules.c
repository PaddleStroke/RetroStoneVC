/*
 * Farkle: the rules (farkle.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/farkle.h"

/* the best reading of these counts (c[v]: dice showing v) using every die; -1: some die cannot score */
static int best(int c[7], int top)
{
    int n = 0, pairs = 0, triples = 0, quads = 0, ones = 0, b = -1;
    for (int v = 1; v <= 6; v++) {
        n += c[v];
        pairs += c[v] == 2;
        triples += c[v] == 3;
        quads += c[v] == 4;
        ones += c[v] == 1;
    }
    if (!n) return 0;
    if (top && n == 6) {                        /* the six-dice combinations */
        if (ones == 6) b = 1500;                /* the straight */
        if (pairs == 3 || (quads == 1 && pairs == 1)) b = b > 1500 ? b : 1500;
        if (triples == 2) b = 2500;
        for (int v = 1; v <= 6; v++)
            if (c[v] == 6) b = 3000;
    }
    for (int v = 1; v <= 6; v++) {
        static const int kind[7] = {0, 0, 0, 0, 1000, 2000, 3000};
        for (int k = 3; k <= c[v]; k++) {       /* k of a kind */
            int pts = k == 3 ? (v == 1 ? 1000 : 100 * v) : kind[k];
            c[v] -= k;
            int r = best(c, 0);
            c[v] += k;
            if (r >= 0 && pts + r > b) b = pts + r;
        }
    }
    for (int v = 1; v <= 5; v += 4)             /* a single 1 or 5 */
        if (c[v]) {
            c[v]--;
            int r = best(c, 0);
            c[v]++;
            if (r >= 0 && (v == 1 ? 100 : 50) + r > b) b = (v == 1 ? 100 : 50) + r;
        }
    return b;
}

int fk_value(const uint8_t *dice, int n)
{
    int c[7] = {0};
    for (int i = 0; i < n; i++) {
        if (dice[i] < 1 || dice[i] > 6) return 0;
        c[dice[i]]++;
    }
    int b = best(c, 1);
    return b > 0 ? b : 0;
}

int fk_sel_value(const fpos *p, unsigned sel)
{
    uint8_t d[FK_DICE];
    int n = 0;
    if (p->phase != FK_CHOOSE || !sel || (sel >> p->nd)) return 0;
    for (int i = 0; i < p->nd; i++)
        if (sel >> i & 1) d[n++] = p->dice[i];
    return fk_value(d, n);
}

unsigned fk_scoring(const fpos *p)
{
    unsigned m = 0;
    if (p->phase != FK_CHOOSE) return 0;
    for (unsigned s = 1; s < 1u << p->nd; s++)
        if ((s & ~m) && fk_sel_value(p, s)) m |= s;
    return m;
}

static int scoring_throw(const uint8_t *d, int nd)    /* 1: some die scores */
{
    int c[7] = {0};
    for (int i = 0; i < nd; i++) c[d[i]]++;
    if (c[1] || c[5]) return 1;
    for (int v = 2; v <= 6; v++)
        if (c[v] >= 3) return 1;
    if (nd == 6) {                              /* three pairs (no 1 nor 5, no three alike here) */
        int pairs = 0;
        for (int v = 2; v <= 6; v++) pairs += c[v] == 2;
        if (pairs == 3) return 1;
    }
    return 0;
}

void fk_new(fgame *g, int players)
{
    memset(g, 0, sizeof *g);
    fpos *p = &g->pos;
    p->players = (uint8_t)(players < 1 ? 1 : players > FK_PLAYERS ? FK_PLAYERS : players);
    p->final_by = -1;
    p->winner = -1;
    p->phase = FK_THROW;
}

int fk_leader(const fpos *p)
{
    int w = 0;
    for (int q = 1; q < p->players; q++)
        if (p->score[q] > p->score[w]) w = q;
    return w;
}

static void push(fgame *g, int kind, unsigned sel, int nd, const int *v)
{
    g->past[g->n] = g->pos;
    fevent *e = &g->ev[g->n++];
    memset(e, 0, sizeof *e);
    e->kind = (uint8_t)kind;
    e->sel = (uint8_t)sel;
    e->nd = (uint8_t)nd;
    for (int i = 0; i < nd && v; i++) e->v[i] = (uint8_t)v[i];
}

/* the turn over: the next player, the final round, the end */
static void end_turn(fpos *p)
{
    p->turns++;
    p->turn_pts = 0;
    p->naside = 0;
    p->nd = 0;
    p->throws = 0;
    p->phase = FK_THROW;
    if (p->players == 1) {                      /* alone: the target within the turns */
        if (p->score[0] >= FK_TARGET) {
            p->phase = FK_OVER;
            p->winner = 0;
        } else if (p->turns >= FK_SOLO_TURNS) p->phase = FK_OVER;
        return;
    }
    if (p->final_by < 0 && p->score[p->turn] >= FK_TARGET) {    /* the final round: everyone else once more */
        p->final_by = (int8_t)p->turn;
        p->final_left = (uint8_t)(((1u << p->players) - 1) & ~(1u << p->turn));
    } else if (p->final_by >= 0) p->final_left &= (uint8_t)~(1u << p->turn);
    if (p->final_by >= 0 && !p->final_left) {   /* over: the highest score */
        p->phase = FK_OVER;
        int w = fk_leader(p), n = 0;
        for (int q = 0; q < p->players; q++) n += p->score[q] == p->score[w];
        p->winner = (int8_t)(n > 1 ? -1 : w);
        return;
    }
    p->turn = (uint8_t)((p->turn + 1) % p->players);
}

int fk_to_throw(const fpos *p, unsigned sel)
{
    if (p->phase == FK_THROW) return FK_DICE;
    int left = p->nd - __builtin_popcount(sel & ((1u << p->nd) - 1));
    return left ? left : FK_DICE;               /* hot dice: all six again */
}

int fk_throw(fgame *g, unsigned sel, const int *v)
{
    fpos *p = &g->pos;
    if (p->phase == FK_OVER || g->n >= FK_MAX_EVENTS) return 0;
    if (p->phase == FK_THROW && sel) return 0;
    int add = 0;
    if (p->phase == FK_CHOOSE && !(add = fk_sel_value(p, sel))) return 0;
    int nd = fk_to_throw(p, sel);
    for (int i = 0; i < nd; i++)
        if (v[i] < 1 || v[i] > 6) return 0;
    push(g, FE_THROW, sel, nd, v);
    g->picked = 0;
    if (p->phase == FK_THROW) {                 /* a new turn */
        p->turn_pts = 0;
        p->naside = 0;
        p->throws = 0;
    } else {
        p->turn_pts += add;
        for (int i = 0; i < p->nd; i++)
            if (sel >> i & 1) {
                p->aside[p->naside] = p->dice[i];
                p->group[p->naside++] = (uint8_t)p->throws;
            }
        if (p->naside >= FK_DICE) p->naside = 0;   /* hot dice: picked up again */
    }
    p->nd = (uint8_t)nd;
    for (int i = 0; i < nd; i++) p->dice[i] = (uint8_t)v[i];
    p->throws++;
    p->phase = FK_CHOOSE;
    if (scoring_throw(p->dice, nd)) return FK_THROWN;
    p->farkles = 1;                             /* a farkle: the turn's points lost */
    end_turn(p);
    return FK_FARKLE;
}

int fk_can_bank(const fpos *p, unsigned sel)
{
    int add = fk_sel_value(p, sel);
    if (!add) return 0;
    return p->score[p->turn] > 0 || p->turn_pts + add >= FK_ENTRY;
}

int fk_bank(fgame *g, unsigned sel)
{
    fpos *p = &g->pos;
    if (!fk_can_bank(p, sel) || g->n >= FK_MAX_EVENTS) return 0;
    push(g, FE_BANK, sel, 0, NULL);
    g->picked = 0;
    p->score[p->turn] += p->turn_pts + fk_sel_value(p, sel);
    p->farkles = 0;
    end_turn(p);
    return p->phase == FK_OVER ? 2 : 1;
}

int fk_undo(fgame *g)
{
    int k = g->n - 1;
    while (k >= 0 && g->ev[k].kind != FE_BANK) {
        if (g->ev[k].sel) return 0;             /* dice set aside to throw again: never thrown again */
        k--;
    }
    if (k < 0) return 0;
    g->pos = g->past[k];
    g->n = k;
    g->picked = 0;
    return 1;
}

int fk_script(fgame *g, const char *s)
{
    unsigned sel = g->picked;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len = 0;
        while (s[len] && s[len] != ' ') len++;
        if (*s == 'd') {                        /* a throw */
            int v[FK_DICE], n = 0;
            for (int i = 1; i < len; i++) {
                if (s[i] < '1' || s[i] > '6' || n >= FK_DICE) return 0;
                v[n++] = s[i] - '0';
            }
            if (n != fk_to_throw(&g->pos, sel) || !fk_throw(g, sel, v)) return 0;
            sel = 0;
        } else if (*s == 'a') {                 /* dice set aside, by their faces */
            const fpos *p = &g->pos;
            if (p->phase != FK_CHOOSE || len < 2) return 0;
            sel = 0;
            for (int i = 1; i < len; i++) {
                int found = 0;
                for (int k = 0; k < p->nd && !found; k++)
                    if (!(sel >> k & 1) && p->dice[k] == s[i] - '0') {
                        sel |= 1u << k;
                        found = 1;
                    }
                if (!found) return 0;
            }
        } else if (*s == 'b' && len == 1) {
            if (!fk_bank(g, sel)) return 0;
            sel = 0;
        } else return 0;
        s += len;
    }
    g->picked = sel;
    return 1;
}
