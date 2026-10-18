/*
 * Paint Roller's levels: src/games/paintroller_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of floor (its size, its special tiles: plain floors small then larger, double tiles, portals,
 * arrows, all of them) and makes many with the rules' maker (pnr_make, from fixed seeds: a symmetric floor carved by a
 * ball rolling at random, its special tiles laid in symmetric sets, changed while the fewest pushes do not drop). A
 * floor is kept when the chapter's new tiles matter (made plain, the way gets shorter or the floor cannot be painted
 * whole) and when its layout was not met before (turned or mirrored either). The difficulty: the fewest pushes, a
 * little more for a larger floor and for the states the search went through. Each chapter keeps 25 floors spread over
 * its difficulties, from an easy one (chapter 1 from the very easiest) to its hardest; of the floors near each place on
 * the curve the one that looks best is kept (the board well used, near square, few dead ends); the chapter's last
 * floor, its showpiece, the best looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/pnrgen apps/boardgames/tools/paintroller_gen.c -lm && \
 *       /tmp/pnrgen > apps/boardgames/src/games/paintroller_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define PNR_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/paintroller_rules.c"

#define PER (PNR_LEVELS / PNR_CHAPTERS)
#define MAKES 260                               /* floors kept a chapter, at most */
#define CAP 300000

typedef struct {
    char text[PNR_TEXT];
    int par, states;
    double diff, look;
} cand;

/* a chapter: size w x h (up to grow more each way), the rolls carving it, the special sets, the changes tried, the
 * tile that must matter (a PNR_* kind; 0 none; -1 two kinds at least); lo..hi: the pushes */
typedef struct { int w, h, grow, rolls, doubles, portals, arrows, iters, from_easiest, need, lo, hi; } chapter;

static const chapter CH[PNR_CHAPTERS] = {
    {4, 5, 2, 10, 0, 0, 0, 10, 1, 0, 2, 9},             /* plain floors, small */
    {6, 7, 2, 22, 0, 0, 0, 30, 0, 0, 7, 16},            /* larger */
    {6, 7, 2, 20, 2, 0, 0, 30, 0, PNR_DOUBLE, 7, 18},   /* double tiles */
    {6, 7, 2, 20, 0, 1, 0, 30, 0, PNR_PORTAL, 7, 18},   /* portals */
    {6, 7, 2, 20, 0, 0, 2, 30, 0, PNR_ARROW, 7, 20},    /* arrows */
    {7, 8, 2, 28, 1, 1, 1, 40, 0, -1, 10, 24},          /* all of them */
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

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
            int states, par = pnr_solve(&m, &s, CAP, NULL, &states);
            if (par < C->lo || par > C->hi || m.w < 3 || m.h < 3 || !matters(&m, C->need, par) || met(&m)) continue;
            int tiles = 0;
            for (int c = 0; c < m.w * m.h; c++) tiles += m.t[c] != PNR_WALL;
            pnr_text(&m, pool[n].text);
            pool[n].par = par;
            pool[n].states = states;
            pool[n].diff = par + tiles / 25.0 + states / 4000.0;
            pool[n].look = look_of(&m);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d floors\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = C->from_easiest ? 0 : n / 6;
        double step = (double)(n - 1 - lo) / (PER - 1);
        for (int k = 0, last = -1; k < PER; k++) {
            int mid = lo + (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
            if (k == 0 && C->from_easiest) a = b = 0;
            if (k == PER - 1) a = n - 1 - n / 25 - 2, b = n - 1;
            if (a < lo) a = lo;
            if (a <= last) a = last + 1;
            if (b > n - 1) b = n - 1;
            if (b < a) b = a;
            for (int j = a; j <= b; j++)
                if (i < 0 || pool[j].look > pool[i].look) i = j;
            last = i;
            strcpy(out[ch * PER + k], pool[i].text);
            outpar[ch * PER + k] = pool[i].par;
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
