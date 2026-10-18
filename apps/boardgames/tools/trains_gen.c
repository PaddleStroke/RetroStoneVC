/*
 * Train Switches' levels: src/games/trains_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of network (its size, portals, stations and trains; toggles, crossings and loops, trains
 * together, tunnels) and lays many with the rules' maker (from fixed seeds). A network is kept when every switch is
 * run over, the trains go to stations of two colours or more, and it was not met before (nor its mirror). The
 * difficulty: the par (the fewest flips), how rare the settings that work are, the trains and the switches. The look,
 * for a hand-laid feel: the track spread over the board, neither sparse nor crowded, few zigzags, the stations apart;
 * of the networks near each place on the curve the best-looking one is kept, the chapter's last one (its showpiece)
 * the best-looking of its hardest few. Chapter 1 starts from the very easiest (one train, one switch). Deterministic.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/trsgen apps/boardgames/tools/trains_gen.c -lm && \
 *       /tmp/trsgen > apps/boardgames/src/games/trains_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TRS_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/trains_rules.c"

#define PER (TRS_LEVELS / TRS_CHAPTERS)
#define MAXC 3000

typedef struct {
    char text[TRS_TEXT], shape[2 * TRS_CELLS + 1];
    int par;
    double diff, look;
    int used;
} cand;

typedef struct { trs_spec sp[3]; int nsp, makes; } chapter;

/* w h portals stations trains switches loops toggles crossings tunnels together lo hi tries */
static const chapter CH[TRS_CHAPTERS] = {
    {{{5, 6, 1, 2, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1}, {5, 6, 1, 3, 2, 2, 0, 0, 0, 0, 0, 1, 2, 1},
      {5, 6, 2, 3, 2, 2, 0, 0, 0, 0, 0, 1, 2, 1}}, 3, 40000},
    {{{5, 7, 1, 3, 3, 2, 0, 0, 0, 0, 0, 2, 3, 1}, {6, 7, 2, 3, 3, 3, 0, 0, 0, 0, 0, 2, 3, 1},
      {6, 7, 2, 4, 3, 3, 0, 0, 0, 0, 0, 2, 4, 1}}, 3, 40000},
    {{{6, 7, 1, 3, 3, 2, 0, 1, 0, 0, 0, 1, 3, 1}, {6, 7, 2, 3, 4, 3, 0, 1, 0, 0, 0, 2, 4, 1},
      {6, 8, 2, 4, 4, 3, 0, 2, 0, 0, 0, 2, 4, 1}}, 3, 40000},
    {{{6, 8, 2, 3, 3, 3, 1, 0, 1, 0, 0, 2, 4, 1}, {6, 8, 2, 4, 4, 4, 1, 1, 1, 0, 0, 3, 5, 1},
      {6, 8, 3, 4, 4, 4, 0, 1, 1, 0, 0, 3, 5, 1}}, 3, 40000},
    {{{6, 8, 2, 3, 2, 2, 0, 0, 1, 0, 1, 2, 4, 1}, {6, 8, 3, 3, 3, 3, 0, 0, 1, 0, 1, 2, 4, 1},
      {6, 8, 3, 4, 3, 4, 1, 1, 0, 0, 1, 3, 5, 1}}, 3, 40000},
    {{{7, 9, 2, 4, 4, 4, 1, 1, 1, 1, 0, 3, 5, 1}, {7, 9, 3, 4, 3, 4, 0, 0, 1, 1, 1, 3, 5, 1},
      {7, 9, 2, 5, 5, 5, 1, 2, 0, 1, 0, 3, 6, 1}}, 3, 40000},
};

static cand C[MAXC];
static int nc;

static void shape_of(const trs_map *m, int mirror, char *out)   /* the network's squares (kinds and sides) */
{
    static const char HEX[] = "0123456789abcdef";
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            int c = (mirror ? m->w - 1 - x : x) + y * m->w, o = m->open[c];
            if (mirror) o = (o & 5) | (o & 2) << 2 | (o & 8) >> 2;
            *out++ = (char)('a' + m->kind[c]);
            *out++ = HEX[o];
        }
    *out = 0;
}

static double look_of(const trs_map *m)
{
    int W = m->w * m->h, used = 0, x0 = m->w, x1 = -1, y0 = m->h, y1 = -1, curves = 0, zig = 0;
    for (int c = 0; c < W; c++) {
        if (m->kind[c] == TRS_EMPTY) continue;
        used++;
        int x = c % m->w, y = c / m->w;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
        if (m->kind[c] == TRS_TRACK && m->open[c] != 5 && m->open[c] != 10) {
            curves++;
            for (int d = 0; d < 4; d++) {      /* a curve into a curve: a zigzag */
                int b = trs_neighbour(m, c, d);
                if ((m->open[c] >> d & 1) && b >= 0 && m->kind[b] == TRS_TRACK && m->open[b] != 5 && m->open[b] != 10)
                    zig++;
            }
        }
    }
    double cov = (double)used / W, spread = (double)(x1 - x0 + 1) * (y1 - y0 + 1) / W;
    return -4 * fabs(cov - 0.55) + 1.5 * spread - 1.2 * zig / (double)(used + 1) - 0.4 * curves / (double)(used + 1);
}

static int seen(const char *shape)
{
    for (int i = 0; i < nc; i++)
        if (!strcmp(C[i].shape, shape)) return 1;
    return 0;
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    char out[TRS_LEVELS][TRS_TEXT];
    int par[TRS_LEVELS], k = 0;
    for (int ch = 0; ch < TRS_CHAPTERS; ch++) {
        nc = 0;
        uint32_t rs = pr_seed(1000u + 7919u * (uint32_t)ch);
        for (int t = 0; t < CH[ch].makes && nc < MAXC; t++) {
            const trs_spec *sp = &CH[ch].sp[t % CH[ch].nsp];
            trs_map m;
            trs_quality q;
            if (!make_one(&m, sp, &rs, &q) || q.par < sp->lo || q.par > sp->hi) continue;
            char sh[2 * TRS_CELLS + 1], mi[2 * TRS_CELLS + 1];
            shape_of(&m, 0, sh);
            shape_of(&m, 1, mi);
            if (seen(sh) || seen(mi)) continue;
            cand *c = &C[nc++];
            trs_text(&m, c->text);
            strcpy(c->shape, sh);
            c->par = q.par;
            c->diff = q.par + 1.2 * log2((double)(1 << m.ns) / q.sols) + 0.4 * m.nt + 0.25 * m.ns;
            c->look = look_of(&m);
            c->used = 0;
        }
        if (nc < PER) { fprintf(stderr, "chapter %d: only %d networks\n", ch + 1, nc); return 1; }
        qsort(C, (size_t)nc, sizeof C[0], by_diff);
        fprintf(stderr, "chapter %d: %d networks, difficulty %.1f .. %.1f\n", ch + 1, nc, C[0].diff, C[nc - 1].diff);
        for (int i = 0; i < PER; i++) {         /* along the curve: the best-looking near each place */
            int at = ch == 0 && i == 0 ? 0 : (int)((double)i / (PER - 1) * (nc - 1) * 0.92 + 0.04 * nc);
            int win = i == PER - 1 ? nc / 12 + 2 : nc / (3 * PER) + 2, best = -1;
            if (i == PER - 1) at = nc - win / 2 - 1;
            for (int j = at - win / 2; j <= at + win / 2; j++) {
                if (j < 0 || j >= nc || C[j].used) continue;
                if (ch == 0 && i == 0 && j != 0) continue;
                if (best < 0 || C[j].look > C[best].look) best = j;
            }
            for (int j = 0; best < 0 && j < nc; j++)
                if (!C[(at + j) % nc].used) best = (at + j) % nc;
            C[best].used = 1;
            strcpy(out[k], C[best].text);
            par[k++] = C[best].par;
        }
    }
    printf("/* Train Switches' levels: made by tools/trains_gen.c (our own networks, found by its search; do not edit).\n"
           " * Each network as text (trains.h), and its par: the fewest flips. */\n");
    printf("const char *const trs_levels[TRS_LEVELS] = {\n");
    for (int i = 0; i < TRS_LEVELS; i++) printf("    \"%s\",\n", out[i]);
    printf("};\n\nconst uint8_t trs_level_par[TRS_LEVELS] = {");
    for (int i = 0; i < TRS_LEVELS; i++) printf("%s%d,", i % 25 ? " " : "\n    ", par[i]);
    printf("\n};\n");
    return 0;
}
