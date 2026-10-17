/*
 * Loop's levels, made and chosen for a hand-set look: src/games/loop_levels.h. Our own puzzles. Each candidate:
 *   - a loop from the game's own maker (a region of cells grown by chance, its border the loop), kept only when it
 *     reaches every row and every column of the grid;
 *   - every number counted, then numbers taken away in symmetric sets as a setter would: a cell and its partner a
 *     half turn away together (a quarter turn, four at a time, for the showpiece that ends each chapter), as long as
 *     the solver still finishes it at its level (hard: then one more set taken away so that trials are needed);
 *   - its grade (bench.h: a number's sides, a dot's two lines, no small loop; a number with the dots at its corners;
 *     a side tried and followed to a contradiction), the solver's passes (its work), its numbers; one solution
 *     (a plain search finds no other);
 *   - its look: no big empty square without a number, the numbers spread over the four quarters;
 *   - near-duplicates dropped (the same numbers turned or mirrored).
 * Each chapter has its makes (5 x 5 with the plainest reasoning up to 15 x 15 needing trials); a make's best-looking
 * candidates are ranked by difficulty and picked evenly over them. Then each chapter goes from its easiest to its
 * hardest, its showpiece last.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/slkgen apps/boardgames/tools/loop_gen.c \
 *       apps/boardgames/src/games/loop_rules.c && /tmp/slkgen > apps/boardgames/src/games/loop_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/loop.h"
#include "games/puzrand.h"

#define CANDS 40                                /* candidates made for each make */
#define KEY (SLK_CELLS + 8)

typedef struct {
    slk_puzzle p;
    int grade, numbers, length, sym;
    long passes;
    double score, look;
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

static int by_look(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->look < y->look ? -1 : x->look > y->look;
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

/* a cell's place turned k quarter turns about the middle */
static int turn(int n, int c, int k)
{
    int x = c % n, y = c / n;
    for (int i = 0; i < k; i++) {
        int t = x;
        x = n - 1 - y, y = t;
    }
    return y * n + x;
}

/* the cells of c's set: sym 2 (a half turn) or 4 (quarter turns); its size */
static int orbit(int n, int c, int sym, int *o)
{
    int k = 0;
    for (int t = 0; t < 4; t += 4 / sym) {
        int d = turn(n, c, t), dup = 0;
        for (int i = 0; i < k; i++) dup |= o[i] == d;
        if (!dup) o[k++] = d;
    }
    return k;
}

/* 1: the solver finishes it with nothing past `top` */
static int solves(const slk_puzzle *p, int top)
{
    int r = slk_rate(p);
    return r >= BENCH_EASY && r <= top;
}

/* 1: the loop reaches every row and every column */
static int reaches_all(const slk_puzzle *p)
{
    int n = p->n, row[SLK_MAX] = {0}, col[SLK_MAX] = {0};
    for (int s = 0; s < slk_sides(n); s++) {
        if (!p->sol[s]) continue;
        int x0, y0, x1, y1;
        slk_ends(n, s, &x0, &y0, &x1, &y1);
        if (y0 == y1) col[x0] = 1;
        else row[y0] = 1;
    }
    for (int i = 0; i < n; i++)
        if (!row[i] || !col[i]) return 0;
    return 1;
}

/* how it looks (0 best): the side of the biggest square without a number, the quarters' counts far from even */
static double look(const slk_puzzle *p)
{
    int n = p->n, big = 0, q[4] = {0}, tot = 0;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            for (int s = big + 1; x + s <= n && y + s <= n; s++) {
                int empty = 1;
                for (int j = 0; j < s && empty; j++)
                    for (int i = 0; i < s && empty; i++) empty = p->clue[(y + j) * n + x + i] < 0;
                if (!empty) break;
                big = s;
            }
    for (int c = 0; c < n * n; c++)
        if (p->clue[c] >= 0) {
            int x = c % n, y = c / n;
            if (2 * x + 1 != n && 2 * y + 1 != n) q[(2 * x >= n) + 2 * (2 * y >= n)]++, tot++;
        }
    double spread = 0;
    for (int i = 0; i < 4 && tot; i++) spread += abs(4 * q[i] - tot) / (double)tot;
    return (double)big / n + 0.25 * spread;
}

/* a candidate: the maker's loop, its numbers taken away in symmetric sets; 1 when it is at the level, one solution */
static int candidate(cand *c, int n, int level, int sym, uint32_t seed)
{
    memset(c, 0, sizeof *c);
    slk_puzzle *p = &c->p;
    slk_make(n, level, seed, p);
    if (p->n != n || !reaches_all(p)) return 0;
    for (int i = 0; i < n * n; i++) {
        int x = i % n, y = i / n;
        p->clue[i] = (int8_t)(p->sol[slk_across(n, x, y)] + p->sol[slk_across(n, x, y + 1)] +
                              p->sol[slk_down(n, x, y)] + p->sol[slk_down(n, x + 1, y)]);
    }
    uint32_t rng = pr_seed(seed ^ 0x5eedu);
    int order[SLK_CELLS], no = 0, top = level < SLK_HARD ? level : SLK_MEDIUM;
    for (int i = 0; i < n * n; i++) {
        int o[4], k = orbit(n, i, sym, o), least = i;
        for (int j = 0; j < k; j++) least = o[j] < least ? o[j] : least;
        if (least == i) order[no++] = i;
    }
    pr_shuffle(&rng, order, no);
    if (!solves(p, top)) return 0;
    int8_t was[4];
    for (int t = 0; t < no; t++) {
        int o[4], k = orbit(n, order[t], sym, o);
        for (int j = 0; j < k; j++) was[j] = p->clue[o[j]], p->clue[o[j]] = -1;
        if (!solves(p, top))
            for (int j = 0; j < k; j++) p->clue[o[j]] = was[j];
    }
    if (level == SLK_HARD) {                    /* one more set taken away: trials needed */
        int done = 0;
        for (int t = 0; t < no && !done; t++) {
            int o[4], k = orbit(n, order[t], sym, o);
            if (p->clue[o[0]] < 0) continue;
            for (int j = 0; j < k; j++) was[j] = p->clue[o[j]], p->clue[o[j]] = -1;
            if (slk_rate(p) == SLK_HARD) done = 1;
            else
                for (int j = 0; j < k; j++) p->clue[o[j]] = was[j];
        }
        if (!done) return 0;
    }
    c->grade = slk_rate_work(p, &c->passes);
    if (c->grade != level || slk_count(p, 2) != 1) return 0;
    slk_encode(p, c->code);
    slk_game g;
    if (!slk_decode(&g, c->code) || memcmp(g.p.sol, p->sol, (size_t)slk_sides(n))) return 0;
    canon(p, c->key);
    for (int i = 0; i < n * n; i++) c->numbers += p->clue[i] >= 0;
    for (int s = 0; s < slk_sides(n); s++) c->length += p->sol[s];
    c->sym = sym;
    c->look = look(p);
    c->score = c->grade * 1000.0 + c->passes * 3.0 - c->numbers * 0.5;
    return 1;
}

/* a pool of candidates for a make; how many */
static int make_pool(const make *mk, int sym, int want, uint32_t *seed)
{
    int np = 0;
    for (int tries = 0; np < want && tries < 40 * want; tries++) {
        cand *c = &pool[np];
        if (!candidate(c, mk->n, mk->level, sym, 0x100b0000u + (*seed)++ * 2654435761u)) continue;
        int dup = seen(c->key);
        for (int i = 0; i < np && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (!dup) np++;
    }
    return np;
}

int main(void)
{
    uint32_t seed = 1;
    for (int ch = 0; ch < SLK_CHAPTERS; ch++) {
        int first = nchosen, last = 0;
        while (last < 2 && CHAPTER[ch][last + 1].count) last++;
        for (int m = 0; m <= last; m++) {
            const make *mk = &CHAPTER[ch][m];
            int count = mk->count - (m == last);   /* the chapter's last make gives its showpiece too */
            int np = make_pool(mk, 2, CANDS, &seed);
            if (np < count + 2) {
                fprintf(stderr, "chapter %d make %d: only %d\n", ch + 1, m, np);
                return 1;
            }
            qsort(pool, (size_t)np, sizeof pool[0], by_look);   /* the best-looking two thirds, by difficulty */
            int keep = np * 2 / 3 > count ? np * 2 / 3 : count;
            qsort(pool, (size_t)keep, sizeof pool[0], by_score);
            fprintf(stderr, "chapter %d make %d: %d candidates\n", ch + 1, m, np);
            for (int i = 0; i < count; i++) chosen[nchosen++] = pool[count > 1 ? i * (keep - 1) / (count - 1) : 0];
        }
        for (int i = first; i < nchosen; i++)  /* the chapter from its easiest: grade, work, size */
            chosen[i].score = chosen[i].grade * 1000.0 + chosen[i].passes * 3.0 + chosen[i].p.n * 10.0;
        qsort(chosen + first, (size_t)(nchosen - first), sizeof chosen[0], by_score);
        int np = make_pool(&CHAPTER[ch][last], 4, 12, &seed);   /* the showpiece: four-fold, the best-looking */
        if (!np) {
            fprintf(stderr, "chapter %d: no showpiece\n", ch + 1);
            return 1;
        }
        qsort(pool, (size_t)np, sizeof pool[0], by_look);
        chosen[nchosen++] = pool[0];
        fprintf(stderr, "chapter %d done (showpiece of %d)\n", ch + 1, np);
    }
    static const char *const GRADE[3] = {"sides and dots", "corners", "a side tried"};
    printf("/*\n * Loop's levels: our own, made by tools/loop_gen.c (a loop from the game's maker, its numbers taken\n"
           " * away in symmetric sets while the solver finishes it at its grade; chosen for their look), six chapters\n"
           " * of 25 from the easiest, each ending on a showpiece with quarter-turn symmetry.\n"
           " * Each: \"<n>:\", a character a cell ('.' no number), ':' and the loop's sides (slk_decode); in the\n"
           " * comment the reasoning it needs (its grade), the solver's passes, its numbers and the loop's length.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const slk_levels[SLK_NLEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        const cand *c = &chosen[i];
        printf("    \"%s\", /* %d: %dx%d, %s, %ld passes, %d numbers, %d long%s */\n", c->code, i + 1, c->p.n, c->p.n,
               GRADE[c->grade], c->passes, c->numbers, c->length, c->sym == 4 ? ", showpiece" : "");
    }
    printf("};\n");
    return nchosen != SLK_NLEVELS;
}
