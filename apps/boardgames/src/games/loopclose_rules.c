/*
 * Close the Loops for one player: the rules (loopclose.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/loopclose.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

/* ---- boards as text ---------------------------------------------------------------------------------------------- */
int lcl_parse(lcl_map *m, const char *text)
{
    int w = 0, h = 0, x = 0;
    memset(m, 0, sizeof *m);
    uint8_t g[LCL_MAXW * LCL_MAXH];
    for (const char *s = text;; s++) {
        if (*s == '|' || !*s) {
            if (!h) w = x;
            if (x != w || !w) return 0;
            h++;
            x = 0;
            if (!*s) break;
            continue;
        }
        if (x >= LCL_MAXW || h >= LCL_MAXH || !isxdigit((unsigned char)*s)) return 0;
        g[x + h * LCL_MAXW] = (uint8_t)(isdigit((unsigned char)*s) ? *s - '0' : tolower((unsigned char)*s) - 'a' + 10);
        x++;
    }
    m->w = w;
    m->h = h;
    for (int y = 0; y < h; y++)
        for (int xx = 0; xx < w; xx++) m->start[xx + y * w] = g[xx + y * LCL_MAXW];
    return 1;
}

void lcl_text(const lcl_map *m, char *out)
{
    int o = 0;
    for (int y = 0; y < m->h; y++) {
        if (y) out[o++] = '|';
        for (int x = 0; x < m->w; x++) out[o++] = "0123456789abcdef"[m->start[x + y * m->w] & 15];
    }
    out[o] = 0;
}

/* ---- play -------------------------------------------------------------------------------------------------------- */
void lcl_begin(lcl_game *g)
{
    memcpy(g->cur, g->m.start, sizeof g->cur);
    g->nh = g->moves = g->hints = 0;
    g->par = lcl_solve(g->m.w, g->m.h, g->cur, NULL, NULL);
}

int lcl_taps(int from, int to)
{
    for (int r = 0; r < 4; r++, from = lcl_rot(from))
        if (from == to) return r;
    return -1;
}

int lcl_tap(lcl_game *g, int c)
{
    if (c < 0 || c >= g->m.w * g->m.h || lcl_rot(g->cur[c]) == g->cur[c]) return 0;
    if (g->nh == LCL_HIST) {                    /* (the oldest taps forgotten) */
        memmove(g->hist, g->hist + 1, LCL_HIST - 1);
        g->nh--;
    }
    g->hist[g->nh++] = (uint8_t)c;
    g->cur[c] = (uint8_t)lcl_rot(g->cur[c]);
    g->moves++;
    return 1;
}

int lcl_undo(lcl_game *g)
{
    if (!g->nh) return 0;
    int c = g->hist[--g->nh];
    g->cur[c] = (uint8_t)lcl_rot(lcl_rot(lcl_rot(g->cur[c])));
    if (g->moves > 0) g->moves--;
    return 1;
}

int lcl_closed(int w, int h, const uint8_t *cur)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int m = cur[x + y * w];
            int r = x + 1 < w ? cur[x + 1 + y * w] : 0, d = y + 1 < h ? cur[x + (y + 1) * w] : 0;
            if (!!(m & LCL_RIGHT) != !!(r & LCL_LEFT) || !!(m & LCL_DOWN) != !!(d & LCL_UP)) return 0;
            if ((x == 0 && (m & LCL_LEFT)) || (y == 0 && (m & LCL_UP))) return 0;
        }
    return 1;
}

int lcl_solved(const lcl_game *g) { return lcl_closed(g->m.w, g->m.h, g->cur); }

/* ---- the search along the rows ----------------------------------------------------------------------------------
 * A state: bit x (x < w) the tile last placed in column x reaches down; bit w the tile just placed reaches right. */
#define BIG 1000000
static int cost[2][1 << (LCL_MAXW + 1)];
static long ways_n[2][1 << (LCL_MAXW + 1)];
static uint16_t back_state[LCL_CELLS][1 << (LCL_MAXW + 1)];
static uint8_t back_mask[LCL_CELLS][1 << (LCL_MAXW + 1)];

int lcl_solve(int w, int h, const uint8_t *cur, uint8_t *way, long *ways)
{
    int S = 1 << (w + 1), a = 0;
    for (int s = 0; s < S; s++) { cost[0][s] = BIG; ways_n[0][s] = 0; }
    cost[0][0] = 0;
    ways_n[0][0] = 1;
    for (int c = 0; c < w * h; c++) {
        int x = c % w, y = c / w, b = a ^ 1;
        for (int s = 0; s < S; s++) { cost[b][s] = BIG; ways_n[b][s] = 0; }
        int seen = 0, cand[4], taps[4], nc = 0;
        for (int r = 0, m = cur[c]; r < 4; r++, m = lcl_rot(m))
            if (!(seen >> m & 1)) {
                seen |= 1 << m;
                cand[nc] = m;
                taps[nc++] = r;
            }
        for (int s = 0; s < S; s++) {
            if (cost[a][s] >= BIG) continue;
            int up = s >> x & 1, left = s >> w & 1;
            for (int k = 0; k < nc; k++) {
                int m = cand[k];
                if (!!(m & LCL_UP) != up || !!(m & LCL_LEFT) != left) continue;
                if ((x == w - 1 && (m & LCL_RIGHT)) || (y == h - 1 && (m & LCL_DOWN))) continue;
                int n = (s & ~(1 << x) & ~(1 << w)) | (!!(m & LCL_DOWN) << x) | (!!(m & LCL_RIGHT) << w);
                int v = cost[a][s] + taps[k];
                long wn = ways_n[b][n] + ways_n[a][s];
                ways_n[b][n] = wn > 1000000 ? 1000000 : wn;
                if (v < cost[b][n]) {
                    cost[b][n] = v;
                    back_state[c][n] = (uint16_t)s;
                    back_mask[c][n] = (uint8_t)m;
                }
            }
        }
        a = b;
    }
    if (ways) *ways = ways_n[a][0];
    if (cost[a][0] >= BIG) return -1;
    if (way)
        for (int c = w * h - 1, s = 0; c >= 0; c--) {
            way[c] = back_mask[c][s];
            s = back_state[c][s];
        }
    return cost[a][0];
}

int lcl_hint(const lcl_game *g, int *cell, int *taps)
{
    uint8_t way[LCL_CELLS];
    if (lcl_solved(g) || lcl_solve(g->m.w, g->m.h, g->cur, way, NULL) < 0) return 0;
    int best = -1, bt = 9;
    for (int c = 0; c < g->m.w * g->m.h; c++) {
        int t = lcl_taps(g->cur[c], way[c]);
        if (t > 0 && t < bt) { bt = t; best = c; }   /* (the nearest: the fewest taps, first in reading order) */
    }
    if (best < 0) return 0;
    *cell = best;
    *taps = bt;
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
/* tile (x, y)'s images under the symmetry: how many (with itself first) */
static int images(const lcl_spec *sp, int w, int h, int x, int y, int *ix, int *iy)
{
    int n = 0;
    ix[n] = x; iy[n++] = y;
    switch (sp->sym) {
    case 0: ix[n] = w - 1 - x; iy[n++] = y; break;
    case 1:
        ix[n] = w - 1 - x; iy[n++] = y;
        ix[n] = x; iy[n++] = h - 1 - y;
        ix[n] = w - 1 - x; iy[n++] = h - 1 - y;
        break;
    case 2:                                     /* (x, y) -> (w - 1 - y, x), w == h */
        for (int k = 0; k < 3; k++, n++) {
            ix[n] = w - 1 - iy[n - 1];
            iy[n] = ix[n - 1];
        }
        break;
    default: ix[n] = w - 1 - x; iy[n++] = h - 1 - y; break;
    }
    return n;
}

int lcl_make(lcl_map *m, const lcl_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    int w = sp->w < 2 ? 2 : sp->w > LCL_MAXW ? LCL_MAXW : sp->w, h = sp->h < 2 ? 2 : sp->h > LCL_MAXH ? LCL_MAXH : sp->h;
    lcl_spec s = *sp;
    if (s.sym == 2 && w != h) s.sym = 1;
    memset(m, 0, sizeof *m);
    m->w = w;
    m->h = h;
    /* the squares between the tiles' middles: face (i, j) has tiles (i, j) .. (i + 1, j + 1) at its corners */
    static uint8_t face[LCL_MAXW][LCL_MAXH], done[LCL_MAXW][LCL_MAXH];
    memset(face, 0, sizeof face);
    memset(done, 0, sizeof done);
    for (int j = 0; j < h - 1; j++)
        for (int i = 0; i < w - 1; i++) {
            if (done[i][j]) continue;
            int in = pr_below(&r, 100) < s.fill;
            int ax[4], ay[4], bx[4], by[4], n = images(&s, w, h, i, j, ax, ay);
            images(&s, w, h, i + 1, j + 1, bx, by);
            for (int k = 0; k < n; k++) {      /* a face's image: the smaller corner of its corners' images */
                int fi = ax[k] < bx[k] ? ax[k] : bx[k], fj = ay[k] < by[k] ? ay[k] : by[k];
                face[fi][fj] = (uint8_t)in;
                done[fi][fj] = 1;
            }
        }
    /* the outlines: an edge between two tiles where the faces on its two sides differ */
    uint8_t arm[LCL_CELLS] = {0};
#define FACE(i, j) ((i) >= 0 && (j) >= 0 && (i) < w - 1 && (j) < h - 1 ? face[i][j] : 0)
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (x + 1 < w && FACE(x, y - 1) != FACE(x, y)) { arm[x + y * w] |= LCL_RIGHT; arm[x + 1 + y * w] |= LCL_LEFT; }
            if (y + 1 < h && FACE(x - 1, y) != FACE(x, y)) { arm[x + y * w] |= LCL_DOWN; arm[x + (y + 1) * w] |= LCL_UP; }
        }
    /* inner walls (between two faces inside) and stubs (from an empty tile), in symmetric sets */
    static uint8_t edone[LCL_CELLS][2];
    for (int pass = 0; pass < 2; pass++) {
        memset(edone, 0, sizeof edone);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int d = 0; d < 2; d++) {   /* d 0: the edge to the right, 1: the edge down */
                    int x2 = x + !d, y2 = y + d;
                    if (x2 >= w || y2 >= h || edone[x + y * w][d]) continue;
                    int has = d ? arm[x + y * w] & LCL_DOWN : arm[x + y * w] & LCL_RIGHT;
                    if (has) continue;
                    int want = 0;
                    if (pass == 0) {            /* a wall: both faces beside it inside */
                        int f1 = d ? FACE(x - 1, y) : FACE(x, y - 1), f2 = FACE(x, y);
                        want = f1 && f2 && pr_below(&r, 100) < s.walls;
                    } else                      /* a stub: one of its tiles empty */
                        want = (!arm[x + y * w] || !arm[x2 + y2 * w]) && pr_below(&r, 100) < s.stubs;
                    int ax[4], ay[4], bx[4], by[4], n = images(&s, w, h, x, y, ax, ay);
                    images(&s, w, h, x2, y2, bx, by);
                    for (int k = 0; k < n; k++) {
                        int p = ax[k] + ay[k] * w, q = bx[k] + by[k] * w;
                        if (p > q) { int t = p; p = q; q = t; }
                        int dd = q - p == 1 ? 0 : 1;
                        edone[p][dd] = 1;
                        if (!want) continue;
                        arm[p] |= dd ? LCL_DOWN : LCL_RIGHT;
                        arm[q] |= dd ? LCL_UP : LCL_LEFT;
                    }
                }
    }
#undef FACE
    /* a design worth playing: most tiles carry a line, no empty row or column at the edge */
    int used = 0, lines[4] = {0};
    for (int c = 0; c < w * h; c++) used += arm[c] != 0;
    for (int x = 0; x < w; x++) { lines[0] |= arm[x] != 0; lines[1] |= arm[x + (h - 1) * w] != 0; }
    for (int y = 0; y < h; y++) { lines[2] |= arm[y * w] != 0; lines[3] |= arm[w - 1 + y * w] != 0; }
    if (used * 10 < w * h * 6 || !lines[0] || !lines[1] || !lines[2] || !lines[3]) return 0;
    if (s.mix < 0) {                            /* the design itself, closed (a picture) */
        memcpy(m->start, arm, sizeof arm);
        return 1;
    }
    /* turned at random: never closed at the start, the fewest taps not too few */
    int least = w * h * (s.mix > 0 ? s.mix : 100) / 300;
    if (least < 3) least = 3;
    for (int k = 0; k < 20; k++) {
        for (int c = 0; c < w * h; c++) {
            int t = arm[c];
            int mix = s.mix > 0 ? s.mix : 100;
            if (pr_below(&r, 100) < mix)
                for (int n = 1 + pr_below(&r, 3); n > 0; n--) t = lcl_rot(t);
            m->start[c] = (uint8_t)t;
        }
        if (!lcl_closed(w, h, m->start) && lcl_solve(w, h, m->start, NULL, NULL) >= least) return 1;
    }
    return 0;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int lcl_script(lcl_game *g, const char *moves)
{
    const char *s = moves;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            s++;
            for (int guard = 0; guard < LCL_CELLS; guard++) {
                int c, t;
                if (!lcl_hint(g, &c, &t)) break;
                if (!all) g->hints++;
                while (t-- > 0) lcl_tap(g, c);
                if (!all) break;
            }
            continue;
        }
        if (!islower((unsigned char)*s) || !isdigit((unsigned char)s[1])) return 0;
        int x = *s - 'a', y = 0;
        s++;
        while (isdigit((unsigned char)*s)) y = y * 10 + (*s++ - '0');
        y--;
        if (x < 0 || y < 0 || x >= g->m.w || y >= g->m.h || !lcl_tap(g, x + y * g->m.w)) return 0;
    }
    return 1;
}

/* ---- the variants, the levels ------------------------------------------------------------------------------------ */
void lcl_variant(int v, lcl_spec *sp)
{
    static const lcl_spec V[LCL_VARIANTS] = {
        {5, 6, 0, 50, 10, 15, 0}, {7, 9, 0, 50, 15, 15, 0}, {9, 12, 1, 50, 15, 15, 0},
    };
    *sp = V[v < 0 || v >= LCL_VARIANTS ? 0 : v];
}

#ifndef LCL_GEN                                 /* not in the levels' tool (tools/loopclose_gen.c): it makes them */
#include "games/loopclose_levels.h"

int lcl_level(lcl_game *g, int level)
{
    if (level < 0 || level >= LCL_LEVELS || !lcl_parse(&g->m, lcl_levels[level].board)) return 0;
    lcl_begin(g);
    return 1;
}

void lcl_random(lcl_game *g, int variant, uint32_t seed)
{
    lcl_spec sp;
    lcl_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    for (int tries = 0; tries < 400; tries++) {
        sp.sym = pr_below(&r, 4) == 0 ? 3 : pr_below(&r, 2);
        sp.fill = 40 + pr_below(&r, 25);
        if (lcl_make(&g->m, &sp, pr_next(&r))) { lcl_begin(g); return; }
    }
    lcl_level(g, 0);
}
#endif
