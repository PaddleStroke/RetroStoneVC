/*
 * The Marienbad game's bot (bots.h), every variant (Marienbad or a random layout; misere or normal play). Nim is
 * solved: the rules' nim_best() finds the move that leaves a position the opponent cannot win (the nim-sum, and the
 * misere ending), so no search is needed; the levels differ in how often they use it.
 *   hard: always the perfect move; in a lost position (the nim-sum zero), one match from the longest row, so the game
 *         lasts and the opponent has every chance to slip.
 *   medium: perfect once few matches are left (7 or fewer, or a single row), otherwise perfect one move in two.
 *   easy: perfect only at the very end (5 matches or fewer, or a single row with more than one match); before that
 *         it takes from a row at random, mostly one to three matches, as a beginner does.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: a row's letter and how many to take, the rightmost ones ("d3"), played by
 * nim_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/nim.h"

/* the move for the position (its player to move), at a level: the row and how many matches; 0: none */
int nmb_choose(const nim_game *g, int level, uint64_t seed, int *row, int *take)
{
    int left = nim_left(g), rows = 0, longest = -1;
    if (!left || nim_status(g) != NIM_PLAY) return 0;
    uint64_t key = 0;
    for (int r = 0; r < g->rows; r++) {
        int c = nim_count(g->mask[r]);
        rows += c > 0;
        if (c && (longest < 0 || c > nim_count(g->mask[longest]))) longest = r;
        key = key * 257 + g->mask[r];
    }
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, key * 2 + (uint64_t)g->n));
    int perfect;
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   perfect = left <= 5 || rows == 1; break;
    case AI_MEDIUM: perfect = left <= 7 || rows == 1 || ai_rand_n(&rng, 2) != 0; break;
    default:        perfect = 1; break;
    }
    int r, mask;
    if (perfect && nim_best(g, &r, &mask)) {
        *row = r;
        *take = nim_count((uint8_t)mask);
        return 1;
    }
    if (perfect) {                              /* lost against best play: one match off the longest row */
        *row = longest;
        *take = 1;
        return 1;
    }
    int k = ai_rand_n(&rng, rows);              /* a row at random, a few matches */
    for (r = 0; r < g->rows; r++)
        if (g->mask[r] && k-- == 0) break;
    int c = nim_count(g->mask[r]), t = 1 + ai_rand_n(&rng, c < 3 ? c : 3);
    if (ai_rand_n(&rng, 6) == 0) t = 1 + ai_rand_n(&rng, c);   /* now and then: any number */
    *row = r;
    *take = t;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    nim_game g = *nim_live();                   /* the copy, at entry: nothing else is read */
    int row, take;
    if (nim_status(&g) != NIM_PLAY || g.turn != seat || cap < 4) return 0;
    if (!nmb_choose(&g, level, ai_session_seed(), &row, &take) || ai_cancelled()) return 0;
    snprintf(move, (size_t)cap, "%c%d", 'a' + row, take);
    return 1;
}

static const bot nim_bot = {.think = think, .play = nim_show};
REGISTER_BOT(nim, &nim_bot)
