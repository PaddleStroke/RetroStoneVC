/*
 * Tile Connect's levels, made by the rules' own generator and chosen: src/games/tileconnect_levels.h. Our own levels:
 * no published set is used.
 * Six chapters of 25, each in groups of a size, a fall and how many blocked cells: still tiles from 6 x 4 to 10 x 6,
 * then falling down, to the left, towards the middle, and the big boards with all three. A candidate: blocked cells
 * placed at random in pairs turned half round the middle (never on the edge, never side by side), the faces of a
 * share of the pairs different (the rest come again: four or six tiles of a face), the board made by tcn_make (played
 * out first, so it always clears). Its difficulty: how often a player taking pairs at random gets stuck (out of
 * PLAYS tries), then how hard the search works to clear it. Each group keeps its levels spread over the candidates'
 * difficulties, the later groups of a chapter from further up. The first level of all has every face once.
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

typedef struct { int w, h, fall, count, blk0, blk1; float f0, f1; } group;
typedef struct { const group *g; int n; } chapter;

static const group G0[] = {{6, 4, TCN_STILL, 9, 0, 0, 1.0f, 0.55f}, {8, 4, TCN_STILL, 8, 0, 0, 0.75f, 0.5f},
                           {8, 5, TCN_STILL, 8, 0, 0, 0.6f, 0.5f}};
static const group G1[] = {{8, 6, TCN_STILL, 9, 0, 0, 0.6f, 0.5f}, {10, 5, TCN_STILL, 8, 1, 2, 0.55f, 0.5f},
                           {10, 6, TCN_STILL, 8, 1, 3, 0.5f, 0.45f}};
static const group G2[] = {{8, 5, TCN_DOWN, 8, 0, 0, 0.7f, 0.5f}, {8, 6, TCN_DOWN, 9, 0, 1, 0.6f, 0.5f},
                           {10, 6, TCN_DOWN, 8, 1, 2, 0.55f, 0.45f}};
static const group G3[] = {{8, 6, TCN_LEFT, 8, 0, 1, 0.6f, 0.5f}, {10, 6, TCN_LEFT, 9, 1, 2, 0.55f, 0.45f},
                           {10, 7, TCN_LEFT, 8, 1, 3, 0.5f, 0.45f}};
static const group G4[] = {{10, 6, TCN_CENTRE, 8, 0, 1, 0.55f, 0.5f}, {10, 7, TCN_CENTRE, 9, 1, 2, 0.5f, 0.45f},
                           {12, 7, TCN_CENTRE, 8, 1, 3, 0.5f, 0.4f}};
static const group G5[] = {{12, 7, TCN_DOWN, 6, 1, 3, 0.5f, 0.4f}, {12, 7, TCN_LEFT, 6, 2, 3, 0.5f, 0.4f},
                           {12, 8, TCN_CENTRE, 6, 2, 4, 0.45f, 0.4f}, {12, 8, TCN_DOWN, 7, 2, 4, 0.45f, 0.36f}};
static const chapter CH[TCN_CHAPTERS] = {{G0, 3}, {G1, 3}, {G2, 3}, {G3, 3}, {G4, 3}, {G5, 4}};

typedef struct {
    tcn_game g;
    double dead, nodes;
    int faces, blocks;
} cand;

static cand pool[CAND];
static uint32_t rng = 20261016u;

/* blocked cells: k pairs, each cell and its half turn round the middle, inside the edge, never side by side */
static int place_blocks(int w, int h, int k, uint8_t *blk)
{
    memset(blk, 0, TCN_CELLS);
    for (int placed = 0, tries = 0; placed < k && tries < 500; tries++) {
        int x = 1 + pr_below(&rng, w - 2), y = 1 + pr_below(&rng, h - 2), c = x + y * w, m = w * h - 1 - c;
        int bad = blk[c] || blk[m] || c == m;
        for (int d = 0; d < 4 && !bad; d++) {
            static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            int nx = x + DX[d], ny = y + DY[d], mx = m % w + DX[d], my = m / w + DY[d];
            if (nx >= 0 && ny >= 0 && nx < w && ny < h && blk[nx + ny * w]) bad = 1;
            if (mx >= 0 && my >= 0 && mx < w && my < h && blk[mx + my * w]) bad = 1;
            if (mx + my * w == c) bad = 1;      /* the pair itself side by side */
        }
        if (bad) continue;
        blk[c] = blk[m] = 1;
        placed++;
    }
    int n = 0;
    for (int c = 0; c < w * h; c++) n += blk[c];
    return n;
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
    for (int ch = 0; ch < TCN_CHAPTERS; ch++) {
        int made = 0;
        for (int gi = 0; gi < CH[ch].n; gi++) {
            const group *G = &CH[ch].g[gi];
            for (int i = 0; i < CAND; i++) {
                cand *c = &pool[i];
                float t = (float)i / (CAND - 1);
                int blk = G->blk0 + (int)((G->blk1 - G->blk0 + 1) * t);
                if (blk > G->blk1) blk = G->blk1;
                uint8_t blocks[TCN_CELLS];
                int nb, faces, made = 0;
                do {                            /* blocks that no plan gets past: placed again */
                    nb = place_blocks(G->w, G->h, blk, blocks);
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
            for (int k = 0; k < G->count; k++) {
                int i = (int)((lo + (hi - lo) * k / (G->count > 1 ? G->count - 1 : 1)) * (CAND - 1) + 0.5f);
                if (level == 0 && k == 0) i = 0;
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
                       ", search %.0f */\n",
                       g->b.w, g->b.h, g->b.fall, fc, oc, level + 1, g->b.w, g->b.h, FALL_NAME[g->b.fall],
                       pool[i].faces, pool[i].blocks, 100 * pool[i].dead, pool[i].nodes);
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
