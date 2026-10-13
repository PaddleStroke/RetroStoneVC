/*
 * Loop's levels, made by chance and chosen: src/games/loop_levels.h. Our own puzzles, from the game's own maker (a
 * region of cells grown by chance, its border the loop, every number counted, then numbers taken away while the
 * solver still finishes it at its level). Each candidate:
 *   - its grade (bench.h: a number's sides, a dot's two lines, no small loop; a number with the dots at its corners;
 *     a side tried and followed to a contradiction), the solver's passes (its work), its numbers;
 *   - one solution: the grader proves it (and up to 10 x 10 a plain search finds no other);
 *   - near-duplicates dropped (the same numbers turned or mirrored).
 * Each chapter has its makes (5 x 5 with the plainest reasoning up to 15 x 15 needing trials); a make's candidates
 * are ranked by difficulty and picked evenly over them. Then each chapter goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/slkgen apps/boardgames/tools/loop_gen.c \
 *       apps/boardgames/src/games/loop_rules.c && /tmp/slkgen > apps/boardgames/src/games/loop_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/loop.h"

#define CANDS 36                                /* candidates made for each make */
#define KEY (SLK_CELLS + 8)

typedef struct {
    slk_puzzle p;
    int grade, numbers, length;
    long passes;
    double score;
    char key[KEY], code[SLK_CODE];
} cand;

typedef struct { int n, level, count; } make;
static const make CHAPTER[SLK_CHAPTERS][3] = {
    {{5, 0, 13}, {6, 0, 12}, {0, 0, 0}},
    {{6, 1, 8}, {7, 0, 8}, {7, 1, 9}},
    {{7, 2, 8}, {8, 1, 8}, {9, 1, 9}},
    {{8, 2, 8}, {10, 1, 8}, {10, 2, 9}},
    {{11, 2, 8}, {12, 1, 8}, {12, 2, 9}},
    {{13, 2, 8}, {14, 2, 8}, {15, 2, 9}},
};

/* the numbers whichever way they lie: the smallest of the eight (turned, mirrored) */
static void canon(const slk_puzzle *p, char *key)
{
    int n = p->n;
    char best[KEY], k[KEY];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int u = sx; sx = sy; sy = u; }
                int v = p->clue[sy * n + sx];
                k[y * n + x] = v < 0 ? '.' : (char)('0' + v);
            }
        k[n * n] = 0;
        if (!best[0] || strcmp(k, best) < 0) strcpy(best, k);
    }
    snprintf(key, KEY, "%d:%s", n, best);
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[SLK_NLEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

int main(void)
{
    uint32_t seed = 1;
    for (int ch = 0; ch < SLK_CHAPTERS; ch++) {
        int first = nchosen;
        for (int m = 0; m < 3; m++) {
            const make *mk = &CHAPTER[ch][m];
            if (!mk->count) continue;
            int np = 0;
            for (int tries = 0; np < CANDS && tries < 40 * CANDS; tries++) {
                cand c;
                memset(&c, 0, sizeof c);
                slk_make(mk->n, mk->level, 0x100b0000u + seed++ * 2654435761u, &c.p);
                c.grade = slk_rate_work(&c.p, &c.passes);
                if (c.grade != mk->level || (mk->n <= 10 && slk_count(&c.p, 2) != 1)) continue;
                canon(&c.p, c.key);
                int dup = seen(c.key);
                for (int i = 0; i < np && !dup; i++) dup = !strcmp(pool[i].key, c.key);
                if (dup) continue;
                slk_encode(&c.p, c.code);
                slk_game g;
                if (!slk_decode(&g, c.code) || memcmp(g.p.sol, c.p.sol, (size_t)slk_sides(c.p.n))) continue;
                for (int i = 0; i < c.p.n * c.p.n; i++) c.numbers += c.p.clue[i] >= 0;
                for (int s = 0; s < slk_sides(c.p.n); s++) c.length += c.p.sol[s];
                c.score = c.grade * 1000.0 + c.passes * 3.0 - c.numbers * 0.5;
                pool[np++] = c;
            }
            if (np < mk->count) {
                fprintf(stderr, "chapter %d make %d: only %d\n", ch + 1, m, np);
                return 1;
            }
            fprintf(stderr, "chapter %d make %d: %d candidates\n", ch + 1, m, np);
            qsort(pool, (size_t)np, sizeof pool[0], by_score);
            for (int i = 0; i < mk->count; i++) chosen[nchosen++] = pool[i * (np - 1) / (mk->count - 1)];
        }
        for (int i = first; i < nchosen; i++)  /* the chapter from its easiest: grade, work, size */
            chosen[i].score = chosen[i].grade * 1000.0 + chosen[i].passes * 3.0 + chosen[i].p.n * 10.0;
        qsort(chosen + first, (size_t)(nchosen - first), sizeof chosen[0], by_score);
        fprintf(stderr, "chapter %d done\n", ch + 1);
    }
    static const char *const GRADE[3] = {"sides and dots", "corners", "a side tried"};
    printf("/*\n * Loop's levels: our own, made by tools/loop_gen.c (a region grown by chance, its border the loop,\n"
           " * numbers taken away while the solver finishes it at its grade), six chapters of 25 from the easiest.\n"
           " * Each: \"<n>:\", a character a cell ('.' no number), ':' and the loop's sides (slk_decode); in the\n"
           " * comment the reasoning it needs (its grade), the solver's passes, its numbers and the loop's length.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const slk_levels[SLK_NLEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        const cand *c = &chosen[i];
        printf("    \"%s\", /* %d: %dx%d, %s, %ld passes, %d numbers, %d long */\n", c->code, i + 1, c->p.n, c->p.n,
               GRADE[c->grade], c->passes, c->numbers, c->length);
    }
    printf("};\n");
    return nchosen != SLK_NLEVELS;
}
