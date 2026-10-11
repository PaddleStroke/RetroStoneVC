/*
 * Mancala's rules: the sowing, the captures, the feeding, the end, for Oware and Kalah (no SDL; tests/mancala_test.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/mancala.h"

static int side_sum(const mc_pos *p, int player)
{
    int n = 0;
    for (int i = 6 * player; i < 6 * player + 6; i++) n += p->seeds[i];
    return n;
}

void mc_new(mc_game *g, int variant)
{
    memset(g, 0, sizeof *g);
    g->variant = variant == MC_KALAH ? MC_KALAH : MC_OWARE;
    for (int i = 0; i < MC_PITS; i++) g->pos[0].seeds[i] = 4;
}

const mc_pos *mc_now(const mc_game *g) { return &g->pos[g->n]; }
int mc_turn(const mc_game *g) { return mc_now(g)->turn; }
int mc_score(const mc_game *g, int player) { return mc_now(g)->seeds[MC_STORE + (player ? 1 : 0)]; }

int mc_status(const mc_game *g)
{
    const mc_pos *p = mc_now(g);
    if (!p->over) return MC_PLAY;
    int a = p->seeds[MC_STORE], b = p->seeds[MC_STORE + 1];
    return a > b ? MC_WIN0 : b > a ? MC_WIN1 : MC_DRAW;
}

int mc_legal(const mc_game *g, int pit)
{
    const mc_pos *p = mc_now(g);
    if (p->over || g->n >= MC_MAX || pit < 0 || pit >= MC_PITS || MC_OWNER(pit) != p->turn || !p->seeds[pit])
        return 0;
    /* Oware: a player with no seed must be given some, if a move can */
    if (g->variant == MC_OWARE && !side_sum(p, !p->turn)) return p->seeds[pit] >= 6 * (p->turn + 1) - pit;
    return 1;
}

int mc_any(const mc_game *g)
{
    for (int i = 0; i < MC_PITS; i++)
        if (mc_legal(g, i)) return 1;
    return 0;
}

/* the next slot a seed goes to: Kalah passes through the sower's store, never the other's */
static int next_slot(int variant, int s, int player)
{
    if (variant == MC_OWARE) return (s + 1) % MC_PITS;
    int n = s == 5 ? MC_STORE : s == MC_STORE ? 6 : s == 11 ? MC_STORE + 1 : s == MC_STORE + 1 ? 0 : s + 1;
    return n == MC_STORE + !player ? next_slot(variant, n, player) : n;
}

static void take(mc_pos *p, mc_move *m, int pit, int store, int sweep)
{
    m->take[m->ntake++] = (mc_take){(uint8_t)pit, p->seeds[pit], (uint8_t)store, (uint8_t)sweep};
    p->seeds[store] += p->seeds[pit];
    p->seeds[pit] = 0;
}

/* the end: each side's seeds to its player */
static void sweep(mc_pos *p, mc_move *m)
{
    for (int i = 0; i < MC_PITS; i++)
        if (p->seeds[i]) take(p, m, i, MC_STORE + MC_OWNER(i), 1);
    p->over = 1;
}

/* Oware: the same pits and the same player to move as before, with no capture since (the stores the same) */
static int repeated(const mc_game *g)
{
    const mc_pos *now = mc_now(g);
    for (int k = g->n - 1; k >= 0; k--) {
        const mc_pos *p = &g->pos[k];
        if (memcmp(p->seeds + MC_STORE, now->seeds + MC_STORE, 2)) return 0;
        if (p->turn == now->turn && !memcmp(p->seeds, now->seeds, MC_PITS)) return 1;
    }
    return 0;
}

int mc_play(mc_game *g, int pit, mc_move *out)
{
    if (!mc_legal(g, pit)) return 0;
    mc_move m = {.pit = pit};
    mc_pos p = *mc_now(g);
    int me = p.turn, s = pit;
    m.n = p.seeds[pit];
    p.seeds[pit] = 0;
    for (int i = 0; i < m.n; i++) {
        do s = next_slot(g->variant, s, me);
        while (s == pit);                       /* Oware: 12 seeds or more pass by the pit they came from */
        p.seeds[s]++;
        m.to[i] = (uint8_t)s;
    }
    if (g->variant == MC_OWARE) {
        int k = s, n = 0;                       /* the last pit, then the ones before it: 2 or 3 in the other row */
        for (; k >= 0 && k < MC_PITS && MC_OWNER(k) != me && (p.seeds[k] == 2 || p.seeds[k] == 3); k--) n += p.seeds[k];
        if (n && n == side_sum(&p, !me)) m.grand_slam = 1;     /* it would take everything: it takes nothing */
        else for (k = s; n && k >= 0 && MC_OWNER(k) != me && (p.seeds[k] == 2 || p.seeds[k] == 3); k--)
            take(&p, &m, k, MC_STORE + me, 0);
        p.turn = (uint8_t)!me;
    } else {
        if (s == MC_STORE + me) m.again = 1;
        else if (s < MC_PITS && MC_OWNER(s) == me && p.seeds[s] == 1 && p.seeds[11 - s]) {
            take(&p, &m, 11 - s, MC_STORE + me, 0);
            take(&p, &m, s, MC_STORE + me, 0);
        }
        p.turn = (uint8_t)(m.again ? me : !me);
    }
    g->move[g->n] = (uint8_t)pit;
    g->pos[++g->n] = p;
    mc_pos *now = &g->pos[g->n];
    if (g->variant == MC_OWARE) {
        if (now->seeds[MC_STORE + me] > 24) now->over = 1;
        else if (!mc_any(g) || repeated(g) || g->n >= MC_MAX) sweep(now, &m);
    } else if (!side_sum(now, 0) || !side_sum(now, 1) || g->n >= MC_MAX) {
        sweep(now, &m);
    }
    if (out) *out = m;
    return 1;
}

int mc_undo(mc_game *g)
{
    if (!g->n) return 0;
    g->n--;
    return 1;
}
