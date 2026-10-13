/*
 * What the patience games' level makers share (tools/<id>_gen.c: Klondike, FreeCell, Spider, Pyramid, TriPeaks,
 * Golf, Yukon, Canfield). Each candidate deal is a pack shuffled from a fixed seed, so a run always makes the same
 * levels:
 *   - proved winnable by the game's search (sollevels.h: every card known, up to a number of positions);
 *   - graded by plain games: `tries` games played with the game's own candidate moves, the better ones more often
 *     (each move: the first candidate one time in two, the next one time in four...), never twice through the same
 *     position, seeing only what a player sees. The share of them won is the deal's ease; the search's effort breaks
 *     ties (a deal won by many plain games is easy to see through; one won by none needs the right plan). Golf and
 *     TriPeaks, whose plain games almost never win, are graded by how forgiving the deal is instead (sg_forgiving).
 *   - chosen evenly along that grade, easiest first: a group of levels for each variant (Spider's suits...), in order.
 * The header written: the levels as card letters (sollevels.h), each with its grade in a comment.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef SOLGEN_H
#define SOLGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/sollevels.h"

typedef struct {
    const char *name, *id, *prefix, *upper;     /* "Klondike", "klondike", "kl", "KL" */
    const solsearch *S;
    int ncards;                                 /* the pack: 52, or 104 */
    void (*pack)(int variant, uint8_t *deck);   /* the pack in order (Spider: its suits) */
    void (*deal)(solrec *r, int variant, const uint8_t *deck);
    long budget;                                /* the search's positions at most */
    int tries;                                  /* the plain games played on each deal */
    int maxlen;                                 /* the longest win the search looks for (player's moves) */
    int forgiving;                              /* graded by the winning moves along the way instead (sg_forgiving) */
    int keep_win;                               /* each level keeps the search's win (its tests play it, no search) */
} sggame;

typedef struct {
    int variant, count, cands;                  /* the levels of this variant, chosen from this many deals */
    double ease_min, ease_max;                  /* the grades kept (the share of plain games won) */
} sggroup;

typedef struct {
    uint8_t deck[104];
    int variant, moves;
    long nodes;
    double ease;
    char *win;                                  /* the search's win (keep_win) */
} sgcand;

/* the pack most games deal from */
static inline void sg_pack52(int variant, uint8_t *deck)
{
    (void)variant;
    cards_pack(PACK_52, deck);
}

static uint32_t sg_s;
static uint32_t sg_rand(void)
{
    sg_s ^= sg_s << 13;
    sg_s ^= sg_s >> 17;
    sg_s ^= sg_s << 5;
    return sg_s;
}

/* plain games from r: how many of `tries` are won */
static int sg_plain(const sggame *g, const solrec *r, int tries, uint32_t seed)
{
    enum { SEEN = 1 << 14 };
    static uint64_t seen[SEEN];
    static solmove list[SOLVE_BRANCH];
    static solrec w;
    const solsearch *S = g->S;
    int wins = 0;
    sg_s = seed ? seed : 1;
    for (int t = 0; t < tries; t++) {
        memset(seen, 0, sizeof seen);
        w = *r;
        for (int step = 0; step < 1500 && !S->R->won(&w.pos); step++) {
            int n = S->cands(&w.pos, list), played = 0;
            int order[SOLVE_BRANCH];
            for (int i = 0; i < n; i++) order[i] = i;
            /* the first candidate one time in two, the next one time in four...: a pick, then the rest in order */
            int k = 0;
            while (k < n - 1 && (sg_rand() & 1)) k++;
            if (k) {
                int x = order[k];
                memmove(order + 1, order, (size_t)k * sizeof *order);
                order[0] = x;
            }
            for (int j = 0; j < n && !played; j++) {
                solpos before = w.pos;
                int len = w.n;
                if (!S->R->play(&w, list[order[j]])) continue;
                uint64_t key = S->key ? S->key(&w.pos) : sol_key(&w.pos);
                uint32_t i = (uint32_t)key & (SEEN - 1);
                int again = 0;
                for (int probe = 0; probe < SEEN; probe++, i = (i + 1) & (SEEN - 1)) {
                    if (seen[i] == key) { again = 1; break; }
                    if (!seen[i]) { seen[i] = key; break; }
                }
                if (again) {                    /* been there: another move */
                    w.pos = before;
                    w.n = len;
                    continue;
                }
                played = 1;
            }
            if (!played) break;
        }
        wins += S->R->won(&w.pos);
    }
    return wins;
}

/* how forgiving a deal is (games whose plain games almost never win: Golf, TriPeaks): along a winning way, at each
 * position offering a choice, the share of its candidate moves that still win (each checked by the search, a tenth of
 * its budget); their mean. Many winning moves: easy; one narrow path: hard. */
static double sg_forgiving(const sggame *g, const solrec *r)
{
    static solrec w, q;
    static solmove list[SOLVE_BRANCH], path[4000];
    const solsearch *S = g->S;
    double sum = 0;
    int points = 0;
    w = *r;
    for (int step = 0; step < 400 && !S->R->won(&w.pos); step++) {
        int n = S->cands(&w.pos, list), wins = 0, first = -1;
        for (int j = 0; j < n; j++) {
            q = w;
            if (!S->R->play(&q, list[j])) continue;
            if (S->R->won(&q.pos) || sol_solve(S, &q, g->budget / 10, path, g->maxlen, NULL) >= 0) {
                wins++;
                if (first < 0) first = j;
            }
        }
        if (first < 0) break;                   /* (the search gave up here) */
        if (n > 1) sum += (double)wins / n, points++;
        S->R->play(&w, list[first]);
    }
    return points ? sum / points : 1;
}

static int sg_by_grade(const void *a, const void *b)
{
    const sgcand *x = a, *y = b;
    if (x->ease != y->ease) return x->ease > y->ease ? -1 : 1;
    return x->nodes < y->nodes ? -1 : x->nodes > y->nodes;
}

/* the levels: each group's candidates proved and graded, `count` of them chosen evenly from the easiest to the
 * hardest kept; the header on stdout, the progress on stderr */
static void sg_run(const sggame *g, const sggroup *groups, int ngroups, int chapters, const char *about)
{
    int total = 0;
    for (int k = 0; k < ngroups; k++) total += groups[k].count;
    printf("/*\n * %s's levels: our own deals, made by tools/%s_gen.c (tools/solgen.h).\n"
           " * %d levels in %d chapters, from the easiest: %s.\n"
           " * Each: its variant and its pack in the order dealt (sollevels.h); in the comment, the player's moves of\n"
           " * the search's win and %s.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n", g->name, g->id, total, chapters, about,
           g->forgiving ? "the share of winning moves along the way" : "the share of plain games won");
    printf("#define %s_LEVELS %d\n#define %s_CHAPTERS %d\n\nstatic const sollevel %s_levels[%s_LEVELS] = {\n", g->upper,
           total, g->upper, chapters, g->prefix, g->upper);
    static solrec r;
    static solmove path[4000];
    int number = 0;
    for (int k = 0; k < ngroups; k++) {
        const sggroup *G = &groups[k];
        sgcand *c = malloc((size_t)G->cands * sizeof *c);
        int nc = 0;
        time_t t0 = time(NULL);
        for (int i = 0; i < G->cands; i++) {
            uint8_t deck[104];
            g->pack(G->variant, deck);
            cards_shuffle(deck, g->ncards, 0x51e0u + 7919u * (uint32_t)(i + 1) + 104729u * (uint32_t)G->variant);
            g->deal(&r, G->variant, deck);
            long nodes;
            int n = sol_solve(g->S, &r, g->budget, path, g->maxlen, &nodes);
            if (getenv("SG_DEBUG")) fprintf(stderr, "  deal %d: %d moves, %ld positions\n", i, n, nodes);
            if (n <= 0) continue;
            char *win = NULL;
            if (g->keep_win && (win = malloc((size_t)3 * n + 1))) sol_win_write(path, n, win);
            double ease = g->forgiving ? sg_forgiving(g, &r)
                                       : (double)sg_plain(g, &r, g->tries, 977u * (uint32_t)(i + 1)) / g->tries;
            if (ease < G->ease_min || ease > G->ease_max) { free(win); continue; }
            memcpy(c[nc].deck, deck, sizeof deck);
            c[nc].variant = G->variant;
            c[nc].moves = n;
            c[nc].nodes = nodes;
            c[nc].ease = ease;
            c[nc].win = win;
            nc++;
        }
        fprintf(stderr, "%s variant %d: %d of %d deals kept (%lds)\n", g->name, G->variant, nc, G->cands,
                (long)(time(NULL) - t0));
        if (nc < G->count) {                    /* each level a deal of its own: more deals to try */
            fprintf(stderr, "%s variant %d: not enough deals (%d for %d levels)\n", g->name, G->variant, nc, G->count);
            exit(1);
        }
        qsort(c, (size_t)nc, sizeof *c, sg_by_grade);
        for (int j = 0; j < G->count; j++) {
            int i = nc <= 1 ? 0 : (int)((double)j * (nc - 1) / (G->count > 1 ? G->count - 1 : 1) + 0.5);
            if (i >= nc) { fprintf(stderr, "not enough deals\n"); exit(1); }
            char s[128];
            sol_cards_write(c[i].deck, g->ncards, s);
            printf("    {%d, \"%s\", ", c[i].variant, s);
            if (c[i].win) printf("\n     \"%s\"},", c[i].win);
            else printf("NULL},");
            printf(" /* %d: %d moves, %d%% */\n", ++number, c[i].moves, (int)(100 * c[i].ease + 0.5));
        }
        for (int i = 0; i < nc; i++) free(c[i].win);
        free(c);
    }
    printf("};\n");
}

#endif
