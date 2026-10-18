/*
 * Tilt Maze's levels: src/games/tiltmaze_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of tray (its size, its marbles and special tiles: one colour, two colours, gates, one-way
 * tiles, lidded holes, all of them) and makes many with the rules' maker (tlm_make, from fixed seeds: walls laid with
 * a symmetry, then walls, holes and marbles changed one at a time while the fewest tilts grow; each one aimed at a
 * par spread over the chapter's range). A tray is kept when its special tiles all matter (tlm_make checks it) and
 * when it was not met before (nor its mirror). The look, for a hand-laid feel: walls in a mirror or half-turn
 * symmetry (a few trays left free), neither too few nor too many, the marbles and the holes spread over the tray.
 * The difficulty is measured (measure()): the fewest tilts, the share of mistaken tilts along the way, and how many
 * naive players (tilting at random, mostly sinking a marble when they can) finish it. Each chapter keeps 25: one to
 * three easy ones first (chapter 1 from the very easiest) that show its new tiles, then trays no naive player
 * finishes, spread from the chapter's floor to its hardest; of the trays near each place on the curve the
 * best-looking one is kept (another size than the level before when it can), the chapter's last tray (its showpiece)
 * the best-looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/tlmgen apps/boardgames/tools/tiltmaze_gen.c -lm && \
 *       /tmp/tlmgen > apps/boardgames/src/games/tiltmaze_levels.h
 * The levels' difficulty table: the same with -DTLM_TABLE, run with no output file.
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

typedef struct { int par, states; double trap, first, naive, score; } metric;   /* measure() */

typedef struct {
    char text[TEXT];
    int par, states, w, h;
    double diff, look;
    metric m;
} cand;

/* a chapter: its sizes (w x h, up to grow more each way), walls, blocks, marbles a colour (a second set drawn now and
 * then: alt), special tiles; lo..hi the pars aimed at; intro: its first levels, easy ones that show its new tiles
 * (chapter 1 from the very easiest); floor: the least score (measure()) of the others */
typedef struct {
    int w, h, grow, walls, blocks, marbles[TLM_COLOURS], alt[TLM_COLOURS], gates, oneway, lids, lo, hi, iters, makes;
    int intro, from_easiest;
    double floor;
} chapter;

static const chapter CH[TLM_CHAPTERS] = {
    {5, 5, 1, 18, 1, {1, 0, 0, 0}, {2, 0, 0, 0}, 0, 0, 0, 2, 11, 80, 520, 3, 1, 8}, /* one colour */
    {6, 6, 1, 18, 1, {1, 1, 0, 0}, {2, 1, 0, 0}, 0, 0, 0, 7, 15, 100, 420, 1, 0, 13}, /* two colours */
    {6, 7, 1, 16, 1, {1, 1, 0, 0}, {2, 1, 0, 0}, 2, 0, 0, 7, 16, 100, 420, 2, 0, 15}, /* gates */
    {6, 7, 1, 16, 1, {1, 1, 0, 0}, {1, 1, 1, 0}, 0, 3, 0, 8, 17, 100, 420, 2, 0, 16}, /* one-way tiles */
    {6, 7, 1, 16, 1, {2, 1, 0, 0}, {2, 2, 0, 0}, 0, 0, 1, 9, 18, 100, 420, 2, 0, 17}, /* lidded holes */
    {7, 8, 1, 16, 2, {2, 1, 1, 0}, {1, 1, 1, 1}, 1, 2, 1, 12, 22, 100, 420, 1, 0, 23}, /* all of them */
};
#define NAIVE_MAX 0.05                          /* the most naive players a hard level may let win */

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

/* ---- how hard a tray is, measured -------------------------------------------------------------------------------
 * trap: along a shortest way, the share of the tilts that are mistakes (they do not bring the end a tilt nearer, or
 * lose it); first: the same for the first tilt; naive: the share of 300 naive players who finish it within one and a
 * half times its par (each tilt at random among those that sink no marble in a wrong hole, but three times in four
 * one that sinks a marble when there is one) */
static int sunk(const tlm_state *s) { return __builtin_popcount(s->in); }

static void measure(const tlm_map *m, metric *mt)
{
    tlm_game g;
    g.m = *m;
    tlm_begin(&g);
    tlm_state s = g.s;
    mt->par = tlm_solve(m, &s, NULL, &mt->states);
    int legal = 0, bad = 0, legal0 = 0, bad0 = 0;
    tlm_state c = s;
    for (int left = mt->par; left > 0; left--) {
        int dir = -1;
        tlm_solve(m, &c, &dir, NULL);
        for (int k = 0; k < 4; k++) {
            tlm_state n;
            if (!tlm_try(m, &c, k, &n, NULL)) continue;
            int mis = tlm_dead(&n) || tlm_solve(m, &n, NULL, NULL) != left - 1;
            legal++;
            bad += mis;
            if (left == mt->par) { legal0++; bad0 += mis; }
        }
        tlm_try(m, &c, dir, &c, NULL);
    }
    mt->trap = legal ? (double)bad / legal : 0;
    mt->first = legal0 ? (double)bad0 / legal0 : 0;
    uint32_t r = pr_seed(77);
    int wins = 0;
    for (int t = 0; t < 300; t++) {
        tlm_state p = s;
        for (int k = 0; k < mt->par + mt->par / 2 + 1 && !tlm_solved(m, &p); k++) {
            int ok[4], nok = 0, good[4], ngood = 0;
            tlm_state n[4];
            for (int d = 0; d < 4; d++) {
                if (!tlm_try(m, &p, d, &n[d], NULL) || tlm_dead(&n[d])) continue;
                ok[nok++] = d;
                if (sunk(&n[d]) > sunk(&p)) good[ngood++] = d;
            }
            if (!nok) break;
            p = n[ngood && pr_below(&r, 4) ? good[pr_below(&r, ngood)] : ok[pr_below(&r, nok)]];
        }
        wins += tlm_solved(m, &p);
    }
    mt->naive = wins / 300.0;
    /* the score: the tilts, worth more where the mistakes are many and the naive players fail */
    mt->score = mt->par * (0.6 + mt->trap) * (1.2 - mt->naive) + 0.3 * log((double)mt->states + 1);
}

#ifdef TLM_TABLE                                /* the shipped levels' table: -DTLM_TABLE */
#include "games/tiltmaze_levels.h"
int main(void)
{
    (void)CH, (void)by_diff, (void)look_of, (void)met;
    double sum[TLM_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl par states trap first naive score  tray\n");
    for (int lv = 0; lv < TLM_LEVELS; lv++) {
        tlm_map m;
        metric mt;
        tlm_parse(&m, tlm_levels[lv]);
        measure(&m, &mt);
        printf("%4d %3d %6d %5.2f %5.2f %5.2f %6.2f  %s\n", lv + 1, mt.par, mt.states, mt.trap, mt.first, mt.naive,
               mt.score, tlm_levels[lv]);
        int triv = mt.naive > 0.1 || mt.par <= 3;
        double *s = sum[lv / PER];
        s[0] += mt.par; s[1] += mt.trap; s[2] += mt.naive; s[3] += mt.score; s[4] += triv;
        trivial += lv >= 3 && triv;
    }
    printf("\nchapter  par  trap naive score trivial\n");
    for (int c = 0; c < TLM_CHAPTERS; c++)
        printf("%7d %5.1f %5.2f %5.2f %5.1f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3] / PER, sum[c][4]);
    printf("trivial levels from level 4 (naive > 0.1 or par <= 3): %d\n", trivial);
    return 0;
}
#else
/* a chapter's levels from its trays (sorted by score): its first `intro` from the easy end (the chapter's new tiles
 * shown plainly), the others spread from `floor` to the hardest over the trays no naive player solves, the last of
 * the hardest few; of the trays near each place on the curve the best looking, another size than the last level's
 * when it can */
static int pick(const cand *pool, int n, const chapter *C, int *chosen)
{
    static int hard[MAXC];
    int nh = 0, used[MAXC] = {0};
    for (int i = 0; i < n; i++)
        if (pool[i].m.naive <= NAIVE_MAX && pool[i].m.score >= C->floor) hard[nh++] = i;
    fprintf(stderr, "  %d hard trays\n", nh);
    if (nh < PER - C->intro + 2) return 0;
    int lw = 0, lh = 0, last = -1;
    for (int k = 0; k < C->intro; k++) {
        int span = n / 5 > C->intro ? n / 5 : C->intro, i = -1;
        int a = k * span / C->intro, b = (k + 1) * span / C->intro - 1;
        if (C->from_easiest && k == 0) b = a + 1;
        if (a <= last) a = last + 1;
        if (b < a) b = a;
        for (int j = a; j <= b && j < n; j++)       /* short ones: a long easy way is a dull one */
            if (i < 0 || (pool[j].par <= C->lo + 5 && (pool[i].par > C->lo + 5 || pool[j].look > pool[i].look))) i = j;
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
            double v = c->look - (c->w == lw && c->h == lh ? 0.6 : 0);
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
            measure(&m, &x->m);
            x->diff = x->m.score;
            x->w = m.w;
            x->h = m.h;
            x->look = look_of(&m);
            if (++n >= MAXC) break;
        }
        qsort(c, (size_t)n, sizeof *c, by_diff);
        fprintf(stderr, "chapter %d: %d trays, par %d..%d\n", ch + 1, n, n ? c[0].par : 0, n ? c[n - 1].par : 0);
        int chosen[PER];
        if (n < PER || !pick(c, n, C, chosen)) { fprintf(stderr, "too few\n"); return 1; }
        for (int i = 0; i < PER; i++) {
            strcpy(out[ch * PER + i], c[chosen[i]].text);
            outpar[ch * PER + i] = c[chosen[i]].par;
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
#endif
