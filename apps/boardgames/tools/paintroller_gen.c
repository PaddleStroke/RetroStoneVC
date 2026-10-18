/*
 * Paint Roller's levels: src/games/paintroller_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of floor (its size, its special tiles: plain floors small then larger, double tiles, portals,
 * arrows, all of them) and makes many with the rules' maker (pnr_make, from fixed seeds: a symmetric floor carved by a
 * ball rolling at random, its special tiles laid in symmetric sets, changed while the fewest pushes do not drop). A
 * floor is kept when the chapter's new tiles matter (made plain, the way gets shorter or the floor cannot be painted
 * whole) and when its layout was not met before (turned or mirrored either). The difficulty is measured (measure()):
 * the fewest pushes, the share of mistaken pushes along the way, and how many naive players (pushing at random,
 * mostly where some paint goes on) paint it all. Each chapter keeps 25 floors: one to three easy ones first (chapter 1
 * from the very easiest) that show its new tiles, then floors no naive player paints, spread from the chapter's floor
 * score to its hardest; of the floors near each place on the curve the one that looks best is kept (the board well
 * used, near square, few dead ends, another size than the level before); the chapter's last floor, its showpiece, the
 * best looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/pnrgen apps/boardgames/tools/paintroller_gen.c -lm && \
 *       /tmp/pnrgen > apps/boardgames/src/games/paintroller_levels.h
 * The levels' difficulty table: the same with -DPNR_TABLE, run with no output file.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define PNR_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/paintroller_rules.c"

#define PER (PNR_LEVELS / PNR_CHAPTERS)
#define MAKES 2000                              /* floors kept a chapter, at most */
#define CAP 300000

/* a chapter: size w x h (up to grow more each way), the rolls carving it, the special sets, the changes tried, the
 * tile that must matter (a PNR_* kind; 0 none; -1 two kinds at least); lo..hi: the pushes; intro: its first levels,
 * easy ones that show its new tiles; floor: the least score (measure()) of the others */
typedef struct {
    int w, h, grow, rolls, doubles, portals, arrows, iters, from_easiest, need, lo, hi, intro;
    double floor;
} chapter;

static const chapter CH[PNR_CHAPTERS] = {
    {4, 5, 2, 10, 0, 0, 0, 30, 1, 0, 2, 12, 3, 7},              /* plain floors, small */
    {6, 7, 2, 22, 0, 0, 0, 30, 0, 0, 8, 18, 0, 11},             /* larger */
    {6, 7, 2, 20, 2, 0, 0, 30, 0, PNR_DOUBLE, 7, 20, 2, 12},    /* double tiles */
    {6, 7, 2, 20, 0, 1, 0, 30, 0, PNR_PORTAL, 7, 20, 2, 13},    /* portals */
    {6, 7, 2, 20, 0, 0, 2, 30, 0, PNR_ARROW, 7, 22, 2, 14},     /* arrows */
    {7, 8, 2, 28, 1, 1, 1, 40, 0, -1, 12, 26, 1, 17},           /* all of them */
};
#define NAIVE_MAX 0.05                          /* the most naive players a hard level may let win */

static char (*kept)[PNR_TEXT];
static int nkept;

static void layout_of(const pnr_map *m, int tr, char *out)
{
    int w = tr & 4 ? m->h : m->w, h = tr & 4 ? m->w : m->h, n = 0;
    char t[PNR_TEXT];
    pnr_text(m, t);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = tr & 4 ? y : x, sy = tr & 4 ? x : y;
            if (tr & 1) sx = m->w - 1 - sx;
            if (tr & 2) sy = m->h - 1 - sy;
            char c = t[sx + sy * (m->w + 1)];
            out[n++] = strchr("^>v<", c) ? 'a' : strchr("ABC", c) ? 'P' : c == 'S' ? '.' : c;
        }
        out[n++] = '|';
    }
    out[n] = 0;
}

static int met(const pnr_map *m)
{
    char l[8][PNR_TEXT];
    for (int tr = 0; tr < 8; tr++) layout_of(m, tr, l[tr]);
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 8; tr++)
            if (!strcmp(kept[i], l[tr])) return 1;
    strcpy(kept[nkept++], l[0]);
    return 0;
}

static int count(const pnr_map *m, int kind)
{
    int n = 0;
    for (int c = 0; c < m->w * m->h; c++) n += m->t[c] == kind;
    return n;
}

/* the chapter's new tiles matter: made plain, the fewest pushes drop (or the floor can no longer be painted whole) */
static int matters(const pnr_map *m, int kind, int par)
{
    if (kind == 0) return !count(m, PNR_DOUBLE) && !count(m, PNR_PORTAL) && !count(m, PNR_ARROW);
    if (kind < 0) {
        int kinds = !!count(m, PNR_DOUBLE) + !!count(m, PNR_PORTAL) + !!count(m, PNR_ARROW);
        return kinds >= 2 && (matters(m, PNR_PORTAL, par) || matters(m, PNR_ARROW, par));
    }
    if (!count(m, kind)) return 0;
    pnr_map p = *m;
    for (int c = 0; c < p.w * p.h; c++)
        if (p.t[c] == kind) p.t[c] = PNR_TILE;
    number_bits(&p);
    int q = par_of(&p, CAP);
    return q < par;                             /* (-1: no longer possible, also counts) */
}

/* how hand-laid a floor looks: the board well used, near square, few dead ends */
static double look_of(const pnr_map *m)
{
    int tiles = 0, ends = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        if (m->t[c] == PNR_WALL) continue;
        tiles++;
        int x = c % m->w, y = c / m->w, near = 0;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            near += nx >= 0 && ny >= 0 && nx < m->w && ny < m->h && m->t[nx + ny * m->w] != PNR_WALL;
        }
        ends += near <= 1;
    }
    double fill = (double)tiles / (m->w * m->h);
    return -fabs(fill - 0.62) - 0.8 * ends / tiles - 0.3 * abs(m->w - m->h) / (m->w + m->h);
}

/* ---- how hard a floor is, measured ------------------------------------------------------------------------------
 * trap: along a shortest way, the share of the pushes that move the ball but are mistakes (they do not bring the
 * end a push nearer, or lose it); first: the same for the first push; naive: the share of 300 naive players who paint
 * it all within one and a half times its par (each push at random, but three times in four one that lays new paint) */
typedef struct { int par, states, tiles; double trap, first, naive, score; } metric;

typedef struct {
    char text[PNR_TEXT];
    metric m;
    int w, h;
    double look;
} cand;

static int by_score(const void *a, const void *b)
{
    double d = ((const cand *)a)->m.score - ((const cand *)b)->m.score;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

static int coats_of(const pnr_state *s)
{
    int n = 0;
    for (int w = 0; w < PNR_WORDS; w++) n += __builtin_popcountll(s->paint[w]);
    return n;
}

static void measure(const pnr_map *m, metric *mt)
{
    pnr_state s = start_of(m);
    mt->par = pnr_solve(m, &s, CAP, NULL, &mt->states);
    mt->tiles = 0;
    for (int c = 0; c < m->w * m->h; c++) mt->tiles += m->t[c] != PNR_WALL;
    int legal = 0, bad = 0, legal0 = 0, bad0 = 0;
    pnr_state c = s;
    for (int left = mt->par; left > 0; left--) {
        int dir = -1;
        pnr_solve(m, &c, CAP, &dir, NULL);
        for (int k = 0; k < 4; k++) {
            pnr_state n;
            if (!pnr_try(m, &c, k, &n, NULL, NULL)) continue;
            int d = pnr_solve(m, &n, CAP, NULL, NULL), mis = d != left - 1;
            legal++;
            bad += mis;
            if (left == mt->par) { legal0++; bad0 += mis; }
        }
        pnr_try(m, &c, dir, &c, NULL, NULL);
    }
    mt->trap = legal ? (double)bad / legal : 0;
    mt->first = legal0 ? (double)bad0 / legal0 : 0;
    uint32_t r = pr_seed(77);
    int wins = 0;
    for (int t = 0; t < 300; t++) {
        pnr_state p = s;
        for (int k = 0; k < mt->par + mt->par / 2 + 1 && !pnr_solved(m, &p); k++) {
            int ok[4], nok = 0, wet[4], nwet = 0, have = coats_of(&p);
            pnr_state n[4];
            for (int d = 0; d < 4; d++) {
                if (!pnr_try(m, &p, d, &n[d], NULL, NULL)) continue;
                ok[nok++] = d;
                if (coats_of(&n[d]) > have) wet[nwet++] = d;
            }
            if (!nok) break;
            int d = nwet && pr_below(&r, 4) ? wet[pr_below(&r, nwet)] : ok[pr_below(&r, nok)];
            p = n[d];
        }
        wins += pnr_solved(m, &p);
    }
    mt->naive = wins / 300.0;
    /* the score: the pushes, worth more where the mistakes are many and the naive players fail */
    mt->score = mt->par * (0.6 + mt->trap) * (1.2 - mt->naive) + mt->tiles / 30.0;
}

#ifdef PNR_TABLE                                /* the shipped levels' table: -DPNR_TABLE */
#include "games/paintroller_levels.h"
int main(void)
{
    (void)CH, (void)by_score, (void)look_of, (void)met, (void)matters;
    double sum[PNR_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl par states tiles trap first naive score  floor\n");
    for (int lv = 0; lv < PNR_LEVELS; lv++) {
        pnr_map m;
        metric mt;
        pnr_parse(&m, pnr_levels[lv]);
        measure(&m, &mt);
        printf("%4d %3d %6d %4d %5.2f %5.2f %5.2f %6.2f  %s\n", lv + 1, mt.par, mt.states, mt.tiles, mt.trap,
               mt.first, mt.naive, mt.score, pnr_levels[lv]);
        int triv = mt.naive > 0.1 || mt.par <= 4;
        double *s = sum[lv / PER];
        s[0] += mt.par; s[1] += mt.trap; s[2] += mt.naive; s[3] += mt.score; s[4] += triv;
        trivial += lv >= 3 && triv;
    }
    printf("\nchapter  par  trap naive score trivial\n");
    for (int c = 0; c < PNR_CHAPTERS; c++)
        printf("%7d %5.1f %5.2f %5.2f %5.1f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3] / PER, sum[c][4]);
    printf("trivial levels from level 4 (naive > 0.1 or par <= 4): %d\n", trivial);
    return 0;
}
#else
/* a chapter's levels from its pool (sorted by score): its first `intro` from the easy end (the chapter's new tiles
 * shown plainly), the others spread from `floor` to the hardest over the floors no naive player paints, the last of
 * the hardest few; of the floors near each place on the curve the best looking, another size than the last level's
 * when it can */
static int pick(cand *pool, int n, const chapter *C, int *chosen)
{
    static int hard[MAKES];
    int nh = 0, used[MAKES] = {0};
    for (int i = 0; i < n; i++)
        if (pool[i].m.naive <= NAIVE_MAX && pool[i].m.score >= C->floor) hard[nh++] = i;
    if (nh < PER - C->intro + 2) { fprintf(stderr, "only %d hard floors\n", nh); return 0; }
    int lw = 0, lh = 0, last = -1;
    for (int k = 0; k < C->intro; k++) {
        int span = n / 5 > C->intro ? n / 5 : C->intro, i = -1;
        int a = k * span / C->intro, b = (k + 1) * span / C->intro - 1;
        if (C->from_easiest && k == 0) b = a + 2;
        if (a <= last) a = last + 1;
        if (b < a) b = a;
        for (int j = a; j <= b && j < n; j++)       /* short ones: a long easy way is a dull one */
            if (i < 0 || (pool[j].m.par <= C->lo + 5 &&
                          (pool[i].m.par > C->lo + 5 || pool[j].look > pool[i].look))) i = j;
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
            double v = c->look - (c->w == lw && c->h == lh ? 0.15 : 0);
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
    kept = malloc((size_t)PNR_CHAPTERS * MAKES * 2 * sizeof *kept);
    static cand pool[MAKES];
    static char out[PNR_LEVELS][PNR_TEXT];
    static int outpar[PNR_LEVELS];
    printf("/* Paint Roller's levels: made by tools/paintroller_gen.c (our own floors, found by its search; do not "
           "edit).\n * Each floor's rows from the top split by '|' (paintroller.h), and its par: the fewest pushes. */\n");
    for (int ch = 0; ch < PNR_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(5300u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < 30 * MAKES && n < MAKES; k++) {
            int g = pr_below(&r, C->grow + 1), gh = pr_below(&r, C->grow + 1);
            pnr_spec sp = {C->w + g, C->h + gh, C->rolls + 3 * (g + gh), C->doubles, C->portals, C->arrows,
                           C->from_easiest ? pr_below(&r, C->iters + 1) : C->iters, CAP};
            pnr_map m;
            pnr_make(&m, &sp, pr_next(&r));
            pnr_state s = start_of(&m);
            int par = pnr_solve(&m, &s, CAP, NULL, NULL);
            if (par < C->lo || par > C->hi || m.w < 3 || m.h < 3 || !matters(&m, C->need, par) || met(&m)) continue;
            pnr_text(&m, pool[n].text);
            measure(&m, &pool[n].m);
            pool[n].w = m.w;
            pool[n].h = m.h;
            pool[n].look = look_of(&m);
            n++;
        }
        qsort(pool, (size_t)n, sizeof *pool, by_score);
        int chosen[PER];
        if (n < PER || !pick(pool, n, C, chosen)) {
            fprintf(stderr, "chapter %d: only %d floors\n", ch + 1, n);
            return 1;
        }
        for (int k = 0; k < PER; k++) {
            strcpy(out[ch * PER + k], pool[chosen[k]].text);
            outpar[ch * PER + k] = pool[chosen[k]].m.par;
        }
        fprintf(stderr, "chapter %d: %d floors, pushes %d .. %d\n", ch + 1, n, outpar[ch * PER],
                outpar[ch * PER + PER - 1]);
    }
    printf("const char *const pnr_levels[PNR_LEVELS] = {\n");
    for (int l = 0; l < PNR_LEVELS; l++) printf("    \"%s\",\n", out[l]);
    printf("};\n\nconst uint8_t pnr_level_par[PNR_LEVELS] = {");
    for (int l = 0; l < PNR_LEVELS; l++) printf("%s%d,", l % 25 ? " " : "\n    ", outpar[l]);
    printf("\n};\n");
    return 0;
}
#endif
