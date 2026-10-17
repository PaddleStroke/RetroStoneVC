/*
 * Bridges' levels, made by chance and chosen for their look: src/games/hashi_levels.h. Our own puzzles, from the
 * game's own maker (islands set down one from another along the grid's lines, joined as they come, then bridges changed where the
 * reasoning is stuck until the solver finishes it at its level and not below). Each candidate:
 *   - its grade (bench.h: counting; keeping the islands in one group; a bridge tried and followed to a
 *     contradiction), the solver's passes (its work), its islands and bridges;
 *   - its look (taste): islands spread over the four quarters, the board used to its outer lines, none cramped side
 *     by side, islands answering each other across the centre (the half turn hand-set puzzles like);
 *   - near-duplicates dropped (the same islands turned or mirrored).
 * Each chapter has its makes (5 x 5 counting up to 16 x 16 needing trials); a make's candidates are ranked by
 * difficulty and cut in as many slices as picks; each pick takes the best-looking of its slice.
 * Then each chapter goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/hshgen apps/boardgames/tools/hashi_gen.c \
 *       apps/boardgames/src/games/hashi_rules.c && /tmp/hshgen > apps/boardgames/src/games/hashi_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/hashi.h"

#define CANDS 300                               /* candidates made for each make */
#define KEY (HSH_CELLS + 8)

typedef struct {
    hsh_puzzle p;
    int grade, bridges;
    long passes;
    double score, taste;
    char key[KEY], code[KEY];
} cand;

typedef struct { int n, level, count; } make;
static const make CHAPTER[HSH_CHAPTERS][3] = {
    {{5, 0, 8}, {6, 0, 8}, {7, 0, 9}},
    {{7, 1, 8}, {8, 0, 8}, {8, 1, 9}},
    {{9, 1, 8}, {10, 1, 9}, {8, 2, 8}},
    {{10, 2, 8}, {11, 1, 8}, {11, 2, 9}},
    {{12, 2, 8}, {13, 1, 8}, {13, 2, 9}},
    {{14, 2, 8}, {15, 2, 8}, {16, 2, 9}},
};

/* the islands whichever way they lie: the smallest of the eight (turned, mirrored) */
static void canon(const hsh_puzzle *p, char *key)
{
    int n = p->n;
    char best[KEY], k[KEY];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int u = sx; sx = sy; sy = u; }
                int i = p->at[sy * n + sx];
                k[y * n + x] = i < 0 ? '.' : (char)('0' + p->need[i]);
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
static cand chosen[HSH_NLEVELS];
static int nchosen;

/* the look of the islands, lower the better: spread over the four quarters, the board used to its edges (no empty
 * outer line), not cramped (islands side by side), and islands answering each other across the centre (the half
 * turn hand-set puzzles like) */
static double taste(const hsh_puzzle *p)
{
    int n = p->n, sym = 0, tight = 0, rows[HSH_MAX] = {0}, cols[HSH_MAX] = {0};
    double q[4] = {0, 0, 0, 0};
    for (int i = 0; i < p->nisl; i++) {
        int x = p->ix[i], y = p->iy[i];
        rows[y]++, cols[x]++;
        for (int k = 0; k < 4; k++) {
            double wx = n % 2 && x == n / 2 ? 0.5 : (k & 1) == (x >= n / 2);
            double wy = n % 2 && y == n / 2 ? 0.5 : (k >> 1) == (y >= n / 2);
            q[k] += wx * wy;
        }
        sym += p->at[(n - 1 - y) * n + n - 1 - x] >= 0;
        tight += x + 1 < n && p->at[y * n + x + 1] >= 0;
        tight += y + 1 < n && p->at[(y + 1) * n + x] >= 0;
    }
    double t = p->nisl, bal = 0;
    for (int k = 0; k < 4; k++) bal += q[k] > t / 4 ? q[k] - t / 4 : t / 4 - q[k];
    int bare = !rows[0] + !rows[n - 1] + !cols[0] + !cols[n - 1];
    return t ? 3.0 * bal / t + 0.7 * bare + 0.4 * tight - 1.2 * sym / t : 99;
}

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

int main(void)
{
    uint32_t seed = 1;
    for (int ch = 0; ch < HSH_CHAPTERS; ch++) {
        int first = nchosen;
        for (int m = 0; m < 3; m++) {
            const make *mk = &CHAPTER[ch][m];
            int np = 0;
            for (int tries = 0; np < CANDS && tries < 40 * CANDS; tries++) {
                cand c;
                memset(&c, 0, sizeof c);
                hsh_make(mk->n, mk->level, 0x5eed0000u + seed++ * 2654435761u, &c.p);
                c.grade = hsh_rate_work(&c.p, &c.passes);
                if (c.grade != mk->level || hsh_count(&c.p, 2) != 1) continue;
                canon(&c.p, c.key);
                int dup = seen(c.key);
                for (int i = 0; i < np && !dup; i++) dup = !strcmp(pool[i].key, c.key);
                if (dup) continue;
                hsh_encode(&c.p, c.code);
                hsh_game g;
                if (!hsh_decode(&g, c.code) || memcmp(g.p.sol, c.p.sol, (size_t)c.p.nlink)) continue;
                for (int l = 0; l < c.p.nlink; l++) c.bridges += c.p.sol[l];
                c.score = c.grade * 1000.0 + c.passes * 4.0 + c.p.nisl;
                c.taste = taste(&c.p);
                pool[np++] = c;
            }
            if (np < mk->count) {
                fprintf(stderr, "chapter %d make %d: only %d\n", ch + 1, m, np);
                return 1;
            }
            qsort(pool, (size_t)np, sizeof pool[0], by_score);
            for (int i = 0; i < mk->count; i++) {   /* the best-looking of the pick's own slice of the work */
                int a = i * np / mk->count, b = (i + 1) * np / mk->count, at = a;
                for (int j = a + 1; j < b; j++)
                    if (pool[j].taste < pool[at].taste) at = j;
                chosen[nchosen++] = pool[at];
            }
        }
        for (int i = first; i < nchosen; i++)  /* the chapter from its easiest: grade, work, size */
            chosen[i].score = chosen[i].grade * 1000.0 + chosen[i].passes * 4.0 + chosen[i].p.n * 10.0;
        qsort(chosen + first, (size_t)(nchosen - first), sizeof chosen[0], by_score);
        fprintf(stderr, "chapter %d done\n", ch + 1);
    }
    static const char *const GRADE[3] = {"counting", "one group", "a bridge tried"};
    printf("/*\n * Bridges' levels: our own, made by tools/hashi_gen.c (islands laid by chance, bridges changed until\n"
           " * the solver finishes it at its grade), six chapters of 25 from the easiest. Each: \"<n>:\" and a\n"
           " * character a point ('.' water, else an island's number; hsh_decode); in the comment the reasoning it\n"
           " * needs (its grade), the solver's passes, its islands and bridges.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const hsh_levels[HSH_NLEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        const cand *c = &chosen[i];
        printf("    \"%s\", /* %d: %dx%d, %s, %ld passes, %d islands, %d bridges */\n", c->code, i + 1, c->p.n,
               c->p.n, GRADE[c->grade], c->passes, c->p.nisl, c->bridges);
    }
    printf("};\n");
    return 0;
}
