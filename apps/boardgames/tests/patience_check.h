/*
 * The checks the patience games' tests share (La Belle Lucie, Yukon, Forty Thieves, Golf, Canfield, Montana, Clock;
 * their rules are games/solitaire.h's): every card there once a pack, every legal move listed, whole games of random
 * moves and hints from a seed (each hint legal, the position sane after each move, the same moves played again giving
 * the same game, undo all the way back to the deal), games played by the hint alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef PATIENCE_CHECK_H
#define PATIENCE_CHECK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/solitaire.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

#define S(s, r) CARD(SUIT_##s, r)

static solrec G, H;
static solpos *P = &G.pos;

/* every card of `packs` packs there, once a pack (the aces left out: Montana) */
static inline int pc_whole(const solpos *p, int packs, int aces)
{
    int seen[256] = {0}, n = 0;
    for (int i = 0; i < p->npiles; i++)
        for (int k = 0; k < p->n[i]; k++) seen[p->c[i][k]]++, n++;
    for (int s = 0; s < 4; s++)
        for (int r = aces ? 1 : 2; r <= 13; r++)
            if (seen[CARD(s, r)] != packs) return 0;
    return n == packs * (aces ? 52 : 48);
}

static uint32_t pc_seed = 1;
static inline int pc_rnd(int n)
{
    pc_seed ^= pc_seed << 13;
    pc_seed ^= pc_seed >> 17;
    pc_seed ^= pc_seed << 5;
    return (int)(pc_seed % (uint32_t)n);
}

/* every legal move: the stock's, then each pile's cards to each pile */
static inline int pc_moves(const solrules *R, const solpos *p, solmove *out, int max)
{
    int k = 0;
    solmove s = R->stock(p);
    if (s.kind != SM_NONE) out[k++] = s;
    for (int a = 0; a < p->npiles; a++)
        for (int b = 0; b < p->npiles; b++)
            for (int n = 1; n <= p->n[a] && k < max; n++)
                if (R->legal(p, sol_mv(SM_MOVE, a, b, n))) out[k++] = sol_mv(SM_MOVE, a, b, n);
    return k;
}

/* games from seeds 1..seeds: up to `steps` moves each, random legal ones and hints (one in three); a deal is
 * new_game(r, seed); sane(p) checks a position. Each hint legal, the position sane, the game played again from its
 * player's moves the same, undo back to the deal. *ends: games with no move left, *wins: games won. */
static inline void pc_random(const solrules *R, void (*new_game)(solrec *r, uint32_t seed),
                             int (*sane)(const solpos *p), int seeds, int steps, int *ends, int *wins)
{
    static solmove list[4096], mine[SOL_MOVES];
    int ok = 1;
    *ends = *wins = 0;
    for (int seed = 1; seed <= seeds; seed++) {
        pc_seed = (uint32_t)seed * 7919u;
        new_game(&G, (uint32_t)seed);
        solpos start = G.pos;
        int played = 0;
        for (int step = 0; step < steps; step++) {
            solmove h = R->hint(P);
            if (h.kind != SM_NONE && !R->legal(P, h)) { ok = 0; printf("seed %d: a hint not legal\n", seed); }
            int n = pc_moves(R, P, list, 4096);
            if (!n) { (*ends)++; break; }
            solmove m = pc_rnd(3) == 0 && h.kind != SM_NONE ? h : list[pc_rnd(n)];
            if (!R->play(&G, m)) { ok = 0; printf("seed %d: a legal move not played\n", seed); break; }
            mine[played++] = m;
            if (!sane(P)) { ok = 0; printf("seed %d step %d: not sane\n", seed, step); break; }
            if (R->won(P)) { (*wins)++; break; }
        }
        new_game(&H, (uint32_t)seed);           /* the same moves again: the same game */
        for (int i = 0; i < played; i++)
            if (!R->play(&H, mine[i])) ok = 0;
        if (memcmp(&H.pos, &G.pos, sizeof H.pos) || H.n != G.n || H.moves != played) {
            ok = 0;
            printf("seed %d: not the same game again\n", seed);
        }
        while (sol_undo(&G)) {}
        if (memcmp(&G.pos, &start, sizeof start) || G.n || G.moves) { ok = 0; printf("seed %d: undo\n", seed); }
    }
    CHECK(ok);
}

/* games played by the hint alone, seeds 1..seeds, up to `steps` moves: the games won */
static inline int pc_hinted(const solrules *R, void (*new_game)(solrec *r, uint32_t seed), int seeds, int steps)
{
    int won = 0;
    for (int seed = 1; seed <= seeds; seed++) {
        new_game(&G, (uint32_t)seed);
        for (int step = 0; step < steps; step++) {
            solmove h = R->hint(P);
            if (h.kind == SM_NONE || !R->play(&G, h)) break;
        }
        won += R->won(P);
    }
    return won;
}

#endif
