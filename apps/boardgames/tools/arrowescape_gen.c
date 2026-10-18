/*
 * Clear the Arrows' levels: src/games/arrowescape_levels.h. Our own boards: no published or commercial set is used.
 * Each chapter has its idea (single squares on small boards; long bars; turnstiles; bigger boards; shapes: hearts,
 * diamonds, rounds; all of it on the largest boards) and makes many boards with the rules' maker (arx_make, from fixed
 * seeds: tiles laid backwards in mirrored or half-turned pairs over the chapter's shapes). A board is kept when it can
 * be cleared, when its tiles are not all free at the start, when its depth (the rounds of taking every free tile at
 * once) is in the chapter's range, when it has the chapter's feature, and when it was not met before (mirrored
 * either). The difficulty: the depth, the share of tiles blocked at the start, the tiles. Each chapter keeps 25 spread
 * over its difficulties (chapter 1 from the very easiest); of the boards near each place on the curve the one that
 * looks best is kept (a shape, bars and turnstiles in the chapters that have them); the chapter's last level, its
 * showpiece, the best looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/arxgen apps/boardgames/tools/arrowescape_gen.c -lm && \
 *       /tmp/arxgen > apps/boardgames/src/games/arrowescape_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define ARX_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/arrowescape_rules.c"

#define PER (ARX_LEVELS / ARX_CHAPTERS)
#define MAKES 700                               /* boards kept a chapter, at most */

typedef struct {
    char text[ARX_TEXT];
    int rounds, n, shape;
    double diff, look;
} cand;

/* a chapter: the sizes w x h (up to grow more each way), its shapes (bits), the longest bar, bars %, turnstiles %,
 * how deep the maker digs (lo..hi), the depth's range; need: 1 bars, 2 turnstiles, 3 a shape not a rectangle */
typedef struct { int w, h, grow, shapes, maxlen, bars, turns, deep_lo, deep_hi, lo, hi, need; } chapter;

#define S(x) (1 << (x))
static const chapter CH[ARX_CHAPTERS] = {
    {3, 3, 3, S(ARX_RECT) | S(ARX_DIAMOND) | S(ARX_PLUS) | S(ARX_ROUND), 1, 0, 0, 0, 80, 2, 5, 0},
    {5, 5, 2, S(ARX_RECT) | S(ARX_OCTAGON) | S(ARX_PLUS) | S(ARX_DIAMOND), 3, 55, 0, 30, 90, 3, 7, 1},
    {5, 5, 2, S(ARX_RECT) | S(ARX_OCTAGON) | S(ARX_ROUND), 3, 30, 22, 40, 90, 3, 7, 2},
    {8, 8, 2, S(ARX_RECT) | S(ARX_OCTAGON) | S(ARX_ROUND) | S(ARX_DIAMOND), 3, 40, 0, 50, 100, 5, 11, 1},
    {8, 9, 2, S(ARX_HEART) | S(ARX_DIAMOND) | S(ARX_ROUND) | S(ARX_BLOB), 4, 40, 0, 60, 100, 6, 12, 3},
    {9, 10, 2, S(ARX_HEART) | S(ARX_OCTAGON) | S(ARX_ROUND) | S(ARX_BLOB) | S(ARX_RECT), 4, 40, 8, 70, 100, 7, 14, 1},
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- near-duplicates: the board and its mirror image ------------------------------------------------------------ */
static char (*kept)[ARX_TEXT];
static int nkept;

static void mirror_text(const arx_map *m, char *out)
{
    arx_map t = *m;
    for (int i = 0; i < t.n; i++) {
        t.p[i].x = (int8_t)(m->w - 1 - m->p[i].x);
        if (t.p[i].dir == ARX_LEFT || t.p[i].dir == ARX_RIGHT) t.p[i].dir = (int8_t)(t.p[i].dir ^ 2);
    }
    arx_text(&t, out);
}

static int met(const arx_map *m)
{
    char a[ARX_TEXT], b[ARX_TEXT];
    arx_text(m, a);
    mirror_text(m, b);
    for (int i = 0; i < nkept; i++)
        if (!strcmp(kept[i], a) || !strcmp(kept[i], b)) return 1;
    strcpy(kept[nkept++], a);
    return 0;
}

static int has(const arx_map *m, int need, int shape)
{
    int bars = 0, turns = 0;
    for (int i = 0; i < m->n; i++) {
        bars += m->p[i].len > 1;
        turns += m->p[i].turn;
    }
    if (need == 0) return !bars && !turns;
    if (need == 1) return bars >= 2;
    if (need == 2) return turns >= 2 && turns * 4 <= m->n;
    return shape != ARX_RECT && bars >= 2;
}

/* a board that looks designed: a shape (a heart above all), mirrored, the board well used, bars and turnstiles in
 * measure */
static double look_of(const arx_map *m, int shape)
{
    int bars = 0, turns = 0, cells = 0;
    for (int i = 0; i < m->n; i++) {
        bars += m->p[i].len > 1;
        turns += m->p[i].turn;
    }
    for (int c = 0; c < m->w * m->h; c++) cells += m->shape[c];
    double share = (double)bars / m->n;
    char a[ARX_TEXT], b[ARX_TEXT];
    arx_text(m, a);
    mirror_text(m, b);
    return (shape != ARX_RECT ? 0.6 : 0) + (shape == ARX_HEART ? 0.5 : 0) + (strcmp(a, b) ? 0 : 0.5) +
           0.4 * cells / (m->w * m->h) + (share > 0.2 && share < 0.6 ? 0.3 : 0) + (turns ? 0.1 : 0);
}

int main(void)
{
    kept = malloc((size_t)ARX_CHAPTERS * MAKES * 4 * sizeof *kept);
    static cand pool[MAKES];
    static cand out[ARX_LEVELS];
    printf("/* Clear the Arrows' levels: made by tools/arrowescape_gen.c (our own boards; do not edit). Each level: its "
           "board,\n * rows from the top split by '|' (arrowescape.h). */\n");
    for (int ch = 0; ch < ARX_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(9100u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < 60 * MAKES && n < MAKES; k++) {
            int shapes[ARX_SHAPES], ns = 0;
            for (int s = 0; s < ARX_SHAPES; s++)
                if (C->shapes >> s & 1) shapes[ns++] = s;
            arx_spec sp = {C->w + pr_below(&r, C->grow + 1), C->h + pr_below(&r, C->grow + 1),
                           shapes[pr_below(&r, ns)], pr_below(&r, 4) == 0 ? 1 : 0, C->maxlen, C->bars, C->turns,
                           C->deep_lo + pr_below(&r, C->deep_hi - C->deep_lo + 1)};
            if (ch == 0 && sp.w + sp.h <= 7) sp.shape = ARX_RECT;   /* (a 3 x 4 heart is no heart) */
            if (sp.shape == ARX_HEART && sp.w < 7) continue;
            arx_map m;
            if (!arx_make(&m, &sp, pr_next(&r))) continue;
            int f0, rounds = arx_rounds(&m, &f0);
            if (rounds < C->lo || rounds > C->hi || f0 * 10 > m.n * 6 || m.n < 4) continue;
            if (!has(&m, C->need, sp.shape) || met(&m)) continue;
            arx_text(&m, pool[n].text);
            pool[n].rounds = rounds;
            pool[n].n = m.n;
            pool[n].shape = sp.shape;
            pool[n].diff = rounds + 1.5 * (1.0 - (double)f0 / m.n) + m.n / 30.0;
            pool[n].look = look_of(&m, sp.shape);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = ch == 0 ? 0 : n / 10;
        double step = (double)(n - 1 - lo) / (PER - 1);
        for (int k = 0, last = -1; k < PER; k++) {
            int mid = lo + (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
            if (k == 0 && ch == 0) a = b = 0;
            if (k == PER - 1) a = n - 1 - n / 25 - 2, b = n - 1;
            if (a < lo) a = lo;
            if (a <= last) a = last + 1;
            if (b > n - 1) b = n - 1;
            if (b < a) b = a;
            for (int j = a; j <= b; j++)
                if (i < 0 || pool[j].look > pool[i].look) i = j;
            last = i;
            out[ch * PER + k] = pool[i];
        }
        fprintf(stderr, "chapter %d: %d boards, depth %d .. %d, tiles %d .. %d\n", ch + 1, n, out[ch * PER].rounds,
                out[ch * PER + PER - 1].rounds, out[ch * PER].n, out[ch * PER + PER - 1].n);
    }
    printf("const char *const arx_levels[ARX_LEVELS] = {\n");
    for (int l = 0; l < ARX_LEVELS; l++) printf("    \"%s\",   /* %d: depth %d */\n", out[l].text, l + 1, out[l].rounds);
    printf("};\n");
    return 0;
}
