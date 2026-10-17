/*
 * Tile Connect's levels, made by the rules' own generator and chosen: src/games/tileconnect_levels.h. Our own levels:
 * no published set is used.
 * Six chapters of 25, each in groups of a size, a fall and how many blocked cells: still tiles from 6 x 4 to 10 x 6,
 * then falling down, to the left, towards the middle, and the big boards with all three. A candidate: the board's
 * shape from a hand-made design of blocked cells (DESIGN: cut corners, an octagon, pillars, notches, a centre stone,
 * bars...; mirrored both ways), the group's designs taken in turn; the faces of a share of the pairs different (the
 * rest come again: four or six tiles of a face), the board made by tcn_make (played out first, so it always clears);
 * then its faces given from one or two families (THEME: the dots, the bamboo, the honours, the flowers...). Its
 * difficulty: how often a player taking pairs at random gets stuck (out of PLAYS tries), then how hard the search
 * works to clear it. Each group keeps its levels spread over the candidates' difficulties, the later groups of a
 * chapter from further up, never the design or the faces' families of the level before. The first level of all has
 * every face once.
 * Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/tcngen apps/boardgames/tools/tileconnect_gen.c && \
 *       /tmp/tcngen > apps/boardgames/src/games/tileconnect_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TCN_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/tileconnect_rules.c"

#define CAND 24
#define PLAYS 40

typedef struct { int w, h, fall, count, d0, d1; float f0, f1; } group;   /* d0..d1: the designs (DESIGN) used */
typedef struct { const group *g; int n; } chapter;

static const group G0[] = {{6, 4, TCN_STILL, 9, 0, 3, 1.0f, 0.55f}, {8, 4, TCN_STILL, 8, 0, 4, 0.75f, 0.5f},
                           {8, 5, TCN_STILL, 8, 0, 5, 0.6f, 0.5f}};
static const group G1[] = {{8, 6, TCN_STILL, 9, 0, 6, 0.6f, 0.5f}, {10, 5, TCN_STILL, 8, 1, 8, 0.55f, 0.5f},
                           {10, 6, TCN_STILL, 8, 2, 10, 0.5f, 0.45f}};
static const group G2[] = {{8, 5, TCN_DOWN, 8, 0, 5, 0.7f, 0.5f}, {8, 6, TCN_DOWN, 9, 1, 8, 0.6f, 0.5f},
                           {10, 6, TCN_DOWN, 8, 2, 12, 0.55f, 0.45f}};
static const group G3[] = {{8, 6, TCN_LEFT, 8, 0, 6, 0.6f, 0.5f}, {10, 6, TCN_LEFT, 9, 1, 9, 0.55f, 0.45f},
                           {10, 7, TCN_LEFT, 8, 2, 12, 0.5f, 0.45f}};
static const group G4[] = {{10, 6, TCN_CENTRE, 8, 0, 8, 0.55f, 0.5f}, {10, 7, TCN_CENTRE, 9, 1, 10, 0.5f, 0.45f},
                           {12, 7, TCN_CENTRE, 8, 2, 12, 0.5f, 0.4f}};
static const group G5[] = {{12, 7, TCN_DOWN, 6, 1, 12, 0.5f, 0.4f}, {12, 7, TCN_LEFT, 6, 1, 12, 0.5f, 0.4f},
                           {12, 8, TCN_CENTRE, 6, 2, 12, 0.45f, 0.4f}, {12, 8, TCN_DOWN, 7, 2, 12, 0.45f, 0.36f}};
static const chapter CH[TCN_CHAPTERS] = {{G0, 3}, {G1, 3}, {G2, 3}, {G3, 3}, {G4, 3}, {G5, 4}};

typedef struct {
    tcn_game g;
    double dead, nodes;
    int faces, blocks, design, theme;
} cand;

static cand pool[CAND];
static uint32_t rng = 20261016u;

/* the board's shapes: hand-made designs of blocked cells, mirrored both ways (the widths are even: an even number of
 * cells is left). Features, in the top left quarter: */
enum { F_CORNER = 1, F_BEVEL = 2, F_PILLAR = 4, F_CENTRE = 8, F_NOTCH = 16, F_SIDES = 32, F_BAR = 64 };
static const int DESIGN[] = {
    0, F_CORNER, F_PILLAR, F_NOTCH, F_CORNER | F_BEVEL, F_CENTRE, F_CORNER | F_CENTRE, F_BAR, F_NOTCH | F_SIDES,
    F_CORNER | F_BEVEL | F_CENTRE, F_PILLAR | F_NOTCH, F_CORNER | F_BAR, F_PILLAR | F_SIDES,
};
static const char *const DESIGN_NAME[] = {"plain", "corners", "pillars", "notches", "octagon", "centre", "corners+centre",
                                          "bars", "gates", "octagon+centre", "pillars+notches", "corners+bars",
                                          "pillars+sides"};
#define NDESIGN ((int)(sizeof DESIGN / sizeof DESIGN[0]))

static void block4(uint8_t *blk, int w, int h, int x, int y)
{
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    blk[x + y * w] = blk[w - 1 - x + y * w] = blk[x + (h - 1 - y) * w] = blk[w - 1 - x + (h - 1 - y) * w] = 1;
}

static int place_design(int w, int h, int d, uint8_t *blk)
{
    int f = DESIGN[d], mid = (h - 1) / 2;
    memset(blk, 0, TCN_CELLS);
    if (f & F_CORNER) block4(blk, w, h, 0, 0);
    if (f & F_BEVEL) block4(blk, w, h, 1, 0), block4(blk, w, h, 0, 1);
    if (f & F_PILLAR) block4(blk, w, h, w / 4, h / 4);
    if (f & F_CENTRE) block4(blk, w, h, w / 2 - 1, h % 2 ? mid : h / 2 - 1);
    if (f & F_NOTCH) block4(blk, w, h, w / 2 - 1, 0);
    if (f & F_SIDES) block4(blk, w, h, 0, mid);
    if (f & F_BAR)
        for (int x = 2; x <= w / 2 - 2; x++) block4(blk, w, h, x, mid);
    int n = 0;
    for (int c = 0; c < w * h; c++) n += blk[c];
    return n;
}

/* the faces of a level from one family or two (the suits, the honours, the flowers and seasons): the faces made are
 * given again, one for one, from the smallest set that holds them (or a pair of sets) */
static const int THEME[][2] = {{0, 9}, {9, 9}, {18, 9}, {27, 7}, {34, 8}, {27, 15}, {0, 18}, {9, 18}, {18, 16}, {18, 24},
                               {0, 27}, {9, 33}, {0, 42}};
#define NTHEME ((int)(sizeof THEME / sizeof THEME[0]))
static int theme_faces(tcn_game *g, int pick)
{
    int used[TCN_FACES] = {0}, n = 0, map[TCN_FACES];
    for (int c = 0; c < g->b.w * g->b.h; c++)
        if (g->b.face[c] < TCN_FACES && !used[g->b.face[c]]) used[g->b.face[c]] = 1, n++;
    int fit[NTHEME], nf = 0;
    for (int t = 0; t < NTHEME; t++)
        if (THEME[t][1] >= n && THEME[t][1] <= n + 12) fit[nf++] = t;
    if (!nf) fit[nf++] = NTHEME - 1;
    int t = fit[pick % nf], set[TCN_FACES];
    for (int i = 0; i < THEME[t][1]; i++) set[i] = THEME[t][0] + i;
    for (int i = THEME[t][1] - 1; i > 0; i--) {
        int j = pr_below(&rng, i + 1), x = set[i];
        set[i] = set[j], set[j] = x;
    }
    for (int f = 0, k = 0; f < TCN_FACES; f++) map[f] = used[f] ? set[k++] : f;
    for (int c = 0; c < g->b.w * g->b.h; c++)
        if (g->b.face[c] < TCN_FACES) g->b.face[c] = (uint8_t)map[g->b.face[c]];
    return t;
}

/* a player taking pairs at random: how often out of PLAYS they get stuck */
static double dead_rate(const tcn_game *g0)
{
    int dead = 0;
    uint32_t s = 7u;
    for (int p = 0; p < PLAYS; p++) {
        tcn_board b = g0->b;
        int tiles = tiles_on(&b);
        while (tiles) {
            uint8_t cnt[TCN_FACES], cells[TCN_FACES][8];
            by_face(&b, cnt, cells);
            int pa[256], pc[256], n = 0;
            for (int f = 0; f < TCN_FACES; f++)
                for (int i = 0; i < cnt[f]; i++)
                    for (int j = i + 1; j < cnt[f] && n < 256; j++)
                        if (tcn_connect(&b, cells[f][i], cells[f][j], NULL, NULL)) pa[n] = cells[f][i], pc[n++] = cells[f][j];
            if (!n) { dead++; break; }
            int k = pr_below(&s, n);
            take_tiles(&b, pa[k], pc[k], NULL);
            tiles -= 2;
        }
    }
    return (double)dead / PLAYS;
}

/* the plan played from the start clears the board */
static int plan_clears(const tcn_game *g0)
{
    tcn_game t = *g0;
    for (int k = 0; k < t.pairs; k++) {
        int a, c;
        if (!plan_pair(&t, &a, &c) || !tcn_take(&t, a, c, NULL)) return 0;
    }
    return tcn_won(&t);
}

static int by_diff(const void *x, const void *y)
{
    const cand *a = x, *b = y;
    if (a->dead != b->dead) return a->dead < b->dead ? -1 : 1;
    return a->nodes < b->nodes ? -1 : a->nodes > b->nodes;
}

static const char *FALL_NAME[TCN_FALLS] = {"still", "down", "left", "centre"};

int main(void)
{
    printf("/*\n * Tile Connect's levels: our own, made by tools/tileconnect_gen.c (each board played out first by the "
           "rules' generator,\n * so it always clears; chosen by how often taking pairs at random gets stuck). Each: "
           "its size, its fall, a\n * character a cell for the faces ('#' blocked), then the step of the plan that "
           "takes each tile (the tests play it).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("const tcn_lvl tcn_levels[TCN_LEVELS] = {\n");
    int level = 0;
    int prev_design = -1, prev_theme = -1;
    for (int ch = 0; ch < TCN_CHAPTERS; ch++) {
        int made = 0;
        for (int gi = 0; gi < CH[ch].n; gi++) {
            const group *G = &CH[ch].g[gi];
            for (int i = 0; i < CAND; i++) {
                cand *c = &pool[i];
                int design = G->d0 + i % (G->d1 - G->d0 + 1), tries = 0;
                uint8_t blocks[TCN_CELLS];
                int nb, faces, made = 0;
                do {                            /* a design that no plan gets past: the next one */
                    if (tries++ > 8) design = G->d0 + (design - G->d0 + 1) % (G->d1 - G->d0 + 1), tries = 0;
                    nb = place_design(G->w, G->h, design, blocks);
                    int pairs = (G->w * G->h - nb) / 2;
                    float f = G->f0 + (G->f1 - G->f0) * (float)pr_below(&rng, 1000) / 999.0f;
                    faces = (int)(f * pairs + 0.5f);
                    if (level == 0 && i == 0) faces = pairs;   /* the very first: every face once */
                    made = tcn_make(&c->g, G->w, G->h, G->fall, blocks, faces, pr_next(&rng));
                    if (!made && getenv("TCN_VERBOSE")) fprintf(stderr, "  blocks placed again\n");
                } while (!made);
                if (getenv("TCN_VERBOSE")) fprintf(stderr, "  candidate %d made\n", i);
                if (!plan_clears(&c->g)) { fprintf(stderr, "plan failed\n"); return 1; }
                c->dead = dead_rate(&c->g);
                long nodes = 0;
                int r = tcn_solve(&c->g.b, 30000, NULL, NULL, &nodes);
                c->nodes = r == 1 ? (double)nodes : 1e9;
                c->faces = faces;
                c->blocks = nb;
                c->design = design;
                c->theme = theme_faces(&c->g, i / (G->d1 - G->d0 + 1) + i);
                if (getenv("TCN_VERBOSE")) fprintf(stderr, "  candidate %d: dead %.2f nodes %.0f\n", i, c->dead, c->nodes);
            }
            cand first = pool[0];
            qsort(pool, CAND, sizeof pool[0], by_diff);
            if (level == 0) {                   /* the very first level stays first */
                for (int i = 0; i < CAND; i++)
                    if (!memcmp(&pool[i].g.b, &first.g.b, sizeof first.g.b)) {
                        memmove(pool + 1, pool, sizeof pool[0] * i);
                        pool[0] = first;
                        break;
                    }
            }
            /* spread over the difficulties: the first group from the easiest, the later ones further up */
            float lo = CH[ch].n > 1 ? 0.35f * gi / (CH[ch].n - 1) : 0, hi = lo + 0.65f;
            int taken[CAND] = {0};
            for (int k = 0; k < G->count; k++) {
                int i = (int)((lo + (hi - lo) * k / (G->count > 1 ? G->count - 1 : 1)) * (CAND - 1) + 0.5f);
                if (level == 0 && k == 0) i = 0;
                /* a mix: not the design or the faces of the level before, nor a board already taken; the nearest
                 * difficulty that differs */
                int best = -1, any = -1;
                for (int d = 0; d < 2 * CAND && level > 0; d++) {
                    int j = i + (d % 2 ? -(d + 1) / 2 : d / 2);
                    if (j < 0 || j >= CAND || taken[j]) continue;
                    if (any < 0) any = j;
                    if (pool[j].design != prev_design && pool[j].theme != prev_theme) { best = j; break; }
                }
                if (level > 0) i = best >= 0 ? best : any;
                taken[i] = 1;
                prev_design = pool[i].design;
                prev_theme = pool[i].theme;
                const tcn_game *g = &pool[i].g;
                char fc[TCN_CELLS + 1], oc[TCN_CELLS + 1];
                int n = g->b.w * g->b.h;
                for (int x = 0; x < n; x++) {
                    uint8_t f = g->b.face[x];
                    fc[x] = f == TCN_BLOCK ? '#' : TCN_FACE_CH[f];
                    oc[x] = f == TCN_BLOCK ? '#' : TCN_STEP_CH[g->step[g->b.id[x]]];
                }
                fc[n] = oc[n] = 0;
                printf("    {%d, %d, %d, \"%s\",\n     \"%s\"}, /* %d: %d x %d, %s, %d faces, %d blocked, stuck %.0f%%"
                       ", search %.0f, %s */\n",
                       g->b.w, g->b.h, g->b.fall, fc, oc, level + 1, g->b.w, g->b.h, FALL_NAME[g->b.fall],
                       pool[i].faces, pool[i].blocks, 100 * pool[i].dead, pool[i].nodes, DESIGN_NAME[pool[i].design]);
                level++;
                made++;
            }
            fprintf(stderr, "chapter %d group %d done (%d levels)\n", ch + 1, gi + 1, level);
        }
        if (made != TCN_LEVELS / TCN_CHAPTERS) { fprintf(stderr, "chapter %d: %d levels\n", ch + 1, made); return 1; }
    }
    printf("};\n");
    return level == TCN_LEVELS ? 0 : 1;
}
