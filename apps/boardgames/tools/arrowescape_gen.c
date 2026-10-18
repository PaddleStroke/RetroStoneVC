/*
 * Clear the Arrows' levels: src/games/arrowescape_levels.h. Our own boards: no published or commercial set is used.
 * Each chapter has its idea (single squares on small boards; long bars; turnstiles; bigger boards; shapes: hearts,
 * diamonds, rounds; all of it on the largest boards) and makes many boards with the rules' maker (arx_make, from fixed
 * seeds: tiles laid backwards in mirrored or half-turned pairs over the chapter's shapes). A board is kept when it can
 * be cleared, when its tiles are not all free at the start, when its depth (the rounds of taking every free tile at
 * once) is in the chapter's range, when it has the chapter's feature, and when it was not met before (mirrored
 * either). The difficulty is measured (measure()): the depth, the share of tiles blocked at the start, the tiles,
 * and the mistakes of a near-sighted player (tapping tiles whose next square is clear). Each chapter keeps 25: up to
 * three easy ones first (chapter 1 from the very easiest) that show its idea, then boards three rounds deep or more
 * that the near-sighted player gets wrong, spread from the chapter's floor to its hardest; of the boards near each
 * place on the curve the one that looks best is kept (a shape, bars and turnstiles in the chapters that have them,
 * another size than the level before); the chapter's last level, its showpiece, the best looking of its hardest few.
 * Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/arxgen apps/boardgames/tools/arrowescape_gen.c -lm && \
 *       /tmp/arxgen > apps/boardgames/src/games/arrowescape_levels.h
 * The levels' difficulty table: the same with -DARX_TABLE, run with no output file.
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

typedef struct { int rounds, n, decoys; double free, naive, score; } metric;   /* measure() */

typedef struct {
    char text[ARX_TEXT];
    int rounds, n, shape, w, h;
    double diff, look;
    metric m;
} cand;

/* a chapter: the sizes w x h (up to grow more each way), its shapes (bits), the longest bar, bars %, turnstiles %,
 * how deep the maker digs (lo..hi), the depth's range; need: 1 bars, 2 turnstiles, 3 a shape not a rectangle;
 * intro: its first levels, easy ones that show its idea (chapter 1 from the very easiest); naive, floor: the least
 * mistakes of the near-sighted player (measure()) and the least score of the others */
typedef struct {
    int w, h, grow, shapes, maxlen, bars, turns, deep_lo, deep_hi, lo, hi, need, intro, from_easiest;
    double naive, floor;
} chapter;

#define S(x) (1 << (x))
static const chapter CH[ARX_CHAPTERS] = {
    {3, 3, 3, S(ARX_RECT) | S(ARX_DIAMOND) | S(ARX_PLUS) | S(ARX_ROUND), 1, 0, 0, 0, 80, 2, 6, 0, 3, 1, 1.2, 7},
    {5, 5, 2, S(ARX_RECT) | S(ARX_OCTAGON) | S(ARX_PLUS) | S(ARX_DIAMOND), 3, 55, 0, 30, 90, 3, 7, 1, 1, 0, 1.5, 8},
    {5, 5, 2, S(ARX_RECT) | S(ARX_OCTAGON) | S(ARX_ROUND), 3, 30, 22, 40, 90, 3, 7, 2, 2, 0, 2, 9},
    {8, 8, 2, S(ARX_RECT) | S(ARX_OCTAGON) | S(ARX_ROUND) | S(ARX_DIAMOND), 3, 40, 0, 50, 100, 5, 11, 1, 0, 0, 3, 15},
    {8, 9, 2, S(ARX_HEART) | S(ARX_DIAMOND) | S(ARX_ROUND) | S(ARX_BLOB), 4, 40, 0, 60, 100, 6, 12, 3, 1, 0, 3.5, 17},
    {9, 10, 2, S(ARX_HEART) | S(ARX_OCTAGON) | S(ARX_ROUND) | S(ARX_BLOB) | S(ARX_RECT), 4, 40, 8, 70, 100, 7, 14, 1,
     0, 0, 5, 24},
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

/* ---- how hard a board is, measured ------------------------------------------------------------------------------
 * free: the share of the tiles free at the start; decoys: the tiles at the start whose next square is clear but whose
 * way out is not; naive: the mistakes of a near-sighted player, on average over 200 games (each tap on a tile whose
 * next square is clear, at random, never twice on a tile until another has left) */
static void measure(const arx_map *m, metric *mt)
{
    int f0;
    mt->rounds = arx_rounds(m, &f0);
    mt->n = m->n;
    mt->free = (double)f0 / m->n;
    arx_game g;
    g.m = *m;
    arx_begin(&g);
    mt->decoys = 0;
    for (int i = 0; i < m->n; i++) {
        int gap = 0;
        mt->decoys += !arx_free(&g, i) && (arx_blocker(&g, i, &gap) < 0 || gap > 0);
    }
    uint32_t r = pr_seed(77);
    long bumps = 0;
    for (int t = 0; t < 200; t++) {
        arx_game p = g;
        uint8_t tried[ARX_MAXP] = {0};
        for (int k = 0; k < 6 * m->n && !arx_solved(&p); k++) {
            int look[ARX_MAXP], nl = 0, any[ARX_MAXP], na = 0;
            for (int i = 0; i < m->n; i++) {
                if (p.gone[i] || tried[i]) continue;
                any[na++] = i;
                int gap = 0;
                if (arx_blocker(&p, i, &gap) < 0 || gap > 0) look[nl++] = i;
            }
            if (!na) break;
            int i = nl ? look[pr_below(&r, nl)] : any[pr_below(&r, na)], left = p.left;
            arx_tap(&p, i, NULL);
            if (p.left != left) memset(tried, 0, sizeof tried);
            else tried[i] = 1;
        }
        bumps += p.mistakes;
    }
    mt->naive = bumps / 200.0;
    /* the score: the depth, the near-sighted player's mistakes, the tiles blocked at the start, the tiles */
    mt->score = mt->rounds + 1.5 * mt->naive + 2 * (1 - mt->free) + m->n / 20.0;
}

#ifdef ARX_TABLE                                /* the shipped levels' table: -DARX_TABLE */
#include "games/arrowescape_levels.h"
int main(void)
{
    (void)CH, (void)by_diff, (void)look_of, (void)met, (void)has;
    double sum[ARX_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl depth tiles free decoys naive score  board\n");
    for (int lv = 0; lv < ARX_LEVELS; lv++) {
        arx_map m;
        metric mt;
        arx_parse(&m, arx_levels[lv]);
        measure(&m, &mt);
        printf("%4d %3d %4d %5.2f %3d %5.2f %6.2f  %s\n", lv + 1, mt.rounds, mt.n, mt.free, mt.decoys, mt.naive,
               mt.score, arx_levels[lv]);
        int triv = mt.naive < 1 || mt.rounds <= 2;
        double *s = sum[lv / PER];
        s[0] += mt.rounds; s[1] += mt.n; s[2] += mt.naive; s[3] += mt.score; s[4] += triv;
        trivial += lv >= 3 && triv;
    }
    printf("\nchapter depth tiles naive score trivial\n");
    for (int c = 0; c < ARX_CHAPTERS; c++)
        printf("%7d %5.1f %5.1f %5.2f %5.1f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3] / PER, sum[c][4]);
    printf("trivial levels from level 4 (near-sighted mistakes < 1 or depth <= 2): %d\n", trivial);
    return 0;
}
#else
/* a chapter's levels from its pool (sorted by score): its first `intro` from the easy end (the chapter's idea shown
 * plainly), the others spread from `floor` to the hardest over the boards a near-sighted player gets wrong, the last
 * of the hardest few; of the boards near each place on the curve the best looking, another size than the last
 * level's when it can */
static int pick(const cand *pool, int n, const chapter *C, int *chosen)
{
    static int hard[MAKES];
    int nh = 0, used[MAKES] = {0};
    for (int i = 0; i < n; i++)
        if (pool[i].m.naive >= C->naive && pool[i].m.rounds >= 3 && pool[i].m.score >= C->floor) hard[nh++] = i;
    fprintf(stderr, "  %d hard boards\n", nh);
    if (nh < PER - C->intro + 2) return 0;
    int lw = 0, lh = 0, last = -1;
    for (int k = 0; k < C->intro; k++) {
        int span = n / 5 > C->intro ? n / 5 : C->intro, i = -1;
        int a = k * span / C->intro, b = (k + 1) * span / C->intro - 1;
        if (C->from_easiest && k == 0) b = a + 1;
        if (a <= last) a = last + 1;
        if (b < a) b = a;
        for (int j = a; j <= b && j < n; j++)
            if (i < 0 || pool[j].look > pool[i].look) i = j;
        used[i] = 1;
        last = i;
        chosen[k] = i;
        lw = pool[i].w, lh = pool[i].h;
    }
    double step = (double)(nh - 1) / (PER - C->intro - 1);
    last = -1;
    for (int k = 0; k < PER - C->intro; k++) {
        int mid = (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
        if (k == PER - C->intro - 1) a = nh - 1 - nh / 25 - 2, b = nh - 1;   /* the showpiece: of the hardest few */
        if (a <= last) a = last + 1;
        if (b > nh - 1) b = nh - 1;
        if (b < a) b = a;
        double best = -1e9;
        for (int j = a; j <= b; j++) {
            const cand *c = &pool[hard[j]];
            double v = c->look - (c->w == lw && c->h == lh ? 0.3 : 0);
            if (!used[hard[j]] && (i < 0 || v > best)) { best = v; i = j; }
        }
        if (i < 0) i = a;
        last = i;
        used[hard[i]] = 1;
        chosen[C->intro + k] = hard[i];
        lw = pool[hard[i]].w, lh = pool[hard[i]].h;
    }
    return 1;
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
            arx_map back;                       /* measured as the level will read it (its tiles in board order) */
            arx_parse(&back, pool[n].text);
            measure(&back, &pool[n].m);
            pool[n].diff = pool[n].m.score;
            pool[n].w = m.w;
            pool[n].h = m.h;
            pool[n].look = look_of(&m, sp.shape);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int chosen[PER];
        if (!pick(pool, n, C, chosen)) { fprintf(stderr, "chapter %d: too few hard boards\n", ch + 1); return 1; }
        for (int k = 0; k < PER; k++) out[ch * PER + k] = pool[chosen[k]];
        fprintf(stderr, "chapter %d: %d boards, depth %d .. %d, tiles %d .. %d\n", ch + 1, n, out[ch * PER].rounds,
                out[ch * PER + PER - 1].rounds, out[ch * PER].n, out[ch * PER + PER - 1].n);
    }
    printf("const char *const arx_levels[ARX_LEVELS] = {\n");
    for (int l = 0; l < ARX_LEVELS; l++) printf("    \"%s\",   /* %d: depth %d */\n", out[l].text, l + 1, out[l].rounds);
    printf("};\n");
    return 0;
}
#endif
