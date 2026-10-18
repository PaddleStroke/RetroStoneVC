/*
 * Tilt Maze's levels: src/games/tiltmaze_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of tray (its size, its marbles and special tiles: one colour, two colours, gates, one-way
 * tiles, lidded holes, all of them) and makes many with the rules' maker (tlm_make, from fixed seeds: walls laid with
 * a symmetry, then walls, holes and marbles changed one at a time while the fewest tilts grow; each one aimed at a
 * par spread over the chapter's range). A tray is kept when its special tiles all matter (tlm_make checks it) and
 * when it was not met before (nor its mirror). The look, for a hand-laid feel: walls in a mirror or half-turn
 * symmetry (a few trays left free), neither too few nor too many, the marbles and the holes spread over the tray; of
 * the trays near each place on the curve the best-looking one is kept, the chapter's last tray (its showpiece) the
 * best-looking of its hardest few. The difficulty: the fewest tilts, and a little for the states the search went
 * through. Chapter 1 starts from the very easiest (a first look). Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/tlmgen apps/boardgames/tools/tiltmaze_gen.c -lm && \
 *       /tmp/tlmgen > apps/boardgames/src/games/tiltmaze_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TLM_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/tiltmaze_rules.c"

#define PER (TLM_LEVELS / TLM_CHAPTERS)
#define TEXT (3 * TLM_CELLS)
#define MAXC 1000

typedef struct {
    char text[TEXT];
    int par, states;
    double diff, look;
} cand;

/* a chapter: its sizes (w x h, up to grow more each way), walls, blocks, marbles a colour (a second set drawn now and
 * then: alt), special tiles; lo..hi the pars aimed at */
typedef struct {
    int w, h, grow, walls, blocks, marbles[TLM_COLOURS], alt[TLM_COLOURS], gates, oneway, lids, lo, hi, iters, makes;
} chapter;

static const chapter CH[TLM_CHAPTERS] = {
    {5, 5, 1, 18, 1, {1, 0, 0, 0}, {2, 0, 0, 0}, 0, 0, 0, 2, 9, 80, 400},          /* one colour */
    {6, 6, 1, 18, 1, {1, 1, 0, 0}, {2, 1, 0, 0}, 0, 0, 0, 6, 13, 100, 360},        /* two colours */
    {6, 7, 1, 16, 1, {1, 1, 0, 0}, {2, 1, 0, 0}, 2, 0, 0, 7, 14, 100, 360},        /* gates */
    {6, 7, 1, 16, 1, {1, 1, 0, 0}, {1, 1, 1, 0}, 0, 3, 0, 7, 15, 100, 360},        /* one-way tiles */
    {6, 7, 1, 16, 1, {2, 1, 0, 0}, {2, 2, 0, 0}, 0, 0, 1, 8, 16, 100, 360},        /* lidded holes */
    {7, 8, 1, 16, 2, {2, 1, 1, 0}, {1, 1, 1, 1}, 1, 2, 1, 10, 20, 100, 360},       /* all of them */
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

static char (*kept)[TEXT];
static int nkept;

static void mirror_map(const tlm_map *m, tlm_map *o)   /* left and right swapped */
{
    *o = *m;
    int w = m->w;
    for (int c = 0; c < w * m->h; c++) {
        int x = c % w, y = c / w, d = (w - 1 - x) + y * w;
        o->t[d] = m->t[c];
        o->k[d] = m->k[c];
        if (m->t[c] == TLM_ONEWAY && (m->k[c] == TLM_LEFT || m->k[c] == TLM_RIGHT)) o->k[d] = (uint8_t)(4 - m->k[c]);
        o->wall[d] = m->wall[c] & TLM_WALL_D;
    }
    for (int c = 0; c < w * m->h; c++) {
        int x = c % w, y = c / w;
        if (x < w - 1 && (m->wall[c] & TLM_WALL_R)) o->wall[(w - 2 - x) + y * w] |= TLM_WALL_R;
    }
    for (int i = 0; i < m->nm; i++) o->start[i] = (uint8_t)((w - 1 - m->start[i] % w) + m->start[i] / w * w);
    sort_marbles(o);
}

static int met(const tlm_map *m, const char *t)
{
    char mir[TEXT];
    tlm_map o;
    mirror_map(m, &o);
    tlm_text(&o, mir);
    for (int i = 0; i < nkept; i++)
        if (!strcmp(kept[i], t) || !strcmp(kept[i], mir)) return 1;
    strcpy(kept[nkept++], t);
    return 0;
}

/* how good a tray looks: its walls' symmetry, a fair share of walls, the marbles and holes spread out */
static double look_of(const tlm_map *m)
{
    int w = m->w, h = m->h, cells = w * h, edges = (w - 1) * h + w * (h - 1), walls = 0, best = 0;
    for (int c = 0; c < cells; c++) walls += !!(m->wall[c] & TLM_WALL_R) + !!(m->wall[c] & TLM_WALL_D);
    for (int sym = 1; sym <= 3; sym++) {
        int same = 0;
        for (int c = 0; c < cells; c++)
            for (int d = TLM_RIGHT; d <= TLM_DOWN; d++) {
                int b = c + DX[d] + DY[d] * w;
                if ((d == TLM_RIGHT && c % w == w - 1) || (d == TLM_DOWN && c / w == h - 1)) continue;
                int on = !!has_wall(m, c, b), im = !!has_wall(m, image(w, h, sym, c), image(w, h, sym, b));
                same += on && im;
            }
        if (same > best) best = same;
    }
    double sym = walls ? (double)best / walls : 1, wf = (double)walls / edges;
    double look = 3 * sym - 8 * fabs(wf - 0.22);
    int pts[2 * TLM_MAXM], n = 0;
    for (int i = 0; i < m->nm; i++) pts[n++] = m->start[i];
    for (int c = 0; c < cells; c++)
        if (m->t[c] == TLM_HOLE || m->t[c] == TLM_LID) pts[n++] = c;
    double spread = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) spread += abs(pts[i] % w - pts[j] % w) + abs(pts[i] / w - pts[j] / w);
    if (n > 1) look += 1.2 * spread / (n * (n - 1) / 2.0) / (w + h);
    int bare = 0;                               /* rows or columns with no wall across them at all: bare */
    for (int x = 0; x < w - 1; x++) {
        int any = 0;
        for (int y = 0; y < h; y++) any |= m->wall[x + y * w] & TLM_WALL_R;
        bare += !any;
    }
    look -= 0.3 * bare;
    for (int c = 0; c < cells; c++) look -= m->t[c] == TLM_BLOCK ? 0.12 : 0;   /* blocks: a few */
    return look;
}

int main(void)
{
    kept = malloc(sizeof *kept * 6000);
    cand *c = malloc(sizeof *c * MAXC);
    if (!kept || !c) return 1;
    printf("/* Tilt Maze's levels: made by tools/tiltmaze_gen.c (our own trays, found by its search; do not edit).\n"
           " * Each tray's rows from the top split by '/' (tiltmaze.h), and its par: the fewest tilts. */\n");
    static char out[TLM_LEVELS][TEXT];
    static int outpar[TLM_LEVELS];
    for (int ch = 0; ch < TLM_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        int n = 0;
        for (int k = 0; k < C->makes; k++) {
            uint32_t seed = 2000003u * (uint32_t)(ch + 1) + 6151u * (uint32_t)k;
            uint32_t rs = pr_seed(seed ^ 0x5eedu);
            tlm_spec sp = {C->w, C->h, -1, C->walls, C->blocks, {0}, C->gates, C->oneway, C->lids, 0, 0, C->iters, 0};
            int big = k * (C->grow + 1) / C->makes;
            sp.w += big;
            sp.h += big + (pr_below(&rs, 3) == 0);
            if (pr_below(&rs, 6) == 0) sp.sym = 0;
            const int *mb = pr_below(&rs, 2) ? C->alt : C->marbles;
            if (ch == 0) mb = k < C->makes / 2 ? C->marbles : C->alt;
            memcpy(sp.marbles, mb, sizeof sp.marbles);
            sp.lo = sp.hi = C->lo + k * (C->hi - C->lo + 1) / C->makes;
            tlm_map m;
            if (!tlm_make(&m, &sp, seed)) continue;
            int states, par = par_of(&m, &states);
            if (par <= 0) continue;
            cand *x = &c[n];
            tlm_text(&m, x->text);
            if (met(&m, x->text)) continue;
            x->par = par;
            x->states = states;
            x->diff = par + 0.5 * log((double)states + 1);
            x->look = look_of(&m);
            if (++n >= MAXC) break;
        }
        qsort(c, (size_t)n, sizeof *c, by_diff);
        fprintf(stderr, "chapter %d: %d trays, par %d..%d\n", ch + 1, n, n ? c[0].par : 0, n ? c[n - 1].par : 0);
        if (n < PER) { fprintf(stderr, "too few\n"); return 1; }
        int used[MAXC] = {0}, prev = -1;
        for (int i = 0; i < PER; i++) {
            int at = ch == 0 && i == 0 ? 0 : (int)((double)i * (n - 1) / (PER - 1) + 0.5), pick = -1;
            int win = ch == 0 && i < 3 ? 1 : n / (PER * 2) + 1;
            if (i == PER - 1) { at = n - 1; win = n / 8 + 1; }
            double best = -1e9;
            for (int j = at - win; j <= at + win; j++) {
                if (j <= prev || j < 0 || j >= n || used[j]) continue;
                if (c[j].look > best) { best = c[j].look; pick = j; }
            }
            if (pick < 0)
                for (int j = prev + 1; j < n && pick < 0; j++)
                    if (!used[j]) pick = j;
            if (pick < 0) { fprintf(stderr, "chapter %d: ran out\n", ch + 1); return 1; }
            used[pick] = 1;
            prev = pick;
            strcpy(out[ch * PER + i], c[pick].text);
            outpar[ch * PER + i] = c[pick].par;
        }
    }
    printf("const char *const tlm_levels[TLM_LEVELS] = {\n");
    for (int i = 0; i < TLM_LEVELS; i++) printf("    \"%s\",\n", out[i]);
    printf("};\n\nconst uint8_t tlm_level_par[TLM_LEVELS] = {");
    for (int i = 0; i < TLM_LEVELS; i++) printf("%s%d,", i % 25 ? " " : "\n    ", outpar[i]);
    printf("\n};\n");
    for (int ch = 0; ch < TLM_CHAPTERS; ch++) {
        fprintf(stderr, "ch%d pars:", ch + 1);
        for (int i = 0; i < PER; i++) fprintf(stderr, " %d", outpar[ch * PER + i]);
        fprintf(stderr, "\n");
    }
    free(kept);
    free(c);
    return 0;
}
