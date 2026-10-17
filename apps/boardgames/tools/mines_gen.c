/*
 * Mines' levels, made by chance and chosen: src/games/mines_levels.h. Our own fields, from the game's own reasoner (the
 * one that lays the no-guessing fields and gives the hints). Each candidate: a first cell drawn by chance, the mines
 * laid by chance away from it (never on it nor round it, so it opens an area), kept when the reasoner clears the field
 * from that area without a guess. Then:
 *   - its grade: how often the reasoner had to compare two numbers side by side (a subset, the 1-2-1 and 1-2 patterns)
 *     or count the mines left, because no number alone told anything more; and how many rounds it took;
 *   - the work left after the first area (an opening that clears most of the field is dropped);
 *   - near-duplicates dropped (the same field turned or mirrored, its first cell with it).
 * Taste, so the fields look laid rather than thrown: the mines spread over the four quarters (no empty half, no
 * heap of mines in one corner: at most five in any 3 x 3), the first cell away from the edge (its area opens in the
 * field, not along a side), and each pick takes, among the few candidates nearest its place in the ranking, the one
 * that looks best. Each chapter ends on a showpiece: a field whose mines lie turned half round (each mine's twin a
 * mine too), the hardest of its last make.
 * Each chapter has five makes (a size and its mines, the comparisons wanted); a make's candidates are ranked by
 * difficulty and picked evenly over them, from the easiest. Chapter 1 asks only one number at a time (5 x 5 with 3
 * mines first); chapter 6 ends on fields of the expert's size.
 * Deterministic (fixed seeds): run it again after a change to the reasoner and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/mnsgen apps/boardgames/tools/mines_gen.c \
 *       apps/boardgames/src/games/mines_rules.c && /tmp/mnsgen > apps/boardgames/src/games/mines_levels.h
 * "/tmp/mnsgen survey W H MINES": how the grades spread for that size (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/mines.h"
#include "games/puzrand.h"

#define CANDS 60                                /* candidates made for each make */
#define TRIES 400000                            /* fields laid at most for them */

typedef struct {
    int w, h, mines, first;
    uint8_t mine[MNS_CELLS];
    int rounds[MNS_TIERS], hard, left;          /* hard: the comparisons and counts; left: safe cells after the area */
    double score, look;
    int sym;
    char key[MNS_CELLS + 8];
} cand;

typedef struct { int w, h, mines, hmin, hmax, count; } make;
/* w x h with mines; the rounds of harder reasoning wanted (hmin..hmax); how many levels */
static const make CHAPTER[MNS_CHAPTERS][5] = {
    {{5, 5, 3, 0, 0, 5}, {5, 6, 4, 0, 0, 5}, {6, 6, 5, 0, 0, 5}, {6, 7, 6, 0, 0, 5}, {7, 7, 7, 0, 0, 5}},
    {{7, 8, 8, 1, 1, 5}, {7, 9, 9, 1, 2, 5}, {8, 9, 11, 1, 2, 5}, {8, 10, 12, 2, 3, 5}, {9, 10, 13, 2, 3, 5}},
    {{9, 10, 14, 2, 4, 5}, {9, 11, 16, 3, 4, 5}, {9, 12, 17, 3, 5, 5}, {10, 12, 19, 3, 6, 5}, {10, 13, 20, 4, 6, 5}},
    {{10, 13, 21, 4, 7, 5}, {10, 14, 23, 4, 7, 5}, {11, 14, 25, 5, 8, 5}, {11, 15, 27, 5, 9, 5},
     {12, 15, 30, 6, 10, 5}},
    {{12, 16, 33, 6, 10, 5}, {12, 18, 37, 7, 11, 5}, {13, 18, 41, 7, 12, 5}, {13, 20, 46, 8, 13, 5},
     {14, 20, 50, 8, 14, 5}},
    {{14, 22, 57, 9, 15, 5}, {15, 22, 62, 10, 16, 5}, {16, 24, 72, 11, 18, 5}, {16, 26, 82, 12, 20, 5},
     {16, 30, 99, 14, 99, 5}},
};

/* the field whichever way it lies: the smallest of its eight (turned, mirrored), the first cell marked */
static void canon(cand *c)
{
    char best[MNS_CELLS + 8], k[MNS_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        int sw = t & 4 ? c->h : c->w, sh = t & 4 ? c->w : c->h, n = 0;
        n += sprintf(k, "%dx%d:", sw, sh);
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++) {
                int a = x, b = y;
                if (t & 1) a = sw - 1 - a;
                if (t & 2) b = sh - 1 - b;
                int fx = t & 4 ? b : a, fy = t & 4 ? a : b, cell = fx + fy * c->w;
                k[n++] = cell == c->first ? 'F' : c->mine[cell] ? '*' : '.';
            }
        k[n] = 0;
        if (!best[0] || strcmp(k, best) < 0) strcpy(best, k);
    }
    strcpy(c->key, best);
}

/* how good it looks (more is better): mines in all four quarters alike, no heap of them, the first cell inside */
static double looks(const cand *c)
{
    int w = c->w, h = c->h, q[4] = {0}, heap = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (c->mine[x + y * w] && 2 * x != w - 1 && 2 * y != h - 1) q[(2 * x > w - 1) + 2 * (2 * y > h - 1)]++;
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++) {
            int k = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) k += c->mine[x + dx + (y + dy) * w];
            if (k > heap) heap = k;
        }
    int lo = q[0], hi = q[0], sum = 0;
    for (int i = 0; i < 4; i++) lo = q[i] < lo ? q[i] : lo, hi = q[i] > hi ? q[i] : hi, sum += q[i];
    int fx = c->first % w, fy = c->first / w;
    int edge = fx == 0 || fy == 0 || fx == w - 1 || fy == h - 1;
    return -4.0 * (hi - lo) / (sum / 4.0 + 1) - 1.5 * (heap > 4 ? heap - 4 : 0) - 1.0 * edge;
}

/* one field laid by chance (sym: its mines in twins turned half round); 1 when it is a level (no guess, the work
 * left) */
static int lay_as(cand *c, const make *m, uint32_t *s, int sym)
{
    int N = m->w * m->h, free_[MNS_CELLS], nf = 0;
    c->w = m->w;
    c->h = m->h;
    c->mines = m->mines;
    c->first = pr_below(s, N);
    int fx = c->first % m->w, fy = c->first / m->w;
    for (int i = 0; i < N; i++) {
        int dx = i % m->w - fx, dy = i / m->w - fy;
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1) free_[nf++] = i;
    }
    memset(c->mine, 0, sizeof c->mine);
    c->sym = sym;
    if (sym) {                                  /* the twins: both away from the first cell */
        int ok[MNS_CELLS] = {0}, np = 0, left = m->mines;
        for (int i = 0; i < nf; i++) ok[free_[i]] = 1;
        nf = 0;
        for (int i = 0; i < N; i++)
            if (ok[i] && ok[N - 1 - i] && i < N - 1 - i) free_[nf++] = i;
        if (left % 2) {
            if (N % 2 == 0 || !ok[N / 2]) return 0;
            c->mine[N / 2] = 1;
            left--;
        }
        for (int i = 0; i < left / 2 && np < nf; i++, np++) {
            int j = i + pr_below(s, nf - i), t = free_[i];
            free_[i] = free_[j];
            free_[j] = t;
            c->mine[free_[i]] = c->mine[N - 1 - free_[i]] = 1;
        }
        if (np < left / 2) return 0;
    } else
        for (int i = 0; i < m->mines; i++) {
            int j = i + pr_below(s, nf - i), t = free_[i];
            free_[i] = free_[j];
            free_[j] = t;
            c->mine[free_[i]] = 1;
        }
    if (!mns_effort(m->w, m->h, m->mines, c->mine, c->first, c->rounds)) return 0;
    c->hard = c->rounds[MNS_PAIR] + c->rounds[MNS_COUNT];
    mns_level L = {(uint8_t)m->w, (uint8_t)m->h, (uint16_t)c->first, NULL};
    char bits[MNS_CELLS / 6 + 8];
    mns_encode(m->w, m->h, c->mine, bits);
    L.bits = bits;
    mns_game g;
    if (!mns_load(&g, &L)) return 0;
    c->left = N - m->mines - g.opened;
    c->look = looks(c);
    return 1;
}

static int lay(cand *c, const make *m, uint32_t *s) { return lay_as(c, m, s, 0); }

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[MNS_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int make_pool(const make *m, uint32_t seed, int sym)
{
    uint32_t s = pr_seed(seed);
    int k = 0, safe = m->w * m->h - m->mines;
    for (int t = 0; t < TRIES && k < CANDS; t++) {
        cand *c = &pool[k];
        if (!lay_as(c, m, &s, sym) || c->hard < m->hmin || c->hard > m->hmax) continue;
        if (c->left < safe / 2 || c->left < 6) continue;   /* the first area leaves most of the field to clear */
        canon(c);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        /* the harder reasoning first, then the rounds (how long it goes on) and the work left */
        c->score = 3.0 * c->rounds[MNS_PAIR] + 4.0 * c->rounds[MNS_COUNT] + 0.25 * c->rounds[MNS_SINGLE] +
                   0.02 * c->left;
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

static void survey(int w, int h, int mines)
{
    make m = {w, h, mines, 0, 999, 0};
    uint32_t s = pr_seed(7);
    int hist[32] = {0}, ok = 0, n = 20000;
    for (int t = 0; t < n; t++) {
        cand c;
        if (!lay(&c, &m, &s)) continue;
        ok++;
        hist[c.hard < 31 ? c.hard : 31]++;
    }
    printf("%dx%d/%d: %d of %d no-guess (%.1f%%); harder rounds:", w, h, mines, ok, n, 100.0 * ok / n);
    for (int i = 0; i < 32; i++)
        if (hist[i]) printf(" %d:%d", i, hist[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    if (argc == 5 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        return 0;
    }
    for (int ch = 0; ch < MNS_CHAPTERS; ch++)
        for (int p = 0; p < 5; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 5 + p + 1), 0);
            fprintf(stderr, "chapter %d, %dx%d/%d: %d candidates\n", ch + 1, m->w, m->h, m->mines, k);
            int count = m->count - (p == 4);    /* the chapter's last: the showpiece below */
            for (int i = 0; i < count; i++) {
                int at = (int)((i + 0.5f) / m->count * k);
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough of %dx%d/%d\n", ch + 1, m->w, m->h, m->mines);
                    return 1;
                }
                /* the best looking of the few nearest that place in the ranking */
                int span = k / (2 * m->count) > 1 ? k / (2 * m->count) : 1, best = at;
                for (int j = at - span; j <= at + span; j++)
                    if (j >= 0 && j < k && !seen(pool[j].key) && pool[j].look > pool[best].look) best = j;
                chosen[nchosen++] = pool[best];
            }
            if (p == 4) {                       /* the showpiece: the mines in twins, of the hardest kept */
                make ms = *m;                   /* twins on an even field: an even count of mines */
                if (ms.mines % 2 && ms.w * ms.h % 2 == 0) ms.mines++;
                k = make_pool(&ms, 1000u * (uint32_t)(ch * 5 + p + 1) + 500u, 1);
                fprintf(stderr, "chapter %d, showpiece: %d candidates\n", ch + 1, k);
                int best = -1;
                for (int j = k - 1; j >= k * 2 / 3 && j >= 0; j--)
                    if (!seen(pool[j].key) && (best < 0 || pool[j].look > pool[best].look)) best = j;
                if (best < 0) {
                    fprintf(stderr, "chapter %d: no showpiece\n", ch + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[best];
            }
        }
    printf("/*\n * Mines' levels: our own, made by tools/mines_gen.c (mines laid by chance away from a first cell,\n"
           " * kept when the game's reasoner clears the field from that first area without a guess), six chapters\n"
           " * of 25 from the easiest. Each: its size, its first cell (x + y * w) and its mines (mines.h,\n"
           " * mns_encode); in the comment the mines and the reasoner's rounds: with one number alone, two numbers\n"
           " * side by side, the mines left.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n"
           "static const mns_level mns_levels[MNS_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        const cand *c = &chosen[i];
        char bits[MNS_CELLS / 6 + 8];
        mns_encode(c->w, c->h, c->mine, bits);
        printf("    {%d, %d, %d, \"%s\"}, /* %d: %dx%d/%d, rounds %d %d %d%s */\n", c->w, c->h, c->first, bits,
               i + 1, c->w, c->h, c->mines, c->rounds[MNS_SINGLE], c->rounds[MNS_PAIR], c->rounds[MNS_COUNT],
               c->sym ? ", in twins" : "");
    }
    printf("};\n");
    return nchosen == MNS_LEVELS ? 0 : 1;
}
