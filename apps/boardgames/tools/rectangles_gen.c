/*
 * Rectangles' levels, made and chosen with taste: src/games/rectangles_levels.h. Our own puzzles, made here as a
 * setter makes them: the grid cut into rectangles that turn into one another half round (the givens' rotational
 * symmetry of the genre's classic puzzles), or a quarter round for a chapter's last level (its showpiece). A number in
 * each rectangle, at cells that turn into one another too; numbers moved within their rectangles (a whole turning set
 * at once) until the game's reasoning grader finishes it at its level: one solution (counted apart too). Each
 * candidate:
 *   - its grade (the hardest reasoning the grader needed) and its steps, weighed by their level (its effort);
 *   - its look: numbers touching side by side, ones, too many twos, the numbers' spread over the four quarters, runs
 *     of three rows or columns without a number, few different areas, all count against it;
 *   - near-duplicates dropped (the same numbers turned or mirrored).
 * Each chapter has two makes (a size and a level) and a showpiece; a make's candidates are ranked by effort and
 * picked evenly over them (the last two chapters over its harder part), each pick the best looking of its
 * neighbours. Each chapter goes from its smallest and easiest to its biggest and hardest, its showpiece last.
 * Deterministic (fixed seeds): run it again after a change and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/rctgen apps/boardgames/tools/rectangles_gen.c \
 *       apps/boardgames/src/games/rectangles_rules.c && /tmp/rctgen > apps/boardgames/src/games/rectangles_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/rectangles.h"
#include "games/puzrand.h"

#define CANDS 48                                /* candidates made for each make */
#define CODE (RCT_CELLS + 8)
#define WINDOW 3                                /* a pick: the best looking within this many of its place */

typedef struct {
    rct_puz p;
    int grade, steps, clues;
    double score, look;
    char key[CODE];
} cand;

typedef struct { int n, level, count, lo, sym; } make;   /* lo: picks from that part (in a hundred) of the pool up;
                                                          * sym: 2 half-turn symmetry, 4 quarter-turn */
static const make CHAPTER[RCT_CHAPTERS][3] = {
    {{5, RCT_EASY, 12, 0, 2}, {6, RCT_EASY, 12, 0, 2}, {6, RCT_EASY, 1, 0, 4}},
    {{7, RCT_EASY, 10, 0, 2}, {7, RCT_MEDIUM, 14, 0, 2}, {7, RCT_MEDIUM, 1, 0, 4}},
    {{8, RCT_MEDIUM, 12, 0, 2}, {9, RCT_MEDIUM, 12, 0, 2}, {9, RCT_MEDIUM, 1, 0, 4}},
    {{9, RCT_HARD, 12, 15, 2}, {10, RCT_HARD, 12, 15, 2}, {10, RCT_HARD, 1, 0, 4}},
    {{11, RCT_HARD, 12, 0, 2}, {12, RCT_HARD, 12, 0, 2}, {12, RCT_HARD, 1, 0, 4}},
    {{13, RCT_HARD, 12, 30, 2}, {14, RCT_HARD, 12, 30, 2}, {14, RCT_HARD, 1, 0, 4}},
};

static int N;
static int rot_cell(int c) { int x = c % N, y = c / N; return x * N + N - 1 - y; }   /* a quarter turn */
static uint32_t rot_rect(uint32_t r)
{
    int x, y, w, h;
    rct_unpack(r, &x, &y, &w, &h);
    return rct_pack(N - y - h, x, h, w);
}

/* a rectangle's set: it and its turns (by a half or a quarter); how many */
static int orbit(uint32_t r, int sym, uint32_t *orb)
{
    int k = 0, step = 4 / sym;
    orb[k++] = r;
    for (;;) {
        uint32_t t = orb[k - 1];
        for (int s = 0; s < step; s++) t = rot_rect(t);
        if (t == r || k == 4) break;
        orb[k++] = t;
    }
    return k;
}

/* the grid cut into rectangles whose turns are rectangles of the cut too; 0: stuck */
static int cut(int n, int sym, int maxa, uint32_t *rng, uint32_t *own)
{
    N = n;
    memset(own, 0, sizeof own[0] * RCT_CELLS);
    for (int c = 0; c < n * n; c++) {
        if (own[c]) continue;
        int x = c % n, y = c / n;
        static uint32_t opt[RCT_CELLS * 4];
        static int wt[RCT_CELLS * 4];
        int no = 0, total = 0;
        for (int w = 1; x + w <= n; w++)
            for (int h = 1; y + h <= n && w * h <= maxa; h++) {
                int free = 1;
                for (int j = y; j < y + h && free; j++)
                    for (int i = x; i < x + w && free; i++) free = !own[j * n + i];
                if (!free) break;
                uint32_t orb[4];
                int k = orbit(rct_pack(x, y, w, h), sym, orb), ok = 1;
                static uint8_t mark[RCT_CELLS];
                memset(mark, 0, sizeof mark);
                for (int i = 0; i < k && ok; i++) {   /* its turns: each free, and apart from the others */
                    int rx, ry, rw, rh;
                    rct_unpack(orb[i], &rx, &ry, &rw, &rh);
                    for (int j = ry; j < ry + rh && ok; j++)
                        for (int q = rx; q < rx + rw && ok; q++) {
                            ok = !own[j * n + q] && !mark[j * n + q];
                            mark[j * n + q] = 1;
                        }
                }
                if (!ok) continue;
                if (k < sym && !(n % 2 && w % 2 && h % 2)) continue;   /* turning into itself: about the centre */
                int a = w * h, lo = w < h ? w : h, hi = w < h ? h : w;
                int v = a == 1 ? 1 : (4 * lo - hi + 6) * (a <= 2 ? 3 : a <= 9 ? 6 : 4);
                if (v < 1) v = 1;
                opt[no] = rct_pack(x, y, w, h), wt[no] = v, total += v, no++;
            }
        if (!no) return 0;
        int pick = pr_below(rng, total), i = 0;
        while (pick >= wt[i]) pick -= wt[i++];
        uint32_t orb[4];
        int k = orbit(opt[i], sym, orb);
        for (int o = 0; o < k; o++) {
            int rx, ry, rw, rh;
            rct_unpack(orb[o], &rx, &ry, &rw, &rh);
            for (int j = ry; j < ry + rh; j++)
                for (int q = rx; q < rx + rw; q++) own[j * n + q] = orb[o];
        }
    }
    return 1;
}

/* the numbers of a rectangle's set put at a random cell of it and that cell's turns */
static void place(rct_puz *p, const uint32_t *own, int c, int sym, uint32_t *rng)
{
    int n = p->n, step = 4 / sym, x, y, w, h;
    uint32_t orb[4];
    int k = orbit(own[c], sym, orb);
    for (int o = 0; o < k; o++) {               /* the set's old numbers away */
        int rx, ry, rw, rh;
        rct_unpack(orb[o], &rx, &ry, &rw, &rh);
        for (int j = ry; j < ry + rh; j++)
            for (int q = rx; q < rx + rw; q++) p->clue[j * n + q] = 0;
    }
    rct_unpack(own[c], &x, &y, &w, &h);
    int at = k < sym ? (n * n) / 2 : (y + pr_below(rng, h)) * n + x + pr_below(rng, w);
    for (int o = 0; o < sym; o++) {
        p->clue[at] = (uint8_t)(w * h);
        for (int s = 0; s < step; s++) at = rot_cell(at);
    }
}

static double look(const rct_puz *p)
{
    int n = p->n, adj = 0, ones = 0, twos = 0, clues = 0, quad[4] = {0}, used[64] = {0}, kinds = 0, empty = 0;
    for (int c = 0; c < n * n; c++) {
        int v = p->clue[c], x = c % n, y = c / n;
        if (!v) continue;
        clues++;
        ones += v == 1, twos += v == 2;
        if (!used[v]) used[v] = 1, kinds++;
        if (x + 1 < n && p->clue[c + 1]) adj++;
        if (y + 1 < n && p->clue[c + n]) adj++;
        if (2 * x + 1 != n && 2 * y + 1 != n) quad[(2 * x >= n) + 2 * (2 * y >= n)]++;
    }
    for (int d = 0; d < 2; d++) {               /* runs of three rows (columns) with no number */
        int run = 0;
        for (int i = 0; i < n; i++) {
            int any = 0;
            for (int j = 0; j < n; j++) any |= p->clue[d ? j * n + i : i * n + j];
            run = any ? 0 : run + 1;
            if (run >= 3) empty++;
        }
    }
    int qlo = quad[0], qhi = quad[0];
    for (int i = 1; i < 4; i++) qlo = quad[i] < qlo ? quad[i] : qlo, qhi = quad[i] > qhi ? quad[i] : qhi;
    double tw = clues ? (double)twos / clues : 0;
    return -1.5 * adj - 3.0 * ones - (tw > 0.25 ? 20 * (tw - 0.25) : 0) - 0.6 * (qhi - qlo) - 1.0 * empty + 0.4 * kinds;
}

/* the numbers whichever way they lie: the smallest code of their eight (turned, mirrored) */
static void canon(const rct_puz *p, char *key)
{
    int n = p->n;
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        rct_puz q = *p;
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int s = sx; sx = sy; sy = s; }
                q.clue[y * n + x] = p->clue[sy * n + sx];
            }
        char k[CODE];
        rct_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[RCT_STAGES];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* one puzzle of the make from the seed: 0 none found */
static int make_one(const make *m, uint32_t seed, cand *c)
{
    static const int MAXA[RCT_MAX + 1] = {10, 10, 10, 10, 10, 10, 10, 10, 12, 12, 12, 14, 14, 14, 16, 16, 16};
    static uint32_t own[RCT_CELLS];
    uint32_t rng = pr_seed(seed);
    int n = m->n;
    for (int attempt = 0; attempt < 40; attempt++) {
        if (!cut(n, m->sym, MAXA[n], &rng, own)) continue;
        rct_puz p;
        memset(&p, 0, sizeof p);
        p.n = n;
        int lead[RCT_CELLS], nl = 0;            /* a cell of each set */
        for (int i = 0; i < n * n; i++) {
            int x, y, w, h;
            rct_unpack(own[i], &x, &y, &w, &h);
            if (y * n + x != i || p.clue[i]) continue;
            int any = 0;                        /* a set not numbered yet */
            uint32_t orb[4];
            int k = orbit(own[i], m->sym, orb);
            for (int o = 0; o < k; o++) {
                int rx, ry, rw, rh;
                rct_unpack(orb[o], &rx, &ry, &rw, &rh);
                for (int j = ry; j < ry + rh; j++)
                    for (int q = rx; q < rx + rw; q++) any |= p.clue[j * n + q];
            }
            if (any) continue;
            place(&p, own, i, m->sym, &rng);
            lead[nl++] = i;
        }
        for (int fix = 0; fix < 30; fix++) {
            int steps, g = rct_effort(&p, &steps);
            if (g == m->level && rct_count(&p, 2, NULL) == 1) {
                c->p = p, c->grade = g, c->steps = steps;
                return 1;
            }
            if (g < m->level) break;
            place(&p, own, lead[pr_below(&rng, nl)], m->sym, &rng);
        }
    }
    return 0;
}

static int make_pool(const make *m, uint32_t seed0, int want)
{
    int k = 0;
    for (uint32_t seed = seed0; k < want && seed < seed0 + 30u * (uint32_t)want; seed++) {
        cand *c = &pool[k];
        if (!make_one(m, seed, c) || c->steps < 1) continue;     /* not a puzzle that solves itself */
        canon(&c->p, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->clues = 0;
        for (int i = 0; i < m->n * m->n; i++) c->clues += c->p.clue[i] != 0;
        c->look = look(&c->p);
        c->score = c->steps + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < RCT_CHAPTERS; ch++) {
        int first = nchosen;
        for (int mi = 0; mi < 3; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 3 + mi + 1), m->count == 1 ? 12 : CANDS);
            fprintf(stderr, "chapter %d, make %d (%dx%d level %d, turns %d): %d candidates, effort %d..%d\n",
                    ch + 1, mi + 1, m->n, m->n, m->level, m->sym, k, k ? pool[0].steps : 0,
                    k ? pool[k - 1].steps : 0);
            if (m->count == 1) {                /* the showpiece: the best looking of the harder half */
                int best = -1;
                for (int i = k / 2; i < k; i++)
                    if (!seen(pool[i].key) && (best < 0 || pool[i].look > pool[best].look)) best = i;
                if (best < 0) {
                    fprintf(stderr, "chapter %d: no showpiece\n", ch + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[best];
                continue;
            }
            for (int i = 0; i < m->count; i++) {
                int lo = m->lo * (k - 1) / 100, at = lo + (int)((i + 0.5f) / m->count * (k - 1 - lo)), best = -1;
                for (int j = at - WINDOW; j <= at + WINDOW; j++)
                    if (j >= lo && j < k && !seen(pool[j].key) && (best < 0 || pool[j].look > pool[best].look))
                        best = j;
                if (best < 0) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[best];
            }
        }
        for (int i = first + 1; i < nchosen - 1; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                double ka = a->p.n * 1e6 + a->grade * 1e5 + a->score, kb = b->p.n * 1e6 + b->grade * 1e5 + b->score;
                if (ka <= kb) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n"
           " * Rectangles' levels: our own, made by tools/rectangles_gen.c (symmetric givens, the game's reasoning\n"
           " * grader's grade and effort, chosen by their look), six chapters of 25 from the easiest, each ending on\n"
           " * a showpiece with quarter-turn symmetry. Each: \"<n>:\" and a character a cell, row by row: '.' none,\n"
           " * '1'..'9', 'a' 10, 'b' 11... (rectangles.h, rct_decode); in the comment the reasoning it needs (0 easy,\n"
           " * 1 medium, 2 hard), the grader's steps weighed by their level, and the numbers.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const rct_levels[RCT_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[CODE];
        rct_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, grade %d, effort %d, %d numbers */\n", code, i + 1, chosen[i].p.n,
               chosen[i].p.n, chosen[i].grade, chosen[i].steps, chosen[i].clues);
    }
    printf("};\n");
    return nchosen == RCT_STAGES ? 0 : 1;
}
