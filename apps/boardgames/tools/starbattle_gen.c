/*
 * Star Battle's levels, made by chance and chosen for their look: src/games/starbattle_levels.h. Our own
 * puzzles, from the game's own maker (the stars placed by chance, regions grown round them, cells passed between
 * regions until the reasoning grader finishes it at its level: one solution). Each candidate:
 *   - its grade (the hardest reasoning the grader needed, in tiers: the cells round a star, full lines and regions;
 *     a star's room, a region held in a line; two or three regions held in as many lines, a star followed to a
 *     contradiction) and its steps, weighed by their tier (its effort);
 *   - its look (taste): region sizes near each other (no one-cell region, none swallowing the board), compact
 *     shapes (outlines near a square's: no snakes or ragged arms);
 *   - near-duplicates dropped (the same regions turned or mirrored).
 * Each chapter has two makes (a size, its stars and a level); a make's candidates are ranked by effort, cut in as
 * many slices as picks, and each pick takes the best-looking of its slice (the last chapter over its harder part).
 * Then each chapter goes from its smallest and easiest to its biggest and hardest: 5 x 5 with one star a line first,
 * 10 x 10 with two hard last.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/sbxgen apps/boardgames/tools/starbattle_gen.c \
 *       apps/boardgames/src/games/starbattle_rules.c && /tmp/sbxgen > apps/boardgames/src/games/starbattle_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/starbattle.h"

#define CANDS 120                               /* candidates made for each make */
#define CODE (SBX_CELLS + 8)

typedef struct {
    sbx_puz p;
    int grade, steps;
    double score, taste;
    char key[CODE];
} cand;

typedef struct { int n, k, level, count, lo; } make;   /* lo: the picks from that part (in a hundred) of the pool up */
static const make CHAPTER[SBX_CHAPTERS][2] = {
    {{5, 1, SBX_EASY, 12, 0}, {6, 1, SBX_EASY, 13, 0}},
    {{6, 1, SBX_MEDIUM, 10, 0}, {7, 1, SBX_EASY, 15, 0}},
    {{7, 1, SBX_MEDIUM, 12, 0}, {8, 1, SBX_MEDIUM, 13, 0}},
    {{8, 1, SBX_HARD, 12, 0}, {9, 1, SBX_HARD, 13, 0}},
    {{10, 2, SBX_EASY, 12, 0}, {10, 2, SBX_MEDIUM, 13, 0}},
    {{10, 2, SBX_MEDIUM, 10, 50}, {10, 2, SBX_HARD, 15, 25}},
};

/* the regions whichever way they lie: the smallest code of their eight (turned, mirrored) */
static void canon(const sbx_puz *p, char *key)
{
    int n = p->n;
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        sbx_puz q = *p;
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int s = sx; sx = sy; sy = s; }
                q.region[y * n + x] = p->region[sy * n + sx];
            }
        int name[SBX_MAX], next = 0;                /* the regions named again in reading order */
        memset(name, 0xff, sizeof name);
        for (int c = 0; c < n * n; c++) {
            if (name[q.region[c]] < 0) name[q.region[c]] = next++;
            q.region[c] = (uint8_t)name[q.region[c]];
        }
        char k[CODE];
        sbx_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[SBX_STAGES];
static int nchosen;
static uint8_t sol[SBX_CELLS];

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* the look of the regions, lower the better: sizes near each other (no region of one cell, none swallowing the
 * board), compact shapes (a region's outline against a square's of its size: no snakes, no ragged arms) */
static double taste(const sbx_puz *p)
{
    int n = p->n, size[SBX_MAX] = {0}, edge[SBX_MAX] = {0};
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int r = p->region[y * n + x];
            size[r]++;
            edge[r] += (x == 0 || p->region[y * n + x - 1] != r) + (x == n - 1 || p->region[y * n + x + 1] != r) +
                       (y == 0 || p->region[(y - 1) * n + x] != r) + (y == n - 1 || p->region[(y + 1) * n + x] != r);
        }
    double mean = (double)n, spread = 0, shape = 0, t = 0;
    for (int r = 0; r < n; r++) {
        double d = (size[r] - mean) / mean, sq = 16.0 * size[r];   /* a square's outline, squared */
        spread += d * d;
        shape += (double)edge[r] * edge[r] / sq;
        if (size[r] == 1) t += 1.0;                     /* a one-cell region gives its star away */
        if (size[r] > 2.2 * mean) t += size[r] / mean - 2.2;
    }
    return 3.0 * spread / n + 2.0 * (shape / n - 1) + t;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
        cand *c = &pool[k];
        sbx_make(m->n, m->k, m->level, seed, &c->p, sol);
        c->grade = sbx_effort(&c->p, &c->steps);
        if (c->grade != m->level || sbx_count(&c->p, 2, NULL) != 1) continue;
        canon(&c->p, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = c->steps + 0.01 * (seed % 7);
        c->taste = taste(&c->p);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < SBX_CHAPTERS; ch++) {
        int first = nchosen;
        for (int mi = 0; mi < 2; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d, %d a line, level %d): %d candidates, effort %d..%d\n", ch + 1,
                    mi + 1, m->n, m->n, m->k, m->level, k, k ? pool[0].steps : 0, k ? pool[k - 1].steps : 0);
            for (int i = 0; i < m->count; i++) {
                int lo = m->lo * (k - 1) / 100, at = -1;   /* the best-looking of the pick's own slice */
                int a = lo + i * (k - 1 - lo) / m->count, b = lo + (i + 1) * (k - 1 - lo) / m->count;
                for (int j = a; j <= b && j < k; j++)
                    if (!seen(pool[j].key) && (at < 0 || pool[j].taste < pool[at].taste)) at = j;
                for (int j = b + 1; at < 0 && j < k; j++)
                    if (!seen(pool[j].key)) at = j;
                if (at < 0) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                double ka = a->p.k * 1e7 + a->p.n * 1e6 + a->grade * 1e5 + a->score;
                double kb = b->p.k * 1e7 + b->p.n * 1e6 + b->grade * 1e5 + b->score;
                if (ka <= kb) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n"
           " * Star Battle's levels: our own, made by tools/starbattle_gen.c (the game's maker, the reasoning grader's\n"
           " * grade and effort), six chapters of 25 from the easiest. Each: \"<n>/<k>:\" (the side, the stars a line)\n"
           " * and a letter a cell, row by row, its region (starbattle.h, sbx_decode); in the comment the reasoning it\n"
           " * needs (0 easy, 1 medium, 2 hard) and the grader's steps weighed by their tier.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const sbx_levels[SBX_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[CODE];
        sbx_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, %d a line, grade %d, effort %d */\n", code, i + 1, chosen[i].p.n,
               chosen[i].p.n, chosen[i].p.k, chosen[i].grade, chosen[i].steps);
    }
    printf("};\n");
    return nchosen == SBX_STAGES ? 0 : 1;
}
