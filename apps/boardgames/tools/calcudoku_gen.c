/*
 * Calcudoku's levels, made by chance and chosen: src/games/calcudoku_levels.h. Our own puzzles, made here with the
 * game's solver (calcudoku_rules.c): a Latin square by chance, cut into cages laid out with a half turn's symmetry (each
 * cage has its image, as in a puzzle laid out by hand; few single cells), a sign for each; made again, or made harder
 * a step at a time (a pair of cages joined at once), until the reasoning solver finishes it at its level: one
 * solution. Each candidate:
 *   - its grade (the hardest reasoning the solver needed: the cages' ways and the singles; a cage's digits held in one
 *     line, pairs and triples; trials) and the solver's passes (its effort);
 *   - near-duplicates dropped (the same grid turned or mirrored).
 * Each chapter has two makes (a size and a level); a make's candidates are ranked by effort and picked evenly over
 * them. Then each chapter goes from its smallest and easiest to its biggest and hardest: 4 x 4 easy first, 9 x 9 hard
 * last.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/cdkgen apps/boardgames/tools/calcudoku_gen.c \
 *       apps/boardgames/src/games/calcudoku_rules.c && /tmp/cdkgen > apps/boardgames/src/games/calcudoku_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/calcudoku.h"
#include "games/puzrand.h"

#define CANDS 60                                /* candidates made for each make */
#define CODE (3 * CDK_CELLS + 8)

typedef struct {
    cdk_puzzle p;
    int grade;
    long passes;
    double score;
    char key[CODE];
} cand;

typedef struct { int n, level, count; } make;
static const make CHAPTER[CDK_CHAPTERS][2] = {
    {{4, CDK_EASY, 12}, {5, CDK_EASY, 13}},
    {{4, CDK_MEDIUM, 10}, {5, CDK_MEDIUM, 15}},
    {{6, CDK_EASY, 10}, {6, CDK_MEDIUM, 15}},
    {{6, CDK_HARD, 12}, {7, CDK_MEDIUM, 13}},
    {{7, CDK_HARD, 12}, {8, CDK_MEDIUM, 13}},
    {{8, CDK_HARD, 12}, {9, CDK_HARD, 13}},
};

/* the puzzle whichever way it lies: the smallest code of its eight (turned, mirrored) */
static void canon(const cdk_puzzle *p, char *key)
{
    int n = p->n;
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        cdk_puzzle q = *p;
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int s = sx; sx = sy; sy = s; }
                q.sol[y * n + x] = p->sol[sy * n + sx];
                q.cage_of[y * n + x] = p->cage_of[sy * n + sx];
            }
        char k[CODE];
        cdk_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

/* ---- the handmade look: cages laid out with a half turn's symmetry -------------------------------------------------
 * Each cage has its image under a half turn of the grid (a cage of its own, or itself across the centre), as in a
 * puzzle laid out by hand; few single cells (a free digit), and the cages' sizes and signs mixed as the game's maker
 * mixes them (calcudoku_rules.c). The hard ones are made harder a step at a time, a pair of cages at once. */
static const int WANT[CDK_LEVELS][4] = {{14, 52, 28, 6}, {5, 42, 40, 13}, {2, 30, 46, 22}};   /* sizes 1..4, % */

static int latin_fill(int n, uint8_t *g, int c, uint32_t *s)
{
    if (c == n * n) return 1;
    int ds[CDK_MAX];
    for (int i = 0; i < n; i++) ds[i] = i + 1;
    pr_shuffle(s, ds, n);
    for (int i = 0; i < n; i++) {
        int ok = 1;
        for (int j = 0; j < c % n && ok; j++) ok = g[c - c % n + j] != ds[i];
        for (int j = c % n; j < c && ok; j += n) ok = g[j] != ds[i];
        if (!ok) continue;
        g[c] = (uint8_t)ds[i];
        if (latin_fill(n, g, c + 1, s)) return 1;
    }
    g[c] = 0;
    return 0;
}

/* the puzzle from the cells' labels and each label's sign (a sign that does not fit the cage's size made again) */
static void build(cdk_puzzle *p, const int *lab, int *op, uint32_t *s)
{
    int n = p->n, map[CDK_CELLS];
    memset(map, -1, sizeof map);
    p->ncages = 0;
    for (int c = 0; c < n * n; c++) {
        int l = lab[c];
        if (map[l] < 0) {
            map[l] = p->ncages++;
            p->cage[map[l]].n = 0;
        }
        cdk_cage *g = &p->cage[map[l]];
        g->cell[g->n++] = (uint8_t)c;
        p->cage_of[c] = (uint8_t)map[l];
    }
    for (int c = 0; c < n * n; c++) {
        int l = lab[c];
        cdk_cage *g = &p->cage[p->cage_of[c]];
        if (g->cell[0] != c) continue;
        int v[CDK_CAGE];
        for (int i = 0; i < g->n; i++) v[i] = p->sol[g->cell[i]];
        if (g->n == 1) op[l] = CDK_ONE;
        else if (g->n == 2) {
            int hi = v[0] > v[1] ? v[0] : v[1], lo = v[0] + v[1] - hi;
            if (op[l] == CDK_ONE || (op[l] == CDK_DIV && hi % lo)) {
                int roll = pr_below(s, 100);
                op[l] = hi % lo == 0 && roll < 55 ? CDK_DIV : (roll = pr_below(s, 100)) < 50 ? CDK_SUB
                      : roll < 75 ? CDK_ADD : CDK_MUL;
            }
        } else if (op[l] != CDK_ADD && op[l] != CDK_MUL) op[l] = pr_below(s, 2) ? CDK_MUL : CDK_ADD;
        g->op = (uint8_t)op[l];
        g->target = cdk_value(g->op, v, g->n);
    }
}

static int adj(int n, int a, int b)
{
    return (a / n == b / n && abs(a - b) == 1) || abs(a - b) == n;
}

/* the cells cut into cages two by two (a cage and its image); 0: too many single cells */
static int sym_cages(int n, int level, uint32_t *s, int *lab)
{
    int nn = n * n, order[CDK_CELLS], nl = 0;
    for (int c = 0; c < nn; c++) lab[c] = -1, order[c] = c;
    pr_shuffle(s, order, nn);
    for (int k = 0; k < nn; k++) {
        int c = order[k], im = nn - 1 - c;
        if (lab[c] >= 0) continue;
        int roll = pr_below(s, 100), want = 1;
        for (int t = 0; t < 3 && roll >= WANT[level][t]; t++) roll -= WANT[level][t], want++;
        int mem[CDK_CAGE] = {c}, m = 1, a = nl++, b = c == im ? a : nl++;
        lab[c] = a;
        lab[im] = b;
        if (c == im) { if (want > 1) want = 3; }   /* the centre: alone, or a bar of three through it */
        while (m < want) {
            int cand[4 * CDK_CAGE], nc = 0;
            for (int i = 0; i < m; i++)
                for (int x = 0; x < nn; x++)
                    if (lab[x] < 0 && adj(n, x, mem[i]) && lab[nn - 1 - x] < 0 && x != nn - 1 - x)
                        cand[nc++] = x;
            if (!nc) break;
            int x = cand[pr_below(s, nc)];
            lab[x] = a;
            mem[m++] = x;
            lab[nn - 1 - x] = b;
            if (c == im) mem[m++] = nn - 1 - x;
        }
    }
    int singles = 0, size[CDK_CELLS] = {0};
    for (int c = 0; c < nn; c++) size[lab[c]]++;
    for (int l = 0; l < nl; l++) singles += size[l] == 1;
    int most = level == CDK_EASY ? n / 2 + 1 : level == CDK_MEDIUM ? n / 3 + 1 : 1;
    return singles <= most;
}

/* two neighbouring cages joined (four cells at most), and their images likewise; or a pair's sign loosened */
static int sym_harden(cdk_puzzle *p, int *lab, int *op, uint32_t *s)
{
    int n = p->n, nn = n * n;
    for (int tries = 0; tries < 40; tries++) {
        int c = pr_below(s, nn), x = c % n, y = c / n, dir = pr_below(s, 4);
        int a = lab[c], ai = lab[nn - 1 - c];
        int size[CDK_CELLS] = {0};
        for (int i = 0; i < nn; i++) size[lab[i]]++;
        if (pr_below(s, 4) == 0) {
            if (size[a] < 2) continue;
            op[a] = op[ai] = size[a] > 2 ? (op[a] == CDK_ADD ? CDK_MUL : CDK_ADD) : pr_below(s, 2) ? CDK_ADD : CDK_MUL;
            return 1;
        }
        int nx = x + (dir == 0) - (dir == 1), ny = y + (dir == 2) - (dir == 3);
        if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
        int d = ny * n + nx, b = lab[d], bi = lab[nn - 1 - d];
        if (a == b) continue;
        int joined = size[a] + size[b];
        /* a cage with its own image: one cage across the centre; else two pairs joined (none of them the centre's) */
        if (joined > CDK_CAGE || (a != bi && (a == ai || b == bi))) continue;
        int mul = pr_below(s, 2) ? CDK_MUL : CDK_ADD;
        for (int i = 0; i < nn; i++) {
            if (lab[i] == b) lab[i] = a;
            else if (lab[i] == bi && a != bi) lab[i] = ai;
        }
        op[a] = op[ai] = joined > 2 ? mul : CDK_ONE;
        return 1;
    }
    return 0;
}

static int sym_make(int n, int level, uint32_t seed, cdk_puzzle *p)
{
    uint32_t s = pr_seed(seed);
    int lab[CDK_CELLS], op[CDK_CELLS];
    memset(p, 0, sizeof *p);
    p->n = n;
    for (int tries = 0; tries < 60; tries++) {
        if (tries % 4 == 0 && !latin_fill(n, p->sol, 0, &s)) return 0;
        if (!sym_cages(n, level == CDK_HARD ? CDK_MEDIUM : level, &s, lab)) continue;
        for (int i = 0; i < n * n; i++) op[i] = CDK_ONE;
        build(p, lab, op, &s);
        int r = cdk_rate(p);
        if (level < CDK_HARD) {
            if (r == level) return 1;
            continue;
        }
        if (r < 0 || r > CDK_MEDIUM) continue;
        for (int step = 0, expensive = 0; step < 80 && expensive < 6; step++) {
            int was_lab[CDK_CELLS], was_op[CDK_CELLS];
            cdk_puzzle was = *p;
            memcpy(was_lab, lab, sizeof lab);
            memcpy(was_op, op, sizeof op);
            if (!sym_harden(p, lab, op, &s)) break;
            build(p, lab, op, &s);
            r = cdk_rate(p);
            if (r >= 0 && r <= CDK_MEDIUM) continue;
            if (r == CDK_HARD) return 1;
            expensive++;
            *p = was;
            memcpy(lab, was_lab, sizeof lab);
            memcpy(op, was_op, sizeof op);
        }
    }
    return 0;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[CDK_STAGES];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!sym_make(m->n, m->level, seed, &c->p)) continue;
        c->grade = cdk_effort(&c->p, &c->passes);
        if (c->grade != m->level || cdk_count(&c->p, 2) != 1) continue;
        canon(&c->p, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = (double)c->passes + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < CDK_CHAPTERS; ch++) {
        int first = nchosen;
        for (int mi = 0; mi < 2; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d level %d): %d candidates, effort %ld..%ld\n", ch + 1, mi + 1,
                    m->n, m->n, m->level, k, k ? pool[0].passes : 0, k ? pool[k - 1].passes : 0);
            for (int i = 0; i < m->count; i++) {
                int at = (int)((i + 0.5f) / m->count * (k - 1));
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                double ka = a->p.n * 1e6 + a->grade * 1e5 + a->score, kb = b->p.n * 1e6 + b->grade * 1e5 + b->score;
                if (ka <= kb) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n * Calcudoku's levels: our own, made by tools/calcudoku_gen.c (the game's maker, the reasoning solver's grade\n"
           " * and effort), six chapters of 25 from the easiest. Each: \"<n>:\", the solution's digits, \":\", each cell's\n"
           " * joins to its cage (1 right, 2 down), \":\", the cages' signs (calcudoku.h, cdk_decode; the targets come from\n"
           " * the digits); in the comment the reasoning it needs (0 easy, 1 medium, 2 hard) and the solver's passes.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const cdk_levels[CDK_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[CODE];
        cdk_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, grade %d, %ld passes */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               chosen[i].grade, chosen[i].passes);
    }
    printf("};\n");
    return nchosen == CDK_STAGES ? 0 : 1;
}
