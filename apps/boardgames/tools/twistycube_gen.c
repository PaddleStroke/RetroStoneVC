/*
 * Twisty Cube's levels: src/games/twistycube_levels.h. Our own: scrambles made by the rules' own maker (tcb_scramble)
 * from fixed seeds. Six chapters of 25, the scrambles growing longer: one move, two, three... then full scrambles.
 * Each chapter is a list of groups (a size, a scramble's length, how many); a group short of puzzles leaves the rest
 * to the next one. A scramble is kept when a search finds no shorter way to solve it (its par is then its length;
 * past the search's budget it is kept as it is: its length is the best way known) and when the same puzzle was not
 * met before: the same stickers up to a turn or a mirror of the whole cube and a change of colours. Within a group the
 * scrambles with fewer inner slices come first. Deterministic (fixed seeds).
 *   gcc -O2 -std=c11 -Iapps/boardgames/src -o /tmp/tcbgen apps/boardgames/tools/twistycube_gen.c && \
 *       /tmp/tcbgen > apps/boardgames/src/games/twistycube_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TCB_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/twistycube_rules.c"

typedef struct { int n, depth, count; } group;   /* depth 0: a full scramble */
static const group CHAPTER[TCB_CHAPTERS][6] = {
    {{3, 1, 2}, {2, 1, 1}, {4, 1, 2}, {2, 2, 4}, {3, 2, 12}, {4, 2, 4}},
    {{2, 3, 5}, {3, 3, 14}, {4, 3, 6}},
    {{2, 4, 5}, {3, 4, 10}, {4, 4, 4}, {3, 5, 6}},
    {{2, 5, 4}, {3, 5, 6}, {4, 5, 5}, {3, 6, 10}},
    {{2, 6, 4}, {4, 6, 3}, {3, 7, 8}, {3, 8, 6}, {3, 10, 4}},
    {{2, 0, 8}, {3, 0, 12}, {4, 0, 5}},
};
#define PER (TCB_LEVELS / TCB_CHAPTERS)
#define BUDGET 40000000L
#define TRIES 3000

typedef struct {
    int n, par, len, inner;
    tcb_move mv[TCB_MAXSCR];
    uint8_t key[TCB_STICKERS];
} level;
static level L[TCB_LEVELS];
static int nl, unchecked;

/* ---- is there a shorter way? a search over the moves in a fixed order (along one axis: the layers rising) */
static long nodes;
static int dfs(const tcb_cube *c, int left, int la, int ll, int lb, int lk)
{
    if (tcb_solved(c)) return 1;
    if (left == 0) return 0;
    if (++nodes > BUDGET) return -1;
    for (int a = 0; a < 3; a++)
        for (int l = 0; l < c->n; l++)
            for (int b = 0; b < 2; b++) {
                if (a == la && (l < ll || (l == ll && (b != lb || lk >= 2)))) continue;
                tcb_cube d = *c;
                tcb_turn(&d, tcb_mv(a, l, b));
                int r = dfs(&d, left - 1, a, l, b, a == la && l == ll ? lk + 1 : 1);
                if (r) return r;
            }
    return 0;
}

/* ---- the same puzzle: the stickers under the 48 turns and mirrors of the cube, the colours named in order ------ */
static void canon(const tcb_cube *c, uint8_t *key)
{
    int n = c->n, k = 6 * n * n, first = 1;
    static const int P[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
        for (int sg = 0; sg < 8; sg++) {
            uint8_t o[TCB_STICKERS], name[6], next = 0;
            for (int s = 0; s < k; s++) {
                int p[3], nr[3], q[3], m[3];
                tcb_place(n, s, p, nr);
                for (int i = 0; i < 3; i++) {
                    int f = sg >> i & 1 ? -1 : 1;
                    q[i] = f * p[P[pi][i]];
                    m[i] = f * nr[P[pi][i]];
                }
                o[tcb_sticker_at(n, q, m)] = c->c[s];
            }
            memset(name, 0xff, sizeof name);
            for (int s = 0; s < k; s++) {
                if (name[o[s]] == 0xff) name[o[s]] = next++;
                o[s] = name[o[s]];
            }
            if (first || memcmp(o, key, (size_t)k) < 0) memcpy(key, o, (size_t)k);
            first = 0;
        }
}

static int cmp_inner(const void *a, const void *b)
{
    const level *x = a, *y = b;
    return x->inner - y->inner;
}

static int make_group(const group *g, int want, uint32_t seed)
{
    int got = 0, from = nl;
    for (int t = 0; t < TRIES && got < want; t++) {
        uint32_t r = pr_seed(seed + (uint32_t)t * 7919u);
        level v = {.n = g->n};
        v.len = tcb_scramble(&r, g->n, g->depth ? g->depth : tcb_full_depth(g->n), v.mv);
        tcb_cube c;
        tcb_init(&c, g->n);
        for (int i = 0; i < v.len; i++) {
            tcb_turn(&c, v.mv[i]);
            int l = tcb_layer(v.mv[i]);
            v.inner += l > 0 && l < g->n - 1;
        }
        if (tcb_solved(&c)) continue;
        canon(&c, v.key);
        int dup = 0;
        for (int i = 0; i < nl && !dup; i++) dup = L[i].n == v.n && !memcmp(L[i].key, v.key, (size_t)(6 * v.n * v.n));
        if (dup) continue;
        int ok = 1;
        if (g->depth) {
            nodes = 0;
            int r2 = dfs(&c, g->depth - 1, -1, -1, 0, 0);
            if (r2 == 1) ok = 0;
            else if (r2 < 0) unchecked++;
            v.par = g->depth;
        }
        if (!ok) continue;
        L[nl++] = v;
        got++;
    }
    qsort(L + from, (size_t)got, sizeof L[0], cmp_inner);
    return got;
}

int main(void)
{
    for (int ch = 0; ch < TCB_CHAPTERS; ch++) {
        int left = 0, start = nl;
        for (int gi = 0; gi < 6 && CHAPTER[ch][gi].count; gi++) {
            const group *g = &CHAPTER[ch][gi];
            int want = g->count + left, last = gi == 5 || !CHAPTER[ch][gi + 1].count;
            int got = make_group(g, want, 1000003u * (uint32_t)(ch + 1) + 7777u * (uint32_t)gi);
            left = want - got;
            fprintf(stderr, "chapter %d: %d x %d, %d moves: %d of %d\n", ch + 1, g->n, g->n, g->depth, got, want);
            if (last && left) { fprintf(stderr, "chapter %d short by %d\n", ch + 1, left); return 1; }
        }
        if (nl - start != PER) { fprintf(stderr, "chapter %d: %d levels\n", ch + 1, nl - start); return 1; }
    }
    fprintf(stderr, "%d levels, %d past the search's budget\n", nl, unchecked);
    printf("/*\n * Twisty Cube's levels, made by tools/twistycube_gen.c (do not edit): %d levels, %d chapters of %d.\n"
           " * Each level: its size n, its par (the scramble's length; 0: a full scramble, no par), the scramble's length,\n"
           " * then its moves (twistycube.h's bytes: axis << 3 | layer << 1 | back).\n */\n",
           nl, TCB_CHAPTERS, PER);
    printf("static const uint16_t TCB_LEVEL_AT[TCB_LEVELS] = {");
    int at = 0;
    for (int i = 0; i < nl; i++) {
        printf("%s%d,", i % 16 ? " " : "\n    ", at);
        at += 3 + L[i].len;
    }
    printf("\n};\nstatic const uint8_t TCB_LEVEL_DATA[] = {\n");
    for (int i = 0; i < nl; i++) {
        printf("    %d, %d, %d,", L[i].n, L[i].par, L[i].len);
        for (int k = 0; k < L[i].len; k++) printf(" %d,", L[i].mv[k]);
        printf("\n");
    }
    printf("};\n");
    return 0;
}
