/*
 * Torch Maze's levels: src/games/torchmaze_levels.h. Our own mazes, made by the rules' maker (tmz_make, from fixed
 * seeds): each chapter its things (plain mazes of every shape, keys and doors, gates and plates, one-way doors, oil and
 * the torch's fuel, then the map fading), its sizes growing along it. A maze is kept when its things matter (the doors
 * by their making; a one-way door: the way changes or an explorer gets trapped; the flasks: the fuel does not do
 * without them) and when its layout was not met before. The difficulty is measured (tmz_measure): the fewest steps,
 * and how far two sensible explorers walk who see only their torch's light (heading for the exit's glow, or for the
 * nearest unknown); trivial: they walk hardly more than the fewest steps. Each chapter keeps 25: one or two easy ones
 * first that show its new things (chapter 1: three tiny mazes), then mazes no explorer walks straight through, spread
 * from the chapter's floor to its hardest by score, another shape or size than the level before; the chapter's last,
 * its showpiece, of the hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/tmzgen apps/boardgames/tools/torchmaze_gen.c -lm && \
 *       /tmp/tmzgen > apps/boardgames/src/games/torchmaze_levels.h
 * The levels' difficulty table: the same with -DTMZ_TABLE, run with no output file.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TMZ_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/torchmaze_rules.c"

#define PER (TMZ_LEVELS / TMZ_CHAPTERS)
#define MAKES 420                               /* mazes kept a chapter, at most */
#define TRIVIAL 1.3                             /* explorers' steps over the fewest: under it, a level is trivial */

/* a chapter: sizes w0..w1 (h a little more at times), rooms, keys' colours, shut gates, plates, open gates, one-way
 * doors, flasks (slack -1: no fuel; else the fuel's slack up to it), braziers, the light, the fading (0 none; else up
 * to fade + 10); intro: its first, easy levels; floor: the least ratio of the others */
typedef struct {
    int w0, w1, rooms, col0, col1, gates, plates, opens, ow0, ow1, flasks, slack, braz, radius, fade, intro, least;
    double floor;
} chapter;

static const chapter CH[TMZ_CHAPTERS] = {
    {4, 12, 2, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 30, 0, 3, 12, 1.5},    /* corridors: every shape */
    {7, 13, 2, 1, 2, 0, 0, 0, 0, 0, 0, -1, 1, 28, 0, 1, 18, 1.5},    /* keys and doors */
    {8, 14, 2, 0, 1, 1, 1, 2, 0, 0, 0, -1, 1, 28, 0, 1, 18, 1.5},    /* gates and plates */
    {10, 15, 2, 0, 1, 0, 0, 0, 2, 4, 0, -1, 1, 27, 0, 1, 22, 1.6},    /* one-way doors */
    {8, 14, 2, 0, 1, 0, 0, 0, 0, 1, 2, 10, 0, 30, 0, 1, 20, 1.4},    /* oil: the torch's fuel */
    {11, 17, 3, 1, 2, 1, 1, 1, 0, 2, 1, 12, 2, 25, 14, 1, 30, 1.6}, /* the map fades: all of it */
};

typedef struct {
    char text[TMZ_TEXT];
    tmz_metric m;
    int w, h, shape;
} cand;

/* ---- layouts met before: the maze's walls in its 8 turns and mirrors ------------------------------------------- */
static uint32_t *kept;
static int nkept;

static int src_cell(const tmz_map *m, int tr, int x, int y)
{
    int sx = tr & 4 ? y : x, sy = tr & 4 ? x : y;
    if (tr & 1) sx = m->w - 1 - sx;
    if (tr & 2) sy = m->h - 1 - sy;
    return sx + sy * m->w;
}

static uint32_t layout_hash(const tmz_map *m, int tr)
{
    int w = tr & 4 ? m->h : m->w, h = tr & 4 ? m->w : m->h;
    uint32_t hsh = 2166136261u;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = src_cell(m, tr, x, y), v = m->in[c];
            if (x + 1 < w) {                            /* its openings right and down, as seen after the turn */
                int n = src_cell(m, tr, x + 1, y), d = dir_between(m, c, n);
                v = v * 2 + (d >= 0 && tmz_opening(m, c, d));
            }
            if (y + 1 < h) {
                int n = src_cell(m, tr, x, y + 1), d = dir_between(m, c, n);
                v = v * 2 + (d >= 0 && tmz_opening(m, c, d));
            }
            hsh = (hsh ^ (uint32_t)v) * 16777619u;
        }
    return hsh ^ (uint32_t)(w * 131 + h);
}

static int met(const tmz_map *m)
{
    uint32_t hs[8];
    for (int tr = 0; tr < 8; tr++) hs[tr] = layout_hash(m, tr);
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 8; tr++)
            if (kept[i] == hs[tr]) return 1;
    kept[nkept++] = hs[0];
    return 0;
}

/* the chapter's things matter */
static int matters(const tmz_map *m, const tmz_metric *mt, int ch)
{
    if (ch == 3) {                              /* one-way doors: made two-way, the way shorter, or a trap */
        tmz_map o = *m;
        for (int s = 0; s < 2; s++)
            for (int c = 0; c < o.w * o.h; c++)
                if (o.e[s][c] == TMZ_ONEWAY) o.e[s][c] = TMZ_OPEN;
        return best_of(&o) != mt->best || mt->naive[0] < 0 || mt->naive[1] < 0;
    }
    if (ch == 4 && m->nflask) {                 /* the flasks: without them, the fuel does not do */
        tmz_map o = *m;
        for (int c = 0; c < o.w * o.h; c++)
            if (o.item[c] == TMZ_OIL) o.item[c] = TMZ_NONE;
        o.nflask = 0;
        return best_of(&o) < 0;
    }
    if (ch == 2) {                              /* the gates: none shut on the way at first, the way shorter */
        tmz_map o = *m;
        for (int s = 0; s < 2; s++)
            for (int c = 0; c < o.w * o.h; c++)
                if (o.e[s][c] == TMZ_GATE) o.e[s][c] = TMZ_OPEN;
        return best_of(&o) < mt->best;
    }
    return 1;
}

static void spec_of(const chapter *C, int ch, int k, uint32_t *r, tmz_spec *sp)
{
    memset(sp, 0, sizeof *sp);
    /* the size grows along the candidates (k: 0..MAKES), with a spread */
    int span = C->w1 - C->w0, w = C->w0 + (span * k) / MAKES + pr_below(r, 3) - 1;
    if (w < C->w0) w = C->w0;
    if (w > C->w1) w = C->w1;
    int h = w + pr_below(r, 4) - 1;
    if (h < 4) h = 4;
    if (h > TMZ_MAXH) h = TMZ_MAXH;
    sp->w = w;
    sp->h = h;
    sp->shape = w >= 7 ? pr_below(r, 7) : 0;
    if (sp->shape == 4 && w < 9) sp->shape = 1;
    sp->newest = 25 + pr_below(r, 70);
    sp->braid = ch == 0 && k < 40 ? 0 : pr_below(r, 35);
    sp->rooms = pr_below(r, C->rooms + 1);
    sp->colours = C->col0 + pr_below(r, C->col1 - C->col0 + 1);
    sp->gates = C->gates;
    sp->plates = C->plates ? 1 + (pr_below(r, 3) == 0) : 0;
    sp->opens = C->opens ? pr_below(r, C->opens + 1) : 0;
    sp->oneways = C->ow0 + pr_below(r, C->ow1 - C->ow0 + 1);
    sp->flasks = C->flasks ? 1 + pr_below(r, C->flasks) : 0;
    sp->slack = C->slack < 0 ? -1 : pr_below(r, C->slack + 1);
    sp->braziers = C->braz ? pr_below(r, C->braz + 1) + (w >= 13) : 0;
    sp->radius = C->radius;
    sp->fade = C->fade ? C->fade + pr_below(r, 11) : 0;
    sp->tries = 6;
    (void)ch;
}

static int by_score(const void *a, const void *b)
{
    double x = ((const cand *)a)->m.score, y = ((const cand *)b)->m.score;
    return x < y ? -1 : x > y;
}

#ifdef TMZ_TABLE                                /* the shipped levels' table: -DTMZ_TABLE */
#include "games/torchmaze_levels.h"
int main(void)
{
    (void)CH, (void)by_score, (void)met, (void)matters, (void)spec_of;
    double sum[TMZ_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl  w  h best naive0 naive1 ratio  score dead\n");
    for (int lv = 0; lv < TMZ_LEVELS; lv++) {
        tmz_map m;
        tmz_metric mt;
        tmz_parse(&m, tmz_levels[lv]);
        tmz_measure(&m, &mt);
        int triv = mt.ratio < TRIVIAL;
        printf("%3d %2d %2d %4d %6d %6d %5.2f %6.1f %4d%s\n", lv + 1, m.w, m.h, mt.best, mt.naive[0], mt.naive[1],
               mt.ratio, mt.score, mt.deadends, triv ? "  trivial" : "");
        double *s = sum[lv / PER];
        s[0] += mt.best; s[1] += mt.ratio; s[2] += mt.score; s[3] += triv;
        s[4] += (mt.naive[0] < 0) + (mt.naive[1] < 0);
        trivial += lv >= 3 && triv;
    }
    printf("\nchapter  best ratio score trivial explorers_failed\n");
    for (int c = 0; c < TMZ_CHAPTERS; c++)
        printf("%7d %5.1f %5.2f %5.1f %4.0f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3], sum[c][4]);
    printf("trivial levels from level 4 (ratio < %.1f): %d\n", TRIVIAL, trivial);
    return 0;
}
#else
/* a chapter's levels from its pool (sorted by score): its first `intro` from the easy end, the others spread over the
 * mazes no explorer walks straight through, the last of the hardest few; another shape or size than the level before
 * when it can */
static int pick(cand *pool, int n, const chapter *C, int *chosen)
{
    static int hard[MAKES];
    int nh = 0, used[MAKES] = {0};
    for (int i = 0; i < n; i++)
        if (pool[i].m.ratio >= C->floor && pool[i].m.best >= C->least) hard[nh++] = i;
    if (nh < PER - C->intro + 2) { fprintf(stderr, "only %d hard mazes\n", nh); return 0; }
    int lw = 0, lh = 0, ls = -1;
    for (int k = 0; k < C->intro; k++) {        /* the easiest few, growing */
        int i = -1;
        for (int j = 0; j < n && i < 0; j++)
            if (!used[j] && pool[j].w > lw) i = j;
        if (i < 0) i = k;
        used[i] = 1;
        chosen[k] = i;
        lw = pool[i].w, lh = pool[i].h, ls = pool[i].shape;
    }
    double step = (double)(nh - 1) / (PER - C->intro - 1);
    int last = -1;
    for (int k = 0; k < PER - C->intro; k++) {
        int mid = (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
        if (k == PER - C->intro - 1) a = nh - 1 - nh / 20 - 2, b = nh - 1;   /* the showpiece */
        if (a <= last) a = last + 1;
        if (b > nh - 1) b = nh - 1;
        if (b < a) b = a;
        double best = -1e9;
        for (int j = a; j <= b; j++) {
            const cand *c = &pool[hard[j]];
            if (used[hard[j]]) continue;
            double v = -(c->w == lw && c->h == lh ? 1 : 0) - (c->shape == ls ? 1 : 0) + c->m.ratio * 0.1;
            if (i < 0 || v > best) { best = v; i = j; }
        }
        if (i < 0) for (int j = a; j < nh && i < 0; j++) if (!used[hard[j]]) i = j;
        if (i < 0) return 0;
        last = i;
        used[hard[i]] = 1;
        chosen[C->intro + k] = hard[i];
        lw = pool[hard[i]].w, lh = pool[hard[i]].h, ls = pool[hard[i]].shape;
    }
    return 1;
}

int main(void)
{
    kept = malloc(sizeof *kept * TMZ_CHAPTERS * MAKES * 8);
    static cand pool[MAKES];
    static char out[TMZ_LEVELS][TMZ_TEXT];
    static int outbest[TMZ_LEVELS];
    printf("/* Torch Maze's levels: made by tools/torchmaze_gen.c (our own mazes, found by its search; do not edit).\n"
           " * Each maze as text (torchmaze.h), and its best: the fewest steps knowing the whole maze. */\n");
    for (int ch = 0; ch < TMZ_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(7300u + (uint32_t)ch);
        int n = 0, made = 0;
        for (int k = 0; k < 30 * MAKES && n < MAKES; k++) {
            tmz_spec sp;
            spec_of(C, ch, n, &r, &sp);
            tmz_map m;
            if (!tmz_make(&m, &sp, pr_next(&r))) continue;
            made++;
            if (m.fuel) {                       /* the fuel's slack: a share of the way, more or less */
                int least = m.fuel - sp.slack, b = best_of(&m);
                m.fuel = least + (b * pr_below(&r, ch == 4 ? 45 : 25)) / 100;
            }
            if (met(&m)) continue;
            tmz_metric mt;
            tmz_measure(&m, &mt);
            if (mt.best < 3 || !matters(&m, &mt, ch)) continue;
            tmz_text(&m, pool[n].text);
            pool[n].m = mt;
            pool[n].w = m.w;
            pool[n].h = m.h;
            pool[n].shape = sp.shape;
            n++;
        }
        qsort(pool, (size_t)n, sizeof *pool, by_score);
        int chosen[PER];
        if (n < PER || !pick(pool, n, C, chosen)) {
            fprintf(stderr, "chapter %d: only %d mazes (%d made)\n", ch + 1, n, made);
            return 1;
        }
        for (int k = 0; k < PER; k++) {
            strcpy(out[ch * PER + k], pool[chosen[k]].text);
            outbest[ch * PER + k] = pool[chosen[k]].m.best;
        }
        fprintf(stderr, "chapter %d: %d mazes of %d, best %d .. %d\n", ch + 1, n, made, outbest[ch * PER],
                outbest[ch * PER + PER - 1]);
    }
    printf("const char *const tmz_levels[TMZ_LEVELS] = {\n");
    for (int l = 0; l < TMZ_LEVELS; l++) printf("    \"%s\",\n", out[l]);
    printf("};\n\nconst uint16_t tmz_level_best[TMZ_LEVELS] = {");
    for (int l = 0; l < TMZ_LEVELS; l++) printf("%s%d,", l % 25 ? " " : "\n    ", outbest[l]);
    printf("\n};\n");
    return 0;
}
#endif
